// src/commands/commit.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#include "commands.h"

// Check if we're in a repository (defined in add.c)
int check_repo();

// Simple hash function (same as in add.c - we'll refactor this later)
int store_object(const char* type, const char* content, size_t size, char* hash_out);

// Create tree object from current index
int create_tree(char* tree_hash) {
    FILE* index = fopen(".avc/index", "r");
    if (!index) {
        fprintf(stderr, "No files to commit (index is empty)\n");
        return -1;
    }

    // Create tree content
    char tree_content[4096] = "";
    char line[1024];

    while (fgets(line, sizeof(line), index)) {
        char hash[41], filepath[256];
        unsigned int mode;

        if (sscanf(line, "%40s %255s %o", hash, filepath, &mode) == 3) {
            // Add to tree: mode filepath hash
            char entry[512];
            snprintf(entry, sizeof(entry), "%o %s %s\n", mode, filepath, hash);
            strcat(tree_content, entry);
        }
    }
    fclose(index);

    // Generate tree hash
    if (store_object("tree", tree_content, strlen(tree_content), tree_hash) == -1) {
        return -1;
    }

    return 0;
}

// Get current commit (HEAD)
int get_current_commit(char* commit_hash) {
    FILE* head = fopen(".avc/HEAD", "r");
    if (!head) {
        commit_hash[0] = '\0';  // No previous commit
        return 0;
    }

    char head_content[256];
    if (!fgets(head_content, sizeof(head_content), head)) {
        fclose(head);
        commit_hash[0] = '\0';
        return 0;
    }
    fclose(head);

    // Parse HEAD content
    if (strncmp(head_content, "ref: ", 5) == 0) {
        // HEAD points to a branch
        char branch_path[512];
        char* branch_ref = head_content + 5;
        branch_ref[strcspn(branch_ref, "\n")] = '\0';  // Remove newline

        snprintf(branch_path, sizeof(branch_path), ".avc/%s", branch_ref);

        FILE* branch_file = fopen(branch_path, "r");
        if (!branch_file) {
            commit_hash[0] = '\0';  // Branch doesn't exist yet
            return 0;
        }

        if (fgets(commit_hash, 41, branch_file)) {
            commit_hash[strcspn(commit_hash, "\n")] = '\0';  // Remove newline
        } else {
            commit_hash[0] = '\0';
        }
        fclose(branch_file);
    }

    return 0;
}

// Update HEAD to point to new commit
int update_head(const char* commit_hash) {
    // Read current HEAD
    FILE* head = fopen(".avc/HEAD", "r");
    if (!head) {
        return -1;
    }

    char head_content[256];
    if (!fgets(head_content, sizeof(head_content), head)) {
        fclose(head);
        return -1;
    }
    fclose(head);

    // Update branch reference
    if (strncmp(head_content, "ref: ", 5) == 0) {
        char branch_path[512];
        char* branch_ref = head_content + 5;
        branch_ref[strcspn(branch_ref, "\n")] = '\0';  // Remove newline

        snprintf(branch_path, sizeof(branch_path), ".avc/%s", branch_ref);

        FILE* branch_file = fopen(branch_path, "w");
        if (!branch_file) {
            perror("Failed to update branch");
            return -1;
        }

        fprintf(branch_file, "%s\n", commit_hash);
        fclose(branch_file);
    }

    return 0;
}

int cmd_commit(int argc, char* argv[]) {
    if (check_repo() == -1) {
        return 1;
    }

    // Check if there are staged changes
    struct stat st;
    if (stat(".avc/index", &st) == -1 || st.st_size == 0) {
        printf("No changes to commit.\n");
        return 0;
    }

    // Get commit message
    char message[1024];
    if (argc > 2 && strcmp(argv[1], "-m") == 0) {
        strncpy(message, argv[2], sizeof(message) - 1);
        message[sizeof(message) - 1] = '\0';
    } else {
        printf("Commit message: ");
        if (!fgets(message, sizeof(message), stdin)) {
            fprintf(stderr, "Failed to read commit message\n");
            return 1;
        }
        message[strcspn(message, "\n")] = '\0';  // Remove newline
    }

    if (strlen(message) == 0) {
        fprintf(stderr, "Commit message cannot be empty\n");
        return 1;
    }

    // Create tree from index
    char tree_hash[41];
    if (create_tree(tree_hash) == -1) {
        return 1;
    }

    // Get parent commit
    char parent_hash[41];
    get_current_commit(parent_hash);

    // Create commit object
    char commit_content[2048];
    time_t now = time(NULL);
    char* author = getenv("USER");
    if (!author) author = "unknown";

    if (strlen(parent_hash) > 0) {
        snprintf(commit_content, sizeof(commit_content),
                "tree %s\nparent %s\nauthor %s %ld\ncommitter %s %ld\n\n%s\n",
                tree_hash, parent_hash, author, now, author, now, message);
    } else {
        snprintf(commit_content, sizeof(commit_content),
                "tree %s\nauthor %s %ld\ncommitter %s %ld\n\n%s\n",
                tree_hash, author, now, author, now, message);
    }

    // Generate commit hash
    // Generate commit hash and store object
    char commit_hash[41];
    if (store_object("commit", commit_content, strlen(commit_content), commit_hash) == -1) {
        fprintf(stderr, "Failed to create commit object\n");
        return 1;
    }

    // Update HEAD
    if (update_head(commit_hash) == -1) {
        fprintf(stderr, "Failed to update HEAD\n");
        return 1;
    }

    // Clear index
    FILE* index = fopen(".avc/index", "w");
    if (index) {
        fclose(index);
    }

    printf("[main %.7s] %s\n", commit_hash, message);

    return 0;
}