#define _XOPEN_SOURCE 700
//
// Created by Atheria on 6/20/25.
//
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <omp.h>
#include "hash.h"
#include "objects.h"
#include "compression.h"
#include <blake3.h>
#include <zstd.h>

// Compression constants
static int g_fast_mode = 0; // 0 = normal, 1 = fast
void objects_set_fast_mode(int fast) { g_fast_mode = fast; }
#define AVC_COMPRESSION_LEVEL_MAX 6  // Default maximum compression level

#define AVC_COMPRESSION_LEVEL_BALANCED 3

// Memory pool configuration
#define MEMORY_POOL_CHUNK_SIZE (1024 * 1024)  // 1MB chunks
#define MAX_MEMORY_POOL_CHUNKS 1000          // Max 1GB total
#define COMPRESSOR_POOL_SIZE 16             // Max 16 compressors per thread
#define MAX_COMPRESSED_SIZE (1024 * 1024 * 1024)  // 1GB max compressed size

// All compression now uses zstd. AVC_COMPRESS_LIBDEFLATE is for Git compatibility only.

// NEW: Unified compression wrapper
char* compress_data_unified(const char* data, size_t size, size_t* compressed_size, int level) {
    avc_compression_type_t backend = avc_get_compression_backend();
    if (g_fast_mode) level = 0;
    return avc_compress(data, size, compressed_size, backend, level);
}

// Legacy wrapper for libdeflate (uses zstd with zlib header for compatibility)
    char* compress_data_libdeflate(const char* data, size_t size, size_t* compressed_size, int level) {
    if (g_fast_mode) level = 0;
    return avc_compress(data, size, compressed_size, AVC_COMPRESS_LIBDEFLATE, level);
}

// NEW: Unified decompression wrapper
char* decompress_data_unified(const char* compressed_data, size_t compressed_size, size_t expected_size) {
    avc_compression_type_t type = avc_detect_compression_type(compressed_data, compressed_size);
    return avc_decompress(compressed_data, compressed_size, expected_size, type);
}

// Legacy wrapper for libdeflate
char* decompress_data_libdeflate(const char* compressed_data, size_t compressed_size, size_t expected_size) {
    return avc_decompress(compressed_data, compressed_size, expected_size, AVC_COMPRESS_LIBDEFLATE);
}

// NEW: Use unified compression by default
char* compress_data(const char* data, size_t size, size_t* compressed_size) {
    return compress_data_unified(data, size, compressed_size, 6);
}

char* compress_data_aggressive(const char* data, size_t size, size_t* compressed_size) {
    return compress_data_unified(data, size, compressed_size, 9);
}

char* decompress_data(const char* compressed_data, size_t compressed_size, size_t expected_size) {
    return decompress_data_unified(compressed_data, compressed_size, expected_size);
}

// Fast compression wrapper
char* compress_data_fast(const char* data, size_t size, size_t* compressed_size) {
    return compress_data_unified(data, size, compressed_size, 3);
}

// Wrapper functions for utility functions from compression module
void show_compression_stats(size_t original_size, size_t compressed_size, const char* type) {
    if (compressed_size > 0) {
        double ratio = (double)compressed_size / original_size * 100.0;
        printf("[%s] %zu -> %zu bytes (%.1f%%)\n", type, original_size, compressed_size, ratio);
    }
}

// REMOVED: flush_to_file_libdeflate - no longer needed

// Stream a file into a compressed blob object while computing its SHA-256.
int store_blob_from_file(const char* filepath, char* hash_out) {
    struct stat st;
    if (stat(filepath, &st) == -1) {
        perror("stat");
        return -1;
    }
    size_t size = st.st_size;

    // Read the entire file into memory
    FILE* src = fopen(filepath, "rb");
    if (!src) {
        perror("fopen");
        return -1;
    }

    char* file_content = malloc(size);
    if (!file_content) {
        fclose(src);
        return -1;
    }

    size_t bytes_read = fread(file_content, 1, size, src);
    fclose(src);

    if (bytes_read != size) {
        free(file_content);
        return -1;
    }

    // Delegate to store_object, which handles hashing, compression, and storage
    int result = store_object("blob", file_content, size, hash_out);

    free(file_content);

    return result;
}

