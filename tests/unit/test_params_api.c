/**
 * @file test_params_api.c
 * @brief Unit tests for Parameter API functions.
 *
 * M8.1.8: Parameter API Tests
 */

#include "unity.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_types.h"
#include <string.h>

static CxfEnv *env;

void setUp(void) {
    env = NULL;
    cxf_loadenv(&env, NULL);
}

void tearDown(void) {
    cxf_freeenv(env);
    env = NULL;
}

/*******************************************************************************
 * cxf_setintparam Tests
 ******************************************************************************/

void test_setintparam_null_env_returns_error(void) {
    int status = cxf_setintparam(NULL, "OutputFlag", 0);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);
}

void test_setintparam_null_paramname_returns_error(void) {
    int status = cxf_setintparam(env, NULL, 0);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);
}

void test_setintparam_unknown_parameter_returns_error(void) {
    int status = cxf_setintparam(env, "UnknownParam", 42);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, status);
}

void test_setintparam_output_flag_valid_values(void) {
    int status;

    /* Set to 0 */
    status = cxf_setintparam(env, "OutputFlag", 0);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(0, env->output_flag);

    /* Set to 1 */
    status = cxf_setintparam(env, "OutputFlag", 1);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(1, env->output_flag);
}

void test_setintparam_output_flag_invalid_values(void) {
    int status;

    /* Invalid: 2 */
    status = cxf_setintparam(env, "OutputFlag", 2);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, status);

    /* Invalid: -1 */
    status = cxf_setintparam(env, "OutputFlag", -1);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, status);
}

void test_setintparam_verbosity_valid_values(void) {
    int status;

    /* Set to 0 */
    status = cxf_setintparam(env, "Verbosity", 0);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(0, env->verbosity);

    /* Set to 1 */
    status = cxf_setintparam(env, "Verbosity", 1);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(1, env->verbosity);

    /* Set to 2 */
    status = cxf_setintparam(env, "Verbosity", 2);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(2, env->verbosity);
}

void test_setintparam_verbosity_invalid_values(void) {
    int status;

    /* Invalid: 3 */
    status = cxf_setintparam(env, "Verbosity", 3);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, status);

    /* Invalid: -1 */
    status = cxf_setintparam(env, "Verbosity", -1);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, status);
}

void test_setintparam_refactor_interval_valid_values(void) {
    int status;

    /* Minimum: 1 */
    status = cxf_setintparam(env, "RefactorInterval", 1);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(1, env->refactor_interval);

    /* Middle value: 500 */
    status = cxf_setintparam(env, "RefactorInterval", 500);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(500, env->refactor_interval);

    /* Maximum: 10000 */
    status = cxf_setintparam(env, "RefactorInterval", 10000);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(10000, env->refactor_interval);
}

void test_setintparam_refactor_interval_invalid_values(void) {
    int status;

    /* Too small: 0 */
    status = cxf_setintparam(env, "RefactorInterval", 0);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, status);

    /* Too large: 10001 */
    status = cxf_setintparam(env, "RefactorInterval", 10001);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, status);
}

void test_setintparam_max_eta_count_valid_values(void) {
    int status;

    /* Minimum: 10 */
    status = cxf_setintparam(env, "MaxEtaCount", 10);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(10, env->max_eta_count);

    /* Middle value: 500 */
    status = cxf_setintparam(env, "MaxEtaCount", 500);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(500, env->max_eta_count);

    /* Maximum: 1000 */
    status = cxf_setintparam(env, "MaxEtaCount", 1000);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(1000, env->max_eta_count);
}

void test_setintparam_max_eta_count_invalid_values(void) {
    int status;

    /* Too small: 9 */
    status = cxf_setintparam(env, "MaxEtaCount", 9);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, status);

    /* Too large: 1001 */
    status = cxf_setintparam(env, "MaxEtaCount", 1001);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, status);
}

