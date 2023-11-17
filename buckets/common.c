#include <stdlib.h>
#include <math.h>

#define HASH_SIZE 8
#define PREFIX_SIZE 1

struct hashObject
{
    char byteArray[HASH_SIZE - PREFIX_SIZE];
    long int value;
};

const size_t FILE_SIZE = 178;

const size_t NUM_BUCKETS = 1 << (PREFIX_SIZE * 8);
const size_t HASHES_PER_BUCKET = 256 * 1024 * 2; 
const size_t HASHES_PER_BUCKET_READ = 256 * 256 * 256;
const size_t FULL_BUCKET_SIZE = HASHES_PER_BUCKET_READ * 3;

const int SEARCH_COUNT = 1000;

const size_t SORT_SIZE = 16; //In MB
const size_t MAX_HASHES_SORTABLE = (SORT_SIZE * 1024 * 1024) / sizeof(struct hashObject);
const size_t HASHES_PER_CHUNK_SORT = HASHES_PER_BUCKET_READ < MAX_HASHES_SORTABLE ? HASHES_PER_BUCKET_READ : MAX_HASHES_SORTABLE; 
const int NUM_THREADS = 2;   

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

