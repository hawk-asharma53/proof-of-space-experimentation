#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include <math.h>
#include <stdbool.h>
#include "blake3.h"

#define HASHGEN_THREADS_BUFFER 1024
#define NUM_HASHGEN_THREADS 4
#define HASH_SIZE 8
#define NUM_HASHES (1024 * 1024 * 64)
#define BATCH_SIZE 256

struct CircularArray {
    unsigned char array[HASHGEN_THREADS_BUFFER][HASH_SIZE];
    size_t head;
    size_t tail;
    int producerFinished;  // Flag to indicate when the producer is finished
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
};

void initCircularArray(struct CircularArray *circularArray) {
    circularArray->head = 0;
    circularArray->tail = 0;
    circularArray->producerFinished = 0;
    pthread_mutex_init(&(circularArray->mutex), NULL);
    pthread_cond_init(&(circularArray->not_empty), NULL);
    pthread_cond_init(&(circularArray->not_full), NULL);
}

void destroyCircularArray(struct CircularArray *circularArray) {
    pthread_mutex_destroy(&(circularArray->mutex));
    pthread_cond_destroy(&(circularArray->not_empty));
    pthread_cond_destroy(&(circularArray->not_full));
}

void insertBatch(struct CircularArray *circularArray, unsigned char values[BATCH_SIZE][HASH_SIZE]) {
    pthread_mutex_lock(&(circularArray->mutex));

    // Wait while the circular array is full and producer is not finished
    while ((circularArray->head + BATCH_SIZE) % HASHGEN_THREADS_BUFFER == circularArray->tail && !circularArray->producerFinished) {
        pthread_cond_wait(&(circularArray->not_full), &(circularArray->mutex));
    }

    // Insert values
    for (int i = 0; i < BATCH_SIZE; i++) {
        memcpy(circularArray->array[circularArray->head], values[i], HASH_SIZE);
        circularArray->head = (circularArray->head + 1) % HASHGEN_THREADS_BUFFER;
    }

    // Signal that the circular array is not empty
    pthread_cond_signal(&(circularArray->not_empty));

    pthread_mutex_unlock(&(circularArray->mutex));
}

void removeBatch(struct CircularArray *circularArray, unsigned char result[BATCH_SIZE][HASH_SIZE]) {
    pthread_mutex_lock(&(circularArray->mutex));

    // Wait while the circular array is empty and producer is not finished
    while (circularArray->tail == circularArray->head && !circularArray->producerFinished) {
        pthread_cond_wait(&(circularArray->not_empty), &(circularArray->mutex));
    }

    // Remove values
    for (int i = 0; i < BATCH_SIZE; i++) {
        memcpy(result[i], circularArray->array[circularArray->tail], HASH_SIZE);
        circularArray->tail = (circularArray->tail + 1) % HASHGEN_THREADS_BUFFER;
    }

    // Signal that the circular array is not full
    pthread_cond_signal(&(circularArray->not_full));

    pthread_mutex_unlock(&(circularArray->mutex));
}

// Thread data structure
struct ThreadData {
    struct CircularArray *circularArray;
    int threadID;
};

// Function to generate a random 8-byte array
void generateRandomByteArray(unsigned char result[HASH_SIZE], int NONCE) {
    blake3_hasher hasher;
    blake3_hasher_init(&hasher);
    blake3_hasher_update(&hasher, &NONCE, sizeof(NONCE));
    blake3_hasher_finalize(&hasher, result, HASH_SIZE);
}

// Function to be executed by each thread for array generation
void *arrayGenerationThread(void *arg) {
    struct ThreadData *data = (struct ThreadData *)arg;

    unsigned char batch[BATCH_SIZE][HASH_SIZE];
    int NUM_HASHES_PER_THREAD = (int)(NUM_HASHES / NUM_HASHGEN_THREADS);

    for (int i = 0; i < NUM_HASHES_PER_THREAD; i += BATCH_SIZE) {
        for (int j = 0; j < BATCH_SIZE; j++) {
            int hashIndex = NUM_HASHES_PER_THREAD * data->threadID + i + j;
            generateRandomByteArray(batch[j], hashIndex);
        }

        insertBatch(data->circularArray, batch);
    }

    return NULL;
}

int main() {
    // Initialize the circular array
    struct CircularArray circularArray;
    initCircularArray(&circularArray);

    // Create thread data structure
    struct ThreadData threadData[NUM_HASHGEN_THREADS];

    // Create threads
    pthread_t threads[NUM_HASHGEN_THREADS];
    for (int i = 0; i < NUM_HASHGEN_THREADS; i++) {
        threadData[i].circularArray = &circularArray;
        threadData[i].threadID = i;
        pthread_create(&threads[i], NULL, arrayGenerationThread, &threadData[i]);
    }

    // Measure time taken for hash generation using gettimeofday
    struct timeval start_time, end_time;
    gettimeofday(&start_time, NULL);

    // Main thread: Consume elements from the circular array
    for (int i = 0; i < NUM_HASHES; i += BATCH_SIZE) {
        // Consume BATCH_SIZE array elements
        unsigned char consumedArray[BATCH_SIZE][HASH_SIZE];
        removeBatch(&circularArray, consumedArray);

        // Process the consumed array (e.g., print it)
        // printf("Consumed Array: ");
        // for (size_t k = 0; k < HASH_SIZE; k++) {
        //     printf("%02X ", consumedArray[k]);
        // }
        // printf("\n");

        if (i % 1000000 == 0) {
            gettimeofday(&end_time, NULL);
            // Calculate and print estimated completion time
            double progress = i * 100.0 / NUM_HASHES;
            double elapsed_time = (end_time.tv_sec - start_time.tv_sec) +
                                  (end_time.tv_usec - start_time.tv_usec) / 1.0e6;
            double remaining_time = elapsed_time / (progress / 100.0) - elapsed_time;
            printf("Progress: %d %.2lf%%, Estimated Completion Time: %.2lf seconds\n", i, progress, remaining_time);
        }
    }

    // Measure elapsed time
    gettimeofday(&end_time, NULL);
    double elapsed_time = (end_time.tv_sec - start_time.tv_sec) +
                          (end_time.tv_usec - start_time.tv_usec) / 1.0e6;

    // Calculate throughput
    double throughput = NUM_HASHES / elapsed_time;

    // Print timing information
    printf("Elapsed Time: %lf seconds\n", elapsed_time);
    printf("Throughput: %lf hashes per second\n", throughput);
    printf("Throughput: %f MB second\n", throughput * 8.0 / (1024 * 1024));

    // Set the producerFinished flag to true
    circularArray.producerFinished = 1;

    // Signal that the circular array is not empty
    pthread_cond_broadcast(&(circularArray.not_empty));

    // Wait for threads to finish
    for (int i = 0; i < NUM_HASHGEN_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    destroyCircularArray(&circularArray);

    return 0;
}
