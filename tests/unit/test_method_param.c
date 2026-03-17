/**
 * @file test_method_param.c
 * @brief Unit tests for Method parameter (V2 parameters_defaults.md S3).
 *
 * Validates: default value, set/get, range validation, dispatch behavior.
 */

#include "unity.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_types.h"

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
 * Default Value
 ******************************************************************************/

void test_method_default_is_auto(void) {
    int value = 999;
    int status = cxf_getintparam(env, "Method", &value);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(-1, value);
}

void test_method_struct_field_default(void) {
    TEST_ASSERT_EQUAL_INT(-1, env->method);
}

/*******************************************************************************
 * Set/Get Round-Trip
 ******************************************************************************/

void test_method_set_get_all_valid_values(void) {
    int value;
    int status;
    int valid[] = {-1, 0, 1, 2, 3, 4, 5};
    int i;

    for (i = 0; i < 7; i++) {
        status = cxf_setintparam(env, "Method", valid[i]);
        TEST_ASSERT_EQUAL_INT(CXF_OK, status);

        status = cxf_getintparam(env, "Method", &value);
        TEST_ASSERT_EQUAL_INT(CXF_OK, status);
        TEST_ASSERT_EQUAL_INT(valid[i], value);
    }
}

/*******************************************************************************
 * Range Validation
 ******************************************************************************/

void test_method_reject_below_range(void) {
    int status = cxf_setintparam(env, "Method", -2);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_VALUE_OUT_OF_RANGE, status);
    /* Value unchanged */
    TEST_ASSERT_EQUAL_INT(-1, env->method);
}

void test_method_reject_above_range(void) {
    int status = cxf_setintparam(env, "Method", 6);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_VALUE_OUT_OF_RANGE, status);
    TEST_ASSERT_EQUAL_INT(-1, env->method);
}

void test_method_reject_large_negative(void) {
    int status = cxf_setintparam(env, "Method", -100);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_VALUE_OUT_OF_RANGE, status);
}

void test_method_reject_large_positive(void) {
    int status = cxf_setintparam(env, "Method", 999);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_VALUE_OUT_OF_RANGE, status);
}

/*******************************************************************************
 * Case-Insensitive Lookup
 ******************************************************************************/

void test_method_case_insensitive(void) {
    int value;
    int status;

    status = cxf_setintparam(env, "method", 0);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);

    status = cxf_getintparam(env, "METHOD", &value);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(0, value);
}

/*******************************************************************************
 * Dispatch: unsupported methods return CXF_ERROR_NOT_SUPPORTED
 ******************************************************************************/

void test_method_unsupported_returns_not_supported(void) {
    CxfModel *model = NULL;
    int status;

    /* Create a minimal valid model */
    status = cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);

    /* Set method to dual simplex (not implemented) */
    status = cxf_setintparam(env, "Method", 2);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);

    /* Optimize should return NOT_SUPPORTED */
    status = cxf_optimize(model);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NOT_SUPPORTED, status);

    cxf_freemodel(model);
}

/*******************************************************************************
 * Test Runner
 ******************************************************************************/

int main(void) {
    UNITY_BEGIN();

    /* Default value */
    RUN_TEST(test_method_default_is_auto);
    RUN_TEST(test_method_struct_field_default);

    /* Set/Get round-trip */
    RUN_TEST(test_method_set_get_all_valid_values);

    /* Range validation */
    RUN_TEST(test_method_reject_below_range);
    RUN_TEST(test_method_reject_above_range);
    RUN_TEST(test_method_reject_large_negative);
    RUN_TEST(test_method_reject_large_positive);

    /* Case insensitive */
    RUN_TEST(test_method_case_insensitive);

    /* Dispatch */
    RUN_TEST(test_method_unsupported_returns_not_supported);

    return UNITY_END();
}
