#include "common.c"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>

typedef struct Node {
    size_t write_index;
    struct hashObject *array;
} Node;

typedef struct {
    Node *arr;
    int front, rear;
    int capacity;
    pthread_mutex_t mutex;
    int size;
} CircularQueue;

CircularQueue write_queue;

void initializeQueue(CircularQueue *queue, int size) {
    // queue = (CircularQueue *) malloc( sizeof(CircularQueue) );
    queue->capacity = size;
    queue->front = queue->rear = -1;
    queue->arr = (Node *) malloc( size * sizeof(Node) );
    queue->size = 0;
    for (int i = 0; i < size; i++)
    {
        queue->arr[i].array = (struct hashObject *) malloc( HASHES_PER_BUCKET * sizeof(struct hashObject) );
    }
    pthread_mutex_init(&(queue->mutex), NULL);
}

// Function to check if the circular queue is empty
int isEmpty(CircularQueue *queue) {
    // printf("isEmpty\n");
    return queue->front == -1;
}

// Function to check if the circular queue is full
int isFull(CircularQueue *queue) {
    return (queue->rear + 1) % queue->capacity == queue->front;
}

void enqueue(CircularQueue *queue, size_t write_index, struct hashObject *array) {
    // printf("enqueue, location - %ld\n", write_index);
    pthread_mutex_lock(&queue->mutex);
    while (isFull(queue)) {
        // printf("Queue is full. Cannot enqueue %d\n", value);
        // return;
    }

    // printf("after lock, enqueue, location - %ld\n", write_index);
    if (isEmpty(queue)) {
        queue->front = queue->rear = 0;
    } else {
        queue->rear = (queue->rear + 1) % queue->capacity;
    }

    queue->arr[queue->rear].write_index = write_index;
    memcpy(array, queue->arr[queue->rear].array, HASHES_PER_BUCKET * sizeof(struct hashObject));
    queue->size++;
    printf("Queue Size - %d, Front - %d, Rear - %d\n", queue->size, queue->front, queue->rear);
    pthread_mutex_unlock(&queue->mutex);
}

Node* dequeue(CircularQueue *queue) {
    pthread_mutex_lock(&queue->mutex);

    if (isEmpty(queue)) {
        // printf("Queue is empty. Cannot dequeue\n");
        pthread_mutex_unlock(&queue->mutex);
        return NULL;
    }
    // printf("dequeue 1\n");
    
    Node *node = &(queue->arr[queue->front]);

    if (queue->front == queue->rear) {
        queue->front = queue->rear = -1;
    } else {
        queue->front = (queue->front + 1) % queue->capacity;
    }

    // printf("Dequeued %d\n", value);
    queue->size--;
    printf("Queue Size - %d, Front - %d, Rear - %d\n", queue->size, queue->front, queue->rear);
    // printf("Queue Size - %d\n", queue->size);

    pthread_mutex_unlock(&queue->mutex);
    // printf("Dequeued\n");
    return node;
}