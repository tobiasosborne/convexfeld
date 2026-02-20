/**
 * @file test_perturbation.c
 * @brief Tests for EXPAND perturbation (v2 — P2.6)
 *
 * Tests cxf_simplex_perturbation implied bound analysis:
 * null args, empty problem, nonbasic degeneracy removal,
 * infeasibility detection, unperturbation.
 */

#include "unity.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_matrix.h"
#include "convexfeld/cxf_types.h"
#include <stdlib.h>
#include <string.h>

extern int cxf_simplex_perturbation(SolverState *state, CxfEnv *env);
extern int cxf_simplex_unperturb(SolverState *state, CxfEnv *env);
extern int cxf_addconstr(CxfModel *model, int numnz, const int *cind,
                         const double *cval, char sense, double rhs,
                         const char *name);

static CxfEnv *env = NULL;

void setUp(void) { cxf_loadenv(&env, NULL); }
void tearDown(void) { cxf_freeenv(env); env = NULL; }

/*******************************************************************************
 * Helper: create model with n vars and 1 LE constraint
 ******************************************************************************/
static CxfModel *make_model(int n, double rhs, char sense) {
    CxfModel *model;
    cxf_newmodel(env, &model, "perturb_test", 0, NULL, NULL, NULL, NULL, NULL);
    for (int j = 0; j < n; j++) {
        cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', NULL);
    }
    /* Add constraint: sum(x_j) sense rhs */
    int *ind = (int *)malloc((size_t)n * sizeof(int));
    double *val = (double *)malloc((size_t)n * sizeof(double));
    for (int j = 0; j < n; j++) { ind[j] = j; val[j] = 1.0; }
    cxf_addconstr(model, n, ind, val, sense, rhs, NULL);
    free(ind); free(val);
    return model;
}

/*******************************************************************************
 * Null argument tests
 ******************************************************************************/
void test_perturb_null_state(void) {
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT,
                          cxf_simplex_perturbation(NULL, env));
}

void test_perturb_null_env(void) {
    SolverState dummy;
    memset(&dummy, 0, sizeof(dummy));
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT,
                          cxf_simplex_perturbation(&dummy, NULL));
}

void test_unperturb_null(void) {
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT,
                          cxf_simplex_unperturb(NULL, env));
}

/*******************************************************************************
 * Empty problem → OK, zero perturbed
 ******************************************************************************/
void test_perturb_empty_problem(void) {
    SolverState dummy;
    memset(&dummy, 0, sizeof(dummy));
    CxfModel model;
    memset(&model, 0, sizeof(model));
    dummy.model_ref = &model;
    TEST_ASSERT_EQUAL_INT(CXF_OK, cxf_simplex_perturbation(&dummy, env));
    TEST_ASSERT_EQUAL_INT(0, dummy.perturb_count);
}

/*******************************************************************************
 * No basis yet → safe no-op
 ******************************************************************************/
void test_perturb_no_basis(void) {
    SolverState dummy;
    memset(&dummy, 0, sizeof(dummy));
    CxfModel model;
    memset(&model, 0, sizeof(model));
    dummy.model_ref = &model;
    dummy.num_vars = 2;
    dummy.num_constrs = 1;
    dummy.basis = NULL;
    TEST_ASSERT_EQUAL_INT(CXF_OK, cxf_simplex_perturbation(&dummy, env));
}

/*******************************************************************************
 * Nonbasic degeneracy removal: near-zero RC at lower bound → FIXED
 ******************************************************************************/
