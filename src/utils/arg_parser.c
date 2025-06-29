#include "arg_parser.h"
#include "file_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

parsed_args_t* parse_args(int argc, char* argv[], const char* valid_flags) {
    parsed_args_t* args = calloc(1, sizeof(parsed_args_t));
    if (!args) {
        return NULL;
    }

    // Allocate space for positional arguments
    args->positional_args = malloc(argc * sizeof(char*));
    if (!args->positional_args) {
        free(args);
        return NULL;
    }

    // Parse arguments
    for (int i = 1; i < argc; i++) {
        char* arg = argv[i];
        
        // Check if it's a flag
        if (arg[0] == '-') {
            if (strcmp(arg, "--cached") == 0) {
                if (strchr(valid_flags, 'c')) {
                    args->flags |= FLAG_CACHED;
                } else {
                    fprintf(stderr, "Error: --cached flag not valid for this command\n");
                    free_parsed_args(args);
                    return NULL;
                }
            } else if (strcmp(arg, "-r") == 0) {
                if (strchr(valid_flags, 'r')) {
                    args->flags |= FLAG_RECURSIVE;
                } else {
                    fprintf(stderr, "Error: -r flag not valid for this command\n");
                    free_parsed_args(args);
                    return NULL;
                }
            } else if (strcmp(arg, "--hard") == 0) {
                if (strchr(valid_flags, 'h')) {
                    args->flags |= FLAG_HARD;
                } else {
                    fprintf(stderr, "Error: --hard flag not valid for this command\n");
                    free_parsed_args(args);
                    return NULL;
                }
            } else if (strcmp(arg, "--clean") == 0) {
                if (strchr(valid_flags, 'l')) {
                    args->flags |= FLAG_CLEAN;
                } else {
                    fprintf(stderr, "Error: --clean flag not valid for this command\n");
                    free_parsed_args(args);
                    return NULL;
                }
            } else if (strcmp(arg, "--fast") == 0 || strcmp(arg, "-f") == 0) {
                if (strchr(valid_flags, 'f')) {
                    args->flags |= FLAG_FAST;
                } else {
                    fprintf(stderr, "Error: --fast flag not valid for this command\n");
                    free_parsed_args(args);
                    return NULL;
                }
            } else if (strcmp(arg, "--empty-dirs") == 0 || strcmp(arg, "-e") == 0) {
                if (strchr(valid_flags, 'e')) {
                    args->flags |= FLAG_EMPTY_DIRS;
                } else {
                    fprintf(stderr, "Error: --empty-dirs flag not valid for this command\n");
                    free_parsed_args(args);
                    return NULL;
                }
            } else if (strcmp(arg, "-m") == 0) {
                if (strchr(valid_flags, 'm')) {
                    if (i + 1 < argc) {
                        args->message = strdup2(argv[i + 1]);
                        i++; // Skip the next argument
                    } else {
                        fprintf(stderr, "Error: -m flag requires a message\n");
                        free_parsed_args(args);
                        return NULL;
                    }
                } else {
                    fprintf(stderr, "Error: -m flag not valid for this command\n");
                    free_parsed_args(args);
                    return NULL;
                }
            } else {
                fprintf(stderr, "Error: Unknown flag '%s'\n", arg);
                free_parsed_args(args);
                return NULL;
            }
        } else {
            // Positional argument
            args->positional_args[args->positional_count] = strdup2(arg);
            args->positional_count++;
        }
    }

    return args;
}

void free_parsed_args(parsed_args_t* args) {
    if (!args) return;
    
    if (args->positional_args) {
        for (size_t i = 0; i < args->positional_count; i++) {
            free(args->positional_args[i]);
        }
        free(args->positional_args);
    }
    
    free(args->message);
    free(args->commit_hash);
    free(args);
}

int has_flag(parsed_args_t* args, int flag) {
    return args ? (args->flags & flag) != 0 : 0;
}

char* get_message(parsed_args_t* args) {
    return args ? args->message : NULL;
}

char* get_commit_hash(parsed_args_t* args) {
    return args ? args->commit_hash : NULL;
}

char** get_positional_args(parsed_args_t* args) {
    return args ? args->positional_args : NULL;
}

size_t get_positional_count(parsed_args_t* args) {
    return args ? args->positional_count : 0;
} 