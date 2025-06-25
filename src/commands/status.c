// src/commands/status.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "commands.h"
#include "repository.h"
#include "objects.h"
#include "tui.h"
#include "fast_index.h"
#include <stdint.h>

// ANSI color codes
#define ANSI_RESET "\033[0m"
#define ANSI_YELLOW "\033[33m"
#define ANSI_BRIGHT_GREEN "\033[92m"

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

// Simple hash table for tree entries (open addressing, FNV-1a hash)
#define TREE_TABLE_SIZE 4096

typedef struct {
    char path[256];
    char hash[65];
    int used;
} tree_entry_t;

static uint32_t fnv1a_hash(const char* s) {
    uint32_t h = 2166136261u;
    while (*s) h = (h ^ (unsigned char)*s++) * 16777619u;
    return h;
}

// Parse tree content into hash table
static void build_tree_table(const char* tree_content, size_t tree_size, tree_entry_t* table) {
    char* content_copy = malloc(tree_size + 1);
    memcpy(content_copy, tree_content, tree_size);
    content_copy[tree_size] = '\0';
    char* line = strtok(content_copy, "\n");
    while (line) {
        unsigned int mode; char path[256], hash[65];
        if (sscanf(line, "%o %255s %64s", &mode, path, hash) == 3) {
            uint32_t idx = fnv1a_hash(path) % TREE_TABLE_SIZE;
            for (int i = 0; i < TREE_TABLE_SIZE; ++i) {
                uint32_t j = (idx + i) % TREE_TABLE_SIZE;
                if (!table[j].used) {
                    strcpy(table[j].path, path);
                    strcpy(table[j].hash, hash);
                    table[j].used = 1;
                    break;
                }
            }
        }
        line = strtok(NULL, "\n");
    }
    free(content_copy);
}

static const char* lookup_tree_hash(tree_entry_t* table, const char* path) {
    uint32_t idx = fnv1a_hash(path) % TREE_TABLE_SIZE;
    for (int i = 0; i < TREE_TABLE_SIZE; ++i) {
        uint32_t j = (idx + i) % TREE_TABLE_SIZE;
        if (!table[j].used) return NULL;
        if (strcmp(table[j].path, path) == 0) return table[j].hash;
    }
    return NULL;
}

int cmd_status(int argc, char* argv[]) {
    if (check_repo() == -1) {
        return 1;
    }

    tui_header("Repository Status");
    printf("On branch main\n");

    struct stat st;
    if (stat(".avc/index", &st) == -1 || st.st_size == 0) {
        tui_info("No changes to be committed");
        return 0;
    }

    // Get the tree hash from the last commit
    char last_tree_hash[65];
    get_last_commit_tree(last_tree_hash);

    // Load and parse last commit's tree ONCE
    tree_entry_t* tree_table = calloc(TREE_TABLE_SIZE, sizeof(tree_entry_t));
    if (strlen(last_tree_hash) > 0) {
        size_t tree_size; char tree_type[16];
        char* tree_content = load_object(last_tree_hash, &tree_size, tree_type);
        if (tree_content && strcmp(tree_type, "tree") == 0) {
            build_tree_table(tree_content, tree_size, tree_table);
            free(tree_content);
        }
    }

    // Use fast index for O(1) operations
    fast_index_t* fast_idx = fast_index_create();
    if (!fast_idx || fast_index_load(fast_idx) != 0) {
        tui_error("Failed to load index");
        if (fast_idx) fast_index_free(fast_idx);
        free(tree_table);
        return 1;
    }

    int has_staged = 0;
    printf("Changes to be committed:\n");
    printf("  (use \"avc commit\" to commit)\n\n");
    
    // Fast iteration through hash table buckets
    for (int i = 0; i < FAST_INDEX_SIZE; i++) {
        index_entry_t* entry = fast_idx->buckets[i];
        while (entry) {
            const char* old_hash = lookup_tree_hash(tree_table, entry->path);
            if (old_hash && strcmp(old_hash, entry->hash) == 0) {
                printf("  " ANSI_YELLOW "modified:   %s" ANSI_RESET "\n", entry->path);
            } else {
                printf("  " ANSI_BRIGHT_GREEN "new file:   %s" ANSI_RESET "\n", entry->path);
            }
            has_staged = 1;
            entry = entry->next;
        }
    }
    
    fast_index_free(fast_idx);
    free(tree_table);
    if (!has_staged) {
        printf("No changes to be committed.\n");
    }
    printf("\n");
    return 0;
}