#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#include "../include/bf.h"
#include "../include/hash_file.h"
// #include "../include/sht_file.h"

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
// GLOBAL VARIABLES
static int num_files = 0; 
file *__openFiles;  



HT_ErrorCode HT_Init()
{


  __openFiles = (file *)malloc(sizeof(file) * MAX_OPEN_FILES); // allocate MAX_OPEN_FILES space for our files
  
  for (int i = 0; i < MAX_OPEN_FILES; i++){
    __openFiles[i].descriptor = -1; // put all our file identifiers to -1
    __openFiles[i].__global_depth = 0;  // init global depth to 0 this will change anyways
    
  }
  return HT_OK;
}

HT_ErrorCode HT_CreateIndex(const char *filename, int depth)
{
  printf("Creating index...\n");
  int fd1, index;
  BF_Block *block;
  BF_Block_Init(&block);    // creating a new file and initializing first blocks
  CALL_BF(BF_CreateFile(filename));
  CALL_BF(BF_OpenFile(filename, &fd1));
  char *data;
  int __global_depth = depth;   // starting global depth

  CALL_BF(BF_AllocateBlock(fd1, block));
  CALL_BF(BF_GetBlock(fd1,0,block));
  data = BF_Block_GetData(block); // init block 0 with global depth
  memcpy(data,&__global_depth,sizeof(int));
  
  BF_Block_SetDirty(block);
  CALL_BF(BF_UnpinBlock(block));

  /***************** Init hash block ******************************/
  for (int i = 0; i < MAX_HASH_BLOCKS; i++)
  {
    CALL_BF(BF_AllocateBlock(fd1, block)); // allocate blocks of hash index   
    CALL_BF(BF_UnpinBlock(block));
  }

    /***********  Init first record blocks     **********************/
  int rec_num = 0;
  int local_depth = __global_depth;
  for (int i = 0; i < power(2,__global_depth); i++)
  {
    CALL_BF(BF_AllocateBlock(fd1, block));  
    data = BF_Block_GetData(block);
    memcpy(data + (BF_BLOCK_SIZE - 4) * sizeof(char), &rec_num, sizeof(int));     // set num of records = 0
    memcpy(data + (BF_BLOCK_SIZE - 8) * sizeof(char), &local_depth, sizeof(int)); // set local depth = global depth

    BF_Block_SetDirty(block);
    CALL_BF(BF_UnpinBlock(block));
  }
  
  // set directories to "point" to record blocks
  size_t position = 0;
  for (int i = 0; i < power(2,__global_depth); i++)
  {
    CALL_BF(BF_GetBlock(fd1,i/(BF_BLOCK_SIZE/sizeof(int)) + 1,block));
    data = BF_Block_GetData(block);

    index = i + MAX_HASH_BLOCKS + 1;
    memcpy(data + position,&index,sizeof(int));
    BF_Block_SetDirty(block);
    CALL_BF(BF_UnpinBlock(block));

    if (i/(BF_BLOCK_SIZE/sizeof(int)) == (i+1)/(BF_BLOCK_SIZE/sizeof(int)) )
    {
      position+= sizeof(int);
    }
    else{
      position = 0;
    }

  
  }
  

  BF_Block_Destroy(&block);
  CALL_BF(BF_CloseFile(fd1));
  printf("Index created...\n");
  return HT_OK;
}

HT_ErrorCode HT_OpenIndex(const char *fileName, int *indexDesc)
{
  printf("Opening index...\n");
  int fd1, i;
  int flag = 0;
  //int blocks_num;
  *(indexDesc) = num_files;

  CALL_BF(BF_OpenFile(fileName, &fd1)); // first add the file descriptor in the array of open files
  for (i = 0; i <= MAX_OPEN_FILES; i++)
  {
    if (__openFiles[i].descriptor == -1)
    {
      __openFiles[i].descriptor = fd1;
      __openFiles[i].filetype = PRIMARY;
      flag = 1;
      break;
    }
  }
  if (flag == 0)
  {
    printf("MAX FILES ARE OPEN RIGHT NOW\n");
    return HT_ERROR;
  }


  BF_Block *block;
  BF_Block_Init(&block);
  char* data;
  CALL_BF(BF_GetBlock(fd1, 0, block)); // when we open file we get the current global depth 
  data = BF_Block_GetData(block);      // if file was open and closed before we have the gl depth stored in block 0
  memcpy(&__openFiles[*indexDesc].__global_depth, data, sizeof(int));
  CALL_BF(BF_UnpinBlock(block));
  
  num_files++;
  printf("Index Opened...\n");
  return HT_OK;
}

