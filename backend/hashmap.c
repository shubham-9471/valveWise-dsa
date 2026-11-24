/*
 * hashmap.c — Open-addressing hash map implementation for ValveWise
 * Teammate A owns this file.
 */

#include "hashmap.h"
#include <stdint.h>   /* SIZE_MAX */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ── djb2 hash ─────────────────────────────────────────────────────── */

unsigned long djb2(const char *str) {
    unsigned long hash = 5381;
    int c;
    while ((c = (unsigned char)*str++) != '\0')
        hash = ((hash << 5) + hash) + (unsigned long)c; /* hash * 33 + c */
    return hash;
}

/* ── Internal probe helper ─────────────────────────────────────────── */

/**
 * find_slot — returns the index of the slot where key should live.
 * Returns the first tombstone encountered (for insert) or the occupied
 * matching slot (for get). Returns SIZE_MAX if table is completely full
 * of live entries (should not happen if load factor is maintained).
 */
static size_t find_slot(const HashMap *hm, const char *key, bool for_insert) {
    size_t idx  = djb2(key) % hm->capacity;
    size_t tomb = SIZE_MAX; /* first tombstone seen */

    for (size_t probe = 0; probe < hm->capacity; probe++) {
        HashEntry *e = &hm->table[idx];

        if (!e->occupied && !e->deleted) {
            /* Empty slot — key not present */
            return for_insert ? (tomb != SIZE_MAX ? tomb : idx) : SIZE_MAX;
        }
        if (e->deleted) {
            if (tomb == SIZE_MAX) tomb = idx; /* remember first tombstone */
        } else if (strncmp(e->key, key, HM_KEY_LEN) == 0) {
            return idx; /* Found matching live entry */
        }

        idx = (idx + 1) % hm->capacity;
    }

    /* Table is full of live/deleted entries */
    return for_insert ? tomb : SIZE_MAX;
}

/* ── hashmap_create ────────────────────────────────────────────────── */

HashMap *hashmap_create(size_t initial_capacity) {
    if (initial_capacity < 8) initial_capacity = 8;

    HashMap *hm = calloc(1, sizeof(HashMap));
    if (!hm) return NULL;

    hm->table = calloc(initial_capacity, sizeof(HashEntry));
    if (!hm->table) { free(hm); return NULL; }

    hm->capacity              = initial_capacity;
    hm->size                  = 0;
    hm->load_factor_threshold = 0.70f;
    return hm;
}

/* ── hashmap_free ──────────────────────────────────────────────────── */

void hashmap_free(HashMap *hm) {
    if (!hm) return;
    free(hm->table);
    free(hm);
}

/* ── hashmap_resize ────────────────────────────────────────────────── */

void hashmap_resize(HashMap *hm, size_t new_capacity) {
    if (!hm || new_capacity <= hm->capacity) return;

    HashEntry *old_table    = hm->table;
    size_t     old_capacity = hm->capacity;

    hm->table = calloc(new_capacity, sizeof(HashEntry));
    if (!hm->table) {
        hm->table = old_table; /* restore on failure */
        return;
    }
    hm->capacity = new_capacity;
    hm->size     = 0;

    /* Re-insert all live entries */
    for (size_t i = 0; i < old_capacity; i++) {
        if (old_table[i].occupied && !old_table[i].deleted) {
            hashmap_insert(hm, old_table[i].key, old_table[i].value);
        }
    }
    free(old_table);
}

/* ── hashmap_insert ────────────────────────────────────────────────── */

bool hashmap_insert(HashMap *hm, const char *key, int value) {
    if (!hm || !key) return false;

    /* Resize if load factor would be exceeded */
    if (hm->capacity == 0 ||
        (float)(hm->size + 1) / (float)hm->capacity > hm->load_factor_threshold) {
        hashmap_resize(hm, hm->capacity * 2);
    }

    size_t idx = find_slot(hm, key, true);
    if (idx == SIZE_MAX) return false;

    HashEntry *e = &hm->table[idx];

    /* Overwrite existing live entry or fill empty/tombstone slot */
    if (!e->occupied || e->deleted) {
        hm->size++;
    }
    strncpy(e->key, key, HM_KEY_LEN - 1);
    e->key[HM_KEY_LEN - 1] = '\0';
    e->value    = value;
    e->occupied = true;
    e->deleted  = false;
    return true;
}

/* ── hashmap_get ───────────────────────────────────────────────────── */

bool hashmap_get(const HashMap *hm, const char *key, int *out_value) {
    if (!hm || !key || !out_value) return false;

    size_t idx = find_slot(hm, key, false);
    if (idx == SIZE_MAX) return false;

    HashEntry *e = &hm->table[idx];
    if (!e->occupied || e->deleted) return false;

    *out_value = e->value;
    return true;
}

/* ── hashmap_delete ────────────────────────────────────────────────── */

bool hashmap_delete(HashMap *hm, const char *key) {
    if (!hm || !key) return false;

    size_t idx = find_slot(hm, key, false);
    if (idx == SIZE_MAX) return false;

    HashEntry *e = &hm->table[idx];
    if (!e->occupied || e->deleted) return false;

    e->deleted = true;
    hm->size--;
    return true;
}

/* ── hashmap_print_stats ───────────────────────────────────────────── */

void hashmap_print_stats(const HashMap *hm) {
    if (!hm) return;
    printf("HashMap: capacity=%zu, size=%zu, load=%.2f\n",
           hm->capacity, hm->size,
           hm->capacity ? (float)hm->size / (float)hm->capacity : 0.0f);
}
