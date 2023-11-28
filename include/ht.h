#ifndef HT_H
#define HT_H

typedef enum {
    false,
    true
} bool;

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
	FileType type;
	int number_of_records_per_block;
	int global_depth;
    int first_hash_table_block_id;
    int number_of_hash_table_blocks;

    int cells_per_hash_block;
} HT_info;

typedef struct {
    int file_desc;
    char* filename;
} content_table_entry;

// The table that contains the file descriptors and the filenames
content_table_entry* file_table;

typedef struct {
    int block_id;
} HashTableCell;

typedef struct {
    int next_block_id;
} HashTable_Block_metadata;

#endif // HT_H