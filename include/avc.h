#ifndef AVC_H
#define AVC_H

#include <stddef.h>
#include <time.h>

#define HASH_SIZE 40
#define MAX_PATH 256
#define MAX_MESSAGE 1024

// Core data structures
typedef struct {
    char hash[HASH_SIZE + 1];
    char* content;
    size_t size;
} blob_t;

typedef struct {
    char name[MAX_PATH];
    char hash[HASH_SIZE + 1];
    int mode;
} tree_entry_t;

typedef struct {
    tree_entry_t* entries;
    size_t count;
    size_t capacity;
} tree_t;

typedef struct {
    char hash[HASH_SIZE + 1];
    char message[MAX_MESSAGE];
    char author[256];
    time_t timestamp;
    char parent_hash[HASH_SIZE + 1];
    char tree_hash[HASH_SIZE + 1];
} commit_t;

#endif