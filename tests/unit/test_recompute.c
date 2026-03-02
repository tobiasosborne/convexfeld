/**
 * @file test_recompute.c
 * @brief Unit tests for from-scratch recomputation (recompute.c).
 *
 * Tests cxf_recompute_xB, cxf_recompute_objective, cxf_ftran_residual
 * with both identity (slack) and non-trivial (structural) bases.
 *
 * Test LP:  min 3x1 - 2x2
 *            x1 +  x2 <= 4   (slack s1 = var 2)
 *           2x1 +  x2 <= 6   (slack s2 = var 3)
 *           x1, x2 >= 0
 */

#include "unity.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_basis.h"
#include <string.h>
#include <math.h>

/* Internal functions (basis_internal.h) */
BasisState *cxf_basis_create(int m, int n);
void cxf_basis_free(BasisState *basis);

/* Functions under test */
int cxf_recompute_xB(SolverState *state);
void cxf_recompute_objective(SolverState *state);
double cxf_ftran_residual(SolverState *state, const double *a,
                          const double *x);

/* CSC of structural matrix A (2x2):
 *   col 0 (x1): [1.0 in row 0, 2.0 in row 1]
 *   col 1 (x2): [1.0 in row 0, 1.0 in row 1] */
static int64_t csc_col_ptr[] = {0, 2, 4};
static int     csc_row_idx[] = {0, 1, 0, 1};
static double  csc_values[]  = {1.0, 2.0, 1.0, 1.0};

#define M 2
#define N 2
#define TOTAL (N + M)

static SolverState state;
static BasisState *basis;
static double work_lb[TOTAL], work_ub[TOTAL], work_x[TOTAL];
static double work_obj[TOTAL], work_rhs[M];
static double work_column[M], work_cB[M];

/** Create basis with given basic vars, factorize, wire into state. */
static void build_basis(const int bvars[M], const int vstatus[TOTAL]) {
    basis = cxf_basis_create(M, TOTAL);
    TEST_ASSERT_NOT_NULL(basis);
    memcpy(basis->basic_vars, bvars, M * sizeof(int));
    memcpy(basis->var_status, vstatus, TOTAL * sizeof(int));
    for (int i = 0; i < M; i++) basis->diag_coeff[i] = 1.0;

    memset(&state, 0, sizeof(state));
    state.num_vars    = N;
    state.num_constrs = M;
    state.basis       = basis;
    state.csc_col_ptr = csc_col_ptr;
    state.csc_row_idx = csc_row_idx;
    state.csc_values  = csc_values;
    state.work_lb     = work_lb;
    state.work_ub     = work_ub;
    state.work_x      = work_x;
    state.work_obj    = work_obj;
    state.work_rhs    = work_rhs;
    state.work_column = work_column;
    state.work_cB     = work_cB;

    basis->lu = cxf_lu_create(M, 10, 10);
    TEST_ASSERT_NOT_NULL(basis->lu);
    TEST_ASSERT_EQUAL_INT(CXF_OK, cxf_lu_factorize(basis->lu, &state));
}

void setUp(void) {
    work_lb[0] = 0; work_lb[1] = 0; work_lb[2] = 0; work_lb[3] = 0;
    work_ub[0] = 10; work_ub[1] = 10; work_ub[2] = 100; work_ub[3] = 100;
    work_rhs[0] = 4.0;
    work_rhs[1] = 6.0;
    memset(work_obj, 0, sizeof(work_obj));
    basis = NULL;
}

void tearDown(void) {
    if (basis) { cxf_basis_free(basis); basis = NULL; }
}

/* ==== cxf_recompute_xB ==== */

void test_xB_identity_basis(void) {
    /* Slack basis (B = I). x_N = 0 => x_B = b = [4, 6]. */
    int bv[] = {2, 3};
    int vs[] = {CXF_VAR_AT_LOWER, CXF_VAR_AT_LOWER, 0, 1};
    build_basis(bv, vs);
    work_x[0] = 0; work_x[1] = 0;
    work_x[2] = 99; work_x[3] = 99;

    TEST_ASSERT_EQUAL_INT(CXF_OK, cxf_recompute_xB(&state));
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 4.0, work_x[2]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 6.0, work_x[3]);
}

void test_xB_snaps_nonbasic_to_bound(void) {
    /* x1 drifted to 1.5 but lb=1. Snap to 1, then
     * x_B = b - A1*1 = [4-1, 6-2] = [3, 4]. */
    int bv[] = {2, 3};
    int vs[] = {CXF_VAR_AT_LOWER, CXF_VAR_AT_LOWER, 0, 1};
    work_lb[0] = 1.0;
    build_basis(bv, vs);
    work_x[0] = 1.5; work_x[1] = 0;
    work_x[2] = 99; work_x[3] = 99;

    cxf_recompute_xB(&state);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 1.0, work_x[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 3.0, work_x[2]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 4.0, work_x[3]);
}

