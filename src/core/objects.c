#define _XOPEN_SOURCE 700
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
#include <blake3.h>
#include <libdeflate.h>

// Compression constants
#define AVC_COMPRESSION_LEVEL_MAX 12
#define AVC_COMPRESSION_LEVEL_BALANCED 6

// Global flag to disable compression for maximum speed
static int disable_compression = 0;

// Function to enable/disable compression
void set_compression_enabled(int enabled) {
    disable_compression = !enabled;
}

// Wrapper functions for libdeflate compression
char* compress_data_libdeflate(const char* data, size_t size, size_t* compressed_size, int level) {
    if (disable_compression) {
        // Return uncompressed data
        char* result = malloc(size);
        if (!result) return NULL;
        memcpy(result, data, size);
        *compressed_size = size;
        return result;
    }

    struct libdeflate_compressor* compressor = libdeflate_alloc_compressor(level);
    if (!compressor) return NULL;

    // Estimate compressed size (worst case)
    size_t max_compressed_size = libdeflate_zlib_compress_bound(compressor, size);
    char* compressed = malloc(max_compressed_size);
    if (!compressed) {
        libdeflate_free_compressor(compressor);
        return NULL;
    }

    *compressed_size = libdeflate_zlib_compress(compressor, data, size, compressed, max_compressed_size);
    libdeflate_free_compressor(compressor);

    if (*compressed_size == 0) {
        free(compressed);
        return NULL;
    }

    // Reallocate to actual size
    char* result = realloc(compressed, *compressed_size);
    return result ? result : compressed;
}

char* decompress_data_libdeflate(const char* compressed_data, size_t compressed_size, size_t expected_size) {
    struct libdeflate_decompressor* decompressor = libdeflate_alloc_decompressor();
    if (!decompressor) return NULL;

    char* decompressed = malloc(expected_size);
    if (!decompressed) {
        libdeflate_free_decompressor(decompressor);
        return NULL;
    }

    size_t actual_size;
    enum libdeflate_result result = libdeflate_zlib_decompress(decompressor, 
                                                              compressed_data, compressed_size,
                                                              decompressed, expected_size, &actual_size);
    libdeflate_free_decompressor(decompressor);

    if (result != LIBDEFLATE_SUCCESS) {
        free(decompressed);
        return NULL;
    }

    // Reallocate to actual size
    char* result_ptr = realloc(decompressed, actual_size);
    return result_ptr ? result_ptr : decompressed;
}

// Wrapper functions for backward compatibility
char* compress_data(const char* data, size_t size, size_t* compressed_size) {
    return compress_data_libdeflate(data, size, compressed_size, AVC_COMPRESSION_LEVEL_MAX);
}

char* compress_data_aggressive(const char* data, size_t size, size_t* compressed_size) {
    return compress_data_libdeflate(data, size, compressed_size, AVC_COMPRESSION_LEVEL_MAX);
}

char* decompress_data(const char* compressed_data, size_t compressed_size, size_t expected_size) {
    return decompress_data_libdeflate(compressed_data, compressed_size, expected_size);
}

// Fast compression wrapper
char* compress_data_fast(const char* data, size_t size, size_t* compressed_size) {
    return compress_data_libdeflate(data, size, compressed_size, AVC_COMPRESSION_LEVEL_MAX);
}

// Wrapper functions for utility functions from compression module
void show_compression_stats(size_t original_size, size_t compressed_size, const char* type) {
    if (compressed_size > 0) {
        double ratio = (double)compressed_size / original_size * 100.0;
        printf("[%s] %zu -> %zu bytes (%.1f%%)\n", type, original_size, compressed_size, ratio);
    }
}

