#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include "utility.h"

/*
Doubles The old hast table in size and returns the new one
This update is done in both the memory and the disk
In any error case NULL is returned

note: This function updates the following fields in the metadata block:
	- first_hash_table_block_id
	- number_of_hash_table_blocks
	- global_depth
*/
HashTableCell *DoubleHashTable(int file_dsc, int old_depth, HashTableCell *hash_table_old)
{
	HashTableCell *retval = NULL;
	BF_ErrorCode err = BF_OK;

	// Find the blocks that the table was stored in
	BF_Block *metadata_block;
	BF_Block_Init(&metadata_block);

	err = BF_GetBlock(file_dsc, 0, metadata_block);
	if (err != BF_OK)
	{
		perror("Error in DoubleHashTable - BF_GetBlock returned Error Code\n");
		goto DoubleHashTable_destroy_metadata_leave;
	}

	HT_info *ht_info = (HT_info *)BF_Block_GetData(metadata_block);

	size_t new_table_size = 1 << (old_depth + 1);

	// Create a new table in memory
	HashTableCell *hash_table_new;
	hash_table_new = malloc(sizeof(HashTableCell) * new_table_size);

	if (ht_info->first_hash_table_block_id == -1 || ht_info->number_of_hash_table_blocks == 0 || hash_table_old == NULL || old_depth == 0)
	{
		// Create the table from scratch΄
		for (int i = 0; i < new_table_size; i++)
			hash_table_new[i].block_id = -1;

		if (hash_table_old != NULL)
			free(hash_table_old);
	}
	else
	{
		// Create the new table from the old one
		unsigned int old_table_size = 1 << old_depth;

		for (int i = 0; i < old_table_size; i++)
		{
			hash_table_new[2 * i].block_id = hash_table_old[i].block_id;
			hash_table_new[2 * i + 1].block_id = hash_table_old[i].block_id;
		}

		// Free the old table
		free(hash_table_old);
	}

	BF_Block *hash_table_block;
	BF_Block_Init(&hash_table_block);

	int *old_used_blocks = NULL;
	int old_used_blocks_num = ht_info->number_of_hash_table_blocks;
	if (ht_info->first_hash_table_block_id != -1)
	{
		old_used_blocks = malloc(sizeof(int) * ht_info->number_of_hash_table_blocks);
		old_used_blocks[0] = ht_info->first_hash_table_block_id;

		// Find the blocks that the table was stored in
		int next_block_id = ht_info->first_hash_table_block_id;

		for (int i = 1; i < ht_info->number_of_hash_table_blocks; i++)
		{
			if (next_block_id == -1)
			{
				perror("Error in DoubleHashTable(next_block_id == -1)\n");
				retval = NULL;
				goto DoubleHashTable_destroy_hash_table_block_leave;
			}

			err = BF_GetBlock(file_dsc, next_block_id, hash_table_block);
			if (err != BF_OK)
			{
				perror("Error in DoubleHashTable - Error getting first block\n");
				goto DoubleHashTable_destroy_hash_table_block_leave;
			}

			HashTable_Block_metadata *first_block_metadata = (HashTable_Block_metadata *)BF_Block_GetData(hash_table_block);
			next_block_id = first_block_metadata->next_block_id;
			old_used_blocks[i] = next_block_id;

			// Clean Up
			err = BF_UnpinBlock(hash_table_block);
			if (err != BF_OK)
			{
				perror("Error in DoubleHashTable - Error unpinning block\n");
				goto DoubleHashTable_destroy_hash_table_block_leave;
			}
		}
	}

	size_t remaining_size = new_table_size;
	int remaining_preallocated_blocks = old_used_blocks_num;
	int cur_block_id = -1;

	int *new_block_ids = malloc(sizeof(int) * ceil((double)new_table_size / ht_info->cells_per_hash_block));
	int new_blocks_num = 0;

	while (remaining_size > 0)
	{
		if (remaining_preallocated_blocks > 0)
		{
			cur_block_id = old_used_blocks[old_used_blocks_num - remaining_preallocated_blocks];
			err = BF_GetBlock(file_dsc, cur_block_id, hash_table_block);
			if (err != BF_OK)
			{
				perror("Error in DoubleHashTable - Error getting first block\n");
				goto DoubleHashTable_free_new_block_ids_and_leave;
			}
			remaining_preallocated_blocks--;
		}
		else
		{
			err = BF_GetBlockCounter(file_dsc, &cur_block_id);
			if (err != BF_OK)
			{
				perror("Error in DoubleHashTable - Error getting block count\n");
				goto DoubleHashTable_free_new_block_ids_and_leave;
			}

			err = BF_AllocateBlock(file_dsc, hash_table_block);
			if (err != BF_OK)
			{
				perror("Error in DoubleHashTable - Error allocating block\n");
				goto DoubleHashTable_free_new_block_ids_and_leave;
			}

			void *block_data = (void *)BF_Block_GetData(hash_table_block);
			memset(block_data, 0, BF_BLOCK_SIZE);
		}

		if (cur_block_id == -1)
		{
			perror("Error in DoubleHashTable(cur_block_id == -1)\n");
			retval = NULL;
			goto DoubleHashTable_free_new_block_ids_and_leave;
		}

		if (ht_info->first_hash_table_block_id == -1)
		{
			ht_info->first_hash_table_block_id = cur_block_id;
		}

		// Copy the table to the block
		void *block_data = (void *)BF_Block_GetData(hash_table_block);
		void *table_data = (void *)hash_table_new;

		size_t offset_in_table = (new_blocks_num * ht_info->cells_per_hash_block) * sizeof(HashTableCell);
		size_t offset_in_block = sizeof(HashTable_Block_metadata);

		int amount_to_copy = min(remaining_size, ht_info->cells_per_hash_block);
		memcpy(block_data + offset_in_block, table_data + offset_in_table, amount_to_copy * sizeof(HashTableCell));

		// Set the metadata of the block
		HashTable_Block_metadata *block_metadata = (HashTable_Block_metadata *)block_data;
		block_metadata->next_block_id = -1;

		BF_Block_SetDirty(hash_table_block);
		err = BF_UnpinBlock(hash_table_block);
		if (err != BF_OK)
		{
			perror("Error in DoubleHashTable - Error unpinning block\n");
			goto DoubleHashTable_free_new_block_ids_and_leave;
		}

		new_block_ids[new_blocks_num] = cur_block_id;
		new_blocks_num++;
		remaining_size -= amount_to_copy;
	}

	// Update the metadata of the blocks

	for (int i = 0; i < new_blocks_num - 1; i++)
	{
		err = BF_GetBlock(file_dsc, new_block_ids[i], hash_table_block);
		if (err != BF_OK)
		{
			perror("Error in DoubleHashTable - Error getting block\n");
			goto DoubleHashTable_free_new_block_ids_and_leave;
		}

		HashTable_Block_metadata *block_metadata = (HashTable_Block_metadata *)BF_Block_GetData(hash_table_block);
		block_metadata->next_block_id = new_block_ids[i + 1];
		BF_Block_SetDirty(hash_table_block);
		err = BF_UnpinBlock(hash_table_block);
		if (err != BF_OK)
		{
			perror("Error in DoubleHashTable - Error unpinning block\n");
			goto DoubleHashTable_free_new_block_ids_and_leave;
		}
	}

	// Update the metadata of the file
	ht_info->number_of_hash_table_blocks = new_blocks_num;
	ht_info->global_depth = old_depth + 1;
	BF_Block_SetDirty(metadata_block);

	// Success
	retval = hash_table_new;

DoubleHashTable_free_new_block_ids_and_leave:
	if (new_block_ids != NULL)
		free(new_block_ids);

DoubleHashTable_destroy_hash_table_block_leave:
	if (old_used_blocks != NULL)
		free(old_used_blocks);

	BF_Block_Destroy(&hash_table_block);

DoubleHashTable_destroy_metadata_leave:
	BF_Block_Destroy(&metadata_block);

	if (err != BF_OK)
	{
		BF_PrintError(err);
		retval = NULL;
	}

	return retval;
}

