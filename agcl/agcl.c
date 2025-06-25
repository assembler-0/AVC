// AVC Git Compatibility Layer (AGCL)
// Handles conversion between AVC and Git formats
#define _XOPEN_SOURCE 700   /* for strptime */
#define _GNU_SOURCE 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <openssl/sha.h>
#include <zlib.h>
#include "commands.h"
#include "repository.h"
#include "objects.h"
#include "fast_agcl.h"
#include "tui.h"
#include <blake3/blake3.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>


// Git object types
#define GIT_BLOB_TYPE "blob"
#define GIT_TREE_TYPE "tree"
#define GIT_COMMIT_TYPE "commit"

// Git object format: "type size\0content" (same as AVC)
typedef struct {
    char type[16];
    size_t size;
    char* content;
} git_object_t;

// Convert BLAKE3 hash to SHA-1 (for Git compatibility)
// This is a simplified conversion - in practice you'd need a proper mapping
static void blake3_to_sha1(const char* blake3_hash, char* sha1_hash) {
    // For now, we'll use a simple truncation + padding approach
    // In a real implementation, you'd maintain a mapping table
    SHA_CTX ctx;
    unsigned char digest[SHA_DIGEST_LENGTH];
    
    SHA1_Init(&ctx);
    SHA1_Update(&ctx, blake3_hash, strlen(blake3_hash));
    SHA1_Final(digest, &ctx);
    
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
        sprintf(sha1_hash + i * 2, "%02x", digest[i]);
    }
    sha1_hash[40] = '\0';
}

// Convert SHA-1 hash to BLAKE3 (reverse mapping)
static void sha1_to_blake3(const char* sha1_hash, char* blake3_hash) {
    // This would require a reverse mapping table
    // For now, we'll use a simple approach
    blake3_hasher hasher;
    blake3_hasher_init(&hasher);
    blake3_hasher_update(&hasher, sha1_hash, strlen(sha1_hash));
    
    uint8_t digest[32];
    blake3_hasher_finalize(&hasher, digest, 32);
    
    for (int i = 0; i < 32; i++) {
        sprintf(blake3_hash + i * 2, "%02x", digest[i]);
    }
    blake3_hash[64] = '\0';
}

// Path to mapping file that stores lines: "<avc_hash> <git_hash>\n"
#define AGCL_MAP_PATH ".git/avc-map"

// Forward declarations
static int read_mapping(const char* avc_hash, char* git_hash_out);
static int append_mapping(const char* avc_hash, const char* git_hash);

// Helper: check if a Git object already exists
static int git_object_exists(const char* git_hash) {
    char path[512];
    snprintf(path, sizeof(path), ".git/objects/%c%c/%s", git_hash[0], git_hash[1], git_hash + 2);
    return access(path, F_OK) == 0;
}

// Global hash mapping cache
static hash_map_t* g_hash_map = NULL;

// Initialize hash mapping cache
static void init_hash_map(void) {
    if (!g_hash_map) {
        g_hash_map = hash_map_create();
        hash_map_load(g_hash_map);
    }
}

// Fast mapping lookup
static int read_mapping(const char* avc_hash, char* git_hash_out) {
    init_hash_map();
    const char* git_hash = hash_map_get(g_hash_map, avc_hash);
    if (git_hash) {
        strcpy(git_hash_out, git_hash);
        return 0;
    }
    return -1;
}

// Fast mapping insert
static int append_mapping(const char* avc_hash, const char* git_hash) {
    init_hash_map();
    return hash_map_set(g_hash_map, avc_hash, git_hash);
}

