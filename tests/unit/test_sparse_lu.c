/**
 * @file test_sparse_lu.c
 * @brief Tests for sparse LU factorization (sparse_work, sparse_elim).
 */

#include "unity.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_types.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Internal declarations */
typedef struct {
    int *row_idx; double *values; int len, cap;
} SparseCol;
typedef struct {
    int m; SparseCol *cols; int *row_count; double *col_max;
    int *row_active; int *col_active; int active_count; int total_nnz;
} SparseWork;

SparseWork *sparse_work_create(int m);
void sparse_work_free(SparseWork *sw);
int sparse_col_append(SparseCol *col, int row, double val);
double sparse_work_density(const SparseWork *sw);
int sparse_find_pivot(const SparseWork *sw, int *r, int *c, double *v);
int sparse_extract_pivot_row(const SparseWork *sw, int pr, int *c, double *v);
int sparse_eliminate(SparseWork *sw, int pr, int pc, double pv,
                     const int *prc, const double *prv, int prl,
                     int **Li, int **Lj, double **Lv,
                     int *Lc, int *Lcap, int step);

BasisState *cxf_basis_create(int m, int n);
void cxf_basis_free(BasisState *basis);
int cxf_ftran(BasisState *basis, const double *column, double *result);

void setUp(void) {}
void tearDown(void) {}

/* === SparseWork lifecycle === */

void test_sparse_work_create_valid(void) {
    SparseWork *sw = sparse_work_create(5);
    TEST_ASSERT_NOT_NULL(sw);
    TEST_ASSERT_EQUAL_INT(5, sw->m);
    TEST_ASSERT_EQUAL_INT(5, sw->active_count);
    TEST_ASSERT_EQUAL_INT(0, sw->total_nnz);
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_EQUAL_INT(1, sw->row_active[i]);
        TEST_ASSERT_EQUAL_INT(1, sw->col_active[i]);
    }
    sparse_work_free(sw);
}

void test_sparse_work_create_zero_returns_null(void) {
    TEST_ASSERT_NULL(sparse_work_create(0));
    TEST_ASSERT_NULL(sparse_work_create(-1));
}

void test_sparse_work_free_null_safe(void) {
    sparse_work_free(NULL);
    TEST_PASS();
}

/* === SparseCol append === */

void test_sparse_col_append_basic(void) {
    SparseCol col = {NULL, NULL, 0, 0};
    TEST_ASSERT_EQUAL_INT(0, sparse_col_append(&col, 2, 3.5));
    TEST_ASSERT_EQUAL_INT(0, sparse_col_append(&col, 0, -1.0));
    TEST_ASSERT_EQUAL_INT(0, sparse_col_append(&col, 4, 2.0));
    TEST_ASSERT_EQUAL_INT(3, col.len);
    TEST_ASSERT_EQUAL_INT(2, col.row_idx[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 3.5, col.values[0]);
    TEST_ASSERT_EQUAL_INT(0, col.row_idx[1]);
    TEST_ASSERT_EQUAL_INT(4, col.row_idx[2]);
    free(col.row_idx); free(col.values);
}

void test_sparse_col_append_growth(void) {
    SparseCol col = {NULL, NULL, 0, 0};
    for (int i = 0; i < 20; i++)
        TEST_ASSERT_EQUAL_INT(0, sparse_col_append(&col, i, (double)i));
    TEST_ASSERT_EQUAL_INT(20, col.len);
    TEST_ASSERT_TRUE(col.cap >= 20);
    for (int i = 0; i < 20; i++)
        TEST_ASSERT_DOUBLE_WITHIN(1e-15, (double)i, col.values[i]);
    free(col.row_idx); free(col.values);
}

/* === Density === */

void test_sparse_work_density(void) {
    SparseWork *sw = sparse_work_create(4);
    TEST_ASSERT_NOT_NULL(sw);
    /* 4 entries in 4x4 = 0.25 density */
    for (int j = 0; j < 4; j++)
        sparse_col_append(&sw->cols[j], j, 1.0);
    sw->total_nnz = 4;
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 0.25, sparse_work_density(sw));
    sparse_work_free(sw);
}

/* === Pivot search === */

void test_sparse_find_pivot_singleton(void) {
    /* 3x3 identity: all singletons, Markowitz cost 0 */
    SparseWork *sw = sparse_work_create(3);
    for (int j = 0; j < 3; j++) {
        sparse_col_append(&sw->cols[j], j, 1.0);
        sw->row_count[j] = 1;
        sw->col_max[j] = 1.0;
    }
    sw->total_nnz = 3;
    int r, c; double v;
    TEST_ASSERT_EQUAL_INT(0, sparse_find_pivot(sw, &r, &c, &v));
    /* Any singleton is valid (cost 0) */
    TEST_ASSERT_TRUE(r >= 0 && r < 3);
    TEST_ASSERT_EQUAL_INT(r, c);  /* identity: row == col */
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 1.0, v);
    sparse_work_free(sw);
}