/*
Creates a hash table with depth depth
This table is created both in memory and in the disk
In any error case NULL is returned

Note: The table will have size 2^depth

Note: This function updates the following fields in the metadata block:
	- first_hash_table_block_id
	- number_of_hash_table_blocks
	- global_depth
*/
HashTableCell *CreateHashTable(int file_dsc, int depth)
{
	return DoubleHashTable(file_dsc, depth - 1, NULL);
}

/*
Loads the hash table from the disk and returns it in memory
In any error case NULL is returned

Note: The global depth in the metadata of the file MUST be correct
*/
HashTableCell *LoadTableFromDisk(int file_dsc)
{
	HashTableCell * retval = NULL;
	BF_ErrorCode err = BF_OK;

	// Initialize the blocks that we will use
	BF_Block *metadata_block; // Block for the file metadata
	BF_Block_Init(&metadata_block);

	BF_Block *hash_table_block; // Block to loop through the hash table blocks
	BF_Block_Init(&hash_table_block);

	HashTableCell *hash_table = NULL;

	err = BF_GetBlock(file_dsc, 0, metadata_block);
	if (err != BF_OK)
	{
		perror("Error in LoadTableFromDisk - Error getting metadata block\n");
		goto LoadTableFromDisk_error_exit;
	}

	HT_info *ht_info = (HT_info *)BF_Block_GetData(metadata_block);

	if (ht_info->first_hash_table_block_id == -1 || ht_info->number_of_hash_table_blocks == 0)
	{
		fprintf(stderr,"Error in LoadTableFromDisk - No hash table blocks found for file dsc %d\n", file_dsc);
		goto LoadTableFromDisk_error_exit;
	}

	// Create the table in memory
	hash_table = malloc(sizeof(HashTableCell) * (1 << ht_info->global_depth));

	// Load the blocks of the table
	int next_block_id = ht_info->first_hash_table_block_id;

	int cells_to_read = (1 << ht_info->global_depth);

	for (int i = 0; i < ht_info->number_of_hash_table_blocks; i++)
	{
		if (next_block_id == -1)
		{
			perror("Error in LoadTableFromDisk(next_block_id == -1)\n");
			goto LoadTableFromDisk_error_exit;
		}

		err = BF_GetBlock(file_dsc, next_block_id, hash_table_block);
		if (err != BF_OK)
		{
			perror("Error in LoadTableFromDisk - Error getting first block\n");
			goto LoadTableFromDisk_error_exit;
		}

		void *block_data = (void *)BF_Block_GetData(hash_table_block);

		// Read the next block id
		HashTable_Block_metadata *block_metadata = (HashTable_Block_metadata *)block_data;
		next_block_id = block_metadata->next_block_id;

		// move the block_data pointer after the metadata
		block_data += sizeof(HashTable_Block_metadata);

		// Copy the table to the block
		void *table_data = (void *)hash_table;
		size_t offset_in_table = i * ht_info->cells_per_hash_block * sizeof(HashTableCell);

		int amount_of_cells_to_copy = min(cells_to_read, ht_info->cells_per_hash_block);

		memcpy(table_data + offset_in_table, block_data, amount_of_cells_to_copy * sizeof(HashTableCell));

		cells_to_read -= amount_of_cells_to_copy;
		err = BF_UnpinBlock(hash_table_block);
		if (err != BF_OK)
		{
			perror("Error in LoadTableFromDisk - Error unpinning block\n");
			goto LoadTableFromDisk_error_exit;
		}
	}

	// Success
	retval = hash_table;

LoadTableFromDisk_error_exit:

	BF_Block_Destroy(&hash_table_block);
	BF_Block_Destroy(&metadata_block);

	if (hash_table != NULL && retval == NULL)
		free(hash_table);

	if (err != BF_OK)
	{
		BF_PrintError(err);
		retval = NULL;
	}

	return retval;
}

