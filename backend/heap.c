/*
 * heap.c — Binary Min/Max-Heap implementation for ValveWise
 * Teammate A owns this file.
 */

#include "heap.h"
#include <stdlib.h>
#include <stdio.h>

/* ── Comparison helper ─────────────────────────────────────────────── */

/** Return true if element at index a has higher priority than element at index b. */
static bool has_priority(const Heap *h, int a, int b) {
    if (h->is_max_heap)
        return h->data[a].key > h->data[b].key;
    else
        return h->data[a].key < h->data[b].key;
}

/* ── Swap ──────────────────────────────────────────────────────────── */

static void swap(HeapNode *a, HeapNode *b) {
    HeapNode tmp = *a; *a = *b; *b = tmp;
}

/* ── Sift up ───────────────────────────────────────────────────────── */

static void sift_up(Heap *h, int idx) {
    while (idx > 0) {
        int parent = (idx - 1) / 2;
        if (has_priority(h, idx, parent)) {
            swap(&h->data[idx], &h->data[parent]);
            idx = parent;
        } else {
            break;
        }
    }
}

/* ── Sift down ─────────────────────────────────────────────────────── */

static void sift_down(Heap *h, int idx) {
    int n = h->size;
    while (true) {
        int left  = 2 * idx + 1;
        int right = 2 * idx + 2;
        int best  = idx;

        if (left  < n && has_priority(h, left,  best)) best = left;
        if (right < n && has_priority(h, right, best)) best = right;

        if (best == idx) break;

        swap(&h->data[idx], &h->data[best]);
        idx = best;
    }
}

/* ── Public API ────────────────────────────────────────────────────── */

Heap *heap_create(int capacity, bool is_max_heap) {
    if (capacity <= 0) capacity = 64;
    Heap *h = malloc(sizeof(Heap));
    if (!h) return NULL;
    h->data = malloc((size_t)capacity * sizeof(HeapNode));
    if (!h->data) { free(h); return NULL; }
    h->size       = 0;
    h->capacity   = capacity;
    h->is_max_heap = is_max_heap;
    return h;
}

void heap_free(Heap *h) {
    if (!h) return;
    free(h->data);
    free(h);
}

bool heap_push(Heap *h, double key, int value) {
    if (!h || h->size >= h->capacity) return false;
    h->data[h->size].key   = key;
    h->data[h->size].value = value;
    sift_up(h, h->size);
    h->size++;
    return true;
}

bool heap_pop(Heap *h, HeapNode *out) {
    if (!h || h->size == 0 || !out) return false;
    *out = h->data[0];
    h->size--;
    if (h->size > 0) {
        h->data[0] = h->data[h->size];
        sift_down(h, 0);
    }
    return true;
}

bool heap_peek(const Heap *h, HeapNode *out) {
    if (!h || h->size == 0 || !out) return false;
    *out = h->data[0];
    return true;
}

bool heap_is_empty(const Heap *h) {
    return (!h || h->size == 0);
}

void heap_clear(Heap *h) {
    if (h) h->size = 0;
}
