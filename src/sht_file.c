#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#include "../include/bf.h"
#include "../include/types.h"
#include "../include/sht_file.h"
#include "../include/hash_file.h"
#include "../include/help_functions.h"

#define MAX_OPEN_FILES 20

#define CALL_BF(call)         \
  {                           \
    BF_ErrorCode code = call; \
    if (code != BF_OK)        \
    {                         \
      BF_PrintError(code);    \
      return HT_ERROR;        \
    }                         \
  }

// #ifndef HASH_FILE_H
// #define HASH_FILE_H

HT_ErrorCode SHT_Init()
{
  return HT_OK;
}

HT_ErrorCode SHT_CreateSecondaryIndex(const char *sfileName, char *attrName, int attrLength, int depth, char *fileName)
{
  // insert code here

  int filedesc, index;
  BF_Block *sblock;
  BF_Block_Init(&sblock);
  CALL_BF(BF_CreateFile(sfileName));
  CALL_BF(BF_OpenFile(sfileName, &filedesc));
  char *data;
  int globaldepth = depth;
  /*************** BLOCK 0 ********************/
  CALL_BF(BF_AllocateBlock(filedesc, sblock));
  CALL_BF(BF_GetBlock(filedesc, 0, sblock)); // edw tha kratame to gd, to attribute(surname h city), to length tou kai to filename to primary
  data = BF_Block_GetData(sblock);

  memcpy(data, &globaldepth, sizeof(int)); // first gd
  size_t offset = sizeof(int);
  memcpy(data + offset, &attrLength, sizeof(int)); // then attrlength
  offset += sizeof(int);
  memcpy(data + offset, attrName, attrLength + 1); // then the attribute (surname,city)
  offset += attrLength + 1;
  memcpy(data + offset, fileName, BF_BLOCK_SIZE - offset); // then the primary filename

  BF_Block_SetDirty(sblock);
  CALL_BF(BF_UnpinBlock(sblock));

  /***************** Init hash block ******************************/

  for (int i = 0; i < MAX_HASH_BLOCKS; i++)
  {
    CALL_BF(BF_AllocateBlock(filedesc, sblock)); // allocate blocks of hash index
    CALL_BF(BF_UnpinBlock(sblock));
  }

  /***********  Init first record blocks     **********************/
  int rec_num = 0;
  int local_depth = globaldepth;
  for (int i = 0; i < power(2, globaldepth); i++)
  {
    CALL_BF(BF_AllocateBlock(filedesc, sblock));
    data = BF_Block_GetData(sblock);
    memcpy(data + (BF_BLOCK_SIZE - 4) * sizeof(char), &rec_num, sizeof(int));     // set num of records = 0
    memcpy(data + (BF_BLOCK_SIZE - 8) * sizeof(char), &local_depth, sizeof(int)); // set local depth = global depth

    BF_Block_SetDirty(sblock);
    CALL_BF(BF_UnpinBlock(sblock));
  }

  // set directories to "point" to record blocks
  size_t position = 0;
  for (int i = 0; i < power(2, globaldepth); i++)
  {
    CALL_BF(BF_GetBlock(filedesc, i / (BF_BLOCK_SIZE / sizeof(int)) + 1, sblock));
    data = BF_Block_GetData(sblock);

    index = i + MAX_HASH_BLOCKS + 1;
    memcpy(data + position, &index, sizeof(int));
    BF_Block_SetDirty(sblock);
    CALL_BF(BF_UnpinBlock(sblock));

    if (i / (BF_BLOCK_SIZE / sizeof(int)) == (i + 1) / (BF_BLOCK_SIZE / sizeof(int)))
    {
      position += sizeof(int);
    }
    else
    {
      position = 0;
    }
  }

  CALL_BF(BF_CloseFile(filedesc));
  BF_Block_Destroy(&sblock);
  printf("Index created...\n");

  return HT_OK;
}

HT_ErrorCode SHT_OpenSecondaryIndex(const char *sfileName, int *indexDesc)
{
  // insert code here

  int fd;
  int flag = 0;
  CALL_BF(BF_OpenFile(sfileName, &fd));
  for (int i = 0; i < MAX_OPEN_FILES; i++)
  {
    if (__openFiles[i].descriptor == -1)
    {
      __openFiles[i].descriptor = fd;
      __openFiles[i].filetype = SECONDARY;
      flag = 1;
      *(indexDesc) = i;
      break;
    }
  }
  if (!flag)
  {
    printf("MAX FILES ARE OPEN RIGHT NOW\n");
    return HT_ERROR;
  }

  BF_Block *sblock;
  BF_Block_Init(&sblock);
  char *data;
  int num_blocks;
  CALL_BF(BF_GetBlock(fd, 0, sblock));
  data = BF_Block_GetData(sblock);

  // write global depth , connected file name (primary hash file) to our openfiles variable when opening the file
  int attrlen;
  memcpy(&__openFiles[*(indexDesc)].__global_depth, data, sizeof(int));
  memcpy(&attrlen, data + sizeof(int), sizeof(int));
  

  size_t offset = 2 * sizeof(int) + attrlen + 1;

  memcpy(&__openFiles[*(indexDesc)].connected_file, data + offset, sizeof(__openFiles[*(indexDesc)].connected_file)); 



  CALL_BF(BF_UnpinBlock(sblock));

  BF_Block_Destroy(&sblock);
  return HT_OK;
}