/*
Frees the memory used by the hash table
*/
int FreeHashTable(HashTableCell *hash_table)
{
	free(hash_table);
	return 0;
}

/*
This function splits a block in 2 and updates the hash table.
If a doubling of the hash table is needed it is done here.
The new block will be created at the hash value of the record

In any error case NULL is returned

The created_block_id is the id of the new block that was just created

IMPORTANT!: The caller MUST unpin the blocks

Note: This function updates the following fields in the metadata block:
	- first_hash_table_block_id
	- number_of_hash_table_blocks
	- global_depth
*/
HashTableCell *SplitBlock(int block_id, int file_dsc, HashTableCell *hash_table, uint hash_value, int *created_block_id)
{
	HashTableCell *retval = NULL;
	BF_ErrorCode err = BF_OK;

	// Get the first block with the metadata
	BF_Block *metadata_block;
	BF_Block_Init(&metadata_block);

	err = BF_GetBlock(file_dsc, 0, metadata_block);
	if (err != BF_OK)
	{
		perror("Error in SplitBlock - Error getting metadata block\n");
		goto SplitBlock_destroy_metadata_block;
	}

	HT_info *ht_info = (HT_info *)BF_Block_GetData(metadata_block);

	// Get the block to split
	BF_Block *record_block;
	BF_Block_Init(&record_block);

	err = BF_GetBlock(file_dsc, block_id, record_block);
	if (err != BF_OK)
	{
		perror("Error in SplitBlock - Error getting record block\n");
		goto SplitBlock_destroy_record_block;
	}

	HT_block_info *block_info = (HT_block_info *)BF_Block_GetData(record_block);

	// Initialize the new block
	int new_block_id;
	BF_Block *new_block;
	BF_Block_Init(&new_block);

	// 2 cases here

	// 1. The block needs to be splited in 2 but in the same hash table
	// 2. The new block does not fit in the hash table so we need to double the hash table
	int new_hash_value = hash_value;
	if (block_info->local_depth == ht_info->global_depth) // Case 2
	{
		// Double the hash table
		printf(":: DEBUG :: Doubling hash table...\n");
		hash_table = DoubleHashTable(file_dsc, ht_info->global_depth, hash_table);
		show_hash_table(hash_table, ht_info->global_depth, file_dsc);
		if (hash_table == NULL)
		{
			perror("Error in SplitBlock - Error doubling hash table\n");
			goto SplitBlock_destroy_new_block;
		}

		new_hash_value = hash_value << 1;
	}

	// Prefix of all friends of the block that will be split
	int prefix = new_hash_value >> (ht_info->global_depth - block_info->local_depth);

	// Index of first friend of the block that will be split
	int first_friend_index = prefix << (ht_info->global_depth - block_info->local_depth);

	// Index of the last friend of the block that will be split
	int last_friend_index = first_friend_index | ((1 << (ht_info->global_depth - block_info->local_depth)) - 1);

	// Allocate the new block and get its id
	if (BF_GetBlockCounter(file_dsc, &new_block_id) != BF_OK)
	{
		perror("Error in SplitBlock - Error getting Block Counter\n");
		goto SplitBlock_destroy_new_block;
	}

	if (BF_AllocateBlock(file_dsc, new_block) != BF_OK)
	{
		perror("Error in SplitBlock - Error Allocating block\n");
		goto SplitBlock_destroy_new_block;
	}

	HT_block_info *new_block_info = (HT_block_info *)BF_Block_GetData(new_block);
	memset(new_block_info, 0, BF_BLOCK_SIZE);
	*created_block_id = new_block_id;

	// Update the metadata of the blocks
	block_info->local_depth++;

	new_block_info->local_depth = block_info->local_depth;
	new_block_info->number_of_records_on_block = 0;
	BF_Block_SetDirty(record_block);
	BF_Block_SetDirty(new_block);

	// Update the pointers in the hash table
	for (int i = ((last_friend_index - first_friend_index) / 2) + 1; i <= last_friend_index - first_friend_index; i++)
	{
		UpdateHashTableValue(hash_table, first_friend_index + i, new_block_id, file_dsc);
	}

	retval = hash_table;

	// Clean up
SplitBlock_destroy_new_block:
	BF_Block_Destroy(&new_block);

SplitBlock_destroy_record_block:
	BF_Block_Destroy(&record_block);

SplitBlock_destroy_metadata_block:
	BF_Block_Destroy(&metadata_block);

	if(err != BF_OK)
	{
		BF_PrintError(err);
		retval = NULL;
	}

	return retval;
}

