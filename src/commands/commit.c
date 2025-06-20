// src/commands/commit.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include "commands.h"
#include "repository.h"
#include "objects.h"
#include "index.h"

int create_tree(char* tree_hash) {
    FILE* index = fopen(".avc/index", "r");
    if (!index) {
        fprintf(stderr, "No files to commit (index is empty)\n");
        return -1;
    }

    // Start with initial buffer size and grow as needed
    size_t buffer_size = 8192;
    size_t content_len = 0;
    char* tree_content = malloc(buffer_size);
    if (!tree_content) {
        fprintf(stderr, "Memory allocation failed\n");
        fclose(index);
        return -1;
    }
    tree_content[0] = '\0';

    char line[1024];
    while (fgets(line, sizeof(line), index)) {
        char hash[65], filepath[256]; // Updated to 64 chars for SHA-256
        unsigned int mode;

        if (sscanf(line, "%64s %255s %o", hash, filepath, &mode) == 3) {
            // Create entry: mode filepath hash
            char entry[512];
            int entry_len = snprintf(entry, sizeof(entry), "%o %s %s\n", mode, filepath, hash);

            // Check if we need to grow the buffer
            if (content_len + entry_len + 1 > buffer_size) {
                buffer_size *= 2;
                char* new_buffer = realloc(tree_content, buffer_size);
                if (!new_buffer) {
                    fprintf(stderr, "Memory reallocation failed\n");
                    free(tree_content);
                    fclose(index);
                    return -1;
                }
                tree_content = new_buffer;
            }

            // Safe concatenation
            strcat(tree_content, entry);
            content_len += entry_len;
        }
    }
    fclose(index);

    // Generate tree hash
    int result = store_object("tree", tree_content, strlen(tree_content), tree_hash);
    free(tree_content);

    if (result == -1) {
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

        if (fgets(commit_hash, 65, branch_file)) { // Updated to 65 for SHA-256
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
        printf("Enter a commit message (or use -m <msg>): ");
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
    char tree_hash[65]; // Updated to 65 for SHA-256
    if (create_tree(tree_hash) == -1) {
        return 1;
    }

    // Get parent commit
    char parent_hash[65]; // Updated to 65 for SHA-256
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

    // Generate commit hash and store object
    char commit_hash[65]; // Updated to 65 for SHA-256
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
    if (clear_index() == -1) {
        fprintf(stderr, "Warning: Failed to clear index after commit\n");
    }

    printf("[main %.7s] %s\n", commit_hash, message);

    return 0;
}