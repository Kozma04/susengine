#include "./dsa.h"

#define _BINSEARCH_FUNC(name, bufftype, valtype, cmp, cmp_inv, mid_read) \
    uint8_t name(const bufftype *const haystack, size_t len, valtype needle, uint32_t *pos) { \
        int32_t left = 0, right = len - 1, mid; \
        valtype mid_val; \
        if(len == 0) { \
            if(pos) \
                *pos = 0; \
            return 0; \
        } \
        while(left <= right) { \
            mid = (left + right) >> 1; \
            mid_val = mid_read; \
            if(mid_val cmp needle) \
                right = mid - 1; \
            else if(mid_val cmp_inv needle) \
                left = mid + 1; \
            else { \
                if(pos) \
                    *pos = mid; \
                return 1; \
            } \
        } \
        if(pos) \
            *pos = mid; \
        return 0; \
    }


#define _INSERTSORT_FUNC(name, binsearch_func, buftype, searchtype) \
    uint32_t name(buftype *const buf, size_t size, searchtype val) { \
        int32_t pos, i; \
        binsearch_func(buf, size, val, &pos); \
        if(size) pos++; \
        for(i = size; i > pos; i--) \
            buf[i] = buf[i - 1]; \
        buf[pos] = val; \
        return pos; \
    }


Hashmap hashmap_init() {
    return (Hashmap){0, 0, 0};
}

uint8_t hashmap_resize(Hashmap *const hmap, const size_t size) {
    HashmapEntry *const newArr = realloc(hmap->entries, size * sizeof(HashmapEntry));
    if(newArr) {
        hmap->entries = newArr;
        hmap->allocSize = size;
        if(hmap->nEntries > hmap->allocSize)
            hmap->nEntries = hmap->allocSize;
        return 1;
    }
    return 0;
}

uint8_t hashmap_set(Hashmap *const hmap, const uint32_t key,
                    const HashmapVal value) {
    uint32_t pos = 0;
    uint8_t retVal, res;
    if(hmap->entries != NULL)
        res = binsearch_hashmapInc(hmap->entries, hmap->nEntries, key, &pos);
    //printf("%u insert at %u\n", key, pos);
    if(hmap->entries == NULL || !res) {
        if(hmap->nEntries + 1 > hmap->allocSize) {
            if(!hashmap_resize(hmap, hmap->allocSize * 2 + 1))
                return 0;
        }
        if(hmap->nEntries && pos == hmap->nEntries - 1 && key > hmap->entries[pos].key)
            pos++;
        hmap->nEntries++;
        for(uint32_t i = hmap->nEntries - 1; i > pos; i--)
            hmap->entries[i] = hmap->entries[i - 1];
        retVal = 1;
    }
    else retVal = 2;
    hmap->entries[pos].key = key;
    hmap->entries[pos].val = value;

    /*printf("values: ");
    for(int i = 0; i < hmap->nEntries; i++)
        printf("%u ", hmap->entries[i].key);
    printf("\n");*/
    return retVal;
}

uint8_t hashmap_get(const Hashmap *const hmap, const uint32_t key,
                    HashmapVal* const out) {
    uint32_t pos;
    if(hmap->entries == NULL || !binsearch_hashmapInc(hmap->entries, hmap->nEntries, key, &pos))
        return 0;
    if(out)
        *out = hmap->entries[pos].val;
    return 1;
}

uint8_t hashmap_getU32(const Hashmap *hmap, const uint32_t key,
                       uint32_t *const out) {
    HashmapVal val;
    uint8_t res = hashmap_get(hmap, key, &val);
    if(res) *out = val.u32;
    return res;
}

uint8_t hashmap_getP(const Hashmap *hmap, const uint32_t key, void **const out) {
    HashmapVal val;
    uint8_t res = hashmap_get(hmap, key, &val);
    if(res) *out = val.ptr;
    return res;
}

uint8_t hashmap_delResize(Hashmap *const hmap, const uint32_t key,
                          const uint8_t resize) {
    const uint32_t final_nEntries = hmap->nEntries - 1;
    uint32_t pos, i;
    if(hmap->entries == 0)
        return 0;
    if(!binsearch_hashmapInc(hmap->entries, hmap->nEntries, key, &pos))
        return 0;
    if(resize)
        hashmap_resize(hmap, hmap->nEntries - 1);
    hmap->nEntries = final_nEntries;
    if(final_nEntries) {
        for(i = pos; i < final_nEntries - 1; i++)
            hmap->entries[i] = hmap->entries[i + 1];
    }
    return 1;
}

