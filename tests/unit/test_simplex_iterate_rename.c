/**
 * @file test_simplex_iterate_rename.c
 * @brief Verify cxf_simplex_iterate exists with V2 spec name (convexfeld-ge8e)
 *
 * The V2 spec (simplex_iteration.md P3.20) requires the function be named
 * cxf_simplex_iterate. Previously it was cxf_log_iteration_progress.
 * These tests confirm the rename is complete and the function works.
 */

#include "unity.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_types.h"

/* The V2 spec name — must link successfully */
extern void cxf_simplex_iterate(CxfModel *model, SolverState *state);

static CxfEnv *env = NULL;
static CxfModel *model = NULL;

void setUp(void) {
    cxf_loadenv(&env, NULL);
    cxf_newmodel(env, &model, "iterate_rename", 0, NULL, NULL, NULL, NULL, NULL);
}

void tearDown(void) {
    cxf_freemodel(model);
    model = NULL;
    cxf_freeenv(env);
    env = NULL;
}

/* Test 1: cxf_simplex_iterate symbol exists and is callable */
void test_simplex_iterate_symbol_exists(void) {
    /* Null args must not crash — function has NULL guard */
    cxf_simplex_iterate(NULL, NULL);
    TEST_PASS();
}

/* Test 2: cxf_simplex_iterate accepts valid model + state */
void test_simplex_iterate_with_state(void) {
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");
    SolverState *state = NULL;
    int rc = cxf_simplex_init(model, &state);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    /* Must not crash; exercises log throttle + callback path */
    cxf_simplex_iterate(model, state);

    cxf_simplex_final(state);
}

/* Test 3: iteration counter is unchanged after logging call */
void test_simplex_iterate_no_side_effect_on_iteration(void) {
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");
    SolverState *state = NULL;
    cxf_simplex_init(model, &state);

    int before = state->iteration;
    cxf_simplex_iterate(model, state);
    TEST_ASSERT_EQUAL_INT(before, state->iteration);

    cxf_simplex_final(state);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_simplex_iterate_symbol_exists);
    RUN_TEST(test_simplex_iterate_with_state);
    RUN_TEST(test_simplex_iterate_no_side_effect_on_iteration);
    return UNITY_END();
}
