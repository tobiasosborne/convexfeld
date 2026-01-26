/**
 * @file test_simplex_edge.c
 * @brief TDD tests for simplex edge cases (M7.1.3)
 *
 * Tests: degeneracy handling, unbounded/infeasible detection, numerical
 * stability edge cases, empty/trivial problems. ~200 LOC.
 */

#include "unity.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_types.h"

/* External declarations - simplex functions to be implemented in M7.1.x */
int cxf_simplex_init(CxfModel *model, void *warmStart, int mode,
                     double *timing, SolverContext **stateOut);
int cxf_simplex_final(SolverContext *state);
int cxf_simplex_setup(SolverContext *state, CxfEnv *env);
int cxf_simplex_perturbation(SolverContext *state, CxfEnv *env);
int cxf_simplex_unperturb(SolverContext *state, CxfEnv *env);
int cxf_solve_lp(CxfModel *model);  /* Stub signature */
int cxf_addconstr(CxfModel *model, int numnz, const int *cind,
                  const double *cval, char sense, double rhs, const char *name);

/* Test fixtures */
static CxfEnv *env = NULL;
static CxfModel *model = NULL;
static double timing[16];

void setUp(void) {
    cxf_loadenv(&env, NULL);
    cxf_newmodel(env, &model, "edge_test");
    for (int i = 0; i < 16; i++) timing[i] = 0.0;
}

void tearDown(void) {
    cxf_freemodel(model);
    model = NULL;
    cxf_freeenv(env);
    env = NULL;
}

/* Degeneracy handling tests (cycling prevention via perturbation) */

void test_perturbation_null_args(void) {
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, cxf_simplex_perturbation(NULL, env));
    cxf_addvar(model, 0.0, 10.0, 1.0, 'C', "x");
    SolverContext *state = NULL;
    cxf_simplex_init(model, NULL, 0, timing, &state);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, cxf_simplex_perturbation(state, NULL));
    cxf_simplex_final(state);
}

void test_perturbation_basic(void) {
    cxf_addvar(model, 0.0, 10.0, 1.0, 'C', "x");
    SolverContext *state = NULL;
    cxf_simplex_init(model, NULL, 0, timing, &state);
    cxf_simplex_setup(state, env);
    TEST_ASSERT_EQUAL_INT(CXF_OK, cxf_simplex_perturbation(state, env));
    /* Idempotent - calling again should also return OK */
    TEST_ASSERT_EQUAL_INT(CXF_OK, cxf_simplex_perturbation(state, env));
    cxf_simplex_final(state);
}

void test_unperturb_null_args(void) {
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, cxf_simplex_unperturb(NULL, env));
}

void test_unperturb_sequence(void) {
    cxf_addvar(model, 0.0, 10.0, 1.0, 'C', "x");
    SolverContext *state = NULL;
    cxf_simplex_init(model, NULL, 0, timing, &state);
    cxf_simplex_setup(state, env);
    /* Without perturbation: returns 1 */
    TEST_ASSERT_EQUAL_INT(1, cxf_simplex_unperturb(state, env));
    /* After perturbation: returns OK */
    cxf_simplex_perturbation(state, env);
    TEST_ASSERT_EQUAL_INT(CXF_OK, cxf_simplex_unperturb(state, env));
    cxf_simplex_final(state);
}

/* Unbounded problem detection tests */

void test_solve_unbounded_simple(void) {
    /* min -x with x >= 0, no upper bound -> unbounded */
    cxf_addvar(model, 0.0, CXF_INFINITY, -1.0, 'C', "x");
    TEST_ASSERT_EQUAL_INT(CXF_UNBOUNDED, cxf_solve_lp(model));
}

void test_unbounded_with_constraint(void) {
    /* min -(x+y) with x-y <= 1, x,y >= 0 -> unbounded in y direction */
    cxf_addvar(model, 0.0, CXF_INFINITY, -1.0, 'C', "x");
    cxf_addvar(model, 0.0, CXF_INFINITY, -1.0, 'C', "y");
    int ind[] = {0, 1};
    double val[] = {1.0, -1.0};
    cxf_addconstr(model, 2, ind, val, '<', 1.0, "c1");
    TEST_ASSERT_EQUAL_INT(CXF_UNBOUNDED, cxf_solve_lp(model));
}

