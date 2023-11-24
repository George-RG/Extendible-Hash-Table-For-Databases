#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hash_file.h"

#define RECORDS_NUM 1000 // you can change it if you want
#define GLOBAL_DEPT 2	 // you can change it if you want
#define FILE_NAME "data.db"

#define CALL_OR_DIE(call)     	\
{                             	\
	HT_ErrorCode code = call; 	\
	if (code != HT_OK)        	\
	{                         	\
	printf("Error\n");    		\
	exit(code);           		\
	}                         	\
}

int main(void)
{
    CALL_OR_DIE(HT_Init());

	int indexDesc;
	CALL_OR_DIE(HT_CreateIndex(FILE_NAME, GLOBAL_DEPT));
	CALL_OR_DIE(HT_OpenIndex(FILE_NAME, &indexDesc));

    show_files();

    CALL_OR_DIE(HT_CloseFile(indexDesc));

    return 0;
}