//
// Created by Atheria on 6/20/25.
//
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <zlib.h>
#include "hash.h"
#include "objects.h"

// Compress data using zlib
char* compress_data(const char* data, size_t size, size_t* compressed_size) {
    uLongf dest_len = compressBound(size);
    char* compressed = malloc(dest_len);
    if (!compressed) {
        return NULL;
    }
    
    if (compress((Bytef*)compressed, &dest_len, (const Bytef*)data, size) != Z_OK) {
        free(compressed);
        return NULL;
    }
    
    *compressed_size = dest_len;
    return compressed;
}

// Decompress data using zlib
char* decompress_data(const char* compressed_data, size_t compressed_size, size_t expected_size) {
    char* decompressed = malloc(expected_size + 1);
    if (!decompressed) {
        return NULL;
    }
    
    uLongf dest_len = expected_size;
    if (uncompress((Bytef*)decompressed, &dest_len, (const Bytef*)compressed_data, compressed_size) != Z_OK) {
        free(decompressed);
        return NULL;
    }
    
    decompressed[dest_len] = '\0';
    return decompressed;
}

int store_object(const char* type, const char* content, size_t size, char* hash_out) {
    // Generate hash first (before compression, like Git)
    sha256_hash_object(type, content, size, hash_out);

    // Create object path: .avc/objects/ab/cdef123... (Git-style subdirectories)
    char obj_dir[512], obj_path[512];
    snprintf(obj_dir, sizeof(obj_dir), ".avc/objects/%.2s", hash_out);
    snprintf(obj_path, sizeof(obj_path), "%s/%s", obj_dir, hash_out + 2);

    // Create subdirectory if it doesn't exist
    struct stat st;
    if (stat(obj_dir, &st) == -1) {
        if (mkdir(obj_dir, 0755) == -1 && errno != EEXIST) {
            perror("mkdir");
            return -1;
        }
    }

    // Check if object already exists
    if (stat(obj_path, &st) == 0) {
        // Object already exists, no need to store again
        return 0;
    }

    // Create full object content (header + content)
    char header[64];
    int header_len = snprintf(header, sizeof(header), "%s %zu", type, size);
    
    size_t full_size = header_len + 1 + size;
    char* full_content = malloc(full_size);
    if (!full_content) {
        return -1;
    }
    
    memcpy(full_content, header, header_len);
    full_content[header_len] = '\0';
    memcpy(full_content + header_len + 1, content, size);

    // Compress the full content
    size_t compressed_size;
    char* compressed = compress_data(full_content, full_size, &compressed_size);
    free(full_content);
    
    if (!compressed) {
        fprintf(stderr, "Failed to compress object\n");
        return -1;
    }

    // Store compressed object
    FILE* obj_file = fopen(obj_path, "wb");
    if (!obj_file) {
        perror("Failed to create object file");
        free(compressed);
        return -1;
    }

    fwrite(compressed, 1, compressed_size, obj_file);
    fclose(obj_file);
    free(compressed);

    return 0;
}

// Load object content
// Fixed load_object function for objects.c
char* load_object(const char* hash, size_t* size_out, char* type_out) {
    // Create object path
    char obj_path[512];
    snprintf(obj_path, sizeof(obj_path), ".avc/objects/%.2s/%s", hash, hash + 2);

    // Read compressed object file
    FILE* obj_file = fopen(obj_path, "rb");
    if (!obj_file) {
        perror("fopen");
        return NULL;
    }

    // Get file size
    fseek(obj_file, 0, SEEK_END);
    size_t compressed_size = ftell(obj_file);
    fseek(obj_file, 0, SEEK_SET);

    // Read compressed data
    char* compressed_data = malloc(compressed_size);
    if (!compressed_data) {
        fclose(obj_file);
        return NULL;
    }

    size_t bytes_read = fread(compressed_data, 1, compressed_size, obj_file);
    fclose(obj_file);

    if (bytes_read != compressed_size) {
        free(compressed_data);
        return NULL;
    }

    // Try decompression with increasing buffer sizes
    size_t try_size = compressed_size * 4; // Start with 4x compressed size
    char* decompressed = NULL;
    size_t actual_decompressed_size = 0;

    for (int attempts = 0; attempts < 64; attempts++) {
        char* temp_buffer = malloc(try_size + 1);
        if (!temp_buffer) {
            break;
        }

        uLongf dest_len = try_size;
        int result = uncompress((Bytef*)temp_buffer, &dest_len, (const Bytef*)compressed_data, compressed_size);

        if (result == Z_OK) {
            decompressed = temp_buffer;
            actual_decompressed_size = dest_len;
            break;
        } else {
            free(temp_buffer);
            if (result == Z_BUF_ERROR) {
                try_size *= 2; // Try larger buffer
                continue;
            } else {
                break; // Other errors are not recoverable by increasing buffer size
            }
        }
    }

    free(compressed_data);
    if (!decompressed) {
        return NULL;
    }

    // Null-terminate the decompressed data
    decompressed[actual_decompressed_size] = '\0';

    // Parse header: "type size\0content"
    char* space = strchr(decompressed, ' ');
    if (!space) {
        free(decompressed);
        return NULL;
    }

    char* null_pos = memchr(decompressed, '\0', actual_decompressed_size);
    if (!null_pos) {
        free(decompressed);
        return NULL;
    }

    // Extract type
    *space = '\0';
    strcpy(type_out, decompressed);

    // Extract size
    size_t declared_size = atoll(space + 1);

    // Find content start (after the null terminator)
    char* content_start = null_pos + 1;
    size_t header_size = content_start - decompressed;
    size_t available_content_size = actual_decompressed_size - header_size;

    // Verify we have enough content
    if (available_content_size < declared_size) {
        free(decompressed);
        return NULL;
    }

    // Allocate buffer for content only
    char* content = malloc(declared_size + 1);
    if (!content) {
        free(decompressed);
        return NULL;
    }

    // Copy content
    memcpy(content, content_start, declared_size);
    content[declared_size] = '\0';

    free(decompressed);

    *size_out = declared_size;
    return content;
}