BF_ErrorCode UpdateHashTableValue(HashTableCell *hashtable, int index, int value, int file_dsc)
{
	BF_ErrorCode retval = BF_OK;

	hashtable[index].block_id = value;

	// file metadata block
	BF_Block *metadata_block;
	BF_Block_Init(&metadata_block);

	retval = BF_GetBlock(file_dsc, 0, metadata_block);
	if (retval != BF_OK)
	{
		fprintf(stderr,"Error in UpdateHashTableValue - Error getting the block\n");
		goto UpdateHashTableValue_destroy_metadata_block;
	}
	HT_info *ht_info = (HT_info *)BF_Block_GetData(metadata_block);

	// Find the index of the block in the chain of blocks
	int block_index = index / ht_info->cells_per_hash_block;

	// Follow the chain of blocks until we find the block we want
	int next_block_id = ht_info->first_hash_table_block_id;
	void *block_data = NULL;

	BF_Block *hash_table_block;
	BF_Block_Init(&hash_table_block);
	for (int cur_block_index = 0; cur_block_index < ht_info->number_of_hash_table_blocks; cur_block_index++)
	{
		if (next_block_id == -1)
		{
			perror("Error in UpdateHashTableValue(next_block_id == -1)\n");
			retval = BF_ERROR;
			goto UpdateHashTableValue_destroy_hashtable_block_and_exit;
		}

		retval = BF_GetBlock(file_dsc, next_block_id, hash_table_block);
		if(retval != BF_OK)
		{
			perror("Error in UpdateHashTableValue - Error getting the block\n");
			goto UpdateHashTableValue_destroy_hashtable_block_and_exit;
		}
		
		HashTable_Block_metadata *block_metadata = (HashTable_Block_metadata *)BF_Block_GetData(hash_table_block);
		block_data = ((void *)block_metadata) + sizeof(HashTable_Block_metadata);

		if (cur_block_index == block_index)
		{
			// We found the block
			break;
		}

		next_block_id = block_metadata->next_block_id;
		// unpin the block

		retval = BF_UnpinBlock(hash_table_block);
		if(retval != BF_OK)
		{
			perror("Error in UpdateHashTableValue - Error getting the block\n");
			goto UpdateHashTableValue_destroy_hashtable_block_and_exit;
		}
	}

	if (block_data == NULL)
	{
		perror("Error in UpdateHashTableValue\n");
		retval = BF_ERROR;
		goto UpdateHashTableValue_unpin_hashtable_block_and_exit;
	}

	// Update the block
	HashTableCell *block_table = (HashTableCell *)block_data;
	index = index % ht_info->cells_per_hash_block;

	block_table[index].block_id = value;

	// Set the block as dirty
	BF_Block_SetDirty(hash_table_block);
	retval = BF_UnpinBlock(hash_table_block);
	if(retval != BF_OK)
	{
		perror("Error in UpdateHashTableValue - Error getting the block\n");
		goto UpdateHashTableValue_destroy_hashtable_block_and_exit;
	}

UpdateHashTableValue_unpin_hashtable_block_and_exit:
	retval = BF_UnpinBlock(hash_table_block);
	if(retval != BF_OK)
	{
		perror("Error in UpdateHashTableValue - Error unpining the block\n");
	}

UpdateHashTableValue_destroy_hashtable_block_and_exit:
	BF_Block_Destroy(&hash_table_block);

UpdateHashTableValue_destroy_metadata_block:
	BF_Block_Destroy(&metadata_block);

	return retval;
}

