#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>
#include <sys/types.h>
#include <math.h>
#include <sys/time.h> // for gettimeofday function

#define HASH_SIZE 8        // 11
const int PREFIX_SIZE = 3; // 3
unsigned int MAX_PREFIX = 0;
bool DEBUG = false;

const off_t MB = 1024 * 1024;
const off_t num_buckets = 256 * 256 * 256;
off_t bucket_size = 64;

// Define the struct
struct hashObject
{
    char byteArray[HASH_SIZE];
    long int value;
};

// Function to generate a random 8-byte array
unsigned int generateRandomByteArray(unsigned char result[HASH_SIZE])
{
    for (int i = 0; i < HASH_SIZE - 1; i++)
    {
        result[i] = rand() % 256; // Generate a random byte (0-255)
    }
    result[HASH_SIZE - 1] = '\n';

    return (unsigned int)(rand() % num_buckets);
}

void printArray(unsigned char byteArray[HASH_SIZE], int arraySize)
{
    printf("printArray(): ");
    for (int i = 0; i < arraySize; i++)
    {
        printf("%02x ", byteArray[i]); // Print each byte in hexadecimal format
    }
    printf("\n"); // Add a newline character at the end
}

// Function to convert a 12-byte array to an integer
/*unsigned int byteArrayToInt(unsigned char byteArray[HASH_SIZE], int startIndex) {
    unsigned int result = 0;
    if (DEBUG)
        printArray(byteArray, HASH_SIZE);
    for (int i = 0; i < PREFIX_SIZE; i++) {
        result = (result << 8) | byteArray[startIndex + i];
    }
    return result;
}*/

char getRandomChar()
{
    const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    const int charsetSize = sizeof(charset) - 1;
    return charset[rand() % charsetSize];
}

// Function to generate a random alphanumeric string of a given length
char *generateRandomString(int length)
{
    char *randomString = (char *)malloc((length + 1) * sizeof(char)); // +1 for the null-terminator

    if (randomString == NULL)
    {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1); // Exit with an error code
    }

    for (int i = 0; i < length; i++)
    {
        randomString[i] = getRandomChar();
    }

    randomString[length] = '\0'; // Null-terminate the string

    return randomString;
}

