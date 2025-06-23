// src/commands/clean.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include "commands.h"
#include "repository.h"

// Recursively remove directory and all contents
static int remove_directory_recursive(const char* path) {
    DIR* dir = opendir(path);
    if (!dir) {
        return -1;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        
        struct stat st;
        if (stat(full_path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                if (remove_directory_recursive(full_path) == -1) {
                    closedir(dir);
                    return -1;
                }
            } else {
                if (unlink(full_path) == -1) {
                    closedir(dir);
                    return -1;
                }
            }
        }
    }
    
    closedir(dir);
    
    if (rmdir(path) == -1) {
        return -1;
    }
    
    return 0;
}

int cmd_clean(int argc, char* argv[]) {
    // Check if we're in a repository
    if (check_repo() == -1) {
        fprintf(stderr, "Not in an avc repository\n");
        return 1;
    }
    
    printf("This will permanently remove the .avc directory and all repository data.\n");
    printf("This action cannot be undone.\n");
    printf("Are you sure you want to continue? (yes/no): ");
    
    char answer[16];
    if (!fgets(answer, sizeof(answer), stdin)) {
        fprintf(stderr, "Failed to read input. Aborting.\n");
        return 1;
    }
    
    // Remove newline
    answer[strcspn(answer, "\n")] = '\0';
    
    if (strcmp(answer, "yes") != 0 && strcmp(answer, "y") != 0) {
        printf("Clean aborted.\n");
        return 0;
    }
    
    printf("Removing .avc directory...\n");
    
    if (remove_directory_recursive(".avc") == -1) {
        fprintf(stderr, "Failed to remove .avc directory: %s\n", strerror(errno));
        return 1;
    }
    
    printf("Repository cleaned successfully.\n");
    return 0;
} 