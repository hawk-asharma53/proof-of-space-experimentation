#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/time.h>
#include "common.c"

bool DEBUG = false;
size_t bucketSizeInBytes;
struct hashObject *buckets;

// Function to sort a bucket
void *sortBucket(void *arg)
{
    int thread_id = *(int *)arg;
    // printf("Thread id - %d\n", thread_id);

    struct hashObject *bucket = &buckets[thread_id * HASHES_PER_CHUNK_SORT];

    // printf("Thread id - %d, Starting Sort\n", thread_id);
    qsort(bucket, HASHES_PER_CHUNK_SORT, sizeof(struct hashObject), compareHashObjects);
    // printf("Thread id - %d, Sorting Done\n", thread_id);
    
    return NULL;
}

int main() {

    const size_t NUM_CHUNKS_SORT = (BUCKET_SIZE / HASHES_PER_CHUNK_SORT) * NUM_BUCKETS;

    printf("BUCKET_SIZE : %zu\n", BUCKET_SIZE);
    printf("NUM_BUCKETS : %zu\n", NUM_BUCKETS);
    printf("HASHES_PER_CHUNK_SORT : %zu\n", HASHES_PER_CHUNK_SORT);
    printf("NUM_CHUNKS_SORT : %zu\n", NUM_CHUNKS_SORT);

    bucketSizeInBytes = HASHES_PER_CHUNK_SORT * sizeof(struct hashObject);
    buckets = (struct hashObject *) malloc( bucketSizeInBytes * NUM_THREADS );

    printf("Malloc Size : %zu\n", bucketSizeInBytes * NUM_THREADS );

    printf("fopen()...\n");

    FILE *file = fopen("plot.memo", "rb+"); // Open for appending
    if (file == NULL)
    {
        perror("Error opening file");
        return 1;
    }

    printf("Starting Sort.... \n");

    double reading_time, sorting_time, writing_time, total_time;
    struct timeval start_time, end_time, start_all, end_all;
    long long elapsed;

    gettimeofday(&start_all, NULL);
    for (size_t i = 0; i < (NUM_CHUNKS_SORT / NUM_THREADS); i++)
    {

        gettimeofday(&start_time, NULL);
        //STEP 1 : Seek to location
        size_t bucket_location = i * bucketSizeInBytes * NUM_THREADS;
        if (fseeko(file, bucket_location, SEEK_SET) != 0)
        {
            perror("Error seeking in file");
            fclose(file);
            return 1;
        }
        if (DEBUG) {
            printf("Bucket Location : %zu\n", bucket_location/(1024*1024));
        }

        //STEP 2 : Read the data from the file
        size_t bytesRead = fread(buckets, 1, bucketSizeInBytes * NUM_THREADS, file);
        if (bytesRead != bucketSizeInBytes * NUM_THREADS)
        {
            perror("Error reading file");
            fclose(file);
            return 1;
        }

        gettimeofday(&end_time, NULL);
        elapsed = (end_time.tv_sec - start_time.tv_sec) * 1000000LL +
                             (end_time.tv_usec - start_time.tv_usec);
        reading_time += (double)elapsed / 1000000.0;


        //STEP 3 : Sort the bucket
        gettimeofday(&start_time, NULL);
        pthread_t threads[NUM_THREADS];
        int threadsIds[NUM_THREADS];

        // Create threads to sort each bucket in parallel
        for (int i = 0; i < NUM_THREADS; i++)
        {
            threadsIds[i] = i;
            pthread_create(&threads[i], NULL, sortBucket, &threadsIds[i]);
        }

        for (int i = 0; i < NUM_THREADS; i++) {
            pthread_join(threads[i], NULL);
        }   
        gettimeofday(&end_time, NULL);
        elapsed = (end_time.tv_sec - start_time.tv_sec) * 1000000LL +
                             (end_time.tv_usec - start_time.tv_usec);

        sorting_time += (double)elapsed / 1000000.0;



        //STEP 4 : Seek to the start of the bucket
        gettimeofday(&start_time, NULL);
        if (fseeko(file, bucket_location, SEEK_SET) != 0)
        {
            perror("Error seeking in file");
            fclose(file);
            return 1;
        }
        if (DEBUG) {
            printf("Bucket Location : %zu\n", bucket_location/(1024*1024));
        }

        //STEP 5 : Update bucket in file
        size_t bytesWritten = fwrite(buckets, 1, bucketSizeInBytes * NUM_THREADS, file);
        if (bytesWritten != bucketSizeInBytes * NUM_THREADS)
        {
            perror("Error writing to file");
            fclose(file);
            return 1;
        }

        gettimeofday(&end_time, NULL);
        elapsed = (end_time.tv_sec - start_time.tv_sec) * 1000000LL +
                             (end_time.tv_usec - start_time.tv_usec);

        writing_time += (double)elapsed / 1000000.0;

        if ( (i * NUM_THREADS ) % 16 == 0 )
            printf("Buckets sorted : %zu\n", i*NUM_THREADS);
    }

    gettimeofday(&end_all, NULL);
    elapsed = (end_all.tv_sec - start_all.tv_sec) * 1000000LL +
                            (end_all.tv_usec - start_all.tv_usec);
    total_time += (double)elapsed / 1000000.0;

    printf("Ending Sort.... \n");

    printf("Reading time: %lf seconds \n", reading_time);
    printf("Sorting time: %lf seconds \n", sorting_time);
    printf("Writing time: %lf seconds \n", writing_time);
    printf("Total time: %lf seconds \n", total_time);

    fclose(file);
    free(buckets);

    return 0;
}