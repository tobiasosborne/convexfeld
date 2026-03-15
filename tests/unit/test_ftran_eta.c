/** @file test_ftran_eta.c
 *  @brief Tests for FTRAN with eta factors (beyond identity basis).
 *  Beads: convexfeld-gtk */

#include "unity.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_types.h"
#include <stdlib.h>
#include <math.h>

/* Forward declarations */
BasisState *cxf_basis_create(int m, int n);
void cxf_basis_free(BasisState *basis);
int cxf_ftran(BasisState *basis, const double *column, double *result);
int cxf_pivot_with_eta(BasisState *basis, int pivotRow,
                       const double *pivotCol, int enteringVar,
                       int leavingVar, int leavingStatus,
                       double reducedCost, int direction);

void setUp(void) {}
void tearDown(void) {}

/** Create a 3x6 basis with identity diagonal (slacks 3,4,5 basic). */
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

/*--- Single pivot eta tests ------------------------------------------------*/

void test_ftran_single_eta(void) {
    /* After pivot at row 0, FTRAN of pivot column should give e_0. */
    BasisState *b = make_basis_3x6();
    double pcol[] = {2.0, 0.5, 0.25};
    int rc = cxf_pivot_with_eta(b, 0, pcol, 0, 3, CXF_VAR_AT_LOWER, 0.0, 1);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    /* FTRAN of the same pivot column should give e_0 (identity pivot col) */
    double result[3];
    rc = cxf_ftran(b, pcol, result);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 1.0, result[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 0.0, result[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 0.0, result[2]);

    cxf_basis_free(b);
}

void test_ftran_eta_nonpivot_column(void) {
    /* After one pivot, FTRAN a different column. */
    BasisState *b = make_basis_3x6();
    double pcol[] = {2.0, 0.5, 0.25};
    cxf_pivot_with_eta(b, 0, pcol, 0, 3, CXF_VAR_AT_LOWER, 0.0, 1);

    /* FTRAN [1,0,0]: factor=1/2=0.5, r[0]=0.5, r[1]-=0.25, r[2]-=0.125 */
    double col[] = {1.0, 0.0, 0.0};
    double result[3];
    cxf_ftran(b, col, result);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 0.5, result[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, -0.25, result[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, -0.125, result[2]);

    cxf_basis_free(b);
}

void test_ftran_eta_zero_in_pivot_row(void) {
    /* Hyper-sparse skip: if result[pivot_row]==0 before eta, skip it. */
    BasisState *b = make_basis_3x6();
    double pcol[] = {2.0, 0.5, 0.25};
    cxf_pivot_with_eta(b, 0, pcol, 0, 3, CXF_VAR_AT_LOWER, 0.0, 1);

    /* Column [0, 1, 0] has 0 in pivot_row 0, so eta is skipped. */
    double col[] = {0.0, 1.0, 0.0};
    double result[3];
    cxf_ftran(b, col, result);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 0.0, result[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 1.0, result[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 0.0, result[2]);

    cxf_basis_free(b);
}

/*--- Multiple pivot eta tests ----------------------------------------------*/

void test_ftran_two_pivots(void) {
    /* Two consecutive pivots, then FTRAN the second pivot column. */
    BasisState *b = make_basis_3x6();

    /* Pivot 1: entering var 0 at row 0 */
    double pcol1[] = {2.0, 0.5, 0.25};
    cxf_pivot_with_eta(b, 0, pcol1, 0, 3, CXF_VAR_AT_LOWER, 0.0, 1);

    /* FTRAN column [0.1, 1.5, 0.3] through updated basis for pivot 2 */
    double col2[] = {0.1, 1.5, 0.3};
    double ftran2[3];
    cxf_ftran(b, col2, ftran2);

    /* Pivot 2: use ftran2 as the pivot column, entering var 1 at row 1 */
    cxf_pivot_with_eta(b, 1, ftran2, 1, 4, CXF_VAR_AT_LOWER, 0.0, 1);

    /* FTRAN of ftran2 through updated basis should give e_1 */
    double result[3];
    cxf_ftran(b, col2, result);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 0.0, result[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 1.0, result[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 0.0, result[2]);

    cxf_basis_free(b);
}

void test_ftran_three_pivots_all_rows(void) {
    /* Three pivots replacing all slack variables. */
    BasisState *b = make_basis_3x6();

    /* Pivot 1: row 0 */
    double pcol1[] = {2.0, 0.5, 0.25};
    cxf_pivot_with_eta(b, 0, pcol1, 0, 3, CXF_VAR_AT_LOWER, 0.0, 1);

    /* FTRAN for pivot 2: row 1 */
    double col2[] = {0.1, 1.5, 0.3};
    double ftran2[3];
    cxf_ftran(b, col2, ftran2);
    cxf_pivot_with_eta(b, 1, ftran2, 1, 4, CXF_VAR_AT_LOWER, 0.0, 1);

    /* FTRAN for pivot 3: row 2 */
    double col3[] = {0.2, 0.4, 3.0};
    double ftran3[3];
    cxf_ftran(b, col3, ftran3);
    cxf_pivot_with_eta(b, 2, ftran3, 2, 5, CXF_VAR_AT_LOWER, 0.0, 1);

    /* FTRAN of col3 through final basis should give e_2 */
    double result[3];
    cxf_ftran(b, col3, result);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 0.0, result[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 0.0, result[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 1.0, result[2]);

    cxf_basis_free(b);
}

/*--- Edge case: identity eta column (nnz=0) --------------------------------*/

void test_ftran_identity_eta(void) {
    /* Pivot column is a unit vector at pivot_row: eta has nnz=0. */
    BasisState *b = make_basis_3x6();
    double pcol[] = {1.0, 0.0, 0.0};
    cxf_pivot_with_eta(b, 0, pcol, 0, 3, CXF_VAR_AT_LOWER, 0.0, 1);
    TEST_ASSERT_EQUAL_INT(0, b->eta_head->nnz);

    /* FTRAN should still work correctly (identity eta) */
    double col[] = {3.0, 7.0, 11.0};
    double result[3];
    cxf_ftran(b, col, result);
    /* Eta: factor = 3.0/1.0 = 3.0, result[0] = 3.0, no off-diag */
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 3.0, result[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 7.0, result[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 11.0, result[2]);

    cxf_basis_free(b);
}

/*--- Edge case: negative pivot element -------------------------------------*/

void test_ftran_negative_pivot(void) {
    BasisState *b = make_basis_3x6();
    double pcol[] = {-3.0, 1.0, 0.0};
    cxf_pivot_with_eta(b, 0, pcol, 0, 3, CXF_VAR_AT_LOWER, 0.0, 1);

    /* FTRAN of pcol should give e_0 */
    double result[3];
    cxf_ftran(b, pcol, result);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 1.0, result[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 0.0, result[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 0.0, result[2]);

    cxf_basis_free(b);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_ftran_single_eta);
    RUN_TEST(test_ftran_eta_nonpivot_column);
    RUN_TEST(test_ftran_eta_zero_in_pivot_row);
    RUN_TEST(test_ftran_two_pivots);
    RUN_TEST(test_ftran_three_pivots_all_rows);
    RUN_TEST(test_ftran_identity_eta);
    RUN_TEST(test_ftran_negative_pivot);
    return UNITY_END();
}
