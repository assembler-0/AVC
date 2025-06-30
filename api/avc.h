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
int cmd_repo_migrate(int argc, char* argv[]);

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





// =============================================================================
// INDEX API
// =============================================================================



// Transactional index updates (load once, write once)
int index_load(void);
int index_upsert_entry(const char* filepath, const char* hash, unsigned int mode, int* unchanged_out);
int index_commit(void);

// Returns pointer to hash string for given path if present, else NULL
const char* index_get_hash(const char* filepath);



// =============================================================================
// FILE UTILITIES API
// =============================================================================

// File I/O operations


// =============================================================================
// ARGUMENT PARSER API
// =============================================================================



// =============================================================================
// TUI (Terminal User Interface) API
// =============================================================================

// Progress bar


// =============================================================================
// AGCL (AVC Git Compatibility Layer) API
// =============================================================================

// Hash mapping for AVC <-> Git conversion


// =============================================================================
// MEMORY MANAGEMENT API
// =============================================================================



// =============================================================================
// VERSION INFO
// =============================================================================

#define AVC_VERSION "0.3.1"
#define AVC_CODENAME "Delta Spectre"

#endif // AVC_H