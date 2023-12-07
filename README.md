# Data-Base-Project-2

## Student Information

- __*Full names*__ : Στέφανος Γκίκας, Γιώργος Νικολαΐδης, Ιωάννα Πούλου
- __*Academic ID*__ : 1115202100025, 1115202100118, 1115202100161

## Makefile and main function

- __*Makefile*__ : The makefile is used to compile the program. It contains the following commands:

  - __*make*__ : Compiles the program.
  - __*make run*__ : Runs the program.
  - __*make clean*__ : Deletes the executable file and the object files.
  - __*make remove_db*__ : Deletes the database file.

    </br>
- __*src/main.c*__ : The main function that we have created is used to test all the functions that we have implemented. It creates a database hash file and then it inserts some records in it (1000, this can be changed for testing) . After that, it prints the records that it has inserted and then it prints out the statistics of a file with a specific name. Finally, it closes the file.

## Functions, structs and design choices

### General

    In order to have a better understanding of what is going on below, we will first explain some general things about the project.

The structure that we created was made with the following things in mind:

- <a id="reliability_subsection"></a> __*Reliability*__<br>
  We wanted to make sure that the data as well as the table are secure. So while we use the hashtable directly from the memory of the system, we also keep an exact copy of it in the disk. This way, if the system crashes, we can recover the data from the disk.
- __*Performance*__<br>
  We also kept in mind that this structure may be used for tons of data so no slow downs are allowed. For this reason we keep the hashtable in the system's memory and only update it to the disk when it is necessary. This way we ensure the fast speed of both reading and writing data for the majority of the actions.

  > Another optimization that we made is to always keep the metadata of the file pinned in the memory. This way we avoid the overhead of pinning and unpinning it every time we want to use it. ( It is used alot XD )
  >
- __*Space efficiency*__<br>
  We wanted to make sure that we use the minimum amount of space possible. To achieve this, we try to utilize the allocated blocks to the maximum. For example, when writing the hash table to the disk, we first write in the pre allocated blocks that we already had the old hash table written on. If there is not enough space in these blocks, we allocate new ones and write the hash table there.

  > This forces as to use a list structure for the hash table blocks so that we can easily add new blocks to the end of the list as well as to keep track of the blocks that we have already used as they are among all the data blocks of the file.
  >

### <a id="user_function_subsection"></a> User Functions

> __NOTE__ : Any function that returns an int returns `HT_OK` on success and `HT_ERROR` on failure.

- __*HT_Init*__  <br>
  This function is used to initialize the BF layer which we will use for writting to the disk. It also creates a table in the memory with the information of the files that are open. These information include the file descriptor, the name of the file and a pointer to the hash table of the file in the memory. The table is global to the file with all HT functions because we want only these functions to have access to it.

  The structure of the file table is the following:

  ```c
  typedef struct {
      int file_desc;
      char* filename;
      HashTableCell* hash_table;
  }
  ```
  </br>
- __*HT_CreateIndex*__ <br>
  This function is used for the creation of a new Hash Table file. It creates a new file with the given name and then initializes the metadata of the file which are stored in the first block of the file.

  The file metadata structure is the following:

  ```c
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
  ```
  </br>

  > In any case of failure (ex. File is already open) an error message is printed in stderr and the the function returns `HT_ERROR`
  >

  </br>
