#ifndef FAST_INDEX_H
#define FAST_INDEX_H

#include <stddef.h>
#include <stdint.h>

// Fast hash table index for O(1) lookups
#define FAST_INDEX_SIZE 8192
#define MAX_PATH_LEN 256
#define MAX_HASH_LEN 65

typedef struct index_entry {
    char path[MAX_PATH_LEN];
    char hash[MAX_HASH_LEN];
    uint32_t mode;
    struct index_entry* next; // Chaining for collisions
} index_entry_t;

typedef struct {
    index_entry_t* buckets[FAST_INDEX_SIZE];
    size_t count;
    int loaded;
} fast_index_t;

// Initialize fast index
fast_index_t* fast_index_create(void);

// Load index from file into hash table
int fast_index_load(fast_index_t* idx);

// Get entry by path (O(1) average case)
const index_entry_t* fast_index_get(fast_index_t* idx, const char* path);

// Insert/update entry (O(1) average case)
int fast_index_set(fast_index_t* idx, const char* path, const char* hash, uint32_t mode);

// Remove entry (O(1) average case)
int fast_index_remove(fast_index_t* idx, const char* path);

// Write hash table back to file
int fast_index_commit(fast_index_t* idx);

// Free hash table
void fast_index_free(fast_index_t* idx);

// Get hash for path (convenience function)
const char* fast_index_get_hash(fast_index_t* idx, const char* path);

#endif // FAST_INDEX_H