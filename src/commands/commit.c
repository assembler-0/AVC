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

// Structure to hold file information for parallel processing
typedef struct {
    char hash[65];
    char filepath[256];
    unsigned int mode;
    char entry[512];
    int entry_len;
} file_entry_t;

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
    
    printf("Using %d threads for parallel processing\n", num_cores);
}

int create_tree(char* tree_hash) {
    FILE* index = fopen(".avc/index", "r");
    if (!index) {
        fprintf(stderr, "No files to commit (index is empty)\n");
        return -1;
    }

    // First pass: count files and collect file information
    char line[1024];
    int file_count = 0;
    
    // Count total files
    while (fgets(line, sizeof(line), index)) {
        file_count++;
    }
    rewind(index);
    
    if (file_count == 0) {
        fclose(index);
        fprintf(stderr, "No files to commit (index is empty)\n");
        return -1;
    }
    
    // Allocate array for file entries
    file_entry_t* files = malloc(file_count * sizeof(file_entry_t));
    if (!files) {
        fprintf(stderr, "Memory allocation failed\n");
        fclose(index);
        return -1;
    }
    
    // Second pass: collect file information
    int idx = 0;
    while (fgets(line, sizeof(line), index) && idx < file_count) {
        char hash[65], filepath[256];
        unsigned int mode;

        if (sscanf(line, "%64s %255s %o", hash, filepath, &mode) == 3) {
            strcpy(files[idx].hash, hash);
            strcpy(files[idx].filepath, filepath);
            files[idx].mode = mode;
            
            // Pre-create entry string
            files[idx].entry_len = snprintf(files[idx].entry, sizeof(files[idx].entry), 
                                          "%o %s %s\n", mode, filepath, hash);
            idx++;
        }
    }
    fclose(index);
    
    int actual_file_count = idx;
    
    // Sort entries for consistent tree creation
    for (int i = 0; i < actual_file_count - 1; i++) {
        for (int j = i + 1; j < actual_file_count; j++) {
            if (strcmp(files[i].filepath, files[j].filepath) > 0) {
                file_entry_t temp = files[i];
                files[i] = files[j];
                files[j] = temp;
            }
        }
    }
    
    // Calculate total size needed for tree content
    size_t total_size = 0;
    for (int i = 0; i < actual_file_count; i++) {
        total_size += files[i].entry_len;
    }
    
    // Allocate tree content buffer
    char* tree_content = malloc(total_size + 1);
    if (!tree_content) {
        fprintf(stderr, "Memory allocation failed\n");
        free(files);
        return -1;
    }
    
    // Build tree content
    size_t offset = 0;
    printf("Creating tree with %d files...\n", actual_file_count);
    
    for (int i = 0; i < actual_file_count; i++) {
        memcpy(tree_content + offset, files[i].entry, files[i].entry_len);
        offset += files[i].entry_len;
    }
    tree_content[total_size] = '\0';
    
    free(files);
    
    // Generate tree hash
    printf("Storing tree object...\n");
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

    // Parse command line options using the unified parser
    parsed_args_t* args = parse_args(argc, argv, "mn"); // m=message, n=no-compression
    if (!args) {
        return 1;
    }
    
    // Set compression based on flag
    int no_compression = has_flag(args, FLAG_NO_COMPRESSION);
    set_compression_enabled(!no_compression);
    if (no_compression) {
        printf("Compression disabled for maximum speed\n");
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

    printf("Starting commit process...\n");
    clock_t start_time = clock();

    // Configure OpenMP
    configure_parallel_processing();

    // Create tree from index
    printf("Creating tree object...\n");
    char tree_hash[65]; // Updated to 65 for SHA-256
    if (create_tree(tree_hash) == -1) {
        free_parsed_args(args);
        return 1;
    }

    // Get parent commit
    char parent_hash[65]; // Updated to 65 for SHA-256
    get_current_commit(parent_hash);

    // Create commit object
    printf("Creating commit object...\n");
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
        free_parsed_args(args);
        return 1;
    }

    // Update HEAD
    if (update_head(commit_hash) == -1) {
        fprintf(stderr, "Failed to update HEAD\n");
        free_parsed_args(args);
        return 1;
    }

    // Clear index
    if (clear_index() == -1) {
        fprintf(stderr, "Warning: Failed to clear index after commit\n");
    }

    clock_t end_time = clock();
    double elapsed_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
    
    printf("[main %.7s] %s\n", commit_hash, message);
    printf("Commit completed in %.3f seconds\n", elapsed_time);

    free_parsed_args(args);
    return 0;
}