#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef union HashmapVal {
    uint32_t u32;
    float flt;
    void *ptr;
} HashmapVal;

typedef struct HashmapEntry {
    uint32_t key;
    HashmapVal val;
} HashmapEntry;

typedef struct Hashmap {
    size_t nEntries;
    size_t allocSize;
    HashmapEntry *entries;
} Hashmap;

typedef union ArrayVal {
    uint32_t u32;
    float flt;
    void *ptr;
} ArrayVal;

typedef struct Array {
    size_t nEntries;
    size_t allocSize;
    ArrayVal *entries;
} Array;

Hashmap hashmap_init();
uint8_t hashmap_resize(Hashmap *hmap, size_t size);
uint8_t hashmap_set(Hashmap *hmap, uint32_t key, HashmapVal value);
uint8_t hashmap_get(const Hashmap *hmap, uint32_t key, HashmapVal *out);
uint8_t hashmap_getU32(const Hashmap *hmap, uint32_t key, uint32_t *out);
uint8_t hashmap_getP(const Hashmap *hmap, uint32_t key, void **out);
uint8_t hashmap_exists(const Hashmap *hmap, uint32_t key);
uint8_t hashmap_delResize(Hashmap *hmap, uint32_t key, uint8_t resize);
uint8_t hashmap_del(Hashmap *hmap, uint32_t key, uint8_t resize);

Array array_init();
uint8_t array_resize(Array *arr, size_t size);
void array_set(Array *arr, uint32_t pos, ArrayVal value);
ArrayVal array_get(const Array *arr, uint32_t pos);
uint32_t array_at(const Array *arr, ArrayVal value);
uint8_t array_has(const Array *arr, ArrayVal value);
void array_swap(Array *arr, uint32_t posA, uint32_t posB);
void array_insert(Array *arr, uint32_t pos, ArrayVal value);
void array_pushBack(Array *arr, ArrayVal value);
ArrayVal array_popBack(Array *arr);
uint8_t array_del(Array *arr, uint32_t pos);
void array_clear(Array *arr);
size_t array_size(const Array *arr);
size_t array_capacity(const Array *arr);

// Calculate hash for a string
uint32_t str_hash(const char *const str);

// Find value in sorted buffer.
// Returns 0 if value not found, otherwise 1.
// The closest index with value below needle is written to pos, capped to 0.
// pos can be null
uint8_t binsearch_s32Inc(const int32_t *haystack, size_t len, int32_t needle,
                         uint32_t *pos);
uint8_t binsearch_u32Inc(const uint32_t *haystack, size_t len, uint32_t needle,
                         uint32_t *pos);
uint8_t binsearch_fltInc(const float *haystack, size_t len, float needle,
                         uint32_t *pos);
uint8_t binsearch_fltArrayInc(const ArrayVal *haystack, size_t len,
                              float needle, uint32_t *pos);
uint8_t binsearch_hashmapInc(const HashmapEntry *hmap, size_t len, uint32_t key,
                             uint32_t *pos);

// Insert value into sorted buffer.
// Returns the inserted value index in buffer.
// len = buffer length before insertion
uint32_t insertsort_s32Inc(int32_t *buff, size_t len, int32_t val);
uint32_t insertsort_u32Inc(uint32_t *buff, size_t len, uint32_t val);