HT_ErrorCode SHT_CloseSecondaryIndex(int indexDesc)
{
  BF_Block *block;
  char *data;
  BF_Block_Init(&block);
  CALL_BF(BF_GetBlock(__openFiles[indexDesc].descriptor, 0, block)); // when we close the file we write the global depth in block 0
  data = BF_Block_GetData(block);
  memcpy(data, &__openFiles[indexDesc].__global_depth, sizeof(int));
  BF_Block_SetDirty(block);
  CALL_BF(BF_UnpinBlock(block));

  CALL_BF(BF_CloseFile(__openFiles[indexDesc].descriptor)); // setting descriptor to -1 
  __openFiles[indexDesc].descriptor = -1;
  

  BF_Block_Destroy(&block);
  return HT_OK;
}

HT_ErrorCode SHT_SecondaryInsertEntry(int sindexDesc, SecondaryRecord record)
{

  if (power(2, __openFiles[sindexDesc].__global_depth) > MAX_HASH_BLOCKS * (BF_BLOCK_SIZE / sizeof(int)))
  {
    perror("GLOBAL DEPTH EXCEEDS HASH TABLE\n"); // if global depth exceeds the max directory size we return error
    return HT_ERROR;
  }
  int num_of_blocks;
  BF_Block *sblock;

  BF_Block_Init(&sblock);

  char *data;

  int filedesc = __openFiles[sindexDesc].descriptor; // our current file identifier we insert data
  if (__openFiles[sindexDesc].descriptor == -1)
  {
    printf("There is no File in the %d position", sindexDesc);
    return HT_ERROR;
  }

  // Hash record id to find its bucket
  int index = (int)hashString(record.index_key, __openFiles[sindexDesc].__global_depth);

  int hash_block_index = index / (BF_BLOCK_SIZE / sizeof(int)) + 1; // find the block in the hash table that points to the required block
  int index_in_hashblock = index % (BF_BLOCK_SIZE / sizeof(int));   // find the position of the block
  // Get Block of the Hash table
  CALL_BF(BF_GetBlock(filedesc, hash_block_index, sblock));

  data = BF_Block_GetData(sblock);
  int blockNo;
  memcpy(&blockNo, data + index_in_hashblock * sizeof(int), sizeof(int)); // get the block number where we need to insert the id
  CALL_BF(BF_UnpinBlock(sblock));

  CALL_BF(BF_GetBlock(filedesc, blockNo, sblock)); // getting the block/bucket and it's data
  data = BF_Block_GetData(sblock);

  // Get the records number and local depth of the bucket
  int rec_num;
  int local_depth;
  memcpy(&rec_num, data + (BF_BLOCK_SIZE - 4) * sizeof(char), sizeof(int));
  memcpy(&local_depth, data + (BF_BLOCK_SIZE - 8) * sizeof(char), sizeof(int));

  // if bucket is full with records
  if (rec_num == (BF_BLOCK_SIZE - 8) / sizeof(SecondaryRecord))
  {


    BF_Block *stemp_block;
    BF_Block_Init(&stemp_block);

    BF_AllocateBlock(filedesc, stemp_block);               // first alocate a new block with local depth N+1
    CALL_BF(BF_GetBlockCounter(filedesc, &num_of_blocks)); // keeping the new number of blocks

    if (local_depth == __openFiles[sindexDesc].__global_depth)
    { // we need to perform a split

      int temp_directories[power(2, __openFiles[sindexDesc].__global_depth)]; // temporary help array for the expansion

      char *hash_block_data; // keep hash block data here

      size_t position = 0;
      for (int i = 0; i < power(2, __openFiles[sindexDesc].__global_depth); i++) // copy the full hash table to the array
      {
        CALL_BF(BF_GetBlock(filedesc, i / (BF_BLOCK_SIZE / sizeof(int)) + 1, stemp_block));
        hash_block_data = BF_Block_GetData(stemp_block);

        memcpy(&temp_directories[i], hash_block_data + position, sizeof(int));

        CALL_BF(BF_UnpinBlock(stemp_block));
        if (i / (BF_BLOCK_SIZE / sizeof(int)) == (i + 1) / (BF_BLOCK_SIZE / sizeof(int)))
        {
          position += sizeof(int);
        }
        else
        {
          position = 0;
        }
      }

      int new_dir[power(2, __openFiles[sindexDesc].__global_depth + 1)]; // make a new directories array with global depth + 1 (twice as big) ,pointing to correct buckets
      for (int i = 0; i < power(2, __openFiles[sindexDesc].__global_depth); i++)
      {
        new_dir[2 * i] = temp_directories[i];
        new_dir[2 * i + 1] = temp_directories[i];
      }
      __openFiles[sindexDesc].__global_depth++; // increase global depth

      new_dir[hashString(record.index_key, __openFiles[sindexDesc].__global_depth)] = num_of_blocks - 1; // fix the directory to the new hash block after increasing global depth

      /********************** FIX HASH TABLE *********************/
      position = 0;
      for (int i = 0; i < power(2, __openFiles[sindexDesc].__global_depth); i++) // saving the pointers in the hash table
      {

        CALL_BF(BF_GetBlock(filedesc, i / (BF_BLOCK_SIZE / sizeof(int)) + 1, stemp_block));
        hash_block_data = BF_Block_GetData(stemp_block);
        memcpy(hash_block_data + position, &new_dir[i], sizeof(int));
        if (i / (BF_BLOCK_SIZE / sizeof(int)) == (i + 1) / (BF_BLOCK_SIZE / sizeof(int))) // if we are on same hash block
        {
          position += sizeof(int);
        }
        else
        { // if we need to swap hash block
          position = 0;
        }
        BF_Block_SetDirty(stemp_block);
        CALL_BF(BF_UnpinBlock(stemp_block));
      }

      // SAVING DATA OF THE BLOCK TO TEMP ARRAY
      SecondaryRecord *records = malloc(((BF_BLOCK_SIZE - 8) / sizeof(SecondaryRecord)) * sizeof(SecondaryRecord)); // malloc temp array
      size_t offset = 0;

      CALL_BF(BF_GetBlock(filedesc, blockNo, sblock));
      data = BF_Block_GetData(sblock);

      for (int i = 0; i < (BF_BLOCK_SIZE - 8) / sizeof(SecondaryRecord); i++)     // keep records in temp array
      {

        memcpy(&records[i].index_key, data + offset, sizeof(record.index_key));
        memcpy(&records[i].tupleId, data + offset + offsetof(SecondaryRecord, tupleId), sizeof(record.tupleId)); 
        offset += sizeof(SecondaryRecord);
      }
      // clearing data first
      memset(data, 0, BF_BLOCK_SIZE);

      // we need to refix the bucket and init the new bucket with correct records number and local depth
      int new_rec_num = 0;
      int new_local_depth = local_depth + 1;

      memcpy(data + (BF_BLOCK_SIZE - 4) * sizeof(char), &new_rec_num, sizeof(int));     // set num of records = 0
      memcpy(data + (BF_BLOCK_SIZE - 8) * sizeof(char), &new_local_depth, sizeof(int)); // set local depth =n+1

      BF_Block_SetDirty(sblock);
      CALL_BF(BF_UnpinBlock(sblock));

      CALL_BF(BF_GetBlockCounter(filedesc, &num_of_blocks));
      CALL_BF(BF_GetBlock(filedesc, num_of_blocks - 1, sblock));
      data = BF_Block_GetData(sblock);
      memcpy(data + (BF_BLOCK_SIZE - 4) * sizeof(char), &new_rec_num, sizeof(int));     // set num of records = 0
      memcpy(data + (BF_BLOCK_SIZE - 8) * sizeof(char), &new_local_depth, sizeof(int)); // set local depth = n+1
      BF_Block_SetDirty(sblock);
      CALL_BF(BF_UnpinBlock(sblock));

      // when we are done try to insert again the record and the records with new hashing (global depth + 1)
      SHT_SecondaryInsertEntry(sindexDesc, record);

      for (int i = 0; i < (BF_BLOCK_SIZE - 8) / sizeof(SecondaryRecord); i++)
      {

        SHT_SecondaryInsertEntry(sindexDesc, records[i]);
      }

      free(records);
    }
    else
    { // local depth < global depth so no need to resize just overflow

      int num_of_blocks;

      SecondaryRecord *records = malloc(((BF_BLOCK_SIZE - 8) / sizeof(SecondaryRecord)) * sizeof(SecondaryRecord));

      // SAVE ALL RECORDS OF THE BLOCK TO A TEMPORARY ARRAY TO REINSERT THEM AFTER THE NEW BUCKETS HAVE BEEN CREATED
      size_t offset = 0;
      CALL_BF(BF_GetBlock(filedesc, blockNo, sblock));
      data = BF_Block_GetData(sblock);

      for (int i = 0; i < (BF_BLOCK_SIZE - 8) / sizeof(SecondaryRecord); i++)
      {
        memcpy(records[i].index_key, data + offset, sizeof(records[i].index_key));
        memcpy(&records[i].tupleId, data + offset + offsetof(SecondaryRecord, tupleId), sizeof(int));
        offset += sizeof(SecondaryRecord);
      }

      // clearing data first
      memset(data, 0, BF_BLOCK_SIZE);

      int new_rec_num = 0;
      int new_local_depth = local_depth + 1;
      memcpy(data + (BF_BLOCK_SIZE - 4) * sizeof(char), &new_rec_num, sizeof(int));     // set num of records = 0
      memcpy(data + (BF_BLOCK_SIZE - 8) * sizeof(char), &new_local_depth, sizeof(int)); // set local depth =n+1

      BF_Block_SetDirty(sblock);
      CALL_BF(BF_UnpinBlock(sblock));

      CALL_BF(BF_GetBlockCounter(filedesc, &num_of_blocks)); // same spliting as before
      int newblockNo = num_of_blocks - 1;
      CALL_BF(BF_GetBlock(filedesc, newblockNo, sblock));
      data = BF_Block_GetData(sblock);
      memcpy(data + (BF_BLOCK_SIZE - 4) * sizeof(char), &new_rec_num, sizeof(int));     // set num of records = 0
      memcpy(data + (BF_BLOCK_SIZE - 8) * sizeof(char), &new_local_depth, sizeof(int)); // set local depth = n+1

      BF_Block_SetDirty(sblock);
      CALL_BF(BF_UnpinBlock(sblock));

      // now refix the directory to point to the correct bucket
      CALL_BF(BF_GetBlock(filedesc, hash_block_index, sblock));
      data = BF_Block_GetData(sblock);

      memcpy(data + index_in_hashblock * sizeof(int), &newblockNo, sizeof(int));
      BF_Block_SetDirty(sblock);
      CALL_BF(BF_UnpinBlock(sblock));
      

      SHT_SecondaryInsertEntry(sindexDesc, record);
      // BF_Block_Destroy(&sblock); // this is the problem!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
      for (int i = 0; i < (BF_BLOCK_SIZE - 8) / sizeof(SecondaryRecord); i++)
      {
        SHT_SecondaryInsertEntry(sindexDesc, records[i]);
      }
      free(records); // free temp array
    }
    BF_Block_Destroy(&stemp_block); // free stemp_block memory
  }

  else // just a normal insertion since bucket is not full
  {

    // Copying data one at a time to avoid mistakes in copy to memory
    memcpy(data + rec_num * sizeof(SecondaryRecord), &record.index_key, sizeof(record.index_key));
    memcpy(data + rec_num * sizeof(SecondaryRecord) + offsetof(SecondaryRecord, tupleId), &record.tupleId, sizeof(int)); // todo ask mike an ontos doulevei auto

    // adding to num of records
    rec_num++;
    memcpy(data + (BF_BLOCK_SIZE - 4) * sizeof(char), &rec_num, sizeof(int));

    BF_Block_SetDirty(sblock);
    CALL_BF(BF_UnpinBlock(sblock));
    
  }
  
  BF_Block_Destroy(&sblock); // free sblock memory
  return HT_OK;
}

