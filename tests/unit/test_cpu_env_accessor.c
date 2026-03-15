/**
 * @file test_cpu_env_accessor.c
 * @brief V2 spec compliance tests for CPU environment accessors
 *
 * Per threading_sync.md V2 spec:
 * - cxf_get_logical_processors(CxfEnv*) returns cached value from env
 * - cxf_get_physical_cores(CxfEnv*) returns min(logical, physical)
 * - Both are pure accessors, detected at env init time
 *
 * Issues: convexfeld-f87d, convexfeld-2c8i
 */

#include "unity.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_env.h"

/* Functions under test (V2: take CxfEnv*) */
int cxf_get_logical_processors(CxfEnv *env);
int cxf_get_physical_cores(CxfEnv *env);

/* Internal detection helpers (for verifying cached values match) */
int cxf_detect_logical_processors(void);
int cxf_detect_physical_cores(void);

/* Environment lifecycle */
int cxf_loadenv(CxfEnv **envP, const char *logfilename);
int cxf_freeenv(CxfEnv *env);

static CxfEnv *env = NULL;

void setUp(void) { cxf_loadenv(&env, NULL); }
void tearDown(void) { if (env) { cxf_freeenv(env); env = NULL; } }

/*==========================================================================
 * cxf_get_logical_processors V2 accessor tests
 *=========================================================================*/

void test_logical_returns_cached_value(void) {
    /* Accessor must return env->logical_processors */
    int result = cxf_get_logical_processors(env);
    TEST_ASSERT_EQUAL_INT(env->logical_processors, result);
}

void test_logical_matches_detection(void) {
    /* Cached value should match fresh detection */
    int detected = cxf_detect_logical_processors();
    TEST_ASSERT_EQUAL_INT(detected, cxf_get_logical_processors(env));
}

void test_logical_null_env_returns_one(void) {
    /* Null safety: return 1 (minimum valid) */
    TEST_ASSERT_EQUAL_INT(1, cxf_get_logical_processors(NULL));
}

void test_logical_positive(void) {
    int count = cxf_get_logical_processors(env);
    TEST_ASSERT_GREATER_OR_EQUAL(1, count);
}

void test_logical_immutable_across_calls(void) {
    int first = cxf_get_logical_processors(env);
    int second = cxf_get_logical_processors(env);
    TEST_ASSERT_EQUAL_INT(first, second);
}

/*==========================================================================
 * cxf_get_physical_cores V2 accessor tests
 *=========================================================================*/

void test_physical_returns_min_of_logical_and_physical(void) {
    /* V2 spec: returns min(logical, physical) */
    int result = cxf_get_physical_cores(env);
    int expected = env->logical_processors;
    if (env->physical_cores < expected) {
        expected = env->physical_cores;
    }
    TEST_ASSERT_EQUAL_INT(expected, result);
}

void test_physical_not_more_than_logical(void) {
    /* Physical cores must never exceed logical processors */
    int physical = cxf_get_physical_cores(env);
    int logical = cxf_get_logical_processors(env);
    TEST_ASSERT_LESS_OR_EQUAL(logical, physical);
}

void test_physical_null_env_returns_one(void) {
    /* Null safety: return 1 (minimum valid) */
    TEST_ASSERT_EQUAL_INT(1, cxf_get_physical_cores(NULL));
}

void test_physical_positive(void) {
    int count = cxf_get_physical_cores(env);
    TEST_ASSERT_GREATER_OR_EQUAL(1, count);
}

void test_physical_immutable_across_calls(void) {
    int first = cxf_get_physical_cores(env);
    int second = cxf_get_physical_cores(env);
    TEST_ASSERT_EQUAL_INT(first, second);
}

/*==========================================================================
 * Env init caching tests
 *=========================================================================*/

void test_env_caches_logical_at_init(void) {
    /* env->logical_processors must be populated during cxf_loadenv */
    TEST_ASSERT_GREATER_OR_EQUAL(1, env->logical_processors);
}

void test_env_caches_physical_at_init(void) {
    /* env->physical_cores must be populated during cxf_loadenv */
    TEST_ASSERT_GREATER_OR_EQUAL(1, env->physical_cores);
}

void test_separate_envs_get_same_values(void) {
    /* Two independent environments should detect the same hardware */
    CxfEnv *env2 = NULL;
    cxf_loadenv(&env2, NULL);
    TEST_ASSERT_EQUAL_INT(
        cxf_get_logical_processors(env),
        cxf_get_logical_processors(env2));
    TEST_ASSERT_EQUAL_INT(
        cxf_get_physical_cores(env),
        cxf_get_physical_cores(env2));
    cxf_freeenv(env2);
}

/*==========================================================================
 * Main
 *=========================================================================*/

int main(void) {
    UNITY_BEGIN();

    /* Logical processor accessor */
    RUN_TEST(test_logical_returns_cached_value);
    RUN_TEST(test_logical_matches_detection);
    RUN_TEST(test_logical_null_env_returns_one);
    RUN_TEST(test_logical_positive);
    RUN_TEST(test_logical_immutable_across_calls);

    /* Physical cores accessor */
    RUN_TEST(test_physical_returns_min_of_logical_and_physical);
    RUN_TEST(test_physical_not_more_than_logical);
    RUN_TEST(test_physical_null_env_returns_one);
    RUN_TEST(test_physical_positive);
    RUN_TEST(test_physical_immutable_across_calls);

    /* Init caching */
    RUN_TEST(test_env_caches_logical_at_init);
    RUN_TEST(test_env_caches_physical_at_init);
    RUN_TEST(test_separate_envs_get_same_values);

    return UNITY_END();
}
