// src/commands/rm.c
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "commands.h"
#include "repository.h"
#include "index.h"

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

        // Check if file is in index
        if (!is_file_in_index(filepath)) {
            fprintf(stderr, "File '%s' is not in the staging area\n", filepath);
            continue;
        }

        // Remove from index
        if (remove_file_from_index(filepath) == -1) {
            fprintf(stderr, "Failed to remove '%s' from staging area\n", filepath);
            continue;
        }

        printf("Removed '%s' from staging area\n", filepath);

        // If not --cached, also remove from working directory
        if (!cached_only) {
            struct stat st;
            if (stat(filepath, &st) == 0) {
                if (unlink(filepath) == -1) {
                    perror("Failed to remove file from working directory");
                    fprintf(stderr, "File '%s' removed from staging area but not from working directory\n", filepath);
                } else {
                    printf("Removed '%s' from working directory\n", filepath);
                }
            }
        }
    }

    return 0;
}