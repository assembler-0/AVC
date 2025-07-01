// AVC Git Compatibility Layer (AGCL)
// Handles conversion between AVC and Git formats
#define _XOPEN_SOURCE 700   /* for strptime */
#define _GNU_SOURCE

// Suppress OpenSSL SHA1 deprecation warnings for Git compatibility
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <openssl/sha.h>
#include <zlib.h>
#include <zstd.h>
#include "commands.h"
#include "repository.h"
#include "objects.h"
#include "fast_agcl.h"
#include "tui.h"
#include <blake3/blake3.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>


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



// Path to mapping file that stores lines: "<avc_hash> <git_hash>\n"
#define AGCL_MAP_PATH ".git/avc-map"

// Forward declarations
static int read_mapping(const char* avc_hash, char* git_hash_out);
static int append_mapping(const char* avc_hash, const char* git_hash);
static char* load_git_object(const char* git_hash, size_t* size_out, char* type_out);
static int convert_git_blob_to_avc(const char* git_hash, char* avc_hash_out);
static int convert_git_tree_to_avc(const char* git_hash, char* avc_hash_out);
static void fix_git_permissions();
int cmd_agcl_push(int argc, char* argv[]);
int cmd_agcl_pull(int argc, char* argv[]);

// Helper: check if a Git object already exists
static int git_object_exists(const char* git_hash) {
    char path[512];
    snprintf(path, sizeof(path), ".git/objects/%c%c/%s", git_hash[0], git_hash[1], git_hash + 2);
    return access(path, F_OK) == 0;
}

// Global hash mapping cache
static hash_map_t* g_hash_map = NULL;

// Initialize hash mapping cache
static void init_hash_map() {
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

// Fix Git directory permissions to prevent access issues
static void fix_git_permissions() {
    system("chmod -R 755 .git/ 2>/dev/null");
}

// Convert ISO 8601 datetime to epoch seconds (UTC)
static long iso_to_epoch(const char *iso_str) {
    struct tm tm = {0};

    // Try multiple date formats
    if (!strptime(iso_str, "%Y-%m-%d %H:%M:%S", &tm) &&
        !strptime(iso_str, "%Y-%m-%d %H:%M", &tm) &&
        !strptime(iso_str, "%Y-%m-%d", &tm)) {
        // If all parsing fails, use current time
        return time(NULL);
    }
    // Convert to epoch (try timegm for UTC, fall back to mktime which assumes local time)
    #ifdef _GNU_SOURCE
    return timegm(&tm);
    #else
    // For systems without timegm, mktime assumes local time, which is fine now
    return mktime(&tm);
    #endif
}

// FIXED AGCL: Use REAL zlib compression for Git compatibility
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

// Decompress Git objects with real zlib
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
        printf("Failed to create Git object file: %s (error: %s)\n", git_obj_path, strerror(errno));
        free(compressed);
        return -1;
    }

    size_t written = fwrite(compressed, 1, compressed_size, obj_file);
    if (written != compressed_size) {
        printf("Failed to write Git object data: wrote %zu/%zu bytes\n", written, compressed_size);
        fclose(obj_file);
        free(compressed);
        return -1;
    }

    if (fclose(obj_file) != 0) {
        printf("Failed to close Git object file: %s\n", strerror(errno));
        free(compressed);
        return -1;
    }

    free(compressed);
    return 0;
}

