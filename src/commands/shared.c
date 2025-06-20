#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "commands.h"
#include <string.h>
#include <errno.h>
#include <openssl/sha.h>
int check_repo() {
    struct stat st;
    if (stat(".avc", &st) == -1) {
        fprintf(stderr, "Not an avc repository (no .avc directory found)\n");
        return -1;
    }
    return 0;
}
// src/commands/shared.

// Read file content
char* read_file(const char* filepath, size_t* size) {
    FILE* file = fopen(filepath, "rb");
    if (!file) {
        perror("fopen");
        return NULL;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    *size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Allocate buffer
    char* content = malloc(*size + 1);
    if (!content) {
        fclose(file);
        return NULL;
    }

    // Read content
    fread(content, 1, *size, file);
    content[*size] = '\0';

    fclose(file);
    return content;
}

// SHA-1 hash function
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
int store_object(const char* type, const char* content, size_t size, char* hash_out) {
    // Generate hash
    sha1_hash_object(type, content, size, hash_out);

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

    // Store object with Git-style format
    FILE* obj_file = fopen(obj_path, "wb");
    if (!obj_file) {
        perror("Failed to create object file");
        return -1;
    }

    // Write header
    fprintf(obj_file, "%s %zu", type, size);
    fputc('\0', obj_file);  // Null separator

    // Write content
    fwrite(content, 1, size, obj_file);
    fclose(obj_file);

    return 0;
}

// Load object content
char* load_object(const char* hash, size_t* size_out, char* type_out) {
    // Create object path
    char obj_path[512];
    snprintf(obj_path, sizeof(obj_path), ".avc/objects/%.2s/%s", hash, hash + 2);

    // Read object file
    FILE* obj_file = fopen(obj_path, "rb");
    if (!obj_file) {
        return NULL;
    }

    // Read header to find content size
    char header[256];
    int i = 0;
    int c;
    while ((c = fgetc(obj_file)) != '\0' && c != EOF && i < 255) {
        header[i++] = c;
    }
    header[i] = '\0';

    // Parse header: "type size"
    char* space = strchr(header, ' ');
    if (!space) {
        fclose(obj_file);
        return NULL;
    }

    *space = '\0';
    strcpy(type_out, header);
    size_t content_size = atoll(space + 1);

    // Read content
    char* content = malloc(content_size + 1);
    if (!content) {
        fclose(obj_file);
        return NULL;
    }

    fread(content, 1, content_size, obj_file);
    content[content_size] = '\0';
    fclose(obj_file);

    *size_out = content_size;
    return content;
}