/**
 * @file test_expand_eps_base.c
 * @brief Tests that EXPAND eps_base scales from feasibility tolerance.
 *
 * Spec: perturbation.md Phase 5 — "epsilon_base is typically on the
 * order of 1e-6 to 1e-8 (scaled from the feasibility tolerance)."
 *
 * With default feas_tol = 1e-6 the base should land in the middle of
 * [1e-8, 1e-6], NOT always at 1e-6 (the max).
 *
 * Issue: convexfeld-4xf3
 */

#include "unity.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_types.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Internal function under test */
extern int cxf_expand_widen_bounds(SolverState *state, CxfEnv *env,
                                   double feas_tol, BasisState *basis,
                                   int m, int total);
extern int cxf_addconstr(CxfModel *model, int numnz, const int *cind,
                         const double *cval, char sense, double rhs,
                         const char *name);

static CxfEnv *env = NULL;

void setUp(void) { cxf_loadenv(&env, NULL); }
void tearDown(void) { cxf_freeenv(env); env = NULL; }

/* Helper: 2-var model with one LE constraint */
static CxfModel *make_model(void) {
    CxfModel *model;
    cxf_newmodel(env, &model, "eps_test", 0, NULL, NULL, NULL, NULL, NULL);
    for (int j = 0; j < 2; j++)
        cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', NULL);
    int ind[] = {0, 1};
    double val[] = {1.0, 1.0};
    cxf_addconstr(model, 2, ind, val, '<', 10.0, NULL);
    return model;
}

/* Helper: activate EXPAND conditions, place basic var at lower bound */
static void setup_expand(SolverState *s, BasisState *b, int n, int m,
                         int total) {
    /* Basic var (slack) at lower bound */
    b->basic_vars[0] = n;
    b->var_status[n] = 0;
    b->var_status[0] = CXF_VAR_AT_LOWER;
    b->var_status[1] = CXF_VAR_AT_LOWER;
    s->work_x[n] = 0.0;

    /* Copy bounds into saved arrays */
    memcpy(s->saved_lb, s->work_lb, (size_t)total * sizeof(double));
    memcpy(s->saved_ub, s->work_ub, (size_t)total * sizeof(double));

    /* Satisfy EXPAND activation: degen_count > 100, perturb_count > 0 */
    s->degenerate_count = 200;
    s->perturb_count = 1;
    s->mechanism_a_applied = 1;
    s->perturb_expand_active = 0;
    s->iteration = 1;
}

/**
 * Default feas_tol = 1e-6 gives eps_base = 1e-7 (mid-range).
 * The widened lb must reflect 1e-7 scaling, NOT 1e-6.
 */
void test_default_tol_gives_mid_range(void) {
    CxfModel *model = make_model();
    SolverState *state = NULL;
    cxf_simplex_init(model, &state);

    int n = state->num_vars;
    int m = state->num_constrs;
    int total = n + m;
    BasisState *basis = state->basis;

    setup_expand(state, basis, n, m, total);
    double saved_lb = state->saved_lb[n];

    int widened = cxf_expand_widen_bounds(state, env, 1e-6, basis, m, total);
    TEST_ASSERT_TRUE(widened > 0);

    double shift = saved_lb - state->work_lb[n];
    /* eps_base = 1e-7; eps = eps_base * (1+|lb|) * (1+hash).
     * With lb=0, eps = 1e-7 * 1 * (1+hash) in [1e-7, 2e-7).
     * Shift must be < 2e-7 (i.e. not at old 1e-6 scale). */
    TEST_ASSERT_TRUE_MESSAGE(shift > 0.0, "bound must widen");
    TEST_ASSERT_TRUE_MESSAGE(shift < 2.5e-7,
        "shift must reflect 1e-7 eps_base, not 1e-6");
    TEST_ASSERT_TRUE_MESSAGE(shift >= 1e-7,
        "shift must be at least eps_base");

    cxf_state_free(state);
    cxf_freemodel(model);
}

