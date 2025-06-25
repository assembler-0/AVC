#ifndef AVC_H
#define AVC_H

#include <stddef.h>
#include <stdint.h>
#include <time.h>

// =============================================================================
// AVC - Archive Version Control API
// High-performance version control system with BLAKE3 hashing
// =============================================================================

// Constants
#define AVC_HASH_SIZE 64
#define AVC_MAX_PATH 256
#define AVC_MAX_MESSAGE 1024
#define AVC_FAST_INDEX_SIZE 8192

// =============================================================================
// CORE COMMANDS API
// =============================================================================

// Command function declarations
int cmd_init(int argc, char* argv[]);
int cmd_add(int argc, char* argv[]);
int cmd_commit(int argc, char* argv[]);
int cmd_status(int argc, char* argv[]);
int cmd_log(int argc, char* argv[]);
int cmd_rm(int argc, char* argv[]);
int cmd_reset(int argc, char* argv[]);
int cmd_version(int argc, char* argv[]);
int cmd_clean(int argc, char* argv[]);
int cmd_agcl(int argc, char* argv[]);

// =============================================================================
// REPOSITORY API
// =============================================================================

// Check if current directory is an AVC repository
int check_repo(void);

// =============================================================================
// OBJECT STORAGE API
// =============================================================================

// Store a blob object from a file
int store_blob_from_file(const char* filepath, char* hash_out);

// Store an object with given type and content
int store_object(const char* type, const char* content, size_t size, char* hash_out);

// Load an object by hash
char* load_object(const char* hash, size_t* size_out, char* type_out);

// Enable/disable fast compression mode (level 0)
void objects_set_fast_mode(int fast);

// Calculate hash of a file using BLAKE3
int blake3_file_hex(const char* filepath, char hash_out[65]);

// Diagnostic function to check object format
int check_object_format(const char* hash);

// =============================================================================
// HASHING API
// =============================================================================

// BLAKE3 hash functions
void blake3_hash(const char* content, size_t size, char* hash_out);
void blake3_hash_object(const char* type, const char* content, size_t size, char* hash_out);

// =============================================================================
// INDEX API
// =============================================================================

// Index management functions
int add_file_to_index(const char* filepath);
int remove_file_from_index(const char* filepath);
int is_file_in_index(const char* filepath);
int clear_index(void);

// Transactional index updates (load once, write once)
int index_load(void);
int index_upsert_entry(const char* filepath, const char* hash, unsigned int mode, int* unchanged_out);
int index_commit(void);

// Returns pointer to hash string for given path if present, else NULL
const char* index_get_hash(const char* filepath);

// =============================================================================
// FAST INDEX API (O(1) Operations)
// =============================================================================

typedef struct index_entry {
    char path[AVC_MAX_PATH];
    char hash[AVC_HASH_SIZE + 1];
    uint32_t mode;
    struct index_entry* next;
} index_entry_t;

typedef struct {
    index_entry_t* buckets[AVC_FAST_INDEX_SIZE];
    size_t count;
    int loaded;
} fast_index_t;

// Fast index operations
fast_index_t* fast_index_create(void);
int fast_index_load(fast_index_t* idx);
const index_entry_t* fast_index_get(fast_index_t* idx, const char* path);
int fast_index_set(fast_index_t* idx, const char* path, const char* hash, uint32_t mode);
int fast_index_remove(fast_index_t* idx, const char* path);
int fast_index_commit(fast_index_t* idx);
void fast_index_free(fast_index_t* idx);
const char* fast_index_get_hash(fast_index_t* idx, const char* path);

// =============================================================================
// FILE UTILITIES API
// =============================================================================

// File I/O operations
char* read_file(const char* filepath, size_t* size);
int write_file(const char* filepath, const char* content, size_t size);
char* strdup2(const char* s);
int remove_directory_recursive(const char* path);

// =============================================================================
// ARGUMENT PARSER API
// =============================================================================

typedef struct {
    char** positional_args;
    size_t positional_count;
    int flags;
    char* message;
    char* commit_hash;
} parsed_args_t;

// Flag definitions
#define FLAG_CACHED          (1 << 0)
#define FLAG_RECURSIVE       (1 << 1)
#define FLAG_HARD            (1 << 2)
#define FLAG_CLEAN           (1 << 3)
#define FLAG_FAST            (1 << 4)

// Argument parsing functions
parsed_args_t* parse_args(int argc, char* argv[], const char* valid_flags);
void free_parsed_args(parsed_args_t* args);
int has_flag(parsed_args_t* args, int flag);
char* get_message(parsed_args_t* args);
char* get_commit_hash(parsed_args_t* args);
char** get_positional_args(parsed_args_t* args);
size_t get_positional_count(parsed_args_t* args);

// =============================================================================
// TUI (Terminal User Interface) API
// =============================================================================

// Progress bar
typedef struct {
    size_t current;
    size_t total;
    int width;
    char* label;
    int show_percentage;
    int show_count;
} progress_bar_t;

// Spinner
typedef struct {
    int frame;
    char* label;
    int active;
} spinner_t;

// Progress bar functions
progress_bar_t* progress_create(const char* label, size_t total, int width);
void progress_update(progress_bar_t* bar, size_t current);
void progress_finish(progress_bar_t* bar);
void progress_free(progress_bar_t* bar);

// Spinner functions
spinner_t* spinner_create(const char* label);
void spinner_update(spinner_t* spinner);
void spinner_stop(spinner_t* spinner);
void spinner_free(spinner_t* spinner);

// Terminal control
void tui_clear_line(void);
void tui_move_cursor_up(int lines);
void tui_hide_cursor(void);
void tui_show_cursor(void);

// Colored output
void tui_success(const char* message);
void tui_error(const char* message);
void tui_warning(const char* message);
void tui_info(const char* message);
void tui_header(const char* message);

// =============================================================================
// AGCL (AVC Git Compatibility Layer) API
// =============================================================================

// Hash mapping for AVC <-> Git conversion
typedef struct hash_map_entry {
    char avc_hash[65];
    char git_hash[41];
    struct hash_map_entry* next;
} hash_map_entry_t;

typedef struct {
    hash_map_entry_t* buckets[4096];
    size_t count;
} hash_map_t;

// AGCL functions
hash_map_t* hash_map_create(void);
int hash_map_load(hash_map_t* map);
const char* hash_map_get(hash_map_t* map, const char* avc_hash);
int hash_map_set(hash_map_t* map, const char* avc_hash, const char* git_hash);
int hash_map_commit(hash_map_t* map);
void hash_map_free(hash_map_t* map);

// =============================================================================
// MEMORY MANAGEMENT API
// =============================================================================

// Memory pool functions (if implemented)
void free_memory_pool(void);
void reset_memory_pool(void);

// =============================================================================
// VERSION INFO
// =============================================================================

#define AVC_VERSION "0.3.1"
#define AVC_CODENAME "Delta Spectre"

#endif // AVC_H