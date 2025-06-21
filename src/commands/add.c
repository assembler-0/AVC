// src/commands/add.c
#include <stdio.h>
#include "commands.h"
#include "repository.h"
#include "index.h"

int cmd_add(int argc, char* argv[]) {
    if (check_repo() == -1) {
        return 1;
    }

    if (argc < 2) {
        fprintf(stderr, "Usage: avc add <file>\n");
        return 1;
    }

    // Load index once, process all paths, then commit.
    if (index_load() == -1) {
        fprintf(stderr, "Failed to load index\n");
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        if (add_file_to_index(argv[i]) == -1) {
            fprintf(stderr, "Failed to add file: %s\n", argv[i]);
            return 1;
        }
    }

    if (index_commit() == -1) {
        fprintf(stderr, "Failed to write index\n");
        return 1;
    }

    return 0;
}