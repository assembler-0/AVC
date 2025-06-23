#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <blake3.h> // BLAKE3 reference implementation
#include "hash.h"
#define HASH_SIZE 64
void blake3_hash(const char* content, size_t size, char* hash_out) {
    uint8_t digest[32];
    blake3_hasher hasher;
    blake3_hasher_init(&hasher);
    blake3_hasher_update(&hasher, content, size);
    blake3_hasher_finalize(&hasher, digest, 32);
    for (int i = 0; i < 32; i++) {
        sprintf(hash_out + (i * 2), "%02x", digest[i]);
    }
    hash_out[64] = '\0';
}

// Hash with Git-style object format using BLAKE3
// Git prepends "blob <size>\0" before hashing (same as before)
void blake3_hash_object(const char* type, const char* content, size_t size, char* hash_out) {
    // Create Git-style object format
    char header[64];
    int header_len = snprintf(header, sizeof(header), "%s %zu", type, size);

    // Allocate buffer for header + null byte + content
    size_t total_size = header_len + 1 + size;
    char* full_content = malloc(total_size);
    if (!full_content) {
        fprintf(stderr, "Memory allocation failed\n");
        return;
    }

    // Copy header, null byte, then content
    memcpy(full_content, header, header_len);
    full_content[header_len] = '\0';
    memcpy(full_content + header_len + 1, content, size);

    // Hash the full content
    blake3_hash(full_content, total_size, hash_out);

    free(full_content);
}

// Store object with proper format