HT_ErrorCode HT_CloseFile(int indexDesc)
{
  // insert code here
  BF_Block* block;
  char* data;
  BF_Block_Init(&block);
  CALL_BF(BF_GetBlock(__openFiles[indexDesc].descriptor,0,block));   // when we close the file we write the global depth in block 0
  data = BF_Block_GetData(block);
  memcpy(data,&__openFiles[indexDesc].__global_depth,sizeof(int));
  BF_Block_SetDirty(block);
  CALL_BF(BF_UnpinBlock(block));

  CALL_BF(BF_CloseFile(__openFiles[indexDesc].descriptor));
  num_files--;
  __openFiles[indexDesc].descriptor = -1;

  BF_Block_Destroy(&block);
  return HT_OK;
}

HT_ErrorCode HT_InsertEntry(int indexDesc, Record record, int *tupleid,UpdateRecordArray *updateArray)
{




  if( power(2,__openFiles[indexDesc].__global_depth) > MAX_HASH_BLOCKS*(BF_BLOCK_SIZE/sizeof(int)) ){
    perror("GLOBAL DEPTH EXCEEDS HASH TABLE\n");      // if global depth exceeds the max directory size we return error
    return HT_ERROR;
  }
  
  int num_of_blocks;
  BF_Block *block;
  BF_Block_Init(&block);
  
  char *data; 

  int filedesc = __openFiles[indexDesc].descriptor; // our current file identifier we insert data
  if (__openFiles[indexDesc].descriptor == -1)
  {
    printf("There is no File in the %d position", indexDesc);
    return HT_ERROR;
  }
  // updateArray = NULL;

  // Hash record id to find its bucket
  int index = hash(record.id, __openFiles[indexDesc].__global_depth);
  
  
  
  int hash_block_index = index / (BF_BLOCK_SIZE/sizeof(int)) + 1; // find the block in the hash table that points to the required block
  int index_in_hashblock = index % (BF_BLOCK_SIZE/sizeof(int));   // find the position of the block


  // Get Block of the Hash table
  CALL_BF(BF_GetBlock(filedesc, hash_block_index, block));

  data = BF_Block_GetData(block);
  int blockNo;
  memcpy(&blockNo, data + index_in_hashblock * sizeof(int), sizeof(int)); // get the block number where we need to insert the id
  CALL_BF(BF_UnpinBlock(block));


  CALL_BF(BF_GetBlock(filedesc, blockNo, block)); // getting the block/bucket and it's data
  data = BF_Block_GetData(block);
  int rec_num;
  int local_depth;

  // Get the records number and local depth of the bucket
  memcpy(&rec_num, data + (BF_BLOCK_SIZE - 4) * sizeof(char), sizeof(int));
  memcpy(&local_depth, data + (BF_BLOCK_SIZE - 8) * sizeof(char), sizeof(int));

  *tupleid = blockNo*(BF_BLOCK_SIZE/sizeof(Record)) + rec_num; // returning tupleid

  /**************************  OVERFLOW  ****************************************/
  if (rec_num == (BF_BLOCK_SIZE-8)/sizeof(Record) )     // if bucket is full with records
  {

    BF_Block * temp_block;
    BF_Block_Init(&temp_block);
    
    
    // updateArray = (UpdateRecordArray*)malloc( ((BF_BLOCK_SIZE-8) /sizeof(Record)) *sizeof(UpdateRecordArray)); // malloc the array we need to return
  
    BF_AllocateBlock(filedesc, temp_block); // first alocate a new block with local depth N+1
    CALL_BF(BF_GetBlockCounter(filedesc, &num_of_blocks)); // keeping the new number of blocks

    if(local_depth == __openFiles[indexDesc].__global_depth){  // we need to expand directories
      
      int temp_directories[power(2,__openFiles[indexDesc].__global_depth)]; // temporary help array for the expansion
      
      

      char* hash_block_data;    // keep hash block data here 
      
      size_t position = 0;
      for (int i = 0; i < power(2,__openFiles[indexDesc].__global_depth); i++) // copy the full hash table to the array
      {
        CALL_BF(BF_GetBlock(filedesc,i/(BF_BLOCK_SIZE/sizeof(int))+1,temp_block));
        hash_block_data = BF_Block_GetData(temp_block);

        memcpy(&temp_directories[i],hash_block_data + position ,sizeof(int));

        CALL_BF(BF_UnpinBlock(temp_block));
        if(i/(BF_BLOCK_SIZE/sizeof(int)) == (i+1)/(BF_BLOCK_SIZE/sizeof(int))){
          position += sizeof(int);
        }
        else{
          position = 0;
        }
      }
      


      int new_dir[power(2,__openFiles[indexDesc].__global_depth +1)];       // make a new directories array with global depth + 1 (twice as big) ,pointing to correct buckets
      for (int i = 0; i < power(2,__openFiles[indexDesc].__global_depth); i++)
      {
        new_dir[2*i] = temp_directories[i];
        new_dir[2*i + 1] = temp_directories[i];   

      }
      __openFiles[indexDesc].__global_depth++; // increase global depth

      new_dir[hash(record.id,__openFiles[indexDesc].__global_depth)] = num_of_blocks -1; // fix the directory to the new hash block after increasing global depth
      




      /********************** FIX HASH TABLE *********************/
      position = 0;
      for (int i = 0; i < power(2,__openFiles[indexDesc].__global_depth); i++)   // saving the pointers in the hash table
      {
        
        CALL_BF(BF_GetBlock(filedesc,i/(BF_BLOCK_SIZE/sizeof(int))+1,temp_block)); 
        hash_block_data = BF_Block_GetData(temp_block);
        memcpy( hash_block_data + position, &new_dir[i], sizeof(int) );
        if ( i/(BF_BLOCK_SIZE/sizeof(int)) == (i+1)/(BF_BLOCK_SIZE/sizeof(int)) ) // if we are on same hash block
        {
          position += sizeof(int);
        }
        else{           // if we need to swap hash block
          position = 0;
        }
        BF_Block_SetDirty(temp_block);
        CALL_BF(BF_UnpinBlock(temp_block));

      }  
      

      // SAVING DATA OF THE BLOCK TO TEMP ARRAY
      //Record records[(BF_BLOCK_SIZE-8)/sizeof(Record)];
      Record *records = malloc(sizeof(Record)*(BF_BLOCK_SIZE-8)/sizeof(Record));
      size_t offset = 0;

      
      

      CALL_BF(BF_GetBlock(filedesc, blockNo, block));
      data = BF_Block_GetData(block);
      
      for (int i = 0; i < (BF_BLOCK_SIZE-8)/sizeof(Record); i++)
      {  
        
        memcpy(&records[i].id, data + offset , sizeof(int));
        memcpy(records[i].name, data + offset + offsetof(Record,name), sizeof(record.name));
        memcpy(records[i].surname, data + offset + offsetof(Record,surname), sizeof(record.surname));
        memcpy(records[i].city, data + offset + offsetof(Record,city), sizeof(record.city));
        
       /************  UPDATE ON OVERFLOW       ****************/
        
        memcpy(updateArray[i].surname,records[i].surname,sizeof(record.surname));        // new array of sec records for update
        memcpy(updateArray[i].city,records[i].city,sizeof(record.city));                 // will be used for SHT_Update
        // updateArray[i].newTupleId = new_dir[hash(records[i].id,__openFiles[indexDesc].__global_depth)];
        updateArray[i].oldTupleId = blockNo*((BF_BLOCK_SIZE-8)/sizeof(Record)) + i;

        offset += sizeof(Record);
      }
      
      /************       END UPDATE             ****************/ 
      
      //clearing data first
      memset(data,0,BF_BLOCK_SIZE);
      // we need to refix the bucket and init the new bucket with correct records number and local depth
      int new_rec_num = 0;
      int new_local_depth = local_depth + 1;
      
      memcpy(data + (BF_BLOCK_SIZE - 4) * sizeof(char), &new_rec_num, sizeof(int));     // set num of records = 0
      memcpy(data + (BF_BLOCK_SIZE - 8) * sizeof(char), &new_local_depth, sizeof(int)); // set local depth =n+1
      BF_Block_SetDirty(block);
      CALL_BF(BF_UnpinBlock(block));
      
      
      
      CALL_BF(BF_GetBlockCounter(filedesc, &num_of_blocks));
      CALL_BF(BF_GetBlock(filedesc, num_of_blocks - 1, block));
      data = BF_Block_GetData(block);
      memcpy(data + (BF_BLOCK_SIZE - 4) * sizeof(char), &new_rec_num, sizeof(int));     // set num of records = 0
      memcpy(data + (BF_BLOCK_SIZE - 8) * sizeof(char), &new_local_depth, sizeof(int)); // set local depth = n+1
      BF_Block_SetDirty(block);
      CALL_BF(BF_UnpinBlock(block));
      BF_Block_Destroy(&block);
      BF_Block_Destroy(&temp_block);
      // when we are done try to insert again the record and the records with new hashing (global depth + 1)
      int newtupleid;
      UpdateRecordArray *newarr;
      HT_InsertEntry(indexDesc,record,tupleid,newarr);
      
      for (int i = 0; i < (BF_BLOCK_SIZE-8)/sizeof(Record); i++)
      {
        HT_InsertEntry(indexDesc,records[i],&newtupleid,newarr);
        updateArray[i].newTupleId = newtupleid; // fix new tupleId
      }
      free(records);
      return HT_OK;

      
    }
    else{ // local depth < global depth so no need to resize just split
      
      
      int num_of_blocks;

      // Record records[(BF_BLOCK_SIZE-8)/sizeof(Record)];
      Record *records = malloc(sizeof(Record)*(BF_BLOCK_SIZE-8)/sizeof(Record));

      // SAVE ALL RECORDS OF THE BLOCK TO A TEMPORARY ARRAY TO REINSERT THEM AFTER THE NEW BUCKETS HAVE BEEN CREATED
      size_t offset = 0;
      CALL_BF(BF_GetBlock(filedesc, blockNo, block));
      data = BF_Block_GetData(block);
      
      for (int i = 0; i < (BF_BLOCK_SIZE-8)/sizeof(Record); i++)
      {
        memcpy(&records[i].id, data + offset , sizeof(int));
        memcpy(records[i].name, data + offset + offsetof(Record,name), sizeof(record.name));
        memcpy(records[i].surname, data + offset + offsetof(Record,surname), sizeof(record.surname));
        memcpy(records[i].city, data + offset + offsetof(Record,city), sizeof(record.city));

        memcpy(updateArray[i].surname,records[i].surname,sizeof(record.surname));        // new array of sec records for update
        memcpy(updateArray[i].city,records[i].city,sizeof(record.city));
        updateArray[i].oldTupleId = blockNo*((BF_BLOCK_SIZE-8)/sizeof(Record)) + i;

        offset += sizeof(Record);
      }

      //clearing data first
      memset(data,0,BF_BLOCK_SIZE);

      int new_rec_num = 0;
      int new_local_depth = local_depth + 1;
      memcpy(data + (BF_BLOCK_SIZE - 4) * sizeof(char), &new_rec_num, sizeof(int));     // set num of records = 0
      memcpy(data + (BF_BLOCK_SIZE - 8) * sizeof(char), &new_local_depth, sizeof(int)); // set local depth =n+1
      
      
      BF_Block_SetDirty(block);
      // CALL_BF(BF_UnpinBlock(block));

      CALL_BF(BF_GetBlockCounter(filedesc, &num_of_blocks)); // same spliting as before
      int newblockNo = num_of_blocks - 1;
      CALL_BF(BF_GetBlock(filedesc, newblockNo , block));
      data = BF_Block_GetData(block);
      memcpy(data + (BF_BLOCK_SIZE - 4) * sizeof(char), &new_rec_num, sizeof(int));     // set num of records = 0
      memcpy(data + (BF_BLOCK_SIZE - 8) * sizeof(char), &new_local_depth, sizeof(int)); // set local depth = n+1
      
      BF_Block_SetDirty(block);
      CALL_BF(BF_UnpinBlock(block));
      // now refix the directory to point to the correct bucket
      CALL_BF(BF_GetBlock(filedesc, hash_block_index, block));
      data = BF_Block_GetData(block);


      memcpy(data + index_in_hashblock * sizeof(int), &newblockNo , sizeof(int));
      BF_Block_SetDirty(block);
      CALL_BF(BF_UnpinBlock(block));

      BF_Block_Destroy(&block);
      // try to insert the record again as well as the records of the bucket with new hashing
      HT_InsertEntry(indexDesc,record,tupleid,updateArray);

      int newtupleid;
      UpdateRecordArray *newarr;
      for (int i = 0; i < (BF_BLOCK_SIZE-8)/sizeof(Record); i++)
      {
        HT_InsertEntry(indexDesc,records[i],&newtupleid,newarr);
        updateArray[i].newTupleId = newtupleid;
      }
      free(records);
      return HT_OK;
      
    }
    

  }
  else    // just a normal insertion since bucket is not full
  {
 
    // Copying data one at a time to avoid mistakes in copy to memory
    memcpy(data + rec_num * sizeof(Record), &record.id, sizeof(record.id));
    memcpy(data + rec_num * sizeof(Record) + offsetof(Record,name), &record.name, sizeof(record.name));
    memcpy(data + rec_num * sizeof(Record) + offsetof(Record,surname) , &record.surname, sizeof(record.surname));
    memcpy(data + rec_num * sizeof(Record) + offsetof(Record,city) , &record.city, sizeof(record.city));
    
    
    updateArray = NULL; //TODO 3

    // adding to num of records
    rec_num++;
    memcpy(data + (BF_BLOCK_SIZE - 4) * sizeof(char), &rec_num, sizeof(int));
    

    BF_Block_SetDirty(block);
    CALL_BF(BF_UnpinBlock(block));
    BF_Block_Destroy(&block);
  }

  return HT_OK;
}

