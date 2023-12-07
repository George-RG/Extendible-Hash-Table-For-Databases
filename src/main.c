#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "hash_file.h"

#define RECORDS_NUM 1000000 // you can change it if you want
#define GLOBAL_DEPT 2 // you can change it if you want
#define FILE_NAME "data.db"
#define FILE_NAME2 "data2.db"

const char *names[] = {
	"Yannis",
	"Christofos",
	"Sofia",
	"Marianna",
	"Vagelis",
	"Maria",
	"Iosif",
	"Dionisis",
	"Konstantina",
	"Theofilos",
	"Giorgos",
	"Dimitris"};

const char *surnames[] = {
	"Ioannidis",
	"Svingos",
	"Karvounari",
	"Rezkalla",
	"Nikolopoulos",
	"Berreta",
	"Koronis",
	"Gaitanis",
	"Oikonomou",
	"Mailis",
	"Michas",
	"Halatsis"};

const char *cities[] = {
	"Athens",
	"San Francisco",
	"Los Angeles",
	"Amsterdam",
	"London",
	"New York",
	"Tokyo",
	"Hong Kong",
	"Munich",
	"Miami"};

#define CALL_OR_DIE(call)         \
	{                             \
		HT_ErrorCode code = call; \
		if (code != HT_OK)        \
		{                         \
			printf("Error\n");    \
			exit(code);           \
		}                         \
	}

int main(void)
{
	if (HT_Init() != HT_OK)
		goto exit_program;

	int indexDesc;

	int flag = 1;
	// If the file already exists, flag is 0, else 1
	if (access(FILE_NAME, F_OK) != -1)
		flag = 0;

	HT_CreateIndex(FILE_NAME, GLOBAL_DEPT);

	if (HT_OpenIndex(FILE_NAME, &indexDesc) != HT_OK)
		goto exit_program;

	// Try to open a file that is already open
	int index_of_reopen;
	if(HT_OpenIndex(FILE_NAME, &index_of_reopen) != HT_OK)
		goto exit_program;

	// See what the file table contains
	show_files();

	// Close the file
	if(HT_CloseFile(index_of_reopen) != HT_OK)
		goto exit_program;
	
	if(flag == 0)
	{
		printf("File already exists\n");
		printf("Printing all entries...\n");
		HT_PrintAllEntries(indexDesc, NULL);
		goto exit_program;
	}

	Record record;
	srand(12569874);
	int r;
	printf("Inserting %d records in data.db...\n", RECORDS_NUM);
	for (int id = 0; id < RECORDS_NUM; ++id)
	{
		// create a record with random data
		memset(&record, 0, sizeof(Record));
		record.id = id;
		r = rand() % 12;
		memcpy(record.name, names[r], strlen(names[r]) + 1);
		r = rand() % 12;
		memcpy(record.surname, surnames[r], strlen(surnames[r]) + 1);
		r = rand() % 10;
		memcpy(record.city, cities[r], strlen(cities[r]) + 1);

		// printf("Record %d: with this data was inserted: %d, %s, %s, %s in data.db\n", id, record.id, record.name, record.surname, record.city);

		if(id % 10000 == 0)
			printf("Inserted %d records\n", id);

		CALL_OR_DIE(HT_InsertEntry(indexDesc, record));
	}

	// Create a new file
	HT_CreateIndex(FILE_NAME2, GLOBAL_DEPT);

	// Open the new file
	int indexDesc2;
	if(HT_OpenIndex(FILE_NAME2, &indexDesc2) != HT_OK)
		goto exit_program;

	// Insert some records
	printf("Inserting 10 records in data2.db...\n");
	for (int id = 0; id < 10; ++id)
	{
		// create a record with random data
		memset(&record, 0, sizeof(Record));
		record.id = id;
		r = rand() % 12;
		memcpy(record.name, names[r], strlen(names[r]) + 1);
		r = rand() % 12;
		memcpy(record.surname, surnames[r], strlen(surnames[r]) + 1);
		r = rand() % 10;
		memcpy(record.city, cities[r], strlen(cities[r]) + 1);

		// printf("Record %d: with this data was inserted: %d, %s, %s, %s in data2.db\n", id, record.id, record.name, record.surname, record.city);

		CALL_OR_DIE(HT_InsertEntry(indexDesc2, record));
	}

	// Try printing records that don't exist
	printf("Trying to print record with ID 10 in data2.db (shouldn't find anything) ...\n");
	int false_id = 10;
	HT_PrintAllEntries(indexDesc2, &false_id);

	// Insert a record with an existing ID
	printf("Inserting record with ID 0 in data2.db... \n");
	memset(&record, 0, sizeof(Record));
	record.id = 0;
	r = rand() % 12;
	memcpy(record.name, names[r], strlen(names[r]) + 1);
	r = rand() % 12;
	memcpy(record.surname, surnames[r], strlen(surnames[r]) + 1);
	r = rand() % 10;
	memcpy(record.city, cities[r], strlen(cities[r]) + 1);

	printf("Record %d: with this data was inserted: %d, %s, %s, %s in data2.db\n", 0, record.id, record.name, record.surname, record.city);

	CALL_OR_DIE(HT_InsertEntry(indexDesc2, record));

	// Print entries with ID 0
	printf("Printing record with ID 0 in data2.db...\n");
	int id_0 = 0;
	HT_PrintAllEntries(indexDesc2, &id_0);

	// Now try printing the records
	printf("Printing all entries from data2.db...\n");
	HT_PrintAllEntries(indexDesc2, NULL);

	// Print the statistics of data2.db
	printf("Printing statistics of data2.db...\n");
	HashStatistics(FILE_NAME2);

	// Close the old file (data.db)
	printf("Closing data.db...\n");
	if (HT_CloseFile(indexDesc) != HT_OK)
		goto exit_program;

	// Reopen the old file (data.db)
	printf("Reopening data.db...\n");
	if (HT_OpenIndex(FILE_NAME, &indexDesc) != HT_OK)
		goto exit_program;

	// Print all entries of data.db
	printf("Printing all entries...\n");
	HT_PrintAllEntries(indexDesc, NULL);

	// Print statistics of data.db
	printf("Printing statistics...\n");
	HashStatistics(FILE_NAME);

	// Close the old file (data.db)
	if (HT_CloseFile(indexDesc) != HT_OK)
		goto exit_program;

	// Close the new file (data2.db)
	if (HT_CloseFile(indexDesc2) != HT_OK)
		goto exit_program;

	if (HT_Close() != HT_OK)
		return -1;

	return 0;

exit_program:
	HT_Close();
	return -1;
}



