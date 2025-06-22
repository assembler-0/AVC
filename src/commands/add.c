// src/commands/add.c
#include <stdio.h>
#include "commands.h"
#include "repository.h"
#include "index.h"
#include "objects.h"
#include "file_utils.h"
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <omp.h>
#include <stdlib.h>

static void collect_files(const char* path, char*** paths, size_t* count, size_t* cap) {
    struct stat st;
    if (stat(path, &st) == -1) return;
    if (S_ISDIR(st.st_mode)) {
        DIR* d = opendir(path);
        if (!d) return;
        // Skip internal .avc directory
        if (strcmp(path, ".avc") == 0 || strstr(path, "/.avc") != NULL) { closedir(d); return; }
        struct dirent* e;
        while ((e = readdir(d))) {
            if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0 || strcmp(e->d_name, ".avc") == 0) continue;
            char child[1024];
            snprintf(child, sizeof(child), "%s/%s", path, e->d_name);
            collect_files(child, paths, count, cap);
        }
        closedir(d);
    } else if (S_ISREG(st.st_mode)) {
        if (*count == *cap) {
            *cap = *cap ? *cap*2 : 256;
            *paths = realloc(*paths, *cap*sizeof(char*));
        }
        (*paths)[*count] = strdup2(path);
        (*count)++;
    }
}

int cmd_add(int argc, char* argv[]) {
    if (check_repo() == -1) {
        return 1;
    }

    if (argc < 2) {
        fprintf(stderr, "Usage: avc add <file>\n");
        return 1;
    }

        // Collect all files to add first
    char** file_paths = NULL;
    size_t file_count = 0, file_cap = 0;

    // forward declaration


    for (int i = 1; i < argc; ++i) collect_files(argv[i], &file_paths, &file_count, &file_cap);

    if (!file_count) {
        fprintf(stderr, "Nothing to add\n");
        return 1;
    }

    printf("Adding %zu files...\n", file_count);
    // Results arrays
    char (*hashes)[65] = calloc(file_count, 65);
    unsigned int* modes = calloc(file_count, sizeof(unsigned int));
    int* changed = calloc(file_count, sizeof(int)); // Track which files are actually changed

    // Load index once so we can compare hashes
    if (index_load() == -1) {
        fprintf(stderr, "Failed to load index\n");
        return 1;
    }

    // Parallel processing of blobs
    #pragma omp parallel for schedule(dynamic)
    for (size_t i = 0; i < file_count; ++i) {
        struct stat st;
        if (stat(file_paths[i], &st) == -1) continue;
        
        // Normalize file path to match index format (add "./" prefix if not present)
        char normalized_path[1024];
        if (file_paths[i][0] == '/') {
            // Absolute path, use as is
            strcpy(normalized_path, file_paths[i]);
        } else if (strncmp(file_paths[i], "./", 2) == 0) {
            // Already has "./" prefix
            strcpy(normalized_path, file_paths[i]);
        } else {
            // Add "./" prefix
            snprintf(normalized_path, sizeof(normalized_path), "./%s", file_paths[i]);
        }
        
        const char* old_hash = index_get_hash(normalized_path);
        if (old_hash) {
            char new_hash[65];
            sha256_file_hex(file_paths[i], new_hash);
            if (strcmp(old_hash, new_hash) == 0) {
                // unchanged, reuse old hash but mark as unchanged
                strcpy(hashes[i], old_hash);
                modes[i] = (unsigned int)st.st_mode;
                changed[i] = 0; // Mark as unchanged
                continue;
            }
        }
        store_blob_from_file(file_paths[i], hashes[i]);
        modes[i] = (unsigned int)st.st_mode;
        changed[i] = 1; // Mark as changed
    }

    // Batch updates - only for changed files
    int added_count = 0;
    int unchanged_count = 0;
    for (size_t i = 0; i < file_count; ++i) {
        if (changed[i]) {
            // Normalize file path for index operations
            char normalized_path[1024];
            if (file_paths[i][0] == '/') {
                strcpy(normalized_path, file_paths[i]);
            } else if (strncmp(file_paths[i], "./", 2) == 0) {
                strcpy(normalized_path, file_paths[i]);
            } else {
                snprintf(normalized_path, sizeof(normalized_path), "./%s", file_paths[i]);
            }
            
        int unchanged=0;
            if (index_upsert_entry(normalized_path, hashes[i], modes[i], &unchanged)==-1) {
            fprintf(stderr, "Failed to update index for %s\n", file_paths[i]);
            } else {
                added_count++;
            }
        } else {
            unchanged_count++;
        }
    }

    printf("Committing index...\n");
    if (index_commit() == -1) {
        fprintf(stderr, "Failed to write index\n");
        return 1;
    }
    printf("Added %d files to staging area.\n", added_count);
    printf("Done.\n");

        // free memory
    for (size_t i=0;i<file_count;++i) free(file_paths[i]);
    free(file_paths);
    free(hashes);
    free(modes);
    free(changed);

    return 0;
}
