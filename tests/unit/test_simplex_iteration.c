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

/* External declarations - to be implemented in M7.1.x */
int cxf_simplex_iterate(SolverContext *state, CxfEnv *env);
int cxf_simplex_phase_end(SolverContext *state, CxfEnv *env);
int cxf_simplex_post_iterate(SolverContext *state, CxfEnv *env);
double cxf_simplex_get_objval(SolverContext *state);
int cxf_simplex_set_iteration_limit(SolverContext *state, int limit);
int cxf_simplex_get_iteration_limit(SolverContext *state);

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
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, cxf_simplex_iterate(NULL, env));
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");
    SolverContext *state = NULL;
    cxf_simplex_init(model, &state);
    cxf_simplex_setup(state, env);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, cxf_simplex_iterate(state, NULL));
    cxf_simplex_final(state);
}

void test_simplex_iterate_returns_valid_status(void) {
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");
    SolverContext *state = NULL;
    cxf_simplex_init(model, &state);
    cxf_simplex_setup(state, env);

    int status = cxf_simplex_iterate(state, env);
    /* Valid returns: 0=continue, 1=optimal, 2=infeasible, 3=unbounded, 12=error */
    TEST_ASSERT_TRUE(status == 0 || status == 1 || status == 2 ||
                     status == 3 || status == 12);

    cxf_simplex_final(state);
}

void test_simplex_iterate_increments_iteration(void) {
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");
    SolverContext *state = NULL;
    cxf_simplex_init(model, &state);
    cxf_simplex_setup(state, env);

    int iter_before = state->iteration;
    cxf_simplex_iterate(state, env);
    TEST_ASSERT_EQUAL_INT(iter_before + 1, state->iteration);

    cxf_simplex_final(state);
}

/* Phase transition tests */
void test_phase_end_null_args_fail(void) {
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, cxf_simplex_phase_end(NULL, env));
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");
    SolverContext *state = NULL;
    cxf_simplex_init(model, &state);
    cxf_simplex_setup(state, env);
    state->phase = 1;
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, cxf_simplex_phase_end(state, NULL));
    cxf_simplex_final(state);
}

void test_phase_end_transitions_to_phase2(void) {
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");
    SolverContext *state = NULL;
    cxf_simplex_init(model, &state);
    cxf_simplex_setup(state, env);
    state->phase = 1;
    state->obj_value = 0.0;  /* Feasible end of Phase I */

    int status = cxf_simplex_phase_end(state, env);
    TEST_ASSERT_EQUAL_INT(0, status);  /* 0 = transition to Phase II */
    TEST_ASSERT_EQUAL_INT(2, state->phase);

    cxf_simplex_final(state);
}

void test_phase_end_infeasible_returns_error(void) {
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");
    SolverContext *state = NULL;
    cxf_simplex_init(model, &state);
    cxf_simplex_setup(state, env);
    state->phase = 1;
    state->obj_value = 1.0;  /* Positive infeasibility = no feasible solution */

    int status = cxf_simplex_phase_end(state, env);
    TEST_ASSERT_EQUAL_INT(2, status);  /* 2 = infeasible */

    cxf_simplex_final(state);
}

/* Termination condition tests */
void test_post_iterate_null_state_fails(void) {
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, cxf_simplex_post_iterate(NULL, env));
}

void test_post_iterate_returns_continue_or_refactor(void) {
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");
    SolverContext *state = NULL;
    cxf_simplex_init(model, &state);
    cxf_simplex_setup(state, env);

    int status = cxf_simplex_post_iterate(state, env);
    /* 0 = continue, 1 = refactor triggered */
    TEST_ASSERT_TRUE(status == 0 || status == 1);

    cxf_simplex_final(state);
}

/* Objective value tracking tests */
void test_get_objval_null_returns_nan(void) {
    TEST_ASSERT_TRUE(cxf_simplex_get_objval(NULL) != cxf_simplex_get_objval(NULL));
}

void test_get_objval_returns_current_objective(void) {
    cxf_addvar(model, 0, NULL, NULL, 5.0, 0.0, 10.0, 'C', "x");  /* obj coeff = 5 */
    SolverContext *state = NULL;
    cxf_simplex_init(model, &state);
    cxf_simplex_setup(state, env);
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
    SolverContext *state = NULL;
    cxf_simplex_init(model, &state);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, cxf_simplex_set_iteration_limit(state, -1));
    cxf_simplex_final(state);
}

void test_set_iteration_limit_valid(void) {
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");
    SolverContext *state = NULL;
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
    SolverContext *state = NULL;
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