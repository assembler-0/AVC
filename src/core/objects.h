//
// Created by Atheria on 6/20/25.
//

#ifndef OBJECTS_H
#define OBJECTS_H

#include <stddef.h>

// Object storage functions
int store_object(const char* type, const char* content, size_t size, char* hash_out);
// Store a blob object directly from a file on disk, streaming to avoid reading the full file into memory.
int store_blob_from_file(const char* filepath, char* hash_out);
char* load_object(const char* hash, size_t* size_out, char* type_out);

// Hash functions
int blake3_file_hex(const char* filepath, char hash_out[65]);

// Compression control
void set_compression_enabled(int enabled);

// Free memory pool (call periodically)
void free_memory_pool(void);

#endif // OBJECTS_H