HT_ErrorCode HT_PrintAllEntries(int indexDesc, int *id)
{
  // insert code here

  int flag = 0;
  BF_Block *block;
  BF_Block_Init(&block);
  int filedesc = __openFiles[indexDesc].descriptor; // our file identifier we print content from


  if (__openFiles[indexDesc].descriptor == -1)
  {
    printf("There is no File in the %d position", indexDesc);
    return HT_ERROR;
  }

  int num_of_blocks;

  char *data;
  Record record = {0};

 
  if (id == NULL) // to print all entries
  {
    printf("\nPrinting all Records : \n");
    CALL_BF(BF_GetBlockCounter(filedesc, &num_of_blocks)); // get num of blocks

    for (int k = MAX_HASH_BLOCKS + 1; k < num_of_blocks; k++)   // start after the blocks that store the hash table
    {
      
      printf("for block %d \n", k);
      CALL_BF(BF_GetBlock(filedesc, k, block));
      data = BF_Block_GetData(block);

      size_t offset = 0;

      for (size_t i = 0; i < (BF_BLOCK_SIZE-8)/sizeof(Record); i ++)
      {

        if (data[offset] == 0 && data[offset + 4] == 0) // this means that there is no record at that spot
        {         
         continue;
        }
        memcpy(&record.id, data + offset, sizeof(int));
        memcpy(record.name, data + offset + offsetof(Record,name), sizeof(record.name));
        memcpy(record.surname, data + offset + offsetof(Record,surname), sizeof(record.surname));
        memcpy(record.city, data + offset + offsetof(Record,city), sizeof(record.city));

        offset += sizeof(Record);
        // printing every record
        printf("{id :%d | name: %s | surname: %s | city: %s } \n", record.id, record.name, record.surname, record.city);
      }

      CALL_BF(BF_UnpinBlock(block));
    }
    printf("this is the current gd %d \n", __openFiles[indexDesc].__global_depth);
  }
  else
  {
    printf("Printing entries with id: %d \n" , *id);    // printing only specific id

    int index = hash(*id, __openFiles[indexDesc].__global_depth);              // hash the id
    int hash_block_index = index / (BF_BLOCK_SIZE/sizeof(int)) + 1; // get the block that has the directory
    int index_in_hashblock = index % (BF_BLOCK_SIZE/sizeof(int));   // position of bucket in that block


    CALL_BF(BF_GetBlock(filedesc, hash_block_index, block));
    data = BF_Block_GetData(block);
    int block_no;
  
    
    memcpy(&block_no, &data[index_in_hashblock * sizeof(int)], sizeof(int));  // get the block no

    CALL_BF(BF_UnpinBlock(block));

    CALL_BF(BF_GetBlock(filedesc,block_no,block));  // get the block/bucket that has the specific id
    data = BF_Block_GetData(block);
    size_t offset = 0;
    for (int i = 0; i < (BF_BLOCK_SIZE-8)/sizeof(Record); i++)
    {
        
      memcpy(&record.id, data + offset, sizeof(int));
      memcpy(record.name, data + offset + offsetof(Record,name), sizeof(record.name));
      memcpy(record.surname, data + offset + offsetof(Record,surname), sizeof(record.surname));
      memcpy(record.city, data + offset + offsetof(Record,city), sizeof(record.city));
      offset += sizeof(Record);

      if (record.id == *id)           // print only entries with the specific id
      {
        printf("{id :%d | name: %s | surname: %s | city: %s } \n", record.id, record.name, record.surname, record.city);
      }      
    }

    CALL_BF(BF_UnpinBlock(block));
  }

  BF_Block_Destroy(&block);
  return HT_OK;
}