// Convert ISO 8601 datetime ("YYYY-MM-DD HH:MM:SS") to epoch seconds (UTC)
static long iso_to_epoch(const char *iso_str) {
    struct tm tm = {0};
    // Attempt to parse the incoming date string
    if (!strptime(iso_str, "%Y-%m-%d %H:%M:%S", &tm)) {
        // Try alternative format without seconds
        if (!strptime(iso_str, "%Y-%m-%d %H:%M", &tm)) {
            // If parsing fails, fall back to current time to ensure a valid timestamp
            printf("Warning: Failed to parse date '%s', using current time\n", iso_str);
            return time(NULL);
        }
    }
    // Convert to epoch (try timegm for UTC, fall back to mktime which assumes local time)
    #ifdef _GNU_SOURCE
    return timegm(&tm);
    #else
    // For systems without timegm, manually adjust for UTC
    time_t local_time = mktime(&tm);
    struct tm *utc_tm = gmtime(&local_time);
    time_t utc_time = mktime(utc_tm);
    return local_time + (local_time - utc_time);
    #endif
}

// Compress data using zlib (Git's compression)
static char* git_compress(const char* data, size_t size, size_t* compressed_size) {
    uLong compressed_len = compressBound(size);
    char* compressed = malloc(compressed_len);
    if (!compressed) return NULL;
    
    if (compress2((Bytef*)compressed, &compressed_len, 
                  (const Bytef*)data, size, Z_DEFAULT_COMPRESSION) != Z_OK) {
        free(compressed);
        return NULL;
    }
    
    *compressed_size = compressed_len;
    return compressed;
}

// Decompress data using zlib
static char* git_decompress(const char* compressed_data, size_t compressed_size, size_t expected_size) {
    char* decompressed = malloc(expected_size);
    if (!decompressed) return NULL;
    
    uLong decompressed_len = expected_size;
    if (uncompress((Bytef*)decompressed, &decompressed_len,
                   (const Bytef*)compressed_data, compressed_size) != Z_OK) {
        free(decompressed);
        return NULL;
    }
    
    return decompressed;
}

// Store Git object (using SHA-1 and zlib)
static int store_git_object(const char* type, const char* content, size_t size, char* git_hash_out) {
    // Create Git-style object format
    char header[64];
    int header_len = snprintf(header, sizeof(header), "%s %zu", type, size);
    
    size_t full_size = header_len + 1 + size;
    char* full_content = malloc(full_size);
    if (!full_content) return -1;
    
    memcpy(full_content, header, header_len);
    full_content[header_len] = '\0';
    memcpy(full_content + header_len + 1, content, size);
    
    // Calculate SHA-1 hash
    SHA_CTX ctx;
    unsigned char digest[SHA_DIGEST_LENGTH];
    
    SHA1_Init(&ctx);
    SHA1_Update(&ctx, full_content, full_size);
    SHA1_Final(digest, &ctx);
    
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
        sprintf(git_hash_out + i * 2, "%02x", digest[i]);
    }
    git_hash_out[40] = '\0';
    
    // Compress with zlib
    size_t compressed_size;
    char* compressed = git_compress(full_content, full_size, &compressed_size);
    free(full_content);
    
    if (!compressed) return -1;
    
    // Create Git object path
    char git_obj_path[512];
    snprintf(git_obj_path, sizeof(git_obj_path), ".git/objects/%c%c/%s", 
             git_hash_out[0], git_hash_out[1], git_hash_out + 2);
    
    // Create directory
    char git_obj_dir[512];
    snprintf(git_obj_dir, sizeof(git_obj_dir), ".git/objects/%c%c", 
             git_hash_out[0], git_hash_out[1]);
    
    if (mkdir(git_obj_dir, 0755) == -1 && errno != EEXIST) {
        free(compressed);
        return -1;
    }
    
    // Write compressed object
    FILE* obj_file = fopen(git_obj_path, "wb");
    if (!obj_file) {
        free(compressed);
        return -1;
    }
    
    fwrite(compressed, 1, compressed_size, obj_file);
    fclose(obj_file);
    free(compressed);
    
    return 0;
}

