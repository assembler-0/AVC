// src/commands/reset.c - FIXED VERSION
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <libgen.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>
#include "commands.h"
#include "repository.h"
#include "index.h"
#include "objects.h"
#include "file_utils.h"
#include "arg_parser.h"
// Helper function to create directory recursively
int create_directory_recursive(const char* path) {
    char* path_copy = strdup2(path);
    if (!path_copy) return -1;

    char* dir = dirname(path_copy);
    if (strcmp(dir, ".") == 0 || strcmp(dir, "/") == 0) {
        free(path_copy);
        return 0;
    }

    struct stat st;
    if (stat(dir, &st) == -1) {
        if (create_directory_recursive(dir) == -1) {
            free(path_copy);
            return -1;
        }
        if (mkdir(dir, 0755) == -1 && errno != EEXIST) {
            perror("mkdir");
            free(path_copy);
            return -1;
        }
    }

    free(path_copy);
    return 0;
}

// Recursively delete all files and directories except .avc, .git, .idea
static int clean_working_directory(const char* path) {
    DIR* dir = opendir(path);
    if (!dir) return -1;
    struct dirent* entry;
    int result = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        if (strcmp(entry->d_name, ".avc") == 0 || strcmp(entry->d_name, ".git") == 0 || strcmp(entry->d_name, ".idea") == 0) continue;
        char fullpath[1024];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);
        struct stat st;
        if (stat(fullpath, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                if (clean_working_directory(fullpath) == -1) result = -1;
                if (rmdir(fullpath) == -1) result = -1;
            } else {
                if (unlink(fullpath) == -1) result = -1;
            }
        }
    }
    closedir(dir);
    return result;
}

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
    char* content_copy = malloc(commit_size + 1);
    if (!content_copy) {
        free(commit_content);
        return -1;
    }
    memcpy(content_copy, commit_content, commit_size);
    content_copy[commit_size] = '\0';

    char* line_start = content_copy;
    char* line_end;

    while ((line_end = strchr(line_start, '\n')) != NULL) {
        *line_end = '\0';
        if (strncmp(line_start, "tree ", 5) == 0) {
            strncpy(tree_hash, line_start + 5, 64);
            tree_hash[64] = '\0';
            break;
        }
        line_start = line_end + 1;
    }

    free(commit_content);
    free(content_copy);

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

    // Parse tree content line by line - FIXED VERSION
    char* tree_copy = malloc(tree_size + 1);
    if (!tree_copy) {
        fprintf(stderr, "Memory allocation failed\n");
        free(tree_content);
        return -1;
    }
    memcpy(tree_copy, tree_content, tree_size);
    tree_copy[tree_size] = '\0';
    free(tree_content);

    int files_processed = 0;
    line_start = tree_copy;

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
                // Create directory structure if needed
                if (create_directory_recursive(filepath) == -1) {
                    fprintf(stderr, "Failed to create directory structure for: %s\n", filepath);
                    free(file_content);
                    line_start = line_end + 1;
                    continue;
                }

                // Write file to working directory
                if (write_file(filepath, file_content, file_size) == -1) {
                    fprintf(stderr, "Failed to restore file: %s\n", filepath);
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

    free(tree_copy);

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

                // Create refs/heads directory if it doesn't exist
                char refs_heads_dir[512];
                snprintf(refs_heads_dir, sizeof(refs_heads_dir), ".avc/refs/heads");
                struct stat st;
                if (stat(refs_heads_dir, &st) == -1) {
                    if (mkdir(".avc/refs", 0755) == -1 && errno != EEXIST) {
                        perror("mkdir .avc/refs");
                    }
                    if (mkdir(refs_heads_dir, 0755) == -1 && errno != EEXIST) {
                        perror("mkdir .avc/refs/heads");
                    }
                }

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

    // Parse arguments using the unified parser
    parsed_args_t* args = parse_args(argc, argv, "hl"); // h=hard, l=clean
    if (!args) {
        return 1;
    }

    if (get_positional_count(args) == 0) {
        fprintf(stderr, "Usage: avc reset [--hard] [--clean] <commit-hash>\n");
        fprintf(stderr, "  --hard: Reset working directory and index\n");
        fprintf(stderr, "  --clean: Wipe working directory (except .avc, .git, .idea) before restoring\n");
        fprintf(stderr, "  (default): Reset only index, keep working directory\n");
        fprintf(stderr, "  You can also use: avc reset [--hard] [--clean] HEAD~1  (previous commit)\n");
        free_parsed_args(args);
        return 1;
    }

    int hard_reset = has_flag(args, FLAG_HARD);
    int clean_flag = has_flag(args, FLAG_CLEAN);
    char** positional = get_positional_args(args);
    char* target_hash_str = positional[0]; // First positional argument

    if (!target_hash_str) {
        fprintf(stderr, "Please specify a commit hash\n");
        free_parsed_args(args);
        return 1;
    }

    char resolved_hash[65];

    // Handle HEAD and HEAD~1 resolution
    if (strcmp(target_hash_str, "HEAD") == 0 || strcmp(target_hash_str, "HEAD~1") == 0) {
        // Get the current commit hash from the current branch
        FILE* head_file = fopen(".avc/HEAD", "r");
        if (!head_file) {
            perror("Failed to open .avc/HEAD");
            free_parsed_args(args);
            return 1;
        }

        char head_content[256];
        if (!fgets(head_content, sizeof(head_content), head_file)) {
            fclose(head_file);
            fprintf(stderr, "Failed to read .avc/HEAD\n");
            free_parsed_args(args);
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
                free_parsed_args(args);
                return 1;
            }
        } else {
            // HEAD contains commit hash directly
            strncpy(current_commit_hash, head_content, sizeof(current_commit_hash) - 1);
            printf("Current commit from HEAD: %s\n", current_commit_hash);
        }

        if (strlen(current_commit_hash) == 0) {
            fprintf(stderr, "Could not resolve HEAD commit.\n");
            free_parsed_args(args);
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
                free_parsed_args(args);
                return 1;
            }

            // Look for parent line
            char* content_copy = malloc(commit_size + 1);
            if (!content_copy) {
                free(commit_content);
                free_parsed_args(args);
                return -1;
            }
            memcpy(content_copy, commit_content, commit_size);
            content_copy[commit_size] = '\0';

            char* line_start = content_copy;
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
                line_start = line_end + 1;
            }
            free(commit_content);
            free(content_copy);

            if (!parent_found) {
                fprintf(stderr, "HEAD has no parent commit to reset to.\n");
                free_parsed_args(args);
                return 1;
            }
        }
        target_hash_str = resolved_hash;
    }

    if (strlen(target_hash_str) != 64) {
        fprintf(stderr, "Invalid commit hash format (expected 64 characters, got %zu)\n", strlen(target_hash_str));
        fprintf(stderr, "Use 'avc log' to see available commit hashes\n");
        free_parsed_args(args);
        return 1;
    }

    if (clean_flag) {
        printf("This will delete ALL files and directories except .avc, .git, and .idea.\n");
        printf("Type 'yes' to confirm, or anything else to cancel: ");
        char answer[16];
        if (!fgets(answer, sizeof(answer), stdin)) {
            fprintf(stderr, "Failed to read input. Aborting.\n");
            free_parsed_args(args);
            return 1;
        }
        answer[strcspn(answer, "\n")] = '\0';
        if (strcmp(answer, "yes") != 0) {
            printf("Aborted by user.\n");
            free_parsed_args(args);
            return 1;
        }
        if (clean_working_directory(".") == -1) {
            fprintf(stderr, "Failed to clean working directory.\n");
            free_parsed_args(args);
            return 1;
        }
        printf("Working directory cleaned.\n");
    }

    printf("Resetting to commit %s%s...\n", target_hash_str, hard_reset ? " (hard)" : "");

    if (reset_to_commit(target_hash_str, hard_reset) == -1) {
        free_parsed_args(args);
        return 1;
    }

    printf("Reset complete.\n");
    free_parsed_args(args);
    return 0;
}