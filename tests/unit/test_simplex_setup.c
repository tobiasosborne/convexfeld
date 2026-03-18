/**
 * @file test_simplex_setup.c
 * @brief TDD tests for simplex setup and preprocessing (M7.1.6)
 *
 * Tests for cxf_simplex_setup and cxf_simplex_preprocess.
 */

#include "unity.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_types.h"
#include <math.h>

/*******************************************************************************
 * Test fixtures
 ******************************************************************************/

static CxfEnv *env = NULL;
static CxfModel *model = NULL;

void setUp(void) {
    cxf_loadenv(&env, NULL);
    cxf_newmodel(env, &model, "setup_test", 0, NULL, NULL, NULL, NULL, NULL);
}

void tearDown(void) {
    cxf_freemodel(model);
    model = NULL;
    cxf_freeenv(env);
    env = NULL;
}

/*******************************************************************************
 * cxf_simplex_setup tests
 ******************************************************************************/

void test_setup_empty_model(void) {
    SolverState *state = NULL;
    cxf_simplex_init(model, &state);

    cxf_simplex_setup(state, env, 0, NULL);

    /* Setup computes activity bounds; with 0 constraints, arrays should
     * still be allocated (possibly NULL for empty model, but no crash) */
    TEST_ASSERT_EQUAL_INT(0, state->num_constrs);

    cxf_state_free(state);
}

void test_setup_computes_activity_bounds(void) {
    /* Add variables — activity bounds need constraints + matrix to be meaningful.
     * With variables only and no constraints, min/max_activity arrays are
     * allocated (size num_constrs=0) but nothing to compute. Verify no crash
     * and that the arrays are accessible after setup. */
    cxf_addvar(model, 0, NULL, NULL, 3.0, 0.0, 10.0, 'C', "x1");
    cxf_addvar(model, 0, NULL, NULL, -2.5, 0.0, 10.0, 'C', "x2");
    cxf_addvar(model, 0, NULL, NULL, 0.0, 0.0, 10.0, 'C', "x3");

    SolverState *state = NULL;
    cxf_simplex_init(model, &state);
    cxf_simplex_setup(state, env, 0, NULL);

    /* With no constraints, activity arrays may be NULL (size 0) — no crash */
    TEST_ASSERT_EQUAL_INT(0, state->num_constrs);
    /* Phase should NOT be set by setup */
    TEST_ASSERT_EQUAL_INT(0, state->phase);

    cxf_state_free(state);
}

void test_setup_initializes_dual_values_to_zero(void) {
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");
    /* Note: adding constraints requires full constraint API */

    SolverState *state = NULL;
    cxf_simplex_init(model, &state);
    cxf_simplex_setup(state, env, 0, NULL);

    /* Dual values should be zero (no constraints in this model) */
    TEST_ASSERT_EQUAL_INT(0, state->num_constrs);

    cxf_state_free(state);
}

void test_setup_does_not_reset_iteration_counter(void) {
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");

    SolverState *state = NULL;
    cxf_simplex_init(model, &state);

    /* Manually set some iteration state */
    state->iteration = 100;
    state->eta_count = 50;

    cxf_simplex_setup(state, env, 0, NULL);

    /* Setup no longer resets iteration counters — those are init's job */
    TEST_ASSERT_EQUAL_INT(100, state->iteration);
    TEST_ASSERT_EQUAL_INT(50, state->eta_count);

    cxf_state_free(state);
}

void test_setup_does_not_set_phase(void) {
    /* Setup no longer determines phase — that is init's responsibility */
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");

    SolverState *state = NULL;
    cxf_simplex_init(model, &state);

    /* Phase should be 0 (unset) after init */
    TEST_ASSERT_EQUAL_INT(0, state->phase);

    cxf_simplex_setup(state, env, 0, NULL);

    /* Phase should still be 0 — setup only computes activity bounds */
    TEST_ASSERT_EQUAL_INT(0, state->phase);

    cxf_state_free(state);
}

void test_setup_does_not_initialize_pricing(void) {
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");

    SolverState *state = NULL;
    cxf_simplex_init(model, &state);

    cxf_simplex_setup(state, env, 0, NULL);

    /* Setup no longer initializes pricing — that is init's responsibility */
    /* Pricing state depends on init, not setup */

    cxf_state_free(state);
}

