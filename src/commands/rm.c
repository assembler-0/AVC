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
#include "arg_parser.h"

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

    // Parse arguments using the unified parser
    parsed_args_t* args = parse_args(argc, argv, "cr"); // c=cached, r=recursive
    if (!args) {
        return 1;
    }

    if (get_positional_count(args) == 0) {
        fprintf(stderr, "Usage: avc rm [--cached] [-r] <file...>\n");
        fprintf(stderr, "  --cached: Remove only from staging area, keep working directory file\n");
        fprintf(stderr, "  -r: Remove directories recursively\n");
        free_parsed_args(args);
        return 1;
    }

    int cached_only = has_flag(args, FLAG_CACHED);
    int recursive = has_flag(args, FLAG_RECURSIVE);
    char** paths = get_positional_args(args);
    size_t path_count = get_positional_count(args);

    // Process each path
    for (size_t i = 0; i < path_count; i++) {
        const char* path = paths[i];
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
            char** file_paths = NULL;
            size_t file_count = 0, file_cap = 0;
            collect_paths_to_remove(path, &file_paths, &file_count, &file_cap);

            // Remove all files from index
            for (size_t idx = 0; idx < file_count; idx++) {
                if (is_file_in_index(file_paths[idx])) {
                    if (remove_file_from_index(file_paths[idx]) == -1) {
                        fprintf(stderr, "Failed to remove '%s' from staging area\n", file_paths[idx]);
                    }
                }
            }

            // Clean up collected paths
            for (size_t j = 0; j < file_count; j++) {
                free(file_paths[j]);
            }
            free(file_paths);

            // Remove from working directory if not --cached
            if (!cached_only) {
                if (remove_directory_recursive(path) == -1) {
                    perror("Failed to remove directory from working directory");
                    fprintf(stderr, "Directory '%s' removed from staging area but not from working directory\n", path);
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

    free_parsed_args(args);
    return 0;
}