int main(int argc, char *argv[])
{

    printf("hashgen v0.1\n");

    // Check if there are at least two command-line arguments (including the program name)
    if (argc < 3)
    {
        printf("Usage: %s <path> <hashesPerBucket>\n", argv[0]);
        return 1; // Exit with an error code
    }

    // Convert the first argument to an integer
    bucket_size = atoi(argv[2]);

    struct timeval tv;
    gettimeofday(&tv, NULL);
    // Seed the random number generator with the current time in microseconds
    srand((unsigned int)(tv.tv_sec * 1000000 + tv.tv_usec));

    // char plot_name[100] = "plot-";
    // char plot_ext[] = ".memo";
    int length = 10; // Length of the random string

    char *file_path = argv[1];
    char *plot_name = "/plot-";
    char *plot_ext = ".memo";
    char *randomString = generateRandomString(length);
    char *full_file_name = (char *)malloc(strlen(file_path) + strlen(plot_name) + strlen(plot_ext) + strlen(randomString) + 1);
    strcpy(full_file_name, file_path);
    strcat(full_file_name, plot_name);
    strcat(full_file_name, randomString);
    strcat(full_file_name, plot_ext);
    printf("%s %s %s %s %s\n", file_path, plot_name, randomString, plot_ext, full_file_name);

    // Record the start time
    clock_t start_time = clock();
    MAX_PREFIX = pow(2, PREFIX_SIZE * 8);

    long int NONCE = 0;
    int size_NONCE = sizeof(NONCE);

    // const char* text = "01234567 01234567\n";
    // const int text_len = strlen(text);
    // struct hashObject myHashObject;
    struct hashObject *myHashObject;
    myHashObject = (struct hashObject *)malloc(sizeof(struct hashObject));
    if (myHashObject == NULL)
    {
        perror("malloc");
        return 1;
    }

    size_t hashObject_size = sizeof(struct hashObject);
    const off_t file_size_MB = num_buckets * bucket_size * hashObject_size / MB;
    // int hashObject_size = 16;
    memset(myHashObject->byteArray, 0, sizeof(myHashObject->byteArray));
    myHashObject->value = 0;

    printf("PREFIX_SIZE: %d bytes\n", HASH_SIZE);
    printf("PREFIX_SIZE: %d bytes\n", PREFIX_SIZE);
    printf("NONCE_SIZE: %d bytes\n", size_NONCE);
    printf("Size of hashObject struct: %zu bytes\n", hashObject_size);
    printf("Number of buckets: %lld\n", num_buckets);
    printf("Bucket size: %lld bytes\n", bucket_size * hashObject_size);
    printf("Memory required for bucket offsets: %lld MB\n", num_buckets * sizeof(short) / MB);
    printf("Hashes to generate: %lld\n", num_buckets * bucket_size);
    printf("Plot name: %s\n", full_file_name);
    printf("Size of plot to generate: %lld MB\n", file_size_MB);

    short *bucket_offsets;
    bucket_offsets = (short *)malloc(num_buckets * sizeof(short));
    if (bucket_offsets == NULL)
    {
        perror("malloc");
        return 1;
    }

    for (int i = 0; i < num_buckets; i++)
    {
        bucket_offsets[i] = 0; // Example: Initialize elements with values
    }

    // Create and open the output file
    // const char* filename = "output.txt";
    int fd = open(full_file_name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd == -1)
    {
        perror("open");
        exit(1);
    }

    // Set the file size using ftruncate
    if (ftruncate(fd, file_size_MB * MB) == -1)
    {
        perror("ftruncate");
        close(fd);
        exit(1);
    }

    // Memory map the file
    char *file_data = mmap(NULL, file_size_MB * MB, PROT_WRITE, MAP_SHARED, fd, 0);
    if (file_data == MAP_FAILED)
    {
        perror("mmap");
        close(fd);
        exit(1);
    }

    unsigned char randomArray[HASH_SIZE];

    unsigned int cur_index = 0;
    off_t cur_offset = 0;

    // memset(myHashObject.byteArray, 0, sizeof(myHashObject.byteArray));
    // myHashObject.value = 0;

    // printf("Size of 'hashObject' struct: %zu bytes\n", sizeof(hashObject));
    long int hashesToGenerate = num_buckets * bucket_size;
    long int bucketFull = 0;
    // Write the text repeatedly to fill the file
    for (long int i = 0; i < hashesToGenerate; i++)
    {
        // for (off_t offset = 0; offset < file_size - (HASH_SIZE); offset += (HASH_SIZE)) {
        //  Access and manipulate the elements
        cur_index = generateRandomByteArray(myHashObject->byteArray);
        if (bucket_offsets[cur_index] < bucket_size)
        {
            cur_offset = cur_index * hashObject_size * bucket_size + bucket_offsets[cur_index] * hashObject_size;
            myHashObject->value = NONCE;
            // new_offset = byteArrayToInt(myHashObject->byteArray, 0)*(hashObject_size);
            // new_offset = byteArrayToInt(randomArray, 0)*(HASH_SIZE);
            // printf("offset %lld file_size %lld\n",(long long)new_offset,(long long)file_size);
            memcpy(file_data + cur_offset, myHashObject, hashObject_size);
            // memcpy(file_data + new_offset+HASH_SIZE, NONCE, size_NONCE);
            //  Extract the first 3 bytes starting from index 0 and convert to an integer
            // if (DEBUG)
            //     printf("byteArrayToInt()...\n");
            // unsigned int prefix = byteArrayToInt(randomArray, 0);
            // bIndex = bucketIndex[prefix];
            NONCE++;
            bucket_offsets[cur_index]++;
        }
        else
        {
            bucketFull++;
        }
    }

    // Use msync to flush changes to disk
    if (msync(file_data, file_size_MB * MB, MS_SYNC) == -1)
    {
        perror("msync");
    }

    // Unmap the file and close it
    if (munmap(file_data, file_size_MB * MB) == -1)
    {
        perror("munmap");
    }
    close(fd);

    // long int numHashesWritten = 0;
    // for (int i = 0; i<num_buckets; i++)
    //{
    //	numHashesWritten += (long int)(bucket_size - bucket_offsets[i]);
    // }
    printf("Percent of hashes written (some were skipped due to full buckets): %0.2f\n", (hashesToGenerate - bucketFull) * 1.0 / hashesToGenerate);

    // Record the end time
    clock_t end_time = clock();

    // Calculate and print the total time taken
    double total_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    printf("Total time taken: %.2f seconds with %.2fMB/s\n", total_time, (file_size_MB * 1.0) / total_time);

    return 0;
}