uint8_t hashmap_del(Hashmap *const hmap, const uint32_t key,
                    const uint8_t resize) {
    return hashmap_delResize(hmap, key, 0);
}


Array array_init() {
    return (Array){0, 0, 0};
}

uint8_t array_resize(Array *const arr, const size_t size) {
    ArrayVal *const newArr = realloc(arr->entries, size * sizeof(ArrayVal));
    if(newArr) {
        arr->entries = newArr;
        arr->allocSize = size;
        if(arr->nEntries > arr->allocSize)
            arr->nEntries = arr->allocSize;
        return 1;
    }
    return 0;
}

void array_set(Array *const arr, const uint32_t pos, const ArrayVal value) {
    if(pos < arr->nEntries)
        arr->entries[pos] = value;
}

ArrayVal array_get(const Array *const arr, const uint32_t pos) {
    return arr->entries[pos];
}

uint32_t array_at(const Array *const arr, const ArrayVal value) {
    uint32_t pos = 0;
    while(pos < arr->nEntries) {
        if(!memcmp(arr->entries + pos, &value, sizeof(ArrayVal)))
            break;
        pos++;
    }
    return pos;
}

uint8_t array_has(const Array *const arr, const ArrayVal value) {
    return array_at(arr, value) != arr->nEntries;
}

void array_swap(Array *arr, const uint32_t posA, const uint32_t posB) {
    if(posA >= arr->nEntries || posB >= arr->nEntries)
        return;
    ArrayVal temp = arr->entries[posA];
    arr->entries[posA] = arr->entries[posB];
    arr->entries[posB] = temp;
}

void array_insert(Array *const arr, const uint32_t pos, const ArrayVal value) {
    if(pos > arr->nEntries)
        return;
    if(arr->entries == NULL || arr->nEntries + 1 > arr->allocSize)
        array_resize(arr, arr->allocSize * 2 + 1);
    arr->nEntries++;
    for(uint32_t i = arr->nEntries - 1; i > pos; i--)
            arr->entries[i] = arr->entries[i - 1];
    arr->entries[pos] = value;
}

void array_pushBack(Array *const arr, const ArrayVal value) {
    array_insert(arr, arr->nEntries, value);
}

uint8_t array_del(Array *const arr, const uint32_t pos) {
    if(pos >= arr->nEntries)
        return 0;
    for(uint32_t i = pos; i < arr->nEntries - 1; i++)
        arr->entries[i] = arr->entries[i + 1];
    arr->nEntries--;
    return 1;
}

void array_clear(Array *const arr) {
    arr->nEntries = 0;
}

size_t array_size(const Array *const arr) {
    return arr->nEntries;
}

size_t array_capacity(const Array *const arr) {
    return arr->allocSize;
}


// https://stackoverflow.com/questions/7666509/hash-function-for-string
uint32_t str_hash(const char *const str) {
    uint32_t hash = 5381;
    for(uint32_t pos = 0; str[pos]; pos++) {
        hash = ((hash << 5) + hash) + str[pos]; /* hash * 33 + c */
    }
    return hash;
}


_BINSEARCH_FUNC(binsearch_s32Inc, int32_t, int32_t, >, <, haystack[mid]);
_BINSEARCH_FUNC(binsearch_u32Inc, uint32_t, uint32_t, >, <, haystack[mid]);
_BINSEARCH_FUNC(binsearch_fltInc, float, float, >, <, haystack[mid]);
_BINSEARCH_FUNC(binsearch_fltArrayInc, ArrayVal, float, >, <, haystack[mid].flt);
_BINSEARCH_FUNC(binsearch_hashmapInc, HashmapEntry, uint32_t, >, <, haystack[mid].key);

_INSERTSORT_FUNC(insertsort_u32Inc, binsearch_u32Inc, uint32_t, uint32_t);
_INSERTSORT_FUNC(insertsort_s32Inc, binsearch_s32Inc, int32_t, int32_t);


#undef _BINSEARCH_FUNC