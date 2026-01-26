/**
 * @file test_validation.c
 * @brief TDD tests for validation functions (M2.3.1)
 *
 * Tests for validation module functions:
 * - cxf_validate_array - validate array for NaN values
 * - cxf_validate_vartypes - validate variable type characters
 */

#include "unity.h"
#include "convexfeld/cxf_types.h"
#include <math.h>

/* Forward declarations for validation functions */
int cxf_validate_array(CxfEnv *env, int count, const double *array);
int cxf_validate_vartypes(CxfModel *model);

/* Test fixtures */
void setUp(void) {}
void tearDown(void) {}

/*============================================================================
 * cxf_validate_array Tests
 *===========================================================================*/

void test_cxf_validate_array_valid(void) {
    /* Valid array with finite values should pass */
    double arr[] = {1.0, 2.5, -3.7, 0.0, 100.0};
    int result = cxf_validate_array(NULL, 5, arr);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);
}

void test_cxf_validate_array_null_array(void) {
    /* NULL array should return success (indicates defaults) */
    int result = cxf_validate_array(NULL, 10, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);
}

void test_cxf_validate_array_zero_count(void) {
    /* Zero count should return success */
    double arr[] = {1.0};
    int result = cxf_validate_array(NULL, 0, arr);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);
}

void test_cxf_validate_array_negative_count(void) {
    /* Negative count should return success (defensive) */
    double arr[] = {1.0};
    int result = cxf_validate_array(NULL, -5, arr);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);
}

void test_cxf_validate_array_nan(void) {
    /* Array with NaN should return error */
    double arr[] = {1.0, NAN, 2.0};
    int result = cxf_validate_array(NULL, 3, arr);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, result);
}

void test_cxf_validate_array_nan_first(void) {
    /* NaN at first position */
    double arr[] = {NAN, 1.0, 2.0};
    int result = cxf_validate_array(NULL, 3, arr);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, result);
}

void test_cxf_validate_array_nan_last(void) {
    /* NaN at last position */
    double arr[] = {1.0, 2.0, NAN};
    int result = cxf_validate_array(NULL, 3, arr);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, result);
}

void test_cxf_validate_array_all_nan(void) {
    /* All NaN values */
    double arr[] = {NAN, NAN, NAN};
    int result = cxf_validate_array(NULL, 3, arr);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, result);
}

void test_cxf_validate_array_inf(void) {
    /* Infinity should be allowed per spec */
    double arr[] = {1.0, INFINITY, -INFINITY};
    int result = cxf_validate_array(NULL, 3, arr);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);
}

void test_cxf_validate_array_single_element(void) {
    /* Single element valid */
    double arr[] = {42.0};
    int result = cxf_validate_array(NULL, 1, arr);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);
}

void test_cxf_validate_array_single_nan(void) {
    /* Single NaN element */
    double arr[] = {NAN};
    int result = cxf_validate_array(NULL, 1, arr);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, result);
}

/*============================================================================
 * cxf_validate_vartypes Tests
 *===========================================================================*/

void test_cxf_validate_vartypes_valid(void) {
    /* Valid vartypes: 'C', 'B', 'I', 'S', 'N' */
    /* This test will fail to link until implementation exists */
    /* Per TDD, we define expected behavior */
    TEST_IGNORE_MESSAGE("Requires CxfModel structure implementation");
}

void test_cxf_validate_vartypes_invalid(void) {
    /* Invalid vartype character should return error */
    TEST_IGNORE_MESSAGE("Requires CxfModel structure implementation");
}

void test_cxf_validate_vartypes_null_model(void) {
    /* NULL model should be handled safely */
    int result = cxf_validate_vartypes(NULL);
    /* Expected: NULL argument error or OK (if NULL model is valid) */
    TEST_ASSERT_TRUE(result == CXF_ERROR_NULL_ARGUMENT || result == CXF_OK);
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

    /* cxf_validate_vartypes tests */
    RUN_TEST(test_cxf_validate_vartypes_valid);
    RUN_TEST(test_cxf_validate_vartypes_invalid);
    RUN_TEST(test_cxf_validate_vartypes_null_model);

    return UNITY_END();
}
