/**
 * @file test_env_acquire_lock.c
 * @brief Unit tests for cxf_env_acquire_lock (threading_sync.md).
 *
 * Verifies: error buffer clear when unlocked, preservation when locked,
 * NULL safety, and error_code reset.
 */

#include "unity.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_types.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/*******************************************************************************
 * NULL safety
 ******************************************************************************/

void test_null_env_does_not_crash(void) {
    /* V2 spec: null environment -> silent return, no action */
    cxf_env_acquire_lock(NULL);
    TEST_PASS();
}

/*******************************************************************************
 * Clears error state when unlocked
 ******************************************************************************/

void test_clears_error_buffer_when_unlocked(void) {
    CxfEnv *env = NULL;
    cxf_loadenv(&env, NULL);

    /* Plant a stale error message */
    strcpy(env->error_buffer, "previous error");
    env->error_buf_locked = 0;

    cxf_env_acquire_lock(env);

    TEST_ASSERT_EQUAL_CHAR('\0', env->error_buffer[0]);
    cxf_freeenv(env);
}

void test_clears_error_code_when_unlocked(void) {
    CxfEnv *env = NULL;
    cxf_loadenv(&env, NULL);

    env->error_code = CXF_ERROR_INVALID_ARGUMENT;
    env->error_buf_locked = 0;

    cxf_env_acquire_lock(env);

    TEST_ASSERT_EQUAL_INT(0, env->error_code);
    cxf_freeenv(env);
}

void test_clears_both_fields_together(void) {
    CxfEnv *env = NULL;
    cxf_loadenv(&env, NULL);

    strcpy(env->error_buffer, "some error");
    env->error_code = CXF_ERROR_OUT_OF_MEMORY;
    env->error_buf_locked = 0;

    cxf_env_acquire_lock(env);

    TEST_ASSERT_EQUAL_INT(0, env->error_code);
    TEST_ASSERT_EQUAL_STRING("", env->error_buffer);
    cxf_freeenv(env);
}

/*******************************************************************************
 * Preserves error state when locked
 ******************************************************************************/

void test_preserves_error_buffer_when_locked(void) {
    CxfEnv *env = NULL;
    cxf_loadenv(&env, NULL);

    strcpy(env->error_buffer, "root cause error");
    env->error_code = CXF_ERROR_NUMERIC;
    env->error_buf_locked = 1;

    cxf_env_acquire_lock(env);

    TEST_ASSERT_EQUAL_STRING("root cause error", env->error_buffer);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NUMERIC, env->error_code);
    cxf_freeenv(env);
}

void test_preserves_error_code_when_locked(void) {
    CxfEnv *env = NULL;
    cxf_loadenv(&env, NULL);

    env->error_code = CXF_ERROR_NULL_ARGUMENT;
    env->error_buf_locked = 1;

    cxf_env_acquire_lock(env);

    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, env->error_code);
    cxf_freeenv(env);
}

/*******************************************************************************
 * Fresh environment (already clean)
 ******************************************************************************/

void test_fresh_env_stays_clean(void) {
    CxfEnv *env = NULL;
    cxf_loadenv(&env, NULL);

    /* Fresh env has no error state; acquire_lock is idempotent */
    cxf_env_acquire_lock(env);

    TEST_ASSERT_EQUAL_INT(0, env->error_code);
    TEST_ASSERT_EQUAL_CHAR('\0', env->error_buffer[0]);
    cxf_freeenv(env);
}

/*******************************************************************************
 * main
 ******************************************************************************/

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_null_env_does_not_crash);
    RUN_TEST(test_clears_error_buffer_when_unlocked);
    RUN_TEST(test_clears_error_code_when_unlocked);
    RUN_TEST(test_clears_both_fields_together);
    RUN_TEST(test_preserves_error_buffer_when_locked);
    RUN_TEST(test_preserves_error_code_when_locked);
    RUN_TEST(test_fresh_env_stays_clean);

    return UNITY_END();
}
