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
#include <zlib.h>
#include "hash.h"
#include "objects.h"
#include <openssl/sha.h>
static int flush_to_file(z_stream* zs, FILE* fp, unsigned char* buf, int flush_mode) {
    int ret;
    do {
        zs->avail_out = 8192;
        zs->next_out = buf;
        ret = deflate(zs, flush_mode);
        size_t have = 8192 - zs->avail_out;
        if (have && fwrite(buf, 1, have, fp) != have) {
            ret = Z_ERRNO;
        }
    } while (zs->avail_out == 0);
    return ret;
}
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

    // Set up SHA-256 context
    SHA256_CTX sha_ctx;
    SHA256_Init(&sha_ctx);
    SHA256_Update(&sha_ctx, header, header_len + 1); // include null terminator

    // Prepare temp file inside .avc/objects for compressed data
    if (mkdir(".avc/objects/tmp", 0755) == -1 && errno != EEXIST) {
        perror("mkdir");
        return -1;
    }
    char tmp_template[] = ".avc/objects/tmp/objXXXXXX";
    int tmp_fd = mkstemp(tmp_template);
    if (tmp_fd == -1) {
        perror("mkstemp");
        return -1;
    }
    FILE* tmp_fp = fdopen(tmp_fd, "wb");
    if (!tmp_fp) {
        close(tmp_fd);
        perror("fdopen");
        return -1;
    }

    // Initialize zlib stream for compression
    z_stream strm;
    memset(&strm, 0, sizeof(strm));
    if (deflateInit(&strm, Z_DEFAULT_COMPRESSION) != Z_OK) {
        fclose(tmp_fp);
        remove(tmp_template);
        return -1;
    }

    // Buffer for compressed output
    unsigned char out_buf[8192];

    // Helper to drain zlib output into file


    // Feed header (with null terminator) to compressor
    strm.avail_in = header_len + 1;
    strm.next_in = (unsigned char*)header;
    if (flush_to_file(&strm, tmp_fp, out_buf, Z_NO_FLUSH) != Z_OK) {
        deflateEnd(&strm); fclose(tmp_fp); remove(tmp_template); return -1;
    }

    // Open the source file
    FILE* src = fopen(filepath, "rb");
    if (!src) {
        perror("fopen");
        deflateEnd(&strm); fclose(tmp_fp); remove(tmp_template); return -1;
    }

    unsigned char in_buf[8192];
    size_t read_bytes;
    while ((read_bytes = fread(in_buf, 1, sizeof(in_buf), src)) > 0) {
        SHA256_Update(&sha_ctx, in_buf, read_bytes);
        strm.avail_in = read_bytes;
        strm.next_in = in_buf;
        if (flush_to_file(&strm, tmp_fp, out_buf, Z_NO_FLUSH) != Z_OK) {
            fclose(src); deflateEnd(&strm); fclose(tmp_fp); remove(tmp_template); return -1;
        }
    }
    fclose(src);

    // Finish compression
    if (flush_to_file(&strm, tmp_fp, out_buf, Z_FINISH) != Z_STREAM_END) {
        deflateEnd(&strm); fclose(tmp_fp); remove(tmp_template); return -1;
    }
    deflateEnd(&strm);
    fflush(tmp_fp);
    fclose(tmp_fp);

    // Finalize hash
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256_Final(digest, &sha_ctx);
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
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
            perror("mkdir"); remove(tmp_template); return -1;
        }
    }

    // If object already exists, delete temp and return
    if (access(obj_path, F_OK) == 0) {
        remove(tmp_template);
        return 0;
    }

    // Move temp file into final place
    if (rename(tmp_template, obj_path) != 0) {
        perror("rename"); remove(tmp_template); return -1;
    }

    return 0;
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

// Compute SHA-256 of a file quickly, output hex.
int sha256_file_hex(const char* filepath, char hash_out[65]) {
    FILE* fp = fopen(filepath, "rb");
    if (!fp) return -1;
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    unsigned char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        SHA256_Update(&ctx, buf, n);
    }
    fclose(fp);
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256_Final(digest, &ctx);
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) sprintf(hash_out + i*2, "%02x", digest[i]);
    hash_out[64] = '\0';
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