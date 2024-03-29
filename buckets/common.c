#include <stdlib.h>
#include <math.h>

#define HASH_SIZE 8
#define PREFIX_SIZE 1

struct hashObject
{
    char byteArray[HASH_SIZE - PREFIX_SIZE];
    long int value;
};

const size_t NUM_BUCKETS = 1 << (PREFIX_SIZE * 8);
const size_t CUP_SIZE = 256 * 1024; 
const size_t BUCKET_SIZE = 2 * 256 * 1024 ;
const size_t BUCKET_SIZE_MUTIPLIER = BUCKET_SIZE * 1;

const size_t MAX_HASHES = NUM_BUCKETS * BUCKET_SIZE;

const int SEARCH_COUNT = 1000;

const size_t SORT_SIZE = 128; //In MB
const size_t MAX_HASHES_SORTABLE = (SORT_SIZE * 1024 * 1024) / sizeof(struct hashObject);
const size_t HASHES_PER_CHUNK_SORT = BUCKET_SIZE < MAX_HASHES_SORTABLE ? BUCKET_SIZE : MAX_HASHES_SORTABLE; 
const int NUM_THREADS = 16;   

const int NUM_HASHGEN_THREADS = 2;
const int NUM_WRITE_THREADS = 2;
const int QUEUE_SIZE = 10;

void printArray(unsigned char byteArray[HASH_SIZE], int arraySize)
{
    printf("printArray(): ");
    for (size_t i = 0; i < arraySize; i++)
    {
        printf("%02x ", byteArray[i]); // Print each byte in hexadecimal format
    }
    printf("\n");
}

int compareHashObjects(const void *a, const void *b)
{
    const struct hashObject *hashObjA = (const struct hashObject *)a;
    const struct hashObject *hashObjB = (const struct hashObject *)b;

    // Compare the byteArray fields
    return memcmp(hashObjA->byteArray, hashObjB->byteArray, sizeof(hashObjA->byteArray));
}

