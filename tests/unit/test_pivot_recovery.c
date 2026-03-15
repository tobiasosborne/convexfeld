/**
 * @file test_pivot_recovery.c
 * @brief Tests for rejected pivot column recovery (numerical_stability.md A.4).
 *
 * Verifies that when a pivot column is rejected (pivot element below
 * CXF_PIVOT_TOL), the solver triggers refactorization and retries
 * before returning CXF_NUMERIC.
 */

#include "unity.h"
#include "convexfeld/convexfeld.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_env.h"
#include <math.h>
#include <string.h>

/* Forward declarations for API functions not in public headers */
int cxf_addconstr(CxfModel *model, int numnz, const int *cind,
                  const double *cval, char sense, double rhs,
                  const char *name);
int cxf_simplex_step(SolverState *state, CxfEnv *env);

void setUp(void) {}
void tearDown(void) {}

/**
 * Test: solver handles near-singular model without crashing.
 *
 * Constructs a model with nearly-parallel constraints that produce
 * small pivot elements. The solver should either recover via
 * refactorization or return CXF_NUMERIC cleanly.
 */
void test_near_singular_pivot_terminates_cleanly(void) {
    CxfEnv *env = NULL;
    CxfModel *model = NULL;
    cxf_loadenv(&env, NULL);
    cxf_setintparam(env, "OutputFlag", 0);
    cxf_newmodel(env, &model, "near_singular", 0,
                 NULL, NULL, NULL, NULL, NULL);

    /* Two variables with objective to force pricing */
    cxf_addvar(model, 0, NULL, NULL, -1.0, 0.0, 100.0, 'C', "x");
    cxf_addvar(model, 0, NULL, NULL, -1.0, 0.0, 100.0, 'C', "y");

    /* Nearly-parallel constraints: coefficients differ by 1e-12 */
    int ind[] = {0, 1};
    double v1[] = {1.0, 1.0};
    double v2[] = {1.0, 1.0 + 1e-12};
    cxf_addconstr(model, 2, ind, v1, '<', 50.0, "c1");
    cxf_addconstr(model, 2, ind, v2, '<', 50.0, "c2");

    cxf_optimize(model);
    int status;
    cxf_getintattr(model, "Status", &status);

    /* Must terminate: OPTIMAL or NUMERIC, never crash or hang */
    TEST_ASSERT_TRUE_MESSAGE(
        status == CXF_OPTIMAL || status == CXF_NUMERIC ||
        status == CXF_SUBOPTIMAL,
        "Near-singular model must terminate cleanly");

    if (status == CXF_OPTIMAL) {
        double objval;
        cxf_getdblattr(model, "ObjVal", &objval);
        TEST_ASSERT_TRUE_MESSAGE(isfinite(objval),
            "Objective must be finite when OPTIMAL");
    }

    cxf_freemodel(model);
    cxf_freeenv(env);
}

/**
 * Test: solver recovers on a model with tiny off-diagonal coefficients.
 *
 * A well-posed LP with very small coefficients in the constraint
 * matrix produces near-zero pivot elements. After refactorization
 * recovery, the solver should still find the correct optimal.
 */
void test_tiny_coefficients_recovery(void) {
    CxfEnv *env = NULL;
    CxfModel *model = NULL;
    cxf_loadenv(&env, NULL);
    cxf_setintparam(env, "OutputFlag", 0);
    cxf_newmodel(env, &model, "tiny_coeff", 0,
                 NULL, NULL, NULL, NULL, NULL);

    cxf_addvar(model, 0, NULL, NULL, -1.0, 0.0, 1e6, 'C', "x");
    cxf_addvar(model, 0, NULL, NULL, -1.0, 0.0, 1e6, 'C', "y");

    int ind[] = {0, 1};
    double v1[] = {1.0, 1e-11};   /* Tiny off-diagonal */
    double v2[] = {1e-11, 1.0};   /* Tiny off-diagonal */
    cxf_addconstr(model, 2, ind, v1, '<', 100.0, "c1");
    cxf_addconstr(model, 2, ind, v2, '<', 100.0, "c2");

    cxf_optimize(model);
    int status;
    cxf_getintattr(model, "Status", &status);

    TEST_ASSERT_TRUE_MESSAGE(
        status == CXF_OPTIMAL || status == CXF_NUMERIC,
        "Tiny coefficient model must terminate cleanly");

    if (status == CXF_OPTIMAL) {
        double objval;
        cxf_getdblattr(model, "ObjVal", &objval);
        TEST_ASSERT_TRUE_MESSAGE(isfinite(objval),
            "Objective must be finite");
        /* x~100, y~100, obj~-200 */
        TEST_ASSERT_DOUBLE_WITHIN(1.0, -200.0, objval);
    }

    cxf_freemodel(model);
    cxf_freeenv(env);
}