// Convert AVC blob to Git blob
static int convert_avc_blob_to_git(const char* avc_hash, char* git_hash_out) {
    if (read_mapping(avc_hash, git_hash_out) == 0 && git_object_exists(git_hash_out)) {
        return 0;
    }
    // Load AVC blob object
    size_t blob_size;
    char blob_type[16];
    char* blob_content = load_object(avc_hash, &blob_size, blob_type);
    
    if (!blob_content || strcmp(blob_type, "blob") != 0) {
        free(blob_content);
        return -1;
    }
    
    // Store as Git blob
    int result = store_git_object("blob", blob_content, blob_size, git_hash_out);
    free(blob_content);
    if (result == 0) {
        append_mapping(avc_hash, git_hash_out);
    }
    return result;
}

// Convert AVC tree to Git tree
static int convert_avc_tree_to_git(const char* avc_hash, char* git_hash_out) {
    if (read_mapping(avc_hash, git_hash_out) == 0 && git_object_exists(git_hash_out)) {
        return 0;
    }
    // Load AVC tree object
    size_t tree_size;
    char tree_type[16];
    char* tree_content = load_object(avc_hash, &tree_size, tree_type);
    
    if (!tree_content || strcmp(tree_type, "tree") != 0) {
        free(tree_content);
        return -1;
    }
    
    // Parse tree entries and convert hashes
    char* tree_copy = malloc(tree_size + 1);
    if (!tree_copy) {
        free(tree_content);
        return -1;
    }
    memcpy(tree_copy, tree_content, tree_size);
    tree_copy[tree_size] = '\0';
    
    // Create a working copy for parsing
    char* work_copy = malloc(tree_size + 1);
    if (!work_copy) {
        free(tree_copy);
        free(tree_content);
        return -1;
    }
    memcpy(work_copy, tree_content, tree_size);
    work_copy[tree_size] = '\0';
    
    // Git tree format: binary entries with format: mode filename\0hash
    // We need to calculate total size first
    size_t total_size = 0;
    int entry_count = 0;
    
    // First pass: count entries and calculate size
    char* line_start = work_copy;
    char* line_end;
    while ((line_end = strchr(line_start, '\n')) != NULL) {
        *line_end = '\0';
        
        if (strlen(line_start) > 0) {
            unsigned int mode;
            char filepath[256], avc_hash_entry[65];
            
            if (sscanf(line_start, "%o %255s %64s", &mode, filepath, avc_hash_entry) == 3) {
                // Each entry: mode digits + space + filename + null + hash (20 bytes)
                int mode_len = snprintf(NULL, 0, "%o", mode);
                total_size += mode_len + 1 + strlen(filepath) + 1 + 20;
                entry_count++;
            }
        }
        
        line_start = line_end + 1;
    }

    // Restore work_copy because newline characters were replaced with NULs during the first pass
    memcpy(work_copy, tree_content, tree_size);
    work_copy[tree_size] = '\0';

    printf("Tree has %d entries, total size: %zu\n", entry_count, total_size);
    
    // Allocate buffer for Git tree content
    char* git_tree_content = malloc(total_size);
    if (!git_tree_content) {
        free(work_copy);
        free(tree_copy);
        free(tree_content);
        return -1;
    }
    
    size_t git_tree_offset = 0;
    
    // Second pass: build Git tree content
    line_start = work_copy;
    while ((line_end = strchr(line_start, '\n')) != NULL) {
        *line_end = '\0';
        
        if (strlen(line_start) > 0) {
            unsigned int mode;
            char filepath[256], avc_hash_entry[65];
            
            if (sscanf(line_start, "%o %255s %64s", &mode, filepath, avc_hash_entry) == 3) {
                
                char git_hash_entry[41];
                if (mode == 040000) {
                    // sub-tree
                    if (convert_avc_tree_to_git(avc_hash_entry, git_hash_entry) != 0) {
                        printf("Failed to convert sub-tree %s\n", avc_hash_entry);
                        continue;
                    }
                } else {
                    // treat as blob (regular file or executable, symlink, etc.)
                    if (convert_avc_blob_to_git(avc_hash_entry, git_hash_entry) != 0) {
                        printf("Failed to convert blob %s\n", avc_hash_entry);
                        continue;
                    }
                }
                
                // Convert hex hash to binary
                unsigned char hash_binary[20];
                for (int i = 0; i < 20; i++) {
                    char hex_pair[3] = {git_hash_entry[i*2], git_hash_entry[i*2+1], '\0'};
                    hash_binary[i] = (unsigned char)strtol(hex_pair, NULL, 16);
                }
                
                // Format: "mode filename\0hash" - Git tree format
                int len = snprintf(git_tree_content + git_tree_offset, 
                                 total_size - git_tree_offset, "%o %s", mode, filepath);
                git_tree_offset += len;
                git_tree_content[git_tree_offset++] = '\0';
                memcpy(git_tree_content + git_tree_offset, hash_binary, 20);
                git_tree_offset += 20;

            }
        }
        
        line_start = line_end + 1;
    }
    
    printf("Git tree content size: %zu\n", git_tree_offset);
    
    // Store as Git tree
    int result = store_git_object("tree", git_tree_content, git_tree_offset, git_hash_out);
    if (result == 0) {
        append_mapping(avc_hash, git_hash_out);
    }
    
    free(work_copy);
    free(tree_copy);
    free(git_tree_content);
    free(tree_content);
    
    return result;
}

