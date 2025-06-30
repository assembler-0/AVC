#include "compression.h"
#include <stdlib.h>
#include <string.h>
#include <zstd.h>
#include <omp.h>

// Multi-threaded zstd contexts (thread-local)
static __thread ZSTD_CCtx* g_cctx = NULL;
static __thread ZSTD_DCtx* g_dctx = NULL;

// Initialize thread-local contexts
static void init_zstd_contexts(void) {
    if (!g_cctx) {
        g_cctx = ZSTD_createCCtx();
        if (g_cctx) {
            // Enable multi-threading
            int num_threads = omp_get_num_procs();
            ZSTD_CCtx_setParameter(g_cctx, ZSTD_c_nbWorkers, num_threads);
            ZSTD_CCtx_setParameter(g_cctx, ZSTD_c_jobSize, 1024*1024); // 1MB jobs
        }
    }
    if (!g_dctx) {
        g_dctx = ZSTD_createDCtx();
    }
}

// Global compression backend
static avc_compression_type_t g_compression_backend = AVC_COMPRESS_ZSTD;

// Zstd magic number
#define ZSTD_MAGIC 0xFD2FB528

void avc_set_compression_backend(avc_compression_type_t type) {
    g_compression_backend = type;
}

avc_compression_type_t avc_get_compression_backend(void) {
    return g_compression_backend;
}

avc_compression_type_t avc_detect_compression_type(const char* data, size_t size) {
    if (size < 4) return AVC_COMPRESS_LIBDEFLATE;
    
    // Check for zstd magic number
    uint32_t magic = *(uint32_t*)data;
    if (magic == ZSTD_MAGIC) {
        return AVC_COMPRESS_ZSTD;
    }
    
    // Check for zlib header (libdeflate)
    unsigned char first = (unsigned char)data[0];
    if ((first & 0x0f) == 0x08) {
        return AVC_COMPRESS_LIBDEFLATE;
    }
    
    return AVC_COMPRESS_LIBDEFLATE; // Default fallback
}

char* avc_compress(const char* data, size_t size, size_t* compressed_size, 
                   avc_compression_type_t type, int level) {
    switch (type) {
        case AVC_COMPRESS_ZSTD: {
            init_zstd_contexts();
            
            size_t max_size = ZSTD_compressBound(size);
            char* compressed = malloc(max_size);
            if (!compressed) return NULL;
            
            // Multi-threaded compression for large files
            size_t result;
            if (size > 1024*1024 && g_cctx) { // >1MB = multi-threaded
                ZSTD_CCtx_setParameter(g_cctx, ZSTD_c_compressionLevel, level);
                result = ZSTD_compress2(g_cctx, compressed, max_size, data, size);
            } else {
                result = ZSTD_compress(compressed, max_size, data, size, level);
            }
            
            if (ZSTD_isError(result)) {
                free(compressed);
                return NULL;
            }
            
            *compressed_size = result;
            return realloc(compressed, result);
        }
        
        case AVC_COMPRESS_LIBDEFLATE: {
            // Legacy format - use zstd with fake zlib header
            size_t max_size = ZSTD_compressBound(size);
            char* compressed = malloc(max_size + 2);
            if (!compressed) return NULL;
            
            // Fake zlib header for compatibility
            compressed[0] = 0x78;
            compressed[1] = 0x9c;
            
            // Multi-threaded for large files even in legacy mode
            size_t result;
            if (size > 1024*1024) {
                init_zstd_contexts();
                if (g_cctx) {
                    ZSTD_CCtx_setParameter(g_cctx, ZSTD_c_compressionLevel, level);
                    result = ZSTD_compress2(g_cctx, compressed + 2, max_size - 2, data, size);
                } else {
                    result = ZSTD_compress(compressed + 2, max_size - 2, data, size, level);
                }
            } else {
                result = ZSTD_compress(compressed + 2, max_size - 2, data, size, level);
            }
            
            if (ZSTD_isError(result)) {
                free(compressed);
                return NULL;
            }
            
            *compressed_size = result + 2;
            return realloc(compressed, *compressed_size);
        }
    }
    
    return NULL;
}

char* avc_decompress(const char* compressed_data, size_t compressed_size, 
                     size_t expected_size, avc_compression_type_t type) {
    switch (type) {
        case AVC_COMPRESS_ZSTD: {
            init_zstd_contexts();
            
            char* decompressed = malloc(expected_size);
            if (!decompressed) return NULL;
            
            // Multi-threaded decompression for large files
            size_t result;
            if (expected_size > 1024*1024 && g_dctx) { // >1MB = multi-threaded
                result = ZSTD_decompressDCtx(g_dctx, decompressed, expected_size, compressed_data, compressed_size);
            } else {
                result = ZSTD_decompress(decompressed, expected_size, compressed_data, compressed_size);
            }
            
            if (ZSTD_isError(result)) {
                free(decompressed);
                return NULL;
            }
            
            return decompressed;
        }
        
        case AVC_COMPRESS_LIBDEFLATE: {
            // Legacy format - skip fake zlib header
            if (compressed_size < 2) return NULL;
            
            char* decompressed = malloc(expected_size);
            if (!decompressed) return NULL;
            
            // Multi-threaded decompression for large files
            size_t result;
            if (expected_size > 1024*1024) {
                init_zstd_contexts();
                if (g_dctx) {
                    result = ZSTD_decompressDCtx(g_dctx, decompressed, expected_size, 
                                               compressed_data + 2, compressed_size - 2);
                } else {
                    result = ZSTD_decompress(decompressed, expected_size, 
                                           compressed_data + 2, compressed_size - 2);
                }
            } else {
                result = ZSTD_decompress(decompressed, expected_size, 
                                       compressed_data + 2, compressed_size - 2);
            }
            
            if (ZSTD_isError(result)) {
                free(decompressed);
                return NULL;
            }
            
            return decompressed;
        }
    }
    
    return NULL;
}

// Cleanup compression contexts
void avc_cleanup_compression_contexts(void) {
    if (g_cctx) {
        ZSTD_freeCCtx(g_cctx);
        g_cctx = NULL;
    }
    if (g_dctx) {
        ZSTD_freeDCtx(g_dctx);
        g_dctx = NULL;
    }
}