#define FNV_offset_basis 2166136261
#define FNV_prime 16777619

// We will hash the int using the FNV-1a hash function
// And then keep the most significant bits needed to represent the size of the hash table
uint hash_function(uint x, size_t size)
{
	// Check if size is a power of 2
	if ((size & (size - 1)) != 0)
	{
		fprintf(stderr, "Error in hash_function. Size is not a poower of 2\n");
		return -1;
	}

	uint hash = FNV_offset_basis;

	// 32 bit FNV-1a hash
	for (int i = 0; i < sizeof(int); i++)
	{
		hash ^= (x >> (i * 8)) & 0xff;
		hash *= FNV_prime;
	}

	// Keep the number of bits needed to represent size
	int leading_zeros = __builtin_clz(size - 1);

	return hash >> leading_zeros;
}

int min(int a, int b)
{
	if (a < b)
		return a;
	else
		return b;
}

/*
This function will insert the record at the end of the block data given as an argument
If the block is full it will return 1

In any error case -1 is returned

Note: This function will skip the metadata of the block given by itself so the block_data pointer should point to the start of the data of the block

Note: This function updates the following fields in the metadata of the block:
	- number_of_records_on_block
*/
int InsertRecordInBlock(void *data, Record record, int max_records)
{
	HT_block_info *block_info = (HT_block_info *)data;

	// Check if the block is not full
	if (block_info->number_of_records_on_block >= max_records)
		return 1;

	// Insert the record to the right place in the block
	Record *records_in_block = (Record *)(data + sizeof(HT_block_info) + block_info->number_of_records_on_block * sizeof(Record));
	memcpy(records_in_block, &record, sizeof(Record));

	// Update the metadata of the block
	block_info->number_of_records_on_block++;

	return 0;
}

