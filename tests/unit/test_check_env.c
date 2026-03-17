/**
 * @file test_check_env.c
 * @brief Tests for cxf_check_env (V2 input_validation.md).
 *
 * Validates:
 * - NULL env returns CXF_ERROR_NULL_ARGUMENT
 * - Valid env returns CXF_OK
 * - Corrupted sentinel returns CXF_ERROR_INVALID_ARGUMENT
 * - Freed env (sentinel cleared) returns CXF_ERROR_INVALID_ARGUMENT
 * - Child env with freed root returns CXF_ERROR_INVALID_ARGUMENT
 * - Child env with valid root returns CXF_OK
 */

#include "unity.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_types.h"
#include <string.h>

/* Forward declaration */
int cxf_check_env(CxfEnv *env);

void setUp(void) {}
void tearDown(void) {}

/*******************************************************************************
 * Basic Validation
 ******************************************************************************/

void test_null_env_returns_null_argument(void) {
    int rc = cxf_check_env(NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, rc);
}

void test_valid_env_returns_ok(void) {
    CxfEnv *env = NULL;
    cxf_loadenv(&env, NULL);
    TEST_ASSERT_NOT_NULL(env);

    int rc = cxf_check_env(env);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    cxf_freeenv(env);
}

void test_corrupted_sentinel_returns_invalid(void) {
    CxfEnv *env = NULL;
    cxf_loadenv(&env, NULL);
    TEST_ASSERT_NOT_NULL(env);

    /* Corrupt the sentinel */
    env->magic = 0xDEADBEEFU;
    int rc = cxf_check_env(env);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, rc);

    /* Restore magic so freeenv works */
    env->magic = CXF_ENV_MAGIC;
    cxf_freeenv(env);
}

/*******************************************************************************
 * Freed Environment Detection
 ******************************************************************************/

void test_zeroed_sentinel_returns_invalid(void) {
    /* Simulate a freed env by allocating raw memory and zeroing the magic.
     * We cannot use cxf_freeenv because that actually frees the memory.
     * Instead, manually create a struct with magic = 0. */
    CxfEnv *env = NULL;
    cxf_loadenv(&env, NULL);
    TEST_ASSERT_NOT_NULL(env);

    /* Simulate what cxf_freeenv does to magic */
    env->magic = 0;
    int rc = cxf_check_env(env);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, rc);

    /* Restore magic for cleanup */
    env->magic = CXF_ENV_MAGIC;
    cxf_freeenv(env);
}

/*******************************************************************************
 * Root Environment Validation
 ******************************************************************************/

void test_child_env_with_valid_root_returns_ok(void) {
    CxfEnv *root = NULL;
    CxfEnv *child = NULL;
    cxf_loadenv(&root, NULL);
    cxf_loadenv(&child, NULL);

    /* Set up parent-child relationship */
    child->master_env = root;

    int rc = cxf_check_env(child);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    /* Cleanup: detach before freeing */
    child->master_env = NULL;
    cxf_freeenv(child);
    cxf_freeenv(root);
}

void test_child_env_with_freed_root_returns_invalid(void) {
    CxfEnv *root = NULL;
    CxfEnv *child = NULL;
    cxf_loadenv(&root, NULL);
    cxf_loadenv(&child, NULL);

    /* Set up parent-child relationship */
    child->master_env = root;

    /* Simulate root being freed (magic cleared) */
    root->magic = 0;

    int rc = cxf_check_env(child);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, rc);

    /* Restore and cleanup */
    root->magic = CXF_ENV_MAGIC;
    child->master_env = NULL;
    cxf_freeenv(child);
    cxf_freeenv(root);
}

void test_root_env_with_null_master_returns_ok(void) {
    /* A root environment has master_env == NULL, which is valid */
    CxfEnv *env = NULL;
    cxf_loadenv(&env, NULL);
    TEST_ASSERT_NULL(env->master_env);

    int rc = cxf_check_env(env);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    cxf_freeenv(env);
}

void test_emptyenv_passes_check(void) {
    /* Even inactive envs should pass sentinel check */
    CxfEnv *env = NULL;
    cxf_emptyenv(&env, NULL);
    TEST_ASSERT_NOT_NULL(env);

    int rc = cxf_check_env(env);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    cxf_freeenv(env);
}

/*******************************************************************************
 * Runner
 ******************************************************************************/

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_null_env_returns_null_argument);
    RUN_TEST(test_valid_env_returns_ok);
    RUN_TEST(test_corrupted_sentinel_returns_invalid);
    RUN_TEST(test_zeroed_sentinel_returns_invalid);
    RUN_TEST(test_child_env_with_valid_root_returns_ok);
    RUN_TEST(test_child_env_with_freed_root_returns_invalid);
    RUN_TEST(test_root_env_with_null_master_returns_ok);
    RUN_TEST(test_emptyenv_passes_check);

    return UNITY_END();
}
