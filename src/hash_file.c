#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

// #include "ht.h"
#include "hash_file.h"
#include "ht.h"
#include "utility.h"

HT_ErrorCode HT_Init()
{
	// insert code here
	BF_Init(LRU);

	// Make the table for the file descriptors
	file_table = calloc(MAX_OPEN_FILES, sizeof(content_table_entry));
	for (int i = 0; i < MAX_OPEN_FILES; i++)
	{
		file_table[i].file_desc = -1;
		file_table[i].filename = NULL;
		file_table[i].hash_table = NULL;
	}

	return HT_OK;
}

HT_ErrorCode HT_Close()
{
	// insert code here
	//  Close all files
	for (int i = 0; i < MAX_OPEN_FILES; i++)
	{
		if (file_table[i].file_desc != -1)
		{
			if (HT_CloseFile(i) != HT_OK)
			{
				printf("Error closing file %s\n", file_table[i].filename);
				return HT_ERROR;
			}

			free(file_table[i].filename);

			FreeHashTable(file_table[i].hash_table);
		}
	}

	// Free the table of contents
	free(file_table);

	BF_Close();

	return HT_OK;
}

HT_ErrorCode HT_CreateIndex(const char *filename, int depth)
{
	// insert code here
	printf("Creating File %s ...\n", filename);

	int file_desc;

	// Check if the file exists first and then just open it if it does
	// Else create the file and catch any errors
	if (access(filename, F_OK) == 0)
	{
		printf("The file %s already exists\n", filename);
		return -1;
	}
	else
	{
		CALL_BF(BF_CreateFile(filename), "Error creating file in HT_CreateIndex\n");
	}

	// Open the file and catch any errors
	CALL_BF(BF_OpenFile(filename, &file_desc), "Error opening file in HT_CreateIndex\n");

	// Allocate a block for the first block of the file
	// The first block will inlude the metadata of the file
	BF_Block *first_block;
	BF_Block_Init(&first_block);
	CALL_BF(BF_AllocateBlock(file_desc, first_block), "Error allocating block in HT_CreateIndex\n");

	// Get the data of the block and cast it to HT_info
	HT_info *ht_info = (HT_info *)BF_Block_GetData(first_block);
	memset(ht_info, 0, BF_BLOCK_SIZE);

	// Initialize the metadata of the file
	ht_info->type = HashFile;
	ht_info->number_of_records_per_block = (BF_BLOCK_SIZE - sizeof(HT_block_info)) / sizeof(Record);
	ht_info->global_depth = depth;
	ht_info->first_hash_table_block_id = -1;
	ht_info->number_of_hash_table_blocks = 0;
	ht_info->cells_per_hash_block = (BF_BLOCK_SIZE - sizeof(HashTable_Block_metadata)) / sizeof(HashTableCell);

	// Set the block as dirty because we changed it and unpin it
	BF_Block_SetDirty(first_block);
	CALL_BF(BF_UnpinBlock(first_block), "Error unpinning block in HT_CreateIndex\n");
	BF_Block_Destroy(&first_block);

	// Allocate blocks for the hash table
	// UpdateHashTable(file_desc, depth);

	// Close the file before exiting
	CALL_BF(BF_CloseFile(file_desc), "Error closing file\n");

	return HT_OK;
}

