/*
 * heap.h — Binary Min/Max-Heap storing (double key, int value) pairs
 * ValveWise — Teammate A owns this file.
 *
 * Used to rank pipes/valves by disruption impact for the optimizer.
 */

#ifndef HEAP_H
#define HEAP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/** One element in the heap: a priority key and an associated integer (link index). */
typedef struct {
    double key;   /**< Priority (disruption score, etc.) */
    int    value; /**< Associated link or node index */
} HeapNode;

/** Binary heap (min-heap or max-heap depending on is_max_heap). */
typedef struct {
    HeapNode *data;       /**< Array-based heap storage */
    int       size;       /**< Current number of elements */
    int       capacity;   /**< Allocated capacity of data[] */
    bool      is_max_heap;/**< True → max-heap; False → min-heap */
} Heap;

/** Allocate a heap with the given capacity. is_max_heap controls ordering. */
Heap *heap_create(int capacity, bool is_max_heap);

/** Free all memory owned by h. */
void  heap_free(Heap *h);

/**
 * heap_push — insert a (key, value) pair.
 * @return false if heap is full (no realloc in this implementation), true on success.
 */
bool  heap_push(Heap *h, double key, int value);

/**
 * heap_pop — remove and return the root element (min or max).
 * @param out  written with the popped element
 * @return false if heap is empty, true on success.
 */
bool  heap_pop(Heap *h, HeapNode *out);

/**
 * heap_peek — read the root element without removing it.
 * @param out  written with the root element
 * @return false if heap is empty, true on success.
 */
bool  heap_peek(const Heap *h, HeapNode *out);

/** Return true if the heap is empty. */
bool  heap_is_empty(const Heap *h);

/** Remove all elements without freeing memory. */
void  heap_clear(Heap *h);

#ifdef __cplusplus
}
#endif

#endif /* HEAP_H */
