#include "common.c"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

typedef struct Node {
    size_t write_index;
    struct hashObject *array;
    struct Node* next;
} Node;

typedef struct {
    Node* front;
    Node* rear;
    pthread_mutex_t mutex;
    int count;
} Queue;

Queue* write_queue;

void initializeQueue() {
    write_queue = (Queue *) malloc( sizeof(Queue) );
    write_queue->front = NULL;
    write_queue->rear = NULL;
    write_queue->count = 0;
    pthread_mutex_init(&write_queue->mutex, NULL);
}

void enqueue(size_t write_index, struct hashObject *array) {
    Node* newNode = (Node*)malloc(sizeof(Node));
    newNode->array = (struct hashObject *)malloc(HASHES_PER_BUCKET * sizeof(struct hashObject));
    if (!newNode) {
        perror("Error in malloc");
        exit(EXIT_FAILURE);
    }
    newNode->write_index = write_index;
    memcpy(array, newNode->array, HASHES_PER_BUCKET * sizeof(struct hashObject));

    pthread_mutex_lock(&write_queue->mutex);

    if (write_queue->rear == NULL) {
        // If the queue is empty
        write_queue->front = newNode;
        write_queue->rear = newNode;
    } else {
        // If the queue is not empty
        write_queue->rear->next = newNode;
        write_queue->rear = newNode;
    }
    write_queue->count++;
    // printf("Added to write queue - %d\n", write_queue->count);
    pthread_mutex_unlock(&write_queue->mutex);
}

Node* dequeue() {
    pthread_mutex_lock(&write_queue->mutex);

    if (write_queue->front == NULL) {
        // If the queue is empty
        pthread_mutex_unlock(&write_queue->mutex);
        return NULL; // Signify that the queue is empty
    }

    Node* temp = write_queue->front;

    if (write_queue->front == write_queue->rear) {
        // If there's only one element in the queue
        write_queue->front = NULL;
        write_queue->rear = NULL;
    } else {
        // If there are multiple elements in the queue
        write_queue->front = temp->next;
    }

    write_queue->count--;
    // printf("Removed from queue - %d\n", write_queue->count);
    pthread_mutex_unlock(&write_queue->mutex);

    return temp;
}