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
	// Find the blocks that the table was stored in
	BF_Block *metadata_block;
	BF_Block_Init(&metadata_block);

	CALL_BF_PTR(BF_GetBlock(file_dsc, 0, metadata_block), "Error getting block in UpdateHashTable\n");

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

	int *old_used_blocks = NULL;
	int old_used_blocks_num = ht_info->number_of_hash_table_blocks;
	if (ht_info->first_hash_table_block_id != -1)
	{
		old_used_blocks = malloc(sizeof(int) * ht_info->number_of_hash_table_blocks);
		old_used_blocks[0] = ht_info->first_hash_table_block_id;

		// Find the blocks that the table was stored in
		BF_Block *hash_table_block;
		BF_Block_Init(&hash_table_block);
		int next_block_id = ht_info->first_hash_table_block_id;

		for (int i = 1; i < ht_info->number_of_hash_table_blocks; i++)
		{
			if (next_block_id == -1)
			{
				printf("Error in DoubleHashTable(next_block_id == -1)\n");
				return NULL;
			}

			CALL_BF_PTR(BF_GetBlock(file_dsc, next_block_id, hash_table_block), "Error getting first block in DoubleHashTable\n");
			HashTable_Block_metadata *first_block_metadata = (HashTable_Block_metadata *)BF_Block_GetData(hash_table_block);
			next_block_id = first_block_metadata->next_block_id;
			old_used_blocks[i] = next_block_id;
			// Unpin the block
			CALL_BF_PTR(BF_UnpinBlock(hash_table_block), "Error unpinning block in DoubleHashTable\n");
		}

		// Destroy the block because we don't need it anymore
		BF_Block_Destroy(&hash_table_block);
	}

	size_t remaining_size = new_table_size;
	int remaining_preallocated_blocks = old_used_blocks_num;
	BF_Block *hash_table_block;
	BF_Block_Init(&hash_table_block);
	int cur_block_id = -1;

	int *new_block_ids = malloc(sizeof(int) * ceil((double)new_table_size / ht_info->cells_per_hash_block));
	int new_blocks_num = 0;
	while (remaining_size > 0)
	{
		if (remaining_preallocated_blocks > 0)
		{
			cur_block_id = old_used_blocks[old_used_blocks_num - remaining_preallocated_blocks];
			CALL_BF_PTR(BF_GetBlock(file_dsc, cur_block_id, hash_table_block), "Error getting block in DoubleHashTable\n");
			remaining_preallocated_blocks--;
		}
		else
		{
			CALL_BF_PTR(BF_GetBlockCounter(file_dsc, &cur_block_id), "Error getting block count in DoubleHashTable");
			CALL_BF_PTR(BF_AllocateBlock(file_dsc, hash_table_block), "Error allocating block in DoubleHashTable\n");
			void *block_data = (void *)BF_Block_GetData(hash_table_block);
			memset(block_data, 0, BF_BLOCK_SIZE);
		}

		if (cur_block_id == -1)
		{
			printf("Error in DoubleHashTable(cur_block_id == -1)\n");
			return NULL;
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
		CALL_BF_PTR(BF_UnpinBlock(hash_table_block), "Error unpinning block in DoubleHashTable\n");

		new_block_ids[new_blocks_num] = cur_block_id;
		new_blocks_num++;
		remaining_size -= amount_to_copy;
	}

	// Update the metadata of the blocks

	for (int i = 0; i < new_blocks_num - 1; i++)
	{
		CALL_BF_PTR(BF_GetBlock(file_dsc, new_block_ids[i], hash_table_block), "Error getting block in DoubleHashTable\n");
		HashTable_Block_metadata *block_metadata = (HashTable_Block_metadata *)BF_Block_GetData(hash_table_block);
		block_metadata->next_block_id = new_block_ids[i + 1];
		BF_Block_SetDirty(hash_table_block);
		CALL_BF_PTR(BF_UnpinBlock(hash_table_block), "Error unpinning block in DoubleHashTable\n");
	}

	// Update the metadata of the file
	ht_info->number_of_hash_table_blocks = new_blocks_num;
	ht_info->global_depth = old_depth + 1;

	// Destroy the block because we don't need it anymore
	BF_Block_Destroy(&hash_table_block);

	// Destroy the metadata block after updating it
	// BF_Block_SetDirty(metadata_block);
	// // TODO check if this is needed
	// CALL_BF_PTR(BF_UnpinBlock(metadata_block), "Error unpinning block in DoubleHashTable\n");
	BF_Block_Destroy(&metadata_block);

	// Free the unused tables
	free(new_block_ids);
	if (old_used_blocks != NULL)
		free(old_used_blocks);

	return hash_table_new;
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
	// Initialize the blocks that we will use
	BF_Block *metadata_block; // Block for the file metadata
	BF_Block_Init(&metadata_block);

	BF_Block *hash_table_block; // Block to loop through the hash table blocks
	BF_Block_Init(&hash_table_block);

	HashTableCell *hash_table = NULL;

	CALL_BF_PTR(BF_GetBlock(file_dsc, 0, metadata_block), "Error getting block in UpdateHashTable\n");

	HT_info *ht_info = (HT_info *)BF_Block_GetData(metadata_block);

	if (ht_info->first_hash_table_block_id == -1 || ht_info->number_of_hash_table_blocks == 0)
	{
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
			goto LoadTableFromDisk_error_exit;
		}

		CALL_BF_PTR(BF_GetBlock(file_dsc, next_block_id, hash_table_block), "Error getting first block in LoadTableFromDisk\n");
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
		CALL_BF_PTR(BF_UnpinBlock(hash_table_block), "Error unpinning block in LoadTableFromDisk\n");
	}

	// Destroy the block because we don't need it anymore
	BF_Block_Destroy(&hash_table_block);

	// Destroy the metadata block because we don't need it anymore
	// TODO check if this is needed
	// CALL_BF_PTR(BF_UnpinBlock(metadata_block), "Error unpinning block in LoadTableFromDisk\n");
	BF_Block_Destroy(&metadata_block);

	return hash_table;