void test_perturb_removes_degenerate_nonbasic(void) {
    CxfModel *model = make_model(2, 10.0, '<');
    SolverState *state = NULL;
    cxf_simplex_init(model, &state);

    int n = state->num_vars;
    BasisState *basis = state->basis;

    /* EXPAND requires iteration > 0 (stall-triggered) */
    state->iteration = 1;

    /* Set up: both structural vars nonbasic at lower, slack basic */
    basis->var_status[0] = CXF_VAR_AT_LOWER;
    basis->var_status[1] = CXF_VAR_AT_LOWER;
    basis->var_status[n] = 0;  /* slack basic in row 0 */
    basis->basic_vars[0] = n;

    /* x0: near-zero RC → degenerate */
    state->work_dj[0] = 1e-8;
    /* x1: large positive RC → NOT degenerate at lower (not improving) */
    state->work_dj[1] = 5.0;
    /* slack: basic, skip */
    state->work_dj[n] = 0.0;

    int rc = cxf_simplex_perturbation(state, env);

    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    /* x0 should be AT_UPPER (v2: degenerate vars flipped, not FIXED) */
    TEST_ASSERT_EQUAL_INT(CXF_VAR_AT_UPPER, basis->var_status[0]);
    /* x1 stays AT_LOWER (large positive RC, not near-zero, not negative) */
    TEST_ASSERT_EQUAL_INT(CXF_VAR_AT_LOWER, basis->var_status[1]);
    TEST_ASSERT_TRUE(state->perturb_count >= 1);

    cxf_simplex_final(state);
    cxf_freemodel(model);
}

/*******************************************************************************
 * Severely negative RC on equality slack → INFEASIBLE
 ******************************************************************************/
void test_perturb_infeasible_equality(void) {
    CxfModel *model = make_model(2, 5.0, '=');
    SolverState *state = NULL;
    cxf_simplex_init(model, &state);

    int n = state->num_vars;
    BasisState *basis = state->basis;

    /* EXPAND requires iteration > 0 */
    state->iteration = 1;

    /* Slack for equality constraint at lower bound with negative RC */
    basis->var_status[n] = CXF_VAR_AT_LOWER;
    state->work_dj[n] = -2.0;

    int rc = cxf_simplex_perturbation(state, env);

    TEST_ASSERT_EQUAL_INT(CXF_INFEASIBLE, rc);
    TEST_ASSERT_EQUAL_INT(0, state->problem_row_index);

    cxf_simplex_final(state);
    cxf_freemodel(model);
}

/*******************************************************************************
 * Unperturb restores original bounds
 ******************************************************************************/
void test_unperturb_restores_bounds(void) {
    CxfModel *model = make_model(2, 10.0, '<');
    SolverState *state = NULL;
    cxf_simplex_init(model, &state);

    /* Corrupt working bounds */
    state->work_lb[0] = 99.0;
    state->work_ub[1] = -99.0;
    state->perturb_count = 3;

    int rc = cxf_simplex_unperturb(state, env);

    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 0.0, state->work_lb[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 10.0, state->work_ub[1]);
    TEST_ASSERT_EQUAL_INT(0, state->perturb_count);

    cxf_simplex_final(state);
    cxf_freemodel(model);
}

/*******************************************************************************
 * Unperturb with no perturbation → returns 1
 ******************************************************************************/
void test_unperturb_noop(void) {
    CxfModel *model = make_model(1, 5.0, '<');
    SolverState *state = NULL;
    cxf_simplex_init(model, &state);

    state->perturb_count = 0;
    int rc = cxf_simplex_unperturb(state, env);
    TEST_ASSERT_EQUAL_INT(1, rc);

    cxf_simplex_final(state);
    cxf_freemodel(model);
}

/*******************************************************************************
 * Runner
 ******************************************************************************/
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_perturb_null_state);
    RUN_TEST(test_perturb_null_env);
    RUN_TEST(test_unperturb_null);
    RUN_TEST(test_perturb_empty_problem);
    RUN_TEST(test_perturb_no_basis);
    RUN_TEST(test_perturb_removes_degenerate_nonbasic);
    RUN_TEST(test_perturb_infeasible_equality);
    RUN_TEST(test_unperturb_restores_bounds);
    RUN_TEST(test_unperturb_noop);
    return UNITY_END();
}
