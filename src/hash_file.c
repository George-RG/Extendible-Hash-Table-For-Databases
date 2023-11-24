#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
	ht_info->type = HashTable;
	ht_info->number_of_records_per_block = (BF_BLOCK_SIZE - sizeof(HT_block_info)) / sizeof(Record);
	ht_info->global_depth = depth;

	// Set the block as dirty because we changed it and unpin it
	BF_Block_SetDirty(first_block);
	CALL_BF(BF_UnpinBlock(first_block),"Error unpinning block in HT_CreateIndex\n");
	BF_Block_Destroy(&first_block);

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
	if (ht_info->type != HashTable) {
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
	//insert code here
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