/**
 * @file internal_types.h
 * @brief Full definitions for solver-internal types.
 *
 * VectorContainer, EtaChunk, and EtaBuffer are solver internals that
 * were previously leaked through the public cxf_types.h header.
 * Public headers use forward declarations (pointers only); source
 * files that need the full definition include this header.
 */

#ifndef CXF_INTERNAL_TYPES_H
#define CXF_INTERNAL_TYPES_H

#include <stddef.h>
#include "convexfeld/cxf_types.h"

/**
 * @brief Vector container for sparse vectors with indices and values.
 *
 * Used throughout ConvexFeld for storing sparse vectors, index lists,
 * and coefficient arrays.
 */
struct VectorContainer {
    int *indices;      /**< Array of indices (may be NULL) */
    double *values;    /**< Array of values (may be NULL) */
    void *auxData;     /**< Auxiliary data pointer (may be NULL) */
    int capacity;      /**< Allocated capacity */
    int size;          /**< Current number of elements */
};

/**
 * @brief Chunk in the eta buffer arena allocator.
 *
 * Chunks are linked in a singly-linked list for bulk deallocation.
 */
struct EtaChunk {
    char *data;              /**< Chunk data buffer */
    size_t capacity;         /**< Chunk capacity in bytes */
    struct EtaChunk *next;   /**< Next chunk in chain */
};

/**
 * @brief Arena allocator for eta vectors.
 *
 * Provides efficient bump-pointer allocation with exponential chunk growth.
 * All memory is freed in bulk at refactorization.
 */
struct EtaBuffer {
    EtaChunk *firstChunk;    /**< Head of chunk chain */
    EtaChunk *activeChunk;   /**< Current chunk being allocated from */
    size_t bytesUsed;        /**< Bytes used in active chunk */
    size_t currentChunkSize; /**< Current chunk size for growth */
    size_t minChunkSize;     /**< Minimum chunk size */
};

/* CXF_MAX_CHUNK_SIZE and CXF_MIN_CHUNK_SIZE are in cxf_types.h */

#endif /* CXF_INTERNAL_TYPES_H */
