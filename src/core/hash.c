#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <openssl/sha.h>
#include "hash.h"
void sha1_hash(const char* content, size_t size, char* hash_out) {
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1((unsigned char*)content, size, hash);

    // Convert binary hash to hex string
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
        sprintf(hash_out + (i * 2), "%02x", hash[i]);
    }
    hash_out[40] = '\0';  // Null terminate
}

// Hash with Git-style object format
// Git prepends "blob <size>\0" before hashing
void sha1_hash_object(const char* type, const char* content, size_t size, char* hash_out) {
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
    sha1_hash(full_content, total_size, hash_out);

    free(full_content);
}

// Store object with proper format

