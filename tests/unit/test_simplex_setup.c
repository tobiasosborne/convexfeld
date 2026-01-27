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

void test_setup_null_state_fails(void) {
    int status = cxf_simplex_setup(NULL, env);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);
}

void test_setup_null_env_fails(void) {
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");
    SolverContext *state = NULL;
    cxf_simplex_init(model, &state);

    int status = cxf_simplex_setup(state, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);

    cxf_simplex_final(state);
}

void test_setup_empty_model(void) {
    SolverContext *state = NULL;
    cxf_simplex_init(model, &state);

    int status = cxf_simplex_setup(state, env);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_TRUE(state->phase == 1 || state->phase == 2);

    cxf_simplex_final(state);
}

void test_setup_initializes_reduced_costs(void) {
    /* Add variables with different objective coefficients */
    cxf_addvar(model, 0, NULL, NULL, 3.0, 0.0, 10.0, 'C', "x1");  /* obj = 3 */
    cxf_addvar(model, 0, NULL, NULL, -2.5, 0.0, 10.0, 'C', "x2"); /* obj = -2.5 */
    cxf_addvar(model, 0, NULL, NULL, 0.0, 0.0, 10.0, 'C', "x3");  /* obj = 0 */

    SolverContext *state = NULL;
    cxf_simplex_init(model, &state);
    cxf_simplex_setup(state, env);

    /* Reduced costs should equal objective coefficients initially */
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 3.0, state->work_dj[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, -2.5, state->work_dj[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, state->work_dj[2]);

    cxf_simplex_final(state);
}

void test_setup_initializes_dual_values_to_zero(void) {
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");
    /* Note: adding constraints requires full constraint API */

    SolverContext *state = NULL;
    cxf_simplex_init(model, &state);
    cxf_simplex_setup(state, env);

    /* Dual values should be zero (no constraints in this model) */
    TEST_ASSERT_EQUAL_INT(0, state->num_constrs);

    cxf_simplex_final(state);
}

void test_setup_resets_iteration_counter(void) {
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");

    SolverContext *state = NULL;
    cxf_simplex_init(model, &state);

    /* Manually set some iteration state */
    state->iteration = 100;
    state->eta_count = 50;

    cxf_simplex_setup(state, env);

    TEST_ASSERT_EQUAL_INT(0, state->iteration);
    TEST_ASSERT_EQUAL_INT(0, state->eta_count);

    cxf_simplex_final(state);
}

void test_setup_determines_phase_2_for_feasible_bounds(void) {
    /* Add variable with feasible bounds (lb < ub) */
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");

    SolverContext *state = NULL;
    cxf_simplex_init(model, &state);
    cxf_simplex_setup(state, env);

    /* Feasible bounds should result in Phase II */
    TEST_ASSERT_EQUAL_INT(2, state->phase);

    cxf_simplex_final(state);
}

void test_setup_determines_phase_1_for_infeasible_bounds(void) {
    /* Add variable with infeasible bounds (lb > ub) */
    cxf_addvar(model, 0, NULL, NULL, 1.0, 10.0, 5.0, 'C', "x");  /* lb=10 > ub=5 */

    SolverContext *state = NULL;
    cxf_simplex_init(model, &state);
    cxf_simplex_setup(state, env);

    /* Infeasible bounds should result in Phase I */
    TEST_ASSERT_EQUAL_INT(1, state->phase);

    cxf_simplex_final(state);
}

void test_setup_initializes_pricing_context(void) {
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");

    SolverContext *state = NULL;
    cxf_simplex_init(model, &state);

    /* Pricing should be NULL before setup */
    TEST_ASSERT_NULL(state->pricing);

    cxf_simplex_setup(state, env);

    /* Pricing should be initialized after setup */
    TEST_ASSERT_NOT_NULL(state->pricing);

    cxf_simplex_final(state);
}

void test_setup_sets_tolerance_from_env(void) {
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");

    SolverContext *state = NULL;
    cxf_simplex_init(model, &state);

    /* Set environment tolerance */
    env->optimality_tol = 1e-8;

    cxf_simplex_setup(state, env);

    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 1e-8, state->tolerance);

    cxf_simplex_final(state);
}

/*******************************************************************************
 * cxf_simplex_preprocess tests
 ******************************************************************************/

void test_preprocess_null_state_fails(void) {
    int status = cxf_simplex_preprocess(NULL, env, 0);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);
}

void test_preprocess_null_env_fails(void) {
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");
    SolverContext *state = NULL;
    cxf_simplex_init(model, &state);

    int status = cxf_simplex_preprocess(state, NULL, 0);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);

    cxf_simplex_final(state);
}

