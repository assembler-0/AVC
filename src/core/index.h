
//
// Created by Atheria on 6/20/25.
//

#ifndef INDEX_H
#define INDEX_H

#include <stddef.h>

// Index management functions
int add_file_to_index(const char* filepath);
int remove_file_from_index(const char* filepath);
int is_file_in_index(const char* filepath);
int clear_index(void);

// Transactional index updates (load once, write once)
int index_load(void);

int index_upsert_entry(const char* filepath, const char* hash, unsigned int mode, int* unchanged_out);
int index_commit(void);
// Returns pointer to hash string for given path if present (internal buffer), else NULL
const char* index_get_hash(const char* filepath);

#endif