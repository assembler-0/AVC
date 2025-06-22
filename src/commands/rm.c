// src/commands/rm.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "commands.h"
#include "repository.h"
#include <dirent.h>
#include "index.h"
#include "file_utils.h"

// Recursively collect regular file paths under a directory (skips . and .. and internal .avc)
static void collect_paths_to_remove(const char* path, char*** paths, size_t* count, size_t* cap) {
    struct stat st;
    if (stat(path, &st) == -1) return;

    if (S_ISDIR(st.st_mode)) {
        // Skip internal .avc directory entirely
        if (strcmp(path, ".avc") == 0 || strstr(path, "/.avc") != NULL) {
            return;
        }
        DIR* d = opendir(path);
        if (!d) return;
        struct dirent* e;
        while ((e = readdir(d))) {
            if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
            char child[1024];
            snprintf(child, sizeof(child), "%s/%s", path, e->d_name);
            collect_paths_to_remove(child, paths, count, cap);
        }
        closedir(d);
    } else if (S_ISREG(st.st_mode)) {
        if (*count == *cap) {
            *cap = *cap ? *cap * 2 : 256;
            *paths = realloc(*paths, *cap * sizeof(char*));
        }
        (*paths)[*count] = strdup2(path);
        (*count)++;
    }
}

int cmd_rm(int argc, char* argv[]) {
    if (check_repo() == -1) {
        return 1;
    }

    if (argc < 2) {
        fprintf(stderr, "Usage: avc rm [--cached] [-r] <file...>\n");
        fprintf(stderr, "  --cached: Remove only from staging area, keep working directory file\n");
        fprintf(stderr, "  -r: Remove directories recursively\n");
        return 1;
    }

    int cached_only = 0;
    int recursive = 0;
    int start_idx = 1;

    // Check for flags
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--cached") == 0) {
            cached_only = 1;
        } else if (strcmp(argv[i], "-r") == 0) {
            recursive = 1;
        } else {
            start_idx = i;
            break;
        }
    }

    if (start_idx >= argc) {
        fprintf(stderr, "Usage: avc rm [--cached] [-r] <file...>\n");
        return 1;
    }

    // Process each path
    for (int i = start_idx; i < argc; i++) {
        const char* path = argv[i];
        struct stat st;

        // Check if path exists
        if (stat(path, &st) == -1) {
            fprintf(stderr, "Path '%s' does not exist\n", path);
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            // Handle directory
            if (!recursive) {
                fprintf(stderr, "Cannot remove directory '%s': use -r flag for recursive removal\n", path);
                continue;
            }

            // For directories, we need to remove all files from index first
            char** paths = NULL;
            size_t path_count = 0, path_cap = 0;
            collect_paths_to_remove(path, &paths, &path_count, &path_cap);

            // Remove all files from index
            for (size_t idx = 0; idx < path_count; idx++) {
                if (is_file_in_index(paths[idx])) {
                    if (remove_file_from_index(paths[idx]) == -1) {
                        fprintf(stderr, "Failed to remove '%s' from staging area\n", paths[idx]);
                    } else {
                        printf("Removed '%s' from staging area\n", paths[idx]);
                    }
                }
            }

            // Clean up collected paths
            for (size_t j = 0; j < path_count; j++) {
                free(paths[j]);
            }
            free(paths);

            // Remove from working directory if not --cached
            if (!cached_only) {
                if (remove_directory_recursive(path) == -1) {
                    perror("Failed to remove directory from working directory");
                    fprintf(stderr, "Directory '%s' removed from staging area but not from working directory\n", path);
                } else {
                    printf("Removed directory '%s' from working directory\n", path);
                }
            }
        } else {
            // Handle regular file (existing logic)
            // Check if file exists in working directory
            int file_exists = (stat(path, &st) == 0);

            // Check if file is in index
            int in_index = is_file_in_index(path);

            if (!in_index && !file_exists) {
                fprintf(stderr, "File '%s' does not exist in staging area or working directory\n", path);
                continue;
            }

            // Remove from index if it's there
            if (in_index) {
                if (remove_file_from_index(path) == -1) {
                    fprintf(stderr, "Failed to remove '%s' from staging area\n", path);
                    continue;
                }
                printf("Removed '%s' from staging area\n", path);
            }

            // If not --cached, also remove from working directory
            if (!cached_only && file_exists) {
                if (unlink(path) == -1) {
                    perror("Failed to remove file from working directory");
                    fprintf(stderr, "File '%s' removed from staging area but not from working directory\n", path);
                } else {
                    printf("Removed '%s' from working directory\n", path);
                }
            }
        }
    }

    return 0;
}