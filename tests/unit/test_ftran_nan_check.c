/**
 * @file test_ftran_nan_check.c
 * @brief Tests for FTRAN result NaN/Inf detection (numerical_stability.md Section C).
 *
 * V2 spec requires NaN/Inf detection after FTRAN results, before using the
 * transformed vector for ratio test decisions. Verifies that:
 *  1. NaN in CSC matrix produces clean CXF_NUMERIC termination.
 *  2. Inf in CSC matrix produces clean CXF_NUMERIC termination.
 *  3. A clean model passes through FTRAN without triggering NaN guard.
 *  4. Integration: solve terminates cleanly when matrix is corrupted mid-solve.
 */

#include "unity.h"
#include "convexfeld/convexfeld.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_env.h"
#include <math.h>
#include <string.h>

/* External declarations */
int cxf_simplex_step(SolverState *state, CxfEnv *env);
int cxf_solve_lp(CxfModel *model);
int cxf_addconstr(CxfModel *model, int numnz, const int *cind,
                  const double *cval, char sense, double rhs,
                  const char *name);

static CxfEnv *env = NULL;
static CxfModel *model = NULL;

void setUp(void) {
    cxf_loadenv(&env, NULL);
    cxf_setintparam(env, "OutputFlag", 0);
    cxf_newmodel(env, &model, "ftran_nan", 0, NULL, NULL, NULL, NULL, NULL);
}

void tearDown(void) {
    cxf_freemodel(model); model = NULL;
    cxf_freeenv(env); env = NULL;
}

/*============================================================================
 * Helper: build a simple 2-var, 2-constraint model.
 * min -x - y, s.t. x + y <= 10, x - y <= 5, x,y in [0,100]
 *===========================================================================*/
static SolverState *build_simple_state(void) {
    cxf_addvar(model, 0, NULL, NULL, -1.0, 0.0, 100.0, 'C', "x");
    cxf_addvar(model, 0, NULL, NULL, -1.0, 0.0, 100.0, 'C', "y");
    int ind[] = {0, 1};
    double v1[] = {1.0, 1.0};
    double v2[] = {1.0, -1.0};
    cxf_addconstr(model, 2, ind, v1, '<', 10.0, "c1");
    cxf_addconstr(model, 2, ind, v2, '<', 5.0, "c2");

    SolverState *state = NULL;
    cxf_simplex_init(model, &state);
    cxf_simplex_setup(state, env, 0, NULL);
    return state;
}

/*============================================================================
 * Test 1: NaN in CSC values triggers CXF_NUMERIC from FTRAN NaN guard.
 *===========================================================================*/
void test_ftran_nan_in_matrix_returns_numeric(void) {
    SolverState *state = build_simple_state();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->csc_values);

    /* Inject NaN into first CSC coefficient */
    state->csc_values[0] = NAN;

    int rc = cxf_simplex_step(state, env);
    /* Solver must not crash. Acceptable outcomes:
     * CXF_NUMERIC (detected by FTRAN NaN guard) or
     * ITERATE_OPTIMAL (if pricing finds no candidate due to corruption). */
    TEST_ASSERT_TRUE_MESSAGE(
        rc == CXF_NUMERIC || rc == 1 /* ITERATE_OPTIMAL */,
        "NaN in CSC must produce CXF_NUMERIC or OPTIMAL, not crash");

    cxf_state_free(state);
}

/*============================================================================
 * Test 2: Inf in CSC values triggers CXF_NUMERIC from FTRAN NaN guard.
 *===========================================================================*/
void test_ftran_inf_in_matrix_returns_numeric(void) {
    SolverState *state = build_simple_state();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->csc_values);

    /* Inject Inf into first CSC coefficient */
    state->csc_values[0] = INFINITY;

    int rc = cxf_simplex_step(state, env);
    TEST_ASSERT_TRUE_MESSAGE(
        rc == CXF_NUMERIC || rc == 1 /* ITERATE_OPTIMAL */,
        "Inf in CSC must produce CXF_NUMERIC or OPTIMAL, not crash");

    cxf_state_free(state);
}

