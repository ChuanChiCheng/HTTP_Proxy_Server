#ifndef SAFEQUEUE_H
#define SAFEQUEUE_H

#include <pthread.h>
#include "http.h"

typedef struct {
    int client_fd;
    int priority; 
    struct http_request request;
} QueueElement;

typedef struct SafeQueue {
    QueueElement *heap;     // Dynamic array to store QueueElements
    int capacity;           // Current capacity of the heap
    int size;               // Current size of the heap
    pthread_mutex_t lock;   // Mutex for synchronization
    pthread_cond_t cond;    // Condition variable for blocking
} SafeQueue;

SafeQueue* create_queue(int capacity);
int add_work(SafeQueue *queue, QueueElement element);
QueueElement get_work(SafeQueue *queue);
QueueElement get_work_nonblocking(SafeQueue *queue);

#endif