#ifndef AVC_ARG_PARSER_H
#define AVC_ARG_PARSER_H

#include <stddef.h>

// Structure to hold parsed arguments
typedef struct {
    char** positional_args;  // Non-flag arguments
    size_t positional_count;
    int flags;               // Bit flags for boolean options
    char* message;           // For -m flag
    char* commit_hash;       // For reset command
} parsed_args_t;

// Flag definitions (bit positions)
#define FLAG_CACHED          (1 << 0)
#define FLAG_RECURSIVE       (1 << 1)
#define FLAG_HARD            (1 << 2)
#define FLAG_CLEAN           (1 << 3)
#define FLAG_NO_COMPRESSION  (1 << 4)

// Function to parse command line arguments
parsed_args_t* parse_args(int argc, char* argv[], const char* valid_flags);

// Function to free parsed arguments
void free_parsed_args(parsed_args_t* args);

// Helper functions to check flags
int has_flag(parsed_args_t* args, int flag);
char* get_message(parsed_args_t* args);
char* get_commit_hash(parsed_args_t* args);

// Helper function to get positional arguments
char** get_positional_args(parsed_args_t* args);
size_t get_positional_count(parsed_args_t* args);

#endif // AVC_ARG_PARSER_H 