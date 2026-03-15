/** @file test_btran_eta.c
 *  @brief Tests for BTRAN/BTRAN_VEC with eta factors (beyond identity basis).
 *  Uses BTRAN/FTRAN duality: y^T x = (B^{-T}y)^T a.  Beads: convexfeld-gtk */

#include "unity.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_types.h"
#include <stdlib.h>
#include <math.h>

/* Forward declarations */
BasisState *cxf_basis_create(int m, int n);
void cxf_basis_free(BasisState *basis);
int cxf_ftran(BasisState *basis, const double *column, double *result);
int cxf_btran(BasisState *basis, int row, double *result);
int cxf_btran_vec(BasisState *basis, const double *input, double *result);
int cxf_pivot_with_eta(BasisState *basis, int pivotRow,
                       const double *pivotCol, int enteringVar,
                       int leavingVar, int leavingStatus,
                       double reducedCost, int direction);

void setUp(void) {}
void tearDown(void) {}

static BasisState *make_basis_3x6(void) {
    BasisState *b = cxf_basis_create(3, 6);
    b->basic_vars[0] = 3;
    b->basic_vars[1] = 4;
    b->basic_vars[2] = 5;
    for (int j = 0; j < 3; j++) b->var_status[j] = CXF_VAR_AT_LOWER;
    b->var_status[3] = 0;
    b->var_status[4] = 1;
    b->var_status[5] = 2;
    return b;
}

static double dot3(const double *a, const double *b) {
    return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}

void test_btran_single_eta_row0(void) {
    /* BTRAN row 0 after one pivot: duality y^T pcol = FTRAN(pcol)[0] */
    BasisState *b = make_basis_3x6();
    double pcol[] = {2.0, 0.5, 0.25};
    cxf_pivot_with_eta(b, 0, pcol, 0, 3, CXF_VAR_AT_LOWER, 0.0, 1);

    double y[3];
    cxf_btran(b, 0, y);
    double ftran_pcol[3];
    cxf_ftran(b, pcol, ftran_pcol);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, ftran_pcol[0], dot3(y, pcol));

    cxf_basis_free(b);
}

void test_btran_single_eta_row1(void) {
    BasisState *b = make_basis_3x6();
    double pcol[] = {2.0, 0.5, 0.25};
    cxf_pivot_with_eta(b, 0, pcol, 0, 3, CXF_VAR_AT_LOWER, 0.0, 1);

    double y[3];
    cxf_btran(b, 1, y);
    double a[] = {1.0, 2.0, 3.0};
    double ftran_a[3];
    cxf_ftran(b, a, ftran_a);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, ftran_a[1], dot3(y, a));

    cxf_basis_free(b);
}

void test_btran_vec_single_eta(void) {
    BasisState *b = make_basis_3x6();
    double pcol[] = {2.0, 0.5, 0.25};
    cxf_pivot_with_eta(b, 0, pcol, 0, 3, CXF_VAR_AT_LOWER, 0.0, 1);

    double input[] = {1.0, 2.0, 3.0};
    double y[3];
    cxf_btran_vec(b, input, y);
    double a[] = {4.0, 5.0, 6.0};
    double ftran_a[3];
    cxf_ftran(b, a, ftran_a);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, dot3(input, ftran_a), dot3(y, a));

    cxf_basis_free(b);
}

void test_btran_two_pivots_duality(void) {
    BasisState *b = make_basis_3x6();
    double pcol1[] = {2.0, 0.5, 0.25};
    cxf_pivot_with_eta(b, 0, pcol1, 0, 3, CXF_VAR_AT_LOWER, 0.0, 1);
    double col2[] = {0.1, 1.5, 0.3};
    double ftran2[3];
    cxf_ftran(b, col2, ftran2);
    cxf_pivot_with_eta(b, 1, ftran2, 1, 4, CXF_VAR_AT_LOWER, 0.0, 1);
    double a[] = {7.0, 11.0, 13.0};
    double ftran_a[3];
    cxf_ftran(b, a, ftran_a);
    for (int row = 0; row < 3; row++) {
        double y[3];
        cxf_btran(b, row, y);
        TEST_ASSERT_DOUBLE_WITHIN(1e-10, ftran_a[row], dot3(y, a));
    }

    cxf_basis_free(b);
}