/*******************************************************************************
 * cxf_getintparam Tests
 ******************************************************************************/

void test_getintparam_null_env_returns_error(void) {
    int value;
    int status = cxf_getintparam(NULL, "OutputFlag", &value);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);
}

void test_getintparam_null_paramname_returns_error(void) {
    int value;
    int status = cxf_getintparam(env, NULL, &value);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);
}

void test_getintparam_null_valuep_returns_error(void) {
    int status = cxf_getintparam(env, "OutputFlag", NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);
}

void test_getintparam_unknown_parameter_returns_error(void) {
    int value;
    int status = cxf_getintparam(env, "UnknownParam", &value);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, status);
}

void test_getintparam_output_flag_returns_default(void) {
    int value = -1;
    int status = cxf_getintparam(env, "OutputFlag", &value);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(1, value);
}

void test_getintparam_verbosity_returns_default(void) {
    int value = -1;
    int status = cxf_getintparam(env, "Verbosity", &value);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(1, value);
}

void test_getintparam_refactor_interval_returns_default(void) {
    int value = -1;
    int status = cxf_getintparam(env, "RefactorInterval", &value);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(50, value);  /* DEFAULT_REFACTOR_INTERVAL */
}

void test_getintparam_max_eta_count_returns_default(void) {
    int value = -1;
    int status = cxf_getintparam(env, "MaxEtaCount", &value);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(100, value);  /* DEFAULT_MAX_ETA_COUNT */
}

void test_getintparam_returns_set_value(void) {
    int status;
    int value;

    /* Set and get OutputFlag */
    status = cxf_setintparam(env, "OutputFlag", 0);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    status = cxf_getintparam(env, "OutputFlag", &value);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(0, value);

    /* Set and get Verbosity */
    status = cxf_setintparam(env, "Verbosity", 2);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    status = cxf_getintparam(env, "Verbosity", &value);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(2, value);

    /* Set and get RefactorInterval */
    status = cxf_setintparam(env, "RefactorInterval", 250);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    status = cxf_getintparam(env, "RefactorInterval", &value);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(250, value);

    /* Set and get MaxEtaCount */
    status = cxf_setintparam(env, "MaxEtaCount", 750);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    status = cxf_getintparam(env, "MaxEtaCount", &value);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(750, value);
}

/*******************************************************************************
 * Test Runner
 ******************************************************************************/

int main(void) {
    UNITY_BEGIN();

    /* cxf_setintparam tests */
    RUN_TEST(test_setintparam_null_env_returns_error);
    RUN_TEST(test_setintparam_null_paramname_returns_error);
    RUN_TEST(test_setintparam_unknown_parameter_returns_error);
    RUN_TEST(test_setintparam_output_flag_valid_values);
    RUN_TEST(test_setintparam_output_flag_invalid_values);
    RUN_TEST(test_setintparam_verbosity_valid_values);
    RUN_TEST(test_setintparam_verbosity_invalid_values);
    RUN_TEST(test_setintparam_refactor_interval_valid_values);
    RUN_TEST(test_setintparam_refactor_interval_invalid_values);
    RUN_TEST(test_setintparam_max_eta_count_valid_values);
    RUN_TEST(test_setintparam_max_eta_count_invalid_values);

    /* cxf_getintparam tests */
    RUN_TEST(test_getintparam_null_env_returns_error);
    RUN_TEST(test_getintparam_null_paramname_returns_error);
    RUN_TEST(test_getintparam_null_valuep_returns_error);
    RUN_TEST(test_getintparam_unknown_parameter_returns_error);
    RUN_TEST(test_getintparam_output_flag_returns_default);
    RUN_TEST(test_getintparam_verbosity_returns_default);
    RUN_TEST(test_getintparam_refactor_interval_returns_default);
    RUN_TEST(test_getintparam_max_eta_count_returns_default);
    RUN_TEST(test_getintparam_returns_set_value);

    return UNITY_END();
}
