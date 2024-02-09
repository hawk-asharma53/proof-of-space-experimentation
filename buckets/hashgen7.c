#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include <math.h>
#include <stdbool.h>
#include <fcntl.h>

#include <stdlib.h>
#include <math.h>

#include <time.h>
#include <pthread.h>

#include <sys/stat.h>

//#include "common.c"
#include "blake3.h"


//bool DEBUG = false;
size_t bucketSizeInBytes;
struct writeObject *buckets;



#define HASH_SIZE 10
#define PREFIX_SIZE 2
#define HASHGEN_THREADS_BUFFER 4096
#define NONCE_SIZE 4

size_t FULL_BUCKET_SIZE;
size_t HASHES_PER_CHUNK_SORT;
size_t HASHES_PER_BUCKET;
size_t NUM_BUCKETS;
size_t BATCH_SIZE;
long long NUM_HASHES;
long long MAX_HASHES;
int SEARCH_COUNT;
size_t SORT_SIZE;
size_t MAX_HASHES_SORTABLE;
int NUM_THREADS;   
int NUM_HASHGEN_THREADS;
int NUM_WRITE_THREADS;
int QUEUE_SIZE;

bool DEBUG = false;





struct writeObject
{
    char byteArray[HASH_SIZE - PREFIX_SIZE];
    long long NONCE; //should work up to 64GB vaults, assuming 16B per hash+nonce
};

struct hashObject
{
    struct writeObject data;
    unsigned int prefix; //should be set based on NONCE_SIZE, will work up to 4 bytes
};

// Initialization function
void initializeGlobalState() 
{
	HASHES_PER_BUCKET = 1 * 1024; 


	NUM_BUCKETS = 256;

	BATCH_SIZE = 16;




	NUM_HASHES = 1024;



	//size_t HASHES_PER_BUCKET_READ = HASHES_PER_BUCKET ;
	FULL_BUCKET_SIZE = HASHES_PER_BUCKET;

	//size_t MAX_HASHES = NUM_BUCKETS * HASHES_PER_BUCKET_READ;


	MAX_HASHES = 1024;
	//long long NUM_HASHES = (long long) MAX_HASHES;

	SEARCH_COUNT = 1000;

	SORT_SIZE = 128; //In MB
	MAX_HASHES_SORTABLE = (SORT_SIZE * 1024 * 1024) / sizeof(struct writeObject);
	//const size_t HASHES_PER_CHUNK_SORT = HASHES_PER_BUCKET_READ < MAX_HASHES_SORTABLE ? HASHES_PER_BUCKET_READ : MAX_HASHES_SORTABLE; 
	HASHES_PER_CHUNK_SORT = HASHES_PER_BUCKET;
	NUM_THREADS = 1;   

	NUM_HASHGEN_THREADS = 1;
	NUM_WRITE_THREADS = 2;
	QUEUE_SIZE = 10;
}



void printMemoryInfo() {
printf("Memory Information:\n");
    FILE *meminfo = fopen("/proc/meminfo", "r");
    if (meminfo == NULL) {
        perror("Error opening /proc/meminfo");
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), meminfo) != NULL) {
        printf("%s", line);
    }

    fclose(meminfo);
}


void printArray(unsigned char byteArray[HASH_SIZE], int arraySize)
{
    printf("printArray(): ");
    for (size_t i = 0; i < arraySize; i++)
    {
        printf("%02x ", byteArray[i]); // Print each byte in hexadecimal format
    }
    printf("\n");
}

int compareWriteObjects(const void *a, const void *b)
{
	//printf("compareWriteObjects()\n");
    const struct writeObject *hashObjA = (const struct writeObject *)a;
    const struct writeObject *hashObjB = (const struct writeObject *)b;
	//printf("sizeof(hashObjA->byteArray)=%ld\n", sizeof(hashObjA->byteArray));

    // Compare the byteArray fields
    return memcmp(hashObjA->byteArray, hashObjB->byteArray, sizeof(hashObjA->byteArray));
}