// Convert AVC commit to Git commit
static int convert_avc_commit_to_git(const char* avc_hash, char* git_hash_out) {
    char mapped_hash[41];
    if (read_mapping(avc_hash, mapped_hash) == 0 && git_object_exists(mapped_hash)) {
        if (git_hash_out) strcpy(git_hash_out, mapped_hash);
        return 0;
    }
    // Load AVC commit object
    size_t commit_size;
    char commit_type[16];
    char* commit_content = load_object(avc_hash, &commit_size, commit_type);
    
    if (!commit_content || strcmp(commit_type, "commit") != 0) {
        free(commit_content);
        return -1;
    }
    
    printf("Converting commit %s (size: %zu)\n", avc_hash, commit_size);
    
    // Parse commit and convert tree/parent hashes
    char* commit_copy = malloc(commit_size + 1);
    if (!commit_copy) {
        free(commit_content);
        return -1;
    }
    memcpy(commit_copy, commit_content, commit_size);
    commit_copy[commit_size] = '\0';
    
    char* git_commit_content = malloc(commit_size * 2);
    size_t git_commit_offset = 0;
    
    // Find the empty line that separates headers from message
    char* message_start = strstr(commit_copy, "\n\n");
    if (message_start) {
        *message_start = '\0'; // Split headers and message
        message_start += 2; // Skip the \n\n
    }
    
    char *saveptr;
    char* line = strtok_r(commit_copy, "\n", &saveptr);
    while (line) {
        if (strncmp(line, "tree ", 5) == 0) {
            char avc_tree_hash[65];
            if (sscanf(line, "tree %64s", avc_tree_hash) == 1) {
                printf("Converting tree %s\n", avc_tree_hash);
                char git_tree_hash[41];
                if (convert_avc_tree_to_git(avc_tree_hash, git_tree_hash) == 0) {
                    printf("Tree converted: %s -> %s\n", avc_tree_hash, git_tree_hash);
                    git_commit_offset += snprintf(git_commit_content + git_commit_offset,
                                                commit_size * 2 - git_commit_offset,
                                                "tree %s\n", git_tree_hash);
                } else {
                    printf("Failed to convert tree %s\n", avc_tree_hash);
                    free(commit_copy);
                    free(git_commit_content);
                    free(commit_content);
                    return -1;
                }
            }
        } else if (strncmp(line, "parent ", 7) == 0) {
            char avc_parent_hash[65];
            if (sscanf(line, "parent %64s", avc_parent_hash) == 1) {
                char git_parent_hash[41];
                // Recursively ensure parent commit is present
                if (convert_avc_commit_to_git(avc_parent_hash, git_parent_hash) != 0) {
                    printf("Failed to convert parent commit %s\n", avc_parent_hash);
                    free(commit_copy);
                    free(git_commit_content);
                    free(commit_content);
                    return -1;
                }
                git_commit_offset += snprintf(git_commit_content + git_commit_offset,
                                            commit_size * 2 - git_commit_offset,
                                            "parent %s\n", git_parent_hash);
            }
        } else if (strncmp(line, "author ", 7) == 0) {
            // Parse author line and ensure it has email and proper date format
            char author_info[512];
            if (sscanf(line, "author %511[^\n]", author_info) == 1) {
                // Check if email is present
                if (strchr(author_info, '@') == NULL) {
                    // Add default email if missing
                    git_commit_offset += snprintf(git_commit_content + git_commit_offset,
                                                commit_size * 2 - git_commit_offset,
                                                "author %s <user@example.com>\n", author_info);
                } else {
                    // Parse the author line to fix date format
                    char name[256], email[256], date_part[256];
                    // Try to parse: "name <email> YYYY-MM-DD HH:MM:SS +0000"
                    if (sscanf(author_info, "%255[^<] <%255[^>]> %255s", name, email, date_part) == 3) {
                        // Remove trailing timezone from date_part if it ends with +0000
                        char* tz_pos = strrchr(date_part, '+');
                        if (tz_pos && strcmp(tz_pos, "+0000") == 0) {
                            *tz_pos = '\0';
                        }
                        long epoch = iso_to_epoch(date_part);
                        git_commit_offset += snprintf(git_commit_content + git_commit_offset,
                                                    commit_size * 2 - git_commit_offset,
                                                    "author %s <%s> %ld +0000\n", name, email, epoch);
                    } else {
                        // Copy as-is if we can't parse it
                        git_commit_offset += snprintf(git_commit_content + git_commit_offset,
                                                    commit_size * 2 - git_commit_offset,
                                                    "author %s\n", author_info);
                    }
                }
            }
        } else if (strncmp(line, "committer ", 10) == 0) {
            // Parse committer line and ensure it has email and proper date format
            char committer_info[512];
            if (sscanf(line, "committer %511[^\n]", committer_info) == 1) {
                // Check if email is present
                if (strchr(committer_info, '@') == NULL) {
                    // Add default email if missing
                    git_commit_offset += snprintf(git_commit_content + git_commit_offset,
                                                commit_size * 2 - git_commit_offset,
                                                "committer %s <user@example.com>\n", committer_info);
                } else {
                    // Parse the committer line to fix date format
                    char name[256], email[256], date_part[256];
                    // Try to parse: "name <email> YYYY-MM-DD HH:MM:SS +0000"
                    if (sscanf(committer_info, "%255[^<] <%255[^>]> %255s", name, email, date_part) == 3) {
                        // Remove trailing timezone from date_part if it ends with +0000
                        char* tz_pos = strrchr(date_part, '+');
                        if (tz_pos && strcmp(tz_pos, "+0000") == 0) {
                            *tz_pos = '\0';
                        }
                        long epoch = iso_to_epoch(date_part);
                        git_commit_offset += snprintf(git_commit_content + git_commit_offset,
                                                    commit_size * 2 - git_commit_offset,
                                                    "committer %s <%s> %ld +0000\n", name, email, epoch);
                    } else {
                        // Copy as-is if we can't parse it
                        git_commit_offset += snprintf(git_commit_content + git_commit_offset,
                                                    commit_size * 2 - git_commit_offset,
                                                    "committer %s\n", committer_info);
                    }
                }
            }
        }
        
        line = strtok_r(NULL, "\n", &saveptr);
    }
    
    // Add empty line separator
    git_commit_offset += snprintf(git_commit_content + git_commit_offset,
                                commit_size * 2 - git_commit_offset, "\n");
    
    // Add commit message
    if (message_start && strlen(message_start) > 0) {
        git_commit_offset += snprintf(git_commit_content + git_commit_offset,
                                    commit_size * 2 - git_commit_offset,
                                    "%s", message_start);
    }
    
    printf("Git commit content:\n%s\n", git_commit_content);
    
    // Compute SHA-1 of full commit content before storing (needed for recursion termination)
    // Store as Git commit
    int result = store_git_object("commit", git_commit_content, git_commit_offset, git_hash_out);
    if (result == 0) {
        append_mapping(avc_hash, git_hash_out);
    }
    
    free(commit_copy);
    free(git_commit_content);
    free(commit_content);
    
    return result;
}

