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
#include "fast_index.h"
#include "tui.h"
// Fast directory creation with minimal stat calls
int create_directory_recursive(const char* path) {
    char path_buf[1024];
    strncpy(path_buf, path, sizeof(path_buf) - 1);
    path_buf[sizeof(path_buf) - 1] = '\0';
    
    char* dir = dirname(path_buf);
    if (strcmp(dir, ".") == 0 || strcmp(dir, "/") == 0) {
        return 0;
    }
    
    // Try to create directory first, then recurse only if needed
    if (mkdir(dir, 0755) == 0 || errno == EEXIST) {
        return 0;
    }
    
    if (errno == ENOENT) {
        // Parent doesn't exist, recurse
        if (create_directory_recursive(dir) == 0) {
            return mkdir(dir, 0755) == 0 || errno == EEXIST ? 0 : -1;
        }
    }
    
    return -1;
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

// File entry for flattened tree
typedef struct {
    char path[1024];
    char hash[65];
    unsigned int mode;
} file_entry_reset_t;

// Recursively flatten hierarchical tree into file list
static int flatten_tree_recursive(const char* tree_hash, const char* base_path, file_entry_reset_t** files, int* count, int* capacity) {
    size_t tree_size;
    char tree_type[16];
    char* tree_content = load_object(tree_hash, &tree_size, tree_type);
    
    if (!tree_content || strcmp(tree_type, "tree") != 0) {
        if (tree_content) free(tree_content);
        return -1;
    }
    
    char* tree_copy = malloc(tree_size + 1);
    if (!tree_copy) {
        free(tree_content);
        return -1;
    }
    memcpy(tree_copy, tree_content, tree_size);
    tree_copy[tree_size] = '\0';
    free(tree_content);
    
    char* current_pos = tree_copy;
    char* end_of_buffer = tree_copy + tree_size;
    
    while (current_pos < end_of_buffer) {
        char* next_line = strchr(current_pos, '\n');
        if (next_line) *next_line = '\0';
        
        if (strlen(current_pos) > 0) {
            unsigned int mode;
            char name[256], hash[65];
            
            if (sscanf(current_pos, "%o %255s %64s", &mode, name, hash) == 3) {
                char full_path[1024];
                if (strlen(base_path) > 0) {
                    snprintf(full_path, sizeof(full_path), "%s/%s", base_path, name);
                } else {
                    strcpy(full_path, name);
                }
                
                if (mode == 040000) {
                    // Directory - recurse
                    if (flatten_tree_recursive(hash, full_path, files, count, capacity) != 0) {
                        free(tree_copy);
                        return -1;
                    }
                } else {
                    // File - add to flattened list
                    if (*count >= *capacity) {
                        *capacity = *capacity ? *capacity * 2 : 1024;
                        *files = realloc(*files, *capacity * sizeof(file_entry_reset_t));
                        if (!*files) {
                            free(tree_copy);
                            return -1;
                        }
                    }
                    
                    snprintf((*files)[*count].path, sizeof((*files)[*count].path), "./%s", full_path);
                    strcpy((*files)[*count].hash, hash);
                    (*files)[*count].mode = mode;
                    (*count)++;
                }
            }
        }
        
        if (next_line) {
            current_pos = next_line + 1;
        } else {
            break;
        }
    }
    
    free(tree_copy);
    return 0;
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

    // Use fast index for O(1) operations
    fast_index_t* fast_idx = fast_index_create();
    if (!fast_idx) {
        fprintf(stderr, "Failed to create index\n");
        free(tree_content);
        return -1;
    }

    // Flatten hierarchical tree into file list for batch processing
    file_entry_reset_t* files = NULL;
    int file_count = 0, file_capacity = 0;
    
    if (flatten_tree_recursive(tree_hash, "", &files, &file_count, &file_capacity) != 0) {
        fprintf(stderr, "Failed to flatten tree structure\n");
        fast_index_free(fast_idx);
        if (files) free(files);
        return -1;
    }
    
    // Batch process all files for maximum performance
    int files_processed = 0;
    for (int i = 0; i < file_count; i++) {
        if (fast_index_set(fast_idx, files[i].path, files[i].hash, files[i].mode) == 0) {
            files_processed++;
        }
    }
    
    // Parallel file restoration if hard reset
    if (hard_reset) {
        #pragma omp parallel for schedule(dynamic, 64)
        for (int i = 0; i < file_count; i++) {
            size_t file_size;
            char file_type[16];
            char* file_content = load_object(files[i].hash, &file_size, file_type);
            
            if (file_content && strcmp(file_type, "blob") == 0) {
                // Remove ./ prefix for file creation
                const char* file_path = files[i].path;
                if (strncmp(file_path, "./", 2) == 0) file_path += 2;
                
                create_directory_recursive(file_path);
                write_file(file_path, file_content, file_size);
                free(file_content);
            }
        }
    }
    
    free(files);
    
    free(tree_content);
    
    // Commit fast index to disk
    if (fast_index_commit(fast_idx) != 0) {
        fprintf(stderr, "Failed to commit index\n");
        fast_index_free(fast_idx);
        return -1;
    }
    fast_index_free(fast_idx);

    printf("Processed %d files from hierarchical tree\n", files_processed);

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

    tui_header("Reset Operation");
    printf("Resetting to commit %s%s...\n", target_hash_str, hard_reset ? " (hard)" : "");
    
    spinner_t* reset_spinner = spinner_create("Processing reset");
    spinner_update(reset_spinner);

    int result = reset_to_commit(target_hash_str, hard_reset);
    
    spinner_stop(reset_spinner);
    spinner_free(reset_spinner);
    
    if (result == -1) {
        tui_error("Reset operation failed");
        free_parsed_args(args);
        return 1;
    }

    tui_success("Reset operation completed");
    free_parsed_args(args);
    return 0;
}