int is_likely_compressed(const char* data, size_t size) {
    if (size < 4) return 0;
    
    // Check for common compression signatures
    unsigned char* bytes = (unsigned char*)data;
    
    // zlib/gzip signature
    if (bytes[0] == 0x1f && bytes[1] == 0x8b) return 1;
    
    // zlib header
    if ((bytes[0] & 0x0f) == 0x08 && (bytes[0] & 0xf0) <= 0x70) return 1;
    
    // Check for low entropy (simple heuristic)
    int byte_counts[256] = {0};
    for (size_t i = 0; i < size && i < 1024; i++) {
        byte_counts[bytes[i]]++;
    }
    
    int unique_bytes = 0;
    for (int i = 0; i < 256; i++) {
        if (byte_counts[i] > 0) unique_bytes++;
    }
    
    // If very few unique bytes, likely already compressed
    return unique_bytes < 32;
}

int flush_to_file_libdeflate(struct libdeflate_compressor* compressor,
                                   FILE* fp, unsigned char* buf,
                                   const unsigned char* data, size_t data_size) {
    size_t compressed_size = libdeflate_zlib_compress(compressor, data, data_size, buf, 8192);
    if (compressed_size == 0) {
        return -1;
    }
    if (fwrite(buf, 1, compressed_size, fp) != compressed_size) {
        return -1;
    }
    return 0;
}

