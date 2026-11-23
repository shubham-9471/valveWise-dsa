/*
 * hashmap.h — Open-addressing hash map: string key → int value
 * ValveWise — Teammate A owns this file.
 *
 * Uses djb2 hashing with linear probing and tombstone deletion.
 * Maps node/link string IDs to their integer indices in O(1) average time.
 */

#ifndef HASHMAP_H
#define HASHMAP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>

/* ── Entry ─────────────────────────────────────────────────────────── */

#define HM_KEY_LEN 64  /**< Maximum key (ID) string length */

/** One slot in the hash table */
typedef struct {
    char  key[HM_KEY_LEN]; /**< String key (node/link ID) */
    int   value;            /**< Integer value (node or link index) */
    bool  occupied;         /**< True if this slot holds a live entry */
    bool  deleted;          /**< True if this slot is a tombstone */
} HashEntry;

/* ── HashMap ───────────────────────────────────────────────────────── */

/** Open-addressing hash table */
typedef struct {
    HashEntry *table;              /**< Flat array of HashEntry slots */
    size_t     capacity;           /**< Total number of slots */
    size_t     size;               /**< Number of live entries */
    float      load_factor_threshold; /**< Resize when size/capacity exceeds this (0.70) */
} HashMap;

/* ── Function prototypes ────────────────────────────────────────────── */

/**
 * djb2 — Dan Bernstein's hash-2 function.
 * @param str  NUL-terminated string key
 * @return     unsigned 32-bit hash value
 */
unsigned long djb2(const char *str);

/**
 * hashmap_create — allocate a new hash map.
 * @param initial_capacity  number of initial slots (rounded up to next power-of-2 internally)
 * @return  pointer to HashMap, or NULL on allocation failure
 */
HashMap *hashmap_create(size_t initial_capacity);

/**
 * hashmap_free — free all memory owned by hm (including hm itself).
 */
void hashmap_free(HashMap *hm);

/**
 * hashmap_insert — insert or overwrite a key→value mapping.
 * Triggers a resize when the load factor exceeds the threshold.
 * @return true on success, false on critical allocation failure
 */
bool hashmap_insert(HashMap *hm, const char *key, int value);

/**
 * hashmap_get — look up a key.
 * @param out_value  set to the stored value when found
 * @return true if key exists, false otherwise
 */
bool hashmap_get(const HashMap *hm, const char *key, int *out_value);

/**
 * hashmap_delete — mark a key's slot as a tombstone.
 * @return true if the key was found and deleted, false otherwise
 */
bool hashmap_delete(HashMap *hm, const char *key);

/**
 * hashmap_resize — rehash into a new table of new_capacity slots.
 * Called automatically by hashmap_insert; exposed for testing.
 */
void hashmap_resize(HashMap *hm, size_t new_capacity);

/** Print capacity, live size, and current load factor to stdout. */
void hashmap_print_stats(const HashMap *hm);

#ifdef __cplusplus
}
#endif

#endif /* HASHMAP_H */
