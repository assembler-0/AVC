//
// Created by Atheria on 6/20/25.
//

#include "index.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "objects.h"
#include "../utils/file_utils.h"

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

// Check if file is in index
int is_file_in_index(const char* filepath) {
    FILE* index = fopen(".avc/index", "r");
    if (!index) {
        return 0; // No index, file not staged
    }

    char line[1024];
    while (fgets(line, sizeof(line), index)) {
        char hash[41], indexed_filepath[256];
        unsigned int mode;

        if (sscanf(line, "%40s %255s %o", hash, indexed_filepath, &mode) == 3) {
            if (strcmp(indexed_filepath, filepath) == 0) {
                fclose(index);
                return 1; // Found in index
            }
        }
    }
    fclose(index);
    return 0; // Not found in index
}

// Remove file from staging area (index)
int remove_file_from_index(const char* filepath) {
    FILE* index = fopen(".avc/index", "r");
    if (!index) {
        fprintf(stderr, "No index found\n");
        return -1;
    }

    // Read all index entries
    char** lines = NULL;
    size_t line_count = 0;
    size_t capacity = 10;
    lines = malloc(capacity * sizeof(char*));
    if (!lines) {
        fprintf(stderr, "Memory allocation failed\n");
        fclose(index);
        return -1;
    }

    char line[1024];
    int found = 0;

    while (fgets(line, sizeof(line), index)) {
        char hash[41], indexed_filepath[256];
        unsigned int mode;

        if (sscanf(line, "%40s %255s %o", hash, indexed_filepath, &mode) == 3) {
            // Skip the file we want to remove
            if (strcmp(indexed_filepath, filepath) == 0) {
                found = 1;
                continue;
            }
        }

        // Grow array if needed
        if (line_count >= capacity) {
            capacity *= 2;
            char** new_lines = realloc(lines, capacity * sizeof(char*));
            if (!new_lines) {
                fprintf(stderr, "Memory reallocation failed\n");
                for (size_t i = 0; i < line_count; i++) {
                    free(lines[i]);
                }
                free(lines);
                fclose(index);
                return -1;
            }
            lines = new_lines;
        }

        // Store the line
        lines[line_count] = malloc(strlen(line) + 1);
        if (!lines[line_count]) {
            fprintf(stderr, "Memory allocation failed\n");
            for (size_t i = 0; i < line_count; i++) {
                free(lines[i]);
            }
            free(lines);
            fclose(index);
            return -1;
        }
        strcpy(lines[line_count], line);
        line_count++;
    }
    fclose(index);

    if (!found) {
        fprintf(stderr, "File '%s' not found in staging area\n", filepath);
        for (size_t i = 0; i < line_count; i++) {
            free(lines[i]);
        }
        free(lines);
        return -1;
    }

    // Rewrite index without the removed file
    index = fopen(".avc/index", "w");
    if (!index) {
        perror("Failed to rewrite index");
        for (size_t i = 0; i < line_count; i++) {
            free(lines[i]);
        }
        free(lines);
        return -1;
    }

    for (size_t i = 0; i < line_count; i++) {
        fputs(lines[i], index);
        free(lines[i]);
    }
    free(lines);
    fclose(index);

    return 0;
}

// Clear the entire index
int clear_index(void) {
    FILE* index = fopen(".avc/index", "w");
    if (!index) {
        perror("Failed to clear index");
        return -1;
    }
    fclose(index);
    return 0;
}