#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include "common.c"
#include "blake3.h"

#define HASH_SIZE 8
#define PREFIX_SIZE 1
#define COMPR_LEVEL 1 // Compression level (used in lossyCompression)
#define TOTAL_SIZE HASH_SIZE + 8 - PREFIX_SIZE - COMPR_LEVEL

struct someStruct
{
    // char byteArray[2];
    long int var;
    char temp;
};

void generateRandomByteArray(unsigned char result[HASH_SIZE])
{
    char random_data[HASH_SIZE];
    for (size_t j = 0; j < HASH_SIZE; j++)
    {
        random_data[j] = rand() % 256;
    }
    blake3_hasher hasher;
    blake3_hasher_init(&hasher);
    blake3_hasher_update(&hasher, random_data, HASH_SIZE);
    blake3_hasher_finalize(&hasher, result, HASH_SIZE);
}

unsigned int byteArrayToInt(unsigned char byteArray[HASH_SIZE], int startIndex)
{
    unsigned int result = 0;
    for (int i = 0; i < PREFIX_SIZE; i++)
    {
        result = (result << 8) | byteArray[startIndex + i];
    }
    return result;
}

// void compressArray(unsigned char result[HASH_SIZE])
// {
//     unsigned char result[HASH_SIZE - COMPR_LEVEL];
// }

int main()
{
    // char random_data;
    // unsigned char result[HASH_SIZE];
    // generateRandomByteArray(result);

    // for (int i = 0; i < HASH_SIZE; i++)
    // {
    //     printf("%u\n", result[i]);
    // }

    // printf("\n\n\n%ld", sizeof(result[1]));
    // printf("\n\n%d\n", HASH_SIZE);

    // unsigned int prefix = byteArrayToInt(result, 0);

    // printf("This is the prefix value: %d\n", prefix);
    // printf("PREFIX SIZE: %d\n", PREFIX_SIZE);

    // int arraySizeFinal = (int)(HASH_SIZE - COMPR_LEVEL);

    // printf("\nDisplayign the ByteArray after compressoin\n");
    // for (int i = 0; i < arraySizeFinal; i++)
    // {
    //     printf("%u\n", result[i]);
    // }
    // struct hashObject hashOb;
    // printf("size of structure: %d\n\n", sizeof(hashOb));

    // struct someStruct some;
    // printf("size of new struct: %ld\n", sizeof(some));
    unsigned seed = time(0);
    srand(seed);

    printf("%d", rand());

    printf("total size%d", TOTAL_SIZE);
}

//(array 1d, compression level)
