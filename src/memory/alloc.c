/**
 * @file alloc.c
 * @brief Core memory allocation functions for ConvexFeld.
 *
 * Provides cxf_malloc, cxf_calloc, cxf_realloc, cxf_free.
 *
 * Current implementation wraps standard library functions with
 * edge case handling. Environment-scoped allocation with memory
 * tracking and thread safety will be added in M3 (Threading).
 *
 * @see docs/specs/functions/memory/cxf_malloc.md
 * @see docs/specs/functions/memory/cxf_calloc.md
 * @see docs/specs/functions/memory/cxf_realloc.md
 * @see docs/specs/functions/memory/cxf_free.md
 */

#include <stdlib.h>
#include <string.h>
#include "convexfeld/cxf_types.h"

/**
 * @brief Allocate memory.
 *
 * Allocates at least 'size' bytes of memory aligned for any data type.
 * Returns NULL if size is 0 or allocation fails.
 *
 * @param size Number of bytes to allocate (must be > 0)
 * @return Pointer to allocated memory, or NULL on failure
 *
 * @note Future: Will take CxfEnv* parameter for environment-scoped
 *       allocation with tracking and memory limits.
 */
void *cxf_malloc(size_t size) {
    if (size == 0) {
        return NULL;
    }
    return malloc(size);
}

/**
 * @brief Allocate zero-initialized memory.
 *
 * Allocates 'count * size' bytes of memory, all initialized to zero.
 * Returns NULL if count or size is 0, or allocation fails.
 *
 * @param count Number of elements to allocate
 * @param size Size of each element in bytes
 * @return Pointer to zero-initialized memory, or NULL on failure
 *
 * @note Future: Will take CxfEnv* parameter for environment-scoped
 *       allocation with tracking and memory limits.
 */
void *cxf_calloc(size_t count, size_t size) {
    if (count == 0 || size == 0) {
        return NULL;
    }
    return calloc(count, size);
}

/**
 * @brief Reallocate memory.
 *
 * Resizes a previously allocated memory block. Original contents are
 * preserved up to the minimum of old and new sizes. If ptr is NULL,
 * behaves like cxf_malloc. If new_size is 0, frees ptr and returns NULL.
 *
 * @param ptr Pointer to existing allocation (NULL acts like malloc)
 * @param new_size New size in bytes (0 frees and returns NULL)
 * @return Pointer to resized memory, or NULL on failure/special cases
 *
 * @warning On failure, the original pointer remains valid. Use pattern:
 *          temp = cxf_realloc(ptr, size); if (temp) ptr = temp;
 *
 * @note Future: Will take CxfEnv* parameter for environment-scoped
 *       allocation with tracking and memory limits.
 */
void *cxf_realloc(void *ptr, size_t new_size) {
    /* NULL pointer acts like malloc */
    if (ptr == NULL) {
        return cxf_malloc(new_size);
    }

    /* Zero size acts like free */
    if (new_size == 0) {
        free(ptr);
        return NULL;
    }

    return realloc(ptr, new_size);
}

/**
 * @brief Free allocated memory.
 *
 * Deallocates memory previously allocated by cxf_malloc, cxf_calloc,
 * or cxf_realloc. Safe to call with NULL pointer (no-op).
 *
 * @param ptr Pointer to memory to free (NULL is safe)
 *
 * @warning Do not free the same pointer twice (undefined behavior).
 * @warning Only free pointers from cxf_ allocation functions.
 *
 * @note Future: Will take CxfEnv* parameter for environment-scoped
 *       deallocation with tracking updates.
 */
void cxf_free(void *ptr) {
    free(ptr);
}
