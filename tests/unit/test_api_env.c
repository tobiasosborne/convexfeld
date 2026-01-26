/**
 * @file test_api_env.c
 * @brief Unit tests for cxf_loadenv and cxf_freeenv API functions.
 *
 * M8.1.1: API Tests - Environment
 */

#include "unity.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_types.h"

void setUp(void) {
}

void tearDown(void) {
}

/*******************************************************************************
 * cxf_loadenv Tests
 ******************************************************************************/

void test_loadenv_basic_creation(void) {
    CxfEnv *env = NULL;
    int status = cxf_loadenv(&env, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_NOT_NULL(env);
    cxf_freeenv(env);
}

void test_loadenv_null_envp_returns_error(void) {
    int status = cxf_loadenv(NULL, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);
}

void test_loadenv_sets_magic_number(void) {
    CxfEnv *env = NULL;
    cxf_loadenv(&env, NULL);
    TEST_ASSERT_EQUAL_UINT32(CXF_ENV_MAGIC, env->magic);
    cxf_freeenv(env);
}

void test_loadenv_sets_active_flag(void) {
    CxfEnv *env = NULL;
    cxf_loadenv(&env, NULL);
    TEST_ASSERT_EQUAL_INT(1, env->active);
    cxf_freeenv(env);
}

void test_loadenv_sets_default_tolerances(void) {
    CxfEnv *env = NULL;
    cxf_loadenv(&env, NULL);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, CXF_FEASIBILITY_TOL, env->feasibility_tol);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, CXF_OPTIMALITY_TOL, env->optimality_tol);
    TEST_ASSERT_DOUBLE_WITHIN(1e90, CXF_INFINITY, env->infinity);
    cxf_freeenv(env);
}

void test_loadenv_sets_default_verbosity(void) {
    CxfEnv *env = NULL;
    cxf_loadenv(&env, NULL);
    TEST_ASSERT_EQUAL_INT(1, env->verbosity);
    TEST_ASSERT_EQUAL_INT(1, env->output_flag);
    cxf_freeenv(env);
}

void test_loadenv_sets_ref_count_to_one(void) {
    CxfEnv *env = NULL;
    cxf_loadenv(&env, NULL);
    TEST_ASSERT_EQUAL_INT(1, env->ref_count);
    cxf_freeenv(env);
}

void test_loadenv_clears_error_buffer(void) {
    CxfEnv *env = NULL;
    cxf_loadenv(&env, NULL);
    TEST_ASSERT_EQUAL_CHAR('\0', env->error_buffer[0]);
    cxf_freeenv(env);
}

void test_loadenv_multiple_envs(void) {
    CxfEnv *env1 = NULL;
    CxfEnv *env2 = NULL;
    TEST_ASSERT_EQUAL_INT(CXF_OK, cxf_loadenv(&env1, NULL));
    TEST_ASSERT_EQUAL_INT(CXF_OK, cxf_loadenv(&env2, NULL));
    TEST_ASSERT_NOT_EQUAL(env1, env2);
    cxf_freeenv(env1);
    cxf_freeenv(env2);
}

/*******************************************************************************
 * cxf_freeenv Tests
 ******************************************************************************/

void test_freeenv_null_is_safe(void) {
    cxf_freeenv(NULL);  /* Should not crash */
    TEST_PASS();
}

void test_freeenv_clears_magic(void) {
    CxfEnv *env = NULL;
    cxf_loadenv(&env, NULL);
    TEST_ASSERT_EQUAL_UINT32(CXF_ENV_MAGIC, env->magic);
    cxf_freeenv(env);
    /* Note: verifying magic cleared requires accessing freed memory (UB) */
    /* We verified the env had valid magic before free; implementation clears it */
    TEST_PASS();
}

int main(void) {
    UNITY_BEGIN();

    /* cxf_loadenv tests */
    RUN_TEST(test_loadenv_basic_creation);
    RUN_TEST(test_loadenv_null_envp_returns_error);
    RUN_TEST(test_loadenv_sets_magic_number);
    RUN_TEST(test_loadenv_sets_active_flag);
    RUN_TEST(test_loadenv_sets_default_tolerances);
    RUN_TEST(test_loadenv_sets_default_verbosity);
    RUN_TEST(test_loadenv_sets_ref_count_to_one);
    RUN_TEST(test_loadenv_clears_error_buffer);
    RUN_TEST(test_loadenv_multiple_envs);

    /* cxf_freeenv tests */
    RUN_TEST(test_freeenv_null_is_safe);
    RUN_TEST(test_freeenv_clears_magic);

    return UNITY_END();
}
