/**
 * @file test_get_threads.c
 * @brief Tests for cxf_get_threads V2 spec compliance (convexfeld-gx34)
 *
 * Verifies the thread count resolution chain per threading_sync.md:
 *   1. Auto-detect with cap at 32
 *   2. User Threads parameter reduces count
 *   3. License limit reduces count
 *   4. Always returns positive integer (>= 1)
 */

#include "unity.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_env.h"

/* Threading functions under test */
int cxf_get_threads(CxfEnv *env);
int cxf_get_logical_processors(CxfEnv *env);
int cxf_get_physical_cores(CxfEnv *env);

/* Environment lifecycle */
int cxf_loadenv(CxfEnv **envP, const char *logfilename);
int cxf_freeenv(CxfEnv *env);

/* Parameter API */
int cxf_setintparam(CxfEnv *env, const char *paramname, int newvalue);
int cxf_getintparam(CxfEnv *env, const char *paramname, int *valueP);

static CxfEnv *env = NULL;

void setUp(void) { cxf_loadenv(&env, NULL); }
void tearDown(void) { if (env) { cxf_freeenv(env); env = NULL; } }

/*============================================================================
 * Null safety
 *===========================================================================*/

void test_get_threads_null_returns_zero(void) {
    TEST_ASSERT_EQUAL_INT(0, cxf_get_threads(NULL));
}

/*============================================================================
 * Default (auto) behavior
 *===========================================================================*/

void test_get_threads_default_positive(void) {
    int count = cxf_get_threads(env);
    TEST_ASSERT_GREATER_THAN(0, count);
}

void test_get_threads_default_at_most_32(void) {
    int count = cxf_get_threads(env);
    TEST_ASSERT_LESS_OR_EQUAL(32, count);
}

void test_get_threads_default_at_most_logical(void) {
    int count = cxf_get_threads(env);
    int logical = cxf_get_logical_processors(env);
    TEST_ASSERT_LESS_OR_EQUAL(logical, count);
}

/*============================================================================
 * User Threads parameter
 *===========================================================================*/

void test_threads_param_default_is_zero(void) {
    int val = -1;
    int rc = cxf_getintparam(env, "Threads", &val);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_EQUAL_INT(0, val);
}

void test_threads_param_set_and_get(void) {
    int val = -1;
    TEST_ASSERT_EQUAL_INT(CXF_OK, cxf_setintparam(env, "Threads", 4));
    TEST_ASSERT_EQUAL_INT(CXF_OK, cxf_getintparam(env, "Threads", &val));
    TEST_ASSERT_EQUAL_INT(4, val);
}

void test_threads_param_caps_result(void) {
    /* Set Threads to 2 — result must be <= 2 */
    cxf_setintparam(env, "Threads", 2);
    int count = cxf_get_threads(env);
    TEST_ASSERT_LESS_OR_EQUAL(2, count);
    TEST_ASSERT_GREATER_THAN(0, count);
}

void test_threads_param_zero_means_auto(void) {
    /* Threads=0 => auto-detect, same as default */
    cxf_setintparam(env, "Threads", 0);
    int auto_count = cxf_get_threads(env);
    TEST_ASSERT_GREATER_THAN(0, auto_count);
}

void test_threads_param_large_no_increase(void) {
    /* Setting Threads to 999 should NOT increase beyond auto */
    int auto_count = cxf_get_threads(env);
    cxf_setintparam(env, "Threads", 999);
    int large_count = cxf_get_threads(env);
    TEST_ASSERT_EQUAL_INT(auto_count, large_count);
}

void test_threads_param_rejects_negative(void) {
    int rc = cxf_setintparam(env, "Threads", -1);
    TEST_ASSERT_NOT_EQUAL(CXF_OK, rc);
}

/*============================================================================
 * License thread limit
 *===========================================================================*/

void test_license_limit_caps_result(void) {
    env->license_thread_limit = 1;
    int count = cxf_get_threads(env);
    TEST_ASSERT_EQUAL_INT(1, count);
}

void test_license_limit_zero_means_unlimited(void) {
    int auto_count = cxf_get_threads(env);
    env->license_thread_limit = 0;
    int unlimited_count = cxf_get_threads(env);
    TEST_ASSERT_EQUAL_INT(auto_count, unlimited_count);
}

void test_license_overrides_threads_param(void) {
    /* Threads=8 but license=2 => result must be 2 */
    cxf_setintparam(env, "Threads", 8);
    env->license_thread_limit = 2;
    int count = cxf_get_threads(env);
    TEST_ASSERT_LESS_OR_EQUAL(2, count);
}

/*============================================================================
 * Combined constraints
 *===========================================================================*/

void test_most_restrictive_wins(void) {
    cxf_setintparam(env, "Threads", 4);
    env->license_thread_limit = 3;
    int count = cxf_get_threads(env);
    TEST_ASSERT_LESS_OR_EQUAL(3, count);
    TEST_ASSERT_GREATER_THAN(0, count);
}

void test_consistent_across_calls(void) {
    cxf_setintparam(env, "Threads", 2);
    int first = cxf_get_threads(env);
    int second = cxf_get_threads(env);
    TEST_ASSERT_EQUAL_INT(first, second);
}

/*============================================================================
 * Main
 *===========================================================================*/

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_get_threads_null_returns_zero);

    RUN_TEST(test_get_threads_default_positive);
    RUN_TEST(test_get_threads_default_at_most_32);
    RUN_TEST(test_get_threads_default_at_most_logical);

    RUN_TEST(test_threads_param_default_is_zero);
    RUN_TEST(test_threads_param_set_and_get);
    RUN_TEST(test_threads_param_caps_result);
    RUN_TEST(test_threads_param_zero_means_auto);
    RUN_TEST(test_threads_param_large_no_increase);
    RUN_TEST(test_threads_param_rejects_negative);

    RUN_TEST(test_license_limit_caps_result);
    RUN_TEST(test_license_limit_zero_means_unlimited);
    RUN_TEST(test_license_overrides_threads_param);

    RUN_TEST(test_most_restrictive_wins);
    RUN_TEST(test_consistent_across_calls);

    return UNITY_END();
}
