#ifndef HT_H
#define HT_H

typedef enum {
    false,
    true
} bool;

typedef enum {
    Heap,
    HashTable,
    Tree,
} FileType;

typedef struct {
    FileType type;
	int local_depth;
	bool block_has_space;
	int number_of_records_on_block;
} HT_block_info;

typedef struct {
	FileType type;
	int number_of_records_per_block;
	int global_depth;
} HT_info;

typedef struct {
    int file_desc;
    char* filename;
} content_table_entry;

// The table that contains the file descriptors and the filenames
content_table_entry* file_table;

#endif // HT_H