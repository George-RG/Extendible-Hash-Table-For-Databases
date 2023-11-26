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
    if(HT_Init() != HT_OK)
		goto exit_program;

	int indexDesc;

	if(HT_CreateIndex(FILE_NAME, GLOBAL_DEPT) != HT_OK)
		goto exit_program;

	if(HT_OpenIndex(FILE_NAME, &indexDesc) != HT_OK)
		goto exit_program;

    show_files();

	if(HT_CloseFile(indexDesc) != HT_OK)
		goto exit_program;

	if(HT_Close() != HT_OK)
		goto exit_program;

    return 0;


	exit_program:
		HT_Close();
		return -1;
}