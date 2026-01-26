/**
 * @file alloc_stub.c
 * @brief Stub memory allocation functions for tracer bullet.
 *
 * These are minimal wrappers around standard library functions.
 * Full implementation with environment tracking comes in M2.1.
 */

#include <stdlib.h>
#include <string.h>
#include "convexfeld/cxf_types.h"

/**
 * @brief Allocate memory (stub - wraps malloc).
 * @param size Number of bytes to allocate
 * @return Pointer to allocated memory, or NULL on failure
 */
void *cxf_malloc(size_t size) {
    if (size == 0) {
        return NULL;
    }
    return malloc(size);
}

/**
 * @brief Allocate zero-initialized memory (stub - wraps calloc).
 * @param count Number of elements
 * @param size Size of each element
 * @return Pointer to zero-initialized memory, or NULL on failure
 */
void *cxf_calloc(size_t count, size_t size) {
    if (count == 0 || size == 0) {
        return NULL;
    }
    return calloc(count, size);
}

/**
 * @brief Reallocate memory (stub - wraps realloc).
 * @param ptr Pointer to existing memory (NULL acts like malloc)
 * @param size New size in bytes (0 frees and returns NULL)
 * @return Pointer to reallocated memory, or NULL on failure
 */
void *cxf_realloc(void *ptr, size_t size) {
    if (size == 0) {
        free(ptr);
        return NULL;
    }
    return realloc(ptr, size);
}

/**
 * @brief Free memory (stub - wraps free).
 * @param ptr Pointer to memory to free (NULL is safe)
 */
void cxf_free(void *ptr) {
    free(ptr);
}
