#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../include/bf.h"
#include "../include/hash_file.h"


#define RECORDS_NUM 1000 // you can change it if you want
#define GLOBAL_DEPT 2    // you can change it if you want
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
    "Konstantinopoulidis",
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
    Record test = {10,"Onoufrios","Adebayor","Zimbaboue"};
    int files[5];
    char* filenames[] ={
        "data0.db",
        "data1.db",
        "data2.db",
        "data3.db",
        "data4.db"
    };

    BF_Init(LRU);
    CALL_OR_DIE(HT_Init());
    for(int i = 0; i < 5; i++){    
        CALL_OR_DIE(HT_CreateIndex(filenames[i],i+1));
        CALL_OR_DIE(HT_OpenIndex(filenames[i], &files[i]));
    
    }

   
    srand(time(NULL));
    Record record;
    int r;
    for(int i = 0; i < 5; i++){     // we can keep diferent files open and insert entries
        for (int id = 0; id < 100; ++id)
        {
        // create a record
        record.id = id;
        r = rand() % 12;
        memcpy(record.name, names[r], strlen(names[r]) + 1);
        r = rand() % 12;
        memcpy(record.surname, surnames[r], strlen(surnames[r]) + 1);
        r = rand() % 10;
        memcpy(record.city, cities[r], strlen(cities[r]) + 1);

        CALL_OR_DIE(HT_InsertEntry(files[i], record));
        }
    }
    
    for (int i = 0; i < 5; i++) // print entries and statistics for each file
    {
        CALL_OR_DIE(HT_PrintAllEntries(files[i],NULL));
        CALL_OR_DIE(HT_HashStatistics(filenames[i]));
    }
    
    for (int i = 0; i < 4; i++) 
    {
        CALL_OR_DIE(HT_InsertEntry(files[0], test));
    }
    int myid = 10;
    CALL_OR_DIE(HT_PrintAllEntries(files[0],&myid));    // we can see here we allow multiple entries with same id


    for(int i = 0; i < 5; i++){    
        CALL_OR_DIE(HT_CloseFile(files[i]));            // closing all the files
    
    }
    
    BF_Close();
}