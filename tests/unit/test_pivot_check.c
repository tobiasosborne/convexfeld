/**
 * @file test_pivot_check.c
 * @brief TDD tests for pivot validation functions (M3.1.7)
 *
 * Tests for pivot validation functions:
 * - cxf_validate_pivot_element
 * - cxf_special_check
 */

#include "unity.h"
#include "convexfeld/cxf_types.h"
#include <math.h>

/* Forward declarations for pivot functions */
int cxf_validate_pivot_element(double pivot_elem, double tolerance);
int cxf_special_check(double lb, double ub, uint32_t flags, double *work_accum);

void setUp(void) {
    /* Nothing to set up */
}

void tearDown(void) {
    /* Nothing to tear down */
}

/*============================================================================
 * cxf_validate_pivot_element Tests
 *===========================================================================*/

void test_pivot_check_valid_positive(void) {
    int result = cxf_validate_pivot_element(1.0, 1e-10);
    TEST_ASSERT_EQUAL_INT(1, result);
}

void test_pivot_check_valid_negative(void) {
    int result = cxf_validate_pivot_element(-1.0, 1e-10);
    TEST_ASSERT_EQUAL_INT(1, result);
}

void test_pivot_check_valid_small(void) {
    int result = cxf_validate_pivot_element(1e-8, 1e-10);
    TEST_ASSERT_EQUAL_INT(1, result);
}

void test_pivot_check_reject_too_small(void) {
    int result = cxf_validate_pivot_element(1e-12, 1e-10);
    TEST_ASSERT_EQUAL_INT(0, result);
}

void test_pivot_check_reject_zero(void) {
    int result = cxf_validate_pivot_element(0.0, 1e-10);
    TEST_ASSERT_EQUAL_INT(0, result);
}

void test_pivot_check_reject_nan(void) {
    int result = cxf_validate_pivot_element(NAN, 1e-10);
    TEST_ASSERT_EQUAL_INT(0, result);
}

void test_pivot_check_accept_infinity(void) {
    /* Infinity has large magnitude, so it passes */
    int result = cxf_validate_pivot_element(INFINITY, 1e-10);
    TEST_ASSERT_EQUAL_INT(1, result);
}

void test_pivot_check_boundary_exactly_equal(void) {
    /* Exactly at tolerance - accepted (>= tolerance) */
    int result = cxf_validate_pivot_element(1e-10, 1e-10);
    TEST_ASSERT_EQUAL_INT(1, result);
}

void test_pivot_check_boundary_just_above(void) {
    int result = cxf_validate_pivot_element(1.1e-10, 1e-10);
    TEST_ASSERT_EQUAL_INT(1, result);
}

/*============================================================================
 * cxf_special_check Tests
 *===========================================================================*/

void test_special_check_valid_basic(void) {
    /* Normal bounded variable with no special flags */
    int result = cxf_special_check(0.0, 10.0, 0, NULL);
    TEST_ASSERT_EQUAL_INT(1, result);
}

void test_special_check_reject_unbounded_lower(void) {
    /* Lower bound is negative infinity */
    int result = cxf_special_check(-1e100, 10.0, 0, NULL);
    TEST_ASSERT_EQUAL_INT(0, result);
}

void test_special_check_reject_reserved_flags(void) {
    /* Reserved flag bits set (bit 4 = 0x10 is reserved per mask 0xFFFFFFB0) */
    int result = cxf_special_check(0.0, 10.0, 0x00000010, NULL);
    TEST_ASSERT_EQUAL_INT(0, result);
}

void test_special_check_upper_finite_flag_valid(void) {
    /* Upper finite flag with valid bounds */
    int result = cxf_special_check(0.0, 10.0, 0x04, NULL);
    TEST_ASSERT_EQUAL_INT(1, result);
}

void test_special_check_reject_quadratic(void) {
    /* Quadratic flag set - not supported in LP-only */
    int result = cxf_special_check(0.0, 10.0, 0x08, NULL);
    TEST_ASSERT_EQUAL_INT(0, result);
}

void test_special_check_work_accum_null(void) {
    /* Should not crash with NULL work accumulator */
    int result = cxf_special_check(0.0, 10.0, 0, NULL);
    TEST_ASSERT_EQUAL_INT(1, result);
}

void test_special_check_negative_finite_bound(void) {
    /* Negative but finite lower bound */
    int result = cxf_special_check(-100.0, 10.0, 0, NULL);
    TEST_ASSERT_EQUAL_INT(1, result);
}

/*============================================================================
 * Main
 *===========================================================================*/

int main(void) {
    UNITY_BEGIN();

    /* cxf_validate_pivot_element tests */
    RUN_TEST(test_pivot_check_valid_positive);
    RUN_TEST(test_pivot_check_valid_negative);
    RUN_TEST(test_pivot_check_valid_small);
    RUN_TEST(test_pivot_check_reject_too_small);
    RUN_TEST(test_pivot_check_reject_zero);
    RUN_TEST(test_pivot_check_reject_nan);
    RUN_TEST(test_pivot_check_accept_infinity);
    RUN_TEST(test_pivot_check_boundary_exactly_equal);
    RUN_TEST(test_pivot_check_boundary_just_above);

    /* cxf_special_check tests */
    RUN_TEST(test_special_check_valid_basic);
    RUN_TEST(test_special_check_reject_unbounded_lower);
    RUN_TEST(test_special_check_reject_reserved_flags);
    RUN_TEST(test_special_check_upper_finite_flag_valid);
    RUN_TEST(test_special_check_reject_quadratic);
    RUN_TEST(test_special_check_work_accum_null);
    RUN_TEST(test_special_check_negative_finite_bound);

    return UNITY_END();
}
