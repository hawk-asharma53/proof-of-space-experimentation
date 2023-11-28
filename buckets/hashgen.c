#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include "common.c"
#include "blake3.h"

bool DEBUG = false;

// Function to generate a random 8-byte array
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

int main()
{
    srand((unsigned int)time(NULL));

    printf("fopen()...\n");
    FILE *file = fopen("plot.memo", "wb"); // Open for appending
    if (file == NULL)
    {
        perror("Error opening file");
        return 1;
    }

    long int NONCE = 0;
    printf("NUM_BUCKETS=%zu\n", NUM_BUCKETS);
    printf("HASHES_PER_BUCKET=%zu\n", HASHES_PER_BUCKET);
    printf("HASHES_PER_BUCKET_READ=%zu\n", HASHES_PER_BUCKET_READ);

    size_t MAX_HASHES = NUM_BUCKETS * HASHES_PER_BUCKET_READ;
    printf("MAX_HASHES=%zu\n", MAX_HASHES);

    float memSize = 1.0 * NUM_BUCKETS * HASHES_PER_BUCKET * sizeof(struct hashObject) / (1024.0 * 1024 * 1024);
    printf("total memory needed (GB): %f\n", memSize);
    float diskSize = 1.0 * MAX_HASHES * sizeof(struct hashObject) / (1024.0 * 1024 * 1024);
    printf("total disk needed (GB): %f\n", diskSize);

    printf("fseeko()...\n");
    // Seek past the end of the file
    long long desiredFileSize = FULL_BUCKET_SIZE * NUM_BUCKETS * sizeof(struct hashObject);
    printf("hashgen: setting size of file to %lld\n", desiredFileSize);
    if (fseeko(file, desiredFileSize - 1, SEEK_SET) != 0)
    {
        perror("Error seeking in file");
        fclose(file);
        return 1;
    }
    // Write a single byte at the desired position to set the file size
    fputc(0, file);

    printf("allocating bucketIndex memory...\n");
    int *bucketIndex = (int *)malloc(NUM_BUCKETS * sizeof(int));
    if (bucketIndex == NULL)
    {
        perror("Memory allocation failed");
        return 1;
    }

    printf("allocating bucketFlush memory...\n");
    int *bucketFlush = (int *)malloc(NUM_BUCKETS * sizeof(int));
    if (bucketFlush == NULL)
    {
        perror("Memory allocation failed");
        return 1;
    }

    printf("initializing bucketFlush and bucketIndex memory...\n");
    for (int i = 0; i < NUM_BUCKETS; i++)
    {
        bucketIndex[i] = 0;
        bucketFlush[i] = 0;
    }

    printf("allocating array2D memory...\n");
    struct hashObject **array2D = (struct hashObject **)malloc(NUM_BUCKETS * sizeof(struct hashObject *));
    if (array2D == NULL)
    {
        perror("Memory allocation failed");
        return 1;
    }

    printf("allocating array2D[][] memory...\n");
    for (int i = 0; i < NUM_BUCKETS; i++)
    {
        array2D[i] = (struct hashObject *)malloc(HASHES_PER_BUCKET * sizeof(struct hashObject));
        if (array2D[i] == NULL)
        {
            perror("Memory allocation failed");
            return 1;
        }
    }

    printf("initializing misc variables...\n");
    size_t startByteIndex = PREFIX_SIZE;
    size_t bytes_to_write = HASHES_PER_BUCKET * sizeof(struct hashObject);
    unsigned char randomArray[HASH_SIZE];
    int bIndex;
    size_t totalFlushes = 0;
    double write_time = 0;

    long search_items = 0;

    clock_t start_time = clock();

    printf("starting workload...\n");
    for (size_t i = 0; i < MAX_HASHES; i++)
    // for (unsigned int i = 0; i < 1000; i++)
    {
        if (DEBUG)
            printf("generateRandomByteArray()...\n");
        // Access and manipulate the elements
        generateRandomByteArray(randomArray);

        // Extract the first 3 bytes starting from index 0 and convert to an integer
        if (DEBUG)
            printf("byteArrayToInt()...\n");
        unsigned int prefix = byteArrayToInt(randomArray, 0);
        bIndex = bucketIndex[prefix];
        // Use memcpy to copy the contents of randomArray to copyArray
        if (DEBUG)
            printf("memcpy()...\n");
        memcpy(array2D[prefix][bIndex].byteArray, randomArray + PREFIX_SIZE, sizeof(randomArray) - PREFIX_SIZE);
        // bucketIndex[prefix]++;
        // array2D[prefix][bucketIndex[prefix]++]->byteArray;
        if (DEBUG)
            printf("setting array2D and bucketIndex...\n");
        array2D[prefix][bIndex].value = NONCE;
        bucketIndex[prefix]++;
        NONCE++;
        // bucket is full, should write to disk
        if (bucketIndex[prefix] == HASHES_PER_BUCKET)
        {
            clock_t start_write = clock();

            if (DEBUG)
                printf("bucket is full...\n");
            // Seek to offset 100 bytes from the beginning of the file
            if (DEBUG)
                printf("fseeko()... %i %zu %i %zu\n", prefix, FULL_BUCKET_SIZE, bucketFlush[prefix], bytes_to_write);
            // WRITE CODE HERE //
            long write_location = prefix * (FULL_BUCKET_SIZE * sizeof(struct hashObject)) + bucketFlush[prefix] * bytes_to_write;
            if (DEBUG)
                printf("fseeko(%lu)...\n", write_location);
            if (fseeko(file, write_location, SEEK_SET) != 0)
            {
                perror("Error seeking in file");
                fclose(file);
                return 1;
            }
            // Write the buffer to the file
            // long writeLocation = prefix+bucketFlush[prefix]*bytes_to_write;
            if (DEBUG)
                printf("fwrite()... %i %lu %i\n", prefix, bytes_to_write, bucketFlush[prefix]);

            size_t bytesWritten = fwrite(array2D[prefix], 1, bytes_to_write, file);

            if (bytesWritten != bytes_to_write)
            {
                perror("Error writing to file");
                printf("fwrite()... %lu %i %lu %i %zu\n", write_location, prefix, bytes_to_write, bucketFlush[prefix], bytesWritten);
                fclose(file);
                return 1;
            }

            if (DEBUG)
                printf("updating bucketFlush and bucketIndex...\n");

            totalFlushes++;
            bucketFlush[prefix]++;
            bucketIndex[prefix] = 0;

            clock_t end_write = clock();
            write_time += (double)(end_write - start_write) / CLOCKS_PER_SEC;

            if ((int)(totalFlushes % 1024) == 0)
            {
                printf("Flushed : %zu\n", totalFlushes);
            }
        }
    }

    printf("Completed generating hashes\n");

    for (size_t i = 0; i < NUM_BUCKETS; i++)
    {
        if (bucketFlush[i] < (HASHES_PER_BUCKET_READ / HASHES_PER_BUCKET))
        {
            clock_t start_write = clock();

            //Write code here// array2D -> each bucket there is an array
            long write_location = i * (FULL_BUCKET_SIZE * sizeof(struct hashObject)) + bucketFlush[i] * bytes_to_write;
            if (fseeko(file, write_location, SEEK_SET) != 0)
            {
                perror("Error seeking in file");
                fclose(file);
                return 1;
            }
            size_t bytesWritten = fwrite(array2D[i], 1, bytes_to_write, file);
            if (bytesWritten != bytes_to_write)
            {
                perror("Error writing to file");
                printf("fwrite()... %lu %zu %lu %i %zu\n", write_location, i, bytes_to_write, bucketFlush[i], bytesWritten);
                fclose(file);
                return 1;
            }

            // fflush(file);

            totalFlushes++;
            bucketFlush[i]++;
            bucketIndex[i] = 0;

            clock_t end_write = clock();
            write_time += (double)(end_write - start_write) / CLOCKS_PER_SEC;

            if ((int)(totalFlushes % 1024) == 0)
            {
                printf("Flushed : %zu\n", totalFlushes);
            }
        }
    }

    printf("finished workload...\n");

    printf("Total flushes : %zu\n", totalFlushes);

    // its possible not all buckets have been written to disk, should check, and keep generating hashes until all buckets are written to disk

    printf("closing file...\n");
    // Close the file when done
    fclose(file);

    printf("free bucketIndex...\n");
    // Don't forget to free the allocated memory when you're done
    free(bucketIndex);
    free(bucketFlush);

    printf("free array2D[][]...\n");
    // Free the allocated memory
    for (int i = 0; i < NUM_BUCKETS; i++)
    {
        free(array2D[i]);
    }
    printf("free array2D...\n");
    free(array2D);

    clock_t end_time = clock();
    double elapsed_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;

    printf("Time to write: %lf seconds \n", write_time);
    printf("Time to complete: %lf seconds \n", elapsed_time);

    printf("all done!\n");

    return 0;
}