/**
 * @file test_solver_state.c
 * @brief TDD tests for solver state module (M5.3.1)
 *
 * Tests for SolverState lifecycle functions:
 * - SolverState structure verification
 * - cxf_simplex_init (M5.3.3)
 * - cxf_simplex_final (M5.3.3)
 * - Helper and solution extraction functions (M5.3.4, M5.3.5)
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
    cxf_newmodel(env, &model, "test_solver_state", 0, NULL, NULL, NULL, NULL, NULL);
}

void tearDown(void) {
    if (model) { cxf_freemodel(model); model = NULL; }
    if (env) { cxf_freeenv(env); env = NULL; }
}

/*******************************************************************************
 * SolverState Structure Tests
 ******************************************************************************/

void test_solver_context_structure_exists(void) {
    /* Verify SolverState can be instantiated */
    SolverState ctx;
    ctx.num_vars = 0;
    ctx.num_constrs = 0;
    ctx.phase = 0;
    TEST_ASSERT_EQUAL_INT(0, ctx.num_vars);
    TEST_ASSERT_EQUAL_INT(0, ctx.num_constrs);
    TEST_ASSERT_EQUAL_INT(0, ctx.phase);
}

void test_solver_context_has_model_reference(void) {
    SolverState ctx;
    ctx.model_ref = model;
    TEST_ASSERT_EQUAL_PTR(model, ctx.model_ref);
}

void test_solver_context_has_working_arrays(void) {
    SolverState ctx;
    double lb[5], ub[5], obj[5], x[5], pi[3], dj[5];
    ctx.work_lb = lb;
    ctx.work_ub = ub;
    ctx.work_obj = obj;
    ctx.work_x = x;
    ctx.work_pi = pi;
    ctx.work_dj = dj;
    TEST_ASSERT_EQUAL_PTR(lb, ctx.work_lb);
    TEST_ASSERT_EQUAL_PTR(ub, ctx.work_ub);
    TEST_ASSERT_EQUAL_PTR(obj, ctx.work_obj);
    TEST_ASSERT_EQUAL_PTR(x, ctx.work_x);
    TEST_ASSERT_EQUAL_PTR(pi, ctx.work_pi);
    TEST_ASSERT_EQUAL_PTR(dj, ctx.work_dj);
}

void test_solver_context_has_subcomponents(void) {
    SolverState ctx;
    /* Use pointer assignments without instantiating opaque types */
    BasisState *basis_ptr = (BasisState *)0x12345678;
    PricingState *pricing_ptr = (PricingState *)0x87654321;
    ctx.basis = basis_ptr;
    ctx.pricing = pricing_ptr;
    TEST_ASSERT_EQUAL_PTR(basis_ptr, ctx.basis);
    TEST_ASSERT_EQUAL_PTR(pricing_ptr, ctx.pricing);
}

/*******************************************************************************
 * cxf_simplex_init Tests
 ******************************************************************************/

void test_simplex_init_null_model_fails(void) {
    SolverState *state = NULL;
    int status = cxf_simplex_init(NULL, &state);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);
    TEST_ASSERT_NULL(state);
}

void test_simplex_init_null_state_pointer_fails(void) {
    int status = cxf_simplex_init(model, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);
}

void test_simplex_init_returns_non_null_state(void) {
    SolverState *state = NULL;
    int status = cxf_simplex_init(model, &state);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_NOT_NULL(state);
    cxf_state_free(state);
}

void test_simplex_init_sets_model_reference(void) {
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");
    SolverState *state = NULL;
    cxf_simplex_init(model, &state);
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_EQUAL_PTR(model, state->model_ref);
    cxf_state_free(state);
}

void test_simplex_init_copies_dimensions(void) {
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x1");
    cxf_addvar(model, 0, NULL, NULL, 2.0, 0.0, 10.0, 'C', "x2");
    cxf_addvar(model, 0, NULL, NULL, 3.0, 0.0, 10.0, 'C', "x3");

    SolverState *state = NULL;
    cxf_simplex_init(model, &state);
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_EQUAL_INT(3, state->num_vars);
    /* num_constrs should be 0 for unconstrained model */
    TEST_ASSERT_EQUAL_INT(0, state->num_constrs);
    cxf_state_free(state);
}

