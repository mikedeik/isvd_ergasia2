#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../include/bf.h"
#include "../include/help_functions.h"
#include "../include/hash_file.h"
#include "../include/sht_file.h"

#define RECORDS_NUM 1000 // you can change it if you want
#define GLOBAL_DEPT 2    // you can change it if you want
#define FILE_NAME "data.db"
#define SFILE_NAME "sdata.db"

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
    "Halatsis",
    "asdadsasda",
    "qweqweqweqw",
    "asdasdas",
    "lpkioijoij"};

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

#define CALL_OR_DIE(call)     \
  {                           \
    HT_ErrorCode code = call; \
    if (code != HT_OK)        \
    {                         \
      printf("Error\n");      \
      exit(code);             \
    }                         \
  }

int main()
{

  BF_Init(MRU);
  CALL_OR_DIE(HT_Init());

  int PindexDesc1;
  CALL_OR_DIE(HT_CreateIndex("data1.db", GLOBAL_DEPT));
  CALL_OR_DIE(HT_OpenIndex("data1.db", &PindexDesc1));

  int PindexDesc2;
  CALL_OR_DIE(HT_CreateIndex("data2.db", GLOBAL_DEPT));
  CALL_OR_DIE(HT_OpenIndex("data2.db", &PindexDesc2));

  CALL_OR_DIE(SHT_Init());

  // Attribute surname
  int s_index_desc1;
  int attributeLength = strlen("surname");
  char *attr;
  attr = malloc((attributeLength + 1) * sizeof(char));
  attr = strcpy(attr, "surname");

  CALL_OR_DIE(SHT_CreateSecondaryIndex("sdata1.db", attr, attributeLength, GLOBAL_DEPT, "data1.db"));
  CALL_OR_DIE(SHT_OpenSecondaryIndex("sdata1.db", &s_index_desc1));

  int s_index_desc2;
  CALL_OR_DIE(SHT_CreateSecondaryIndex("sdata2.db", attr, attributeLength, GLOBAL_DEPT, "data2.db"));
  CALL_OR_DIE(SHT_OpenSecondaryIndex("sdata2.db", &s_index_desc2));

  Record record;
  int r;
  srand(12569874);
  int tupleid;

  SecondaryRecord testrecord;
  UpdateRecordArray *updateArray;
  CreateArray(&updateArray);

  for (int id = 0; id < 211; ++id) // we can add alot more ids depending on the max size of the hash table (see hash_file.h)
  {
    // create a record
    record.id = id;
    r = rand() % 12;
    memcpy(record.name, names[r], strlen(names[r]) + 1);
    r = id % 12;
    memcpy(record.surname, surnames[r], strlen(surnames[r]) + 1);
    memcpy(testrecord.index_key, surnames[r], 20 * sizeof(char));

    r = rand() % 10;
    memcpy(record.city, cities[r], strlen(cities[r]) + 1);
    CALL_OR_DIE(HT_InsertEntry(PindexDesc1, record, &tupleid, updateArray));

    CALL_OR_DIE(SHT_SecondaryUpdateEntry(s_index_desc1, updateArray));
    testrecord.tupleId = tupleid;
    CALL_OR_DIE(SHT_SecondaryInsertEntry(s_index_desc1, testrecord));
  }

  CALL_OR_DIE(SHT_PrintAllEntries(s_index_desc1, testrecord.index_key));
  CALL_OR_DIE(SHT_HashStatistics("sdata1.db"));

  for (int id = 400; id < 420; ++id) // we can add alot more ids depending on the max size of the hash table (see hash_file.h)
  {
    // create a record
    record.id = id;
    r = rand() % 12;
    memcpy(record.name, names[r], strlen(names[r]) + 1);
    r = id % 12;
    memcpy(record.surname, surnames[r], strlen(surnames[r]) + 1);
    memcpy(testrecord.index_key, surnames[r], 20 * sizeof(char));

    r = rand() % 10;
    memcpy(record.city, cities[r], strlen(cities[r]) + 1);
    CALL_OR_DIE(HT_InsertEntry(PindexDesc2, record, &tupleid, updateArray));

    CALL_OR_DIE(SHT_SecondaryUpdateEntry(s_index_desc2, updateArray));
    testrecord.tupleId = tupleid;
    CALL_OR_DIE(SHT_SecondaryInsertEntry(s_index_desc2, testrecord));
  }

  CALL_OR_DIE(SHT_InnerJoin(s_index_desc1, s_index_desc2, testrecord.index_key));

  CALL_OR_DIE(SHT_PrintAllEntries(s_index_desc2, NULL));

  DeleteArray(&updateArray);
  CALL_OR_DIE(SHT_CloseSecondaryIndex(s_index_desc1));
  CALL_OR_DIE(SHT_CloseSecondaryIndex(s_index_desc2));
  free(__openFiles); // there is no HT_Close() to free this array
  free(attr);
  BF_Close();
}
