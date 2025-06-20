//
// Created by Atheria on 6/20/25.
//

#include "index.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "objects.h"
#include "file_utils.h"
#include <dirent.h>

// Check if file is already in index with same hash
int is_file_unchanged_in_index(const char* filepath, const char* new_hash) {
    FILE* index = fopen(".avc/index", "r");
    if (!index) {
        return 0; // No index, file not staged
    }

    char line[1024];
    while (fgets(line, sizeof(line), index)) {
        char hash[65], indexed_filepath[256]; // 64 chars + null terminator
        unsigned int mode;

        if (sscanf(line, "%64s %255s %o", hash, indexed_filepath, &mode) == 3) {
            if (strcmp(indexed_filepath, filepath) == 0) {
                fclose(index);
                return (strcmp(hash, new_hash) == 0); // Return 1 if unchanged, 0 if changed
            }
        }
    }
    fclose(index);
    return 0; // Not found in index
}

// Remove file from index (if it exists)
int remove_file_from_index_if_exists(const char* filepath) {
    FILE* index = fopen(".avc/index", "r");
    if (!index) {
        return 0; // No index file
    }

    // Read all lines except the one we want to remove
    char** lines = NULL;
    size_t line_count = 0;
    size_t capacity = 10;
    lines = malloc(capacity * sizeof(char*));
    if (!lines) {
        fclose(index);
        return -1;
    }

    char line[1024];
    int found = 0;

    while (fgets(line, sizeof(line), index)) {
        char hash[65], indexed_filepath[256];
        unsigned int mode;

        if (sscanf(line, "%64s %255s %o", hash, indexed_filepath, &mode) == 3) {
            if (strcmp(indexed_filepath, filepath) == 0) {
                found = 1;
                continue; // Skip this line
            }
        }

        // Grow array if needed
        if (line_count >= capacity) {
            capacity *= 2;
            char** new_lines = realloc(lines, capacity * sizeof(char*));
            if (!new_lines) {
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

    // Rewrite index
    index = fopen(".avc/index", "w");
    if (!index) {
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

    return found;
}

// Add file to staging area
int add_file_to_index(const char* filepath) {
    struct stat st;
    if (stat(filepath, &st) == -1) {
        fprintf(stderr, "File not found: %s\n", filepath);
        return -1;
    }

    if (S_ISDIR(st.st_mode)) {
        DIR* dir = opendir(filepath);
        if (!dir) {
            fprintf(stderr, "Failed to open directory: %s\n", filepath);
            return -1;
        }
        struct dirent* entry;
        int result = 0;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            char child_path[1024];
            snprintf(child_path, sizeof(child_path), "%s/%s", filepath, entry->d_name);
            if (add_file_to_index(child_path) == -1) {
                result = -1;
            }
        }
        closedir(dir);
        return result;
    }

    // Read file content
    size_t size;
    char* content = read_file(filepath, &size);
    if (!content) {
        fprintf(stderr, "Failed to read file: %s\n", filepath);
        return -1;
    }

    char hash[65]; // 64 chars + null terminator for SHA-256
    if (store_object("blob", content, size, hash) == -1) {
        free(content);
        fprintf(stderr, "Failed to store object for file: %s\n", filepath);
        return -1;
    }

    // Check if file is already staged with same content
    if (is_file_unchanged_in_index(filepath, hash)) {
        printf("Already staged and unchanged: %s\n", filepath);
        free(content);
        return 0;
    }

    // Remove file from index if it already exists (to update it)
    remove_file_from_index_if_exists(filepath);

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

    printf("Added to staging: %s\n", filepath);

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
        char hash[65], indexed_filepath[256]; // Updated to 64 chars
        unsigned int mode;

        if (sscanf(line, "%64s %255s %o", hash, indexed_filepath, &mode) == 3) {
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
        char hash[65], indexed_filepath[256]; // Updated to 64 chars
        unsigned int mode;

        if (sscanf(line, "%64s %255s %o", hash, indexed_filepath, &mode) == 3) {
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