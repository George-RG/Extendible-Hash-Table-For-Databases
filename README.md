# Data-Base-Project-2

## Student Information

- __*Full names*__ : Γκίκας Στέφανος, Γιώργος Νικολαΐδης, Ιωάννα Πούλου
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

- __*HT_Init*__ : This function is used to initialize the BF layer which we will use for writting to the disk. It also creates a table in the memory with the information of the files that we have opened. These informations are the file descriptor, the name of the file and hash table of the file. The table is global to the file with all HT functions because we want only these functions to have access to it. The structure of the file table is the following:
  
    ```c
    typedef struct {
        int file_desc;
        char* filename;
        HashTableCell* hash_table;
    } content_table_entry;
    ```

    </br>

- __*HT_CreateIndex*__ : This function is used for the creation of a new Hash Table file. It creates a new file with the given name and then it opens it to initialize the metadata of the file which are stored in the first block of the file. The metadata are the following:

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
After the initialization of the metadata, it sets metada block as dirty and closes the file.
    </br>

- __*HT_OpenIndex*__ : This function is used to open an existing Hash Table file. It opens the file with the given name and then it reads the metadata from the first block of the file to check if it is Hash Table file and it leaves this block pinned until the files is closed. After that, the file table is updated, if there is space for a new file to be opened. Finally it returns the index of the file in the file table. 

    </br>

- __*HT_CloseFile*__ : This function is used to close an open Hash Table file with a given index in the file table. It sets as dirty the metadata block of the file and unpins it. After that, it closes the file and removes it from the file table, freeing the memory that it was using.

    </br>