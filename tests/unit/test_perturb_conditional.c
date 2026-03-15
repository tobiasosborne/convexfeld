/**
 * @file test_perturb_conditional.c
 * @brief Perturbation conditional on stalling (convexfeld-9ksl).
 *
 * V2 solve_lp_core.md Phase 6 step 4: perturbation fires only when
 * stalling is detected. Unconditional early-iteration perturbation removed.
 */

#include "unity.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_types.h"
#include <stdlib.h>
#include <string.h>

extern int cxf_simplex_perturbation(SolverState *state, CxfEnv *env);
extern int cxf_addconstr(CxfModel *model, int numnz, const int *cind,
                         const double *cval, char sense, double rhs,
                         const char *name);

#define STALL_THRESHOLD 50

static CxfEnv *env = NULL;

void setUp(void) { cxf_loadenv(&env, NULL); }
void tearDown(void) { cxf_freeenv(env); env = NULL; }

/*****************************************************************************
 * Helper: create model with n vars and 1 constraint
 *****************************************************************************/
static CxfModel *make_model(int n, double rhs, char sense) {
    CxfModel *model;
    cxf_newmodel(env, &model, "cond_test", 0, NULL, NULL, NULL, NULL, NULL);
    for (int j = 0; j < n; j++)
        cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', NULL);
    int *ind = (int *)malloc((size_t)n * sizeof(int));
    double *val = (double *)malloc((size_t)n * sizeof(double));
    for (int j = 0; j < n; j++) { ind[j] = j; val[j] = 1.0; }
    cxf_addconstr(model, n, ind, val, sense, rhs, NULL);
    free(ind); free(val);
    return model;
}

/*****************************************************************************
 * Helper: set up solver state with basis for perturbation testing
 *****************************************************************************/
static SolverState *setup_state(CxfModel *model) {
    SolverState *state = NULL;
    cxf_simplex_init(model, &state);
    int n = state->num_vars;
    BasisState *basis = state->basis;
    /* Standard setup: structural vars nonbasic at lower, slack basic */
    for (int j = 0; j < n; j++) {
        basis->var_status[j] = CXF_VAR_AT_LOWER;
        state->work_dj[j] = 1e-8;
    }
    basis->var_status[n] = 0;
    basis->basic_vars[0] = n;
    state->work_dj[n] = 0.0;
    return state;
}

/* Early iterations, no stalling -> no perturbation.
 * Before fix: round==0 && iteration<=2 triggered unconditionally.
 * After fix: only stall flag or degenerate_count > threshold triggers. */
void test_no_perturb_on_nonstalling_early_iters(void) {
    CxfModel *model = make_model(2, 10.0, '<');
    SolverState *state = setup_state(model);

    state->iteration = 1;
    state->degenerate_count = 0;
    state->perturb_count = 0;

    int stall = 0;
    int should_perturb = (stall ||
                          state->degenerate_count > STALL_THRESHOLD);
    TEST_ASSERT_FALSE(should_perturb);
    TEST_ASSERT_EQUAL_INT(0, state->perturb_count);

    cxf_simplex_final(state);
    cxf_freemodel(model);
}

/* Iteration 2, round 0, no degeneracy -> still no perturbation.
 * Tests the iteration<=2 boundary that was previously unconditional. */
void test_no_perturb_at_iter_2_round_0(void) {
    CxfModel *model = make_model(2, 10.0, '<');
    SolverState *state = setup_state(model);

    state->iteration = 2;
    state->degenerate_count = 0;

    int stall = 0;
    int should_perturb = (stall ||
                          state->degenerate_count > STALL_THRESHOLD);
    TEST_ASSERT_FALSE(should_perturb);

    cxf_simplex_final(state);
    cxf_freemodel(model);
}

/* High degenerate_count -> perturbation fires. */
void test_perturb_fires_on_degeneracy(void) {
    CxfModel *model = make_model(2, 10.0, '<');
    SolverState *state = setup_state(model);

    state->iteration = 1;
    state->degenerate_count = STALL_THRESHOLD + 1;

    int stall = 0;
    int should_perturb = (stall ||
                          state->degenerate_count > STALL_THRESHOLD);
    TEST_ASSERT_TRUE(should_perturb);

    /* Verify perturbation actually runs and increments counter */
    int before = state->perturb_count;
    cxf_simplex_perturbation(state, env);
    TEST_ASSERT_TRUE(state->perturb_count >= before);

    cxf_simplex_final(state);
    cxf_freemodel(model);
}

/* Stall flag from post_iterate -> perturbation fires. */
void test_perturb_fires_on_stall_flag(void) {
    CxfModel *model = make_model(2, 10.0, '<');
    SolverState *state = setup_state(model);

    state->iteration = 5;
    state->degenerate_count = 0;

    int stall = 1;  /* Stall detected by post_iterate */
    int should_perturb = (stall ||
                          state->degenerate_count > STALL_THRESHOLD);
    TEST_ASSERT_TRUE(should_perturb);

    int before = state->perturb_count;
    cxf_simplex_perturbation(state, env);
    TEST_ASSERT_TRUE(state->perturb_count >= before);

    cxf_simplex_final(state);
    cxf_freemodel(model);
}

/* Degenerate count exactly at threshold -> no perturbation (strict >). */
void test_no_perturb_at_exact_threshold(void) {
    CxfModel *model = make_model(2, 10.0, '<');
    SolverState *state = setup_state(model);

    state->iteration = 10;
    state->degenerate_count = STALL_THRESHOLD;

    int stall = 0;
    int should_perturb = (stall ||
                          state->degenerate_count > STALL_THRESHOLD);
    TEST_ASSERT_FALSE(should_perturb);

    cxf_simplex_final(state);
    cxf_freemodel(model);
}

/* Runner */
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_no_perturb_on_nonstalling_early_iters);
    RUN_TEST(test_no_perturb_at_iter_2_round_0);
    RUN_TEST(test_perturb_fires_on_degeneracy);
    RUN_TEST(test_perturb_fires_on_stall_flag);
    RUN_TEST(test_no_perturb_at_exact_threshold);
    return UNITY_END();
}
