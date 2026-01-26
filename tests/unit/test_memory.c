/**
 * @file test_memory.c
 * @brief TDD tests for memory management module (M2.1.1)
 *
 * Tests for cxf_malloc, cxf_calloc, cxf_realloc, cxf_free.
 */

#include "unity.h"
#include <stddef.h>

/* External declarations for memory functions */
void *cxf_malloc(size_t size);
void *cxf_calloc(size_t count, size_t size);
void *cxf_realloc(void *ptr, size_t size);
void cxf_free(void *ptr);

/* Forward declarations for structures */
struct SolverContext;
struct BasisState;
struct CallbackContext;
typedef struct SolverContext SolverContext;
typedef struct BasisState BasisState;
typedef struct CallbackContext CallbackContext;

/* State cleanup functions (M2.1.4) */
void cxf_free_solver_state(SolverContext *ctx);
void cxf_free_basis_state(BasisState *basis);
void cxf_free_callback_state(CallbackContext *ctx);

void setUp(void) {}
void tearDown(void) {}

/*----------------------------------------------------------------------------*/
/* cxf_malloc tests                                                           */
/*----------------------------------------------------------------------------*/

void test_cxf_malloc_basic(void) {
    void *ptr = cxf_malloc(100);
    TEST_ASSERT_NOT_NULL(ptr);
    cxf_free(ptr);
}

void test_cxf_malloc_zero_size(void) {
    void *ptr = cxf_malloc(0);
    TEST_ASSERT_NULL(ptr);
}

void test_cxf_malloc_large_size(void) {
    /* Allocate 1 MB - should succeed on modern systems */
    void *ptr = cxf_malloc(1024 * 1024);
    TEST_ASSERT_NOT_NULL(ptr);
    cxf_free(ptr);
}

/*----------------------------------------------------------------------------*/
/* cxf_calloc tests                                                           */
/*----------------------------------------------------------------------------*/

void test_cxf_calloc_zeroed(void) {
    int *arr = (int *)cxf_calloc(10, sizeof(int));
    TEST_ASSERT_NOT_NULL(arr);
    for (int i = 0; i < 10; i++) {
        TEST_ASSERT_EQUAL_INT(0, arr[i]);
    }
    cxf_free(arr);
}

void test_cxf_calloc_zero_count(void) {
    void *ptr = cxf_calloc(0, sizeof(int));
    TEST_ASSERT_NULL(ptr);
}

void test_cxf_calloc_zero_size(void) {
    void *ptr = cxf_calloc(10, 0);
    TEST_ASSERT_NULL(ptr);
}

/*----------------------------------------------------------------------------*/
/* cxf_realloc tests                                                          */
/*----------------------------------------------------------------------------*/

void test_cxf_realloc_grow(void) {
    int *arr = (int *)cxf_malloc(10 * sizeof(int));
    TEST_ASSERT_NOT_NULL(arr);
    arr[0] = 42;
    arr[9] = 99;

    arr = (int *)cxf_realloc(arr, 20 * sizeof(int));
    TEST_ASSERT_NOT_NULL(arr);
    TEST_ASSERT_EQUAL_INT(42, arr[0]);
    TEST_ASSERT_EQUAL_INT(99, arr[9]);
    cxf_free(arr);
}

void test_cxf_realloc_shrink(void) {
    int *arr = (int *)cxf_malloc(20 * sizeof(int));
    TEST_ASSERT_NOT_NULL(arr);
    arr[0] = 42;
    arr[5] = 55;

    arr = (int *)cxf_realloc(arr, 10 * sizeof(int));
    TEST_ASSERT_NOT_NULL(arr);
    TEST_ASSERT_EQUAL_INT(42, arr[0]);
    TEST_ASSERT_EQUAL_INT(55, arr[5]);
    cxf_free(arr);
}

void test_cxf_realloc_null_ptr(void) {
    /* realloc(NULL, size) should behave like malloc(size) */
    int *arr = (int *)cxf_realloc(NULL, 10 * sizeof(int));
    TEST_ASSERT_NOT_NULL(arr);
    cxf_free(arr);
}

void test_cxf_realloc_zero_size(void) {
    /* realloc(ptr, 0) behavior varies - we return NULL */
    int *arr = (int *)cxf_malloc(10 * sizeof(int));
    TEST_ASSERT_NOT_NULL(arr);

    void *result = cxf_realloc(arr, 0);
    TEST_ASSERT_NULL(result);
    /* Note: arr was freed by realloc when size=0 */
}

/*----------------------------------------------------------------------------*/
/* cxf_free tests                                                             */
/*----------------------------------------------------------------------------*/

void test_cxf_free_null_safe(void) {
    cxf_free(NULL);  /* Should not crash */
    TEST_PASS();
}

void test_cxf_free_after_malloc(void) {
    void *ptr = cxf_malloc(100);
    TEST_ASSERT_NOT_NULL(ptr);
    cxf_free(ptr);  /* Should not crash */
    TEST_PASS();
}

/*----------------------------------------------------------------------------*/
/* State cleanup tests (M2.1.4)                                               */
/*----------------------------------------------------------------------------*/

void test_free_solver_state_null_safe(void) {
    cxf_free_solver_state(NULL);  /* Should not crash */
    TEST_PASS();
}

void test_free_basis_state_null_safe(void) {
    cxf_free_basis_state(NULL);  /* Should not crash */
    TEST_PASS();
}

void test_free_callback_state_null_safe(void) {
    cxf_free_callback_state(NULL);  /* Should not crash */
    TEST_PASS();
}

/*----------------------------------------------------------------------------*/
/* Main test runner                                                           */
/*----------------------------------------------------------------------------*/

int main(void) {
    UNITY_BEGIN();

    /* malloc tests */
    RUN_TEST(test_cxf_malloc_basic);
    RUN_TEST(test_cxf_malloc_zero_size);
    RUN_TEST(test_cxf_malloc_large_size);

    /* calloc tests */
    RUN_TEST(test_cxf_calloc_zeroed);
    RUN_TEST(test_cxf_calloc_zero_count);
    RUN_TEST(test_cxf_calloc_zero_size);

    /* realloc tests */
    RUN_TEST(test_cxf_realloc_grow);
    RUN_TEST(test_cxf_realloc_shrink);
    RUN_TEST(test_cxf_realloc_null_ptr);
    RUN_TEST(test_cxf_realloc_zero_size);

    /* free tests */
    RUN_TEST(test_cxf_free_null_safe);
    RUN_TEST(test_cxf_free_after_malloc);

    /* State cleanup tests (M2.1.4) */
    RUN_TEST(test_free_solver_state_null_safe);
    RUN_TEST(test_free_basis_state_null_safe);
    RUN_TEST(test_free_callback_state_null_safe);

    return UNITY_END();
}
