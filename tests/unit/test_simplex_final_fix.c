/**
 * @file test_simplex_final_fix.c
 * @brief Tests for cxf_simplex_final dual-feasibility variable fixing.
 *
 * Verifies the 5-phase spec behavior per simplex_lifecycle.md:
 *   Phase 1: Target value from reduced costs
 *   Phase 2: Equality constraint verification
 *   Phase 3-4: Activity propagation + feasibility
 *   Phase 5: Apply fixings via cxf_pivot_bound
 */

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_types.h"

/* Phase 1: positive RC → target = lower bound */
static void test_phase1_positive_rc(void) {
    CxfModel model = {0};
    CxfEnv env = {0};
    model.env = &env;
    model.num_vars = 2;
    model.num_constrs = 1;
    double lb[] = {0, 0, 0};
    double ub[] = {10, 10, CXF_INFINITY};
    double obj[] = {1, 2, 0};
    model.lb = lb; model.ub = ub; model.obj_coeffs = obj;

    SolverState *state = NULL;
    int rc = cxf_simplex_init(&model, &state);
    assert(rc == CXF_OK && state != NULL);

    /* Set up reduced costs: dj[0]=0.5 (positive → target=lb) */
    state->work_dj[0] = 0.5;
    state->work_dj[1] = -0.3;
    state->work_x[0] = 5.0;
    state->work_x[1] = 7.0;
    state->basis->var_status[0] = CXF_VAR_AT_LOWER;
    state->basis->var_status[1] = CXF_VAR_AT_UPPER;
    state->basis->var_status[2] = 0; /* basic */

    env.optimality_tol = 1e-6;
    env.feasibility_tol = 1e-6;

    rc = cxf_simplex_final(state, &env, NULL);
    assert(rc == CXF_OK);

    /* After fixing: var 0 at lb (0), var 1 at ub (10) */
    assert(fabs(state->work_x[0] - 0.0) < 1e-12);
    assert(fabs(state->work_x[1] - 10.0) < 1e-12);
    /* Status should be FIXED */
    assert(state->basis->var_status[0] == CXF_VAR_FIXED);
    assert(state->basis->var_status[1] == CXF_VAR_FIXED);

    cxf_state_free(state);
}

/* Phase 1: abort on unbounded variable with nonzero RC */
static void test_phase1_abort_unbounded(void) {
    CxfModel model = {0};
    CxfEnv env = {0};
    model.env = &env;
    model.num_vars = 1;
    model.num_constrs = 1;
    double lb[] = {-CXF_INFINITY, 0};
    double ub[] = {CXF_INFINITY, CXF_INFINITY};
    double obj[] = {1, 0};
    model.lb = lb; model.ub = ub; model.obj_coeffs = obj;

    SolverState *state = NULL;
    int rc = cxf_simplex_init(&model, &state);
    assert(rc == CXF_OK);

    /* Positive RC but lb = -inf → should abort without fixing */
    state->work_dj[0] = 1.0;
    state->work_x[0] = 5.0;
    state->basis->var_status[0] = CXF_VAR_AT_LOWER;
    state->basis->var_status[1] = 0; /* basic */

    env.optimality_tol = 1e-6;
    env.feasibility_tol = 1e-6;

    rc = cxf_simplex_final(state, &env, NULL);
    assert(rc == CXF_OK);
    /* x should NOT be changed (abort early) */
    assert(fabs(state->work_x[0] - 5.0) < 1e-12);

    cxf_state_free(state);
}

/* NULL inputs handled gracefully */
static void test_null_inputs(void) {
    CxfEnv env = {0};
    assert(cxf_simplex_final(NULL, &env, NULL) == CXF_OK);
    assert(cxf_simplex_final(NULL, NULL, NULL) == CXF_OK);
}

/* Zero RC → no fixing */
static void test_zero_rc_no_fix(void) {
    CxfModel model = {0};
    CxfEnv env = {0};
    model.env = &env;
    model.num_vars = 1;
    model.num_constrs = 1;
    double lb[] = {0, 0};
    double ub[] = {10, CXF_INFINITY};
    double obj[] = {1, 0};
    model.lb = lb; model.ub = ub; model.obj_coeffs = obj;

    SolverState *state = NULL;
    int rc = cxf_simplex_init(&model, &state);
    assert(rc == CXF_OK);

    state->work_dj[0] = 0.0; /* zero RC */
    state->work_x[0] = 5.0;
    state->basis->var_status[0] = CXF_VAR_AT_LOWER;
    state->basis->var_status[1] = 0;

    env.optimality_tol = 1e-6;
    env.feasibility_tol = 1e-6;

    rc = cxf_simplex_final(state, &env, NULL);
    assert(rc == CXF_OK);
    /* x unchanged — zero RC means no fixing */
    assert(fabs(state->work_x[0] - 5.0) < 1e-12);

    cxf_state_free(state);
}

int main(void) {
    test_phase1_positive_rc();
    test_phase1_abort_unbounded();
    test_null_inputs();
    test_zero_rc_no_fix();
    return 0;
}