HT_ErrorCode HT_OpenIndex(const char *fileName, int *indexDesc)
{

	// insert code here
	printf("Opening file %s ...\n", fileName);

	// Get file descrtiptor for fileName
	int file_desc;
	CALL_BF(BF_OpenFile(fileName, &file_desc), "Error opening file in HT_OpenIndex\n");

	// Pin the first block so it can be easily accessed, without the need to take it from the disk
	BF_Block *first_block;
	BF_Block_Init(&first_block);
	CALL_BF(BF_GetBlock(file_desc, 0, first_block), "Error getting block in HT_OpenIndex\n");

	// Check that the file is a hash table
	HT_info *ht_info = (HT_info *)BF_Block_GetData(first_block);
	if (ht_info->type != HashFile)
	{
		printf("Error opening file %s\n (File is not a hash table)\n", fileName);

		// Unpin and Destroy the block because we don't need it anymore
		CALL_BF(BF_UnpinBlock(first_block), "Error unpinning block in HT_OpenIndex\n");
		BF_Block_Destroy(&first_block);

		return HT_ERROR;
	}

	// Destroy the block because we don't need it anymore
	BF_Block_Destroy(&first_block);

	// Update the table of contents
	int available_index = -1;
	for (int i = 0; i < MAX_OPEN_FILES; i++)
	{
		if (file_table[i].file_desc == -1)
		{
			available_index = i;
			break;
		}
	}

	if (available_index == -1)
	{
		printf("No space to open the file. MAX_OPEN_FILES = %d\n", MAX_OPEN_FILES);
		return HT_ERROR;
	}

	printf("File %s opened with file desc %d\n", fileName, file_desc);

	file_table[available_index].file_desc = file_desc;
	file_table[available_index].filename = malloc(strlen(fileName) + 1);
	file_table[available_index].hash_table = LoadTableFromDisk(file_desc);
	strcpy(file_table[available_index].filename, fileName);

	// Return the index of the file
	*indexDesc = available_index;

	return HT_OK;
}

HT_ErrorCode HT_CloseFile(int indexDesc)
{

	if (indexDesc < 0 || indexDesc > MAX_OPEN_FILES)
	{
		printf("Error closing file with index %d\n (Out of range)\n", indexDesc);
		return HT_ERROR;
	}

	// Get the file descriptor from the table of contents
	int file_desc = file_table[indexDesc].file_desc;
	if (file_desc == -1)
	{
		printf("Error closing file with index %d\n (File not open)\n", indexDesc);
		return HT_ERROR;
	}

	// Get the first block of the file
	BF_Block *first_block;
	BF_Block_Init(&first_block);
	BF_GetBlock(file_desc, 0, first_block);

	// Set the block as dirty because we changed it and unpin it
	BF_Block_SetDirty(first_block);
	CALL_BF(BF_UnpinBlock(first_block), "Error unpinning block in HT_CloseFile\n");

	// Destroy the block because we don't need it anymore
	BF_Block_Destroy(&first_block);

	// Close the file
	CALL_BF(BF_CloseFile(file_desc), "Error closing file in HT_CloseFile\n");

	// Update the contents table
	file_table[indexDesc].file_desc = -1;
	free(file_table[indexDesc].filename);
	file_table[indexDesc].filename = NULL;

	FreeHashTable(file_table[indexDesc].hash_table);
	file_table[indexDesc].hash_table = NULL;

	return HT_OK;
}