void test_xB_structural_basis(void) {
    /* Basis = {x1, x2}. B = [[1,1],[2,1]], B^-1 = [[-1,1],[2,-1]].
     * x_N = 0 => x_B = B^-1 b = [2, 2]. */
    int bv[] = {0, 1};
    int vs[] = {0, 1, CXF_VAR_AT_LOWER, CXF_VAR_AT_LOWER};
    build_basis(bv, vs);
    work_x[0] = 99; work_x[1] = 99;
    work_x[2] = 0;  work_x[3] = 0;

    TEST_ASSERT_EQUAL_INT(CXF_OK, cxf_recompute_xB(&state));
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 2.0, work_x[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 2.0, work_x[1]);
}

void test_xB_null_returns_error(void) {
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, cxf_recompute_xB(NULL));
}

/* ==== cxf_recompute_objective ==== */

void test_obj_phase2(void) {
    /* obj = 3*2 + (-2)*2 + 0*4 + 0*6 = 2. */
    int bv[] = {2, 3};
    int vs[] = {CXF_VAR_AT_LOWER, CXF_VAR_AT_LOWER, 0, 1};
    build_basis(bv, vs);
    state.phase = 2;
    work_obj[0] = 3.0; work_obj[1] = -2.0;
    work_x[0] = 2; work_x[1] = 2; work_x[2] = 4; work_x[3] = 6;
    state.obj_value = 999;

    cxf_recompute_objective(&state);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 2.0, state.obj_value);
}

void test_obj_phase1_single_violation(void) {
    /* Phase I: s1 = -0.5 (below lb=0). w = -1, obj = 0.5. */
    int bv[] = {2, 3};
    int vs[] = {CXF_VAR_AT_LOWER, CXF_VAR_AT_LOWER, 0, 1};
    build_basis(bv, vs);
    state.phase = 1;
    work_x[2] = -0.5; work_x[3] = 5.0;

    cxf_recompute_objective(&state);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 0.5, state.obj_value);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, -1.0, work_obj[2]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12,  0.0, work_obj[3]);
}

void test_obj_phase1_both_violated(void) {
    /* s1 = -1 (below lb), s2 = 101 (above ub=100).
     * w1 = -1, w2 = +1. obj = 1 + 1 = 2. */
    int bv[] = {2, 3};
    int vs[] = {CXF_VAR_AT_LOWER, CXF_VAR_AT_LOWER, 0, 1};
    build_basis(bv, vs);
    state.phase = 1;
    work_x[2] = -1.0; work_x[3] = 101.0;

    cxf_recompute_objective(&state);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 2.0, state.obj_value);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, -1.0, work_obj[2]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, +1.0, work_obj[3]);
}

/* ==== cxf_ftran_residual ==== */

void test_residual_exact(void) {
    /* Structural basis. B*[2,2] = [4,6] = a => residual = 0. */
    int bv[] = {0, 1};
    int vs[] = {0, 1, CXF_VAR_AT_LOWER, CXF_VAR_AT_LOWER};
    build_basis(bv, vs);
    double a[] = {4.0, 6.0};
    double x[] = {2.0, 2.0};

    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, cxf_ftran_residual(&state, a, x));
}

void test_residual_with_perturbation(void) {
    /* Perturbed x[0] by 0.1: B*[2.1,2] = [4.1,6.2].
     * residual = max(|4-4.1|, |6-6.2|) = 0.2. */
    int bv[] = {0, 1};
    int vs[] = {0, 1, CXF_VAR_AT_LOWER, CXF_VAR_AT_LOWER};
    build_basis(bv, vs);
    double a[] = {4.0, 6.0};
    double x[] = {2.1, 2.0};

    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.2, cxf_ftran_residual(&state, a, x));
}

void test_residual_null_returns_infinity(void) {
    double a[] = {1.0}, x[] = {1.0};
    TEST_ASSERT_TRUE(isinf(cxf_ftran_residual(NULL, a, x)));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_xB_identity_basis);
    RUN_TEST(test_xB_snaps_nonbasic_to_bound);
    RUN_TEST(test_xB_structural_basis);
    RUN_TEST(test_xB_null_returns_error);
    RUN_TEST(test_obj_phase2);
    RUN_TEST(test_obj_phase1_single_violation);
    RUN_TEST(test_obj_phase1_both_violated);
    RUN_TEST(test_residual_exact);
    RUN_TEST(test_residual_with_perturbation);
    RUN_TEST(test_residual_null_returns_infinity);
    return UNITY_END();
}
