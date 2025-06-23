// src/commands/init.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include "commands.h"

// Helper function to create directory
int create_dir(const char* path) {
    if (mkdir(path, 0755) == -1) {
        if (errno != EEXIST) {  // Don't error if directory already exists
            perror("mkdir");
            return -1;
        }
    }
    return 0;
}

// Helper function to create empty file
int create_file(const char* path, const char* content) {
    FILE* file = fopen(path, "w");
    if (!file) {
        perror("fopen");
        return -1;
    }

    if (content) {
        fprintf(file, "%s", content);
    }

    fclose(file);
    return 0;
}

int cmd_init(int argc, char* argv[]) {
    const char* repo_path = ".";

    // If path is provided as argument
    if (argc > 1) {
        repo_path = argv[1];
    }

    printf("Initializing avc repository in %s\n", repo_path);

    // Create .avc directory
    char avc_dir[512];
    snprintf(avc_dir, sizeof(avc_dir), "%s/.avc", repo_path);

    if (create_dir(avc_dir) == -1) {
        fprintf(stderr, "Failed to create .avc directory\n");
        return 1;
    }

    // Create subdirectories
    char objects_dir[512], refs_dir[512], heads_dir[512];
    snprintf(objects_dir, sizeof(objects_dir), "%s/objects", avc_dir);
    snprintf(refs_dir, sizeof(refs_dir), "%s/refs", avc_dir);
    snprintf(heads_dir, sizeof(heads_dir), "%s/refs/heads", avc_dir);

    if (create_dir(objects_dir) == -1 ||
        create_dir(refs_dir) == -1 ||
        create_dir(heads_dir) == -1) {
        fprintf(stderr, "Failed to create repository structure\n");
        return 1;
    }

    // Create HEAD file (points to main branch)
    char head_file[512];
    snprintf(head_file, sizeof(head_file), "%s/HEAD", avc_dir);
    if (create_file(head_file, "ref: refs/heads/main\n") == -1) {
        fprintf(stderr, "Failed to create HEAD file\n");
        return 1;
    }

    // Create empty index file
    char index_file[512];
    snprintf(index_file, sizeof(index_file), "%s/index", avc_dir);
    if (create_file(index_file, "") == -1) {
        fprintf(stderr, "Failed to create index file\n");
        return 1;
    }

    // Create basic config file
    char config_file[512];
    snprintf(config_file, sizeof(config_file), "%s/config", avc_dir);
    const char* config_content =
        "[core]\n"
        "    repositoryformatversion = 0\n"
        "    filemode = true\n";

    if (create_file(config_file, config_content) == -1) {
        fprintf(stderr, "Failed to create config file\n");
        return 1;
    }

    printf("Initialized empty avc repository in %s/.avc/\n", repo_path);
    
    return 0;
}