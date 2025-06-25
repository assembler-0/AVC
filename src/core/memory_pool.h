#ifndef MEMORY_POOL_H
#define MEMORY_POOL_H

#include <stddef.h>

// Fast memory pool for reducing malloc/free overhead
typedef struct memory_pool {
    char* buffer;
    size_t size;
    size_t used;
    struct memory_pool* next;
} memory_pool_t;

// Initialize thread-local memory pool
void memory_pool_init(void);

// Fast allocation from pool
void* memory_pool_alloc(size_t size);

// Reset pool (reuse memory)
void memory_pool_reset(void);

// Free entire pool
void memory_pool_free(void);

#endif // MEMORY_POOL_H