void test_sparse_find_pivot_markowitz(void) {
    /* 3x3: [[2,1,0],[1,3,1],[0,1,4]]
     * Scores: (0,0)=(2-1)*(2-1)=1 |2|, (2,2)=(2-1)*(2-1)=1 |4|
     * Tie-break by largest |value|: should pick (2,2) with val=4 */
    SparseWork *sw = sparse_work_create(3);
    /* Col 0: rows 0,1 */
    sparse_col_append(&sw->cols[0], 0, 2.0);
    sparse_col_append(&sw->cols[0], 1, 1.0);
    /* Col 1: rows 0,1,2 */
    sparse_col_append(&sw->cols[1], 0, 1.0);
    sparse_col_append(&sw->cols[1], 1, 3.0);
    sparse_col_append(&sw->cols[1], 2, 1.0);
    /* Col 2: rows 1,2 */
    sparse_col_append(&sw->cols[2], 1, 1.0);
    sparse_col_append(&sw->cols[2], 2, 4.0);
    sw->row_count[0] = 2; sw->row_count[1] = 3; sw->row_count[2] = 2;
    sw->col_max[0] = 2.0; sw->col_max[1] = 3.0; sw->col_max[2] = 4.0;
    sw->total_nnz = 7;

    int r, c; double v;
    TEST_ASSERT_EQUAL_INT(0, sparse_find_pivot(sw, &r, &c, &v));
    TEST_ASSERT_EQUAL_INT(2, r);
    TEST_ASSERT_EQUAL_INT(2, c);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 4.0, v);
    sparse_work_free(sw);
}

/* === Elimination === */

void test_sparse_eliminate_2x2(void) {
    /* [[4,2],[1,3]] pivot on (0,0) val=4
     * mult = 1/4, L entry = (1, 0, 0.25)
     * (1,1): 3 - 0.25*2 = 2.5 */
    SparseWork *sw = sparse_work_create(2);
    sparse_col_append(&sw->cols[0], 0, 4.0);
    sparse_col_append(&sw->cols[0], 1, 1.0);
    sparse_col_append(&sw->cols[1], 0, 2.0);
    sparse_col_append(&sw->cols[1], 1, 3.0);
    sw->row_count[0] = 2; sw->row_count[1] = 2;
    sw->col_max[0] = 4.0; sw->col_max[1] = 3.0;
    sw->total_nnz = 4;

    int pr_cols[] = {0, 1}; double pr_vals[] = {4.0, 2.0};
    int Lcap = 4, Lc = 0;
    int *Li = malloc(4 * sizeof(int));
    int *Lj = malloc(4 * sizeof(int));
    double *Lv = malloc(4 * sizeof(double));

    int rc = sparse_eliminate(sw, 0, 0, 4.0, pr_cols, pr_vals, 2,
                              &Li, &Lj, &Lv, &Lc, &Lcap, 0);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_INT(1, Lc);
    TEST_ASSERT_EQUAL_INT(1, Li[0]);    /* row 1 */
    TEST_ASSERT_EQUAL_INT(0, Lj[0]);    /* step 0 */
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 0.25, Lv[0]);

    /* Column 1 should have updated entry at row 1 = 2.5 */
    SparseCol *c1 = &sw->cols[1];
    int found = 0;
    for (int k = 0; k < c1->len; k++) {
        if (c1->row_idx[k] == 1) {
            TEST_ASSERT_DOUBLE_WITHIN(1e-12, 2.5, c1->values[k]);
            found = 1;
        }
    }
    TEST_ASSERT_TRUE(found);
    free(Li); free(Lj); free(Lv);
    sparse_work_free(sw);
}

