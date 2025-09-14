// src/commands/version.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>
#include "commands.h"

// Version information
#define AVC_VERSION "0.4.0"
#define AVC_CODENAME "Velocity"
#define AVC_BUILD_DATE __DATE__
#define AVC_BUILD_TIME __TIME__

int cmd_version(int argc, char* argv[]) {
    (void)argc; // Suppress unused parameter warning
    (void)argv;
    
    printf("AVC - Archive Version Control\n");
    printf("Version: %s \"%s\"\n", AVC_VERSION, AVC_CODENAME);
    printf("Build: %s %s\n", AVC_BUILD_DATE, AVC_BUILD_TIME);
    printf("Compile time: %s %s\n", __VERSION__, __DATE__);
    printf("OpenMP: %d threads available\n", omp_get_num_procs());
    printf("\n");
    
    return 0;
} 
