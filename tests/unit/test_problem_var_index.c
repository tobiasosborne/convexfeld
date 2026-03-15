/**
 * @file test_problem_var_index.c
 * @brief Tests for SolverState.problem_var_index (V2 solver_state.md).
 *
 * Validates that problem_var_index stores a variable index (not a row
 * index) when infeasibility or unboundedness is detected.
 *
 * Spec: docs/specs-v2/specs/data-model/solver_state.md
 *   "problemVarIndex: Index of the variable that caused infeasibility
 *    or unboundedness, for diagnostics. 0 to numVars-1, or -1 if unset."
 */

#include "unity.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_types.h"
#include <stdlib.h>
#include <string.h>

extern int cxf_simplex_crash(SolverState *state, CxfEnv *env);
extern int cxf_addconstr(CxfModel *model, int numnz, const int *cind,
                         const double *cval, char sense, double rhs,
                         const char *name);

static CxfEnv *env = NULL;

void setUp(void) { cxf_loadenv(&env, NULL); }
void tearDown(void) { cxf_freeenv(env); env = NULL; }

/*******************************************************************************
 * 1. Initialization: field defaults to -1
 ******************************************************************************/
void test_init_default_minus_one(void) {
    CxfModel *model;
    cxf_newmodel(env, &model, "t", 0, NULL, NULL, NULL, NULL, NULL);
    cxf_addvar(model, 0, NULL, NULL, 0.0, 0.0, 1.0, 'C', NULL);

    SolverState *state = NULL;
    cxf_simplex_init(model, &state);

    TEST_ASSERT_EQUAL_INT(-1, state->problem_var_index);

    cxf_simplex_final(state);
    cxf_freemodel(model);
}

/*******************************************************************************
 * 2. Crash infeasibility sets variable index (n + row), not row index
 ******************************************************************************/
void test_crash_sets_var_index_not_row(void) {
    CxfModel *model;
    cxf_newmodel(env, &model, "t", 0, NULL, NULL, NULL, NULL, NULL);
    /* 3 structural variables */
    for (int j = 0; j < 3; j++)
        cxf_addvar(model, 0, NULL, NULL, 0.0, 0.0, 10.0, 'C', NULL);

    SolverState *state = NULL;
    cxf_simplex_init(model, &state);

    /* Manually set up 2 constraints for crash */
    int n = state->num_vars;  /* 3 */
    state->num_constrs = 2;

    free(state->row_status);
    state->row_status = (int *)calloc(2, sizeof(int));
    free(state->work_rhs);
    state->work_rhs = (double *)malloc(2 * sizeof(double));
    free(state->work_sense);
    state->work_sense = (char *)malloc(2 * sizeof(char));

    /* Row 0: feasible inequality */
    state->work_rhs[0] = 5.0;
    state->work_sense[0] = '<';
    /* Row 1: infeasible inequality (negative RHS) */
    state->work_rhs[1] = -100.0;
    state->work_sense[1] = '<';

    state->num_basic = 0;
    state->problem_var_index = -1;

    int rc = cxf_simplex_crash(state, env);

    TEST_ASSERT_EQUAL_INT(CXF_INFEASIBLE, rc);
    /* V2: variable index = n + row = 3 + 1 = 4 (slack for row 1) */
    TEST_ASSERT_EQUAL_INT(n + 1, state->problem_var_index);
    /* Must NOT be a row index */
    TEST_ASSERT_TRUE(state->problem_var_index >= n);

    cxf_simplex_final(state);
    cxf_freemodel(model);
}

/*******************************************************************************
 * 3. Crash equality infeasibility also sets variable index
 ******************************************************************************/
void test_crash_equality_sets_var_index(void) {
    CxfModel *model;
    cxf_newmodel(env, &model, "t", 0, NULL, NULL, NULL, NULL, NULL);
    cxf_addvar(model, 0, NULL, NULL, 0.0, 0.0, 10.0, 'C', NULL);

    SolverState *state = NULL;
    cxf_simplex_init(model, &state);

    int n = state->num_vars;  /* 1 */
    state->num_constrs = 1;

    free(state->row_status);
    state->row_status = (int *)calloc(1, sizeof(int));
    free(state->work_rhs);
    state->work_rhs = (double *)malloc(sizeof(double));
    free(state->work_sense);
    state->work_sense = (char *)malloc(sizeof(char));

    state->work_rhs[0] = 999.0;  /* Non-zero RHS on equality = infeasible */
    state->work_sense[0] = '=';
    state->num_basic = 0;
    state->problem_var_index = -1;

    int rc = cxf_simplex_crash(state, env);

    TEST_ASSERT_EQUAL_INT(CXF_INFEASIBLE, rc);
    /* Variable index = n + 0 = 1 (slack for row 0) */
    TEST_ASSERT_EQUAL_INT(n, state->problem_var_index);

    cxf_simplex_final(state);
    cxf_freemodel(model);
}

/*******************************************************************************
 * 4. Phase-end free-variable infeasibility: already a variable index
 ******************************************************************************/
void test_phase_end_sets_var_index(void) {
    CxfModel *model;
    cxf_newmodel(env, &model, "t", 0, NULL, NULL, NULL, NULL, NULL);
    /* 2 vars + 1 constraint */
    for (int j = 0; j < 2; j++)
        cxf_addvar(model, 0, NULL, NULL, 0.0, 0.0, 10.0, 'C', NULL);
    int ind[] = {0, 1};
    double val[] = {1.0, 1.0};
    cxf_addconstr(model, 2, ind, val, '<', 10.0, NULL);

    SolverState *state = NULL;
    cxf_simplex_init(model, &state);

    BasisState *basis = state->basis;
    int n = state->num_vars;

    /* Mark var 0 as superbasic (free) with significant reduced cost */
    basis->var_status[0] = CXF_VAR_SUPERBASIC;
    state->work_dj[0] = 5.0;  /* > optimality_tol */

    /* Others basic/at bound */
    basis->var_status[1] = CXF_VAR_AT_LOWER;
    state->work_dj[1] = 0.0;
    basis->var_status[n] = 0;
    state->work_dj[n] = 0.0;

    state->problem_var_index = -1;

    int rc = cxf_simplex_phase_end(state, env, 0);

    TEST_ASSERT_EQUAL_INT(CXF_INFEASIBLE, rc);
    /* problem_var_index should be 0 (the free variable) */
    TEST_ASSERT_EQUAL_INT(0, state->problem_var_index);

    cxf_simplex_final(state);
    cxf_freemodel(model);
}

/*******************************************************************************
 * Runner
 ******************************************************************************/
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_init_default_minus_one);
    RUN_TEST(test_crash_sets_var_index_not_row);
    RUN_TEST(test_crash_equality_sets_var_index);
    RUN_TEST(test_phase_end_sets_var_index);
    return UNITY_END();
}
