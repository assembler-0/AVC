// src/commands/version.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>
#include "commands.h"

// Version information
#define AVC_VERSION "0.3.0"
#define AVC_CODENAME "Delta Spectre"
#define AVC_BUILD_DATE __DATE__
#define AVC_BUILD_TIME __TIME__

int cmd_version(int argc, char* argv[]) {
    (void)argc; // Suppress unused parameter warning
    (void)argv;
    
    printf("AVC - Archive Version Control\n");
    printf("Version: %s \"%s\"\n", AVC_VERSION, AVC_CODENAME);
    printf("Build: %s %s\n", AVC_BUILD_DATE, AVC_BUILD_TIME);
    printf("Compiler: %s %s\n", __VERSION__, __DATE__);
    printf("OpenMP: %d threads available\n", omp_get_num_procs());
    printf("\n");
    printf("Features:\n");
    printf("  • SHA-256 based content addressing\n");
    printf("  • Fast libdeflate compression (level 6, optimized)\n");
    printf("  • BLAKE3 hashing (SHA-256 compatible)\n");
    printf("  • Smart compression detection\n");
    printf("  • Multi-threaded processing\n");
    printf("  • Memory-efficient streaming\n");
    printf("\n");
    printf("Commands:\n");
    printf("  init     - Initialize new repository\n");
    printf("  add      - Add files to staging area (parallel)\n");
    printf("  log      - Show commit history\n");
    printf("  status   - Show repository status\n");
    printf("  reset    - Reset to previous commit (--hard, --clean)\n");
    printf("  rm       - Remove files from staging area\n");
    printf("  version  - Show version information\n");
    printf("\n");
    printf("Performance Features:\n");
    printf("  • Multi-threaded compression\n");
    printf("  • Streaming file processing\n");
    printf("  • Memory pool optimization\n");
    printf("  • Smart object deduplication\n");
    printf("\n");
    printf("Author: Atheria\n");
    printf("License: GPL\n");
    printf("Repository: AVC\n");
    
    return 0;
} 