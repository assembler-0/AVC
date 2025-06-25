#include "memory_pool.h"
#include <stdlib.h>
#include <string.h>

#define POOL_SIZE (1024 * 1024)  // 1MB per pool
#define ALIGNMENT 8

static __thread memory_pool_t* thread_pool = NULL;

void memory_pool_init(void) {
    if (thread_pool) return;
    
    thread_pool = malloc(sizeof(memory_pool_t));
    thread_pool->buffer = malloc(POOL_SIZE);
    thread_pool->size = POOL_SIZE;
    thread_pool->used = 0;
    thread_pool->next = NULL;
}

void* memory_pool_alloc(size_t size) {
    if (!thread_pool) memory_pool_init();
    
    // Align size
    size = (size + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
    
    if (thread_pool->used + size > thread_pool->size) {
        // Pool full, use regular malloc
        return malloc(size);
    }
    
    void* ptr = thread_pool->buffer + thread_pool->used;
    thread_pool->used += size;
    return ptr;
}

void memory_pool_reset(void) {
    if (thread_pool) {
        thread_pool->used = 0;
    }
}

void memory_pool_free(void) {
    if (thread_pool) {
        free(thread_pool->buffer);
        free(thread_pool);
        thread_pool = NULL;
    }
}