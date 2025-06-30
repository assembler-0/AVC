#include "repository.h"
#include "repository_format.h"
#include <stdio.h>
#include <sys/stat.h>
#include "commands.h"
int check_repo() {
    struct stat st;
    if (stat(".avc", &st) == -1) {
        fprintf(stderr, "Not an avc repository (no .avc directory found)\n");
        return -1;
    }
    
    // Check format compatibility and auto-upgrade
    uint32_t format_version = avc_repo_get_format_version();
    if (!avc_repo_is_compatible(format_version)) {
        fprintf(stderr, "Repository format v%u is not supported\n", format_version);
        return -1;
    }
    
    // Auto-upgrade to current format
    if (format_version < AVC_FORMAT_CURRENT) {
        avc_repo_upgrade_format(AVC_FORMAT_CURRENT);
    }
    
    return 0;
}