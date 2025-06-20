
// src/commands/reset.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "commands.h"
#include "repository.h"
#include "index.h"
#include "objects.h"
#include "file_utils.h"

// Reset working directory to match a commit
int reset_to_commit(const char* commit_hash, int hard_reset) {
    // Load commit object
    size_t commit_size;
    char commit_type[16];
    char* commit_content = load_object(commit_hash, &commit_size, commit_type);
    if (!commit_content || strcmp(commit_type, "commit") != 0) {
        fprintf(stderr, "Invalid commit: %s\n", commit_hash);
        if (commit_content) free(commit_content);
        return -1;
    }

    // Parse commit to get tree hash
    char tree_hash[65]; // 64 chars + null terminator for SHA-256
    if (sscanf(commit_content, "tree %64s", tree_hash) != 1) {
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
        if (tree_content) free(tree_content);
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
        char filepath[256], file_hash[65]; // 64 chars + null terminator

        if (sscanf(line, "%o %255s %64s", &mode, filepath, file_hash) == 3) {
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

int cmd_reset(int argc, char* argv[]) {
    if (check_repo() == -1) {
        return 1;
    }

    if (argc < 2) {
        fprintf(stderr, "Usage: avc reset [--hard] <commit-hash>\n");
        fprintf(stderr, "  --hard: Reset working directory and index\n");
        fprintf(stderr, "  (default): Reset only index, keep working directory\n");
        fprintf(stderr, "  You can also use: avc reset [--hard] HEAD~1  (previous commit)\n");
        return 1;
    }

    int hard_reset = 0;
    char* target_hash_str = NULL;

    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--hard") == 0) {
            hard_reset = 1;
        } else {
            target_hash_str = argv[i];
        }
    }

    if (!target_hash_str) {
        fprintf(stderr, "Please specify a commit hash\n");
        return 1;
    }

    char resolved_hash[65]; // SHA-256 hash (64 hex + null)

    // FIX: Properly handle HEAD and HEAD~1 resolution
    if (strcmp(target_hash_str, "HEAD") == 0 || strcmp(target_hash_str, "HEAD~1") == 0) {
        // Get the current commit hash from the current branch
        FILE* head_file = fopen(".avc/HEAD", "r");
        if (!head_file) { perror("Failed to open .avc/HEAD"); return 1; }

        char head_content[256];
        if (!fgets(head_content, sizeof(head_content), head_file)) {
            fclose(head_file);
            fprintf(stderr, "Failed to read .avc/HEAD\n");
            return 1;
        }
        fclose(head_file);

        char current_commit_hash[65] = "";
        if (strncmp(head_content, "ref: ", 5) == 0) {
            char branch_path[512];
            char* branch_ref = head_content + 5;
            branch_ref[strcspn(branch_ref, "\n")] = '\0';
            snprintf(branch_path, sizeof(branch_path), ".avc/%s", branch_ref);

            FILE* branch_file = fopen(branch_path, "r");
            if (branch_file) {
                if (fgets(current_commit_hash, sizeof(current_commit_hash), branch_file)) {
                    current_commit_hash[strcspn(current_commit_hash, "\n")] = '\0';
                }
                fclose(branch_file);
            }
        }

        if (strlen(current_commit_hash) == 0) {
            fprintf(stderr, "Could not resolve HEAD commit.\n");
            return 1;
        }

        if (strcmp(target_hash_str, "HEAD") == 0) {
            strncpy(resolved_hash, current_commit_hash, sizeof(resolved_hash) - 1);
            resolved_hash[sizeof(resolved_hash) - 1] = '\0';
        } else { // HEAD~1
            size_t commit_size;
            char commit_type[16];
            char* commit_content = load_object(current_commit_hash, &commit_size, commit_type);
            if (!commit_content || strcmp(commit_type, "commit") != 0) {
                fprintf(stderr, "Failed to load HEAD commit object.\n");
                if (commit_content) free(commit_content);
                return 1;
            }

            char* line = strtok(commit_content, "\n");
            int parent_found = 0;
            while (line) {
                if (sscanf(line, "parent %64s", resolved_hash) == 1) {
                    parent_found = 1;
                    break;
                }
                line = strtok(NULL, "\n");
            }
            free(commit_content);

            if (!parent_found) {
                fprintf(stderr, "HEAD has no parent commit to reset to.\n");
                return 1;
            }
        }
        target_hash_str = resolved_hash;
    }

    if (strlen(target_hash_str) != 64) {
        fprintf(stderr, "Invalid commit hash format (expected 64 characters, got %zu)\n", strlen(target_hash_str));
        fprintf(stderr, "Use 'avc log' to see available commit hashes\n");
        return 1;
    }

    printf("Resetting to commit %s%s...\n", target_hash_str, hard_reset ? " (hard)" : "");

    if (reset_to_commit(target_hash_str, hard_reset) == -1) {
        return 1;
    }

    printf("Reset complete.\n");
    return 0;
}