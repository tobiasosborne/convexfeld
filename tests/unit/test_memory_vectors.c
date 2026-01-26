/**
 * @file test_memory_vectors.c
 * @brief TDD tests for vector memory management (M2.1.3)
 *
 * Tests for cxf_vector_free and cxf_alloc_eta.
 */

#include "unity.h"
#include "convexfeld/cxf_types.h"
#include <stddef.h>
#include <string.h>

/* External declarations for memory functions */
void *cxf_malloc(size_t size);
void *cxf_calloc(size_t count, size_t size);
void cxf_free(void *ptr);

/* Functions under test */
void cxf_vector_free(VectorContainer *vec);
void *cxf_alloc_eta(CxfEnv *env, EtaBuffer *buffer, size_t size);
void cxf_eta_buffer_init(EtaBuffer *buffer, size_t min_chunk_size);
void cxf_eta_buffer_free(EtaBuffer *buffer);
void cxf_eta_buffer_reset(EtaBuffer *buffer);

void setUp(void) {}
void tearDown(void) {}

/*----------------------------------------------------------------------------*/
/* Helper: Create a VectorContainer for testing                               */
/*----------------------------------------------------------------------------*/

static VectorContainer *create_test_vector(int capacity, int with_values,
                                           int with_aux) {
    VectorContainer *vec = cxf_calloc(1, sizeof(VectorContainer));
    if (!vec) return NULL;

    vec->capacity = capacity;
    vec->size = 0;

    if (capacity > 0) {
        vec->indices = cxf_malloc((size_t)capacity * sizeof(int));
        if (!vec->indices) {
            cxf_free(vec);
            return NULL;
        }
    }

    if (with_values && capacity > 0) {
        vec->values = cxf_malloc((size_t)capacity * sizeof(double));
        if (!vec->values) {
            cxf_free(vec->indices);
            cxf_free(vec);
            return NULL;
        }
    }

    if (with_aux) {
        vec->auxData = cxf_malloc(64);
        if (!vec->auxData) {
            cxf_free(vec->values);
            cxf_free(vec->indices);
            cxf_free(vec);
            return NULL;
        }
    }

    return vec;
}

/*----------------------------------------------------------------------------*/
/* cxf_vector_free tests                                                      */
/*----------------------------------------------------------------------------*/

void test_vector_free_null(void) {
    /* Should not crash on NULL */
    cxf_vector_free(NULL);
    TEST_PASS();
}

void test_vector_free_empty_vector(void) {
    /* Empty vector (all pointers NULL) */
    VectorContainer *vec = cxf_calloc(1, sizeof(VectorContainer));
    TEST_ASSERT_NOT_NULL(vec);

    vec->indices = NULL;
    vec->values = NULL;
    vec->auxData = NULL;

    cxf_vector_free(vec);  /* Should free only the container */
    TEST_PASS();
}

void test_vector_free_indices_only(void) {
    /* Vector with only indices allocated */
    VectorContainer *vec = create_test_vector(10, 0, 0);
    TEST_ASSERT_NOT_NULL(vec);

    cxf_vector_free(vec);  /* Should free indices and container */
    TEST_PASS();
}

void test_vector_free_full_vector(void) {
    /* Vector with all arrays allocated */
    VectorContainer *vec = create_test_vector(10, 1, 1);
    TEST_ASSERT_NOT_NULL(vec);

    cxf_vector_free(vec);  /* Should free indices, values, auxData, container */
    TEST_PASS();
}

/*----------------------------------------------------------------------------*/
/* cxf_eta_buffer_init tests                                                  */
/*----------------------------------------------------------------------------*/

void test_eta_buffer_init_basic(void) {
    EtaBuffer buffer;
    cxf_eta_buffer_init(&buffer, CXF_MIN_CHUNK_SIZE);

    TEST_ASSERT_NULL(buffer.firstChunk);
    TEST_ASSERT_NULL(buffer.activeChunk);
    TEST_ASSERT_EQUAL_size_t(0, buffer.bytesUsed);
    TEST_ASSERT_EQUAL_size_t(CXF_MIN_CHUNK_SIZE, buffer.currentChunkSize);
    TEST_ASSERT_EQUAL_size_t(CXF_MIN_CHUNK_SIZE, buffer.minChunkSize);
}

