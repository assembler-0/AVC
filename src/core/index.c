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

// ---------------- In-memory index (transactional) ----------------

typedef struct {
    char* hash;
    char* path;
    unsigned int mode;
} IndexEntry;

static IndexEntry* idx_entries = NULL;
static size_t idx_count = 0;
static size_t idx_capacity = 0;
static int idx_loaded = 0;   // 0 = on-disk mode; 1 = in-memory transactional mode

static void index_entries_free(void) {
    for (size_t i = 0; i < idx_count; ++i) {
        free(idx_entries[i].hash);
        free(idx_entries[i].path);
    }
    free(idx_entries);
    idx_entries = NULL;
    idx_count = 0;
    idx_capacity = 0;
}

int upsert_file_in_index(const char* filepath, const char* new_hash, unsigned int mode, int* unchanged_out) {
    if (unchanged_out) *unchanged_out = 0;

    // Open existing index (it may not exist yet)
    FILE* src = fopen(".avc/index", "r");
    FILE* dst = fopen(".avc/index.tmp", "w");
    if (!dst) {
        if (src) fclose(src);
        return -1;
    }

    char line[1024];
    int found = 0;

    if (src) {
        while (fgets(line, sizeof(line), src)) {
            char hash[65], indexed_filepath[256];
            unsigned int existing_mode;
            if (sscanf(line, "%64s %255s %o", hash, indexed_filepath, &existing_mode) == 3 &&
                strcmp(indexed_filepath, filepath) == 0) {
                found = 1;
                if (strcmp(hash, new_hash) == 0) {
                    // Unchanged – keep original line as-is and mark unchanged.
                    if (unchanged_out) *unchanged_out = 1;
                    fputs(line, dst);
                }
                // If content changed we simply skip the old line (it will be replaced later)
                continue;
            }
            // Preserve all unrelated entries.
            fputs(line, dst);
        }
        fclose(src);
    }

    // If file changed or was not present, append the fresh entry.
    if (!found || (found && (!unchanged_out || *unchanged_out == 0))) {
        fprintf(dst, "%s %s %o\n", new_hash, filepath, mode);
    }

    fclose(dst);

    // Atomically replace index.
    if (rename(".avc/index.tmp", ".avc/index") != 0) {
        remove(".avc/index.tmp");
        return -1;
    }

    return 0;
}

int index_load(void) {
    if (idx_loaded) return 0; // already
    FILE* f = fopen(".avc/index", "r");
    if (!f) {
        // treat missing file as empty index
        idx_loaded = 1;
        return 0;
    }
    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        char hash[65], path[256];
        unsigned int mode;
        if (sscanf(line, "%64s %255s %o", hash, path, &mode) != 3) continue;
        if (idx_count >= idx_capacity) {
            idx_capacity = idx_capacity ? idx_capacity * 2 : 32;
            idx_entries = realloc(idx_entries, idx_capacity * sizeof(IndexEntry));
            if (!idx_entries) { fclose(f); return -1; }
        }
        idx_entries[idx_count].hash = strdup2(hash);
        idx_entries[idx_count].path = strdup2(path);
        idx_entries[idx_count].mode = mode;
        idx_count++;
    }
    fclose(f);
    idx_loaded = 1;
    return 0;
}

int index_upsert_entry(const char* filepath, const char* hash, unsigned int mode, int* unchanged_out) {
    if (!idx_loaded) {
        // fall back to streaming update if not loaded
        return upsert_file_in_index(filepath, hash, mode, unchanged_out);
    }
    if (unchanged_out) *unchanged_out = 0;
    for (size_t i = 0; i < idx_count; ++i) {
        if (strcmp(idx_entries[i].path, filepath) == 0) {
            if (strcmp(idx_entries[i].hash, hash) == 0) {
                if (unchanged_out) *unchanged_out = 1;
                return 0; // nothing to do
            }
            // update existing entry
            free(idx_entries[i].hash);
            idx_entries[i].hash = strdup2(hash);
            idx_entries[i].mode = mode;
            return 0;
        }
    }
    // append new entry
    if (idx_count >= idx_capacity) {
        idx_capacity = idx_capacity ? idx_capacity * 2 : 32;
        idx_entries = realloc(idx_entries, idx_capacity * sizeof(IndexEntry));
        if (!idx_entries) return -1;
    }
    idx_entries[idx_count].hash = strdup2(hash);
    idx_entries[idx_count].path = strdup2(filepath);
    idx_entries[idx_count].mode = mode;
    idx_count++;
    return 0;
}

int index_commit(void) {
    if (!idx_loaded) return 0; // nothing
    FILE* f = fopen(".avc/index.tmp", "w");
    if (!f) return -1;
    for (size_t i = 0; i < idx_count; ++i) {
        fprintf(f, "%s %s %o\n", idx_entries[i].hash, idx_entries[i].path, idx_entries[i].mode);
    }
    fclose(f);
    if (rename(".avc/index.tmp", ".avc/index") != 0) {
        remove(".avc/index.tmp");
        return -1;
    }
    index_entries_free();
    idx_loaded = 0;
    return 0;
}


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

// Upsert file in index: 
//  - If an entry for `filepath` exists with identical hash, return 1 (unchanged)
//  - Otherwise rewrite the index to contain the new (hash, mode) entry and return 0 (added/updated)
//  - On error return -1


// Remove file from index (legacy helper kept for other commands)
int remove_file_from_index_if_exists(const char* filepath) {
    // Open the existing index for reading. If it doesn't exist, nothing to do.
    FILE* src = fopen(".avc/index", "r");
    if (!src) {
        return 0;
    }

    // Open a temporary file that will hold the rewritten index.
    FILE* dst = fopen(".avc/index.tmp", "w");
    if (!dst) {
        fclose(src);
        return -1;
    }

    char line[1024];
    int found = 0;

    while (fgets(line, sizeof(line), src)) {
        char hash[65], indexed_filepath[256];
        unsigned int mode;

        // Parse the line. If it matches the file we want to remove, skip it.
        if (sscanf(line, "%64s %255s %o", hash, indexed_filepath, &mode) == 3 &&
            strcmp(indexed_filepath, filepath) == 0) {
            found = 1;
            continue; // Skip this entry
        }

        // Otherwise, write the line unchanged.
        fputs(line, dst);
    }

    fclose(src);
    fclose(dst);

    // Atomically replace the old index with the new one.
    if (rename(".avc/index.tmp", ".avc/index") != 0) {
        // If the rename failed, clean up the temporary file and report an error.
        remove(".avc/index.tmp");
        return -1;
    }

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

    int unchanged = 0;
    if (index_upsert_entry(filepath, hash, (unsigned int)st.st_mode, &unchanged) == -1) {
        free(content);
        fprintf(stderr, "Failed to update index for file: %s\n", filepath);
        return -1;
    }

    if (unchanged) {
        printf("Already staged and unchanged: %s\n", filepath);
    } else {
        printf("Added to staging: %s\n", filepath);
    }

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