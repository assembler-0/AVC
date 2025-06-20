// src/commands/reset.c - FIXED VERSION
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
    printf("Loading commit object: %s\n", commit_hash);

    // Load commit object
    size_t commit_size;
    char commit_type[16];
    char* commit_content = load_object(commit_hash, &commit_size, commit_type);
    if (!commit_content) {
        fprintf(stderr, "Failed to load commit object: %s\n", commit_hash);
        return -1;
    }
    if (strcmp(commit_type, "commit") != 0) {
        fprintf(stderr, "Object %s is not a commit (type: %s)\n", commit_hash, commit_type);
        free(commit_content);
        return -1;
    }

    printf("Commit content loaded, size: %zu\n", commit_size);

    // Parse commit to get tree hash - look for "tree " at start of line
    char tree_hash[65] = {0};
    char* line_start = commit_content;
    char* line_end;

    while ((line_end = strchr(line_start, '\n')) != NULL) {
        *line_end = '\0';
        if (strncmp(line_start, "tree ", 5) == 0) {
            strncpy(tree_hash, line_start + 5, 64);
            tree_hash[64] = '\0';
            break;
        }
        *line_end = '\n'; // Restore newline
        line_start = line_end + 1;
    }

    free(commit_content);

    if (strlen(tree_hash) != 64) {
        fprintf(stderr, "Invalid commit format - no tree hash found\n");
        return -1;
    }

    printf("Tree hash found: %s\n", tree_hash);

    // Load tree object
    size_t tree_size;
    char tree_type[16];
    char* tree_content = load_object(tree_hash, &tree_size, tree_type);
    if (!tree_content) {
        fprintf(stderr, "Failed to load tree object: %s\n", tree_hash);
        return -1;
    }
    if (strcmp(tree_type, "tree") != 0) {
        fprintf(stderr, "Object %s is not a tree (type: %s)\n", tree_hash, tree_type);
        free(tree_content);
        return -1;
    }

    printf("Tree content loaded, size: %zu\n", tree_size);

    // Clear current index
    if (clear_index() == -1) {
        fprintf(stderr, "Failed to clear index\n");
        free(tree_content);
        return -1;
    }

    // Parse tree content line by line
    char* content_copy = malloc(tree_size + 1);
    if (!content_copy) {
        fprintf(stderr, "Memory allocation failed\n");
        free(tree_content);
        return -1;
    }
    memcpy(content_copy, tree_content, tree_size);
    content_copy[tree_size] = '\0';
    free(tree_content);
    int files_processed = 0;

    while ((line_end = strchr(line_start, '\n')) != NULL) {
        *line_end = '\0';

        // Skip empty lines
        if (strlen(line_start) == 0) {
            line_start = line_end + 1;
            continue;
        }

        unsigned int mode;
        char filepath[512], file_hash[65];

        // Parse tree entry: "mode filepath hash"
        if (sscanf(line_start, "%o %511s %64s", &mode, filepath, file_hash) == 3) {
            printf("Processing file: %s (hash: %.8s)\n", filepath, file_hash);

            // Load file content
            size_t file_size;
            char file_type[16];
            char* file_content = load_object(file_hash, &file_size, file_type);

            if (!file_content) {
                fprintf(stderr, "Failed to load file content for: %s (hash: %s)\n", filepath, file_hash);
                line_start = line_end + 1;
                continue;
            }

            if (strcmp(file_type, "blob") != 0) {
                fprintf(stderr, "Object %s is not a blob (type: %s)\n", file_hash, file_type);
                free(file_content);
                line_start = line_end + 1;
                continue;
            }

            if (hard_reset) {
                // Write file to working directory
                if (write_file(filepath, file_content, file_size) == -1) {
                    fprintf(stderr, "Failed to restore file: %s\n", filepath);
                } else {
                    printf("Restored: %s\n", filepath);
                }
            }

            // Add to index - use same format as index.c: "hash filepath mode"
            FILE* index = fopen(".avc/index", "a");
            if (index) {
                fprintf(index, "%s %s %o\n", file_hash, filepath, mode);
                fclose(index);
                files_processed++;
            } else {
                fprintf(stderr, "Failed to update index for: %s\n", filepath);
            }

            free(file_content);
        } else {
            printf("Skipping malformed tree entry: %s\n", line_start);
        }

        line_start = line_end + 1;
    }

    free(content_copy);

    printf("Processed %d files from tree\n", files_processed);

    // Update HEAD to point to this commit
    FILE* head = fopen(".avc/HEAD", "r");
    if (head) {
        char head_content[256];
        if (fgets(head_content, sizeof(head_content), head)) {
            // Remove trailing newline
            head_content[strcspn(head_content, "\n")] = '\0';

            if (strncmp(head_content, "ref: ", 5) == 0) {
                // Update branch reference
                char branch_path[512];
                char* branch_ref = head_content + 5;

                snprintf(branch_path, sizeof(branch_path), ".avc/%s", branch_ref);
                printf("Updating branch reference: %s\n", branch_path);

                FILE* branch_file = fopen(branch_path, "w");
                if (branch_file) {
                    fprintf(branch_file, "%s\n", commit_hash);
                    fclose(branch_file);
                    printf("Updated %s to point to commit %s\n", branch_ref, commit_hash);
                } else {
                    fprintf(stderr, "Failed to update branch reference: %s\n", branch_path);
                    perror("fopen");
                }
            } else {
                // HEAD contains commit hash directly
                FILE* head_write = fopen(".avc/HEAD", "w");
                if (head_write) {
                    fprintf(head_write, "%s\n", commit_hash);
                    fclose(head_write);
                    printf("Updated HEAD to point to commit %s\n", commit_hash);
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

    char resolved_hash[65];

    // Handle HEAD and HEAD~1 resolution
    if (strcmp(target_hash_str, "HEAD") == 0 || strcmp(target_hash_str, "HEAD~1") == 0) {
        // Get the current commit hash from the current branch
        FILE* head_file = fopen(".avc/HEAD", "r");
        if (!head_file) {
            perror("Failed to open .avc/HEAD");
            return 1;
        }

        char head_content[256];
        if (!fgets(head_content, sizeof(head_content), head_file)) {
            fclose(head_file);
            fprintf(stderr, "Failed to read .avc/HEAD\n");
            return 1;
        }
        fclose(head_file);

        // Remove trailing newline
        head_content[strcspn(head_content, "\n")] = '\0';

        char current_commit_hash[65] = "";
        if (strncmp(head_content, "ref: ", 5) == 0) {
            char branch_path[512];
            char* branch_ref = head_content + 5;
            snprintf(branch_path, sizeof(branch_path), ".avc/%s", branch_ref);

            printf("Reading branch reference: %s\n", branch_path);
            FILE* branch_file = fopen(branch_path, "r");
            if (branch_file) {
                if (fgets(current_commit_hash, sizeof(current_commit_hash), branch_file)) {
                    current_commit_hash[strcspn(current_commit_hash, "\n")] = '\0';
                    printf("Current commit from branch: %s\n", current_commit_hash);
                }
                fclose(branch_file);
            } else {
                fprintf(stderr, "Failed to read branch file: %s\n", branch_path);
                perror("fopen");
                return 1;
            }
        } else {
            // HEAD contains commit hash directly
            strncpy(current_commit_hash, head_content, sizeof(current_commit_hash) - 1);
            printf("Current commit from HEAD: %s\n", current_commit_hash);
        }

        if (strlen(current_commit_hash) == 0) {
            fprintf(stderr, "Could not resolve HEAD commit.\n");
            return 1;
        }

        if (strcmp(target_hash_str, "HEAD") == 0) {
            strncpy(resolved_hash, current_commit_hash, sizeof(resolved_hash) - 1);
            resolved_hash[sizeof(resolved_hash) - 1] = '\0';
        } else { // HEAD~1
            printf("Looking for parent of commit: %s\n", current_commit_hash);

            size_t commit_size;
            char commit_type[16];
            char* commit_content = load_object(current_commit_hash, &commit_size, commit_type);
            if (!commit_content || strcmp(commit_type, "commit") != 0) {
                fprintf(stderr, "Failed to load HEAD commit object.\n");
                if (commit_content) free(commit_content);
                return 1;
            }

            // Look for parent line
            char* line_start = commit_content;
            char* line_end;
            int parent_found = 0;

            while ((line_end = strchr(line_start, '\n')) != NULL) {
                *line_end = '\0';
                if (strncmp(line_start, "parent ", 7) == 0) {
                    strncpy(resolved_hash, line_start + 7, 64);
                    resolved_hash[64] = '\0';
                    parent_found = 1;
                    printf("Found parent commit: %s\n", resolved_hash);
                    break;
                }
                *line_end = '\n';
                line_start = line_end + 1;
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