void test_eta_buffer_init_custom_size(void) {
    EtaBuffer buffer;
    cxf_eta_buffer_init(&buffer, 8192);

    TEST_ASSERT_EQUAL_size_t(8192, buffer.currentChunkSize);
    TEST_ASSERT_EQUAL_size_t(8192, buffer.minChunkSize);
}

/*----------------------------------------------------------------------------*/
/* cxf_alloc_eta tests                                                        */
/*----------------------------------------------------------------------------*/

void test_alloc_eta_null_buffer(void) {
    void *ptr = cxf_alloc_eta(NULL, NULL, 100);
    TEST_ASSERT_NULL(ptr);
}

void test_alloc_eta_first_allocation(void) {
    EtaBuffer buffer;
    cxf_eta_buffer_init(&buffer, CXF_MIN_CHUNK_SIZE);

    /* First allocation creates a chunk */
    void *ptr = cxf_alloc_eta(NULL, &buffer, 100);
    TEST_ASSERT_NOT_NULL(ptr);
    TEST_ASSERT_NOT_NULL(buffer.firstChunk);
    TEST_ASSERT_NOT_NULL(buffer.activeChunk);
    TEST_ASSERT_EQUAL_size_t(100, buffer.bytesUsed);

    cxf_eta_buffer_free(&buffer);
}

void test_alloc_eta_fast_path(void) {
    EtaBuffer buffer;
    cxf_eta_buffer_init(&buffer, CXF_MIN_CHUNK_SIZE);

    /* First allocation */
    void *ptr1 = cxf_alloc_eta(NULL, &buffer, 100);
    TEST_ASSERT_NOT_NULL(ptr1);

    /* Second allocation should use same chunk (fast path) */
    void *ptr2 = cxf_alloc_eta(NULL, &buffer, 200);
    TEST_ASSERT_NOT_NULL(ptr2);
    TEST_ASSERT_EQUAL_size_t(300, buffer.bytesUsed);

    /* Both pointers should be in the same chunk */
    TEST_ASSERT_EQUAL_PTR(buffer.firstChunk, buffer.activeChunk);

    cxf_eta_buffer_free(&buffer);
}

void test_alloc_eta_slow_path_new_chunk(void) {
    EtaBuffer buffer;
    cxf_eta_buffer_init(&buffer, 256);  /* Small chunks for testing */

    /* First allocation fills most of the chunk */
    void *ptr1 = cxf_alloc_eta(NULL, &buffer, 200);
    TEST_ASSERT_NOT_NULL(ptr1);
    EtaChunk *first_chunk = buffer.activeChunk;

    /* This allocation requires a new chunk */
    void *ptr2 = cxf_alloc_eta(NULL, &buffer, 200);
    TEST_ASSERT_NOT_NULL(ptr2);

    /* Should have a new active chunk */
    TEST_ASSERT_NOT_EQUAL(first_chunk, buffer.activeChunk);
    TEST_ASSERT_EQUAL_PTR(first_chunk->next, buffer.activeChunk);

    cxf_eta_buffer_free(&buffer);
}

void test_alloc_eta_large_allocation(void) {
    EtaBuffer buffer;
    cxf_eta_buffer_init(&buffer, CXF_MIN_CHUNK_SIZE);

    /* Request larger than current chunk size */
    void *ptr = cxf_alloc_eta(NULL, &buffer, CXF_MIN_CHUNK_SIZE * 2);
    TEST_ASSERT_NOT_NULL(ptr);

    /* Chunk should be at least as large as the request */
    TEST_ASSERT_TRUE(buffer.activeChunk->capacity >= CXF_MIN_CHUNK_SIZE * 2);

    cxf_eta_buffer_free(&buffer);
}

void test_alloc_eta_exponential_growth(void) {
    EtaBuffer buffer;
    cxf_eta_buffer_init(&buffer, 256);

    /* First allocation - chunk size should double for next */
    cxf_alloc_eta(NULL, &buffer, 100);
    TEST_ASSERT_TRUE(buffer.currentChunkSize >= 512);

    cxf_eta_buffer_free(&buffer);
}

