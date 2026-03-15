/**
 * @file test_nan_check_v2.c
 * @brief V2 spec compliance tests for single-value NaN/finite checks.
 *
 * Tests cxf_check_nan, cxf_check_is_finite, cxf_check_nan_or_inf
 * per input_validation.md V2 spec.
 *
 * Each function takes a single double and returns int (1=true, 0=false).
 */

#include "unity.h"
#include <math.h>
#include <float.h>

/* Forward declarations — V2 spec single-value functions */
int cxf_check_nan(double val);
int cxf_check_is_finite(double val);
int cxf_check_nan_or_inf(double val);

void setUp(void) {}
void tearDown(void) {}

/*============================================================================
 * cxf_check_nan — returns 1 if NaN, 0 otherwise
 *===========================================================================*/

void test_check_nan_normal(void) {
    TEST_ASSERT_EQUAL_INT(0, cxf_check_nan(1.0));
    TEST_ASSERT_EQUAL_INT(0, cxf_check_nan(-42.5));
    TEST_ASSERT_EQUAL_INT(0, cxf_check_nan(0.0));
}

void test_check_nan_detects_nan(void) {
    TEST_ASSERT_EQUAL_INT(1, cxf_check_nan(NAN));
    TEST_ASSERT_EQUAL_INT(1, cxf_check_nan(-NAN));
    TEST_ASSERT_EQUAL_INT(1, cxf_check_nan(0.0 / 0.0));
}

void test_check_nan_rejects_inf(void) {
    /* Per spec: cxf_check_nan rejects only NaN, not infinity */
    TEST_ASSERT_EQUAL_INT(0, cxf_check_nan(INFINITY));
    TEST_ASSERT_EQUAL_INT(0, cxf_check_nan(-INFINITY));
}

void test_check_nan_extremes(void) {
    TEST_ASSERT_EQUAL_INT(0, cxf_check_nan(DBL_MAX));
    TEST_ASSERT_EQUAL_INT(0, cxf_check_nan(-DBL_MAX));
    TEST_ASSERT_EQUAL_INT(0, cxf_check_nan(DBL_MIN));
    TEST_ASSERT_EQUAL_INT(0, cxf_check_nan(DBL_EPSILON));
}

/*============================================================================
 * cxf_check_is_finite — returns 1 if finite, 0 otherwise
 *===========================================================================*/

void test_is_finite_normal(void) {
    TEST_ASSERT_EQUAL_INT(1, cxf_check_is_finite(1.0));
    TEST_ASSERT_EQUAL_INT(1, cxf_check_is_finite(-42.5));
    TEST_ASSERT_EQUAL_INT(1, cxf_check_is_finite(0.0));
}

void test_is_finite_rejects_nan(void) {
    TEST_ASSERT_EQUAL_INT(0, cxf_check_is_finite(NAN));
    TEST_ASSERT_EQUAL_INT(0, cxf_check_is_finite(-NAN));
}

void test_is_finite_rejects_inf(void) {
    TEST_ASSERT_EQUAL_INT(0, cxf_check_is_finite(INFINITY));
    TEST_ASSERT_EQUAL_INT(0, cxf_check_is_finite(-INFINITY));
}

void test_is_finite_extremes(void) {
    TEST_ASSERT_EQUAL_INT(1, cxf_check_is_finite(DBL_MAX));
    TEST_ASSERT_EQUAL_INT(1, cxf_check_is_finite(-DBL_MAX));
    TEST_ASSERT_EQUAL_INT(1, cxf_check_is_finite(DBL_MIN));
    TEST_ASSERT_EQUAL_INT(1, cxf_check_is_finite(DBL_EPSILON));
}

/*============================================================================
 * cxf_check_nan_or_inf — alias for cxf_check_is_finite per V2 spec
 *
 * Per input_validation.md: "despite the name suggesting it returns true
 * for NaN/Inf, the function actually returns true for finite values."
 *===========================================================================*/

void test_nan_or_inf_is_alias(void) {
    /* Must behave identically to cxf_check_is_finite */
    TEST_ASSERT_EQUAL_INT(1, cxf_check_nan_or_inf(1.0));
    TEST_ASSERT_EQUAL_INT(1, cxf_check_nan_or_inf(0.0));
    TEST_ASSERT_EQUAL_INT(0, cxf_check_nan_or_inf(NAN));
    TEST_ASSERT_EQUAL_INT(0, cxf_check_nan_or_inf(INFINITY));
    TEST_ASSERT_EQUAL_INT(0, cxf_check_nan_or_inf(-INFINITY));
}

void test_nan_or_inf_extremes(void) {
    TEST_ASSERT_EQUAL_INT(1, cxf_check_nan_or_inf(DBL_MAX));
    TEST_ASSERT_EQUAL_INT(1, cxf_check_nan_or_inf(-DBL_MAX));
}

/*============================================================================
 * Main
 *===========================================================================*/

int main(void) {
    UNITY_BEGIN();

    /* cxf_check_nan */
    RUN_TEST(test_check_nan_normal);
    RUN_TEST(test_check_nan_detects_nan);
    RUN_TEST(test_check_nan_rejects_inf);
    RUN_TEST(test_check_nan_extremes);

    /* cxf_check_is_finite */
    RUN_TEST(test_is_finite_normal);
    RUN_TEST(test_is_finite_rejects_nan);
    RUN_TEST(test_is_finite_rejects_inf);
    RUN_TEST(test_is_finite_extremes);

    /* cxf_check_nan_or_inf (alias) */
    RUN_TEST(test_nan_or_inf_is_alias);
    RUN_TEST(test_nan_or_inf_extremes);

    return UNITY_END();
}
