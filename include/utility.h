#ifndef UTILITY_H
#define UTILITY_H

#include "hash_file.h"
#include "ht.h"
#include "bf.h"

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
int FreeHashTable(HashTableCell* hash_table);
HashTableCell* SplitBlock(int block_id, int file_dsc, HashTableCell* hash_table, uint hash_value);
int min(int a, int b);
uint hash_function(unsigned int x, unsigned int size);


#endif // UTILITY_H