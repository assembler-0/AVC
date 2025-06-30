#ifndef AVC_COMPRESSION_H
#define AVC_COMPRESSION_H

#include <stddef.h>

// Compression backends
typedef enum {
    AVC_COMPRESS_LIBDEFLATE = 0,  // Legacy/Git compatibility
    AVC_COMPRESS_ZSTD = 1         // New high-performance backend
} avc_compression_type_t;

// Unified compression interface
char* avc_compress(const char* data, size_t size, size_t* compressed_size, 
                   avc_compression_type_t type, int level);

char* avc_decompress(const char* compressed_data, size_t compressed_size, 
                     size_t expected_size, avc_compression_type_t type);

// Auto-detect compression type from header
avc_compression_type_t avc_detect_compression_type(const char* data, size_t size);

// Set global compression backend
void avc_set_compression_backend(avc_compression_type_t type);
avc_compression_type_t avc_get_compression_backend(void);

// Cleanup function for multi-threaded contexts
void avc_cleanup_compression_contexts(void);

#endif // AVC_COMPRESSION_H