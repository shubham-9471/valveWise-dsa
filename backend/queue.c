/*
 * queue.c — Circular-buffer integer queue implementation
 * ValveWise — Teammate A owns this file.
 */

#include "queue.h"
#include <stdlib.h>
#include <stdio.h>

Queue *queue_create(int capacity) {
    if (capacity <= 0) capacity = 64;
    Queue *q = malloc(sizeof(Queue));
    if (!q) return NULL;
    q->data = malloc((size_t)capacity * sizeof(int));
    if (!q->data) { free(q); return NULL; }
    q->head     = 0;
    q->tail     = 0;
    q->size     = 0;
    q->capacity = capacity;
    return q;
}

void queue_free(Queue *q) {
    if (!q) return;
    free(q->data);
    free(q);
}

bool queue_enqueue(Queue *q, int val) {
    if (!q || q->size == q->capacity) return false;
    q->data[q->tail] = val;
    q->tail = (q->tail + 1) % q->capacity;
    q->size++;
    return true;
}

bool queue_dequeue(Queue *q, int *out) {
    if (!q || q->size == 0 || !out) return false;
    *out    = q->data[q->head];
    q->head = (q->head + 1) % q->capacity;
    q->size--;
    return true;
}

bool queue_is_empty(const Queue *q) {
    return (!q || q->size == 0);
}

void queue_clear(Queue *q) {
    if (!q) return;
    q->head = 0;
    q->tail = 0;
    q->size = 0;
}
