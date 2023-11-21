#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <time.h>
#include "common.c"

bool DEBUG = false;

bool compare(unsigned char array1[HASH_SIZE], unsigned char array2[HASH_SIZE], int arraySize) {
    for (int i = 0; i < arraySize; i++)
    {
        if ( array1[i] != array2[i] ) {
            return false;
        }
    }
    return true;
}

// Function to convert a 12-byte array to an integer
unsigned int byteArrayToInt(unsigned char byteArray[HASH_SIZE], int startIndex)
{
    unsigned int result = 0;
    if (DEBUG)
        printArray(byteArray, HASH_SIZE);
    for (int i = 0; i < PREFIX_SIZE; i++)
    {
        result = (result << 8) | byteArray[startIndex + i];
    }
    return result;
}

int main() {

    double times[SEARCH_COUNT];

    int numBuckets = (int)pow(2, PREFIX_SIZE * 8);
    // int hashesPerBucket = 256; 
    // int hashesPerBucketRead = 256 * 256; // 1MB read
    printf("HASHES_PER_BUCKET = %zu\n", HASHES_PER_BUCKET);


    // Open the file for reading
    FILE *file = fopen("search.txt", "r");

    if (file == NULL) {
        perror("Error opening file");
        return 1;
    }

    unsigned char **array = (unsigned char **) malloc( SEARCH_COUNT * sizeof(char *) );

    unsigned char value;
    int bytesRead;

    for (int i = 0; i < SEARCH_COUNT; i++)
    {
        array[i] = (unsigned char *) malloc( HASH_SIZE );
        
        for (int j = 0; j < HASH_SIZE; j++)
        {
            fscanf(file, "%2hhx%n", &value, &bytesRead);
            array[i][j] = value;
        }
        fseek(file, 1, SEEK_CUR);
    }

    fclose(file);

    file = fopen("plot.memo", "rb");
    if (file == NULL) {
        perror("Error opening file");
        return 1;
    }

    struct hashObject *bucket = (struct hashObject *) malloc( FULL_BUCKET_SIZE * sizeof(struct hashObject) );

    for (int i = 0; i < SEARCH_COUNT; i++)
    // for (int i = 0; i < 1; i++)
    {
        clock_t start_time = clock();

        //Step 1 : Get the prefix
        unsigned int prefix = byteArrayToInt(array[i], 0);
        if (DEBUG) {
            printf("Prefix : %u\n", prefix);
        }

        //STEP 2 : Seek to the position in file
        size_t bucket_location = prefix * FULL_BUCKET_SIZE * sizeof(struct hashObject);
        if (fseeko(file, bucket_location, SEEK_SET) != 0)
        {
            perror("Error seeking in file");
            fclose(file);
            return 1;
        }
        if (DEBUG) {
            printf("Bucket Location : %zu\n", bucket_location/(1024*1024));
        }

        //STEP 3 : Read the bucket in memory
        size_t bytesRead = fread(bucket, 1, FULL_BUCKET_SIZE * sizeof(struct hashObject), file);
        if (bytesRead != FULL_BUCKET_SIZE * sizeof(struct hashObject))
        {
            perror("Error reading file");
            fclose(file);
            return 1;
        }

        //STEP 4 : Loop through records to search
        bool found = false;
        for (size_t j = 0; j < FULL_BUCKET_SIZE; j++)
        {
            if ( compare(bucket[j].byteArray, &array[i][PREFIX_SIZE], HASH_SIZE - PREFIX_SIZE) ) {
                // printArray(bucket[j].byteArray, HASH_SIZE - PREFIX_SIZE);
                break;
            }   
        }
        
        clock_t end_time = clock();
        double elapsed_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;

        times[i] = elapsed_time;

        printf("Iteration : %04d, Elapsed time: %lf seconds \n", i,elapsed_time);

    }
    
    free(bucket);

    for (size_t i = 0; i < SEARCH_COUNT; i++)
    {
        free(array[i]);
    }
    free(array);


    double max, min, mean = 0;
    max = times[0];
    min = times[0];
    for (size_t i = 0; i < SEARCH_COUNT; i++)
    {
        if ( times[i] > max ) {
            max = times[i];
        }
        if ( times[i] < min ) {
            min = times[i];
        }
        mean += times[i];
    }
    mean /= SEARCH_COUNT;
    
    printf("Min: %lf seconds, Max: %lf seconds, Mean: %lf seconds\n", min, max, mean);

    return 0;
}