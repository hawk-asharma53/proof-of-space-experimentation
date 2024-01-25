#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#include "blake3.h"

bool DEBUG = false;

#define HASH_SIZE 8
#define PREFIX_SIZE 1

struct hashObject
{
    char byteArray[HASH_SIZE - PREFIX_SIZE];
    long int value;
};

void printArray(unsigned char byteArray[HASH_SIZE], int arraySize)
{
    printf("printArray(): ");
    for (size_t i = 0; i < arraySize; i++)
    {
        printf("%02x ", byteArray[i]); // Print each byte in hexadecimal format
    }
    printf("\n");
}

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


int main(int argc, char *argv[])
{
    //assigning default values
    struct sysinfo info;

    if (sysinfo(&info) != 0) {
        perror("sysinfo");
        return 1;
    }

    unsigned long long total_memory = (unsigned long long)info.totalram * info.mem_unit - 1073741824;
    
    char *vault_filename = "memo.vault";

    struct statvfs stat;
    // Specify the path to the filesystem you want to check
    const char *path = "/";

    if (statvfs(path, &stat) != 0) {
        perror("statvfs");
        return 1;
    }

    // Calculate and print the available space in bytes
    // unsigned long long available_space = stat.f_bavail * stat.f_frsize;
    size_t available_space = stat.f_bavail * stat.f_frsize;

    //assigning values recieved from CLI input
    if (argc > 1)
    {
        vault_filename = argv[1];
        total_memory = atol(argv[2]) * 1024 * 1024;
        available_space = atol(argv[3]) * 1024 * 1024;
        // printf("You entered Vault Name: %s\n",vault_filename);
        // printf("You entered RAM memory used: %lld MB\n",(total_memory));
        // printf("You entered Totl vault size: %zu MB\n",(available_space));
    }


    //assigning variables based upon values from CLI

    size_t CUP_SIZE = (total_memory / 256) / sizeof(struct hashObject);
    size_t BUCKET_SIZE = (available_space / 256) / sizeof(struct hashObject);
    const size_t NUM_BUCKETS = 1 << (PREFIX_SIZE * 8);
    const size_t BUCKET_SIZE_MUTIPLIER = BUCKET_SIZE * 1;

    FILE *fp;
    fp = freopen("output_hashgen_try.txt", "w", stdout);
    if (fp == NULL) {
    // handle error
    }

    printf("Vault Name: %s\n",vault_filename);
    printf("RAM memory used: %lld MB\n",(total_memory/1024/1024));
    printf("Totl vault size: %zu MB\n",(available_space/1024/1024));

    srand((unsigned int)time(NULL)); 

    printf("fopen()...\n");
    FILE *file = fopen(vault_filename, "wb"); // Open for appending
    if (file == NULL)
    {
        perror("Error opening file");
        return 1;
    }

    long int nonce = 0;
    long int NONCE = 0;
    printf("NUM_BUCKETS=%zu\n", NUM_BUCKETS);
    printf("CUP_SIZE=%zu\n", CUP_SIZE);
    printf("BUCKET_SIZE=%zu\n", BUCKET_SIZE);

    size_t MAX_HASHES = NUM_BUCKETS * BUCKET_SIZE;
    printf("MAX_HASHES=%zu\n", MAX_HASHES);

    float memSize = 1.0 * NUM_BUCKETS * CUP_SIZE * sizeof(struct hashObject) / (1024.0 * 1024 * 1024);
    printf("total memory needed (GB): %f\n", memSize);
    float diskSize = 1.0 * MAX_HASHES * sizeof(struct hashObject) / (1024.0 * 1024 * 1024);
    printf("total disk needed (GB): %f\n", diskSize);
    
    printf("fseeko()...\n");
    // Seek past the end of the file
    long long desiredFileSize = BUCKET_SIZE_MUTIPLIER * NUM_BUCKETS * sizeof(struct hashObject);
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
    for (int i = 0; i < NUM_BUCKETS; i++) {
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
        array2D[i] = (struct hashObject *)malloc(CUP_SIZE * sizeof(struct hashObject));
        if (array2D[i] == NULL)
        {
            perror("Memory allocation failed");
            return 1;
        }
    }

    printf("initializing misc variables...\n");
    size_t startByteIndex = PREFIX_SIZE; 
    size_t bytes_to_write = CUP_SIZE * sizeof(struct hashObject);
    unsigned char randomArray[HASH_SIZE];
    int bIndex;
    size_t totalFlushes = 0;
    double write_time = 0;

    long search_items = 0;

    clock_t start_time = clock();

    size_t ideal_number_of_flushes = NUM_BUCKETS * (BUCKET_SIZE/CUP_SIZE);
    printf("Ideal number of flushes = %ld\n",ideal_number_of_flushes);
    printf("starting workload...\n");

    while(totalFlushes!=ideal_number_of_flushes)
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
        if (bucketIndex[prefix] == CUP_SIZE)
        {
            clock_t start_write = clock();

            if (DEBUG)
                printf("bucket is full...\n");
            // Seek to offset 100 bytes from the beginning of the file
            if (DEBUG)
                printf("fseeko()... %i %zu %i %zu\n", prefix, BUCKET_SIZE_MUTIPLIER, bucketFlush[prefix], bytes_to_write);
            long write_location = prefix * (BUCKET_SIZE_MUTIPLIER * sizeof(struct hashObject)) + bucketFlush[prefix] * bytes_to_write;
            if (DEBUG)
                printf("fseeko(%lu)...\n", write_location);
            if (fseeko(file, write_location, SEEK_SET) != 0)
            {
                perror("Error seeking in file");
                fclose(file);
                return 1;
            }
            // printf("Written cup for : %d\n",prefix);
            if(totalFlushes%64 == 0)
            {
                clock_t output_time = clock();
                double elapsed_output_time = (double)(output_time - start_time) / CLOCKS_PER_SEC;
                printf("Flushed: %zu in %lf sec %f%% => %f MB/sec\n",totalFlushes,elapsed_output_time,((totalFlushes/ideal_number_of_flushes)*1.0),((totalFlushes*CUP_SIZE*sizeof(struct hashObject)/1024/1024)/elapsed_output_time));
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

            if ( (int)(totalFlushes % 32768) == 0 ) {
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