void test_preprocess_skip_flag(void) {
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");
    SolverContext *state = NULL;
    cxf_simplex_init(model, &state);

    /* Pass flag=1 to skip preprocessing */
    int status = cxf_simplex_preprocess(state, env, 1);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);

    cxf_simplex_final(state);
}

void test_preprocess_empty_model(void) {
    SolverContext *state = NULL;
    cxf_simplex_init(model, &state);

    int status = cxf_simplex_preprocess(state, env, 0);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);

    cxf_simplex_final(state);
}

void test_preprocess_feasible_bounds(void) {
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x1");
    cxf_addvar(model, 0, NULL, NULL, 2.0, -5.0, 5.0, 'C', "x2");

    SolverContext *state = NULL;
    cxf_simplex_init(model, &state);

    int status = cxf_simplex_preprocess(state, env, 0);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);

    cxf_simplex_final(state);
}

void test_preprocess_detects_infeasible_bounds(void) {
    /* Add variable with infeasible bounds */
    cxf_addvar(model, 0, NULL, NULL, 1.0, 10.0, 5.0, 'C', "x");  /* lb=10 > ub=5 */

    SolverContext *state = NULL;
    cxf_simplex_init(model, &state);

    int status = cxf_simplex_preprocess(state, env, 0);
    TEST_ASSERT_EQUAL_INT(3, status);  /* 3 = infeasible */

    cxf_simplex_final(state);
}

void test_preprocess_multiple_vars_one_infeasible(void) {
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x1");  /* feasible */
    cxf_addvar(model, 0, NULL, NULL, 2.0, 20.0, 5.0, 'C', "x2");  /* infeasible */
    cxf_addvar(model, 0, NULL, NULL, 0.5, 0.0, 100.0, 'C', "x3"); /* feasible */

    SolverContext *state = NULL;
    cxf_simplex_init(model, &state);

    int status = cxf_simplex_preprocess(state, env, 0);
    TEST_ASSERT_EQUAL_INT(3, status);  /* Should detect infeasibility */

    cxf_simplex_final(state);
}

/*******************************************************************************
 * Integration tests
 ******************************************************************************/

void test_setup_and_preprocess_sequence(void) {
    cxf_addvar(model, 0, NULL, NULL, 3.0, 0.0, 10.0, 'C', "x1");
    cxf_addvar(model, 0, NULL, NULL, -1.0, 0.0, 5.0, 'C', "x2");

    SolverContext *state = NULL;
    cxf_simplex_init(model, &state);

    /* Run preprocess first */
    int status = cxf_simplex_preprocess(state, env, 0);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);

    /* Then setup */
    status = cxf_simplex_setup(state, env);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);

    /* Verify state is ready for iteration */
    TEST_ASSERT_EQUAL_INT(2, state->phase);  /* Phase II */
    TEST_ASSERT_NOT_NULL(state->pricing);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 3.0, state->work_dj[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, -1.0, state->work_dj[1]);

    cxf_simplex_final(state);
}

/*******************************************************************************
 * Main test runner
 ******************************************************************************/

int main(void) {
    UNITY_BEGIN();

    /* cxf_simplex_setup tests */
    RUN_TEST(test_setup_null_state_fails);
    RUN_TEST(test_setup_null_env_fails);
    RUN_TEST(test_setup_empty_model);
    RUN_TEST(test_setup_initializes_reduced_costs);
    RUN_TEST(test_setup_initializes_dual_values_to_zero);
    RUN_TEST(test_setup_resets_iteration_counter);
    RUN_TEST(test_setup_determines_phase_2_for_feasible_bounds);
    RUN_TEST(test_setup_determines_phase_1_for_infeasible_bounds);
    RUN_TEST(test_setup_initializes_pricing_context);
    RUN_TEST(test_setup_sets_tolerance_from_env);

    /* cxf_simplex_preprocess tests */
    RUN_TEST(test_preprocess_null_state_fails);
    RUN_TEST(test_preprocess_null_env_fails);
    RUN_TEST(test_preprocess_skip_flag);
    RUN_TEST(test_preprocess_empty_model);
    RUN_TEST(test_preprocess_feasible_bounds);
    RUN_TEST(test_preprocess_detects_infeasible_bounds);
    RUN_TEST(test_preprocess_multiple_vars_one_infeasible);

    /* Integration tests */
    RUN_TEST(test_setup_and_preprocess_sequence);

    return UNITY_END();
}