HT_ErrorCode HT_HashStatistics(char* fileName)
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
  CALL_BF(BF_OpenFile(fileName, &fd1));
  BF_GetBlockCounter(fd1, &blocks_num);
  printf("\nTotal number of blocks in file is: %d\n", blocks_num);
  for (int k = MAX_HASH_BLOCKS + 1; k < blocks_num; k++) // get every block of records
  {
    
    CALL_BF(BF_GetBlock(fd1, k, block));
    data = BF_Block_GetData(block);
    int rec_num;
    memcpy(&rec_num,data + (BF_BLOCK_SIZE-4)* sizeof(char) , sizeof(int) );

    if (rec_num < min)  // set min records and the block number
    {
      min = rec_num;
      min_pos = k;
    }
    if (rec_num > max)  // set max records and the block number
    {
      max = rec_num;
      max_pos = k;
    }
    total_num_rec += rec_num; // add to total records
    CALL_BF(BF_UnpinBlock(block));
  }
  float avg = (float)total_num_rec/(blocks_num-MAX_HASH_BLOCKS);  // find the average
  printf("bucket number: %d has least records equal to: %d\n", min_pos, min);
  printf("bucket number: %d has most records equal to: %d\n", max_pos, max);
  printf("average number of records in every bucket is: %f\n", avg);

  CALL_BF(BF_CloseFile(fd1));
  BF_Block_Destroy(&block);
  return HT_OK;

}

void CreateArray(UpdateRecordArray **array){
  *(array) = malloc(sizeof(UpdateRecordArray)*8);
}
void DeleteArray(UpdateRecordArray **array){
  free(*(array));
}

/*                    BLOCK(k+1...n positions)
################################################################
|records         |local depth 4 bytes|number of records 4 bytes|
################################################################

FILE STRUCTURE

# ///////#   HB=hashblock
#0) Data #   B=Bucket <=> Block
#1) HB   #
#2) HB   #
#3) HB   #
#4) HB   #
#5) HB   #
#6) HB   #
#7) HB   #
#8) HB   #
#9) HB   #
#10)HB   #
#   .    #
#   .    #
#   .    #
#   .    #
#k) HB   #
#k+1)B   #
#   .    #
#   .    # 
#   .    #
#   .    #
#   .    # 
#   .    #
#n) B    #
#////////#
*/
