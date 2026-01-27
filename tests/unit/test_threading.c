/**
 * @file test_threading.c
 * @brief TDD tests for threading functions (M3.3.1)
 */

#include "unity.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_env.h"

/* Forward declarations for threading functions */
int cxf_get_logical_processors(void);
int cxf_get_physical_cores(void);
int cxf_set_thread_count(CxfEnv *env, int thread_count);
int cxf_get_threads(CxfEnv *env);
void cxf_env_acquire_lock(CxfEnv *env);
void cxf_leave_critical_section(CxfEnv *env);
int cxf_generate_seed(void);

/* API functions */
int cxf_loadenv(CxfEnv **envP, const char *logfilename);
int cxf_freeenv(CxfEnv *env);

static CxfEnv *env = NULL;

void setUp(void) { cxf_loadenv(&env, NULL); }
void tearDown(void) { if (env) { cxf_freeenv(env); env = NULL; } }

/*============================================================================
 * cxf_get_logical_processors Tests
 *===========================================================================*/

void test_get_logical_processors_positive(void) {
    int count = cxf_get_logical_processors();
    TEST_ASSERT_GREATER_OR_EQUAL(1, count);
    TEST_ASSERT_LESS_OR_EQUAL(1024, count);
}

void test_get_logical_processors_consistent(void) {
    int first = cxf_get_logical_processors();
    int second = cxf_get_logical_processors();
    TEST_ASSERT_EQUAL_INT(first, second);
}

/*============================================================================
 * cxf_get_physical_cores Tests
 *===========================================================================*/

void test_get_physical_cores_positive(void) {
    int count = cxf_get_physical_cores();
    TEST_ASSERT_GREATER_OR_EQUAL(1, count);
}

void test_get_physical_cores_not_more_than_logical(void) {
    int physical = cxf_get_physical_cores();
    int logical = cxf_get_logical_processors();
    TEST_ASSERT_LESS_OR_EQUAL(logical, physical);
}

void test_get_physical_cores_consistent(void) {
    int first = cxf_get_physical_cores();
    int second = cxf_get_physical_cores();
    TEST_ASSERT_EQUAL_INT(first, second);
}

/*============================================================================
 * cxf_set_thread_count Tests
 *===========================================================================*/

void test_set_thread_count_success(void) {
    int result = cxf_set_thread_count(env, 1);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);
}

void test_set_thread_count_null_env(void) {
    int result = cxf_set_thread_count(NULL, 4);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, result);
}

void test_set_thread_count_invalid(void) {
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, cxf_set_thread_count(env, 0));
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, cxf_set_thread_count(env, -1));
}

void test_set_thread_count_caps_at_logical(void) {
    int logical = cxf_get_logical_processors();
    int result = cxf_set_thread_count(env, logical + 100);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);
}

/*============================================================================
 * cxf_get_threads Tests
 *===========================================================================*/

void test_get_threads_null_env_returns_zero(void) {
    TEST_ASSERT_EQUAL_INT(0, cxf_get_threads(NULL));
}

void test_get_threads_default(void) {
    int count = cxf_get_threads(env);
    TEST_ASSERT_GREATER_OR_EQUAL(0, count);
}

/*============================================================================
 * cxf_env_acquire_lock / cxf_leave_critical_section Tests
 *===========================================================================*/

void test_env_lock_null_safe(void) {
    cxf_env_acquire_lock(NULL);
    cxf_leave_critical_section(NULL);
    TEST_PASS();
}

void test_env_lock_acquire_release(void) {
    cxf_env_acquire_lock(env);
    cxf_leave_critical_section(env);
    TEST_PASS();
}

void test_env_lock_recursive(void) {
    cxf_env_acquire_lock(env);
    cxf_env_acquire_lock(env);
    cxf_leave_critical_section(env);
    cxf_leave_critical_section(env);
    TEST_PASS();
}

/*============================================================================
 * cxf_generate_seed Tests
 *===========================================================================*/

void test_generate_seed_non_negative(void) {
    int seed = cxf_generate_seed();
    TEST_ASSERT_GREATER_OR_EQUAL(0, seed);
}

void test_generate_seed_varies(void) {
    int seed1 = cxf_generate_seed();
    int seed2 = cxf_generate_seed();
    int seed3 = cxf_generate_seed();
    int all_same = (seed1 == seed2) && (seed2 == seed3);
    TEST_ASSERT_FALSE(all_same);
}

/*============================================================================
 * Main
 *===========================================================================*/

int main(void) {
    UNITY_BEGIN();

    /* cxf_get_logical_processors tests */
    RUN_TEST(test_get_logical_processors_positive);
    RUN_TEST(test_get_logical_processors_consistent);

    /* cxf_get_physical_cores tests */
    RUN_TEST(test_get_physical_cores_positive);
    RUN_TEST(test_get_physical_cores_not_more_than_logical);
    RUN_TEST(test_get_physical_cores_consistent);

    /* cxf_set_thread_count tests */
    RUN_TEST(test_set_thread_count_success);
    RUN_TEST(test_set_thread_count_null_env);
    RUN_TEST(test_set_thread_count_invalid);
    RUN_TEST(test_set_thread_count_caps_at_logical);

    /* cxf_get_threads tests */
    RUN_TEST(test_get_threads_null_env_returns_zero);
    RUN_TEST(test_get_threads_default);

    /* cxf_env_acquire_lock / cxf_leave_critical_section tests */
    RUN_TEST(test_env_lock_null_safe);
    RUN_TEST(test_env_lock_acquire_release);
    RUN_TEST(test_env_lock_recursive);

    /* cxf_generate_seed tests */
    RUN_TEST(test_generate_seed_non_negative);
    RUN_TEST(test_generate_seed_varies);

    return UNITY_END();
}
