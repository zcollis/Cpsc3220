#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<pthread.h>
#include<time.h>

#define CACHE_SIZE 64
#define BLOCK_SIZE 1024
#define BLOCKS_ON_DISK 4096
#define MAX_THREADS 40

double timetaken[MAX_THREADS]; // array of each thread's times to completion
pthread_mutex_t *lock;  //lock for synchronization

//cache element structure
typedef struct cache_node
{
  struct cache_node *prev, *next;
  char* ptr;
  int dirty; //0 for unmodified, 1 for modified
}cache_node;

//cache implemented as double linked list QUEUE containing nodes as blocks
//helpful for implementing lru algorithm
//blocks move forward toward end of queue as procedures finish
typedef struct cache_t
{
  struct cache_node *head, *tail;
  int numFilled; //number of filled
  int available; //default # of available blocks
  int hit, miss;
}cache_t;

//array structure for storing evicted memory blocks
typedef struct disk_t
{
  int blockSize;
  int blocksOnDisk;
  cache_node** arr; //array to store evicted blocks
}disk_t;

//function prototypes
cache_node *create_node(char *x);
cache_t *create_cache();
disk_t *create_disk();
void blockread(char *x, int blocknum, cache_t* cache, disk_t* disk);
void blockwrite(char *x, int blocknum, cache_node* node, disk_t* disk);
void diskblockread(char *x, int blocknum, cache_node *node, disk_t* disk);
void diskblockwrite(char *x, int blocknum, cache_node *node, disk_t* disk);


//create and initialize node to store data
cache_node* create_node(char *x)
{
  cache_node *node = (cache_node*)malloc(sizeof(1024));

  //assign data from memory block into ptr
  node->ptr = x;
  node->dirty = 0;
  //purge all other nodes
  node->prev = NULL;
  node->next = NULL;

  return node;
}

//create and initialize cache to store nodes
//cache can store 64 blocks max
cache_t* create_cache()
{
  cache_t *cache = (cache_t*)malloc(sizeof(cache_t));

  //currently no elements
  cache->numFilled = 0;
  cache->hit = 0;
  cache->miss = 0;
  //64 blocks available from start
  cache->available = CACHE_SIZE;
  cache->head = NULL;
  cache->tail = NULL;

  return cache;
}

disk_t* create_disk()
{
  disk_t* disk = (disk_t*)malloc(sizeof(disk_t));
  int i;

  //4096 available blocks on disk
  disk->blocksOnDisk = BLOCKS_ON_DISK; // 0 - 4095

  //initialize array for uncached blocks
  disk->arr = (cache_node**)malloc(disk->blocksOnDisk * sizeof(cache_node*));

  //initialize empty array
  for(i = 0; i < disk->blocksOnDisk; i++)
  {
    disk->arr[i] = NULL;
  }

  return disk;
}

// reads data into pointer
// tries to read data into buffer cache
// if block already in cache, hit and add to front of queue
// if data not already in cache, miss, read data from disk,
// and place into buffer at available space. if no space available,
// evict least recently used block
void blockread(char *x, int blocknum, cache_t* cache, disk_t* disk)
{

  //read address of blocknum into x
  *x = blocknum;
  cache_node *node = create_node(x);

  //store data in disk for later reading/writing
  printf("block %d storing data in disk...\n", *(node->ptr));

  pthread_mutex_lock(lock);
  node->dirty = 0; // initial storage of data on disk
  disk->arr[blocknum] = node;

  //testing
  // printf("value of blocknum: %d\n", *x);
  // printf("address of blocknum: %p\n", x);

  printf("block %d reading data to cache...\n", blocknum);
  //cache empty, cache miss, insert node at head/tail and increment fill amount
  if(cache->numFilled == 0)
  {
    cache->miss++;
    cache->head = node; //set head to point to current node
    cache->tail = node;  //set tail node also to current (only) node
    node -> next = NULL;
    node -> prev = NULL;
    cache->numFilled++; //increment queue amount

    //testing
    // printf("value in cache:\n%d\n",*(curr->ptr));
    // printf("address of value in cache:\n%p\n",curr->ptr);

    cache->available -= cache->numFilled; //decrement available slots
  }
  else
  {
    cache_node *iter = cache->tail;

    //return data if already in cache (hit)
    while(iter->next != NULL)
    {
      if(*(iter->ptr) == *(node->ptr))  //matching blocknums
      {
        cache->hit++;
        iter = node;
        //return *(iter->ptr);
      }
      else
      {
        iter = iter->next;
        if(iter == NULL)
        {
          cache->miss++;
          //evict least recently used block: write data back to disk (if modified) and remove
          //read data from disk to buffer for node we want to replace with
          cache_node *temp = NULL;
          temp = cache->tail;
          if(cache->tail->dirty != 0)
          {
            cache->tail->dirty = 0;
            blockwrite(cache->tail->ptr, *(cache->tail->ptr), cache->tail, disk);
          }
          cache->tail = NULL;
          diskblockread(node->ptr, *(node->ptr), node, disk);
          node = temp;
          free(temp);
        }
      }
    }
  }
  pthread_mutex_unlock(lock);
  float ratio = cache->hit / cache->miss;
  printf("available cache space: %d\n",cache->available);
  printf("cache hit ratio: %f\n", ratio);
}

//block writes data to disk
void blockwrite(char *x, int blocknum, cache_node* node, disk_t* disk)
{
  printf("block %d writing data to disk...", blocknum);

  pthread_mutex_lock(lock);
  *x = blocknum;
  node->ptr = x;

  //store block in index of same number
  disk->arr[blocknum] = node;

  pthread_mutex_unlock(lock);
  //testing
  // printf("value of blocknum: %d\n", *x);
  // printf("address of blocknum: %p\n", x);
}

//read from disk to data block in buffer
//data not initially present in buffer
void diskblockread(char *x, int blocknum, cache_node *node, disk_t *disk)
{
  printf("reading block %d from disk to buffer...\n", blocknum);

  //retrieve block data and read into buffer
  pthread_mutex_lock(lock);

  node->ptr = x;
  node = disk->arr[blocknum];
  node->dirty = 0;

  pthread_mutex_unlock(lock);
}

//write from disk to block in buffer
void diskblockwrite(char *x, int blocknum, cache_node *node, disk_t* disk)
{
  printf("writing block %d from disk to buffer...\n", blocknum);

  pthread_mutex_lock(lock);

  *x = blocknum;
  node->ptr = x;
  node->dirty = 1;

  pthread_mutex_unlock(lock);
}

int main(int argc, char **argv)
{
  // // thread functionality
  // pthread_t threads[MAX_THREADS];
  // disk_t *disk = create_disk();
  // cache_t *cache = create_cache();
  // int i, block = rand()%4096; //sample data, will replace with command line argument later
  // char *store = NULL;
  // clock_t start, stop;
  //
  // for(i = 0; i < MAX_THREADS;)
  // {
  //   int rc = pthread_create(&threads[i], NULL, blockread, NULL);
  // }


return 0;
}