int store_object(const char* type, const char* content, size_t size, char* hash_out) {
    // Generate hash first (before compression, like Git)
    blake3_hash_object(type, content, size, hash_out);

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

    // Compress the full content with aggressive compression
    size_t compressed_size;
    char* compressed = compress_data_fast(full_content, full_size, &compressed_size);
    free(full_content);
    
    if (!compressed) {
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

// Compute SHA-256 of a file quickly, output hex.
int blake3_file_hex(const char* filepath, char hash_out[65]) {
    struct stat st;
    if (stat(filepath, &st) == -1) return -1;
    size_t size = st.st_size;

    // Prepare header "blob <size>\0" (same as store_blob_from_file)
    char header[64];
    int header_len = snprintf(header, sizeof(header), "blob %zu", size);

    blake3_hasher ctx;
    blake3_hasher_init(&ctx);
    blake3_hasher_update(&ctx, header, header_len + 1);

    FILE* fp = fopen(filepath, "rb");
    if (!fp) return -1;
    unsigned char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        blake3_hasher_update(&ctx, buf, n);
    }
    fclose(fp);
    uint8_t digest2[32];
    blake3_hasher_finalize(&ctx, digest2, 32);
    for (int i = 0; i < 32; ++i) sprintf(hash_out + i*2, "%02x", digest2[i]);
    hash_out[64] = '\0';
    return 0;
}



char* load_object(const char* hash, size_t* size_out, char* type_out) {
    char obj_path[512];
    snprintf(obj_path, sizeof(obj_path), ".avc/objects/%.2s/%s", hash, hash + 2);

    FILE* obj_file = fopen(obj_path, "rb");
    if (!obj_file) return NULL;

    fseek(obj_file, 0, SEEK_END);
    size_t compressed_size = ftell(obj_file);
    fseek(obj_file, 0, SEEK_SET);

    char* compressed_data = malloc(compressed_size);
    if (!compressed_data) {
        fclose(obj_file);
        return NULL;
    }

    if (fread(compressed_data, 1, compressed_size, obj_file) != compressed_size) {
        fclose(obj_file);
        free(compressed_data);
        return NULL;
    }
    fclose(obj_file);

    // AUTO-DETECT and decompress with unified layer
    size_t estimated_size = compressed_size * 20;
    char* decompressed = decompress_data_unified(compressed_data, compressed_size, estimated_size);
    
    // Retry with larger buffer if needed
    if (!decompressed) {
        estimated_size = compressed_size * 50;
        decompressed = decompress_data_unified(compressed_data, compressed_size, estimated_size);
    }
    
    free(compressed_data);
    
    if (!decompressed) {
        return NULL;
    }
    
    size_t actual_size = estimated_size; // Will be corrected by parsing

    // Parse the decompressed content
    char* null_pos = memchr(decompressed, '\0', actual_size);
    if (!null_pos) {
        free(decompressed);
        return NULL;
    }

    sscanf(decompressed, "%s %zu", type_out, size_out);

    size_t content_offset = (null_pos - decompressed) + 1;
    char* content = malloc(*size_out + 1);
    if (!content) {
        free(decompressed);
        return NULL;
    }

    memcpy(content, decompressed + content_offset, *size_out);
    content[*size_out] = '\0';

    free(decompressed);
    return content;
}

// Free memory pool (call periodically)
void free_memory_pool(void) {
    // Nothing to clean up - contexts are managed in compression.c
}

// Reset memory pool (reuse chunks instead of freeing)
void reset_memory_pool(void) {
    // Nothing to reset - we're using simple malloc/free
}
