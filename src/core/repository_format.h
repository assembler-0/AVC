#ifndef AVC_REPOSITORY_FORMAT_H
#define AVC_REPOSITORY_FORMAT_H

#include <stdint.h>

// Repository format versions
#define AVC_FORMAT_VERSION_1 1  // Original libdeflate format
#define AVC_FORMAT_VERSION_2 2  // New zstd format
#define AVC_FORMAT_CURRENT AVC_FORMAT_VERSION_2

// Repository format info
typedef struct {
    uint32_t version;
    uint32_t compression_type;  // 0=libdeflate, 1=zstd
    uint32_t flags;            // Future extensibility
    uint32_t reserved;         // Padding
} avc_repo_format_t;

// Format management functions
int avc_repo_get_format_version(void);
int avc_repo_set_format_version(uint32_t version);
int avc_repo_upgrade_format(uint32_t target_version);
int avc_repo_is_compatible(uint32_t version);

#endif // AVC_REPOSITORY_FORMAT_H