#include "repository.h"
#include <stdio.h>
#include <sys/stat.h>
#include "commands.h"
int check_repo() {
    struct stat st;
    if (stat(".avc", &st) == -1) {
        fprintf(stderr, "Not an avc repository (no .avc directory found)\n");
        return -1;
    }
    return 0;
}