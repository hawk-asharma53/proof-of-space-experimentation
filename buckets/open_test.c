#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define HASH_SIZE 9
#define PREFIX_SIZE 1

struct hashObject
{
    char byteArray[HASH_SIZE - PREFIX_SIZE];
    long int value;
};

void writeStructToFile(const char *filename, struct hashObject *obj)
{
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC , 0644);
    if (fd == -1)
    {
        perror("open");
        exit(EXIT_FAILURE);
    }

    ssize_t bytes_written = write(fd, obj, sizeof(struct hashObject));
    if (bytes_written == -1)
    {
        perror("write");
        close(fd);
        exit(EXIT_FAILURE);
    }

    printf("Struct written to file successfully.\n");
    close(fd);
}

void readStructFromFile(const char *filename, struct hashObject *obj)
{
    int fd = open(filename, O_RDONLY);
    if (fd == -1)
    {
        perror("open");
        exit(EXIT_FAILURE);
    }

    ssize_t bytes_read = read(fd, obj, sizeof(struct hashObject));
    if (bytes_read == -1)
    {
        perror("read");
        close(fd);
        exit(EXIT_FAILURE);
    }

    printf("Struct read from file:\n");
    printf("byteArray: %s\n", obj->byteArray);
    printf("value: %ld\n", obj->value);

    close(fd);
}

int main()
{
    struct hashObject obj_to_write;
    struct hashObject obj_to_read;

    // Initialize the struct with some data
    strcpy(obj_to_write.byteArray, "Hello!");
    obj_to_write.value = 42;

    const char *filename = "hashObject.dat";

    // Write the struct to a binary file
    writeStructToFile(filename, &obj_to_write);

    // Read the struct from the binary file
    readStructFromFile(filename, &obj_to_read);

    return 0;
}