void test_simplex_init_sets_initial_phase_zero(void) {
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");
    SolverState *state = NULL;
    cxf_simplex_init(model, &state);
    TEST_ASSERT_NOT_NULL(state);
    /* Phase should be 0 (setup) after init, before setup() call */
    TEST_ASSERT_EQUAL_INT(0, state->phase);
    cxf_state_free(state);
}

void test_simplex_init_allocates_working_arrays(void) {
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x1");
    cxf_addvar(model, 0, NULL, NULL, 2.0, 0.0, 10.0, 'C', "x2");

    SolverState *state = NULL;
    cxf_simplex_init(model, &state);
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->work_lb);
    TEST_ASSERT_NOT_NULL(state->work_ub);
    TEST_ASSERT_NOT_NULL(state->work_obj);
    TEST_ASSERT_NOT_NULL(state->work_x);
    TEST_ASSERT_NOT_NULL(state->work_dj);
    /* work_pi may be NULL if no constraints */
    cxf_state_free(state);
}

void test_simplex_init_initializes_iteration_counters(void) {
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");
    SolverState *state = NULL;
    cxf_simplex_init(model, &state);
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_EQUAL_INT(0, state->iteration);
    TEST_ASSERT_EQUAL_INT(0, state->eta_count);
    cxf_state_free(state);
}

/*******************************************************************************
 * cxf_simplex_final Tests
 ******************************************************************************/

void test_simplex_final_null_safe(void) {
    /* Should not crash */
    cxf_state_free(NULL);
    TEST_PASS();
}

void test_simplex_final_frees_state(void) {
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");
    SolverState *state = NULL;
    cxf_simplex_init(model, &state);
    TEST_ASSERT_NOT_NULL(state);
    cxf_state_free(state);
    /* After free, we can't verify state is freed, but test shouldn't leak */
    TEST_PASS();
}

void test_simplex_final_idempotent(void) {
    /* Note: This test is conceptually correct but practically dangerous.
     * In real code, never call free twice. This test verifies NULL safety. */
    cxf_state_free(NULL);
    cxf_state_free(NULL);
    TEST_PASS();
}

/*******************************************************************************
 * Integration Tests
 ******************************************************************************/

void test_init_final_cycle(void) {
    /* Multiple init/final cycles should work */
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");

    SolverState *state1 = NULL;
    cxf_simplex_init(model, &state1);
    TEST_ASSERT_NOT_NULL(state1);
    cxf_state_free(state1);

    SolverState *state2 = NULL;
    cxf_simplex_init(model, &state2);
    TEST_ASSERT_NOT_NULL(state2);
    cxf_state_free(state2);
}

void test_init_empty_model(void) {
    /* Init with model that has no variables */
    SolverState *state = NULL;
    int status = cxf_simplex_init(model, &state);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_EQUAL_INT(0, state->num_vars);
    TEST_ASSERT_EQUAL_INT(0, state->num_constrs);
    cxf_state_free(state);
}

/*******************************************************************************
 * Main test runner
 ******************************************************************************/

int main(void) {
    UNITY_BEGIN();

    /* SolverState structure tests */
    RUN_TEST(test_solver_context_structure_exists);
    RUN_TEST(test_solver_context_has_model_reference);
    RUN_TEST(test_solver_context_has_working_arrays);
    RUN_TEST(test_solver_context_has_subcomponents);

    /* cxf_simplex_init tests */
    RUN_TEST(test_simplex_init_null_model_fails);
    RUN_TEST(test_simplex_init_null_state_pointer_fails);
    RUN_TEST(test_simplex_init_returns_non_null_state);
    RUN_TEST(test_simplex_init_sets_model_reference);
    RUN_TEST(test_simplex_init_copies_dimensions);
    RUN_TEST(test_simplex_init_sets_initial_phase_zero);
    RUN_TEST(test_simplex_init_allocates_working_arrays);
    RUN_TEST(test_simplex_init_initializes_iteration_counters);

    /* cxf_simplex_final tests */
    RUN_TEST(test_simplex_final_null_safe);
    RUN_TEST(test_simplex_final_frees_state);
    RUN_TEST(test_simplex_final_idempotent);

    /* Integration tests */
    RUN_TEST(test_init_final_cycle);
    RUN_TEST(test_init_empty_model);

    return UNITY_END();
}