typedef struct {
    unsigned int mode;
    char filename[256];
    char avc_hash[65];
    char git_hash[41];
} tree_entry_t;
int compare_entries(const void *a, const void *b) {
    const tree_entry_t *entry_a = (const tree_entry_t *)a;
    const tree_entry_t *entry_b = (const tree_entry_t *)b;
    return strcmp(entry_a->filename, entry_b->filename);
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

    if (!blob_content) {
        printf("Warning: AVC blob %s not found\n", avc_hash);
        return -1;
    }

    if (strcmp(blob_type, "blob") != 0) {
        printf("Warning: Object %s is not a blob (type: %s)\n", avc_hash, blob_type);
        free(blob_content);
        return -1;
    }

    // Store as Git blob
    int result = store_git_object("blob", blob_content, blob_size, git_hash_out);
    free(blob_content);

    if (result == 0) {
        append_mapping(avc_hash, git_hash_out);
        // Blob converted successfully
    } else {
        printf("Failed to store Git blob for %s\n", avc_hash);
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
    // First collect all entries, deduplicate, and sort



    tree_entry_t* entries = NULL;
    int entry_count = 0;
    int entries_capacity = 0;

    // First pass: collect and convert all entries
    char* line_start = work_copy;
    char* line_end;
    while ((line_end = strchr(line_start, '\n')) != NULL) {
        *line_end = '\0';

        if (strlen(line_start) > 0) {
            unsigned int mode;
            char filepath[256], avc_hash_entry[65];

            if (sscanf(line_start, "%o %255s %64s", &mode, filepath, avc_hash_entry) == 3) {
                // Use the full relative path as stored in AVC tree
                char* filename = filepath;
                // Remove ./ prefix if present for Git compatibility
                if (strncmp(filename, "./", 2) == 0) {
                    filename += 2;
                }

                // Check for duplicates (should not happen with hierarchical trees)
                int is_duplicate = 0;
                for (int i = 0; i < entry_count; i++) {
                    if (strcmp(entries[i].filename, filename) == 0) {
                        is_duplicate = 1;
                        break;
                    }
                }

                if (!is_duplicate) {
                    if (entry_count >= entries_capacity) {
                        entries_capacity = entries_capacity == 0 ? 10 : entries_capacity * 2;
                        entries = realloc(entries, entries_capacity * sizeof(tree_entry_t));
                        if (!entries) {
                            // Handle realloc failure
                            free(work_copy);
                            free(tree_copy);
                            free(tree_content);
                            return -1;
                        }
                    }
                    entries[entry_count].mode = mode;
                    strcpy(entries[entry_count].filename, filename);
                    strcpy(entries[entry_count].avc_hash, avc_hash_entry);

                    // Convert hash
                    if (mode == 040000) {
                        if (convert_avc_tree_to_git(avc_hash_entry, entries[entry_count].git_hash) != 0) {
                            printf("Warning: Failed to convert sub-tree %s, skipping\n", avc_hash_entry);
                            continue;
                        }
                    } else {
                        if (convert_avc_blob_to_git(avc_hash_entry, entries[entry_count].git_hash) != 0) {
                            printf("Warning: Failed to convert blob %s, skipping\n", avc_hash_entry);
                            continue;
                        }
                    }

                    entry_count++;
                }
            }
        }

        line_start = line_end + 1;
    }

    // Replace your bubble sort loop with this single line:
    qsort(entries, entry_count, sizeof(tree_entry_t), compare_entries);

    // Calculate total size
    size_t total_size = 0;
    for (int i = 0; i < entry_count; i++) {
        int mode_len = snprintf(NULL, 0, "%o", entries[i].mode);
        total_size += mode_len + 1 + strlen(entries[i].filename) + 1 + 20;
    }

    // Restore work_copy because newline characters were replaced with NULs during the first pass
    memcpy(work_copy, tree_content, tree_size);
    work_copy[tree_size] = '\0';

    // Allocate buffer for Git tree content
    char* git_tree_content = malloc(total_size);
    if (!git_tree_content) {
        free(work_copy);
        free(tree_copy);
        free(tree_content);
        return -1;
    }

    size_t git_tree_offset = 0;

    // Build Git tree content from sorted, deduplicated entries
    for (int i = 0; i < entry_count; i++) {
        // Convert hex hash to binary
        unsigned char hash_binary[20];
        for (int j = 0; j < 20; j++) {
            char hex_pair[3] = {entries[i].git_hash[j*2], entries[i].git_hash[j*2+1], '\0'};
            hash_binary[j] = (unsigned char)strtol(hex_pair, NULL, 16);
        }

        // Format: "mode filename\0hash" - Git tree format
        int len = snprintf(git_tree_content + git_tree_offset,
                         total_size - git_tree_offset, "%o %s", entries[i].mode, entries[i].filename);
        git_tree_offset += len;
        git_tree_content[git_tree_offset++] = '\0';
        memcpy(git_tree_content + git_tree_offset, hash_binary, 20);
        git_tree_offset += 20;
    }

    if (entries) free(entries);

    // Store as Git tree (even if some entries were skipped)
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
                char git_tree_hash[41];
                if (convert_avc_tree_to_git(avc_tree_hash, git_tree_hash) == 0) {
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

// EXPERIMENTAL: Migrate Git repository to AVC format
int cmd_migrate(int argc, char* argv[]) {
    tui_header("AGCL Git Migration");

    char* git_url = NULL;
    char repo_name[256] = {0};
    int clone_mode = 0;

    // Check if URL is provided
    if (argc > 1) {
        git_url = argv[1];
        clone_mode = 1;

        // Extract repo name from URL
        char* last_slash = strrchr(git_url, '/');
        if (last_slash) {
            strcpy(repo_name, last_slash + 1);
            // Remove .git extension if present
            char* dot_git = strstr(repo_name, ".git");
            if (dot_git) *dot_git = '\0';
        } else {
            strcpy(repo_name, "migrated-repo");
        }
    } else {
        // Check if we're in a Git repository
        struct stat st;
        if (stat(".git", &st) == -1) {
            tui_error("Usage: avc agcl migrate [git-url]");
            printf("\n");
            printf("Examples:\n");
            printf("  avc agcl migrate https://github.com/user/repo.git\n");
            printf("  avc agcl migrate  # (in existing Git repo)\n");
            return 1;
        }

        // Check if AVC already exists
        if (stat(".avc", &st) == 0) {
            tui_error("AVC repository already exists! Migration would overwrite it.");
            return 1;
        }
    }

    if (clone_mode) {
        printf("Migrating: %s\n", git_url);
    } else {
        printf("Migrating current Git repository to AVC\n");
    }

    // Clone if URL provided
    if (clone_mode) {
        char clone_cmd[1024];
        snprintf(clone_cmd, sizeof(clone_cmd), "git clone %s %s", git_url, repo_name);
        if (system(clone_cmd) != 0) {
            tui_error("Clone failed");
            return 1;
        }
        if (chdir(repo_name) != 0) {
            tui_error("Failed to enter directory");
            return 1;
        }
    }

    // Remove Git, init AVC, add files, commit
    system("rm -rf .git");

    char* init_args[] = {"avc"};
    if (cmd_init(1, init_args) != 0) return 1;

    char* add_args[] = {"avc", "."};
    if (cmd_add(2, add_args) != 0) return 1;

    char* commit_args[] = {"avc", "-m", "Initial AVC migration"};
    if (cmd_commit(3, commit_args) != 0) return 1;

    // Setup Git compatibility and sync
    cmd_git_init(0, NULL);
    fix_git_permissions();
    cmd_sync_to_git(0, NULL);

    // Add remote and push if cloned
    if (clone_mode) {
        char remote_cmd[1024];
        snprintf(remote_cmd, sizeof(remote_cmd), "git remote add origin %s", git_url);
        system(remote_cmd);

        tui_info("Pushing AVC-powered repository to origin...");
        if (system("git push -f origin main") == 0) {
            tui_success("Successfully pushed to origin!");
        } else {
            tui_warning("Push failed - you may need to run 'git push -f origin main' manually");
        }
    }

    tui_success("Migration complete");

    return 0;
}

// Verify Git repository state before push
int cmd_verify_git(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
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
                printf("\u2713 Commit object exists: %s\n", obj_path);
            } else {
                printf("\u2717 Commit object missing: %s\n", obj_path);
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




// AGCL Push: sync-to-git + git push
int cmd_agcl_push(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    tui_info("AGCL Push: Syncing to Git and pushing...");

    // Fix Git permissions first
    fix_git_permissions();

    // Step 1: Sync AVC to Git
    if (cmd_sync_to_git(0, NULL) != 0) {
        tui_error("Failed to sync to Git");
        return 1;
    }

    // Step 2: Push to origin
    tui_info("Pushing to origin...");
    if (system("git push origin main --force") != 0) {
        tui_error("Git push failed");
        return 1;
    }

    tui_success("Successfully pushed to origin!");
    return 0;
}

// AGCL Pull: git pull + add + commit (simple workflow)
int cmd_agcl_pull(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    tui_info("AGCL Pull: Simple workflow...");

    // Fix Git permissions first
    fix_git_permissions();

    // Step 1: Pull from Git
    tui_info("Pulling from origin...");
    if (system("git pull origin main") != 0) {
        tui_warning("Git pull failed or no changes");
    }

    tui_info("Adding files to AVC...");
    char* add_args[] = {"avc", "."};
    if (cmd_add(2, add_args) != 0) {
        tui_warning("No new files to add");
    }

    // Step 3: Commit with AVC (creates proper BLAKE3 hashes)
    tui_info("Creating AVC commit...");
    char* commit_args[] = {"avc", "-m", "AutoSync"};
    if (cmd_commit(3, commit_args) != 0) {
        tui_info("No changes to commit");
    }

    tui_info("Pushing to origin...");
    if (system("git push origin main --force") != 0) {
        tui_error("Git push failed");
        return 1;
    }

    tui_success("Pull completed");
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

        printf("  push         Sync to Git and push to origin (shortcut)\n");
        printf("  pull         Pull from origin and sync to AVC (shortcut)\n");
        printf("  verify-git   Verify Git repository state\n");
        printf("  migrate      Convert existing Git repo to AVC\n");
        printf("\n");
        printf("Note: Additional commands (fix-refs) are planned for future releases.\n");
        return 1;
    }

    char* subcommand = argv[1];

    if (strcmp(subcommand, "git-init") == 0) {
        return cmd_git_init(argc - 1, argv + 1);
    } else if (strcmp(subcommand, "sync-to-git") == 0) {
        return cmd_sync_to_git(argc - 1, argv + 1);
    } else if (strcmp(subcommand, "verify-git") == 0) {
        return cmd_verify_git(argc - 1, argv + 1);
    } else if (strcmp(subcommand, "migrate") == 0) {
        return cmd_migrate(argc - 1, argv + 1);

    } else if (strcmp(subcommand, "push") == 0) {
        return cmd_agcl_push(argc - 1, argv + 1);
    } else if (strcmp(subcommand, "pull") == 0) {
        return cmd_agcl_pull(argc - 1, argv + 1);
    } else {
        printf("Unknown AGCL command: %s\n", subcommand);
        return 1;
    }
}



// Convert Git blob to AVC blob
static int convert_git_blob_to_avc(const char* git_hash, char* avc_hash_out) {
    size_t blob_size;
    char blob_type[16];
    char* blob_content = load_git_object(git_hash, &blob_size, blob_type);

    if (!blob_content || strcmp(blob_type, "blob") != 0) {
        free(blob_content);
        return -1;
    }

    int result = store_object("blob", blob_content, blob_size, avc_hash_out);
    free(blob_content);

    if (result == 0) {
        append_mapping(avc_hash_out, git_hash);
    }

    return result;
}

// Convert Git tree to AVC tree
static int convert_git_tree_to_avc(const char* git_hash, char* avc_hash_out) {
    size_t tree_size;
    char tree_type[16];
    char* tree_content = load_git_object(git_hash, &tree_size, tree_type);

    if (!tree_content || strcmp(tree_type, "tree") != 0) {
        free(tree_content);
        return -1;
    }

    // Parse Git tree format and convert to AVC format
    char* avc_tree_content = malloc(tree_size * 2); // Estimate size
    size_t avc_offset = 0;
    size_t git_offset = 0;

    while (git_offset < tree_size) {
        // Parse Git tree entry: "mode filename\0hash(20 bytes)"
        char* mode_start = tree_content + git_offset;
        char* space = strchr(mode_start, ' ');
        if (!space) break;

        char* filename = space + 1;
        char* null_pos = strchr(filename, '\0');
        if (!null_pos) break;

        unsigned char* git_hash_binary = (unsigned char*)(null_pos + 1);

        // Convert binary hash to hex
        char git_entry_hash[41];
        for (int i = 0; i < 20; i++) {
            sprintf(git_entry_hash + i * 2, "%02x", git_hash_binary[i]);
        }
        git_entry_hash[40] = '\0';

        // Parse mode
        unsigned int mode;
        sscanf(mode_start, "%o", &mode);

        // Convert referenced object to AVC
        char avc_entry_hash[65];
        if (mode == 040000) {
            // Directory - convert tree
            convert_git_tree_to_avc(git_entry_hash, avc_entry_hash);
        } else {
            // File - convert blob
            convert_git_blob_to_avc(git_entry_hash, avc_entry_hash);
        }

        // Write AVC tree entry: "mode filepath hash\n" (preserve full path)
        avc_offset += snprintf(avc_tree_content + avc_offset,
                              tree_size * 2 - avc_offset,
                              "%o %s %s\n", mode, filename, avc_entry_hash);

        // Move to next entry
        git_offset = (null_pos + 1 + 20) - tree_content;
    }

    int result = store_object("tree", avc_tree_content, avc_offset, avc_hash_out);
    free(tree_content);
    free(avc_tree_content);

    if (result == 0) {
        append_mapping(avc_hash_out, git_hash);
    }

    return result;
}

// Load Git object (SHA-1 + zlib decompression)
static char* load_git_object(const char* git_hash, size_t* size_out, char* type_out) {
    char obj_path[512];
    snprintf(obj_path, sizeof(obj_path), ".git/objects/%c%c/%s",
             git_hash[0], git_hash[1], git_hash + 2);

    FILE* obj_file = fopen(obj_path, "rb");
    if (!obj_file) return NULL;

    fseek(obj_file, 0, SEEK_END);
    size_t compressed_size = ftell(obj_file);
    fseek(obj_file, 0, SEEK_SET);

    char* compressed = malloc(compressed_size);
    if (!compressed) {
        fclose(obj_file);
        return NULL;
    }
    fread(compressed, 1, compressed_size, obj_file);
    fclose(obj_file);

    size_t estimated_size = compressed_size * 4;
    char* decompressed = git_decompress(compressed, compressed_size, estimated_size);
    free(compressed);

    if (!decompressed) return NULL;

    char* null_pos = strchr(decompressed, '\0');
    if (!null_pos) {
        free(decompressed);
        return NULL;
    }

    sscanf(decompressed, "%15s %zu", type_out, size_out);

    char* content = malloc(*size_out);
    if (!content) {
        free(decompressed);
        return NULL;
    }
    memcpy(content, null_pos + 1, *size_out);
    free(decompressed);

    return content;
}

// Sync Git objects to AVC format



#pragma GCC diagnostic pop