/* Infeasible problem detection tests */

void test_solve_infeasible_bounds(void) {
    /* lb > ub -> infeasible */
    cxf_addvar(model, 5.0, 3.0, 1.0, 'C', "x");
    TEST_ASSERT_EQUAL_INT(CXF_INFEASIBLE, cxf_solve_lp(model));
}

void test_infeasible_constraints(void) {
    /* x + y <= 1 and x + y >= 3 with x,y >= 0 -> infeasible */
    cxf_addvar(model, 0.0, CXF_INFINITY, 1.0, 'C', "x");
    cxf_addvar(model, 0.0, CXF_INFINITY, 1.0, 'C', "y");
    int ind[] = {0, 1};
    double val[] = {1.0, 1.0};
    cxf_addconstr(model, 2, ind, val, '<', 1.0, "c1");
    cxf_addconstr(model, 2, ind, val, '>', 3.0, "c2");
    TEST_ASSERT_EQUAL_INT(CXF_INFEASIBLE, cxf_solve_lp(model));
}

/* Numerical stability edge cases */

void test_small_coefficients(void) {
    cxf_addvar(model, 0.0, 10.0, 1e-12, 'C', "x");
    SolverContext *state = NULL;
    TEST_ASSERT_EQUAL_INT(CXF_OK, cxf_simplex_init(model, NULL, 0, timing, &state));
    cxf_simplex_final(state);
}

void test_large_coefficient_range(void) {
    cxf_addvar(model, 0.0, 1e10, 1e-8, 'C', "x");
    cxf_addvar(model, 0.0, 1e-10, 1e8, 'C', "y");
    SolverContext *state = NULL;
    TEST_ASSERT_EQUAL_INT(CXF_OK, cxf_simplex_init(model, NULL, 0, timing, &state));
    cxf_simplex_final(state);
}

void test_fixed_variable(void) {
    cxf_addvar(model, 5.0, 5.0, 1.0, 'C', "x_fixed");
    SolverContext *state = NULL;
    TEST_ASSERT_EQUAL_INT(CXF_OK, cxf_simplex_init(model, NULL, 0, timing, &state));
    cxf_simplex_final(state);
}

/* Empty/trivial problem tests */

void test_solve_empty_model(void) {
    TEST_ASSERT_EQUAL_INT(CXF_OPTIMAL, cxf_solve_lp(model));
}

void test_solve_trivial(void) {
    cxf_addvar(model, 0.0, 10.0, 1.0, 'C', "x");
    TEST_ASSERT_EQUAL_INT(CXF_OPTIMAL, cxf_solve_lp(model));
}

void test_solve_all_fixed(void) {
    cxf_addvar(model, 5.0, 5.0, 1.0, 'C', "x");
    cxf_addvar(model, 3.0, 3.0, 2.0, 'C', "y");
    TEST_ASSERT_EQUAL_INT(CXF_OPTIMAL, cxf_solve_lp(model));
}

void test_solve_free_variable(void) {
    /* Free variable with zero obj coeff -> optimal with obj=0 */
    cxf_addvar(model, -CXF_INFINITY, CXF_INFINITY, 0.0, 'C', "free");
    TEST_ASSERT_EQUAL_INT(CXF_OPTIMAL, cxf_solve_lp(model));
}

/* Main test runner */

int main(void) {
    UNITY_BEGIN();
    /* Degeneracy handling */
    RUN_TEST(test_perturbation_null_args);
    RUN_TEST(test_perturbation_basic);
    RUN_TEST(test_unperturb_null_args);
    RUN_TEST(test_unperturb_sequence);
    /* Unbounded detection */
    RUN_TEST(test_solve_unbounded_simple);
    RUN_TEST(test_unbounded_with_constraint);
    /* Infeasible detection */
    RUN_TEST(test_solve_infeasible_bounds);
    RUN_TEST(test_infeasible_constraints);
    /* Numerical stability */
    RUN_TEST(test_small_coefficients);
    RUN_TEST(test_large_coefficient_range);
    RUN_TEST(test_fixed_variable);
    /* Empty/trivial */
    RUN_TEST(test_solve_empty_model);
    RUN_TEST(test_solve_trivial);
    RUN_TEST(test_solve_all_fixed);
    RUN_TEST(test_solve_free_variable);
    return UNITY_END();
}
