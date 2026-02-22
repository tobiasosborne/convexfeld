/**
 * @file eta_pool.c
 * @brief Arena allocator for eta vectors (P2.3)
 *
 * Bump-pointer allocation with exponential chunk growth.
 * All memory freed in bulk at refactorization via cxf_eta_pool_reset.
 * O(1) deallocation instead of O(k) individual free chain.
 *
 * Spec: basis_state.md Memory Pool, eta_vector.md Arena Allocation
 * Beads: auj4
 */

#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_basis.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief Create an eta memory pool.
 */
EtaBuffer *cxf_eta_pool_create(size_t initial_size) {
    EtaBuffer *pool = (EtaBuffer *)calloc(1, sizeof(EtaBuffer));
    if (pool == NULL) return NULL;

    if (initial_size < CXF_MIN_CHUNK_SIZE)
        initial_size = CXF_MIN_CHUNK_SIZE;

    pool->minChunkSize = initial_size;
    pool->currentChunkSize = initial_size;

    /* Allocate first chunk */
    EtaChunk *chunk = (EtaChunk *)calloc(1, sizeof(EtaChunk));
    if (chunk == NULL) { free(pool); return NULL; }

    chunk->data = (char *)malloc(initial_size);
    if (chunk->data == NULL) { free(chunk); free(pool); return NULL; }

    chunk->capacity = initial_size;
    chunk->next = NULL;
    pool->firstChunk = chunk;
    pool->activeChunk = chunk;
    pool->bytesUsed = 0;

    return pool;
}

/**
 * @brief Allocate memory from the eta pool (bump pointer).
 *
 * Returns aligned memory. If current chunk is full, allocates
 * a new chunk with exponential growth (up to CXF_MAX_CHUNK_SIZE).
 */
void *cxf_eta_pool_alloc(EtaBuffer *pool, size_t size) {
    if (pool == NULL || size == 0) return NULL;

    /* Align to 8 bytes */
    size = (size + 7) & ~(size_t)7;

    /* Try current chunk */
    if (pool->activeChunk != NULL &&
        pool->bytesUsed + size <= pool->activeChunk->capacity) {
        void *ptr = pool->activeChunk->data + pool->bytesUsed;
        pool->bytesUsed += size;
        memset(ptr, 0, size);
        return ptr;
    }

    /* Need new chunk */
    size_t new_size = pool->currentChunkSize * 2;
    if (new_size > CXF_MAX_CHUNK_SIZE) new_size = CXF_MAX_CHUNK_SIZE;
    if (new_size < size) new_size = size;

    EtaChunk *chunk = (EtaChunk *)calloc(1, sizeof(EtaChunk));
    if (chunk == NULL) return NULL;

    chunk->data = (char *)malloc(new_size);
    if (chunk->data == NULL) { free(chunk); return NULL; }

    chunk->capacity = new_size;
    chunk->next = NULL;

    /* Link new chunk */
    if (pool->activeChunk != NULL)
        pool->activeChunk->next = chunk;
    else
        pool->firstChunk = chunk;

    pool->activeChunk = chunk;
    pool->bytesUsed = size;
    pool->currentChunkSize = new_size;

    memset(chunk->data, 0, size);
    return chunk->data;
}

/**
 * @brief Reset pool for reuse (O(1) bulk deallocation).
 *
 * Keeps the first chunk allocated; frees additional chunks.
 * Resets bump pointer to start of first chunk.
 */
void cxf_eta_pool_reset(EtaBuffer *pool) {
    if (pool == NULL) return;

    /* Free all chunks except the first */
    EtaChunk *chunk = pool->firstChunk;
    if (chunk != NULL) {
        EtaChunk *next = chunk->next;
        while (next != NULL) {
            EtaChunk *tmp = next->next;
            free(next->data);
            free(next);
            next = tmp;
        }
        chunk->next = NULL;
    }

    pool->activeChunk = pool->firstChunk;
    pool->bytesUsed = 0;
    pool->currentChunkSize = pool->minChunkSize;
}

/**
 * @brief Free the entire pool and all chunks.
 */
/**
 * @brief Clear eta list: pool reset or individual free chain (e2t DRY fix).
 *
 * Shared implementation used by refactor.c and warm.c.
 */
void cxf_eta_list_clear(BasisState *basis) {
    if (basis == NULL) return;
    if (basis->eta_pool != NULL) {
        cxf_eta_pool_reset(basis->eta_pool);
    } else {
        EtaVector *eta = basis->eta_head;
        while (eta != NULL) {
            EtaVector *next = eta->next;
            free(eta->indices);
            free(eta->values);
            free(eta);
            eta = next;
        }
    }
    basis->eta_head = NULL;
    basis->eta_count = 0;
    basis->pivots_since_refactor = 0;
}

void cxf_eta_pool_free(EtaBuffer *pool) {
    if (pool == NULL) return;

    EtaChunk *chunk = pool->firstChunk;
    while (chunk != NULL) {
        EtaChunk *next = chunk->next;
        free(chunk->data);
        free(chunk);
        chunk = next;
    }
    free(pool);
}
