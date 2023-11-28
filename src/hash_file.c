#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

// #include "ht.h"
#include "hash_file.h"
#include "ht.h"

#define CALL_BF(call,error_msg)       	\
{										\
	BF_ErrorCode code = call; 			\
  	if (code != BF_OK) 					\
  	{       							\
		printf(error_msg);  			\
    	BF_PrintError(code);    		\
    	return HT_ERROR;        		\
  	}                         			\
}

HT_ErrorCode HT_Init() {
	//insert code here
	BF_Init(LRU);

	// Make the table for the file descriptors
	file_table = calloc(MAX_OPEN_FILES, sizeof(content_table_entry));
	for (int i = 0; i < MAX_OPEN_FILES; i++) {
		file_table[i].file_desc = -1;
		file_table[i].filename = NULL;
	}

	return HT_OK;
}

HT_ErrorCode HT_Close() {
	//insert code here
	// Close all files
	for (int i = 0; i < MAX_OPEN_FILES; i++) {
		if (file_table[i].file_desc != -1) {
			if(HT_CloseFile(i) != HT_OK)
			{
				printf("Error closing file %s\n", file_table[i].filename);
				return HT_ERROR;
			}
		}
	}

	// Free the table of contents
	free(file_table);

	BF_Close();

	return HT_OK;
}

int min(int a, int b)
{
	if(a < b)
		return a;
	else
		return b;
}

HashTableCell* DoubleHashTable(int file_dsc, int old_depth, HashTableCell* hash_table_old)
{
	// Find the blocks that the table was stored in
	BF_Block* metadata_block;
	BF_Block_Init(&metadata_block);

	CALL_BF(BF_GetBlock(file_dsc, 0, metadata_block), "Error getting block in UpdateHashTable\n");

	HT_info* ht_info = (HT_info*)BF_Block_GetData(metadata_block);

	size_t new_table_size = 1 << (old_depth+1);

	// Create a new table in memory
	HashTableCell* hash_table_new;
	hash_table_new = malloc(sizeof(HashTableCell) * new_table_size);
	
	if (ht_info->first_hash_table_block_id == -1 || ht_info->number_of_hash_table_blocks == 0 || hash_table_old == NULL || old_depth == 0)
	{
		// Create the table from scratch΄
		for(int i = 0; i < new_table_size; i++)
			hash_table_new[i].block_id = -1;
	}
	else
	{
		// Create the new table from the old one
		unsigned int old_table_size = 1 << old_depth;

		for(int i = 0; i < old_table_size; i++)
		{
			hash_table_new[i].block_id = hash_table_old[i].block_id;
			hash_table_new[i+1].block_id = hash_table_old[i].block_id;
		}

		// Free the old table
		free(hash_table_old);
	}

	int* old_used_blocks = NULL;
	int old_used_blocks_num = ht_info->number_of_hash_table_blocks;
	if (ht_info->first_hash_table_block_id != -1) 
	{
		old_used_blocks = malloc(sizeof(int) * ht_info->number_of_hash_table_blocks);
		old_used_blocks[0] = ht_info->first_hash_table_block_id;

		// Find the blocks that the table was stored in
		BF_Block* hash_table_block;
		BF_Block_Init(&hash_table_block);
		int next_block_id=  ht_info->first_hash_table_block_id;

		for(int i = 1; i < ht_info->number_of_hash_table_blocks; i++)
		{
			if(next_block_id == -1)
			{
				printf("Error in DoubleHashTable(next_block_id == -1)\n");
				return NULL;
			}

			CALL_BF(BF_GetBlock(file_dsc, next_block_id, hash_table_block), "Error getting first block in DoubleHashTable\n");
			HashTable_Block_metadata* first_block_metadata = (HashTable_Block_metadata*)BF_Block_GetData(hash_table_block);
			next_block_id = first_block_metadata->next_block_id;
			old_used_blocks[i] = next_block_id;
			// Unpin the block
			CALL_BF(BF_UnpinBlock(hash_table_block), "Error unpinning block in DoubleHashTable\n");
		}

		// Destroy the block because we don't need it anymore
		BF_Block_Destroy(&hash_table_block);
	}	

	size_t remaining_size = new_table_size;
	int remaining_preallocated_blocks = old_used_blocks_num;
	BF_Block* hash_table_block;
	BF_Block_Init(&hash_table_block);
	int cur_block_id = -1;

	int* new_block_ids = malloc(sizeof(int) * ceil((double)new_table_size / ht_info->cells_per_hash_block));
	int new_blocks_num = 0;	
	while(remaining_size > 0)
	{
		if(remaining_preallocated_blocks > 0)
		{
			cur_block_id = old_used_blocks[old_used_blocks_num - remaining_preallocated_blocks];
			CALL_BF(BF_GetBlock(file_dsc, cur_block_id, hash_table_block), "Error getting block in DoubleHashTable\n");	
			remaining_preallocated_blocks--;
		}
		else
		{
			CALL_BF(BF_AllocateBlock(file_dsc, hash_table_block), "Error allocating block in DoubleHashTable\n");
			CALL_BF(BF_GetBlockCounter(file_dsc, &cur_block_id),"Error getting block count in DoubleHashTable");
			cur_block_id--;
		}

		if(ht_info->first_hash_table_block_id == -1)
		{
			if(cur_block_id == -1)
			{
				printf("Error in DoubleHashTable(cur_block_id == -1)\n");
				return NULL;
			}

			ht_info->first_hash_table_block_id = cur_block_id;
		}
		
		// Copy the table to the block
		void* block_data = (void*)BF_Block_GetData(hash_table_block);
		void* table_data = (void*)hash_table_new;
		int offset_in_table = (new_blocks_num * ht_info->cells_per_hash_block) * sizeof(HashTableCell);
		int offset_in_block = sizeof(HashTable_Block_metadata);

		int amount_to_copy = min(remaining_size, ht_info->cells_per_hash_block);
		memcpy(block_data + offset_in_block, table_data + offset_in_table, amount_to_copy * sizeof(HashTableCell));

		// Set the metadata of the block
		HashTable_Block_metadata* block_metadata = (HashTable_Block_metadata*)block_data;
		block_metadata->next_block_id = -1;

		BF_Block_SetDirty(hash_table_block);
		CALL_BF(BF_UnpinBlock(hash_table_block), "Error unpinning block in DoubleHashTable\n");

		new_block_ids[new_blocks_num] = cur_block_id;
		new_blocks_num++;
		remaining_size -= amount_to_copy;
	}

	// Update the metadata of the blocks

	for(int i = 0; i < new_blocks_num - 1; i++)
	{
		CALL_BF(BF_GetBlock(file_dsc, new_block_ids[i], hash_table_block), "Error getting block in DoubleHashTable\n");
		HashTable_Block_metadata* block_metadata = (HashTable_Block_metadata*)BF_Block_GetData(hash_table_block);
		block_metadata->next_block_id = new_block_ids[i+1];
		BF_Block_SetDirty(hash_table_block);
		CALL_BF(BF_UnpinBlock(hash_table_block), "Error unpinning block in DoubleHashTable\n");
	}

	// Destroy the block because we don't need it anymore
	BF_Block_Destroy(&hash_table_block);

	// Free the unused tables
	free(new_block_ids);
	if(old_used_blocks != NULL)
		free(old_used_blocks);

	return hash_table_new;
}	

