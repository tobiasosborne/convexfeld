/**
 * @file test_nan_step_guard.c
 * @brief Tests for NaN/Inf step length detection (numerical_stability.md §C).
 *
 * Verifies that:
 * 1. cxf_ratio_test produces finite theta even with corrupted inputs.
 * 2. The solver terminates cleanly on models that stress BFRT step
 *    accumulation with large bounds and small pivots.
 */

#include "unity.h"
#include "convexfeld/convexfeld.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_env.h"
#include <math.h>
#include <string.h>

int cxf_ratio_test(SolverState *state, CxfEnv *env, int enteringVar,
                   const double *pivotColumn, int columnNZ,
                   int *leavingRow_out, double *pivotElement_out,
                   int *status_out, double *theta_out,
                   int *flip_rows_out, int max_flips,
                   int *num_flips_out);
int cxf_addconstr(CxfModel *model, int numnz, const int *cind,
                  const double *cval, char sense, double rhs,
                  const char *name);

/* ---- Unit test fixtures for cxf_ratio_test ---- */

static CxfEnv unit_env;
static SolverState unit_state;
static BasisState unit_basis;
static double wx[4], wlb[4], wub[4], wdj[4];
static int vstatus[4], bvars[2];

static void init_unit_fixture(void) {
    memset(&unit_env, 0, sizeof(unit_env));
    memset(&unit_state, 0, sizeof(unit_state));
    memset(&unit_basis, 0, sizeof(unit_basis));

    unit_env.feasibility_tol = 1e-6;
    unit_env.infinity = 1e20;

    unit_state.num_vars = 2;
    unit_state.num_constrs = 2;
    unit_state.basis = &unit_basis;
    unit_state.work_x = wx;
    unit_state.work_lb = wlb;
    unit_state.work_ub = wub;
    unit_state.work_dj = wdj;

    unit_basis.n = 4;
    unit_basis.var_status = vstatus;
    unit_basis.basic_vars = bvars;

    vstatus[0] = CXF_VAR_AT_LOWER;
    wlb[0] = 0; wub[0] = 10; wx[0] = 0; wdj[0] = 0;
    vstatus[1] = CXF_VAR_AT_LOWER;
    wlb[1] = 0; wub[1] = 10; wx[1] = 0; wdj[1] = 0;

    bvars[0] = 2; bvars[1] = 3;
    vstatus[2] = 0; wlb[2] = 0; wub[2] = 10; wx[2] = 5;
    vstatus[3] = 1; wlb[3] = 0; wub[3] = 10; wx[3] = 5;
    wdj[2] = 0; wdj[3] = 0;
}

void setUp(void) {}
void tearDown(void) {}

/* NaN in work_x: ratio_test should not produce NaN theta */
void test_ratio_test_nan_work_x(void) {
    init_unit_fixture();
    wx[2] = NAN;
    double pivotCol[2] = {1.0, 2.0};
    int lr = -1; double pe = 0.0, theta = -1.0;

    int rc = cxf_ratio_test(&unit_state, &unit_env, 0, pivotCol, 2,
                            &lr, &pe, NULL, &theta, NULL, 0, NULL);
    if (rc == CXF_OK)
        TEST_ASSERT_TRUE_MESSAGE(isfinite(theta), "NaN work_x must not produce NaN theta");
}

/* NaN in pivot column: should be skipped or produce finite theta */
void test_ratio_test_nan_pivot_column(void) {
    init_unit_fixture();
    double pivotCol[2] = {NAN, 2.0};
    int lr = -1; double pe = 0.0, theta = -1.0;

    int rc = cxf_ratio_test(&unit_state, &unit_env, 0, pivotCol, 2,
                            &lr, &pe, NULL, &theta, NULL, 0, NULL);
    if (rc == CXF_OK)
        TEST_ASSERT_TRUE_MESSAGE(isfinite(theta), "NaN pivotCol must not produce NaN theta");
}

/* Inf in work_x: ratio_test should handle gracefully */
void test_ratio_test_inf_work_x(void) {
    init_unit_fixture();
    wx[2] = INFINITY;
    double pivotCol[2] = {1.0, 2.0};
    int lr = -1; double pe = 0.0, theta = -1.0;

    int rc = cxf_ratio_test(&unit_state, &unit_env, 0, pivotCol, 2,
                            &lr, &pe, NULL, &theta, NULL, 0, NULL);
    if (rc == CXF_OK)
        TEST_ASSERT_TRUE_MESSAGE(isfinite(theta) || theta >= 0,
            "Inf work_x must not produce NaN theta");
}

/* Integration: extreme bounds with small coefficients stress BFRT */
void test_solver_extreme_bounds_bfrt_stress(void) {
    CxfEnv *env = NULL;
    CxfModel *model = NULL;
    cxf_loadenv(&env, NULL);
    cxf_setintparam(env, "OutputFlag", 0);
    cxf_newmodel(env, &model, "bfrt_stress", 0, NULL, NULL, NULL, NULL, NULL);

    cxf_addvar(model, 0, NULL, NULL, -1.0, 0.0, 1e15, 'C', "x");
    cxf_addvar(model, 0, NULL, NULL, -1.0, 0.0, 1e15, 'C', "y");

    int ind[] = {0, 1};
    double v1[] = {1e-6, 1.0};
    double v2[] = {1.0, 1e-6};
    cxf_addconstr(model, 2, ind, v1, '<', 1e10, "c1");
    cxf_addconstr(model, 2, ind, v2, '<', 1e10, "c2");

    cxf_optimize(model);
    int status;
    cxf_getintattr(model, "Status", &status);
    TEST_ASSERT_TRUE_MESSAGE(
        status == CXF_OPTIMAL || status == CXF_NUMERIC,
        "Extreme bounds should terminate cleanly");

    if (status == CXF_OPTIMAL) {
        double objval;
        cxf_getdblattr(model, "ObjVal", &objval);
        TEST_ASSERT_TRUE_MESSAGE(isfinite(objval),
            "Objective must be finite when OPTIMAL");
    }
    cxf_freemodel(model);
    cxf_freeenv(env);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_ratio_test_nan_work_x);
    RUN_TEST(test_ratio_test_nan_pivot_column);
    RUN_TEST(test_ratio_test_inf_work_x);
    RUN_TEST(test_solver_extreme_bounds_bfrt_stress);
    return UNITY_END();
}
