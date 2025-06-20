#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <openssl/sha.h>
#include "hash.h"
#define HASH_SIZE 64
void sha256_hash(const char* content, size_t size, char* hash_out) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)content, size, hash);

    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(hash_out + (i * 2), "%02x", hash[i]);
    }
    hash_out[64] = '\0';
}

// Hash with Git-style object format
// Git prepends "blob <size>\0" before hashing
void sha256_hash_object(const char* type, const char* content, size_t size, char* hash_out) {
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
    sha256_hash(full_content, total_size, hash_out);

    free(full_content);
}

// Store object with proper format

