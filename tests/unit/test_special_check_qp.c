/**
 * @file test_special_check_qp.c
 * @brief Tests for cxf_special_check Stage 4 quadratic handling
 *
 * V2 spec (data_validation.md Stage 4): when quadratic flag is set,
 * validate Q-matrix structure.  LP-only solver has no Q-matrix storage,
 * so quadratic-flagged variables must be ACCEPTED (not rejected).
 *
 * Issue: convexfeld-o4n9
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
 * Core: quadratic flag without Q-matrix data => accept
 *=========================================================================*/

void test_qp_flag_alone_accepted(void) {
    /* Quadratic flag set, no Q-matrix data => accept per V2 spec */
    g_flags[0] = VARFLAG_HAS_QUADRATIC;
    TEST_ASSERT_EQUAL_INT(1, cxf_special_check(&g_state, 0));
}

void test_qp_flag_with_finite_ub_accepted(void) {
    /* Both quadratic and finite-UB flags, normal bounds => accept */
    g_flags[1] = VARFLAG_HAS_QUADRATIC | VARFLAG_UPPER_FINITE;
    g_lb[1]    = 0.0;
    g_ub[1]    = 50.0;
    TEST_ASSERT_EQUAL_INT(1, cxf_special_check(&g_state, 1));
}

void test_qp_flag_negative_lb_accepted(void) {
    /* Quadratic flag, negative but finite lower bound => accept */
    g_flags[2] = VARFLAG_HAS_QUADRATIC;
    g_lb[2]    = -500.0;
    TEST_ASSERT_EQUAL_INT(1, cxf_special_check(&g_state, 2));
}

void test_qp_flag_zero_bounds_accepted(void) {
    /* Quadratic flag, fixed variable (lb == ub == 0) => accept */
    g_flags[3] = VARFLAG_HAS_QUADRATIC;
    g_lb[3]    = 0.0;
    g_ub[3]    = 0.0;
    TEST_ASSERT_EQUAL_INT(1, cxf_special_check(&g_state, 3));
}

/*==========================================================================
 * Quadratic flag does NOT override earlier-stage rejections
 *=========================================================================*/

void test_qp_flag_unbounded_below_rejected(void) {
    /* Stage 1 rejects: lb below -CXF_INFINITY, even with quadratic flag */
    g_flags[0] = VARFLAG_HAS_QUADRATIC;
    g_lb[0]    = -1.1e100;
    TEST_ASSERT_EQUAL_INT(0, cxf_special_check(&g_state, 0));
}

void test_qp_flag_reserved_bits_rejected(void) {
    /* Stage 2 rejects: reserved flag bits set alongside quadratic flag */
    g_flags[0] = VARFLAG_HAS_QUADRATIC | 0x80000000;
    TEST_ASSERT_EQUAL_INT(0, cxf_special_check(&g_state, 0));
}

void test_qp_flag_ub_finite_lb_above_inf_rejected(void) {
    /* Stage 3 rejects: finite-UB flag + lb above +CXF_INFINITY */
    g_flags[0] = VARFLAG_HAS_QUADRATIC | VARFLAG_UPPER_FINITE;
    g_lb[0]    = 1.1e100;
    TEST_ASSERT_EQUAL_INT(0, cxf_special_check(&g_state, 0));
}

/*==========================================================================
 * Multiple variables: quadratic vs non-quadratic in same state
 *=========================================================================*/

void test_qp_mixed_variables(void) {
    /* Var 0: no flags, normal bounds => accept */
    g_flags[0] = 0;
    g_lb[0]    = 0.0;
    /* Var 1: quadratic flag => accept (no Q-matrix) */
    g_flags[1] = VARFLAG_HAS_QUADRATIC;
    g_lb[1]    = 10.0;
    /* Var 2: no flags, unbounded below => reject */
    g_flags[2] = 0;
    g_lb[2]    = -2e100;
    /* Var 3: quadratic + unbounded below => reject (Stage 1) */
    g_flags[3] = VARFLAG_HAS_QUADRATIC;
    g_lb[3]    = -2e100;

    TEST_ASSERT_EQUAL_INT(1, cxf_special_check(&g_state, 0));
    TEST_ASSERT_EQUAL_INT(1, cxf_special_check(&g_state, 1));
    TEST_ASSERT_EQUAL_INT(0, cxf_special_check(&g_state, 2));
    TEST_ASSERT_EQUAL_INT(0, cxf_special_check(&g_state, 3));
}

/*==========================================================================
 * Null var_flags with quadratic (flags default to 0 => no quadratic)
 *=========================================================================*/

void test_qp_null_flags_accepted(void) {
    /* var_flags NULL => flags treated as 0 => no quadratic flag => accept */
    g_state.var_flags = NULL;
    TEST_ASSERT_EQUAL_INT(1, cxf_special_check(&g_state, 0));
}

/*==========================================================================
 * Main
 *=========================================================================*/

int main(void) {
    UNITY_BEGIN();
    /* Core acceptance */
    RUN_TEST(test_qp_flag_alone_accepted);
    RUN_TEST(test_qp_flag_with_finite_ub_accepted);
    RUN_TEST(test_qp_flag_negative_lb_accepted);
    RUN_TEST(test_qp_flag_zero_bounds_accepted);
    /* Earlier stages still reject */
    RUN_TEST(test_qp_flag_unbounded_below_rejected);
    RUN_TEST(test_qp_flag_reserved_bits_rejected);
    RUN_TEST(test_qp_flag_ub_finite_lb_above_inf_rejected);
    /* Mixed */
    RUN_TEST(test_qp_mixed_variables);
    /* Null flags */
    RUN_TEST(test_qp_null_flags_accepted);
    return UNITY_END();
}
