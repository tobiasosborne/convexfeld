/**
 * @file test_post_iterate_sig.c
 * @brief V2 spec compliance: cxf_simplex_post_iterate signature
 *
 * V2 simplex_iteration.md specifies:
 *   cxf_simplex_post_iterate(CxfModel*, SolverState*, int*) -> int
 *
 * Previously: (SolverState*, CxfEnv*, int*) -- wrong first param.
 * Issue: convexfeld-mfjq
 */

#include "unity.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_types.h"

static CxfEnv *env = NULL;
static CxfModel *model = NULL;

void setUp(void) {
    cxf_loadenv(&env, NULL);
    cxf_newmodel(env, &model, "post_sig", 0,
                 NULL, NULL, NULL, NULL, NULL);
}

void tearDown(void) {
    cxf_freemodel(model); model = NULL;
    cxf_freeenv(env);     env = NULL;
}

/* V2 spec: first param is CxfModel*, not CxfEnv* */
void test_post_iterate_takes_model(void) {
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");
    SolverState *state = NULL;
    cxf_simplex_init(model, &state);
    cxf_simplex_setup(state, env, 0, NULL);

    int stall = -1;
    int rc = cxf_simplex_post_iterate(model, state, &stall);
    TEST_ASSERT_TRUE(rc == 0 || rc == CXF_ITERATION_LIMIT);
    TEST_ASSERT_TRUE(stall == 0 || stall == 1 || stall == -1);

    cxf_state_free(state);
}

/* V2 spec: null model returns CXF_ERROR_NULL_ARGUMENT */
void test_post_iterate_null_model(void) {
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");
    SolverState *state = NULL;
    cxf_simplex_init(model, &state);
    cxf_simplex_setup(state, env, 0, NULL);

    int rc = cxf_simplex_post_iterate(NULL, state, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, rc);

    cxf_state_free(state);
}

/* V2 spec: null state returns CXF_ERROR_NULL_ARGUMENT */
void test_post_iterate_null_state(void) {
    int rc = cxf_simplex_post_iterate(model, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, rc);
}

/* Derives env from model->env for stagnation check */
void test_post_iterate_derives_env(void) {
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");
    SolverState *state = NULL;
    cxf_simplex_init(model, &state);
    cxf_simplex_setup(state, env, 0, NULL);

    /* Set up conditions for stagnation check at refactor boundary */
    state->eta_count = 100;
    state->obj_value = 1.0;
    state->obj_at_last_refactor = 1.0;

    int stall = 0;
    int rc = cxf_simplex_post_iterate(model, state, &stall);
    /* Function should succeed using model->env for optimality_tol */
    TEST_ASSERT_TRUE(rc == 0 || rc == CXF_ITERATION_LIMIT);
    /* Stagnation: delta_z == 0 < opt_tol => iteration_mode set */
    TEST_ASSERT_EQUAL_INT(1, state->iteration_mode);

    cxf_state_free(state);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_post_iterate_takes_model);
    RUN_TEST(test_post_iterate_null_model);
    RUN_TEST(test_post_iterate_null_state);
    RUN_TEST(test_post_iterate_derives_env);
    return UNITY_END();
}