// Initialize Git repository alongside AVC
int cmd_git_init(int argc, char* argv[]) {
    if (check_repo() == -1) {
        fprintf(stderr, "Not in an AVC repository\n");
        return 1;
    }
    
    // Create .git directory structure
    const char* git_dirs[] = {
        ".git",
        ".git/objects",
        ".git/refs",
        ".git/refs/heads",
        ".git/refs/tags"
    };
    
    for (int i = 0; i < 5; i++) {
        if (mkdir(git_dirs[i], 0755) == -1 && errno != EEXIST) {
            perror("mkdir");
            return 1;
        }
    }
    
    // Create Git HEAD file
    FILE* git_head = fopen(".git/HEAD", "w");
    if (git_head) {
        fprintf(git_head, "ref: refs/heads/main\n");
        fclose(git_head);
    }
    
    // Create Git config with proper settings
    FILE* git_config = fopen(".git/config", "w");
    if (git_config) {
        fprintf(git_config, "[core]\n");
        fprintf(git_config, "\trepositoryformatversion = 0\n");
        fprintf(git_config, "\tfilemode = true\n");
        fprintf(git_config, "\tbare = false\n");
        fprintf(git_config, "\tlogallrefupdates = true\n");
        fprintf(git_config, "\tprecomposeunicode = true\n");
        fprintf(git_config, "[init]\n");
        fprintf(git_config, "\tdefaultBranch = main\n");
        fclose(git_config);
    }
    
    // Create empty Git description file
    FILE* git_desc = fopen(".git/description", "w");
    if (git_desc) {
        fprintf(git_desc, "Unnamed repository; edit this file 'description' to name the repository.\n");
        fclose(git_desc);
    }
    
    printf("Git repository initialized alongside AVC\n");
    return 0;
}