void test_sparse_eliminate_fillin(void) {
    /* [[1,0,1],[0,1,1],[1,1,0]] pivot (0,0) val=1
     * Row 2 has a[2,0]=1, mult=1.0
     * Pivot row entries: col 0 val 1, col 2 val 1
     * Update (2,2): 0 - 1.0*1 = -1 → fill-in! */
    SparseWork *sw = sparse_work_create(3);
    sparse_col_append(&sw->cols[0], 0, 1.0);
    sparse_col_append(&sw->cols[0], 2, 1.0);
    sparse_col_append(&sw->cols[1], 1, 1.0);
    sparse_col_append(&sw->cols[1], 2, 1.0);
    sparse_col_append(&sw->cols[2], 0, 1.0);
    sparse_col_append(&sw->cols[2], 1, 1.0);
    sw->row_count[0] = 2; sw->row_count[1] = 2; sw->row_count[2] = 2;
    sw->col_max[0] = 1.0; sw->col_max[1] = 1.0; sw->col_max[2] = 1.0;
    sw->total_nnz = 6;

    int pr_cols[] = {0, 2}; double pr_vals[] = {1.0, 1.0};
    int Lcap = 4, Lc = 0;
    int *Li = malloc(4 * sizeof(int));
    int *Lj = malloc(4 * sizeof(int));
    double *Lv = malloc(4 * sizeof(double));

    sparse_eliminate(sw, 0, 0, 1.0, pr_cols, pr_vals, 2,
                     &Li, &Lj, &Lv, &Lc, &Lcap, 0);

    /* Check fill-in: column 2 should now have entry at row 2 = -1 */
    SparseCol *c2 = &sw->cols[2];
    int found = 0;
    for (int k = 0; k < c2->len; k++) {
        if (c2->row_idx[k] == 2) {
            TEST_ASSERT_DOUBLE_WITHIN(1e-12, -1.0, c2->values[k]);
            found = 1;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(found, "Fill-in entry at (2,2) not created");
    free(Li); free(Lj); free(Lv);
    sparse_work_free(sw);
}

/* === Full factorize roundtrip === */

void test_lu_factorize_3x3_roundtrip(void) {
    /* Basis = [[2,1,0],[1,3,1],[0,1,2]] (symmetric positive definite)
     * Factorize, then verify FTRAN: B * (B^-1 * e_j) = e_j */
    int m = 3, n = 6;  /* 3 structural + 3 slack */
    BasisState *basis = cxf_basis_create(m, n);
    TEST_ASSERT_NOT_NULL(basis);

    /* Set up as if all 3 basis vars are structural (cols 0,1,2) */
    basis->basic_vars[0] = 0;
    basis->basic_vars[1] = 1;
    basis->basic_vars[2] = 2;
    for (int i = 0; i < m; i++) basis->diag_coeff[i] = 1.0;

    /* Build a mock SolverState with CSC for the 3x3 matrix */
    SolverState ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.basis = basis;
    ctx.num_vars = 3;
    ctx.num_constrs = 3;

    /* CSC of [[2,1,0],[1,3,1],[0,1,2]]:
     * Col 0: rows 0,1 vals 2,1
     * Col 1: rows 0,1,2 vals 1,3,1
     * Col 2: rows 1,2 vals 1,2 */
    int64_t col_ptr[] = {0, 2, 5, 7};
    int row_idx[] = {0, 1, 0, 1, 2, 1, 2};
    double vals[] = {2.0, 1.0, 1.0, 3.0, 1.0, 1.0, 2.0};
    ctx.csc_col_ptr = col_ptr;
    ctx.csc_row_idx = row_idx;
    ctx.csc_values = vals;

    /* Allocate LU and factorize */
    basis->lu = cxf_lu_create(m, 10, 10);
    TEST_ASSERT_NOT_NULL(basis->lu);

    int rc = cxf_lu_factorize(basis->lu, &ctx);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, rc, "Factorization failed");
    TEST_ASSERT_EQUAL_INT(1, basis->lu->valid);

    /* Allocate work arrays for FTRAN */
    basis->work = calloc((size_t)m, sizeof(double));
    basis->work2 = calloc((size_t)m, sizeof(double));
    basis->eta_head = NULL;
    basis->eta_count = 0;

    /* Verify: FTRAN each column of B, result should be unit vector */
    double result[3];
    for (int j = 0; j < 3; j++) {
        memset(result, 0, 3 * sizeof(double));
        /* Extract column j of B */
        for (int i = 0; i < 3; i++) result[i] = 0.0;
        for (int64_t k = col_ptr[j]; k < col_ptr[j + 1]; k++)
            result[row_idx[k]] = vals[k];

        cxf_ftran(basis, result, result);

        /* Should be e_j (unit vector at position j) */
        for (int i = 0; i < 3; i++) {
            double expected = (i == j) ? 1.0 : 0.0;
            TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(
                1e-10, expected, result[i],
                "FTRAN roundtrip failed");
        }
    }

    /* Cleanup: NULL out CSC pointers so basis_free doesn't free stack data */
    ctx.csc_col_ptr = NULL;
    ctx.csc_row_idx = NULL;
    ctx.csc_values = NULL;
    free(basis->work);
    free(basis->work2);
    basis->work = NULL;
    basis->work2 = NULL;
    cxf_basis_free(basis);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_sparse_work_create_valid);
    RUN_TEST(test_sparse_work_create_zero_returns_null);
    RUN_TEST(test_sparse_work_free_null_safe);
    RUN_TEST(test_sparse_col_append_basic);
    RUN_TEST(test_sparse_col_append_growth);
    RUN_TEST(test_sparse_work_density);
    RUN_TEST(test_sparse_find_pivot_singleton);
    RUN_TEST(test_sparse_find_pivot_markowitz);
    RUN_TEST(test_sparse_eliminate_2x2);
    RUN_TEST(test_sparse_eliminate_fillin);
    RUN_TEST(test_lu_factorize_3x3_roundtrip);
    return UNITY_END();
}
