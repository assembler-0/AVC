#include "file_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
char* read_file(const char* filepath, size_t* size) {
    FILE* file = fopen(filepath, "rb");
    if (!file) {
        perror("fopen");
        return NULL;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    *size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Allocate buffer
    char* content = malloc(*size + 1);
    if (!content) {
        fclose(file);
        return NULL;
    }

    // Read content
    fread(content, 1, *size, file);
    content[*size] = '\0';

    fclose(file);
    return content;
}