void test_alloc_eta_max_chunk_size(void) {
    EtaBuffer buffer;
    cxf_eta_buffer_init(&buffer, CXF_MAX_CHUNK_SIZE / 2);

    /* After allocation, growth should be capped at max */
    cxf_alloc_eta(NULL, &buffer, 100);
    TEST_ASSERT_TRUE(buffer.currentChunkSize <= CXF_MAX_CHUNK_SIZE);

    cxf_eta_buffer_free(&buffer);
}

/*----------------------------------------------------------------------------*/
/* cxf_eta_buffer_free tests                                                  */
/*----------------------------------------------------------------------------*/

void test_eta_buffer_free_empty(void) {
    EtaBuffer buffer;
    cxf_eta_buffer_init(&buffer, CXF_MIN_CHUNK_SIZE);

    /* Free empty buffer - should not crash */
    cxf_eta_buffer_free(&buffer);
    TEST_PASS();
}

void test_eta_buffer_free_with_chunks(void) {
    EtaBuffer buffer;
    cxf_eta_buffer_init(&buffer, 256);

    /* Create multiple chunks */
    cxf_alloc_eta(NULL, &buffer, 200);
    cxf_alloc_eta(NULL, &buffer, 200);
    cxf_alloc_eta(NULL, &buffer, 200);

    /* Free should clean up all chunks */
    cxf_eta_buffer_free(&buffer);
    TEST_PASS();
}

/*----------------------------------------------------------------------------*/
/* cxf_eta_buffer_reset tests                                                 */
/*----------------------------------------------------------------------------*/

void test_eta_buffer_reset_basic(void) {
    EtaBuffer buffer;
    cxf_eta_buffer_init(&buffer, CXF_MIN_CHUNK_SIZE);

    /* Allocate some memory */
    cxf_alloc_eta(NULL, &buffer, 100);
    cxf_alloc_eta(NULL, &buffer, 200);

    /* Reset for reuse */
    cxf_eta_buffer_reset(&buffer);

    /* Buffer should be reset but chunks retained */
    TEST_ASSERT_NOT_NULL(buffer.firstChunk);
    TEST_ASSERT_EQUAL_PTR(buffer.firstChunk, buffer.activeChunk);
    TEST_ASSERT_EQUAL_size_t(0, buffer.bytesUsed);

    /* Can allocate again */
    void *ptr = cxf_alloc_eta(NULL, &buffer, 50);
    TEST_ASSERT_NOT_NULL(ptr);

    cxf_eta_buffer_free(&buffer);
}

/*----------------------------------------------------------------------------*/
/* Main test runner                                                           */
/*----------------------------------------------------------------------------*/

int main(void) {
    UNITY_BEGIN();

    /* cxf_vector_free tests */
    RUN_TEST(test_vector_free_null);
    RUN_TEST(test_vector_free_empty_vector);
    RUN_TEST(test_vector_free_indices_only);
    RUN_TEST(test_vector_free_full_vector);

    /* cxf_eta_buffer_init tests */
    RUN_TEST(test_eta_buffer_init_basic);
    RUN_TEST(test_eta_buffer_init_custom_size);

    /* cxf_alloc_eta tests */
    RUN_TEST(test_alloc_eta_null_buffer);
    RUN_TEST(test_alloc_eta_first_allocation);
    RUN_TEST(test_alloc_eta_fast_path);
    RUN_TEST(test_alloc_eta_slow_path_new_chunk);
    RUN_TEST(test_alloc_eta_large_allocation);
    RUN_TEST(test_alloc_eta_exponential_growth);
    RUN_TEST(test_alloc_eta_max_chunk_size);

    /* cxf_eta_buffer_free tests */
    RUN_TEST(test_eta_buffer_free_empty);
    RUN_TEST(test_eta_buffer_free_with_chunks);

    /* cxf_eta_buffer_reset tests */
    RUN_TEST(test_eta_buffer_reset_basic);

    return UNITY_END();
}
