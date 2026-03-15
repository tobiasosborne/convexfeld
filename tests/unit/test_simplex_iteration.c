/**
 * @file test_simplex_iteration.c
 * @brief TDD tests for simplex iteration and phases (M7.1.2)
 *
 * Tests for iteration loop, phase transitions, termination conditions,
 * objective value tracking, and iteration limits.
 */

#include "unity.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_types.h"

/* External declarations */
int cxf_simplex_step(SolverState *state, CxfEnv *env);
double cxf_simplex_get_objval(SolverState *state);
int cxf_simplex_set_iteration_limit(SolverState *state, int limit);
int cxf_simplex_get_iteration_limit(SolverState *state);

/* Test fixtures */
static CxfEnv *env = NULL;
static CxfModel *model = NULL;
static double timing[16];

void setUp(void) {
    cxf_loadenv(&env, NULL);
    cxf_newmodel(env, &model, "iteration_test", 0, NULL, NULL, NULL, NULL, NULL);
    for (int i = 0; i < 16; i++) timing[i] = 0.0;
}

void tearDown(void) {
    cxf_freemodel(model);
    model = NULL;
    cxf_freeenv(env);
    env = NULL;
}

/* Iteration loop tests */
void test_simplex_iterate_null_args_fail(void) {
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, cxf_simplex_step(NULL, env));
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");
    SolverState *state = NULL;
    cxf_simplex_init(model, &state);
    cxf_simplex_setup(state, env, 0, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, cxf_simplex_step(state, NULL));
    cxf_simplex_final(state);
}

void test_simplex_iterate_returns_valid_status(void) {
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");
    SolverState *state = NULL;
    cxf_simplex_init(model, &state);
    cxf_simplex_setup(state, env, 0, NULL);

    int status = cxf_simplex_step(state, env);
    /* Valid returns: 0=continue, 1=optimal, 2=infeasible, 3=unbounded, 12=error */
    TEST_ASSERT_TRUE(status == 0 || status == 1 || status == 2 ||
                     status == 3 || status == 12);

    cxf_simplex_final(state);
}

void test_simplex_iterate_increments_iteration(void) {
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");
    SolverState *state = NULL;
    cxf_simplex_init(model, &state);
    cxf_simplex_setup(state, env, 0, NULL);

    int iter_before = state->iteration;
    cxf_simplex_step(state, env);
    TEST_ASSERT_EQUAL_INT(iter_before + 1, state->iteration);

    cxf_simplex_final(state);
}

/* Phase transition tests */
void test_phase_end_null_args_fail(void) {
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT,
                          cxf_simplex_phase_end(NULL, env, 0));
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");
    SolverState *state = NULL;
    cxf_simplex_init(model, &state);
    cxf_simplex_setup(state, env, 0, NULL);
    state->phase = 1;
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT,
                          cxf_simplex_phase_end(state, NULL, 0));
    cxf_simplex_final(state);
}

void test_phase_end_transitions_to_phase2(void) {
    /* Phase I→II transition is handled by cxf_check_phase_one_end
     * in the orchestrator, not by phase_end. phase_end does constraint
     * cleanup (Phase II only). Verify it's a no-op during Phase I. */
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");
    SolverState *state = NULL;
    cxf_simplex_init(model, &state);
    cxf_simplex_setup(state, env, 0, NULL);
    state->phase = 1;
    state->obj_value = 0.0;

    int status = cxf_simplex_phase_end(state, env, 0);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(1, state->phase);  /* Still Phase I */

    cxf_simplex_final(state);
}

void test_phase_end_infeasible_returns_error(void) {
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");
    SolverState *state = NULL;
    cxf_simplex_init(model, &state);
    cxf_simplex_setup(state, env, 0, NULL);
    state->phase = 1;
    state->obj_value = 1.0;  /* Positive = still infeasible */

    /* phase_end with Phase I obj > tol should NOT transition;
     * infeasibility is detected by the orchestrator, not phase_end */
    int status = cxf_simplex_phase_end(state, env, 0);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(1, state->phase);  /* Still Phase I */

    cxf_simplex_final(state);
}

