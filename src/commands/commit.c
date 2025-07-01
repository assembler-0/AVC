// src/commands/commit.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <omp.h>
#include "commands.h"
#include "repository.h"
#include "objects.h"
#include "index.h"
#include "arg_parser.h"
#include "tui.h"
#include "fast_index.h"

// Structure to hold file information for parallel processing
typedef struct {
    char hash[65];
    char filepath[256];
    unsigned int mode;
    char entry[512];
    int entry_len;
} file_entry_t;

// Comparator for qsort 
 // lexicographic order on filepath
static int file_entry_cmp(const void *a, const void *b) {
    const file_entry_t *fa = (const file_entry_t *)a;
    const file_entry_t *fb = (const file_entry_t *)b;
    return strcmp(fa->filepath, fb->filepath);
}

// Configure OpenMP for optimal performance
static void configure_parallel_processing() {
    // Get number of CPU cores
    int num_cores = omp_get_num_procs();
    
    // Set number of threads (use all cores for maximum performance)
    omp_set_num_threads(num_cores);
    
    // Set thread affinity for better performance
    #ifdef _GNU_SOURCE
    omp_set_schedule(omp_sched_dynamic, 1);
    #endif
}

// Tree node structure for building hierarchical trees
typedef struct tree_node {
    char name[256];
    char hash[65];
    unsigned int mode;
    int is_dir;
    struct tree_node* children;
    struct tree_node* next;
    int child_count;
} tree_node_t;

// Create a new tree node
static tree_node_t* create_tree_node(const char* name, const char* hash, unsigned int mode, int is_dir) {
    tree_node_t* node = calloc(1, sizeof(tree_node_t));
    if (!node) return NULL;

    strncpy(node->name, name, sizeof(node->name) - 1);
    if (hash) strncpy(node->hash, hash, sizeof(node->hash) - 1);
    node->mode = mode;
    node->is_dir = is_dir;
    return node;
}

// Find or create child node
static tree_node_t* find_or_create_child(tree_node_t* parent, const char* name) {
    tree_node_t* child = parent->children;
    while (child) {
        if (strcmp(child->name, name) == 0) {
            return child;
        }
        child = child->next;
    }

    // Create new directory child
    child = create_tree_node(name, NULL, 040000, 1);
    if (!child) return NULL;

    child->next = parent->children;
    parent->children = child;
    parent->child_count++;
    return child;
}

