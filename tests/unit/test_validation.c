/**
 * @file test_validation.c
 * @brief TDD tests for cxf_validate_array (M2.3.1)
 *
 * Tests for cxf_validate_array only. cxf_validate_vartypes tests are
 * in test_validate_vartypes.c (V2 spec signature).
 */

#include "unity.h"
#include "convexfeld/cxf_types.h"
#include <math.h>

/* Forward declaration */
int cxf_validate_array(CxfEnv *env, int count, const double *array);

/* Test fixtures */
void setUp(void) {}
void tearDown(void) {}

/*============================================================================
 * cxf_validate_array Tests
 *===========================================================================*/

void test_cxf_validate_array_valid(void) {
    double arr[] = {1.0, 2.5, -3.7, 0.0, 100.0};
    int result = cxf_validate_array(NULL, 5, arr);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);
}

void test_cxf_validate_array_null_array(void) {
    int result = cxf_validate_array(NULL, 10, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);
}

void test_cxf_validate_array_zero_count(void) {
    double arr[] = {1.0};
    int result = cxf_validate_array(NULL, 0, arr);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);
}

void test_cxf_validate_array_negative_count(void) {
    double arr[] = {1.0};
    int result = cxf_validate_array(NULL, -5, arr);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);
}

void test_cxf_validate_array_nan(void) {
    double arr[] = {1.0, NAN, 2.0};
    int result = cxf_validate_array(NULL, 3, arr);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, result);
}

void test_cxf_validate_array_nan_first(void) {
    double arr[] = {NAN, 1.0, 2.0};
    int result = cxf_validate_array(NULL, 3, arr);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, result);
}

void test_cxf_validate_array_nan_last(void) {
    double arr[] = {1.0, 2.0, NAN};
    int result = cxf_validate_array(NULL, 3, arr);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, result);
}

void test_cxf_validate_array_all_nan(void) {
    double arr[] = {NAN, NAN, NAN};
    int result = cxf_validate_array(NULL, 3, arr);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, result);
}

void test_cxf_validate_array_inf(void) {
    double arr[] = {1.0, INFINITY, -INFINITY};
    int result = cxf_validate_array(NULL, 3, arr);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);
}

void test_cxf_validate_array_single_element(void) {
    double arr[] = {42.0};
    int result = cxf_validate_array(NULL, 1, arr);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);
}

void test_cxf_validate_array_single_nan(void) {
    double arr[] = {NAN};
    int result = cxf_validate_array(NULL, 1, arr);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, result);
}

/*============================================================================
 * Main
 *===========================================================================*/

int main(void) {
    UNITY_BEGIN();

    /* cxf_validate_array tests */
    RUN_TEST(test_cxf_validate_array_valid);
    RUN_TEST(test_cxf_validate_array_null_array);
    RUN_TEST(test_cxf_validate_array_zero_count);
    RUN_TEST(test_cxf_validate_array_negative_count);
    RUN_TEST(test_cxf_validate_array_nan);
    RUN_TEST(test_cxf_validate_array_nan_first);
    RUN_TEST(test_cxf_validate_array_nan_last);
    RUN_TEST(test_cxf_validate_array_all_nan);
    RUN_TEST(test_cxf_validate_array_inf);
    RUN_TEST(test_cxf_validate_array_single_element);
    RUN_TEST(test_cxf_validate_array_single_nan);

    return UNITY_END();
}
