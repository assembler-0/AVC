//
// Created by Atheria on 6/20/25.
//

#include "index.h"
#include "fast_index.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "objects.h"
#include "file_utils.h"
#include <dirent.h>

// ---------------- In-memory index (transactional) ----------------

// Path helpers ----------------------------------------------------
// canonical_path: returns a pointer inside the input string that skips an optional "./" prefix.
static const char* canonical_path(const char* p) {
    if (p && strncmp(p, "./", 2) == 0) {
        return p + 2;
    }
    return p;
}

// paths_equal: compares two paths while ignoring an optional leading "./" in either operand.
static int paths_equal(const char* a, const char* b) {
    a = canonical_path(a);
    b = canonical_path(b);
    return a && b && strcmp(a, b) == 0;
}

// ---------------- In-memory index (transactional) ----------------

typedef struct {
    char* hash;
    char* path;
    unsigned int mode;
} IndexEntry;

// Fast hash table index (replaces linear array)
static fast_index_t* fast_idx = NULL;
static int idx_loaded = 0;



// Public helper: return hash string for given path if present in loaded index, else NULL
const char* index_get_hash(const char* filepath) {
    if (!idx_loaded || !fast_idx) return NULL;
    return fast_index_get_hash(fast_idx, filepath);
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
                paths_equal(indexed_filepath, filepath)) {
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
        fprintf(dst, "%s %s %o\n", new_hash, canonical_path(filepath), mode);
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
    if (idx_loaded) return 0;
    
    if (!fast_idx) {
        fast_idx = fast_index_create();
        if (!fast_idx) return -1;
    }
    
    int result = fast_index_load(fast_idx);
    if (result == 0) {
        idx_loaded = 1;
    }
    
    return result;
}

int index_upsert_entry(const char* filepath, const char* hash, unsigned int mode, int* unchanged_out) {
    if (!idx_loaded || !fast_idx) {
        return upsert_file_in_index(filepath, hash, mode, unchanged_out);
    }
    
    if (unchanged_out) *unchanged_out = 0;
    
    // Check if entry exists and is unchanged
    const char* old_hash = fast_index_get_hash(fast_idx, filepath);
    if (old_hash && strcmp(old_hash, hash) == 0) {
        if (unchanged_out) *unchanged_out = 1;
        return 0;
    }
    
    // Insert or update entry
    return fast_index_set(fast_idx, filepath, hash, mode);
}

int index_commit(void) {
    if (!idx_loaded || !fast_idx) return 0;
    
    int result = fast_index_commit(fast_idx);
    
    // Clean up after commit
    fast_index_free(fast_idx);
    fast_idx = NULL;
    idx_loaded = 0;
    
    return result;
}


// Check if file is already in index with same hash
int is_file_unchanged_in_index(const char* filepath, const char* new_hash) {
    const char* old_hash = index_get_hash(filepath);
    return old_hash && strcmp(old_hash, new_hash) == 0;
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
            paths_equal(indexed_filepath, filepath)) {
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

    char hash[65]; // 64 chars + null terminator for SHA-256
    if (store_blob_from_file(filepath, hash) == -1) {
        fprintf(stderr, "Failed to store object for file: %s\n", filepath);
        return -1;
    }

    int unchanged = 0;
    if (index_upsert_entry(filepath, hash, (unsigned int)st.st_mode, &unchanged) == -1) {
        fprintf(stderr, "Failed to update index for file: %s\n", filepath);
        return -1;
    }
    return 0;
}

// Check if file is in index
int is_file_in_index(const char* filepath) {
    return index_get_hash(filepath) != NULL;
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
            if (paths_equal(indexed_filepath, filepath)) {
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