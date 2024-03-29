#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include "blake3.h"
#include <sys/time.h>
#include <pthread.h>
#include "circular_queue.c"

bool DEBUG = false;

FILE *file;
size_t bytes_to_write = CUP_SIZE * sizeof(struct hashObject);

int *bucketIndex;
int *bucketFlush;
size_t totalFlushes = 0;
struct hashObject **array2D;
bool finished = false;

pthread_mutex_t *bucket_mutexes;
pthread_mutex_t file_mutex;

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

void* writeToDisk(void *arg) {
    int thread_id = *(int *)arg;
    while ( true )
    {
        Node *node = dequeue(&write_queue);
        if ( node == NULL && finished ) {
            break;
        }

        if ( node != NULL ) {
            // printf("dequeue, location - %ld\n", node->write_index);
            pthread_mutex_lock(&file_mutex);
            printf("Thread %d writing start\n", thread_id);

            if (fseeko(file, node->write_index, SEEK_SET) != 0)
            {
                perror("Error seeking in file");
                fclose(file);
                return NULL;
            }
            
            size_t bytesWritten = fwrite(node->array, 1, bytes_to_write, file);
            if (bytesWritten != bytes_to_write)
            {
                perror("Error writing to file");
                printf("fwrite()... %lu %lu %zu\n", node->write_index, bytes_to_write, bytesWritten);
                fclose(file);
                return NULL;
            }

            // free(node);

            totalFlushes++;
            if ( (int)(totalFlushes % 10) == 0 ) {
                printf("Flushed : %zu\n", totalFlushes);
            }
            printf("Thread %d writing done\n", thread_id);

            pthread_mutex_unlock(&file_mutex);
        }
    }
    printf("Thread %d exiting\n", thread_id);

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

void addChunkToQueue( unsigned int prefix ) {
    size_t write_location = prefix * (BUCKET_SIZE_MUTIPLIER * sizeof(struct hashObject)) + bucketFlush[prefix] * bytes_to_write;
    // printf("write_location - %ld, prefix - %d, flushIndex - %d\n", write_location, prefix, bucketFlush[prefix]);

    enqueue(&write_queue, write_location, array2D[prefix]);
    bucketFlush[prefix]++;
    bucketIndex[prefix] = 0;
}

void* generate_hashes(void *arg) {
    int thread_id = *(int *)arg;
    int iterations = MAX_HASHES / NUM_HASHGEN_THREADS;
    long int NONCE = iterations * thread_id;

    unsigned char randomArray[HASH_SIZE];

    printf("starting workload...\n");
    
    for (size_t i = 0; i < iterations; i++) {
        generateRandomByteArray(randomArray);
        unsigned int prefix = byteArrayToInt(randomArray, 0);
        pthread_mutex_lock(&bucket_mutexes[prefix]);
        int bIndex = bucketIndex[prefix];

        array2D[prefix][bIndex].value = NONCE;

        // printf("Thread Id - %d, prefix - %d, bIndex - %d, NONCE - %ld\n", thread_id, prefix, bIndex, NONCE);

        bucketIndex[prefix]++;
        if (bucketIndex[prefix] == CUP_SIZE)
        {
            addChunkToQueue(prefix);
        }
        pthread_mutex_unlock(&bucket_mutexes[prefix]);

        NONCE++;
    }   
}   

int main()
{
    srand((unsigned int)time(NULL));  

    printf("fopen()...\n");
    file = fopen("plot.memo", "wb"); // Open for appending
    if (file == NULL)
    {
        perror("Error opening file");
        return 1;
    }

    long int NONCE = 0;
    printf("NUM_BUCKETS=%zu\n", NUM_BUCKETS);
    printf("HASHES_PER_BUCKET_WRITE=%zu\n", CUP_SIZE);
    printf("BUCKET_SIZE=%zu\n", BUCKET_SIZE);

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
    bucketIndex = (int *)malloc(NUM_BUCKETS * sizeof(int));
    if (bucketIndex == NULL)
    {
        perror("Memory allocation failed");
        return 1;
    }

    printf("allocating bucketFlush memory...\n");
    bucketFlush = (int *)malloc(NUM_BUCKETS * sizeof(int));
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
    array2D = (struct hashObject **)malloc(NUM_BUCKETS * sizeof(struct hashObject *));
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

    printf("allocation mutex library....\n");
    bucket_mutexes = (pthread_mutex_t *) malloc( sizeof(pthread_mutex_t) * NUM_BUCKETS );
    for (size_t i = 0; i < NUM_BUCKETS; i++)
    {
        pthread_mutex_init(&bucket_mutexes[i], NULL);
    }
    pthread_mutex_init(&file_mutex, NULL);

    printf("initializing write queue\n");
    initializeQueue(&write_queue, QUEUE_SIZE);

    double total_time;
    struct timeval start_time, end_time;
    long long elapsed;
    gettimeofday(&start_time, NULL);

    printf("Hash Generation Starting...\n");

    pthread_t worker_threads[NUM_HASHGEN_THREADS];
    pthread_t write_threads[NUM_WRITE_THREADS];
    int threadsIds[NUM_HASHGEN_THREADS+NUM_WRITE_THREADS];

    for (int i = 0; i < NUM_HASHGEN_THREADS; i++)
    {
        threadsIds[i] = i;
        pthread_create(&worker_threads[i], NULL, generate_hashes, &threadsIds[i]);
    }

    for (int i = 0; i < NUM_WRITE_THREADS; i++)
    {
        threadsIds[NUM_HASHGEN_THREADS+i] = NUM_WRITE_THREADS+i;
        pthread_create(&write_threads[i], NULL, writeToDisk, &threadsIds[NUM_HASHGEN_THREADS+i]);
    }

    for (int i = 0; i < NUM_HASHGEN_THREADS; i++)
    {
        pthread_join(worker_threads[i], NULL);
    }    

    printf("Hash Generation Done...\n");

    for (size_t i = 0; i < NUM_BUCKETS; i++)
    {
        if ( bucketFlush[i] < (BUCKET_SIZE / CUP_SIZE) ) {
            addChunkToQueue(i);
        }
    }
    finished = true;

    for (int i = 0; i < NUM_WRITE_THREADS; i++)
    {
        pthread_join(worker_threads[i], NULL);
    }   

    printf("closing file...\n");
    fclose(file);

    gettimeofday(&end_time, NULL);
    elapsed = (end_time.tv_sec - start_time.tv_sec) * 1000000LL +
                            (end_time.tv_usec - start_time.tv_usec);
    total_time += (double)elapsed / 1000000.0;
    printf("Total time: %lf seconds \n", total_time);

    printf("free memory...\n");
    free(array2D);
    free(bucketIndex);
    free(bucketFlush);

    printf("all done!\n");

    return 0;
}