LoadTableFromDisk_error_exit:

	BF_Block_Destroy(&hash_table_block);
	BF_Block_Destroy(&metadata_block);

	if (hash_table != NULL)
		free(hash_table);

	printf(":: ERROR :: - Error in LoadTableFromDisk - Check if it is intentional\n ");

	return NULL;
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
	// Get the first block with the metadata
	BF_Block *metadata_block;
	BF_Block_Init(&metadata_block);
	BF_GetBlock(file_dsc, 0, metadata_block);
	HT_info *ht_info = (HT_info *)BF_Block_GetData(metadata_block);

	// Get the block to split
	BF_Block *record_block;
	BF_Block_Init(&record_block);
	BF_GetBlock(file_dsc, block_id, record_block);
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

		int old_global_depth = ht_info->global_depth;
		printf(":: DEBUG :: - Doubling the hash table - Hash value triggering the split: %d\n", hash_value);

		// Double the hash table
		hash_table = DoubleHashTable(file_dsc, ht_info->global_depth, hash_table);
		if (hash_table == NULL)
		{
			printf(":: ERROR :: - Error in SplitBlock\n");
			return NULL;
		}

		printf(":: DEBUG :: - Doubling the hash table - Old global depth: %d - New global depth: %d\n", old_global_depth, ht_info->global_depth);
		new_hash_value = hash_value << 1;
	}

	// Prefix of all friends of the block that will be split
	int prefix = new_hash_value >> (ht_info->global_depth - block_info->local_depth);

	// Index of first friend of the block that will be split
	int first_friend_index = prefix << (ht_info->global_depth - block_info->local_depth);

	// Index of the last friend of the block that will be split
	int last_friend_index = first_friend_index | ((1 << (ht_info->global_depth - block_info->local_depth)) - 1);

	// Allocate the new block and get its id
	CALL_BF_PTR(BF_GetBlockCounter(file_dsc, &new_block_id), "Error getting block count in SplitBlock\n");
	CALL_BF_PTR(BF_AllocateBlock(file_dsc, new_block), "Error allocating block in SplitBlock\n");
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


	// Clean up
	// CALL_BF_PTR(BF_UnpinBlock(record_block), "Error unpinning block in SplitBlock\n");
	BF_Block_Destroy(&record_block);

	// CALL_BF_PTR(BF_UnpinBlock(new_block), "Error unpinning block in SplitBlock\n");
	BF_Block_Destroy(&new_block);

	BF_Block_Destroy(&metadata_block);

	return hash_table;
}