// Stream a file into a compressed blob object while computing its SHA-256.
int store_blob_from_file(const char* filepath, char* hash_out) {
    struct stat st;
    if (stat(filepath, &st) == -1) {
        perror("stat");
        return -1;
    }
    size_t size = st.st_size;

    // Prepare header "blob <size>\0"
    char header[64];
    int header_len = snprintf(header, sizeof(header), "blob %zu", size);

    // Set up BLAKE3 hasher (unkeyed)
    blake3_hasher hasher;
    blake3_hasher_init(&hasher);
    blake3_hasher_update(&hasher, header, header_len + 1); // include null terminator

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

    // Update hash with file content
    blake3_hasher_update(&hasher, file_content, size);

    // Finalize hash
    uint8_t digest[32];
    blake3_hasher_finalize(&hasher, digest, 32);
    for (int i = 0; i < 32; ++i) {
        sprintf(hash_out + i * 2, "%02x", digest[i]);
    }
    hash_out[64] = '\0';

    // Determine final object path (.avc/objects/ab/cd...)
    char obj_dir[512], obj_path[512];
    snprintf(obj_dir, sizeof(obj_dir), ".avc/objects/%.2s", hash_out);
    snprintf(obj_path, sizeof(obj_path), "%s/%s", obj_dir, hash_out + 2);

    struct stat st_dir;
    if (stat(obj_dir, &st_dir) == -1) {
        if (mkdir(obj_dir, 0755) == -1 && errno != EEXIST) {
            perror("mkdir"); 
            free(file_content);
            return -1;
        }
    }

    // If object already exists, delete temp and return
    if (access(obj_path, F_OK) == 0) {
        free(file_content);
        return 0;
    }

    // Create full object content (header + content) - same as store_object
    size_t full_size = header_len + 1 + size;
    char* full_content = malloc(full_size);
    if (!full_content) {
        free(file_content);
        return -1;
    }
    
    memcpy(full_content, header, header_len);
    full_content[header_len] = '\0';
    memcpy(full_content + header_len + 1, file_content, size);
    free(file_content);

    // Compress the full content with the same method as store_object
    size_t compressed_size;
    char* compressed = compress_data_fast(full_content, full_size, &compressed_size);
    free(full_content);
    
    if (!compressed) {
        fprintf(stderr, "Failed to compress blob object\n");
        return -1;
    }

    // Store compressed object
    FILE* obj_file = fopen(obj_path, "wb");
    if (!obj_file) {
        perror("Failed to create blob object file");
        free(compressed);
        return -1;
    }

    fwrite(compressed, 1, compressed_size, obj_file);
    fclose(obj_file);
    free(compressed);

    return 0;
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

// Load object content - OPTIMIZED VERSION
char* load_object(const char* hash, size_t* size_out, char* type_out) {
    // Create object path
    char obj_path[512];
    snprintf(obj_path, sizeof(obj_path), ".avc/objects/%.2s/%s", hash, hash + 2);

    // Read compressed object file
    FILE* obj_file = fopen(obj_path, "rb");
    if (!obj_file) {
        return NULL;
    }

    // Get file size
    fseek(obj_file, 0, SEEK_END);
    size_t compressed_size = ftell(obj_file);
    fseek(obj_file, 0, SEEK_SET);

    // Sanity check for reasonable file size (max 100MB)
    if (compressed_size > 100 * 1024 * 1024) {
        fclose(obj_file);
        return NULL;
    }

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

    // OPTIMIZED: Better compression detection and single-pass decompression
    char* decompressed = NULL;
    size_t actual_decompressed_size = 0;

    // First, try to detect if it's already uncompressed (raw bytes)
    if (compressed_size > 1024 * 1024) { // Large objects
        char* space = memchr(compressed_data, ' ', compressed_size);
        if (space) {
            char* null_pos = memchr(compressed_data, '\0', compressed_size);
            if (null_pos && null_pos > space) {
                // Try to parse as raw bytes first
                size_t type_len = space - compressed_data;
                if (type_len < 16) {
                    memcpy(type_out, compressed_data, type_len);
                    type_out[type_len] = '\0';
                    
                    size_t declared_size = atoll(space + 1);
                    char* content_start = null_pos + 1;
                    size_t header_size = content_start - compressed_data;
                    size_t available_content_size = compressed_size - header_size;
                    
                    if (available_content_size >= declared_size) {
                        char* content = malloc(declared_size + 1);
                        if (content) {
                            memcpy(content, content_start, declared_size);
                            content[declared_size] = '\0';
                            *size_out = declared_size;
                            free(compressed_data);
                            return content;
                        }
                    }
                }
            }
        }
    }

    // OPTIMIZED: Single-pass decompression with better size estimation
    size_t estimated_size = compressed_size * 6; // Better initial estimate
    if (estimated_size > 50 * 1024 * 1024) { // Cap at 50MB
        estimated_size = 50 * 1024 * 1024;
    }

    // Try deflate decompression first (for tree/commit objects)
    char* temp_buffer = malloc(estimated_size + 1);
    if (temp_buffer) {
        struct libdeflate_decompressor* decompressor = libdeflate_alloc_decompressor();
        if (decompressor) {
            size_t actual_size;
            enum libdeflate_result result = libdeflate_deflate_decompress(decompressor,
                compressed_data, compressed_size, temp_buffer, estimated_size, &actual_size);
            libdeflate_free_decompressor(decompressor);
            
            if (result == LIBDEFLATE_SUCCESS) {
                decompressed = temp_buffer;
                actual_decompressed_size = actual_size;
            } else if (result == LIBDEFLATE_INSUFFICIENT_SPACE) {
                // Try with larger buffer
                free(temp_buffer);
                estimated_size *= 2;
                if (estimated_size <= 50 * 1024 * 1024) {
                    temp_buffer = malloc(estimated_size + 1);
                    if (temp_buffer) {
                        struct libdeflate_decompressor* decompressor2 = libdeflate_alloc_decompressor();
                        if (decompressor2) {
                            enum libdeflate_result result2 = libdeflate_deflate_decompress(decompressor2,
                                compressed_data, compressed_size, temp_buffer, estimated_size, &actual_size);
                            libdeflate_free_decompressor(decompressor2);
                            
                            if (result2 == LIBDEFLATE_SUCCESS) {
                                decompressed = temp_buffer;
                                actual_decompressed_size = actual_size;
                            } else {
                                free(temp_buffer);
                            }
                        } else {
                            free(temp_buffer);
                        }
                    }
                }
            } else {
                free(temp_buffer);
            }
        } else {
            free(temp_buffer);
        }
    }

    // If deflate failed, try libdeflate decompression (for blob objects)
    if (!decompressed) {
        estimated_size = compressed_size * 6;
        if (estimated_size > 50 * 1024 * 1024) {
            estimated_size = 50 * 1024 * 1024;
        }
        
        temp_buffer = malloc(estimated_size + 1);
        if (temp_buffer) {
            struct libdeflate_decompressor* decompressor = libdeflate_alloc_decompressor();
            if (decompressor) {
                size_t actual_size;
                enum libdeflate_result result = libdeflate_zlib_decompress(decompressor,
                    compressed_data, compressed_size, temp_buffer, estimated_size, &actual_size);
                libdeflate_free_decompressor(decompressor);
                
                if (result == LIBDEFLATE_SUCCESS) {
                    decompressed = temp_buffer;
                    actual_decompressed_size = actual_size;
                } else if (result == LIBDEFLATE_BAD_DATA) {
                    // Data was NOT compressed – treat original bytes as the object
                    decompressed = compressed_data;
                    actual_decompressed_size = compressed_size;
                    compressed_data = NULL; // ownership transferred
                } else if (result == LIBDEFLATE_INSUFFICIENT_SPACE) {
                    // Try with larger buffer
                    free(temp_buffer);
                    estimated_size *= 2;
                    if (estimated_size <= 50 * 1024 * 1024) {
                        temp_buffer = malloc(estimated_size + 1);
                        if (temp_buffer) {
                            struct libdeflate_decompressor* decompressor2 = libdeflate_alloc_decompressor();
                            if (decompressor2) {
                                enum libdeflate_result result2 = libdeflate_zlib_decompress(decompressor2,
                                    compressed_data, compressed_size, temp_buffer, estimated_size, &actual_size);
                                libdeflate_free_decompressor(decompressor2);
                                
                                if (result2 == LIBDEFLATE_SUCCESS) {
                                    decompressed = temp_buffer;
                                    actual_decompressed_size = actual_size;
                                } else {
                                    free(temp_buffer);
                                }
                            } else {
                                free(temp_buffer);
                            }
                        }
                    }
                } else {
                    free(temp_buffer);
                }
            } else {
                free(temp_buffer);
            }
        }
    }

    if (compressed_data) free(compressed_data);
    if (!decompressed) {
        return NULL;
    }

    // Sanity check for reasonable decompressed size (max 100MB)
    if (actual_decompressed_size > 100 * 1024 * 1024) {
        free(decompressed);
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

    // Sanity check for declared size
    if (declared_size > 100 * 1024 * 1024) { // Max 100MB content
        free(decompressed);
        return NULL;
    }

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

// Calculate and display compression statistics


// Fast compression with size optimization

// libdeflate-based streaming compression helper

// Memory pool for small objects to reduce fragmentation
#define MEMORY_POOL_SIZE 1024
#define MEMORY_POOL_CHUNK_SIZE 4096

typedef struct MemoryChunk {
    char data[MEMORY_POOL_CHUNK_SIZE];
    size_t used;
    struct MemoryChunk* next;
} MemoryChunk;

static MemoryChunk* memory_pool = NULL;

// Allocate from memory pool for small objects
static char* pool_alloc(size_t size) {
    if (size > MEMORY_POOL_CHUNK_SIZE / 2) {
        return malloc(size); // Too large for pool
    }
    
    if (!memory_pool) {
        memory_pool = malloc(sizeof(MemoryChunk));
        if (!memory_pool) return malloc(size);
        memory_pool->used = 0;
        memory_pool->next = NULL;
    }
    
    // Find chunk with enough space
    MemoryChunk* chunk = memory_pool;
    while (chunk) {
        if (chunk->used + size <= MEMORY_POOL_CHUNK_SIZE) {
            char* ptr = chunk->data + chunk->used;
            chunk->used += size;
            return ptr;
        }
        if (!chunk->next) {
            // Create new chunk
            chunk->next = malloc(sizeof(MemoryChunk));
            if (!chunk->next) return malloc(size);
            chunk->next->used = 0;
            chunk->next->next = NULL;
        }
        chunk = chunk->next;
    }
    
    return malloc(size); // Fallback
}

// Free memory pool (call periodically)
void free_memory_pool(void) {
    MemoryChunk* chunk = memory_pool;
    while (chunk) {
        MemoryChunk* next = chunk->next;
        free(chunk);
        chunk = next;
    }
    memory_pool = NULL;
}
