// src/commands/status.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "commands.h"

// Check if we're in a repository
int check_repo();

int cmd_status(int argc, char* argv[]) {
    if (check_repo() == -1) {
        return 1;
    }

    printf("On branch main\n\n");

    // Read and display staged files
    FILE* index = fopen(".avc/index", "r");
    if (!index) {
        printf("No files staged for commit.\n");
        return 0;
    }

    char line[1024];
    int has_staged = 0;

    printf("Changes to be committed:\n");
    printf("  (use \"avc commit\" to commit)\n\n");

    while (fgets(line, sizeof(line), index)) {
        // Parse index line: hash filepath mode
        char hash[41], filepath[256];
        unsigned int mode;

        if (sscanf(line, "%40s %255s %o", hash, filepath, &mode) == 3) {
            printf("  \033[32mnew file:   %s\033[0m\n", filepath);
            has_staged = 1;
        }
    }

    fclose(index);

    if (!has_staged) {
        printf("No files staged for commit.\n");
    }

    printf("\n");
    return 0;
}