HT_ErrorCode HT_InsertEntry(int indexDesc, Record record)
{
	int file_desc = file_table[indexDesc].file_desc;

	if (file_desc == -1)
	{
		printf("Error inserting entry in file with index %d\n (File not open)\n", indexDesc);
		return HT_ERROR;
	}

	// Get the first block of the file
	BF_Block *file_metadata_block;
	BF_Block_Init(&file_metadata_block);
	BF_GetBlock(file_desc, 0, file_metadata_block);
	HT_info *ht_info = (HT_info *)BF_Block_GetData(file_metadata_block);

	int hash_table_size = 1 << ht_info->global_depth;
	uint hash_value = hash_function(record.id, hash_table_size);
	HashTableCell *hash_table;

	if (file_table[indexDesc].hash_table == NULL)
	{
		hash_table = CreateHashTable(file_desc, ht_info->global_depth);
		file_table[indexDesc].hash_table = hash_table;
	}
	else
	{
		hash_table = file_table[indexDesc].hash_table;
	}

	BF_Block *record_block;
	BF_Block_Init(&record_block);

	int record_block_id = hash_table[hash_value].block_id;

	// Here we have 3 cases

	// 1. The block_id is -1, which means that there is no block for this hash value
	// In this case we allocate a block and insert the record there

	// 2. The block_id is not -1 and the block is full
	// In this case we need to split a block and insert the record there
	// If the table has to be doubled, we need to update the metadata of the file

	// 3. The block_id is not -1 and the block is not full
	// In this case we just insert the record to the block

	size_t block_data_offset = 0;
	void* block_data = NULL;

	if (record_block_id == -1)
	{
		// Allocate a block for the record
		CALL_BF(BF_AllocateBlock(file_desc, record_block), "Error allocating block in DoubleHashTable\n");
		block_data = (void *)BF_Block_GetData(record_block);
		memset(block_data, 0, BF_BLOCK_SIZE);

		// Initialize the block metadata
		HT_block_info *block_info = (HT_block_info *)block_data;
		block_info->local_depth = 1;
		block_info->number_of_records_on_block = 0;
		block_data_offset = sizeof(HT_block_info);
	}
	else
	{
		CALL_BF(BF_GetBlock(file_desc, record_block_id, record_block), "Error getting block in DoubleHashTable\n");
		block_data = (void *)BF_Block_GetData(record_block);

		// Check if the block is full
		HT_block_info *block_info = (HT_block_info *)block_data;
		if(block_info->number_of_records_on_block == ht_info->number_of_records_per_block)
		{
			hash_table = SplitBlock(record_block_id, file_desc, hash_table, hash_function(record.id, hash_table_size));

			hash_table_size = 1 << ht_info->global_depth; // Possible bug
			

			// Rehash the records of the block
			BF_Block *new_record_block;
			BF_Block_Init(&new_record_block);
			block_info->number_of_records_on_block = 0;
			for(int i = 0; i < ht_info->number_of_records_per_block; i++){
				Record* record_location = (Record*)(block_data + sizeof(HT_block_info) + i * sizeof(Record));
				Record temp_record;

				// Keep the record in a temp variable
				memcpy(&temp_record, record_location, sizeof(Record));

				// Delete the record from the block 
				memset(record_location, 0, sizeof(Record));
				uint new_hash_value = hash_function(temp_record.id, hash_table_size);

				// Use the hash value to find the block id
				int new_record_block_id = hash_table[new_hash_value].block_id;
				CALL_BF(BF_GetBlock(file_desc, new_record_block_id, new_record_block), "Error getting block in DoubleHashTable\n");
				void* new_block_data = (void*)BF_Block_GetData(new_record_block);

				// Get the metadata of the block
				HT_block_info *new_block_info = (HT_block_info *)new_block_data;

				// Find the offset in the block where the record should be inserted
				size_t new_block_data_offset = sizeof(HT_block_info) + new_block_info->number_of_records_on_block * sizeof(Record);

				// Copy the record to the rigth block after split
				memcpy(new_block_data + new_block_data_offset, &temp_record, sizeof(Record));
				new_block_info->number_of_records_on_block++;

				// Set the block as dirty because we changed it and unpin it
				BF_Block_SetDirty(new_record_block);
				CALL_BF(BF_UnpinBlock(new_record_block), "Error unpinning block in DoubleHashTable\n");
			}
			BF_Block_Destroy(&new_record_block);
			 
		}

		// TODO
		// Rehash the new record
		hash_value = hash_function(record.id, hash_table_size);
		record_block_id = hash_table[hash_value].block_id;

		// TODO
		CALL_BF(BF_GetBlock(file_desc, record_block_id, record_block), "Error getting block in DoubleHashTable\n");
		block_data = (void *)BF_Block_GetData(record_block);

		block_data_offset = sizeof(HT_block_info) + block_info->number_of_records_on_block * sizeof(Record);
	}

	// Insert the record to the right place in the block
	Record *records_in_block = (Record *)(block_data + block_data_offset);
	memcpy(records_in_block, &record, sizeof(Record));

	// Update the metadata of the block
	HT_block_info *block_info = (HT_block_info *)block_data;
	block_info->number_of_records_on_block++;

	return HT_OK;
}

HT_ErrorCode HT_PrintAllEntries(int indexDesc, int *id)
{
	// insert code here
	return HT_OK;
}

//////////////////////////////////////////////// HELPERS //////////////////////////////////////////////////

// Show all files in content table
void show_files(void)
{
	printf("Showing files...\n");
	for (int i = 0; i < MAX_OPEN_FILES; i++)
	{
		if (file_table[i].file_desc != -1 && file_table[i].filename != NULL)
			printf("File %d: %s with file desc %d\n", i, file_table[i].filename, file_table[i].file_desc);
	}
}