// Sync AVC objects to Git format
int cmd_sync_to_git(int argc, char* argv[]) {
    if (check_repo() == -1) {
        fprintf(stderr, "Not in an AVC repository\n");
        return 1;
    }
    
    // Check if Git repo exists
    struct stat st;
    if (stat(".git", &st) == -1) {
        fprintf(stderr, "Git repository not found. Run 'avc git-init' first.\n");
        return 1;
    }
    
    tui_header("AGCL Sync to Git");
    
    spinner_t* sync_spinner = spinner_create("Syncing AVC objects to Git format ");
    spinner_update(sync_spinner);
    
    // Get current commit hash
    char current_commit[65];
    FILE* head = fopen(".avc/HEAD", "r");
    if (!head) {
        fprintf(stderr, "No HEAD found\n");
        return 1;
    }
    
    char head_content[256];
    if (fgets(head_content, sizeof(head_content), head)) {
        if (strncmp(head_content, "ref: ", 5) == 0) {
            char branch_path[512];
            char* branch_ref = head_content + 5;
            branch_ref[strcspn(branch_ref, "\n")] = '\0';
            
            snprintf(branch_path, sizeof(branch_path), ".avc/%s", branch_ref);
            FILE* branch_file = fopen(branch_path, "r");
            if (branch_file) {
                if (fgets(current_commit, 65, branch_file)) {
                    current_commit[strcspn(current_commit, "\n")] = '\0';
                }
                fclose(branch_file);
            }
        }
    }
    fclose(head);
    
    if (strlen(current_commit) == 0) {
        printf("No commits to sync\n");
        return 0;
    }
    
    // Convert current commit to Git format
    char git_commit_hash[41];
    if (convert_avc_commit_to_git(current_commit, git_commit_hash) == 0) {
        // Update Git HEAD reference
        FILE* git_head = fopen(".git/refs/heads/main", "w");
        if (git_head) {
            fprintf(git_head, "%s\n", git_commit_hash);
            fclose(git_head);
        }
        
        spinner_stop(sync_spinner);
        spinner_free(sync_spinner);
        
        // Commit hash mappings to disk
        if (g_hash_map) {
            hash_map_commit(g_hash_map);
        }
        
        tui_success("Sync completed successfully");
        printf("Synced commit %s -> %s\n", current_commit, git_commit_hash);
    } else {
        spinner_stop(sync_spinner);
        spinner_free(sync_spinner);
        tui_error("Failed to sync commit");
        return 1;
    }
    
    return 0;
}

