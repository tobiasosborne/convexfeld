/**
 * @file test_error_reporting.c
 * @brief Tests for error reporting model (P3.09): cxf_error_env,
 *        cxf_error_model, cxf_set_error_message, cxf_env_set_status,
 *        cxf_error_message, CXF_IS_ERROR macro, status codes (M3.1.1)
 *
 * Split from test_error.c. Tests MUST be written BEFORE implementation (TDD).
 */

#include "unity.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"

/* Forward declarations for functions under test */
extern void cxf_error_env(CxfEnv *env, int error_code, int overwrite,
                          const char *format, ...);
extern void cxf_error_model(CxfModel *model, int error_code, int overwrite,
                            const char *format, ...);
extern void cxf_set_error_message(CxfModel *model, int error_code);
extern void cxf_env_set_status(CxfEnv *env, int error_code);
extern const char *cxf_error_message(int error_code);

/* Test fixtures */
static CxfEnv *env = NULL;

void setUp(void) {
    cxf_loadenv(&env, NULL);
}

void tearDown(void) {
    cxf_freeenv(env);
    env = NULL;
}

/*============================================================================
 * Error Reporting Model Tests (P3.09)
 *===========================================================================*/

void test_error_message_lookup(void) {
    TEST_ASSERT_EQUAL_STRING("Out of memory", cxf_error_message(10001));
    TEST_ASSERT_EQUAL_STRING("NULL argument", cxf_error_message(10002));
    TEST_ASSERT_EQUAL_STRING("Feature not supported", cxf_error_message(10024));
    TEST_ASSERT_EQUAL_STRING("Internal error", cxf_error_message(20003));
    TEST_ASSERT_EQUAL_STRING("Unknown error", cxf_error_message(99999));
}

void test_error_env_writes_code_and_message(void) {
    cxf_clearerrormsg(env);
    cxf_error_env(env, CXF_ERROR_INVALID_ARGUMENT, 1, "bad value: %d", 42);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, env->error_code);
    TEST_ASSERT_EQUAL_STRING("bad value: 42", cxf_geterrormsg(env));
}

void test_error_env_empty_buffer_only(void) {
    cxf_clearerrormsg(env);
    cxf_error_env(env, CXF_ERROR_NULL_ARGUMENT, 0, "first");
    cxf_error_env(env, CXF_ERROR_INVALID_ARGUMENT, 0, "second");
    /* Message should still be "first" (no overwrite) */
    TEST_ASSERT_EQUAL_STRING("first", cxf_geterrormsg(env));
    /* But error_code is always updated */
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, env->error_code);
}

void test_error_env_overwrite(void) {
    cxf_clearerrormsg(env);
    cxf_error_env(env, CXF_ERROR_NULL_ARGUMENT, 0, "first");
    cxf_error_env(env, CXF_ERROR_INVALID_ARGUMENT, 1, "second");
    TEST_ASSERT_EQUAL_STRING("second", cxf_geterrormsg(env));
}

void test_error_env_oom_always_overwrites(void) {
    cxf_clearerrormsg(env);
    cxf_error_env(env, CXF_ERROR_NULL_ARGUMENT, 0, "existing");
    cxf_error_env(env, CXF_ERROR_OUT_OF_MEMORY, 0, "OOM!");
    /* OOM always overwrites even with overwrite=0 */
    TEST_ASSERT_EQUAL_STRING("OOM!", cxf_geterrormsg(env));
}

void test_error_env_null_safe(void) {
    cxf_error_env(NULL, CXF_ERROR_NULL_ARGUMENT, 1, "crash?");
    TEST_PASS();
}

void test_env_set_status_predefined(void) {
    cxf_clearerrormsg(env);
    cxf_env_set_status(env, CXF_ERROR_NOT_SUPPORTED);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NOT_SUPPORTED, env->error_code);
    TEST_ASSERT_EQUAL_STRING("Feature not supported", cxf_geterrormsg(env));
}

void test_error_model_writes_to_env(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_NOT_NULL(model);
    cxf_clearerrormsg(env);
    cxf_error_model(model, CXF_ERROR_INVALID_ARGUMENT, 1, "model err");
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, env->error_code);
    TEST_ASSERT_EQUAL_STRING("model err", cxf_geterrormsg(env));
    cxf_freemodel(model);
}

void test_set_error_message_predefined(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_NOT_NULL(model);
    cxf_clearerrormsg(env);
    cxf_set_error_message(model, CXF_ERROR_OUT_OF_MEMORY);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_OUT_OF_MEMORY, env->error_code);
    TEST_ASSERT_EQUAL_STRING("Out of memory", cxf_geterrormsg(env));
    cxf_freemodel(model);
}

void test_is_error_macro(void) {
    TEST_ASSERT_TRUE(CXF_IS_ERROR(CXF_ERROR_OUT_OF_MEMORY));
    TEST_ASSERT_TRUE(CXF_IS_ERROR(CXF_ERROR_INTERNAL));
    TEST_ASSERT_FALSE(CXF_IS_ERROR(CXF_OK));
    TEST_ASSERT_FALSE(CXF_IS_ERROR(CXF_OPTIMAL));
    TEST_ASSERT_FALSE(CXF_IS_ERROR(CXF_INFEASIBLE));
}

void test_status_code_values(void) {
    TEST_ASSERT_EQUAL_INT(0, CXF_OK);
    TEST_ASSERT_EQUAL_INT(2, CXF_OPTIMAL);
    TEST_ASSERT_EQUAL_INT(3, CXF_INFEASIBLE);
    TEST_ASSERT_EQUAL_INT(5, CXF_UNBOUNDED);
    TEST_ASSERT_EQUAL_INT(7, CXF_ITERATION_LIMIT);
    TEST_ASSERT_EQUAL_INT(9, CXF_TIME_LIMIT);
    TEST_ASSERT_EQUAL_INT(12, CXF_NUMERIC);
    TEST_ASSERT_EQUAL_INT(10001, CXF_ERROR_OUT_OF_MEMORY);
    TEST_ASSERT_EQUAL_INT(10002, CXF_ERROR_NULL_ARGUMENT);
    TEST_ASSERT_EQUAL_INT(10003, CXF_ERROR_INVALID_ARGUMENT);
    TEST_ASSERT_EQUAL_INT(10024, CXF_ERROR_NOT_SUPPORTED);
}

/*============================================================================
 * Main
 *===========================================================================*/

int main(void) {
    UNITY_BEGIN();

    /* Error reporting model (P3.09) */
    RUN_TEST(test_error_message_lookup);
    RUN_TEST(test_error_env_writes_code_and_message);
    RUN_TEST(test_error_env_empty_buffer_only);
    RUN_TEST(test_error_env_overwrite);
    RUN_TEST(test_error_env_oom_always_overwrites);
    RUN_TEST(test_error_env_null_safe);
    RUN_TEST(test_env_set_status_predefined);
    RUN_TEST(test_error_model_writes_to_env);
    RUN_TEST(test_set_error_message_predefined);
    RUN_TEST(test_is_error_macro);
    RUN_TEST(test_status_code_values);

    return UNITY_END();
}
