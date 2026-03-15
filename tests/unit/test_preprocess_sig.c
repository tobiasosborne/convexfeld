/**
 * @file test_preprocess_sig.c
 * @brief V2 spec compliance: cxf_simplex_preprocess signature (state, env) -> int.
 *
 * Verifies that cxf_simplex_preprocess takes exactly two parameters
 * (SolverState*, CxfEnv*) and returns int, per simplex_phases.md P3.21.
 * Issue: convexfeld-frsq.
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
    cxf_newmodel(env, &model, "preprocess_sig", 0, NULL, NULL, NULL, NULL, NULL);
}

void tearDown(void) {
    cxf_freemodel(model);
    model = NULL;
    cxf_freeenv(env);
    env = NULL;
}

/*
 * V2 spec (simplex_phases.md, cxf_simplex_preprocess):
 *   Input: state : pointer-to-SolverState
 *   Input: env : pointer-to-Environment
 *   Output: int
 *
 * No flags parameter. The function takes exactly two inputs.
 */

/** Two-arg call compiles and returns CXF_OK on empty model. */
void test_preprocess_two_arg_signature(void) {
    SolverState *state = NULL;
    cxf_simplex_init(model, &state);

    int rc = cxf_simplex_preprocess(state, env);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    cxf_simplex_final(state);
}

/** NULL state returns error (two-arg call). */
void test_preprocess_null_state_two_arg(void) {
    int rc = cxf_simplex_preprocess(NULL, env);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, rc);
}

/** NULL env returns error (two-arg call). */
void test_preprocess_null_env_two_arg(void) {
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 5.0, 'C', "x");
    SolverState *state = NULL;
    cxf_simplex_init(model, &state);

    int rc = cxf_simplex_preprocess(state, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, rc);

    cxf_simplex_final(state);
}

/** Infeasible bounds detected without flags parameter. */
void test_preprocess_infeasible_two_arg(void) {
    cxf_addvar(model, 0, NULL, NULL, 1.0, 10.0, 5.0, 'C', "x");
    SolverState *state = NULL;
    cxf_simplex_init(model, &state);

    int rc = cxf_simplex_preprocess(state, env);
    TEST_ASSERT_EQUAL_INT(CXF_INFEASIBLE, rc);

    cxf_simplex_final(state);
}

/** Function pointer matches expected two-arg signature. */
void test_preprocess_function_pointer_type(void) {
    typedef int (*preprocess_fn)(SolverState *, CxfEnv *);
    preprocess_fn fn = cxf_simplex_preprocess;
    TEST_ASSERT_NOT_NULL(fn);
    /* If this compiles, the signature is correct. */
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_preprocess_two_arg_signature);
    RUN_TEST(test_preprocess_null_state_two_arg);
    RUN_TEST(test_preprocess_null_env_two_arg);
    RUN_TEST(test_preprocess_infeasible_two_arg);
    RUN_TEST(test_preprocess_function_pointer_type);
    return UNITY_END();
}
