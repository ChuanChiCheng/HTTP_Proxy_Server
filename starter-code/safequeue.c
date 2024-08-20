#include <stdlib.h>
#include <stdio.h>

#include "safequeue.h"

SafeQueue* create_queue(int initial_capacity) {
    SafeQueue *queue = (SafeQueue *)malloc(sizeof(SafeQueue));
    queue->heap = (QueueElement *)malloc(sizeof(QueueElement) * initial_capacity);
    queue->capacity = initial_capacity;
    queue->size = 0;
    pthread_mutex_init(&queue->lock, NULL);
    pthread_cond_init(&queue->cond, NULL);
    return queue;
}



void swap(QueueElement *a, QueueElement *b) {
    QueueElement temp = *a;
    *a = *b;
    *b = temp;
}

void heapify_up(SafeQueue *queue, int index) {
    if (index && queue->heap[(index - 1) / 2].priority < queue->heap[index].priority) {
        swap(&queue->heap[index], &queue->heap[(index - 1) / 2]);
        heapify_up(queue, (index - 1) / 2);
    }
}

int add_work(SafeQueue *queue, QueueElement element) {
    pthread_mutex_lock(&queue->lock);
    int added=0;
    if (queue->size == queue->capacity) {
        pthread_mutex_unlock(&queue->lock);
        return added; // Or handle it in some other way
    }

    queue->heap[queue->size] = element;
    queue->size++;

    heapify_up(queue, queue->size - 1);

    pthread_cond_signal(&queue->cond); // Signal any waiting thread
    pthread_mutex_unlock(&queue->lock);
    added = 1;
    return added;
}

void heapify_down(SafeQueue *queue, int index) {
    int largest = index;
    int left = 2 * index + 1;
    int right = 2 * index + 2;

    if (left < queue->size && queue->heap[left].priority > queue->heap[largest].priority) {
        largest = left;
    }
    if (right < queue->size && queue->heap[right].priority > queue->heap[largest].priority) {
        largest = right;
    }

    if (largest != index) {
        swap(&queue->heap[index], &queue->heap[largest]);
        heapify_down(queue, largest);
    }
}


QueueElement get_work(SafeQueue *queue) {
    pthread_mutex_lock(&queue->lock);

    // Wait while the queue is empty
    while (queue->size == 0) {
        pthread_cond_wait(&queue->cond, &queue->lock);
    }

    // Get the highest priority element (root of the heap)
    QueueElement topElement = queue->heap[0];

    // Replace the root with the last element and heapify down
    queue->heap[0] = queue->heap[queue->size - 1];
    queue->size--;
    heapify_down(queue, 0);

    pthread_mutex_unlock(&queue->lock);
    return topElement;
}


QueueElement get_work_nonblocking(SafeQueue *queue) {
    pthread_mutex_lock(&queue->lock);

    if (queue->size == 0) {
        pthread_mutex_unlock(&queue->lock);
        return (QueueElement){ .priority = -1 };  // Indicate empty queue

    }

    // Get the highest priority element (root of the heap)
    QueueElement topElement = queue->heap[0];

    // Replace the root with the last element and heapify down
    queue->heap[0] = queue->heap[queue->size - 1];
    queue->size--;
    heapify_down(queue, 0);

    pthread_mutex_unlock(&queue->lock);
    return topElement;
}
