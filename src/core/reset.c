//
// Created by Atheria on 6/20/25.
//
#include "file_utils.h"
#include "reset.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "index.h"
#include "objects.h"

// Reset working directory to match a commit
int reset_to_commit(const char* commit_hash, int hard_reset) {
    // Load commit object
    size_t commit_size;
    char commit_type[16];
    char* commit_content = load_object(commit_hash, &commit_size, commit_type);
    if (!commit_content || strcmp(commit_type, "commit") != 0) {
        fprintf(stderr, "Invalid commit: %s\n", commit_hash);
        free(commit_content);
        return -1;
    }

    // Parse commit to get tree hash
    char tree_hash[41];
    if (sscanf(commit_content, "tree %40s", tree_hash) != 1) {
        fprintf(stderr, "Invalid commit format\n");
        free(commit_content);
        return -1;
    }
    free(commit_content);

    // Load tree object
    size_t tree_size;
    char tree_type[16];
    char* tree_content = load_object(tree_hash, &tree_size, tree_type);
    if (!tree_content || strcmp(tree_type, "tree") != 0) {
        fprintf(stderr, "Invalid tree: %s\n", tree_hash);
        free(tree_content);
        return -1;
    }

    // Clear current index
    if (clear_index() == -1) {
        fprintf(stderr, "Failed to clear index\n");
        free(tree_content);
        return -1;
    }

    // Parse tree and restore files
    char* line = strtok(tree_content, "\n");
    while (line) {
        unsigned int mode;
        char filepath[256], file_hash[41];

        if (sscanf(line, "%o %255s %40s", &mode, filepath, file_hash) == 3) {
            // Load file content
            size_t file_size;
            char file_type[16];
            char* file_content = load_object(file_hash, &file_size, file_type);
            if (file_content && strcmp(file_type, "blob") == 0) {
                if (hard_reset) {
                    // Write file to working directory
                    if (write_file(filepath, file_content, file_size) == -1) {
                        fprintf(stderr, "Failed to restore file: %s\n", filepath);
                    } else {
                        printf("Restored: %s\n", filepath);
                    }
                }

                // Add to index
                FILE* index = fopen(".avc/index", "a");
                if (index) {
                    fprintf(index, "%s %s %o\n", file_hash, filepath, mode);
                    fclose(index);
                }

                free(file_content);
            }
        }
        line = strtok(NULL, "\n");
    }
    free(tree_content);

    // Update HEAD to point to this commit
    FILE* head = fopen(".avc/HEAD", "r");
    if (head) {
        char head_content[256];
        if (fgets(head_content, sizeof(head_content), head)) {
            if (strncmp(head_content, "ref: ", 5) == 0) {
                // Update branch reference
                char branch_path[512];
                char* branch_ref = head_content + 5;
                branch_ref[strcspn(branch_ref, "\n")] = '\0';

                snprintf(branch_path, sizeof(branch_path), ".avc/%s", branch_ref);
                FILE* branch_file = fopen(branch_path, "w");
                if (branch_file) {
                    fprintf(branch_file, "%s\n", commit_hash);
                    fclose(branch_file);
                }
            }
        }
        fclose(head);
    }

    return 0;
}