HT_ErrorCode SHT_SecondaryUpdateEntry(int indexDesc, UpdateRecordArray *updateArray)
{

  if (updateArray[0].oldTupleId == 0) // if array is empty nothing to update
  {
    return HT_OK;
  }

  // insert code here
  BF_Block *block;
  BF_Block_Init(&block);
  char *data;

  
  int attrLength;
  int fd = __openFiles[indexDesc].descriptor;
  int gd = __openFiles[indexDesc].__global_depth;

  BF_GetBlock(fd, 0, block); // get the attribute of the secondary hash table
  data = BF_Block_GetData(block);
  memcpy(&attrLength, data + sizeof(int), sizeof(int));

  char attr[attrLength];
  memcpy(&attr, data + 2 * sizeof(int), attrLength + 1);

  if (strcmp(attr, "city") == 0)  // if attribute is city 
  {
    for (int i = 0; i < ((BF_BLOCK_SIZE - 8) / sizeof(Record)); i++) // for every updated record
    {
      int index = (int)hashString(updateArray[i].city, gd);
      int hash_block_index = index / (BF_BLOCK_SIZE / sizeof(int)) + 1; // get the block that has the directory
      int index_in_hashblock = index % (BF_BLOCK_SIZE / sizeof(int));   // position of bucket in that block

      CALL_BF(BF_GetBlock(fd, hash_block_index, block));
      data = BF_Block_GetData(block);
      int block_no;
      memcpy(&block_no, &data[index_in_hashblock * sizeof(int)], sizeof(int)); // get the block no
      CALL_BF(BF_UnpinBlock(block));

      CALL_BF(BF_GetBlock(fd, block_no, block));
      data = BF_Block_GetData(block);
      size_t offset = 0;
      SecondaryRecord temp_record; // use a temp record
      while (offset < BF_BLOCK_SIZE - 8)
      {

        memcpy(temp_record.index_key, data + offset, sizeof(temp_record.index_key));
        memcpy(&temp_record.tupleId, data + offset + offsetof(SecondaryRecord, tupleId), sizeof(temp_record.tupleId));

        if ((!strcmp(temp_record.index_key, updateArray[i].city)) && temp_record.tupleId == updateArray[i].oldTupleId) // if index_key is the same and old tuple id is the same (distinct together)
        {
          memcpy(data + offset + offsetof(SecondaryRecord, tupleId), &updateArray[i].newTupleId, sizeof(int)); // change to new tuple id and break the loop
          break;
        }
        offset += sizeof(SecondaryRecord);
      }
      BF_Block_SetDirty(block);
      CALL_BF(BF_UnpinBlock(block));
    }
  }
  if (strcmp(attr, "surname") == 0) // same for surname attribute
  {
    for (int i = 0; i < ((BF_BLOCK_SIZE - 8) / sizeof(Record)); i++)
    {
      int index = hashString(updateArray[i].surname, gd);
      int hash_block_index = index / (BF_BLOCK_SIZE / sizeof(int)) + 1; // get the block that has the directory
      int index_in_hashblock = index % (BF_BLOCK_SIZE / sizeof(int));   // position of bucket in that block

      CALL_BF(BF_GetBlock(fd, hash_block_index, block));
      data = BF_Block_GetData(block);
      int block_no;
      memcpy(&block_no, &data[index_in_hashblock * sizeof(int)], sizeof(int)); // get the block no
      CALL_BF(BF_UnpinBlock(block));

      CALL_BF(BF_GetBlock(fd, block_no, block));
      data = BF_Block_GetData(block);
      size_t offset = 0;
      SecondaryRecord temp_record;

      while (offset < BF_BLOCK_SIZE - 8)
      {

        memcpy(temp_record.index_key, data + offset, sizeof(temp_record.index_key));
        memcpy(&temp_record.tupleId, data + offset + offsetof(SecondaryRecord, tupleId), sizeof(temp_record.tupleId));

        if ((!strcmp(temp_record.index_key, updateArray[i].surname)) && temp_record.tupleId == updateArray[i].oldTupleId)
        {
          memcpy(data + offset + offsetof(SecondaryRecord, tupleId), &updateArray[i].newTupleId, sizeof(int));
          break;
        }
        offset += sizeof(SecondaryRecord);
      }
      BF_Block_SetDirty(block);
      CALL_BF(BF_UnpinBlock(block));
    }
  }

  BF_Block_Destroy(&block);
  return HT_OK;
}

