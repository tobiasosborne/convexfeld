/**
 * @file vectors.c
 * @brief Vector memory management and eta buffer arena allocator.
 *
 * Provides cxf_vector_free for deallocating VectorContainer structures,
 * and cxf_alloc_eta for arena-based allocation of eta vectors.
 *
 * @see docs/specs/functions/memory/cxf_vector_free.md
 * @see docs/specs/functions/memory/cxf_alloc_eta.md
 */

#include <stdlib.h>
#include "convexfeld/cxf_types.h"

/* External declarations for core memory functions */
void *cxf_malloc(size_t size);
void *cxf_calloc(size_t count, size_t size);
void cxf_free(void *ptr);

/**
 * @brief Deallocate a vector container and all its arrays.
 *
 * Frees indices, values, auxData (if allocated), then the container.
 * Safe to call with NULL (no-op).
 *
 * @param vec Pointer to VectorContainer to free (NULL is safe)
 */
void cxf_vector_free(VectorContainer *vec) {
    if (vec == NULL) {
        return;
    }

    /* Free arrays (each checks for NULL internally via cxf_free) */
    cxf_free(vec->indices);
    cxf_free(vec->values);
    cxf_free(vec->auxData);

    /* Free the container structure */
    cxf_free(vec);
}

/**
 * @brief Initialize an eta buffer for arena allocation.
 *
 * Sets up the buffer with no chunks allocated. First allocation
 * will create the initial chunk.
 *
 * @param buffer Pointer to EtaBuffer to initialize
 * @param min_chunk_size Minimum chunk size (also initial size)
 */
void cxf_eta_buffer_init(EtaBuffer *buffer, size_t min_chunk_size) {
    if (buffer == NULL) {
        return;
    }

    buffer->firstChunk = NULL;
    buffer->activeChunk = NULL;
    buffer->bytesUsed = 0;
    buffer->currentChunkSize = min_chunk_size;
    buffer->minChunkSize = min_chunk_size;
}

/**
 * @brief Free all chunks in an eta buffer.
 *
 * Walks the chunk chain and frees each chunk's data and header.
 * Resets buffer to empty state.
 *
 * @param buffer Pointer to EtaBuffer to free
 */
void cxf_eta_buffer_free(EtaBuffer *buffer) {
    if (buffer == NULL) {
        return;
    }

    EtaChunk *chunk = buffer->firstChunk;
    while (chunk != NULL) {
        EtaChunk *next = chunk->next;
        cxf_free(chunk->data);
        cxf_free(chunk);
        chunk = next;
    }

    buffer->firstChunk = NULL;
    buffer->activeChunk = NULL;
    buffer->bytesUsed = 0;
}

/**
 * @brief Reset an eta buffer for reuse without freeing chunks.
 *
 * Resets allocation position to the beginning of the first chunk.
 * Existing chunks are retained for future allocations.
 *
 * @param buffer Pointer to EtaBuffer to reset
 */
void cxf_eta_buffer_reset(EtaBuffer *buffer) {
    if (buffer == NULL) {
        return;
    }

    /* Reset to use first chunk again */
    buffer->activeChunk = buffer->firstChunk;
    buffer->bytesUsed = 0;
}

/**
 * @brief Allocate memory from the eta buffer arena.
 *
 * Fast path: bump pointer in active chunk if space available.
 * Slow path: allocate new chunk, link to chain, update growth.
 *
 * @param env Environment pointer (unused, for future compatibility)
 * @param buffer Eta buffer to allocate from
 * @param size Number of bytes to allocate
 * @return Pointer to allocated memory, or NULL on failure
 */
void *cxf_alloc_eta(CxfEnv *env, EtaBuffer *buffer, size_t size) {
    (void)env;  /* Unused for now */

    if (buffer == NULL || size == 0) {
        return NULL;
    }

    EtaChunk *active = buffer->activeChunk;

    /* Fast path: allocate from current chunk */
    if (active != NULL && size <= (active->capacity - buffer->bytesUsed)) {
        void *ptr = active->data + buffer->bytesUsed;
        buffer->bytesUsed += size;
        return ptr;
    }

    /* Slow path: need a new chunk */

    /* Determine chunk size: at least size, at least currentChunkSize */
    size_t chunk_size = buffer->currentChunkSize;
    if (size > chunk_size) {
        chunk_size = size;
    }

    /* Allocate chunk header */
    EtaChunk *new_chunk = cxf_calloc(1, sizeof(EtaChunk));
    if (new_chunk == NULL) {
        return NULL;
    }

    /* Allocate chunk data */
    new_chunk->data = cxf_malloc(chunk_size);
    if (new_chunk->data == NULL) {
        cxf_free(new_chunk);
        return NULL;
    }

    new_chunk->capacity = chunk_size;
    new_chunk->next = NULL;

    /* Link to chain */
    if (active != NULL) {
        active->next = new_chunk;
    } else {
        buffer->firstChunk = new_chunk;
    }

    /* Update buffer state */
    buffer->activeChunk = new_chunk;
    buffer->bytesUsed = size;

    /* Exponential growth for next chunk */
    size_t next_size = chunk_size * 2;
    if (next_size < buffer->minChunkSize) {
        next_size = buffer->minChunkSize;
    }
    if (next_size > CXF_MAX_CHUNK_SIZE) {
        next_size = CXF_MAX_CHUNK_SIZE;
    }
    buffer->currentChunkSize = next_size;

    return new_chunk->data;
}
