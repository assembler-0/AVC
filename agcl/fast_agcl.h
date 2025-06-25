#ifndef FAST_AGCL_H
#define FAST_AGCL_H

#include <stddef.h>

// Fast hash mapping cache
typedef struct hash_map_entry {
    char avc_hash[65];
    char git_hash[41];
    struct hash_map_entry* next;
} hash_map_entry_t;

typedef struct {
    hash_map_entry_t* buckets[4096];
    size_t count;
} hash_map_t;

// Initialize hash mapping cache
hash_map_t* hash_map_create(void);

// Load all mappings into memory
int hash_map_load(hash_map_t* map);

// Fast lookup
const char* hash_map_get(hash_map_t* map, const char* avc_hash);

// Fast insert
int hash_map_set(hash_map_t* map, const char* avc_hash, const char* git_hash);

// Batch commit to disk
int hash_map_commit(hash_map_t* map);

// Free hash map
void hash_map_free(hash_map_t* map);

#endif // FAST_AGCL_H