HT_ErrorCode SHT_PrintAllEntries(int sindexDesc, char *index_key)
{
  // insert code here
  int count = 0;
  BF_Block *block;
  BF_Block_Init(&block);
  char *data;
  char * prim_data;
  printf("Printing entries with index key: %s\n", index_key);
  int filedesc = __openFiles[sindexDesc].descriptor;
  if (filedesc == -1)
  {
    printf("There is no File in the %d position", sindexDesc);
    return HT_ERROR;
  }
  int primaryFileDesc;
  CALL_BF(BF_OpenFile(__openFiles[sindexDesc].connected_file, &primaryFileDesc)); // we need to open the primary file again (more in readme why we can't keep the allready open descriptor)
  int tupleID;
  
  if (index_key == NULL) // if index is null we will print all entries
  {
    int total_blocks;
    CALL_BF(BF_GetBlockCounter(filedesc,&total_blocks));
    for (int i = MAX_HASH_BLOCKS + 1; i < total_blocks; i++)
    {
      printf("for block %d\n", i);
      CALL_BF(BF_GetBlock(sindexDesc,i,block));
      data = BF_Block_GetData(block); // get data of block
      CALL_BF(BF_UnpinBlock(block));

      size_t offset = 0;
      Record record;
      while (offset < (BF_BLOCK_SIZE-8))
      {
        if (data[offset + 20] == 0) // tuple id = 0 so no record there
        {
          break; // records are inserted next to each other so we have no more records in block
        }
        memcpy(&tupleID, data + offset + offsetof(SecondaryRecord,tupleId) ,sizeof(int)); // find the position of the record in the primary file 
        int primary_block_no = tupleID / ((BF_BLOCK_SIZE-8)/sizeof(Record));
        int position = tupleID % ((BF_BLOCK_SIZE-8)/sizeof(Record));
        CALL_BF(BF_GetBlock(primaryFileDesc,primary_block_no,block));
        prim_data = BF_Block_GetData(block);
        
        memcpy(&record.id, prim_data + position *sizeof(Record) ,sizeof(record.id));
        memcpy(record.name, prim_data + position *sizeof(Record) + offsetof(Record,name), sizeof(record.name));
        memcpy(record.surname, prim_data + position *sizeof(Record) + offsetof(Record,surname), sizeof(record.surname));
        memcpy(record.city, prim_data + position *sizeof(Record) + offsetof(Record,city), sizeof(record.city));

        printf("{id: %d | name: %s | surname: %s | city: %s }\n",record.id,record.name,record.surname,record.city); //print fo each record
        
        CALL_BF(BF_UnpinBlock(block));
        offset += sizeof(SecondaryRecord);
      }      

    }
    
    BF_Block_Destroy(&block);
    return HT_OK; // return we don't need to go through the rest
  }
  
  // FOR SPECIFIC INDEX_KEY

  int index = hashString(index_key, __openFiles[sindexDesc].__global_depth); // use hash to find the exact position of the index_key
  int hash_block_index = index / (BF_BLOCK_SIZE / sizeof(int)) + 1; // find the block in the hash table that points to the required block
  int index_in_hashblock = index % (BF_BLOCK_SIZE / sizeof(int));   // find the position of the block

  CALL_BF(BF_GetBlock(filedesc, hash_block_index, block));      
  data = BF_Block_GetData(block);
  int block_no;
  memcpy(&block_no, data + index_in_hashblock * sizeof(int), sizeof(int));  // get the block_no
  CALL_BF(BF_UnpinBlock(block));

  CALL_BF(BF_GetBlock(filedesc, block_no, block));
  data = BF_Block_GetData(block);
  int offset = 0;
  int num_of_rec_in_block = (BF_BLOCK_SIZE - 8) / sizeof(Record);
  for (int i = 0; i < (BF_BLOCK_SIZE - 8) / sizeof(SecondaryRecord); i++)   // for every record in that block (since we have multiple with same index_key)
  {
    int tupleID;
    memcpy(&tupleID, data + offset + offsetof(SecondaryRecord, tupleId), sizeof(int));  

    if (tupleID != 0) // if tuple id is 0 there is no record so we skip
    {
      char *primaryData;
      // find position of record in primary file
      int blockNo = tupleID / num_of_rec_in_block; 
      int index_of_rec_in_block = tupleID % num_of_rec_in_block;
      CALL_BF(BF_GetBlock(primaryFileDesc, blockNo, block)); // find the block no in the primary file
      primaryData = BF_Block_GetData(block);
      Record record;
      memcpy(&record.surname, primaryData + index_of_rec_in_block * sizeof(Record) + offsetof(Record, surname), 20 * sizeof(char));
      // first check if attribute is surname
      if (strcmp(record.surname, index_key) == 0) // check if index is surname (we can have different surnames in the same block depending on global depth)
      {
        count++;
        memcpy(&record.id, primaryData + index_of_rec_in_block * sizeof(Record), sizeof(int));
        memcpy(&record.name, primaryData + index_of_rec_in_block * sizeof(Record) + offsetof(Record, name), 12 * sizeof(char));
        memcpy(&record.city, primaryData + index_of_rec_in_block * sizeof(Record) + offsetof(Record, city), 20 * sizeof(char));
        printf("%d)ID: %d ,NAME:%s, SURNAME:%s, CITY:%s\n", count, record.id, record.name, record.surname, record.city);
      }
      // same for city
      
      memcpy(&record.city, primaryData + index_of_rec_in_block * sizeof(Record) + offsetof(Record, city), 20 * sizeof(char));
      if (strcmp(record.city, index_key) == 0)
      {
        count++;
        memcpy(&record.id, primaryData + index_of_rec_in_block * sizeof(Record), sizeof(int));
        memcpy(&record.name, primaryData + index_of_rec_in_block * sizeof(Record) + offsetof(Record, name), 12 * sizeof(char));
        memcpy(&record.surname, primaryData + index_of_rec_in_block * sizeof(Record) + offsetof(Record, surname), 20 * sizeof(char));
        printf("%d)ID: %d ,NAME:%s, SURNAME:%s, CITY:%s\n", count, record.id, record.name, record.surname, record.city);
      }
    
    }
    offset += sizeof(SecondaryRecord);
  }
  CALL_BF(BF_CloseFile(primaryFileDesc));
  BF_Block_Destroy(&block);
  return HT_OK;
}

