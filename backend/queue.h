/*
 * queue.h — Circular-buffer integer queue for BFS
 * ValveWise — Teammate A owns this file.
 */

#ifndef QUEUE_H
#define QUEUE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/** Circular-buffer queue storing int values (node indices for BFS). */
typedef struct {
    int *data;     /**< Heap-allocated ring buffer */
    int  head;     /**< Index of the next element to dequeue */
    int  tail;     /**< Index where the next element will be enqueued */
    int  size;     /**< Number of elements currently in the queue */
    int  capacity; /**< Total number of slots in data[] */
} Queue;

/** Allocate a new queue with the given capacity. Returns NULL on failure. */
Queue *queue_create(int capacity);

/** Free all memory owned by q. */
void   queue_free(Queue *q);

/**
 * queue_enqueue — add val to the back of the queue.
 * @return false if the queue is full, true on success.
 */
bool   queue_enqueue(Queue *q, int val);

/**
 * queue_dequeue — remove and return the front element.
 * @param out  set to the dequeued value on success
 * @return false if the queue is empty, true on success.
 */
bool   queue_dequeue(Queue *q, int *out);

/** Return true if the queue has no elements. */
bool   queue_is_empty(const Queue *q);

/** Reset head/tail/size to 0 (data is not freed or zeroed). */
void   queue_clear(Queue *q);

#ifdef __cplusplus
}
#endif

#endif /* QUEUE_H */
