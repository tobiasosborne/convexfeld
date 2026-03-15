/**
 * @file test_error_zero_code.c
 * @brief Tests for zero error code handling per V2 spec (error_handling.md).
 *
 * Issue convexfeld-mrz6: cxf_error_env/cxf_error_model must early-return
 *   without modifying state when error_code is 0.
 * Issue convexfeld-tyy6: cxf_set_error_message/cxf_env_set_status must
 *   clear error buffer to empty string when error_code is 0.
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
 * convexfeld-mrz6: cxf_error_env zero error code
 *===========================================================================*/

void test_error_env_zero_code_no_state_change(void) {
    /* Set up known state */
    cxf_error_env(env, CXF_ERROR_INVALID_ARGUMENT, 1, "existing error");
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, env->error_code);
    TEST_ASSERT_EQUAL_STRING("existing error", cxf_geterrormsg(env));

    /* Call with error_code=0 -- must not touch anything */
    cxf_error_env(env, 0, 1, "should not appear");

    /* State unchanged */
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, env->error_code);
    TEST_ASSERT_EQUAL_STRING("existing error", cxf_geterrormsg(env));
}

void test_error_env_zero_code_empty_buffer(void) {
    /* Start with clean state */
    cxf_clearerrormsg(env);
    env->error_code = 0;

    /* Call with error_code=0 on empty buffer -- still no change */
    cxf_error_env(env, 0, 1, "should not appear");

    TEST_ASSERT_EQUAL_INT(0, env->error_code);
    TEST_ASSERT_EQUAL_STRING("", cxf_geterrormsg(env));
}

/*============================================================================
 * convexfeld-mrz6: cxf_error_model zero error code
 *===========================================================================*/

void test_error_model_zero_code_no_state_change(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_NOT_NULL(model);

    /* Set up known state */
    cxf_error_env(env, CXF_ERROR_NULL_ARGUMENT, 1, "model error");
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, env->error_code);

    /* Call with error_code=0 -- must not touch anything */
    cxf_error_model(model, 0, 1, "should not appear");

    /* State unchanged */
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, env->error_code);
    TEST_ASSERT_EQUAL_STRING("model error", cxf_geterrormsg(env));

    cxf_freemodel(model);
}

/*============================================================================
 * convexfeld-tyy6: cxf_set_error_message zero code clears buffer
 *===========================================================================*/

void test_set_error_message_zero_clears_buffer(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_NOT_NULL(model);

    /* Set an error first */
    cxf_error_env(env, CXF_ERROR_INVALID_ARGUMENT, 1, "some error");
    TEST_ASSERT_EQUAL_STRING("some error", cxf_geterrormsg(env));

    /* Clear with zero code */
    cxf_set_error_message(model, 0);

    /* Buffer should be empty */
    TEST_ASSERT_EQUAL_STRING("", cxf_geterrormsg(env));

    cxf_freemodel(model);
}

void test_set_error_message_zero_does_not_write_unknown(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_NOT_NULL(model);

    /* Start clean */
    cxf_clearerrormsg(env);

    /* Zero code must NOT write "Unknown error" */
    cxf_set_error_message(model, 0);
    TEST_ASSERT_EQUAL_STRING("", cxf_geterrormsg(env));

    cxf_freemodel(model);
}

/*============================================================================
 * convexfeld-tyy6: cxf_env_set_status zero code clears buffer
 *===========================================================================*/

void test_env_set_status_zero_clears_buffer(void) {
    /* Set an error first */
    cxf_error_env(env, CXF_ERROR_NOT_SUPPORTED, 1, "status error");
    TEST_ASSERT_EQUAL_STRING("status error", cxf_geterrormsg(env));

    /* Clear with zero code */
    cxf_env_set_status(env, 0);

    /* Buffer should be empty */
    TEST_ASSERT_EQUAL_STRING("", cxf_geterrormsg(env));
}

void test_env_set_status_zero_does_not_write_unknown(void) {
    /* Start clean */
    cxf_clearerrormsg(env);

    /* Zero code must NOT write "Unknown error" */
    cxf_env_set_status(env, 0);
    TEST_ASSERT_EQUAL_STRING("", cxf_geterrormsg(env));
}

/*============================================================================
 * Main
 *===========================================================================*/

int main(void) {
    UNITY_BEGIN();

    /* convexfeld-mrz6: zero error code early return */
    RUN_TEST(test_error_env_zero_code_no_state_change);
    RUN_TEST(test_error_env_zero_code_empty_buffer);
    RUN_TEST(test_error_model_zero_code_no_state_change);

    /* convexfeld-tyy6: zero code clears buffer */
    RUN_TEST(test_set_error_message_zero_clears_buffer);
    RUN_TEST(test_set_error_message_zero_does_not_write_unknown);
    RUN_TEST(test_env_set_status_zero_clears_buffer);
    RUN_TEST(test_env_set_status_zero_does_not_write_unknown);

    return UNITY_END();
}
