#include "fast_agcl.h"
#define _XOPEN_SOURCE 700   /* for strptime */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include <arpa/inet.h>

#define AGCL_MAP_PATH ".git/avc-map"

// FNV-1a hash for string keys
static uint32_t hash_string(const char* str) {
    uint32_t hash = 2166136261u;
    while (*str) {
        hash ^= (unsigned char)*str++;
        hash *= 16777619u;
    }
    return hash % 4096;
}

hash_map_t* hash_map_create(void) {
    hash_map_t* map = calloc(1, sizeof(hash_map_t));
    return map;
}

int hash_map_load(hash_map_t* map) {
    if (!map) return -1;
    
    FILE* fp = fopen(AGCL_MAP_PATH, "r");
    if (!fp) return 0; // No mappings yet
    
    char avc_hash[65], git_hash[41];
    while (fscanf(fp, "%64s %40s", avc_hash, git_hash) == 2) {
        hash_map_set(map, avc_hash, git_hash);
    }
    
    fclose(fp);
    return 0;
}

const char* hash_map_get(hash_map_t* map, const char* avc_hash) {
    if (!map || !avc_hash) return NULL;
    
    uint32_t bucket = hash_string(avc_hash);
    hash_map_entry_t* entry = map->buckets[bucket];
    
    while (entry) {
        if (strcmp(entry->avc_hash, avc_hash) == 0) {
            return entry->git_hash;
        }
        entry = entry->next;
    }
    
    return NULL;
}

int hash_map_set(hash_map_t* map, const char* avc_hash, const char* git_hash) {
    if (!map || !avc_hash || !git_hash) return -1;
    
    uint32_t bucket = hash_string(avc_hash);
    
    // Check if exists
    hash_map_entry_t* entry = map->buckets[bucket];
    while (entry) {
        if (strcmp(entry->avc_hash, avc_hash) == 0) {
            strcpy(entry->git_hash, git_hash);
            return 0;
        }
        entry = entry->next;
    }
    
    // Create new entry
    entry = malloc(sizeof(hash_map_entry_t));
    if (!entry) return -1;
    
    strcpy(entry->avc_hash, avc_hash);
    strcpy(entry->git_hash, git_hash);
    entry->next = map->buckets[bucket];
    map->buckets[bucket] = entry;
    map->count++;
    
    return 0;
}

int hash_map_commit(hash_map_t* map) {
    if (!map) return -1;
    
    FILE* fp = fopen(AGCL_MAP_PATH, "w");
    if (!fp) return -1;
    
    for (int i = 0; i < 4096; i++) {
        hash_map_entry_t* entry = map->buckets[i];
        while (entry) {
            fprintf(fp, "%s %s\n", entry->avc_hash, entry->git_hash);
            entry = entry->next;
        }
    }
    
    fclose(fp);
    return 0;
}

void hash_map_free(hash_map_t* map) {
    if (!map) return;
    
    for (int i = 0; i < 4096; i++) {
        hash_map_entry_t* entry = map->buckets[i];
        while (entry) {
            hash_map_entry_t* next = entry->next;
            free(entry);
            entry = next;
        }
    }
    
    free(map);
}