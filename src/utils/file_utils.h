#include <stdio.h>
#ifndef FILE_UTILS_H
#define FILE_UTILS_H

char* read_file(const char* filepath, size_t* size);
int write_file(const char* filepath, const char* content, size_t size);
char* strdup2(const char* s);
int remove_directory_recursive(const char* path);

#endif // FILE_UTILS_H