BF_ErrorCode UpdateHashTableValue(HashTableCell *hashtable, int index, int value, int file_dsc)
{
	hashtable[index].block_id = value;

	// file metadata block
	BF_Block *metadata_block;
	BF_Block_Init(&metadata_block);
	BF_GetBlock(file_dsc, 0, metadata_block);
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
			printf("Error in UpdateHashTableValue(next_block_id == -1)\n");
			goto UpdateHashTableValue_error_exit;
		}

		BF_GetBlock(file_dsc, next_block_id, hash_table_block);
		HashTable_Block_metadata *block_metadata = (HashTable_Block_metadata *)BF_Block_GetData(hash_table_block);
		block_data = ((void *)block_metadata) + sizeof(HashTable_Block_metadata);

		if (cur_block_index == block_index)
		{
			// We found the block
			break;
		}

		next_block_id = block_metadata->next_block_id;
		// unpin the block
		BF_UnpinBlock(hash_table_block);
	}

	if (block_data == NULL)
	{
		printf("Error in UpdateHashTableValue\n");
		goto UpdateHashTableValue_error_exit;
	}

	// Update the block
	HashTableCell *block_table = (HashTableCell *)block_data;
	index = index % ht_info->cells_per_hash_block;

	block_table[index].block_id = value;

	// Set the block as dirty
	BF_Block_SetDirty(hash_table_block);
	CALL_BF(BF_UnpinBlock(hash_table_block), "Error unpinning block in UpdateHashTableValue\n");
	BF_Block_Destroy(&hash_table_block);

	// Unpin the metadata block
	// TODO possibly remove this
	// BF_CALL(BF_UnpinBlock(metadata_block), "Error unpinning block in UpdateHashTableValue\n");
	BF_Block_Destroy(&metadata_block);

	return BF_OK;

UpdateHashTableValue_error_exit:

	CALL_BF(BF_UnpinBlock(hash_table_block), "Error unpinning block in UpdateHashTableValue\n");
	BF_Block_Destroy(&hash_table_block);
	BF_Block_Destroy(&metadata_block);

	return BF_ERROR;
}

// uint hash_function(unsigned int x, unsigned int size)
// {

// 	// Check if size is a power of 2
// 	if ((size & (size - 1)) != 0)
// 	{
// 		fprintf(stderr, "Error in hash_function. Size is not a poower of 2\n");
// 		return -1;
// 	}

// 	// Find the number of bits needed to represent size
// 	// 8 - 1 = 1000 - 0001 = 00000000 00000000 00000000 00000111 

// 	int leading_zeros = __builtin_clz(size - 1);
// 	// int number_of_bits = 32 - leading_zeros;

// 	// Keep the number of bits needed to represent size

// 	// x = 00000000 00000000 01010001 01010111
// 	// y = 00000000 00000000 00000000 01111111 

// 	// new_x = 00000000 00000000 00000000 01010111


// 	// int new_x = x & ((1 << number_of_bits) - 1);
// 	// return new_x;

// 	return x >> leading_zeros;

// 	// return x % size;
// }

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

		printf(":: DEBUG :: - RehashRecords - Record with id %d was rehashed to %d (original %d) and is inserted in bin %d\n", temp_record.id, hash_value, old_hash_value, hash_table[hash_value].block_id );

		if(hash_table[hash_value].block_id == record_block_id)
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