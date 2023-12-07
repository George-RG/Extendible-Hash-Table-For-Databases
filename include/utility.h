#ifndef UTILITY_H
#define UTILITY_H

#include "ht.h"
#include "bf.h"
#include "record.h"

#define CALL_BF(call,error_msg)       	\
{										\
	BF_ErrorCode code = call; 			\
  	if (code != BF_OK) 					\
  	{       							\
		printf(error_msg);  			\
    	BF_PrintError(code);    		\
    	return code;        			\
  	}                         			\
}

#define CALL_BF_PTR(call,error_msg)     \
{										\
	BF_ErrorCode code = call; 			\
  	if (code != BF_OK) 					\
  	{       							\
		printf(error_msg);  			\
    	BF_PrintError(code);    		\
    	return NULL;        			\
  	}                         			\
}

typedef unsigned int uint;

typedef enum {
    false,
    true
} bool;

HashTableCell* CreateHashTable(int file_dsc, int depth);
HashTableCell* LoadTableFromDisk(int file_dsc);
HashTableCell* DoubleHashTable(int file_dsc, int old_depth, HashTableCell* hash_table_old);
HashTableCell* SplitBlock(int block_id, int file_dsc, HashTableCell *hash_table, uint hash_value, int* created_block_id);
BF_ErrorCode UpdateHashTableValue(HashTableCell *hashtable, int index, int value, int file_dsc);
int FreeHashTable(HashTableCell* hash_table);

int min(int a, int b);
uint hash_function(uint x, size_t size);
int InsertRecordInBlock(void* data,Record record, int max_records);
int RehashRecords(void* block_data, void* new_block_data, int record_block_id, int new_record_block_id, HashTableCell* hash_table, uint hash_table_size);

#endif // UTILITY_H