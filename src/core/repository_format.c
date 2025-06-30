#include "repository_format.h"
#include "compression.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define AVC_FORMAT_FILE ".avc/format"

int avc_repo_get_format_version(void) {
    FILE* f = fopen(AVC_FORMAT_FILE, "rb");
    if (!f) {
        // No format file = version 1 (legacy)
        return AVC_FORMAT_VERSION_1;
    }
    
    avc_repo_format_t format;
    if (fread(&format, sizeof(format), 1, f) != 1) {
        fclose(f);
        return AVC_FORMAT_VERSION_1;
    }
    
    fclose(f);
    return format.version;
}

int avc_repo_set_format_version(uint32_t version) {
    FILE* f = fopen(AVC_FORMAT_FILE, "wb");
    if (!f) return -1;
    
    avc_repo_format_t format = {
        .version = version,
        .compression_type = (version >= AVC_FORMAT_VERSION_2) ? AVC_COMPRESS_ZSTD : AVC_COMPRESS_LIBDEFLATE,
        .flags = 0,
        .reserved = 0
    };
    
    if (fwrite(&format, sizeof(format), 1, f) != 1) {
        fclose(f);
        return -1;
    }
    
    fclose(f);
    
    // Set compression backend based on format
    avc_set_compression_backend(format.compression_type);
    
    return 0;
}

int avc_repo_upgrade_format(uint32_t target_version) {
    uint32_t current = avc_repo_get_format_version();
    
    if (current >= target_version) {
        return 0; // Already at target or higher
    }
    
    printf("Upgrading repository format from v%u to v%u...\n", current, target_version);
    
    // For now, just update the format file
    // Future: Add migration logic here
    return avc_repo_set_format_version(target_version);
}

int avc_repo_is_compatible(uint32_t version) {
    return version <= AVC_FORMAT_CURRENT;
}