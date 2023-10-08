#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <time.h>
#include "common.c"
#include <math.h>

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

int compareByteArrays(const char *arr1, const char *arr2)
{
    return memcmp(arr1, arr2, sizeof(arr1));
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


int binarySearch(struct hashObject *array, size_t left, size_t right, const char *target)
{
    while (left <= right)
    {
        int mid = left + (right - left) / 2;

        // Compare the target with the middle element's byteArray
        int cmp = compareByteArrays(target, array[mid].byteArray);

        if (cmp == 0)
        {
            // Found a match, return the index
            return mid;
        }
        else if (cmp < 0)
        {
            // If target is smaller, search in the left half
            right = mid - 1;
        }
        else
        {
            // If target is larger, search in the right half
            left = mid + 1;
        }
    }

    // Element not found
    return -1;
}

// Function to compare two doubles for qsort
int cmpfunc(const void* a, const void* b) {
    double diff = (*(double*)a - *(double*)b);
    if (diff < 0) return -1;
    if (diff > 0) return 1;
    return 0;
}

int main() {

    double times[SEARCH_COUNT];

    const size_t numberOfBucketsToSort = (HASHES_PER_BUCKET_READ / HASHES_PER_CHUNK_SORT) * NUM_BUCKETS;

    printf("HASHES_PER_BUCKET_READ = %zu\n", HASHES_PER_BUCKET_READ);


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

    struct hashObject *bucket = (struct hashObject *) malloc( HASHES_PER_BUCKET_READ * sizeof(struct hashObject) );

    for (int i = 0; i < SEARCH_COUNT; i++)
    // for (int i = 0; i < 1; i++)
    {
        bool found = false;

        clock_t start_time = clock();

        //Step 1 : Get the prefix
        unsigned int prefix = byteArrayToInt(array[i], 0);
        if (DEBUG) {
            printf("Prefix : %u\n", prefix);
        }

        size_t numberOfSortedBuckets = HASHES_PER_BUCKET_READ < MAX_HASHES_SORTABLE ? 1 : HASHES_PER_BUCKET_READ / MAX_HASHES_SORTABLE;

        for (size_t j = 0; j < numberOfSortedBuckets; j++)
        {
            //STEP 2 : Seek to the position in file
            size_t bucket_location = (prefix * HASHES_PER_BUCKET_READ + j * HASHES_PER_CHUNK_SORT) * sizeof(struct hashObject)  ;
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
            size_t bytesRead = fread(bucket, 1, HASHES_PER_CHUNK_SORT * sizeof(struct hashObject), file);
            if (bytesRead != HASHES_PER_CHUNK_SORT * sizeof(struct hashObject))
            {
                perror("Error reading file");
                fclose(file);
                return 1;
            }

            //STEP 4 : Perform binary search
            int search_result = binarySearch( bucket, 0, HASHES_PER_CHUNK_SORT-1, &array[i][PREFIX_SIZE]);
            if ( search_result != -1 ) {
                found = true;
                break;
            }
        }
        
        clock_t end_time = clock();
        double elapsed_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;

        times[i] = elapsed_time;

        printf("Iteration : %04d, Found : %d, Elapsed time: %lf seconds \n", i, found,elapsed_time);

    }
    
    free(bucket);

    for (size_t i = 0; i < SEARCH_COUNT; i++)
    {
        free(array[i]);
    }
    free(array);

    qsort(times, SEARCH_COUNT, sizeof(double), cmpfunc);

    double mean = 0, stdDev = 0;
    for (size_t i = 0; i < SEARCH_COUNT; i++)
    {
        mean += times[i];
    }
    mean /= SEARCH_COUNT;

    for (size_t i = 0; i < SEARCH_COUNT; i++)
    {
        stdDev += (times[i] - mean) * (times[i] - mean);
    }
    stdDev = sqrt(stdDev / SEARCH_COUNT);

    printf("Min: %lf seconds\n", times[0]);
    printf("Max: %lf seconds\n", times[SEARCH_COUNT-1]);
    printf("Mean: %lf seconds\n", mean);
    printf("Medium: %lf seconds\n", times[(SEARCH_COUNT/2)-1]);
    printf("Std Dev: %lf seconds\n", stdDev);

    return 0;
}