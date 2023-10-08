#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include "common.c"

bool DEBUG = false;

int main()
{
    srand(time(NULL));

    long index = 0;

    FILE *file = fopen("plot.memo", "rb"); // Open for appending
    if (file == NULL)
    {
        perror("Error opening file");
        return 1;
    }

    unsigned char **search_array = (unsigned char **)malloc(SEARCH_COUNT * sizeof(char *));

    while (index < SEARCH_COUNT)
    {
        int random_bucket = rand() % NUM_BUCKETS;
        int random_index = rand() % HASHES_PER_BUCKET_READ;

        size_t line_number= (random_bucket * HASHES_PER_BUCKET_READ) + random_index;

        if (fseeko(file, line_number * sizeof(struct hashObject), SEEK_SET) != 0)
        {
            perror("Error seeking in file");
            fclose(file);
            return 1;
        }

        struct hashObject *hashObj = (struct hashObject *) malloc( sizeof(struct hashObject) );
        size_t bytesRead = fread(hashObj, 1, sizeof(struct hashObject), file);
        if (bytesRead != sizeof(struct hashObject))
        {
            perror("Error reading file");
            fclose(file);
            return 1;
        }


        search_array[index] = (unsigned char *)malloc(HASH_SIZE+1);
        memcpy(&search_array[index][PREFIX_SIZE], hashObj->byteArray, HASH_SIZE - PREFIX_SIZE);

        size_t total = 0;
        for (int j = PREFIX_SIZE; j < (HASH_SIZE); j++)
        {
            total += search_array[index][j];
        }
        if (total == 0) {
            continue;
        }
        

        for (size_t j = 0; j < PREFIX_SIZE; j++) {
            search_array[index][j] = (random_bucket >> (8 * (PREFIX_SIZE - 1 - j))) & 0xFF;
        }

        printArray(search_array[index], HASH_SIZE);

        index++;
    }

    fclose(file);


    printf("Write searches to text file...\n");
    FILE *file1 = fopen("search.txt", "w");   
    if (file1 == NULL) {
        perror("Error opening file");
        return 1;
    } 
    // printf("%s", search_array[0]);


    // Write the hash results to the file
    for (size_t k = 0; k < SEARCH_COUNT; k++) {
        for (size_t j = 0; j < HASH_SIZE; j++) {
            fprintf(file1, "%02x", search_array[k][j]);
        }
        fprintf(file1, "\n");
    }
    //Close file
    fclose(file1);
    
    return 0;
}