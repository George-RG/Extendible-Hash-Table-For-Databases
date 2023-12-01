#ifndef HT_H
#define HT_H

typedef enum {
    Heap,
    HashFile,
    Tree,
} FileType;

typedef struct {
	int local_depth;
	int number_of_records_on_block;
} HT_block_info;

typedef struct {
    // General info
	FileType type;
	int number_of_records_per_block;
	int global_depth;

    // Hash table info
    int first_hash_table_block_id;
    int number_of_hash_table_blocks;
    int cells_per_hash_block;
} HT_info;

typedef struct {
    int block_id;
} HashTableCell;

typedef struct {
    int file_desc;
    char* filename;
    HashTableCell* hash_table;
} content_table_entry;

typedef struct {
    int next_block_id;
} HashTable_Block_metadata;

#endif // HT_H