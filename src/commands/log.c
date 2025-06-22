
#include "commands.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "repository.h"
#include "objects.h"

// Helper function to format timestamp
void format_timestamp(time_t timestamp, char* buffer, size_t buffer_size) {
    struct tm* tm_info = localtime(&timestamp);
    strftime(buffer, buffer_size, "%a %b %d %H:%M:%S %Y", tm_info);
}

// Display a single commit
void display_commit(const char* commit_hash, const char* commit_content) {
    printf("commit %s\n", commit_hash);

    // Parse commit content
    char* line = strtok((char*)commit_content, "\n");
    char author[256] = "Unknown";
    char timestamp_str[64] = "";
    time_t commit_time = 0;
    char* message = NULL;

    while (line) {
        if (strncmp(line, "tree ", 5) == 0) {
            // Skip tree line
        } else if (strncmp(line, "parent ", 7) == 0) {
            // Skip parent line for now
        } else if (strncmp(line, "author ", 7) == 0) {
            // Parse author line: "author Name <email> timestamp"
            char* author_start = line + 7;
            char* timestamp_start = strrchr(author_start, ' ');
            if (timestamp_start) {
                *timestamp_start = '\0';
                commit_time = atoll(timestamp_start + 1);
                strncpy(author, author_start, sizeof(author) - 1);
                author[sizeof(author) - 1] = '\0';
            }
        } else if (strlen(line) == 0) {
            // Empty line, next line is the commit message
            line = strtok(NULL, "\n");
            if (line) {
                message = line;
            }
            break;
        }
        line = strtok(NULL, "\n");
    }

    printf("Author: %s\n", author);

    if (commit_time > 0) {
        format_timestamp(commit_time, timestamp_str, sizeof(timestamp_str));
        printf("Date: %s\n", timestamp_str);
    }

    printf("\n");
    if (message) {
        printf("    %s\n", message);
    }
    printf("\n");
}

// Get current HEAD commit hash
char* get_current_commit_hash() {
    FILE* head = fopen(".avc/HEAD", "r");
    if (!head) {
        return NULL;
    }

    char head_content[256];
    if (!fgets(head_content, sizeof(head_content), head)) {
        fclose(head);
        return NULL;
    }
    fclose(head);

    head_content[strcspn(head_content, "\n")] = '\0';

    if (strncmp(head_content, "ref: ", 5) == 0) {
        // Read branch reference
        char branch_path[512];
        char* branch_ref = head_content + 5;

        snprintf(branch_path, sizeof(branch_path), ".avc/%s", branch_ref);
        FILE* branch_file = fopen(branch_path, "r");
        if (!branch_file) {
            return NULL;
        }

        char* commit_hash = malloc(65); // 64 chars + null terminator
        if (commit_hash && fgets(commit_hash, 65, branch_file)) {
            commit_hash[strcspn(commit_hash, "\n")] = '\0';
            fclose(branch_file);
            return commit_hash;
        }
        fclose(branch_file);
        if (commit_hash) free(commit_hash);
        return NULL;
    } else {
        // Direct commit hash
        char* commit_hash = malloc(strlen(head_content) + 1);
        if (commit_hash) {
            strcpy(commit_hash, head_content);
        }
        return commit_hash;
    }
}

int cmd_log(int argc, char* argv[]) {
    if (check_repo() == -1) {
        return 1;
    }

    int max_commits = 10; // Default to show last 10 commits
    int show_all = 0;

    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--all") == 0) {
            show_all = 1;
            max_commits = 1000; // Show many commits
        } else if (strncmp(argv[i], "-", 1) == 0 && strlen(argv[i]) > 1) {
            // Handle -n flag (e.g., -5 to show 5 commits)
            int n = atoi(argv[i] + 1);
            if (n > 0) {
                max_commits = n;
            }
        }
    }

    // Get current commit hash
    char* current_commit = get_current_commit_hash();
    if (!current_commit) {
        printf("No commits found\n");
        return 0;
    }

    printf("Showing last %d commit(s):\n\n", max_commits);

    char* commit_hash = current_commit;
    int count = 0;

    while (commit_hash && count < max_commits) {
        // Load commit object
        size_t commit_size;
        char commit_type[16];
        char* commit_content = load_object(commit_hash, &commit_size, commit_type);

        if (!commit_content || strcmp(commit_type, "commit") != 0) {
            printf("Warning: Invalid commit object: %s\n", commit_hash);
            break;
        }

        // Make a copy for parsing (since strtok modifies the string)
        char* content_copy = malloc(commit_size + 1);
        if (!content_copy) {
            free(commit_content);
            break;
        }
        strcpy(content_copy, commit_content);

        // Display commit
        display_commit(commit_hash, content_copy);

        // Look for parent commit
        char* parent_hash = NULL;
        char* line = strtok(commit_content, "\n");
        while (line) {
            if (strncmp(line, "parent ", 7) == 0) {
                parent_hash = malloc(65);
                if (parent_hash) {
                    // Copy the hash and ensure we strip any trailing newline or whitespace
                    strncpy(parent_hash, line + 7, 64);
                    parent_hash[64] = '\0';
                    // Remove any trailing newline that might have been copied
                    parent_hash[strcspn(parent_hash, "\n")] = '\0';
                }
                break;
            }
            line = strtok(NULL, "\n");
        }

        free(commit_content);
        free(content_copy);

        // Move to parent commit
        if (commit_hash != current_commit) {
            free(commit_hash);
        }
        commit_hash = parent_hash;
        count++;
    }

    if (current_commit) {
        free(current_commit);
    }
    if (commit_hash && commit_hash != current_commit) {
        free(commit_hash);
    }

    if (count == 0) {
        printf("No commits found\n");
    }

    return 0;
}