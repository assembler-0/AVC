#include "fast_index.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// FNV-1a hash function (fast and good distribution)
static uint32_t hash_path(const char* path) {
    uint32_t hash = 2166136261u;
    while (*path) {
        hash ^= (unsigned char)*path++;
        hash *= 16777619u;
    }
    return hash % FAST_INDEX_SIZE;
}

// Normalize path (remove ./ prefix)
static const char* normalize_path(const char* path) {
    return (path && strncmp(path, "./", 2) == 0) ? path + 2 : path;
}

fast_index_t* fast_index_create(void) {
    fast_index_t* idx = calloc(1, sizeof(fast_index_t));
    return idx;
}

int fast_index_load(fast_index_t* idx) {
    if (!idx || idx->loaded) return 0;
    
    FILE* f = fopen(".avc/index", "r");
    if (!f) {
        idx->loaded = 1;
        return 0; // Empty index is OK
    }
    
    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        char hash[MAX_HASH_LEN], path[MAX_PATH_LEN];
        uint32_t mode;
        
        if (sscanf(line, "%64s %255s %o", hash, path, &mode) == 3) {
            fast_index_set(idx, path, hash, mode);
        }
    }
    
    fclose(f);
    idx->loaded = 1;
    return 0;
}

const index_entry_t* fast_index_get(fast_index_t* idx, const char* path) {
    if (!idx || !path) return NULL;
    
    const char* norm_path = normalize_path(path);
    uint32_t bucket = hash_path(norm_path);
    
    index_entry_t* entry = idx->buckets[bucket];
    while (entry) {
        if (strcmp(entry->path, norm_path) == 0) {
            return entry;
        }
        entry = entry->next;
    }
    
    return NULL;
}

int fast_index_set(fast_index_t* idx, const char* path, const char* hash, uint32_t mode) {
    if (!idx || !path || !hash) return -1;
    
    const char* norm_path = normalize_path(path);
    uint32_t bucket = hash_path(norm_path);
    
    // Check if entry exists
    index_entry_t* entry = idx->buckets[bucket];
    while (entry) {
        if (strcmp(entry->path, norm_path) == 0) {
            // Update existing entry
            strncpy(entry->hash, hash, MAX_HASH_LEN - 1);
            entry->hash[MAX_HASH_LEN - 1] = '\0';
            entry->mode = mode;
            return 0;
        }
        entry = entry->next;
    }
    
    // Create new entry
    entry = malloc(sizeof(index_entry_t));
    if (!entry) return -1;
    
    strncpy(entry->path, norm_path, MAX_PATH_LEN - 1);
    entry->path[MAX_PATH_LEN - 1] = '\0';
    strncpy(entry->hash, hash, MAX_HASH_LEN - 1);
    entry->hash[MAX_HASH_LEN - 1] = '\0';
    entry->mode = mode;
    entry->next = idx->buckets[bucket];
    
    idx->buckets[bucket] = entry;
    idx->count++;
    
    return 0;
}

int fast_index_remove(fast_index_t* idx, const char* path) {
    if (!idx || !path) return -1;
    
    const char* norm_path = normalize_path(path);
    uint32_t bucket = hash_path(norm_path);
    
    index_entry_t** entry_ptr = &idx->buckets[bucket];
    while (*entry_ptr) {
        if (strcmp((*entry_ptr)->path, norm_path) == 0) {
            index_entry_t* to_remove = *entry_ptr;
            *entry_ptr = to_remove->next;
            free(to_remove);
            idx->count--;
            return 0;
        }
        entry_ptr = &(*entry_ptr)->next;
    }
    
    return -1; // Not found
}

int fast_index_commit(fast_index_t* idx) {
    if (!idx) return -1;
    
    FILE* f = fopen(".avc/index.tmp", "w");
    if (!f) return -1;
    
    // Write all entries
    for (int i = 0; i < FAST_INDEX_SIZE; i++) {
        index_entry_t* entry = idx->buckets[i];
        while (entry) {
            fprintf(f, "%s %s %o\n", entry->hash, entry->path, entry->mode);
            entry = entry->next;
        }
    }
    
    fclose(f);
    
    // Atomic replace
    if (rename(".avc/index.tmp", ".avc/index") != 0) {
        remove(".avc/index.tmp");
        return -1;
    }
    
    return 0;
}

void fast_index_free(fast_index_t* idx) {
    if (!idx) return;
    
    for (int i = 0; i < FAST_INDEX_SIZE; i++) {
        index_entry_t* entry = idx->buckets[i];
        while (entry) {
            index_entry_t* next = entry->next;
            free(entry);
            entry = next;
        }
    }
    
    free(idx);
}

const char* fast_index_get_hash(fast_index_t* idx, const char* path) {
    const index_entry_t* entry = fast_index_get(idx, path);
    return entry ? entry->hash : NULL;
}