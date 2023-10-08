#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include "common.c"

bool DEBUG = false;

int main() {

    size_t numBuckets = (size_t)pow(2, PREFIX_SIZE * 8);
    printf("Number of bucket : %zu\n", numBuckets);

    const size_t numberOfHashesInBucket = HASHES_PER_BUCKET_READ < MAX_HASHES_SORTABLE ? HASHES_PER_BUCKET_READ : MAX_HASHES_SORTABLE ;    
    printf("Number of hashes per bucket to sort : %zu\n", numberOfHashesInBucket);

    const size_t numberOfBucketsToSort = (HASHES_PER_BUCKET / numberOfHashesInBucket) * numBuckets;
    printf("Number of bucket to sort : %zu\n", numberOfBucketsToSort);

    const size_t bucketSizeInBytes = numberOfHashesInBucket * sizeof(struct hashObject);
    struct hashObject *bucket1 = (struct hashObject *) malloc( bucketSizeInBytes );
    // struct hashObject *bucket2 = (struct hashObject *) malloc( bucketSizeInBytes );

    printf("fopen()...\n");

    FILE *file1 = fopen("plot.memo", "rb"); // Open for appending
    if (file1 == NULL)
    {
        perror("Error opening file1");
        return 1;
    }

    // FILE *file2 = fopen("plot1.memo", "rb"); // Open for appending
    // if (file2 == NULL)
    // {
    //     perror("Error opening file2");
    //     return 1;
    // }

    size_t bytesRead = fread(bucket1, 1, bucketSizeInBytes, file1);
    if (bytesRead != numberOfHashesInBucket * sizeof(struct hashObject))
    {
        perror("Error reading file");
        fclose(file1);
        // fclose(file2);
        return 1;
    }

    // bytesRead = fread(bucket2, 1, bucketSizeInBytes, file2);
    // if (bytesRead != numberOfHashesInBucket * sizeof(struct hashObject))
    // {
    //     perror("Error reading file");
    //     fclose(file1);
    //     fclose(file2);
    //     return 1;
    // }

    printf("Sorted\n");

    for (size_t i = 0; i < 100; i++)
    {
        printArray(bucket1[i].byteArray, 8);
    }
    
    // printf("Unsorted\n");

    // for (size_t i = 0; i < 100; i++)
    // {
    //     printArray(bucket2[i].byteArray, 8);
    // }

    fclose(file1);
    // fclose(file2);

    return 0;

}