void test_btran_vec_two_pivots_duality(void) {
    BasisState *b = make_basis_3x6();
    double pcol1[] = {2.0, 0.5, 0.25};
    cxf_pivot_with_eta(b, 0, pcol1, 0, 3, CXF_VAR_AT_LOWER, 0.0, 1);
    double col2[] = {0.1, 1.5, 0.3};
    double ftran2[3];
    cxf_ftran(b, col2, ftran2);
    cxf_pivot_with_eta(b, 1, ftran2, 1, 4, CXF_VAR_AT_LOWER, 0.0, 1);

    double input[] = {3.0, 5.0, 7.0};
    double y[3];
    cxf_btran_vec(b, input, y);
    double a[] = {2.0, 4.0, 6.0};
    double ftran_a[3];
    cxf_ftran(b, a, ftran_a);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, dot3(input, ftran_a), dot3(y, a));

    cxf_basis_free(b);
}

void test_btran_three_pivots_all_rows(void) {
    BasisState *b = make_basis_3x6();
    double pcol1[] = {2.0, 0.5, 0.25};
    cxf_pivot_with_eta(b, 0, pcol1, 0, 3, CXF_VAR_AT_LOWER, 0.0, 1);
    double col2[] = {0.1, 1.5, 0.3};
    double ftran2[3];
    cxf_ftran(b, col2, ftran2);
    cxf_pivot_with_eta(b, 1, ftran2, 1, 4, CXF_VAR_AT_LOWER, 0.0, 1);
    double col3[] = {0.2, 0.4, 3.0};
    double ftran3[3];
    cxf_ftran(b, col3, ftran3);
    cxf_pivot_with_eta(b, 2, ftran3, 2, 5, CXF_VAR_AT_LOWER, 0.0, 1);

    double a[] = {1.0, 2.0, 3.0};
    double ftran_a[3];
    cxf_ftran(b, a, ftran_a);

    for (int row = 0; row < 3; row++) {
        double y[3];
        cxf_btran(b, row, y);
        TEST_ASSERT_DOUBLE_WITHIN(1e-10, ftran_a[row], dot3(y, a));
    }

    cxf_basis_free(b);
}

void test_btran_identity_eta(void) {
    /* nnz=0 eta: pivot column is a unit vector */
    BasisState *b = make_basis_3x6();
    double pcol[] = {1.0, 0.0, 0.0};
    cxf_pivot_with_eta(b, 0, pcol, 0, 3, CXF_VAR_AT_LOWER, 0.0, 1);

    double y[3];
    cxf_btran(b, 0, y);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 1.0, y[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 0.0, y[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 0.0, y[2]);

    cxf_basis_free(b);
}

void test_btran_negative_pivot(void) {
    BasisState *b = make_basis_3x6();
    double pcol[] = {-3.0, 1.0, 0.0};
    cxf_pivot_with_eta(b, 0, pcol, 0, 3, CXF_VAR_AT_LOWER, 0.0, 1);

    double a[] = {5.0, 10.0, 15.0};
    double ftran_a[3];
    cxf_ftran(b, a, ftran_a);

    for (int row = 0; row < 3; row++) {
        double y[3];
        cxf_btran(b, row, y);
        TEST_ASSERT_DOUBLE_WITHIN(1e-10, ftran_a[row], dot3(y, a));
    }

    cxf_basis_free(b);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_btran_single_eta_row0);
    RUN_TEST(test_btran_single_eta_row1);
    RUN_TEST(test_btran_vec_single_eta);
    RUN_TEST(test_btran_two_pivots_duality);
    RUN_TEST(test_btran_vec_two_pivots_duality);
    RUN_TEST(test_btran_three_pivots_all_rows);
    RUN_TEST(test_btran_identity_eta);
    RUN_TEST(test_btran_negative_pivot);
    return UNITY_END();
}
