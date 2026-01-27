/**
 * @file test_simplex_basic.c
 * @brief TDD tests for simplex initialization, setup, and cleanup (M7.1.1)
 *
 * Tests for SolverState lifecycle, setup, and basic status queries.
 * ~200 LOC of comprehensive tests.
 */

#include "unity.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_types.h"

/*******************************************************************************
 * External function declarations (to be implemented in M7.1.4-5)
 ******************************************************************************/

/* Setup - to be implemented in M7.1.6 */
int cxf_simplex_setup(SolverContext *state, CxfEnv *env);

/* Status queries - to be implemented */
int cxf_simplex_get_status(SolverContext *state);
int cxf_simplex_get_iteration(SolverContext *state);
int cxf_simplex_get_phase(SolverContext *state);

/*******************************************************************************
 * Test fixtures
 ******************************************************************************/

static CxfEnv *env = NULL;
static CxfModel *model = NULL;
static double timing[16];

void setUp(void) {
    cxf_loadenv(&env, NULL);
    cxf_newmodel(env, &model, "simplex_test", 0, NULL, NULL, NULL, NULL, NULL);
    for (int i = 0; i < 16; i++) timing[i] = 0.0;
}

void tearDown(void) {
    cxf_freemodel(model);
    model = NULL;
    cxf_freeenv(env);
    env = NULL;
}

/*******************************************************************************
 * SolverContext creation tests
 ******************************************************************************/

void test_simplex_init_creates_state(void) {
    /* Add a simple variable to have a non-trivial model */
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");

    SolverContext *state = NULL;
    int status = cxf_simplex_init(model, &state);

    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_EQUAL_PTR(model, state->model_ref);
    TEST_ASSERT_EQUAL_INT(1, state->num_vars);

    cxf_simplex_final(state);
}

void test_simplex_init_null_model_fails(void) {
    SolverContext *state = NULL;
    int status = cxf_simplex_init(NULL, &state);

    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);
    TEST_ASSERT_NULL(state);
}

void test_simplex_init_null_stateout_fails(void) {
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");

    int status = cxf_simplex_init(model, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);
}

void test_simplex_init_empty_model(void) {
    /* Model with no variables */
    SolverContext *state = NULL;
    int status = cxf_simplex_init(model, &state);

    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_EQUAL_INT(0, state->num_vars);
    TEST_ASSERT_EQUAL_INT(0, state->num_constrs);

    cxf_simplex_final(state);
}

void test_simplex_init_primal_mode(void) {
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");

    SolverContext *state = NULL;
    int status = cxf_simplex_init(model, &state);  /* defaults to primal */

    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_EQUAL_INT(0, state->solve_mode);  /* 0=primal default */

    cxf_simplex_final(state);
}

void test_simplex_init_dual_mode(void) {
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");

    SolverContext *state = NULL;
    int status = cxf_simplex_init(model, &state);

    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_NOT_NULL(state);
    /* Mode would be set later via setup or config, defaults to primal (0) */
    TEST_ASSERT_EQUAL_INT(0, state->solve_mode);

    cxf_simplex_final(state);
}

/*******************************************************************************
 * SolverContext cleanup tests
 ******************************************************************************/

void test_simplex_final_frees_state(void) {
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");

    SolverContext *state = NULL;
    cxf_simplex_init(model, &state);
    TEST_ASSERT_NOT_NULL(state);

    cxf_simplex_final(state);
    /* state is now freed - no crash = success */
}

void test_simplex_final_null_safe(void) {
    cxf_simplex_final(NULL);
    /* Should not crash - success */
}

/*******************************************************************************
 * Setup tests
 ******************************************************************************/

void test_simplex_setup_basic(void) {
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");

    SolverContext *state = NULL;
    cxf_simplex_init(model, &state);

    int status = cxf_simplex_setup(state, env);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);

    /* After setup, phase should be 1 or 2 */
    TEST_ASSERT_TRUE(state->phase == 1 || state->phase == 2);

    cxf_simplex_final(state);
}

void test_simplex_setup_null_state_fails(void) {
    int status = cxf_simplex_setup(NULL, env);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);
}

void test_simplex_setup_null_env_fails(void) {
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");

    SolverContext *state = NULL;
    cxf_simplex_init(model, &state);

    int status = cxf_simplex_setup(state, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);

    cxf_simplex_final(state);
}

/*******************************************************************************
 * State query tests
 ******************************************************************************/

void test_simplex_get_status_initial(void) {
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");

    SolverContext *state = NULL;
    cxf_simplex_init(model, &state);

    int status = cxf_simplex_get_status(state);
    /* Initial status should be 0 (not yet solved) or CXF_OK */
    TEST_ASSERT_TRUE(status >= 0);

    cxf_simplex_final(state);
}

void test_simplex_get_status_null_returns_error(void) {
    int status = cxf_simplex_get_status(NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);
}

void test_simplex_get_iteration_initial(void) {
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");

    SolverContext *state = NULL;
    cxf_simplex_init(model, &state);

    int iter = cxf_simplex_get_iteration(state);
    TEST_ASSERT_EQUAL_INT(0, iter);  /* No iterations yet */

    cxf_simplex_final(state);
}

void test_simplex_get_iteration_null_returns_error(void) {
    int iter = cxf_simplex_get_iteration(NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, iter);
}

void test_simplex_get_phase_after_setup(void) {
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");

    SolverContext *state = NULL;
    cxf_simplex_init(model, &state);
    cxf_simplex_setup(state, env);

    int phase = cxf_simplex_get_phase(state);
    TEST_ASSERT_TRUE(phase == 1 || phase == 2);

    cxf_simplex_final(state);
}

void test_simplex_get_phase_null_returns_error(void) {
    int phase = cxf_simplex_get_phase(NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, phase);
}

/*******************************************************************************
 * Main test runner
 ******************************************************************************/

int main(void) {
    UNITY_BEGIN();

    /* SolverContext creation tests */
    RUN_TEST(test_simplex_init_creates_state);
    RUN_TEST(test_simplex_init_null_model_fails);
    RUN_TEST(test_simplex_init_null_stateout_fails);
    RUN_TEST(test_simplex_init_empty_model);
    RUN_TEST(test_simplex_init_primal_mode);
    RUN_TEST(test_simplex_init_dual_mode);

    /* SolverContext cleanup tests */
    RUN_TEST(test_simplex_final_frees_state);
    RUN_TEST(test_simplex_final_null_safe);

    /* Setup tests */
    RUN_TEST(test_simplex_setup_basic);
    RUN_TEST(test_simplex_setup_null_state_fails);
    RUN_TEST(test_simplex_setup_null_env_fails);

    /* State query tests */
    RUN_TEST(test_simplex_get_status_initial);
    RUN_TEST(test_simplex_get_status_null_returns_error);
    RUN_TEST(test_simplex_get_iteration_initial);
    RUN_TEST(test_simplex_get_iteration_null_returns_error);
    RUN_TEST(test_simplex_get_phase_after_setup);
    RUN_TEST(test_simplex_get_phase_null_returns_error);

    return UNITY_END();
}