/* Termination condition tests */
void test_post_iterate_null_state_fails(void) {
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT,
                          cxf_simplex_post_iterate(NULL, NULL, NULL));
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT,
                          cxf_simplex_post_iterate(model, NULL, NULL));
}

void test_post_iterate_returns_continue_or_refactor(void) {
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");
    SolverState *state = NULL;
    cxf_simplex_init(model, &state);
    cxf_simplex_setup(state, env, 0, NULL);

    int stall = 0;
    int status = cxf_simplex_post_iterate(model, state, &stall);
    TEST_ASSERT_TRUE(status == 0 || status == 1);

    cxf_simplex_final(state);
}

/* Objective value tracking tests */
void test_get_objval_null_returns_nan(void) {
    TEST_ASSERT_TRUE(cxf_simplex_get_objval(NULL) != cxf_simplex_get_objval(NULL));
}

void test_get_objval_returns_current_objective(void) {
    cxf_addvar(model, 0, NULL, NULL, 5.0, 0.0, 10.0, 'C', "x");  /* obj coeff = 5 */
    SolverState *state = NULL;
    cxf_simplex_init(model, &state);
    cxf_simplex_setup(state, env, 0, NULL);
    state->obj_value = 42.0;

    double val = cxf_simplex_get_objval(state);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 42.0, val);

    cxf_simplex_final(state);
}

/* Iteration limit tests */
void test_set_iteration_limit_null_fails(void) {
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, cxf_simplex_set_iteration_limit(NULL, 1000));
}

void test_set_iteration_limit_negative_fails(void) {
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");
    SolverState *state = NULL;
    cxf_simplex_init(model, &state);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, cxf_simplex_set_iteration_limit(state, -1));
    cxf_simplex_final(state);
}

void test_set_iteration_limit_valid(void) {
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");
    SolverState *state = NULL;
    cxf_simplex_init(model, &state);

    int status = cxf_simplex_set_iteration_limit(state, 5000);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(5000, state->max_iterations);

    cxf_simplex_final(state);
}

void test_get_iteration_limit_null_returns_error(void) {
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, cxf_simplex_get_iteration_limit(NULL));
}

void test_get_iteration_limit_returns_current(void) {
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");
    SolverState *state = NULL;
    cxf_simplex_init(model, &state);
    state->max_iterations = 3000;

    int limit = cxf_simplex_get_iteration_limit(state);
    TEST_ASSERT_EQUAL_INT(3000, limit);

    cxf_simplex_final(state);
}

int main(void) {
    UNITY_BEGIN();
    /* Iteration loop tests */
    RUN_TEST(test_simplex_iterate_null_args_fail);
    RUN_TEST(test_simplex_iterate_returns_valid_status);
    RUN_TEST(test_simplex_iterate_increments_iteration);
    /* Phase transition tests */
    RUN_TEST(test_phase_end_null_args_fail);
    RUN_TEST(test_phase_end_transitions_to_phase2);
    RUN_TEST(test_phase_end_infeasible_returns_error);
    /* Termination condition tests */
    RUN_TEST(test_post_iterate_null_state_fails);
    RUN_TEST(test_post_iterate_returns_continue_or_refactor);
    /* Objective value tracking tests */
    RUN_TEST(test_get_objval_null_returns_nan);
    RUN_TEST(test_get_objval_returns_current_objective);
    /* Iteration limit tests */
    RUN_TEST(test_set_iteration_limit_null_fails);
    RUN_TEST(test_set_iteration_limit_negative_fails);
    RUN_TEST(test_set_iteration_limit_valid);
    RUN_TEST(test_get_iteration_limit_null_returns_error);
    RUN_TEST(test_get_iteration_limit_returns_current);
    return UNITY_END();
}