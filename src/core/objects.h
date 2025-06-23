//
// Created by Atheria on 6/20/25.
//

#ifndef OBJECTS_H
#define OBJECTS_H

#include <stddef.h>

// Store a blob object from a file
int store_blob_from_file(const char* filepath, char* hash_out);

// Store an object with given type and content
int store_object(const char* type, const char* content, size_t size, char* hash_out);

// Load an object by hash
char* load_object(const char* hash, size_t* size_out, char* type_out);

// Free memory pool (call periodically)
void free_memory_pool(void);

// Reset memory pool (reuse chunks instead of freeing)
void reset_memory_pool(void);

// Calculate hash of a file using BLAKE3
int blake3_file_hex(const char* filepath, char hash_out[65]);

// Diagnostic function to check object format
int check_object_format(const char* hash);

#endif // OBJECTS_H
