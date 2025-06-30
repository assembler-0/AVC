#include "index.h"
#include "fast_index.h"
#include "file_utils.h"
#include "objects.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static fast_index_t *fast_idx = NULL;
static int idx_loaded = 0;

const char *index_get_hash(const char *filepath) {
  if (!idx_loaded || !fast_idx)
    return NULL;
  return fast_index_get_hash(fast_idx, filepath);
}

int index_load(void) {
  if (idx_loaded)
    return 0;

  if (!fast_idx) {
    fast_idx = fast_index_create();
    if (!fast_idx)
      return -1;
  }

  int result = fast_index_load(fast_idx);
  if (result == 0) {
    idx_loaded = 1;
  }

  return result;
}

int index_upsert_entry(const char *filepath, const char *hash, unsigned int mode,
                       int *unchanged_out) {
  if (!idx_loaded || !fast_idx) {
    // Fallback or error, depending on desired behavior for unloaded index
    return -1;
  }

  if (unchanged_out)
    *unchanged_out = 0;

  const char *old_hash = fast_index_get_hash(fast_idx, filepath);
  if (hash && old_hash && strcmp(old_hash, hash) == 0) {
    if (unchanged_out)
      *unchanged_out = 1;
    return 0;
  }

  if (hash) {
    return fast_index_set(fast_idx, filepath, hash, mode);
  } else {
    return fast_index_remove(fast_idx, filepath);
  }
}

int index_commit(void) {
  if (!idx_loaded || !fast_idx)
    return 0;

  int result = fast_index_commit(fast_idx);

  fast_index_free(fast_idx);
  fast_idx = NULL;
  idx_loaded = 0;

  return result;
}

int is_file_in_index(const char *filepath) {
  return index_get_hash(filepath) != NULL;
}

int clear_index(void) {
  FILE *index = fopen(".avc/index", "w");
  if (!index) {
    perror("Failed to clear index");
    return -1;
  }
  fclose(index);
  return 0;
}