/**
 * @file test_pivot_check.c
 * @brief TDD tests for pivot validation functions (M3.1.7)
 *
 * Tests for pivot validation functions:
 * - cxf_validate_pivot_element
 * - cxf_special_check (V2 signature: state, varIdx)
 */

#include "unity.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_solver.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

/* Forward declarations */
int cxf_validate_pivot_element(double pivot_elem, double tolerance);
int cxf_special_check(SolverState *state, int varIdx);

/* Minimal SolverState helper for special_check tests */
static SolverState  g_state;
static double       g_lb[4];
static double       g_ub[4];
static uint32_t     g_flags[4];

void setUp(void) {
    memset(&g_state, 0, sizeof(g_state));
    g_state.work_lb   = g_lb;
    g_state.work_ub   = g_ub;
    g_state.var_flags = g_flags;
    g_state.num_vars  = 4;
    /* Default: bounded [0, 10], no flags */
    for (int i = 0; i < 4; i++) {
        g_lb[i]    = 0.0;
        g_ub[i]    = 10.0;
        g_flags[i] = 0;
    }
}

void tearDown(void) { /* nothing */ }

/*==========================================================================
 * cxf_validate_pivot_element Tests
 *=========================================================================*/

void test_pivot_check_valid_positive(void) {
    TEST_ASSERT_EQUAL_INT(1, cxf_validate_pivot_element(1.0, 1e-10));
}
void test_pivot_check_valid_negative(void) {
    TEST_ASSERT_EQUAL_INT(1, cxf_validate_pivot_element(-1.0, 1e-10));
}
void test_pivot_check_valid_small(void) {
    TEST_ASSERT_EQUAL_INT(1, cxf_validate_pivot_element(1e-8, 1e-10));
}
void test_pivot_check_reject_too_small(void) {
    TEST_ASSERT_EQUAL_INT(0, cxf_validate_pivot_element(1e-12, 1e-10));
}
void test_pivot_check_reject_zero(void) {
    TEST_ASSERT_EQUAL_INT(0, cxf_validate_pivot_element(0.0, 1e-10));
}
void test_pivot_check_reject_nan(void) {
    TEST_ASSERT_EQUAL_INT(0, cxf_validate_pivot_element(NAN, 1e-10));
}
void test_pivot_check_accept_infinity(void) {
    TEST_ASSERT_EQUAL_INT(1, cxf_validate_pivot_element(INFINITY, 1e-10));
}
void test_pivot_check_boundary_exactly_equal(void) {
    TEST_ASSERT_EQUAL_INT(1, cxf_validate_pivot_element(1e-10, 1e-10));
}
void test_pivot_check_boundary_just_above(void) {
    TEST_ASSERT_EQUAL_INT(1, cxf_validate_pivot_element(1.1e-10, 1e-10));
}

/*==========================================================================
 * cxf_special_check Tests (V2 signature)
 *=========================================================================*/

void test_special_check_valid_basic(void) {
    TEST_ASSERT_EQUAL_INT(1, cxf_special_check(&g_state, 0));
}
void test_special_check_reject_unbounded_lower(void) {
    g_lb[0] = -2e100;
    TEST_ASSERT_EQUAL_INT(0, cxf_special_check(&g_state, 0));
}
void test_special_check_reject_reserved_flags(void) {
    g_flags[0] = 0x00000010;
    TEST_ASSERT_EQUAL_INT(0, cxf_special_check(&g_state, 0));
}
void test_special_check_upper_finite_flag_valid(void) {
    g_flags[0] = 0x04;
    TEST_ASSERT_EQUAL_INT(1, cxf_special_check(&g_state, 0));
}
void test_special_check_quadratic_accepted_no_qmatrix(void) {
    /* V2 spec: quadratic flag without Q-matrix data => accept */
    g_flags[0] = 0x08;
    TEST_ASSERT_EQUAL_INT(1, cxf_special_check(&g_state, 0));
}
void test_special_check_null_state(void) {
    TEST_ASSERT_EQUAL_INT(0, cxf_special_check(NULL, 0));
}
void test_special_check_negative_finite_bound(void) {
    g_lb[0] = -100.0;
    TEST_ASSERT_EQUAL_INT(1, cxf_special_check(&g_state, 0));
}

/*==========================================================================
 * Main
 *=========================================================================*/

int main(void) {
    UNITY_BEGIN();
    /* cxf_validate_pivot_element */
    RUN_TEST(test_pivot_check_valid_positive);
    RUN_TEST(test_pivot_check_valid_negative);
    RUN_TEST(test_pivot_check_valid_small);
    RUN_TEST(test_pivot_check_reject_too_small);
    RUN_TEST(test_pivot_check_reject_zero);
    RUN_TEST(test_pivot_check_reject_nan);
    RUN_TEST(test_pivot_check_accept_infinity);
    RUN_TEST(test_pivot_check_boundary_exactly_equal);
    RUN_TEST(test_pivot_check_boundary_just_above);
    /* cxf_special_check (V2) */
    RUN_TEST(test_special_check_valid_basic);
    RUN_TEST(test_special_check_reject_unbounded_lower);
    RUN_TEST(test_special_check_reject_reserved_flags);
    RUN_TEST(test_special_check_upper_finite_flag_valid);
    RUN_TEST(test_special_check_quadratic_accepted_no_qmatrix);
    RUN_TEST(test_special_check_null_state);
    RUN_TEST(test_special_check_negative_finite_bound);
    return UNITY_END();
}
