/**
 * @file test_special_check_v2.c
 * @brief V2 spec compliance tests for cxf_special_check
 *
 * Validates the V2 signature (SolverState *state, int varIdx) and
 * multi-stage qualification logic per data_validation.md.
 */

#include "unity.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_solver.h"
#include <string.h>

/* Forward declaration */
int cxf_special_check(SolverState *state, int varIdx);

/* Flag bit constants (must match pivot_check.c) */
#define VARFLAG_UPPER_FINITE   0x04
#define VARFLAG_HAS_QUADRATIC  0x08

#define NUM_VARS 8
static SolverState g_state;
static double      g_lb[NUM_VARS];
static double      g_ub[NUM_VARS];
static uint32_t    g_flags[NUM_VARS];

void setUp(void) {
    memset(&g_state, 0, sizeof(g_state));
    g_state.work_lb   = g_lb;
    g_state.work_ub   = g_ub;
    g_state.var_flags = g_flags;
    g_state.num_vars  = NUM_VARS;
    for (int i = 0; i < NUM_VARS; i++) {
        g_lb[i]    = 0.0;
        g_ub[i]    = 100.0;
        g_flags[i] = 0;
    }
}

void tearDown(void) { /* nothing */ }

/*==========================================================================
 * Stage 1 — Lower bound check
 *=========================================================================*/

void test_v2_stage1_exactly_at_neg_inf(void) {
    /* lb exactly at -CXF_INFINITY is NOT below threshold => pass */
    g_lb[0] = -CXF_INFINITY;
    TEST_ASSERT_EQUAL_INT(1, cxf_special_check(&g_state, 0));
}

void test_v2_stage1_below_neg_inf(void) {
    /* lb < -CXF_INFINITY => reject */
    g_lb[1] = -1.1e100;
    TEST_ASSERT_EQUAL_INT(0, cxf_special_check(&g_state, 1));
}

void test_v2_stage1_finite_negative(void) {
    g_lb[2] = -500.0;
    TEST_ASSERT_EQUAL_INT(1, cxf_special_check(&g_state, 2));
}

/*==========================================================================
 * Stage 2 — Reserved flag bits
 *=========================================================================*/

void test_v2_stage2_no_flags(void) {
    g_flags[0] = 0;
    TEST_ASSERT_EQUAL_INT(1, cxf_special_check(&g_state, 0));
}

void test_v2_stage2_reserved_bit4(void) {
    /* 0x10 (bit 4) is in the reserved mask 0xFFFFFFB0 => reject */
    g_flags[0] = 0x10;
    TEST_ASSERT_EQUAL_INT(0, cxf_special_check(&g_state, 0));
}

void test_v2_stage2_reserved_high_bit(void) {
    g_flags[0] = 0x80000000; /* high bit */
    TEST_ASSERT_EQUAL_INT(0, cxf_special_check(&g_state, 0));
}

/*==========================================================================
 * Stage 3 — Finite UB flag + LB consistency
 *=========================================================================*/

void test_v2_stage3_ub_flag_normal(void) {
    g_flags[0] = VARFLAG_UPPER_FINITE;
    g_lb[0]    = 5.0;
    TEST_ASSERT_EQUAL_INT(1, cxf_special_check(&g_state, 0));
}

void test_v2_stage3_ub_flag_lb_above_pos_inf(void) {
    g_flags[0] = VARFLAG_UPPER_FINITE;
    g_lb[0]    = 1.1e100; /* above +CXF_INFINITY */
    TEST_ASSERT_EQUAL_INT(0, cxf_special_check(&g_state, 0));
}

void test_v2_stage3_no_ub_flag_lb_large(void) {
    /* No VARFLAG_UPPER_FINITE => Stage 3 is skipped, LB large is fine */
    g_flags[0] = 0;
    g_lb[0]    = 1.1e100;
    TEST_ASSERT_EQUAL_INT(1, cxf_special_check(&g_state, 0));
}

/*==========================================================================
 * Stage 4 — Quadratic (LP-only: always reject)
 *=========================================================================*/

void test_v2_stage4_quadratic_rejected(void) {
    g_flags[0] = VARFLAG_HAS_QUADRATIC;
    TEST_ASSERT_EQUAL_INT(0, cxf_special_check(&g_state, 0));
}

/*==========================================================================
 * Edge cases — NULL var_flags, NULL state, multiple vars
 *=========================================================================*/

void test_v2_null_state(void) {
    TEST_ASSERT_EQUAL_INT(0, cxf_special_check(NULL, 0));
}

void test_v2_null_var_flags_treated_as_zero(void) {
    g_state.var_flags = NULL;
    /* flags default to 0 => all stages pass for bounded var */
    TEST_ASSERT_EQUAL_INT(1, cxf_special_check(&g_state, 0));
}

void test_v2_different_var_indices(void) {
    /* Var 0: reject (unbounded lower) */
    g_lb[0] = -2e100;
    /* Var 1: accept */
    g_lb[1] = 0.0;
    /* Var 2: reject (reserved flags) */
    g_flags[2] = 0x100;
    /* Var 3: accept */
    g_lb[3] = -50.0;

    TEST_ASSERT_EQUAL_INT(0, cxf_special_check(&g_state, 0));
    TEST_ASSERT_EQUAL_INT(1, cxf_special_check(&g_state, 1));
    TEST_ASSERT_EQUAL_INT(0, cxf_special_check(&g_state, 2));
    TEST_ASSERT_EQUAL_INT(1, cxf_special_check(&g_state, 3));
}

void test_v2_zero_lb_zero_ub(void) {
    g_lb[0] = 0.0;
    g_ub[0] = 0.0;
    /* Fixed variable, no special flags => qualifies */
    TEST_ASSERT_EQUAL_INT(1, cxf_special_check(&g_state, 0));
}

/*==========================================================================
 * Main
 *=========================================================================*/

int main(void) {
    UNITY_BEGIN();
    /* Stage 1 */
    RUN_TEST(test_v2_stage1_exactly_at_neg_inf);
    RUN_TEST(test_v2_stage1_below_neg_inf);
    RUN_TEST(test_v2_stage1_finite_negative);
    /* Stage 2 */
    RUN_TEST(test_v2_stage2_no_flags);
    RUN_TEST(test_v2_stage2_reserved_bit4);
    RUN_TEST(test_v2_stage2_reserved_high_bit);
    /* Stage 3 */
    RUN_TEST(test_v2_stage3_ub_flag_normal);
    RUN_TEST(test_v2_stage3_ub_flag_lb_above_pos_inf);
    RUN_TEST(test_v2_stage3_no_ub_flag_lb_large);
    /* Stage 4 */
    RUN_TEST(test_v2_stage4_quadratic_rejected);
    /* Edge cases */
    RUN_TEST(test_v2_null_state);
    RUN_TEST(test_v2_null_var_flags_treated_as_zero);
    RUN_TEST(test_v2_different_var_indices);
    RUN_TEST(test_v2_zero_lb_zero_ub);
    return UNITY_END();
}
