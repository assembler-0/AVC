//
// Created by Atheria on 6/20/25.
//
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "commands.h"
#include "repository.h"
#include "file_utils.h"
#include "reset.h"
int cmd_reset(int argc, char* argv[]) {
    if (check_repo() == -1) {
        return 1;
    }

    if (argc < 2) {
        fprintf(stderr, "Usage: avc reset [--hard] <commit-hash>\n");
        fprintf(stderr, "  --hard: Reset working directory and index\n");
        fprintf(stderr, "  (default): Reset only index, keep working directory\n");
        return 1;
    }

    int hard_reset = 0;
    char* commit_hash = NULL;

    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--hard") == 0) {
            hard_reset = 1;
        } else {
            commit_hash = argv[i];
        }
    }

    if (!commit_hash) {
        fprintf(stderr, "Please specify a commit hash\n");
        return 1;
    }

    // Validate commit hash length (should be 40 chars for SHA-1)
    if (strlen(commit_hash) != 40) {
        fprintf(stderr, "Invalid commit hash format\n");
        return 1;
    }

    printf("Resetting to commit %s%s...\n", commit_hash, hard_reset ? " (hard)" : "");

    if (reset_to_commit(commit_hash, hard_reset) == -1) {
        return 1;
    }

    printf("Reset complete.\n");
    return 0;
}