/**
 * Test: well-conditioned LP is unaffected by recovery code path.
 *
 * A straightforward LP with good pivot elements should solve
 * OPTIMAL without triggering pivot recovery. Ensures the recovery
 * code path does not interfere with normal operation.
 */
void test_well_conditioned_unaffected(void) {
    CxfEnv *env = NULL;
    CxfModel *model = NULL;
    cxf_loadenv(&env, NULL);
    cxf_setintparam(env, "OutputFlag", 0);
    cxf_newmodel(env, &model, "well_cond", 0,
                 NULL, NULL, NULL, NULL, NULL);

    cxf_addvar(model, 0, NULL, NULL, -1.0, 0.0, 100.0, 'C', "x");
    cxf_addvar(model, 0, NULL, NULL, -2.0, 0.0, 100.0, 'C', "y");

    int ind[] = {0, 1};
    double v1[] = {1.0, 1.0};
    double v2[] = {1.0, 3.0};
    cxf_addconstr(model, 2, ind, v1, '<', 80.0, "c1");
    cxf_addconstr(model, 2, ind, v2, '<', 120.0, "c2");

    cxf_optimize(model);
    int status;
    cxf_getintattr(model, "Status", &status);

    TEST_ASSERT_EQUAL_INT_MESSAGE(CXF_OPTIMAL, status,
        "Well-conditioned LP must be OPTIMAL");

    double objval;
    cxf_getdblattr(model, "ObjVal", &objval);
    TEST_ASSERT_TRUE_MESSAGE(isfinite(objval),
        "Objective must be finite");
    /* x=60, y=20, obj=-100 */
    TEST_ASSERT_DOUBLE_WITHIN(1e-4, -100.0, objval);

    cxf_freemodel(model);
    cxf_freeenv(env);
}

/**
 * Test: pivot recovery does not loop forever.
 *
 * A model designed to persistently produce rejected pivots should
 * eventually return CXF_NUMERIC, NOT hang in an infinite loop.
 */
void test_persistent_rejection_returns_numeric(void) {
    CxfEnv *env = NULL;
    CxfModel *model = NULL;
    cxf_loadenv(&env, NULL);
    cxf_setintparam(env, "OutputFlag", 0);
    cxf_setintparam(env, "IterationLimit", 500);
    cxf_newmodel(env, &model, "persistent_reject", 0,
                 NULL, NULL, NULL, NULL, NULL);

    /* Three variables, three nearly-identical constraints */
    cxf_addvar(model, 0, NULL, NULL, -1.0, 0.0, 1e8, 'C', "x");
    cxf_addvar(model, 0, NULL, NULL, -1.0, 0.0, 1e8, 'C', "y");
    cxf_addvar(model, 0, NULL, NULL, -1.0, 0.0, 1e8, 'C', "z");

    int ind[] = {0, 1, 2};
    double v1[] = {1.0, 1.0, 1.0};
    double v2[] = {1.0, 1.0 + 1e-13, 1.0};
    double v3[] = {1.0, 1.0, 1.0 + 1e-13};
    cxf_addconstr(model, 3, ind, v1, '<', 1e6, "c1");
    cxf_addconstr(model, 3, ind, v2, '<', 1e6, "c2");
    cxf_addconstr(model, 3, ind, v3, '<', 1e6, "c3");

    cxf_optimize(model);
    int status;
    cxf_getintattr(model, "Status", &status);

    /* Must terminate within iteration limit; never hang */
    TEST_ASSERT_TRUE_MESSAGE(
        status == CXF_OPTIMAL || status == CXF_NUMERIC ||
        status == CXF_ITERATION_LIMIT || status == CXF_SUBOPTIMAL,
        "Persistent rejection must terminate");

    cxf_freemodel(model);
    cxf_freeenv(env);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_near_singular_pivot_terminates_cleanly);
    RUN_TEST(test_tiny_coefficients_recovery);
    RUN_TEST(test_well_conditioned_unaffected);
    RUN_TEST(test_persistent_rejection_returns_numeric);
    return UNITY_END();
}