HT_ErrorCode HT_CreateIndex(const char *filename, int depth) {
	//insert code here
	printf("Creating File %s ...\n", filename);

	int file_desc;

	// Check if the file exists first and then just open it if it does
	// Else create the file and catch any errors
	if(access(filename, F_OK) == 0)
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
	BF_Block* first_block;
	BF_Block_Init(&first_block);
	CALL_BF(BF_AllocateBlock(file_desc, first_block), "Error allocating block in HT_CreateIndex\n");

	// Get the data of the block and cast it to HT_info
	HT_info* ht_info = (HT_info*)BF_Block_GetData(first_block);
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
	CALL_BF(BF_UnpinBlock(first_block),"Error unpinning block in HT_CreateIndex\n");
	BF_Block_Destroy(&first_block);

	// Allocate blocks for the hash table
	// UpdateHashTable(file_desc, depth);

	// Close the file before exiting
	CALL_BF(BF_CloseFile(file_desc), "Error closing file\n");

	return HT_OK;
}

HT_ErrorCode HT_OpenIndex(const char *fileName, int *indexDesc){

	//insert code here
	printf("Opening file %s ...\n", fileName);

	// Get file descrtiptor for fileName
	int file_desc;
	CALL_BF(BF_OpenFile(fileName, &file_desc), "Error opening file in HT_OpenIndex\n");

	// Pin the first block so it can be easily accessed, without the need to take it from the disk
	BF_Block *first_block;
	BF_Block_Init(&first_block);
	CALL_BF(BF_GetBlock(file_desc, 0, first_block), "Error getting block in HT_OpenIndex\n");

	// Check that the file is a hash table
	HT_info *ht_info = (HT_info*)BF_Block_GetData(first_block);
	if (ht_info->type != HashFile) {
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
	for (int i = 0; i < MAX_OPEN_FILES; i++) {
		if (file_table[i].file_desc == -1) {
			available_index = i;
			break;
		}
	}

	if(available_index == -1)
	{
		printf("No space to open the file. MAX_OPEN_FILES = %d\n", MAX_OPEN_FILES);
		return HT_ERROR;
	}

	printf("File %s opened with file desc %d\n", fileName, file_desc);

	file_table[available_index].file_desc = file_desc;
	file_table[available_index].filename = malloc(strlen(fileName) + 1);
	strcpy(file_table[available_index].filename, fileName);

	// Return the index of the file
	*indexDesc = available_index;

	return HT_OK;
}

HT_ErrorCode HT_CloseFile(int indexDesc) {
	
	if (indexDesc < 0 || indexDesc > MAX_OPEN_FILES) {
		printf("Error closing file with index %d\n (Out of range)\n", indexDesc);
		return HT_ERROR;
	}

	// Get the file descriptor from the table of contents
	int file_desc = file_table[indexDesc].file_desc;
	if (file_desc == -1) {
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

	return HT_OK;
}

HT_ErrorCode HT_InsertEntry(int indexDesc, Record record) {


	return HT_OK;
}

HT_ErrorCode HT_PrintAllEntries(int indexDesc, int *id) {
	//insert code here
	return HT_OK;
}

//////////////////////////////////////////////// HELPERS //////////////////////////////////////////////////

// Show all files in content table
void show_files(void){
	printf("Showing files...\n");
	for(int i = 0; i < MAX_OPEN_FILES; i++){
		if (file_table[i].file_desc != -1 && file_table[i].filename != NULL) 
			printf("File %d: %s with file desc %d\n", i, file_table[i].filename, file_table[i].file_desc);
	}
}