/**
 * Very small feas_tol (1e-9) should clamp eps_base to 1e-8 floor.
 */
void test_small_tol_clamps_to_floor(void) {
    CxfModel *model = make_model();
    SolverState *state = NULL;
    cxf_simplex_init(model, &state);

    int n = state->num_vars;
    int m = state->num_constrs;
    int total = n + m;
    BasisState *basis = state->basis;

    setup_expand(state, basis, n, m, total);
    double saved_lb = state->saved_lb[n];

    /* feas_tol = 1e-9 -> feas_tol/10 = 1e-10 -> clamped to 1e-8 */
    int widened = cxf_expand_widen_bounds(state, env, 1e-9, basis, m, total);
    TEST_ASSERT_TRUE(widened > 0);

    double shift = saved_lb - state->work_lb[n];
    /* eps_base = 1e-8; eps in [1e-8, 2e-8) for lb=0 */
    TEST_ASSERT_TRUE_MESSAGE(shift >= 1e-8, "shift at 1e-8 floor");
    TEST_ASSERT_TRUE_MESSAGE(shift < 2.5e-8,
        "shift must reflect 1e-8 eps_base floor");

    cxf_state_free(state);
    cxf_freemodel(model);
}

/**
 * Large feas_tol (1e-4) should clamp eps_base to 1e-6 ceiling.
 */
void test_large_tol_clamps_to_ceiling(void) {
    CxfModel *model = make_model();
    SolverState *state = NULL;
    cxf_simplex_init(model, &state);

    int n = state->num_vars;
    int m = state->num_constrs;
    int total = n + m;
    BasisState *basis = state->basis;

    setup_expand(state, basis, n, m, total);
    double saved_lb = state->saved_lb[n];

    /* feas_tol = 1e-4 -> feas_tol/10 = 1e-5 -> clamped to 1e-6 */
    int widened = cxf_expand_widen_bounds(state, env, 1e-4, basis, m, total);
    TEST_ASSERT_TRUE(widened > 0);

    double shift = saved_lb - state->work_lb[n];
    /* eps_base = 1e-6; eps in [1e-6, 2e-6) for lb=0 */
    TEST_ASSERT_TRUE_MESSAGE(shift >= 1e-6,
        "shift at 1e-6 ceiling");
    TEST_ASSERT_TRUE_MESSAGE(shift < 2.5e-6,
        "shift must reflect 1e-6 eps_base ceiling");

    cxf_state_free(state);
    cxf_freemodel(model);
}

/**
 * Mid-range feas_tol (1e-7) gives eps_base = 1e-8 (at floor).
 */
void test_mid_tol_scales_correctly(void) {
    CxfModel *model = make_model();
    SolverState *state = NULL;
    cxf_simplex_init(model, &state);

    int n = state->num_vars;
    int m = state->num_constrs;
    int total = n + m;
    BasisState *basis = state->basis;

    setup_expand(state, basis, n, m, total);
    double saved_lb = state->saved_lb[n];

    /* feas_tol = 1e-7 -> feas_tol/10 = 1e-8 -> exactly at floor */
    int widened = cxf_expand_widen_bounds(state, env, 1e-7, basis, m, total);
    TEST_ASSERT_TRUE(widened > 0);

    double shift = saved_lb - state->work_lb[n];
    TEST_ASSERT_TRUE_MESSAGE(shift >= 1e-8,
        "shift at 1e-8 for feas_tol=1e-7");
    TEST_ASSERT_TRUE_MESSAGE(shift < 2.5e-8,
        "shift must reflect 1e-8 eps_base");

    cxf_state_free(state);
    cxf_freemodel(model);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_default_tol_gives_mid_range);
    RUN_TEST(test_small_tol_clamps_to_floor);
    RUN_TEST(test_large_tol_clamps_to_ceiling);
    RUN_TEST(test_mid_tol_scales_correctly);
    return UNITY_END();
}
