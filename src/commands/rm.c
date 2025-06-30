#include "commands.h"
#include "file_utils.h"
#include "index.h"
#include "repository.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// Recursively collect regular file paths under a directory (skips . and .. and
// internal .avc)
static void collect_paths_to_remove(const char *path, char ***paths,
                                    size_t *count, size_t *cap) {
  struct stat st;
  if (stat(path, &st) == -1)
    return;

  if (S_ISDIR(st.st_mode)) {
    // Skip internal .avc directory entirely
    if (strcmp(path, ".avc") == 0 || strstr(path, "/.avc") != NULL) {
      return;
    }
    DIR *d = opendir(path);
    if (!d)
      return;
    struct dirent *e;
    while ((e = readdir(d))) {
      if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
        continue;
      char child[1024];
      snprintf(child, sizeof(child), "%s/%s", path, e->d_name);
      collect_paths_to_remove(child, paths, count, cap);
    }
    closedir(d);
  } else if (S_ISREG(st.st_mode)) {
    if (*count == *cap) {
      *cap = *cap ? *cap * 2 : 256;
      *paths = realloc(*paths, *cap * sizeof(char *));
    }
    (*paths)[*count] = strdup2(path);
    (*count)++;
  }
}

int cmd_rm(int argc, char *argv[]) {
  if (check_repo() == -1) {
    return 1;
  }

  int cached_only = 0;
  int recursive = 0;
  char **paths_to_remove = NULL;
  size_t path_count = 0;

  // Manual parsing for simplicity, avoiding arg_parser dependency here
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--cached") == 0) {
      cached_only = 1;
    } else if (strcmp(argv[i], "-r") == 0) {
      recursive = 1;
    } else {
      path_count++;
      paths_to_remove = realloc(paths_to_remove, path_count * sizeof(char *));
      paths_to_remove[path_count - 1] = argv[i];
    }
  }

  if (path_count == 0) {
    fprintf(stderr, "Usage: avc rm [--cached] [-r] <file...>\n");
    free(paths_to_remove);
    return 1;
  }

  if (index_load() != 0) {
    fprintf(stderr, "Failed to load index.\n");
    free(paths_to_remove);
    return 1;
  }

  for (size_t i = 0; i < path_count; i++) {
    const char *path = paths_to_remove[i];
    struct stat st;

    if (stat(path, &st) == -1) {
      fprintf(stderr, "Path '%s' does not exist\n", path);
      continue;
    }

    if (S_ISDIR(st.st_mode)) {
      if (!recursive) {
        fprintf(
            stderr,
            "Cannot remove directory '%s': use -r flag for recursive removal\n",
            path);
        continue;
      }

      char **files_in_dir = NULL;
      size_t file_count = 0, file_cap = 0;
      collect_paths_to_remove(path, &files_in_dir, &file_count, &file_cap);

      for (size_t j = 0; j < file_count; j++) {
        index_upsert_entry(files_in_dir[j], NULL, 0, NULL); // NULL hash removes
        printf("Removed '%s' from staging area\n", files_in_dir[j]);
        if (!cached_only) {
          unlink(files_in_dir[j]);
        }
        free(files_in_dir[j]);
      }
      free(files_in_dir);

      if (!cached_only) {
        if (remove_directory_recursive(path) == -1) {
          perror("Failed to remove directory from working directory");
        }
      }
    } else {
      index_upsert_entry(path, NULL, 0, NULL); // NULL hash removes
      printf("Removed '%s' from staging area\n", path);
      if (!cached_only) {
        if (unlink(path) == -1) {
          perror("Failed to remove file from working directory");
        }
      }
    }
  }

  if (index_commit() != 0) {
    fprintf(stderr, "Failed to commit index changes.\n");
    free(paths_to_remove);
    return 1;
  }

  free(paths_to_remove);
  return 0;
}