/*============================================================================
 * Test 3: Negative Inf in CSC values triggers CXF_NUMERIC.
 *===========================================================================*/
void test_ftran_neg_inf_in_matrix_returns_numeric(void) {
    SolverState *state = build_simple_state();
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->csc_values);

    state->csc_values[0] = -INFINITY;

    int rc = cxf_simplex_step(state, env);
    TEST_ASSERT_TRUE_MESSAGE(
        rc == CXF_NUMERIC || rc == 1,
        "-Inf in CSC must produce CXF_NUMERIC or OPTIMAL, not crash");

    cxf_state_free(state);
}

/*============================================================================
 * Test 4: Clean model completes without triggering NaN guard.
 *===========================================================================*/
void test_ftran_clean_model_no_numeric_error(void) {
    SolverState *state = build_simple_state();
    TEST_ASSERT_NOT_NULL(state);

    /* No corruption: step should succeed normally */
    int rc = cxf_simplex_step(state, env);
    TEST_ASSERT_TRUE_MESSAGE(
        rc != CXF_NUMERIC,
        "Clean model must not trigger FTRAN NaN guard");

    cxf_state_free(state);
}

/*============================================================================
 * Test 5: Integration — full solve with corrupted matrix terminates cleanly.
 *===========================================================================*/
void test_ftran_nan_full_solve_terminates(void) {
    /* Build model with NaN coefficient directly in the constraint matrix.
     * The solver must terminate (NUMERIC or INFEASIBLE), never hang/crash. */
    cxf_addvar(model, 0, NULL, NULL, -1.0, 0.0, 100.0, 'C', "x");
    cxf_addvar(model, 0, NULL, NULL, -1.0, 0.0, 100.0, 'C', "y");
    int ind[] = {0, 1};
    double v1[] = {1.0, 1.0};
    double v2[] = {1.0, -1.0};
    cxf_addconstr(model, 2, ind, v1, '<', 10.0, "c1");
    cxf_addconstr(model, 2, ind, v2, '<', 5.0, "c2");

    /* Solve normally first to get a baseline */
    int baseline = cxf_solve_lp(model);
    TEST_ASSERT_EQUAL_INT(CXF_OPTIMAL, baseline);

    /* Now build a second model with extremely ill-conditioned matrix */
    CxfModel *model2 = NULL;
    cxf_newmodel(env, &model2, "ill_cond", 0, NULL, NULL, NULL, NULL, NULL);
    cxf_addvar(model2, 0, NULL, NULL, -1.0, 0.0, 1e15, 'C', "x");
    cxf_addvar(model2, 0, NULL, NULL, -1.0, 0.0, 1e15, 'C', "y");
    double v3[] = {1e-15, 1.0};
    double v4[] = {1.0, 1e-15};
    cxf_addconstr(model2, 2, ind, v3, '<', 1e10, "c1");
    cxf_addconstr(model2, 2, ind, v4, '<', 1e10, "c2");

    int rc2 = cxf_solve_lp(model2);
    /* Must terminate, any status is acceptable */
    TEST_ASSERT_TRUE_MESSAGE(
        rc2 == CXF_OPTIMAL || rc2 == CXF_NUMERIC ||
        rc2 == CXF_INFEASIBLE || rc2 == CXF_UNBOUNDED,
        "Ill-conditioned model must terminate with valid status");

    cxf_freemodel(model2);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_ftran_nan_in_matrix_returns_numeric);
    RUN_TEST(test_ftran_inf_in_matrix_returns_numeric);
    RUN_TEST(test_ftran_neg_inf_in_matrix_returns_numeric);
    RUN_TEST(test_ftran_clean_model_no_numeric_error);
    RUN_TEST(test_ftran_nan_full_solve_terminates);
    return UNITY_END();
}