- __*HT_OpenIndex*__ <br>
  This function is responsible for loading an Index with a specific name from the disk. For later use, it pins the first block of the file which contains the metadata of the file and it leaves it pinned until the file is closed. Also if the file contains a hash table, it loads it in the memory from the file using the [LoadTableFromDisk](#function_loadtablefromdisk). Finally, it returns the index of the file in the file table

  > Along with all the above, the function performs some checks to see if the file is already open or if the file is a hash table file. If any of these checks fail, the function returns `HT_ERROR`.
  >

  </br>
- __*HT_CloseFile*__ <br>
  This function is used to close an open Hash Table file with a given index in the file table. It sets as dirty the metadata block of the file and unpins it. After that, it closes the file and removes it from the file table, freeing the memory that was used.

  </br>
- <a id="function_ht_insertentry"></a> __*HT_InsertEntry*__ <br>
    This function inserts a new record in the Hash Table file with a given index in the file table. It uses a [FNV-1a](https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function) __hash_function__ to hash the record ID

    > Of this hash value only the __N__ most significant bits are used, where __N__ is the global depth of the hash table. This way we can use the same hash function for all the hash tables and we can change the global depth of the hash table without having to rehash all the records.

  There are 3 cases when inserting a record:

  - __*Case 1*__ : There is no block for the hash value so we allocate a new one and insert the record there.
  - __*Case 2*__ : The block for the hash value has space for the record so we insert it there.
  - __*Case 3*__ : The block for the hash value is full so we split it with [__SplitBlock__](#function_splitblock) and rehash all the records that were in this block using [__RehashRecords__](#function_rehashrecords). Τhe new record gets rehashed as well and inserted in the right block. When splitting a block, we may have to double the size of the hash table using [__DoubleHashTable__](#function_doublehashtable). We repeatidly do this until until we either can insert the record in a block or `MAX_SPLITS` amout of splits have reached.
  
    >In the latter case we give up because this can lead to an infinite loop of splits if all the records hash in the same value repeaditly. This is a very rare case but we have to take it into account.
  
  <br>

  > After inserting the record, we set as dirty the block that we inserted the record in and we unpin it. If anything goes wrong, we return `HT_ERROR`.

  <br>
- __*HT_PrintAllEntries*__ <br>
    The function is used to print all the records of a file with record.id equal to the given id. If the id is `NULL` then it prints all the records of the file. This is done by iterating through all the blocks of the hash table.

  <br>
- __*HashStatistics*__ <br>
    Given a name, print some statistics for the file. 
    
    >In case the file is not open , the function opens it to get the statistics and then closes it again.

    The statistics include the following:
    - Number of blocks that the file has
        - how many of them are used for the hash table 
        - how many for the records 
        - how many for metadata
    - Maximum amout of records that a block contains
    - Minimum amount of records that a block contains
    - Average amount of records stored in a block among all the blocks.



### <a id="internal_function_subsection"></a> Internal Functions

- <a id="function_splitblock"></a> 
    __*SplitBlock*__ <br>
    This function splits buddies into two blocks. It is used when we have to split a block because it is full. This function also calls [DoubleHashTable](#function_doublehashtable) if the global depth of the hash table is equal to the local depth of the block that we are splitting. In other words we double the size of the hash table if the block that fills up does not have any buddies.<br>

    After the allocation of the new block we call the [RehashRecords](#function_rehashrecords) function to rehash all the records of the old block to the old and new one. Finally, we update the metadata of the file and we set as dirty the old and the new block.

- <a id="function_rehashrecords"></a> 
    __*RehashRecords*__ <br>

    This function is responsible for rehashing all the records of a block. It is used when we split a block with [SplitBlock](#function_splitblock) and we rehash all the records of the old block to the old and new one. 

    > An important technical __note__ is that this function takes as input two pointers to the block data in order to avoid opening and closing the blocks again.


- <a id="function_doublehashtable"></a> 
    __*DoubleHashTable*__ <br>

    This function is used to double the size of the hash table. It is used when we need to split a block ([SplitBlock](#function_splitblock)) but the block does not have any buddies. In this case we have to double the size of the hash table so that we can split the block. 

    > This function is also used to create a hash table if the `old_hash_table` pointer is `NULL` but we recomend using the function [CreateHashTable](#function_createhashtable) for that operation.

    > __NOTE__ : As said above ([Reliability section](#reliability_subsection)), this function updates both the memory hash table and the disk hash table.

- <a id="function_createhashtable"></a>
    __*CreateHashTable*__ <br>

    This function is used to create a hash table both on the disk and the memory. It is used when we add the first entry to a file ([HT_InsertEntry](#function_ht_insertentry)) and we need to create a hash table for the file.

    > __TECHNICAL NOTE__ : This is not an actual function. It actually calls the function [DoubleHashTable](#function_doublehashtable) with the arguments a little bit changed.