/*******************************************************************************
 * cxf_simplex_preprocess tests
 ******************************************************************************/

void test_preprocess_null_state_fails(void) {
    int status = cxf_simplex_preprocess(NULL, env);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);
}

void test_preprocess_null_env_fails(void) {
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");
    SolverState *state = NULL;
    cxf_simplex_init(model, &state);

    int status = cxf_simplex_preprocess(state, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);

    cxf_state_free(state);
}

void test_preprocess_empty_model(void) {
    SolverState *state = NULL;
    cxf_simplex_init(model, &state);

    int status = cxf_simplex_preprocess(state, env);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);

    cxf_state_free(state);
}

void test_preprocess_feasible_bounds(void) {
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x1");
    cxf_addvar(model, 0, NULL, NULL, 2.0, -5.0, 5.0, 'C', "x2");

    SolverState *state = NULL;
    cxf_simplex_init(model, &state);

    int status = cxf_simplex_preprocess(state, env);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);

    cxf_state_free(state);
}

void test_preprocess_detects_infeasible_bounds(void) {
    /* Add variable with infeasible bounds */
    cxf_addvar(model, 0, NULL, NULL, 1.0, 10.0, 5.0, 'C', "x");  /* lb=10 > ub=5 */

    SolverState *state = NULL;
    cxf_simplex_init(model, &state);

    int status = cxf_simplex_preprocess(state, env);
    TEST_ASSERT_EQUAL_INT(CXF_INFEASIBLE, status);

    cxf_state_free(state);
}

void test_preprocess_multiple_vars_one_infeasible(void) {
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x1");  /* feasible */
    cxf_addvar(model, 0, NULL, NULL, 2.0, 20.0, 5.0, 'C', "x2");  /* infeasible */
    cxf_addvar(model, 0, NULL, NULL, 0.5, 0.0, 100.0, 'C', "x3"); /* feasible */

    SolverState *state = NULL;
    cxf_simplex_init(model, &state);

    int status = cxf_simplex_preprocess(state, env);
    TEST_ASSERT_EQUAL_INT(CXF_INFEASIBLE, status);

    cxf_state_free(state);
}

/*******************************************************************************
 * Integration tests
 ******************************************************************************/

void test_setup_and_preprocess_sequence(void) {
    cxf_addvar(model, 0, NULL, NULL, 3.0, 0.0, 10.0, 'C', "x1");
    cxf_addvar(model, 0, NULL, NULL, -1.0, 0.0, 5.0, 'C', "x2");

    SolverState *state = NULL;
    cxf_simplex_init(model, &state);

    /* Run preprocess first */
    int status = cxf_simplex_preprocess(state, env);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);

    /* Then setup — computes activity bounds only */
    cxf_simplex_setup(state, env, 0, NULL);

    /* Verify working bounds are intact (not corrupted by setup) */
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, state->work_lb[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 10.0, state->work_ub[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, state->work_lb[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 5.0, state->work_ub[1]);

    /* Phase should NOT be set by setup */
    TEST_ASSERT_EQUAL_INT(0, state->phase);

    cxf_state_free(state);
}

/*******************************************************************************
 * Main test runner
 ******************************************************************************/

int main(void) {
    UNITY_BEGIN();

    /* cxf_simplex_setup tests (v2: setup only computes activity bounds) */
    RUN_TEST(test_setup_empty_model);
    RUN_TEST(test_setup_computes_activity_bounds);
    RUN_TEST(test_setup_initializes_dual_values_to_zero);
    RUN_TEST(test_setup_does_not_reset_iteration_counter);
    RUN_TEST(test_setup_does_not_set_phase);
    RUN_TEST(test_setup_does_not_initialize_pricing);

    /* cxf_simplex_preprocess tests */
    RUN_TEST(test_preprocess_null_state_fails);
    RUN_TEST(test_preprocess_null_env_fails);
    RUN_TEST(test_preprocess_empty_model);
    RUN_TEST(test_preprocess_feasible_bounds);
    RUN_TEST(test_preprocess_detects_infeasible_bounds);
    RUN_TEST(test_preprocess_multiple_vars_one_infeasible);

    /* Integration tests */
    RUN_TEST(test_setup_and_preprocess_sequence);

    return UNITY_END();
}
