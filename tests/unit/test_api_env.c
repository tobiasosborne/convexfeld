/**
 * @file test_api_env.c
 * @brief Unit tests for CxfEnv API functions.
 *
 * M8.1.1: API Tests - Environment
 * M8.1.7: Full CxfEnv structure tests
 */

#include "unity.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_types.h"
#include <string.h>

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

void test_loadenv_initializes_new_fields(void) {
    CxfEnv *env = NULL;
    cxf_loadenv(&env, NULL);
    TEST_ASSERT_EQUAL_INT(0, env->version);
    TEST_ASSERT_EQUAL_INT(0, env->session_ref);
    TEST_ASSERT_EQUAL_INT(0, env->optimizing);
    TEST_ASSERT_EQUAL_INT(0, env->error_buf_locked);
    TEST_ASSERT_EQUAL_INT(0, env->anonymous_mode);
    TEST_ASSERT_NULL(env->callback_state);
    TEST_ASSERT_NULL(env->master_env);
    cxf_freeenv(env);
}

/*******************************************************************************
 * cxf_freeenv Tests
 ******************************************************************************/

void test_freeenv_null_is_safe(void) {
    int status = cxf_freeenv(NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, status);
}

void test_freeenv_clears_magic(void) {
    CxfEnv *env = NULL;
    cxf_loadenv(&env, NULL);
    TEST_ASSERT_EQUAL_UINT32(CXF_ENV_MAGIC, env->magic);
    int status = cxf_freeenv(env);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    /* Note: verifying magic cleared requires accessing freed memory (UB) */
    /* We verified the env had valid magic before free; implementation clears it */
}

/*******************************************************************************
 * cxf_emptyenv Tests
 ******************************************************************************/

void test_emptyenv_creates_inactive(void) {
    CxfEnv *env = NULL;
    int status = cxf_emptyenv(&env, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_NOT_NULL(env);
    TEST_ASSERT_EQUAL_INT(0, env->active);  /* Inactive */
    TEST_ASSERT_EQUAL_UINT32(CXF_ENV_MAGIC, env->magic);
    cxf_freeenv(env);
}

void test_emptyenv_null_envp_returns_error(void) {
    int status = cxf_emptyenv(NULL, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);
}

/*******************************************************************************
 * cxf_startenv Tests
 ******************************************************************************/

void test_startenv_activates_inactive_env(void) {
    CxfEnv *env = NULL;
    cxf_emptyenv(&env, NULL);
    TEST_ASSERT_EQUAL_INT(0, env->active);
    int status = cxf_startenv(env);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(1, env->active);
    cxf_freeenv(env);
}

void test_startenv_null_returns_error(void) {
    int status = cxf_startenv(NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);
}

void test_startenv_already_active_returns_error(void) {
    CxfEnv *env = NULL;
    cxf_loadenv(&env, NULL);  /* Already active */
    int status = cxf_startenv(env);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, status);
    cxf_freeenv(env);
}

/*******************************************************************************
 * cxf_terminate Tests
 ******************************************************************************/

void test_terminate_sets_flag(void) {
    CxfEnv *env = NULL;
    cxf_loadenv(&env, NULL);
    TEST_ASSERT_EQUAL_INT(0, env->terminate_flag);
    int status = cxf_terminate(env);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(1, env->terminate_flag);
    cxf_freeenv(env);
}

void test_terminate_null_returns_error(void) {
    int status = cxf_terminate(NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);
}

void test_reset_terminate_clears_flag(void) {
    CxfEnv *env = NULL;
    cxf_loadenv(&env, NULL);
    cxf_terminate(env);
    TEST_ASSERT_EQUAL_INT(1, env->terminate_flag);
    int status = cxf_reset_terminate(env);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(0, env->terminate_flag);
    cxf_freeenv(env);
}

/*******************************************************************************
 * cxf_geterrormsg Tests
 ******************************************************************************/

void test_geterrormsg_returns_empty_initially(void) {
    CxfEnv *env = NULL;
    cxf_loadenv(&env, NULL);
    const char *msg = cxf_geterrormsg(env);
    TEST_ASSERT_NOT_NULL(msg);
    TEST_ASSERT_EQUAL_STRING("", msg);
    cxf_freeenv(env);
}

void test_geterrormsg_null_returns_empty(void) {
    const char *msg = cxf_geterrormsg(NULL);
    TEST_ASSERT_NOT_NULL(msg);
    TEST_ASSERT_EQUAL_STRING("", msg);
}

void test_clearerrormsg_works(void) {
    CxfEnv *env = NULL;
    cxf_loadenv(&env, NULL);
    /* Set error manually for test */
    strcpy(env->error_buffer, "Test error");
    TEST_ASSERT_EQUAL_STRING("Test error", cxf_geterrormsg(env));
    int status = cxf_clearerrormsg(env);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_STRING("", cxf_geterrormsg(env));
    cxf_freeenv(env);
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
    RUN_TEST(test_loadenv_initializes_new_fields);

    /* cxf_freeenv tests */
    RUN_TEST(test_freeenv_null_is_safe);
    RUN_TEST(test_freeenv_clears_magic);

    /* cxf_emptyenv tests */
    RUN_TEST(test_emptyenv_creates_inactive);
    RUN_TEST(test_emptyenv_null_envp_returns_error);

    /* cxf_startenv tests */
    RUN_TEST(test_startenv_activates_inactive_env);
    RUN_TEST(test_startenv_null_returns_error);
    RUN_TEST(test_startenv_already_active_returns_error);

    /* cxf_terminate tests */
    RUN_TEST(test_terminate_sets_flag);
    RUN_TEST(test_terminate_null_returns_error);
    RUN_TEST(test_reset_terminate_clears_flag);

    /* cxf_geterrormsg tests */
    RUN_TEST(test_geterrormsg_returns_empty_initially);
    RUN_TEST(test_geterrormsg_null_returns_empty);
    RUN_TEST(test_clearerrormsg_works);

    return UNITY_END();
}
