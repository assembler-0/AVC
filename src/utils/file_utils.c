#include "file_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <libgen.h>
#include <errno.h>

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

// Create directory recursively (helper function)
static int create_directories(const char* path) {
    char* path_copy = strdup(path);
    if (!path_copy) {
        return -1;
    }

    char* dir_path = dirname(path_copy);

    // Check if directory already exists
    struct stat st;
    if (stat(dir_path, &st) == 0) {
        free(path_copy);
        return 0; // Directory exists
    }

    // Create parent directories recursively
    if (create_directories(dir_path) == -1) {
        free(path_copy);
        return -1;
    }

    // Create this directory
    if (mkdir(dir_path, 0755) == -1 && errno != EEXIST) {
        perror("mkdir");
        free(path_copy);
        return -1;
    }

    free(path_copy);
    return 0;
}

int write_file(const char* filepath, const char* content, size_t size) {
    // Create directory structure if it doesn't exist
    char* filepath_copy = strdup(filepath);
    if (!filepath_copy) {
        fprintf(stderr, "Memory allocation failed\n");
        return -1;
    }

    char* dir_path = dirname(filepath_copy);
    struct stat st;
    if (stat(dir_path, &st) == -1) {
        if (create_directories(filepath) == -1) {
            fprintf(stderr, "Failed to create directory structure for: %s\n", filepath);
            free(filepath_copy);
            return -1;
        }
    }
    free(filepath_copy);

    // Open file for writing
    FILE* file = fopen(filepath, "wb");
    if (!file) {
        perror("Failed to open file for writing");
        return -1;
    }

    // Write content
    size_t written = fwrite(content, 1, size, file);
    fclose(file);

    if (written != size) {
        fprintf(stderr, "Failed to write complete content to file: %s\n", filepath);
        return -1;
    }

    return 0;
}