/*
This function will rehash the records found in the block_data pointer and will split them in both the block_data and the new_block_data pointers
Returns 0 on success

! Important !
This function will not set the blocks as dirty so the caller should do it

Note: This function updates the following fields in the metadata of the blocks:
	- number_of_records_on_block

*/
int RehashRecords(void *block_data, void *new_block_data, int record_block_id, int new_record_block_id, HashTableCell *hash_table, uint hash_table_size)
{
	// Get the old and new block metadata
	HT_block_info *block_info = (HT_block_info *)block_data;
	HT_block_info *new_block_info = (HT_block_info *)new_block_data;

	// How many records we have to rehash
	int records_to_rehash = block_info->number_of_records_on_block;

	// Reset the counters in both blocks
	block_info->number_of_records_on_block = 0;
	new_block_info->number_of_records_on_block = 0;

	// Start the rehashing
	Record *records_array_in_block = (Record *)(block_data + sizeof(HT_block_info));
	Record *new_records_array_in_block = (Record *)(new_block_data + sizeof(HT_block_info));
	Record temp_record;

	int records_in_old_block = 0;
	int records_in_new_block = 0;

	for (int i = 0; i < records_to_rehash; i++)
	{
		// Get the record
		memcpy(&temp_record, records_array_in_block + i, sizeof(Record));

		// Rehash the record
		uint hash_value = hash_function(temp_record.id, hash_table_size);
		uint old_hash_value = hash_function(temp_record.id, hash_table_size >> 1);

		if (hash_table[hash_value].block_id == record_block_id)
		{
			// The record stays in the old block
			memcpy(records_array_in_block + records_in_old_block, &temp_record, sizeof(Record));
			records_in_old_block++;
		}
		else
		{
			// The record goes to the new block
			memcpy(new_records_array_in_block + records_in_new_block, &temp_record, sizeof(Record));
			records_in_new_block++;
		}
	}

	// Update the metadata of the blocks
	block_info->number_of_records_on_block = records_in_old_block;
	new_block_info->number_of_records_on_block = records_in_new_block;

	// Fill the rest of the block with zeros
	memset(records_array_in_block + records_in_old_block, 0, (BF_BLOCK_SIZE - sizeof(HT_block_info) - records_in_old_block * sizeof(Record)));

	return 0;
}


// Show hash table
int show_hash_table(HashTableCell *hash_table, int size, int file_dsc)
{
	// Create a block to get the data from the hash table
	BF_Block *block;
	BF_Block_Init(&block);


	printf(":: DEBUG :: Showing hash table...\n");
	for (int i = 0; i < size; i++)
	{
		int block_id = hash_table[i].block_id;

		if(block_id == -1)
		{
			printf(":: DEBUG :: Hash table cell %d: block_id %d\n", i, block_id);
			continue;
		}

		BF_GetBlock(file_dsc, block_id, block);
		void *block_data = (void *)BF_Block_GetData(block);
		HT_block_info *block_info = (HT_block_info *)block_data;

		printf(":: DEBUG :: Hash table cell %d: block_id %d with local depth %d and %d entries\n", i, block_id, block_info->local_depth, block_info->number_of_records_on_block);
	
		// Unpin the block
		CALL_BF(BF_UnpinBlock(block), "Error unpinning block in show_hash_table\n");
	}

	BF_Block_Destroy(&block);

	return 0;
}