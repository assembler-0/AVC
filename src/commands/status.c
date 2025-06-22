// src/commands/status.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "commands.h"
#include "repository.h"
#include "objects.h"

// Check if we're in a repository
int check_repo();

// Get the tree hash from the last commit
int get_last_commit_tree(char* tree_hash) {
    FILE* head = fopen(".avc/HEAD", "r");
    if (!head) {
        tree_hash[0] = '\0';
        return 0;
    }

    char head_content[256];
    if (!fgets(head_content, sizeof(head_content), head)) {
        fclose(head);
        tree_hash[0] = '\0';
        return 0;
    }
    fclose(head);

    // Parse HEAD content
    if (strncmp(head_content, "ref: ", 5) == 0) {
        char branch_path[512];
        char* branch_ref = head_content + 5;
        branch_ref[strcspn(branch_ref, "\n")] = '\0';

        snprintf(branch_path, sizeof(branch_path), ".avc/%s", branch_ref);

        FILE* branch_file = fopen(branch_path, "r");
        if (!branch_file) {
            tree_hash[0] = '\0';
            return 0;
        }

        char commit_hash[65];
        if (fgets(commit_hash, 65, branch_file)) {
            commit_hash[strcspn(commit_hash, "\n")] = '\0';
        } else {
            fclose(branch_file);
            tree_hash[0] = '\0';
            return 0;
        }
        fclose(branch_file);

        // Load commit object to get tree hash
        size_t commit_size;
        char commit_type[16];
        char* commit_content = load_object(commit_hash, &commit_size, commit_type);
        if (!commit_content || strcmp(commit_type, "commit") != 0) {
            free(commit_content);
            tree_hash[0] = '\0';
            return 0;
        }

        // Parse tree hash from commit
        char* content_copy = malloc(commit_size + 1);
        if (!content_copy) {
            free(commit_content);
            tree_hash[0] = '\0';
            return 0;
        }
        memcpy(content_copy, commit_content, commit_size);
        content_copy[commit_size] = '\0';

        char* line_start = content_copy;
        char* line_end;
        int found = 0;

        while ((line_end = strchr(line_start, '\n')) != NULL) {
            *line_end = '\0';
            if (strncmp(line_start, "tree ", 5) == 0) {
                strncpy(tree_hash, line_start + 5, 64);
                tree_hash[64] = '\0';
                found = 1;
                break;
            }
            line_start = line_end + 1;
        }

        free(commit_content);
        free(content_copy);
        return found ? 0 : -1;
    }

    tree_hash[0] = '\0';
    return 0;
}

// Check if file exists in tree with same hash
int file_in_tree_with_hash(const char* tree_hash, const char* filepath, const char* hash) {
    if (strlen(tree_hash) == 0) return 0; // No previous commit

    size_t tree_size;
    char tree_type[16];
    char* tree_content = load_object(tree_hash, &tree_size, tree_type);
    if (!tree_content || strcmp(tree_type, "tree") != 0) {
        free(tree_content);
        return 0;
    }

    char* content_copy = malloc(tree_size + 1);
    if (!content_copy) {
        free(tree_content);
        return 0;
    }
    memcpy(content_copy, tree_content, tree_size);
    content_copy[tree_size] = '\0';

    char* line_start = content_copy;
    char* line_end;
    int found = 0;

    while ((line_end = strchr(line_start, '\n')) != NULL) {
        *line_end = '\0';
        char tree_mode[16], tree_path[256], tree_hash_from_tree[65];
        if (sscanf(line_start, "%15s %255s %64s", tree_mode, tree_path, tree_hash_from_tree) == 3) {
            if (strcmp(tree_path, filepath) == 0) {
                found = (strcmp(tree_hash_from_tree, hash) == 0);
                break;
            }
        }
        line_start = line_end + 1;
    }

    free(tree_content);
    free(content_copy);
    return found;
}

int cmd_status(int argc, char* argv[]) {
    if (check_repo() == -1) {
        return 1;
    }

    printf("On branch main\n\n");

    // Check if index is empty
    struct stat st;
    if (stat(".avc/index", &st) == -1 || st.st_size == 0) {
        printf("No changes to be committed.\n");
        return 0;
    }

    // Get the tree hash from the last commit
    char last_tree_hash[65];
    get_last_commit_tree(last_tree_hash);

    // Read and display staged files
    FILE* index = fopen(".avc/index", "r");
    if (!index) {
        printf("No changes to be committed.\n");
        return 0;
    }

    char line[1024];
    int has_staged = 0;

    printf("Changes to be committed:\n");
    printf("  (use \"avc commit\" to commit)\n\n");

    while (fgets(line, sizeof(line), index)) {
        // Parse index line: hash filepath mode
        char hash[65], filepath[256];
        unsigned int mode;

        if (sscanf(line, "%64s %255s %o", hash, filepath, &mode) == 3) {
            // Check if this file exists in the last commit with the same hash
            if (file_in_tree_with_hash(last_tree_hash, filepath, hash)) {
                printf("  \033[33mmodified:   %s\033[0m\n", filepath);
            } else {
            printf("  \033[32mnew file:   %s\033[0m\n", filepath);
            }
            has_staged = 1;
        }
    }

    fclose(index);

    if (!has_staged) {
        printf("No changes to be committed.\n");
    }

    printf("\n");
    return 0;
}