// Verify Git repository state before push
int cmd_verify_git(int argc, char* argv[]) {
    printf("Verifying Git repository state...\n");
    
    // Check if .git directory exists
    struct stat st;
    if (stat(".git", &st) == -1) {
        fprintf(stderr, "No .git directory found\n");
        return 1;
    }
    
    // Check HEAD reference
    FILE* head = fopen(".git/HEAD", "r");
    if (!head) {
        fprintf(stderr, "No HEAD file found\n");
        return 1;
    }
    
    char head_content[256];
    if (fgets(head_content, sizeof(head_content), head)) {
        printf("HEAD: %s", head_content);
    }
    fclose(head);
    
    // Check main branch reference
    FILE* main_ref = fopen(".git/refs/heads/main", "r");
    if (main_ref) {
        char commit_hash[42];
        if (fgets(commit_hash, sizeof(commit_hash), main_ref)) {
            printf("main branch: %s", commit_hash);
            
            // Verify the commit object exists
            commit_hash[strcspn(commit_hash, "\n")] = '\0';
            char obj_path[512];
            snprintf(obj_path, sizeof(obj_path), ".git/objects/%c%c/%s", 
                     commit_hash[0], commit_hash[1], commit_hash + 2);
            
            if (access(obj_path, F_OK) == 0) {
                printf("✓ Commit object exists: %s\n", obj_path);
            } else {
                printf("✗ Commit object missing: %s\n", obj_path);
                fclose(main_ref);
                return 1;
            }
        }
        fclose(main_ref);
    } else {
        fprintf(stderr, "No main branch reference found\n");
        return 1;
    }
    
    printf("Git repository state verified successfully\n");
    return 0;
}

// Main AGCL command dispatcher
int cmd_agcl(int argc, char* argv[]) {
    if (argc < 2) {
        printf("AVC Git Compatibility Layer (AGCL)\n");
        printf("Usage: avc agcl <command> [args]\n");
        printf("Commands:\n");
        printf("  git-init     Initialize Git repository alongside AVC\n");
        printf("  sync-to-git  Sync AVC objects to Git format\n");
        printf("  verify-git   Verify Git repository state\n");
        printf("  fix-refs     Fix Git references and index\n");
        printf("  sync-from-git Sync Git objects to AVC format\n");
        printf("  migrate      Convert existing Git repo to AVC\n");
        return 1;
    }
    
    char* subcommand = argv[1];
    
    if (strcmp(subcommand, "git-init") == 0) {
        return cmd_git_init(argc - 1, argv + 1);
    } else if (strcmp(subcommand, "sync-to-git") == 0) {
        return cmd_sync_to_git(argc - 1, argv + 1);
    } else if (strcmp(subcommand, "verify-git") == 0) {
        return cmd_verify_git(argc - 1, argv + 1);
    } else {
        printf("Unknown AGCL command: %s\n", subcommand);
        return 1;
    }
} 