HT_ErrorCode SHT_HashStatistics(char *filename) // works the same as in HT
{
  int fd1, blocks_num;
  int max = 0;
  int min = 1000;
  int max_pos = 1;
  int min_pos = 1;
  int total_num_rec = 0;

  BF_Block *block;
  BF_Block_Init(&block);
  char *data;
  Record record = {0};
  CALL_BF(BF_OpenFile(filename, &fd1));
  BF_GetBlockCounter(fd1, &blocks_num);
  printf("\nTotal number of blocks in file is: %d\n", blocks_num);
  for (int k = MAX_HASH_BLOCKS + 1; k < blocks_num; k++) // get every block of records
  {

    CALL_BF(BF_GetBlock(fd1, k, block));
    data = BF_Block_GetData(block);
    int rec_num;
    memcpy(&rec_num, data + (BF_BLOCK_SIZE - 4) * sizeof(char), sizeof(int));

    if (rec_num < min) // set min records and the block number
    {
      min = rec_num;
      min_pos = k;
    }
    if (rec_num > max) // set max records and the block number
    {
      max = rec_num;
      max_pos = k;
    }
    total_num_rec += rec_num; // add to total records
    CALL_BF(BF_UnpinBlock(block));
  }
  float avg = (float)total_num_rec / (blocks_num - MAX_HASH_BLOCKS); // find the average
  printf("bucket number: %d has least records equal to: %d\n", min_pos, min);
  printf("bucket number: %d has most records equal to: %d\n", max_pos, max);
  printf("average number of records in every bucket is: %f\n", avg);

  CALL_BF(BF_CloseFile(fd1));
  BF_Block_Destroy(&block);
  return HT_OK;
}
HT_ErrorCode SHT_InnerJoin(int sindexDesc1, int sindexDesc2, char *index_key) // joining for 2 different files
{
  // insert code here
  if (__openFiles[sindexDesc1].descriptor == -1 || __openFiles[sindexDesc2].descriptor == -1) // if any of the descriptors is -1 the input is wrong
  {
    perror("FIlE NOT OPEN");
    return HT_ERROR;
  }
  int fd1 = __openFiles[sindexDesc1].descriptor;
  int fd2 = __openFiles[sindexDesc2].descriptor;
  int primary_fd1;
  int primary_fd2;

  CALL_BF(BF_OpenFile(__openFiles[sindexDesc1].connected_file, &primary_fd1));  // get the primary file descriptors
  CALL_BF(BF_OpenFile(__openFiles[sindexDesc2].connected_file, &primary_fd2));
  BF_Block *block1;
  BF_Block *block2;
  BF_Block_Init(&block1);
  BF_Block_Init(&block2);

  int attrlen1;
  int attrlen2;
  char *data1;
  char *data2;
  CALL_BF(BF_GetBlock(fd1, 0, block1));
  data1 = BF_Block_GetData(block1);
  memcpy(&attrlen1, data1 + sizeof(int), sizeof(int));
  char attr[attrlen1]; 

  memcpy(&attr,data1 + sizeof(int)*2,attrlen1 +1); // get the attribute
  CALL_BF(BF_UnpinBlock(block1));

  CALL_BF(BF_GetBlock(fd2, 0, block2));
  data2 = BF_Block_GetData(block2);
  memcpy(&attrlen2, data2 + sizeof(int), sizeof(int));
  CALL_BF(BF_UnpinBlock(block2));

  if (attrlen1 != attrlen2) // if attribute length is diferent it means it's different attribute
  {
    perror("FILES HAVE DIFERENT ATTRIBUTE\n");
    return HT_ERROR;
  }

  if (index_key == NULL)  // we didn't implement the NULL case
  {
    return HT_OK;
  }

  // GET THE BLOCK NUMBER IN THE FIRST FILE
  int index = hashString(index_key, __openFiles[sindexDesc1].__global_depth); // hash with gd of first file
  int hash_block_index = index / (BF_BLOCK_SIZE / sizeof(int)) + 1; // find the block in the hash table that points to the required block
  int index_in_hashblock = index % (BF_BLOCK_SIZE / sizeof(int));   // find the position of the block
  CALL_BF(BF_GetBlock(fd1, hash_block_index, block1));
  data1 = BF_Block_GetData(block1);
  int block_no1;
  memcpy(&block_no1, data1 + index_in_hashblock * sizeof(int), sizeof(int));
  CALL_BF(BF_UnpinBlock(block1));

  // GET THE BLOCK NUMBER IN THE SECOND FILE
  index = hashString(index_key,__openFiles[sindexDesc2].__global_depth);  // hash with gd of 2nd file
  hash_block_index = index / (BF_BLOCK_SIZE / sizeof(int)) + 1; // find the block in the hash table that points to the required block
  index_in_hashblock = index % (BF_BLOCK_SIZE / sizeof(int));   // find the position of the block
  CALL_BF(BF_GetBlock(fd2, hash_block_index, block2));
  data2 = BF_Block_GetData(block2);
  int block_no2;
  memcpy(&block_no2, data2 + index_in_hashblock * sizeof(int), sizeof(int));
  CALL_BF(BF_UnpinBlock(block2));

  // GET THE DATA OF BOTH BLOCKS
  CALL_BF(BF_GetBlock(fd1, block_no1, block1));
  data1 = BF_Block_GetData(block1);
  CALL_BF(BF_UnpinBlock(block1));

  CALL_BF(BF_GetBlock(fd2, block_no2, block2));
  data2 = BF_Block_GetData(block2);
  CALL_BF(BF_UnpinBlock(block2));

  if (!strcmp(attr,"surname")) // if attribute is surname
  {
    size_t offset = 0;
    int num_of_recs = (BF_BLOCK_SIZE - 8) / sizeof(Record);
    for (int i = 0; i < (BF_BLOCK_SIZE - 8) / sizeof(SecondaryRecord); i++) // for every record in the block
    {
      int tupleID;
      memcpy(&tupleID, data1 + offset + offsetof(SecondaryRecord, tupleId), sizeof(int));
      if (tupleID != 0) // check if tuple id is 0 (if yes there is no record there)
      {
        char *primary_data1;
        block_no1 = tupleID / num_of_recs;
        int index_of_rec_in_block = tupleID % num_of_recs;
        CALL_BF(BF_GetBlock(primary_fd1, block_no1, block1)); // get the block no from TupleID
        primary_data1 = BF_Block_GetData(block1);
        Record record1;
        memcpy(&record1.surname,primary_data1 + index_of_rec_in_block *sizeof(Record) + offsetof(Record,surname), sizeof(record1.surname));
        if (strcmp(record1.surname,index_key) == 0) // if surname is index_key
        {
          memcpy(&record1.id, primary_data1 + index_of_rec_in_block *sizeof(Record) , sizeof(int));
          memcpy(&record1.name, primary_data1 + index_of_rec_in_block *sizeof(Record) + offsetof(Record,name), sizeof(record1.name));
          memcpy(&record1.city, primary_data1 + index_of_rec_in_block *sizeof(Record) + offsetof(Record,city), sizeof(record1.city));
          printf("%s : {%d | %s | %s }",record1.surname ,record1.id,record1.name,record1.city); // print record on the first file with specific Surname
          size_t offset2 = 0;
          for (int j = 0; j < (BF_BLOCK_SIZE - 8) / sizeof(SecondaryRecord); j++) // check in now in the block we found for 2nd file
          {
            memcpy(&tupleID, data2 + offset2 + offsetof(SecondaryRecord,tupleId), sizeof(int));
            if (tupleID != 0)
            {
              char * primary_data2;
              block_no2 = tupleID / num_of_recs;
              index_of_rec_in_block = tupleID % num_of_recs;
              CALL_BF(BF_GetBlock(primary_fd2,block_no2,block2));
              primary_data2 = BF_Block_GetData(block2);
              Record record2;
              memcpy(&record2.surname,primary_data2 + index_of_rec_in_block *sizeof(Record) + offsetof(Record,surname), sizeof(record2.surname));
              if (strcmp(record2.surname,index_key) == 0) // if surname is the same print the record from the second file
              {
                memcpy(&record2.id, primary_data2 + index_of_rec_in_block *sizeof(Record) , sizeof(int));
                memcpy(&record2.name, primary_data2 + index_of_rec_in_block *sizeof(Record) + offsetof(Record,name), sizeof(record2.name));
                memcpy(&record2.city, primary_data2 + index_of_rec_in_block *sizeof(Record) + offsetof(Record,city), sizeof(record2.city));
                printf(" {%d | %s | %s }",record2.id,record2.name,record2.city);
              }
              CALL_BF(BF_UnpinBlock(block2));
            }
            offset2 += sizeof(SecondaryRecord);
          }
          printf("\n");
        }
        CALL_BF(BF_UnpinBlock(block1));
      }
      offset += sizeof(SecondaryRecord);
    }
  }
  if (!strcmp(attr,"city")) // ATTRIBUTE IS CITY exactly the same as above
  {
    size_t offset = 0;
    int num_of_recs = (BF_BLOCK_SIZE - 8) / sizeof(Record);
    for (int i = 0; i < (BF_BLOCK_SIZE - 8) / sizeof(SecondaryRecord); i++)
    {
      int tupleID;
      memcpy(&tupleID, data1 + offset + offsetof(SecondaryRecord, tupleId), sizeof(int));
      if (tupleID != 0)
      {
        char *primary_data1;
        block_no1 = tupleID / num_of_recs;
        int index_of_rec_in_block = tupleID % num_of_recs;
        CALL_BF(BF_GetBlock(primary_fd1, block_no1, block1));
        primary_data1 = BF_Block_GetData(block1);
        Record record1;
        memcpy(&record1.city,primary_data1 + index_of_rec_in_block *sizeof(Record) + offsetof(Record,city), sizeof(record1.city));
        if (strcmp(record1.city,index_key) == 0)
        {
          memcpy(&record1.id, primary_data1 + index_of_rec_in_block *sizeof(Record) , sizeof(int));
          memcpy(&record1.name, primary_data1 + index_of_rec_in_block *sizeof(Record) + offsetof(Record,name), sizeof(record1.name));
          memcpy(&record1.surname, primary_data1 + index_of_rec_in_block *sizeof(Record) + offsetof(Record,surname), sizeof(record1.surname));
          printf("%s : {%d | %s | %s }",record1.city ,record1.id,record1.name,record1.surname);//
          size_t offset2 = 0;
          for (int j = 0; j < (BF_BLOCK_SIZE - 8) / sizeof(SecondaryRecord); j++)
          {
            memcpy(&tupleID, data2 + offset2 + offsetof(SecondaryRecord,tupleId), sizeof(int));
            if (tupleID != 0)
            {
              char * primary_data2;
              block_no2 = tupleID / num_of_recs;
              index_of_rec_in_block = tupleID % num_of_recs;
              CALL_BF(BF_GetBlock(primary_fd2,block_no2,block2));
              primary_data2 = BF_Block_GetData(block2);
              Record record2;
              memcpy(&record2.city,primary_data2 + index_of_rec_in_block *sizeof(Record) + offsetof(Record,city), sizeof(record2.city));
              if (strcmp(record2.city,index_key) == 0)
              {
                memcpy(&record2.id, primary_data2 + index_of_rec_in_block *sizeof(Record) , sizeof(int));
                memcpy(&record2.name, primary_data2 + index_of_rec_in_block *sizeof(Record) + offsetof(Record,name), sizeof(record2.name));
                memcpy(&record2.surname, primary_data2 + index_of_rec_in_block *sizeof(Record) + offsetof(Record,surname), sizeof(record2.surname));
                printf(" {%d | %s | %s }",record2.id,record2.name,record2.surname);
              }
              CALL_BF(BF_UnpinBlock(block2));
            }
            offset2 += sizeof(SecondaryRecord);
          }
          printf("\n");
        }
        CALL_BF(BF_UnpinBlock(block1));
      }
      offset += sizeof(SecondaryRecord);
    }
  }
     

  

  CALL_BF(BF_CloseFile(primary_fd1)); // close the files we opened
  CALL_BF(BF_CloseFile(primary_fd2));
  BF_Block_Destroy(&block1);  // free memory
  BF_Block_Destroy(&block2);
  return HT_OK;
}

// #endif // HASH_FILE_H