/**
 * @file memory_internal.h
 * @brief Internal declarations for memory allocation functions.
 *
 * Replaces scattered extern declarations of cxf_malloc/calloc/realloc/free.
 */

#ifndef MEMORY_INTERNAL_H
#define MEMORY_INTERNAL_H

#include <stddef.h>

void *cxf_malloc(size_t size);
void *cxf_calloc(size_t count, size_t size);
void *cxf_realloc(void *ptr, size_t size);
void  cxf_free(void *ptr);

#endif /* MEMORY_INTERNAL_H */