struct CircularArray {
    //unsigned char array[HASHGEN_THREADS_BUFFER][HASH_SIZE];
    struct hashObject array[HASHGEN_THREADS_BUFFER];
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

void insertBatch(struct CircularArray *circularArray, struct hashObject values[BATCH_SIZE]) {
    pthread_mutex_lock(&(circularArray->mutex));

    // Wait while the circular array is full and producer is not finished
    while ((circularArray->head + BATCH_SIZE) % HASHGEN_THREADS_BUFFER == circularArray->tail && !circularArray->producerFinished) {
        pthread_cond_wait(&(circularArray->not_full), &(circularArray->mutex));
    }

    // Insert values
    for (int i = 0; i < BATCH_SIZE; i++) {
        memcpy(&circularArray->array[circularArray->head], &values[i], sizeof(struct hashObject));
        circularArray->head = (circularArray->head + 1) % HASHGEN_THREADS_BUFFER;
    }

    // Signal that the circular array is not empty
    pthread_cond_signal(&(circularArray->not_empty));

    pthread_mutex_unlock(&(circularArray->mutex));
}

void removeBatch(struct CircularArray *circularArray, struct hashObject *result) {
    pthread_mutex_lock(&(circularArray->mutex));

    // Wait while the circular array is empty and producer is not finished
    while (circularArray->tail == circularArray->head && !circularArray->producerFinished) {
        pthread_cond_wait(&(circularArray->not_empty), &(circularArray->mutex));
    }

    // Remove values
    for (int i = 0; i < BATCH_SIZE; i++) {
        memcpy(&result[i], &circularArray->array[circularArray->tail], sizeof(struct hashObject));
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
void generateBlake3(unsigned char result[HASH_SIZE], unsigned int NONCE) {
    blake3_hasher hasher;
    blake3_hasher_init(&hasher);
    blake3_hasher_update(&hasher, &NONCE, sizeof(NONCE));
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


// Function to be executed by each thread for array generation
void *arrayGenerationThread(void *arg) {
    struct ThreadData *data = (struct ThreadData *)arg;
	int hashObjectSize = sizeof(struct hashObject);
    //unsigned char batch[BATCH_SIZE][HASH_SIZE];
    struct hashObject batch[BATCH_SIZE];
    long long NUM_HASHES_PER_THREAD = (long long)(NUM_HASHES / NUM_HASHGEN_THREADS);
	unsigned char hash[HASH_SIZE];
	long long hashIndex = 0;
	long long i = 0;
    while(data->circularArray->producerFinished == 0)
    {
    //for (unsigned int i = 0; i < NUM_HASHES_PER_THREAD; i += BATCH_SIZE) {
        for (long long j = 0; j < BATCH_SIZE; j++) {
            hashIndex = (long long)(NUM_HASHES_PER_THREAD * data->threadID + i + j);
            generateBlake3(hash, hashIndex);
            batch[j].prefix = byteArrayToInt(hash,0);
            memcpy(batch[j].data.byteArray, hash+PREFIX_SIZE, HASH_SIZE-PREFIX_SIZE);
            batch[j].data.NONCE = hashIndex;
        }
		//should add hashIndex as NONCE to hashObject
        insertBatch(data->circularArray, batch);
        i += BATCH_SIZE;
    }

	if (DEBUG)	
		printf("finished generating hashes on thread id %d, thread exiting...\n",data->threadID);
    return NULL;
}


// Function to sort a bucket
void *sortBucket(void *arg)
{
    int thread_id = *(int *)arg;
	if (DEBUG)
    printf("Thread id - %d\n", thread_id);

    struct writeObject *bucket = &buckets[thread_id * HASHES_PER_CHUNK_SORT];

	if (DEBUG)
    printf("Thread id - %d, Starting Sort\n", thread_id);
    qsort(bucket, HASHES_PER_CHUNK_SORT, sizeof(struct writeObject), compareWriteObjects);
	if (DEBUG)
    printf("Thread id - %d, Sorting Done\n", thread_id);
    
    return NULL;
}



long getFileSize(const char *filename) {
    struct stat st;

    if (stat(filename, &st) == 0) {
        return st.st_size;
    } else {
        perror("Error getting file size");
        return -1; // Return -1 to indicate an error
    }
}

int psort(char *FILENAME, int num_buckets, int num_threads_sort) {

	NUM_THREADS = num_threads_sort;
	printf("NUM_THREADS : %d\n", NUM_THREADS);

    printf("FILENAME : %s\n", FILENAME);
    
	long filesize = getFileSize(FILENAME);

    if (filesize != -1) {
        printf("File size of %s: %ld bytes\n", FILENAME, filesize);
    } else {
        printf("Failed to get file size.\n");
    }    
    
    printf("num_buckets : %d\n", num_buckets);
    int bucket_size_sort = filesize/num_buckets;
    printf("bucket_size_sort : %d\n", bucket_size_sort);
    int hash_size_sort = sizeof(struct writeObject);
    printf("hash_size_sort : %d\n", hash_size_sort);
    int num_hashes_per_bucket_sort = bucket_size_sort/hash_size_sort;
    printf("num_hashes_per_bucket_sort : %d\n", num_hashes_per_bucket_sort);
    
    

	const size_t NUM_CHUNKS_SORT = num_buckets; 
    //const size_t NUM_CHUNKS_SORT = (HASHES_PER_BUCKET_READ / HASHES_PER_CHUNK_SORT) * NUM_BUCKETS;

    //printf("HASHES_PER_BUCKET_READ : %zu\n", HASHES_PER_BUCKET_READ);
    printf("NUM_BUCKETS : %zu\n", NUM_BUCKETS);
    HASHES_PER_CHUNK_SORT = num_hashes_per_bucket_sort;
    printf("HASHES_PER_CHUNK_SORT : %zu\n", HASHES_PER_CHUNK_SORT);
    printf("NUM_CHUNKS_SORT : %zu\n", NUM_CHUNKS_SORT);
    
    if (HASHES_PER_BUCKET != bucket_size_sort && NUM_BUCKETS != num_buckets)
    {
    	printf("HASHES_PER_BUCKET and NUM_BUCKETS not set properly...\n");
    	return 1;
    }
    else
    {

    bucketSizeInBytes = HASHES_PER_CHUNK_SORT * sizeof(struct writeObject);
    buckets = (struct writeObject *) malloc( bucketSizeInBytes * NUM_THREADS );
    
    //return 0;
    
    

    printf("Malloc Size : %zu\n", bucketSizeInBytes * NUM_THREADS );

    printf("fopen()...\n");

    //FILE *file = fopen("plot.memo", "rb+"); // Open for appending
    FILE *file = fopen(FILENAME, "rb+"); // Open for appending
    if (file == NULL)
    {
        perror("Error opening file");
        return 1;
    }

	if (DEBUG)
    printf("Starting Sort.... \n");

    double reading_time, sorting_time, writing_time, total_time;
    struct timeval start_time, end_time, start_all, end_all;
    long long elapsed;
    long long last_progress_update = 0.0;
	int EXPECTED_BUCKETS_SORTED = NUM_CHUNKS_SORT;
    gettimeofday(&start_all, NULL);
    for (size_t i = 0; i < (NUM_CHUNKS_SORT / NUM_THREADS); i++)
    {
	if (DEBUG)
    	printf("for loop to process buckets %ld up to %ld\n",i,(NUM_CHUNKS_SORT / NUM_THREADS));

        gettimeofday(&start_time, NULL);
        //STEP 1 : Seek to location
        size_t bucket_location = i * bucketSizeInBytes * NUM_THREADS;
	if (DEBUG)
    	printf("bucket_location= %ld\n",bucket_location);
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
	if (DEBUG)
    	printf("will fread() %ld bytes\n",bucketSizeInBytes * NUM_THREADS);
        size_t bytesRead = fread(buckets, 1, bucketSizeInBytes * NUM_THREADS, file);
	if (DEBUG)
        printf("finished fread() with %ld bytes read\n",bytesRead);
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
	if (DEBUG)
        printf("allocate threads...\n");

        pthread_t threads[NUM_THREADS];
        int threadsIds[NUM_THREADS];

	if (DEBUG)
        printf("creating threads...\n");
        // Create threads to sort each bucket in parallel
        for (int i = 0; i < NUM_THREADS; i++)
        {
            threadsIds[i] = i;
            pthread_create(&threads[i], NULL, sortBucket, &threadsIds[i]);
        }
	if (DEBUG)
		printf("waiting for threads to finish...\n");
        for (int i = 0; i < NUM_THREADS; i++) {
            pthread_join(threads[i], NULL);
        }   
        gettimeofday(&end_time, NULL);
        elapsed = (end_time.tv_sec - start_time.tv_sec) * 1000000LL +
                             (end_time.tv_usec - start_time.tv_usec);

        sorting_time += (double)elapsed / 1000000.0;

	if (DEBUG)
		printf("finished sorting bucket...\n");

	if (DEBUG)
		printf("Seek to the start of the bucket %ld...\n",bucket_location);

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

	if (DEBUG)
		printf("Update bucket in file with %ld bytes...\n",bucketSizeInBytes * NUM_THREADS);
        //STEP 5 : Update bucket in file
        size_t bytesWritten = fwrite(buckets, 1, bucketSizeInBytes * NUM_THREADS, file);
	if (DEBUG)
		printf("write finished with %ld bytes written...\n",bytesWritten);
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
gettimeofday(&end_all, NULL);
    elapsed = (double)((end_all.tv_sec - start_all.tv_sec) * 1000000LL +
                            (end_all.tv_usec - start_all.tv_usec))/ 1000000.0;


        if (elapsed > last_progress_update + 1.0) {
        	last_progress_update = elapsed;


        //if ( i % 1 == 0 )
	//{
	     	int totalBucketsSorted = i*NUM_THREADS; 
		float perc_done = totalBucketsSorted*100.0/EXPECTED_BUCKETS_SORTED;
              float eta = elapsed / (perc_done/100.0) - elapsed;
                float diskSize = NUM_CHUNKS_SORT * bucketSizeInBytes / (1024 * 1024);
	    float throughput_MB = diskSize*perc_done/100.0/elapsed;
		printf("Buckets sorted : %zu in %lld sec %.2f%% ETA %lf sec => %.2f MB/sec\n", i*NUM_THREADS, elapsed, perc_done, eta, throughput_MB);
		//printf("Buckets sorted : %zu\n", i*NUM_THREADS);
	}
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
    
}





void printUsage() {
    printf("Usage: ./your_program -t <num_threads_hash> -o <num_threads_sort> -f <filename> -s <filesize_GB>\n");
}

void printHelp() {
    printf("Help:\n");
    printf("  -t <num_threads_hash>: Specify the number of threads to generate hashes\n");
    printf("  -o <num_threads_sort>: Specify the number of threads to sort hashes\n");
    printf("  -f <filename>: Specify the filename\n");
    printf("  -s <filesize>: Specify the filesize as an integer\n");
    printf("  -h, --help: Display this help message\n");
}


int main(int argc, char *argv[]) {

initializeGlobalState();

char *FILENAME = NULL; // Default value
long long FILESIZE = 0; // Default value
int num_threads_sort = 0;
long long memory_size = 0; //in GB


   int opt;
    while ((opt = getopt(argc, argv, "t:o:m:f:s:h")) != -1) {
        switch (opt) {
            case 't':
                NUM_THREADS = atoi(optarg);
                printf("NUM_THREADS=%d\n", NUM_THREADS);
                break;
            case 'o':
                num_threads_sort = atoi(optarg);
                printf("num_threads_sort=%d\n", num_threads_sort);
                break;
            case 'm':
                memory_size = atoi(optarg);
                printf("memory_size=%lld GB\n", memory_size);
                break;
            case 'f':
                FILENAME = optarg;
                printf("FILENAME=%s\n", FILENAME);
                break;
            case 's':
                FILESIZE = atoi(optarg);
                printf("FILESIZE=%lld GB\n", FILESIZE);
                //if (FILESIZE > 64 || FILESIZE < 1)
                //{
                //	printf("FILESIZE does not have a supported value, it must be a value between 1 and 64, exiting...\n");
                //	return 1;
                //}
                	
                
                
                break;
            case 'h':
                printHelp();
                return 0;
            default:
                printUsage();
                return 1;
        }
    } 
    
    if (NUM_THREADS <= 0 || num_threads_sort <=0 || FILENAME == NULL || FILESIZE <= 0 || memory_size <= 0) {
        printf("Error: Number of threads (-t), filename (-f), and filesize (-s) are mandatory.\n");
        printUsage();
        return 1;
    }
    
    
                NUM_HASHGEN_THREADS = NUM_THREADS;
                printf("NUM_HASHGEN_THREADS=%d\n", NUM_HASHGEN_THREADS);
                BATCH_SIZE = HASHGEN_THREADS_BUFFER/NUM_HASHGEN_THREADS;
                printf("BATCH_SIZE=%ld\n", BATCH_SIZE);

                printf("PREFIX_SIZE=%d\n", PREFIX_SIZE);
                NUM_BUCKETS = 1 << (PREFIX_SIZE * 8);
                printf("NUM_BUCKETS=%zu\n", NUM_BUCKETS);
                
                	
                NUM_HASHES = (long long)(FILESIZE*1024*1024*1024/sizeof(struct writeObject));
                printf("NUM_HASHES=%lld\n", NUM_HASHES);
                //NUM_HASHES = 4*1024*1024*1024;
                MAX_HASHES = (long long)NUM_HASHES;
                printf("MAX_HASHES=%lld\n", MAX_HASHES);
                //HASHES_PER_BUCKET_READ = MAX_HASHES/NUM_BUCKETS;
                //printf("HASHES_PER_BUCKET_READ=%zu\n", HASHES_PER_BUCKET_READ);
                FULL_BUCKET_SIZE = MAX_HASHES/NUM_BUCKETS;
                printf("FULL_BUCKET_SIZE=%zu\n", FULL_BUCKET_SIZE);
                
                HASHES_PER_BUCKET = (((memory_size)*1024*1024*1024/NUM_BUCKETS)/sizeof(struct writeObject))/2;
                printf("HASHES_PER_BUCKET=%zu\n", HASHES_PER_BUCKET);
                
                printf("WRITE_PER_FLUSH=%zu\n", HASHES_PER_BUCKET*sizeof(struct writeObject));
                
                
    
    
       
    srand((unsigned int)time(NULL)); 

	if (DEBUG)
	printMemoryInfo();

    long long NONCE = 0;
    
    
    //printf("HASHES_PER_BUCKET_READ=%zu\n", HASHES_PER_BUCKET_READ);

    long int EXPECTED_TOTAL_FLUSHES = ((long int)(NUM_HASHES))/HASHES_PER_BUCKET;
    printf("EXPECTED_TOTAL_FLUSHES=%zu\n", EXPECTED_TOTAL_FLUSHES);
    int EXPECTED_FLUSHES_PER_BUCKET = ((long int)(NUM_HASHES))/HASHES_PER_BUCKET/NUM_BUCKETS;
    printf("EXPECTED_FLUSHES_PER_BUCKET=%d\n", EXPECTED_FLUSHES_PER_BUCKET);
    //long long MAX_HASHES = NUM_BUCKETS * HASHES_PER_BUCKET_READ;
    printf("MAX_HASHES=%llu\n", MAX_HASHES);
    
    
    

    float memSize = 1.0 * NUM_BUCKETS * HASHES_PER_BUCKET * sizeof(struct hashObject) / (1024.0 * 1024 * 1024);
    printf("total memory needed (GB): %f\n", memSize);
    float diskSize = 1.0 * MAX_HASHES * sizeof(struct writeObject) / (1024.0 * 1024 * 1024);
    printf("total disk needed (GB): %f\n", diskSize);

    printf("fseeko()...\n");
    // Seek past the end of the file
    //long long desiredFileSize = FULL_BUCKET_SIZE * NUM_BUCKETS * sizeof(struct writeObject);
    long long desiredFileSize = MAX_HASHES * sizeof(struct writeObject);
    printf("hashgen: setting size of file to %lld\n", desiredFileSize);
    
    if (DEBUG)
    {
    	printf("exiting...\n");
    	return 0;
    }
    
    //off_t desiredFileSize = 1024 * 1024;  // Set the desired file size in bytes

    // Open the file (or create it if it doesn't exist)
    int fileDescriptor = open(FILENAME, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fileDescriptor == -1) {
        perror("Error opening file");
        return 1;
    }

    // Set the file size using truncate
    if (ftruncate(fileDescriptor, desiredFileSize) == -1) {
        perror("Error truncating file");
        close(fileDescriptor);
        return 1;
    }

    // Close the file
    close(fileDescriptor);

    printf("File size set to %lld bytes\n", (long long)desiredFileSize);
    
	long filesize = getFileSize(FILENAME);

    if (filesize != -1) {
        printf("File size of %s: %ld bytes\n", FILENAME, filesize);
    } else {
        printf("Failed to get file size.\n");
    }    
    
    

    printf("fopen()...\n");
    FILE *file = fopen(FILENAME, "wb"); // Open for appending
    if (file == NULL)
    {
        printf("Error opening file %s",FILENAME);
        return 1;
    }

    
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
        array2D[i] = (struct hashObject *)malloc(HASHES_PER_BUCKET * sizeof(struct hashObject));
        if (array2D[i] == NULL)
        {
            perror("Memory allocation failed");
            return 1;
        }
    }

    printf("initializing misc variables...\n");
    size_t startByteIndex = PREFIX_SIZE; 
    size_t bytes_to_write = HASHES_PER_BUCKET * sizeof(struct writeObject);
    unsigned char randomArray[HASH_SIZE];
    int bIndex;
    size_t totalFlushes = 0;
    double write_time = 0;

    long search_items = 0;

    printf("initializing circular array...\n");
    // Initialize the circular array
    struct CircularArray circularArray;
    initCircularArray(&circularArray);

    printf("Create thread data structure...\n");
    // Create thread data structure
    struct ThreadData threadData[NUM_HASHGEN_THREADS];

    printf("Create hash generation threads...\n");
    // Create threads
    pthread_t threads[NUM_HASHGEN_THREADS];
    for (int i = 0; i < NUM_HASHGEN_THREADS; i++) {
        threadData[i].circularArray = &circularArray;
        threadData[i].threadID = i;
        pthread_create(&threads[i], NULL, arrayGenerationThread, &threadData[i]);
    }

    // Measure time taken for hash generation using gettimeofday
    struct timeval start_time, end_time, last_progress_time;
    gettimeofday(&start_time, NULL);
    gettimeofday(&last_progress_time, NULL);
    long long last_progress_i = 0;
    double elapsed_time = 0.0;

	double last_progress_update = 0.0;
    printf("starting main writer thread...\n");
    // Main thread: Consume elements from the circular array
    for (long long i = 0; i < NUM_HASHES; i += BATCH_SIZE) 
    //unsigned int i = 0;
    //while(i < NUM_HASHES)
    //long long i = 0;
    //int i=0;
    //while(totalFlushes < EXPECTED_TOTAL_FLUSHES)
    {
        if (DEBUG)
    	printf("while()...%ld\n",totalFlushes);
        // Consume BATCH_SIZE array elements
        //unsigned char consumedArray[BATCH_SIZE][HASH_SIZE];
        struct hashObject consumedArray[BATCH_SIZE];
        
        if (DEBUG)
        printf("removeBatch()...\n");
        removeBatch(&circularArray, consumedArray);

        if (DEBUG)
        printf("processing batch of size %ld...\n",BATCH_SIZE);
		for (int b = 0; b<BATCH_SIZE; b++)
		{
        // Extract the first 3 bytes starting from index 0 and convert to an integer
        if (DEBUG)
            printf("byteArrayToInt()...\n");
        //unsigned int prefix = byteArrayToInt(consumedArray[b], 0);
        
        //crashes with a core-dump, probably consumedArray is not valid
        
        unsigned int prefix = consumedArray[b].prefix;
        bIndex = bucketIndex[prefix];
        // Use memcpy to copy the contents of randomArray to copyArray
        if (DEBUG)
            printf("memcpy()...%ld %d\n",totalFlushes,b);
        memcpy(&array2D[prefix][bIndex], &consumedArray[b], sizeof(consumedArray[b]));
        // bucketIndex[prefix]++;
        // array2D[prefix][bucketIndex[prefix]++]->byteArray;
        if (DEBUG)
            printf("setting array2D and bucketIndex...\n");
        array2D[prefix][bIndex].data.NONCE = NONCE;
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
            long write_location = prefix * (FULL_BUCKET_SIZE * sizeof(struct writeObject)) + bucketFlush[prefix] * bytes_to_write;
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

			//sizeof(struct writeObject)
            size_t bytesWritten = fwrite(&array2D[prefix]->data, 1, bytes_to_write, file);

            if (bytesWritten != bytes_to_write)
            {
                perror("Error writing to file");
                printf("fwrite()... %lu %i %lu %i %zu\n", write_location, prefix, bytes_to_write, bucketFlush[prefix], bytesWritten);
                fclose(file);
                return 1;
            }

            if (DEBUG)
                printf("updating bucketFlush and bucketIndex...\n");

			//printf("bucketFlush[prefix]=%d %d\n",bucketFlush[prefix],prefix);
            totalFlushes++;
            bucketFlush[prefix]++;
            bucketIndex[prefix] = 0;
            
        }

    	}

            gettimeofday(&end_time, NULL);
            elapsed_time = (end_time.tv_sec - start_time.tv_sec) +
                                  (end_time.tv_usec - start_time.tv_usec) / 1.0e6;


        if (elapsed_time > last_progress_update + 1.0) {
        	double elapsed_time_since_last_progress_update = (end_time.tv_sec - last_progress_time.tv_sec) +
                                  (end_time.tv_usec - last_progress_time.tv_usec) / 1.0e6;
        	last_progress_update = elapsed_time;
        	
        	
            // Calculate and print estimated completion time
            double progress = i * 100.0 / NUM_HASHES;
            double remaining_time = elapsed_time / (progress / 100.0) - elapsed_time;
            
            
            printf("[%.2lf]: %.2lf%% completed, ETA %.2lf seconds, %zu/%zu flushes, %f MB/sec - %f MB/sec\n", elapsed_time, progress, remaining_time,totalFlushes,EXPECTED_TOTAL_FLUSHES, (i / elapsed_time)* (8.0 + 8.0) / (1024 * 1024), ((i-last_progress_i) / elapsed_time_since_last_progress_update)* (8.0 + 8.0) / (1024 * 1024));
            last_progress_i = i;
            gettimeofday(&last_progress_time, NULL);
        }
        
        //if(i<NUM_HASHES-BATCH_SIZE)
	    //	i += BATCH_SIZE;
	    //else
	    	
	    	
	    	

    }


    printf("Completed generating hashes with %zu flushes so far...\n",totalFlushes);
    printf("Flushing remaining %ld buckets to disk...\n",EXPECTED_TOTAL_FLUSHES - totalFlushes);
    


	
    for (size_t i = 0; i < NUM_BUCKETS; i++)
    {
        if ( bucketFlush[i] < EXPECTED_FLUSHES_PER_BUCKET ) 
        {
        	if (DEBUG)
        		printf("Flushing final bucket : %ld %d %d\n", i,bucketFlush[i],EXPECTED_FLUSHES_PER_BUCKET);
            clock_t start_write = clock();

            long write_location = i * (FULL_BUCKET_SIZE * sizeof(struct writeObject)) + bucketFlush[i] * bytes_to_write;
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
        }
    }
    
    

    printf("finished workload...\n");

    printf("Total flushes : %zu\n", totalFlushes);

    // its possible not all buckets have been written to disk, should check, and keep generating hashes until all buckets are written to disk

    printf("closing file...\n");
    // Close the file when done
    fclose(file);
    
	filesize = getFileSize(FILENAME);

    if (filesize != -1) {
        printf("File size of %s: %ld bytes\n", FILENAME, filesize);
    } else {
        printf("Failed to get file size.\n");
    }    
    

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



	printf("setting circular array producer finished sentinel...\n");
    // Set the producerFinished flag to true
    circularArray.producerFinished = 1;

	printf("broadcasting sentinel...\n");
    // Signal that the circular array is not empty
    //pthread_cond_broadcast(&(circularArray.not_empty));
    pthread_cond_broadcast(&(circularArray.not_full));

	printf("Wait for hash generating threads to finish...\n");
    // Wait for threads to finish
    for (int i = 0; i < NUM_HASHGEN_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

	printf("destroy circular array...\n");
    destroyCircularArray(&circularArray);


    //if (NUM_BUCKETS == 256)
    //{
    if (DEBUG)
    printMemoryInfo();
    printf("starting sorting...\n");
    psort(FILENAME, NUM_BUCKETS, num_threads_sort);
    printf("finished sorting...\n");
    //}
    //else
    //{
    //	printf("buckets are small enough, no need to sort...\n");
//}

    
    // Measure elapsed time
    gettimeofday(&end_time, NULL);
    elapsed_time = (end_time.tv_sec - start_time.tv_sec) +
                          (end_time.tv_usec - start_time.tv_usec) / 1.0e6;

    // Calculate throughput
    double throughput = NUM_HASHES / elapsed_time;

    // Print timing information
    printf("Elapsed Time: %lf seconds\n", elapsed_time);
    printf("Throughput: %lf hashes per second\n", throughput);
    printf("Throughput: %f MB second\n", throughput * (8.0 + 8.0) / (1024 * 1024));

    return 0;
}
