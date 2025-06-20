// src/commands/add.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include "commands.h"

// Check if we're in a repository
int check_repo();

// Read file content
char* read_file(const char* filepath, size_t* size);

int store_object(const char* type, const char* content, size_t size, char* hash_out);
// Add file to staging area
int add_file_to_index(const char* filepath) {
    // Check if file exists
    struct stat st;
    if (stat(filepath, &st) == -1) {
        fprintf(stderr, "File not found: %s\n", filepath);
        return -1;
    }

    // Read file content
    size_t size;
    char* content = read_file(filepath, &size);
    if (!content) {
        fprintf(stderr, "Failed to read file: %s\n", filepath);
        return -1;
    }

    char hash[41];
    if (store_object("blob", content, size, hash) == -1) {
        free(content);
        fprintf(stderr, "Failed to store object for file: %s\n", filepath);
        return -1;
    }

    // Add entry to index
    FILE* index = fopen(".avc/index", "a");
    if (!index) {
        free(content);
        perror("Failed to open index");
        return -1;
    }

    // Write index entry: hash filepath mode
    fprintf(index, "%s %s %o\n", hash, filepath, (unsigned int)st.st_mode);
    fclose(index);

    printf("Added '%s' to staging area (hash: %.8s)\n", filepath, hash);

    free(content);
    return 0;
}

int cmd_add(int argc, char* argv[]) {
    if (check_repo() == -1) {
        return 1;
    }

    if (argc < 2) {
        fprintf(stderr, "Usage: avc add <file>\n");
        return 1;
    }

    // Add each file specified
    for (int i = 1; i < argc; i++) {
        if (add_file_to_index(argv[i]) == -1) {
            fprintf(stderr, "Failed to add file: %s\n", argv[i]);
            return 1;
        }
    }

    return 0;
}