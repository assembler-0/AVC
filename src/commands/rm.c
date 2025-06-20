// src/commands/rm.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "commands.h"
#include "repository.h"
#include "../core/index.h"

int cmd_rm(int argc, char* argv[]) {
    if (check_repo() == -1) {
        return 1;
    }

    if (argc < 2) {
        fprintf(stderr, "Usage: avc rm [--cached] <file...>\n");
        fprintf(stderr, "  --cached: Remove only from staging area, keep working directory file\n");
        return 1;
    }

    int cached_only = 0;
    int start_idx = 1;

    // Check for --cached flag
    if (argc > 1 && strcmp(argv[1], "--cached") == 0) {
        cached_only = 1;
        start_idx = 2;
        if (argc < 3) {
            fprintf(stderr, "Usage: avc rm --cached <file...>\n");
            return 1;
        }
    }

    // Process each file
    for (int i = start_idx; i < argc; i++) {
        const char* filepath = argv[i];

        // Check if file exists in working directory
        struct stat st;
        int file_exists = (stat(filepath, &st) == 0);

        // Check if file is in index
        int in_index = is_file_in_index(filepath);

        if (!in_index && !file_exists) {
            fprintf(stderr, "File '%s' does not exist in staging area or working directory\n", filepath);
            continue;
        }

        // Remove from index if it's there
        if (in_index) {
            if (remove_file_from_index(filepath) == -1) {
                fprintf(stderr, "Failed to remove '%s' from staging area\n", filepath);
                continue;
            }
            printf("Removed '%s' from staging area\n", filepath);
        }

        // If not --cached, also remove from working directory
        if (!cached_only && file_exists) {
            if (unlink(filepath) == -1) {
                perror("Failed to remove file from working directory");
                fprintf(stderr, "File '%s' removed from staging area but not from working directory\n", filepath);
            } else {
                printf("Removed '%s' from working directory\n", filepath);
            }
        }
    }

    return 0;
}