// Add file to tree structure
static int add_file_to_tree(tree_node_t* root, const char* filepath, const char* hash, unsigned int mode) {
    char path_copy[512];
    strncpy(path_copy, filepath, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';

    // Skip ./ prefix if present
    char* path = path_copy;
    if (strncmp(path, "./", 2) == 0) {
        path += 2;
    }

    tree_node_t* current = root;
    char* token = strtok(path, "/");
    char* next_token = NULL;

    while (token) {
        next_token = strtok(NULL, "/");

        if (next_token) {
            // This is a directory component
            current = find_or_create_child(current, token);
            if (!current) return -1;
        } else {
            // This is the file component
            tree_node_t* file_node = create_tree_node(token, hash, mode, 0);
            if (!file_node) return -1;

            file_node->next = current->children;
            current->children = file_node;
            current->child_count++;
        }

        token = next_token;
    }

    return 0;
}

// Compare tree nodes for sorting
static int compare_tree_nodes(const void* a, const void* b) {
    const tree_node_t* node_a = *(const tree_node_t**)a;
    const tree_node_t* node_b = *(const tree_node_t**)b;
    return strcmp(node_a->name, node_b->name);
}

// Create tree object from tree node (recursive)
static int create_tree_object_recursive(tree_node_t* node, char* tree_hash_out) {
    if (!node->children) {
        // Empty tree
        return store_object("tree", "", 0, tree_hash_out);
    }

    // Collect children into array for sorting
    tree_node_t** children_array = malloc(node->child_count * sizeof(tree_node_t*));
    if (!children_array) return -1;

    tree_node_t* child = node->children;
    int idx = 0;
    while (child && idx < node->child_count) {
        children_array[idx++] = child;
        child = child->next;
    }

    // Sort children alphabetically
    qsort(children_array, node->child_count, sizeof(tree_node_t*), compare_tree_nodes);

    // Build tree content
    size_t content_size = 0;
    char* tree_content = malloc(node->child_count * 512); // Estimate size
    if (!tree_content) {
        free(children_array);
        return -1;
    }

    for (int i = 0; i < node->child_count; i++) {
        tree_node_t* child_node = children_array[i];

        if (child_node->is_dir) {
            // Recursively create subtree
            char subtree_hash[65];
            if (create_tree_object_recursive(child_node, subtree_hash) != 0) {
                free(tree_content);
                free(children_array);
                return -1;
            }

            int entry_len = snprintf(tree_content + content_size, 512,
                                   "%o %s %s\n", 040000, child_node->name, subtree_hash);
            content_size += entry_len;
        } else {
            // File entry
            int entry_len = snprintf(tree_content + content_size, 512,
                                   "%o %s %s\n", child_node->mode, child_node->name, child_node->hash);
            content_size += entry_len;
        }
    }

    free(children_array);

    // Store tree object
    int result = store_object("tree", tree_content, content_size, tree_hash_out);
    free(tree_content);

    return result;
}

// Free tree structure
static void free_tree_node(tree_node_t* node) {
    if (!node) return;

    tree_node_t* child = node->children;
    while (child) {
        tree_node_t* next = child->next;
        free_tree_node(child);
        child = next;
    }

    free(node);
}

int create_tree(char* tree_hash) {
    // Use fast index for O(1) operations
    fast_index_t* fast_idx = fast_index_create();
    if (!fast_idx || fast_index_load(fast_idx) != 0) {
        fprintf(stderr, "Failed to load index\n");
        if (fast_idx) fast_index_free(fast_idx);
        return -1;
    }

    if (fast_idx->count == 0) {
        fprintf(stderr, "No files to commit (index is empty)\n");
        fast_index_free(fast_idx);
        return -1;
    }

    // Create root tree node
    tree_node_t* root = create_tree_node("", NULL, 040000, 1);
    if (!root) {
        fprintf(stderr, "Failed to create root tree node\n");
        fast_index_free(fast_idx);
        return -1;
    }

    // Build hierarchical tree structure
    for (int i = 0; i < FAST_INDEX_SIZE; i++) {
        index_entry_t* entry = fast_idx->buckets[i];
        while (entry) {
            if (add_file_to_tree(root, entry->path, entry->hash, entry->mode) != 0) {
                fprintf(stderr, "Failed to add file to tree: %s\n", entry->path);
                free_tree_node(root);
                fast_index_free(fast_idx);
                return -1;
            }
            entry = entry->next;
        }
    }

    fast_index_free(fast_idx);

    // Create tree objects recursively
    int result = create_tree_object_recursive(root, tree_hash);

    free_tree_node(root);

    if (result != 0) {
        fprintf(stderr, "Failed to create tree objects\n");
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

    // Parse command line options using the unified parser
    parsed_args_t* args = parse_args(argc, argv, "m"); // m=message
    if (!args) {
        fprintf(stderr, "Usage: avc commit [-m <message>]\n");
        fprintf(stderr, "  -m <message>: Specify commit message\n");
        return 1;
    }

    // Get commit message
    char message[1024];
    char* msg = get_message(args);
    if (msg) {
        strncpy(message, msg, sizeof(message) - 1);
        message[sizeof(message) - 1] = '\0';
    } else {
        printf("Enter a commit message (or use -m <msg>): ");
        if (!fgets(message, sizeof(message), stdin)) {
            fprintf(stderr, "Failed to read commit message\n");
            free_parsed_args(args);
            return 1;
        }
        message[strcspn(message, "\n")] = '\0';  // Remove newline
    }

    if (strlen(message) == 0) {
        fprintf(stderr, "Commit message cannot be empty\n");
        free_parsed_args(args);
        return 1;
    }

    tui_header("Creating Commit");
    clock_t start_time = clock();

    spinner_t* commit_spinner = spinner_create("Configuring parallel processing...");
    spinner_update(commit_spinner);

    // Configure OpenMP
    configure_parallel_processing();

    // Create tree from index
    spinner_set_label(commit_spinner, "Building hierarchical tree...");
    spinner_update(commit_spinner);
    char tree_hash[65]; // Updated to 65 for SHA-256
    if (create_tree(tree_hash) == -1) {
        spinner_stop(commit_spinner);
        spinner_free(commit_spinner);
        free_parsed_args(args);
        return 1;
    }

    // Get parent commit
    char parent_hash[65]; // Updated to 65 for SHA-256
    get_current_commit(parent_hash);

    // Create commit object
    spinner_set_label(commit_spinner, "Creating commit object...");
    spinner_update(commit_spinner);
    char commit_content[2048];
    time_t now = time(NULL);
    char* author = getenv("USER");
    if (!author) author = "unknown";

    // Get email from environment or use default
    char* email = getenv("EMAIL");
    if (!email) email = "user@example.com";

    // Format date as YYYY-MM-DD HH:MM:SS (Git format)
    struct tm* tm_info = gmtime(&now);
    char date_str[64];
    strftime(date_str, sizeof(date_str), "%Y-%m-%d %H:%M:%S", tm_info);

    if (strlen(parent_hash) > 0) {
        snprintf(commit_content, sizeof(commit_content),
                "tree %s\nparent %s\nauthor %s <%s> %s +0000\ncommitter %s <%s> %s +0000\n\n%s\n",
                tree_hash, parent_hash, author, email, date_str, author, email, date_str, message);
    } else {
        snprintf(commit_content, sizeof(commit_content),
                "tree %s\nauthor %s <%s> %s +0000\ncommitter %s <%s> %s +0000\n\n%s\n",
                tree_hash, author, email, date_str, author, email, date_str, message);
    }

    // Generate commit hash and store object
    char commit_hash[65]; // Updated to 65 for SHA-256
    if (store_object("commit", commit_content, strlen(commit_content), commit_hash) == -1) {
        spinner_stop(commit_spinner);
        spinner_free(commit_spinner);
        fprintf(stderr, "Failed to create commit object\n");
        free_parsed_args(args);
        return 1;
    }

    // Update HEAD
    spinner_set_label(commit_spinner, "Updating HEAD...");
    spinner_update(commit_spinner);
    if (update_head(commit_hash) == -1) {
        spinner_stop(commit_spinner);
        spinner_free(commit_spinner);
        fprintf(stderr, "Failed to update HEAD\n");
        free_parsed_args(args);
        return 1;
    }

    // Clear index
    spinner_set_label(commit_spinner, "Clearing index...");
    spinner_update(commit_spinner);
    if (clear_index() == -1) {
        fprintf(stderr, "Warning: Failed to clear index after commit\n");
    }

    spinner_stop(commit_spinner);
    spinner_free(commit_spinner);

    clock_t end_time = clock();
    double elapsed_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
    
    tui_success("Commit created successfully");
    printf("[main %.7s] %s\n", commit_hash, message);
    char time_msg[128];
    snprintf(time_msg, sizeof(time_msg), "Commit completed in %.3f seconds", elapsed_time);
    tui_info(time_msg);

    // Clean up memory pool to prevent memory leaks
    reset_memory_pool();

    free_parsed_args(args);
    return 0;
}