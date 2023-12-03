#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hash_file.h"

#define RECORDS_NUM 40 // you can change it if you want
#define GLOBAL_DEPT 2 // you can change it if you want
#define FILE_NAME "data.db"

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

	// if(HT_CreateIndex(FILE_NAME, GLOBAL_DEPT) != HT_OK)
	// 	goto exit_program;

	HT_CreateIndex(FILE_NAME, GLOBAL_DEPT);

	show_files();

	Record record;
	srand(12569874);
	int r;
	printf("Inserting %d records...\n", RECORDS_NUM);
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

		printf("Record %d: with this data was inserted: %d, %s, %s, %s\n", id, record.id, record.name, record.surname, record.city);

		CALL_OR_DIE(HT_InsertEntry(indexDesc, record));
	}

	if (HT_CloseFile(indexDesc) != HT_OK)
		goto exit_program;

	if (HT_OpenIndex(FILE_NAME, &indexDesc) != HT_OK)
		goto exit_program;

	if (HT_CloseFile(indexDesc) != HT_OK)
		goto exit_program;

	if (HT_Close() != HT_OK)
		return -1;

	return 0;

exit_program:
	HT_Close();
	return -1;
}