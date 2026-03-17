/**
 * @file test_clear_pending.c
 * @brief Tests for cxf_clear_pending_buffer (state_cleanup_buffers.md)
 *
 * Verifies:
 * - NULL bufferPtr: no crash (no-op)
 * - NULL *bufferPtr: no crash (no-op)
 * - Allocated buffer: freed and pointer set to NULL
 * - Fresh model (no pending data): no crash
 * - Model with pending_buffer set: cleared by freemodel path
 */

#include "unity.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_types.h"
#include <stddef.h>

/* Forward declarations for internal functions */
void *cxf_malloc(size_t size);

static CxfEnv *env = NULL;

void setUp(void) {
    cxf_loadenv(&env, NULL);
}

void tearDown(void) {
    cxf_freeenv(env);
    env = NULL;
}

/**
 * NULL bufferPtr is a no-op (no crash).
 */
void test_clear_pending_null_bufferPtr(void) {
    cxf_clear_pending_buffer(env, NULL);
    /* If we get here without crashing, the test passes */
    TEST_PASS();
}

/**
 * NULL env is tolerated (no crash).
 */
void test_clear_pending_null_env(void) {
    void *buf = NULL;
    cxf_clear_pending_buffer(NULL, &buf);
    TEST_ASSERT_NULL(buf);
}

/**
 * *bufferPtr == NULL is a no-op (no crash, pointer stays NULL).
 */
void test_clear_pending_null_buffer(void) {
    void *buf = NULL;
    cxf_clear_pending_buffer(env, &buf);
    TEST_ASSERT_NULL(buf);
}

/**
 * Allocated buffer is freed and *bufferPtr is set to NULL.
 */
void test_clear_pending_frees_buffer(void) {
    void *buf = cxf_malloc(64);
    TEST_ASSERT_NOT_NULL(buf);

    cxf_clear_pending_buffer(env, &buf);
    TEST_ASSERT_NULL(buf);
}

/**
 * Fresh model has NULL pending_buffer; clear is safe.
 */
void test_clear_pending_fresh_model(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "fresh", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_NOT_NULL(model);
    TEST_ASSERT_NULL(model->pending_buffer);

    cxf_clear_pending_buffer(env, &model->pending_buffer);
    TEST_ASSERT_NULL(model->pending_buffer);

    cxf_freemodel(model);
}

/**
 * Model with an allocated pending_buffer: freemodel path exercises
 * cxf_clear_pending_buffer (buffer is freed, no leak, no double-free).
 */
void test_clear_pending_via_freemodel(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "pending", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_NOT_NULL(model);

    /* Simulate a pending modification by allocating a buffer */
    model->pending_buffer = cxf_malloc(128);
    TEST_ASSERT_NOT_NULL(model->pending_buffer);

    /* freemodel now uses cxf_clear_pending_buffer internally */
    cxf_freemodel(model);
    /* If we get here without crash/leak, the test passes */
    TEST_PASS();
}

/**
 * Double-clear is safe: clearing an already-cleared buffer is a no-op.
 */
void test_clear_pending_double_clear(void) {
    void *buf = cxf_malloc(32);
    TEST_ASSERT_NOT_NULL(buf);

    cxf_clear_pending_buffer(env, &buf);
    TEST_ASSERT_NULL(buf);

    /* Second clear must be a no-op */
    cxf_clear_pending_buffer(env, &buf);
    TEST_ASSERT_NULL(buf);
}

/*******************************************************************************
 * Main
 ******************************************************************************/

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_clear_pending_null_bufferPtr);
    RUN_TEST(test_clear_pending_null_env);
    RUN_TEST(test_clear_pending_null_buffer);
    RUN_TEST(test_clear_pending_frees_buffer);
    RUN_TEST(test_clear_pending_fresh_model);
    RUN_TEST(test_clear_pending_via_freemodel);
    RUN_TEST(test_clear_pending_double_clear);

    return UNITY_END();
}
