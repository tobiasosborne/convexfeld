/**
 * @file test_crash_csr.c
 * @brief Tests for crash basis CSR source and inactive sentinel marking.
 *
 * Validates convexfeld-tx3m (CSR from SolverState, not MatrixData)
 * and convexfeld-y8ur (inactive sentinel -1 in csr_col_idx).
 *
 * Spec: docs/specs-v2/specs/algorithms/crash_basis.md
 */

#include "unity.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_matrix.h"
#include "convexfeld/cxf_types.h"
#include <stdlib.h>
#include <string.h>

extern int cxf_simplex_crash(SolverState *state, CxfEnv *env);

static CxfEnv *env = NULL;

void setUp(void) {
    cxf_loadenv(&env, NULL);
}

void tearDown(void) {
    cxf_freeenv(env);
    env = NULL;
}

/*******************************************************************************
 * Helper: build a SolverState with SolverState-owned CSR arrays
 ******************************************************************************/
static SolverState *make_csr_state(int n, int m, double *rhs, char *sense,
                                   int64_t *row_ptr, int *col_idx,
                                   int64_t nnz) {
    CxfModel *model;
    cxf_newmodel(env, &model, "csr_test", 0, NULL, NULL, NULL, NULL, NULL);
    for (int j = 0; j < n; j++) {
        cxf_addvar(model, 0, NULL, NULL, 0.0, 0.0, 10.0, 'C', NULL);
    }

    SolverState *state = NULL;
    cxf_simplex_init(model, &state);

    state->num_constrs = m;

    /* Allocate crash arrays */
    free(state->row_status);
    state->row_status = (int *)calloc((size_t)m, sizeof(int));
    free(state->col_nz_count);
    state->col_nz_count = (int *)calloc((size_t)(n + m), sizeof(int));

    /* Set SolverState working copies (spec-required source) */
    free(state->work_rhs);
    state->work_rhs = (double *)malloc((size_t)m * sizeof(double));
    memcpy(state->work_rhs, rhs, (size_t)m * sizeof(double));

    free(state->work_sense);
    state->work_sense = (char *)malloc((size_t)m * sizeof(char));
    memcpy(state->work_sense, sense, (size_t)m * sizeof(char));

    /* Set SolverState CSR (the arrays crash should use) */
    free(state->csr_row_ptr);
    state->csr_row_ptr = (int64_t *)malloc((size_t)(m + 1) * sizeof(int64_t));
    memcpy(state->csr_row_ptr, row_ptr, (size_t)(m + 1) * sizeof(int64_t));

    free(state->csr_col_idx);
    state->csr_col_idx = (int *)malloc((size_t)nnz * sizeof(int));
    memcpy(state->csr_col_idx, col_idx, (size_t)nnz * sizeof(int));

    state->num_basic = 0;
    state->problem_var_index = -1;

    return state;
}

static void free_csr_state(SolverState *state) {
    cxf_freemodel(state->model_ref);
    cxf_state_free(state);
}

/*******************************************************************************
 * Test: sentinel -1 is written for every entry in a removed row
 ******************************************************************************/
void test_sentinel_marking_on_removal(void) {
    /* 3 vars, 2 rows. Row 0 unassigned, row 1 candidate.
     * Row 1 has entries in cols 0, 1, 2. */
    double rhs[] = {5.0, 3.0};
    char sense[] = {'<', '<'};
    int64_t row_ptr[] = {0, 0, 3};  /* row 0: 0 entries, row 1: 3 entries */
    int col_idx[] = {0, 1, 2};

    SolverState *state = make_csr_state(3, 2, rhs, sense,
                                        row_ptr, col_idx, 3);
    state->row_status[1] = 1;  /* candidate */
    state->col_nz_count[0] = 1;
    state->col_nz_count[1] = 1;
    state->col_nz_count[2] = 1;

    int rc = cxf_simplex_crash(state, env);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    /* All 3 entries in row 1 must be marked -1 */
    for (int64_t k = 0; k < 3; k++) {
        TEST_ASSERT_EQUAL_INT(-1, state->csr_col_idx[k]);
    }

    /* Column counts decremented */
    TEST_ASSERT_EQUAL_INT(0, state->col_nz_count[0]);
    TEST_ASSERT_EQUAL_INT(0, state->col_nz_count[1]);
    TEST_ASSERT_EQUAL_INT(0, state->col_nz_count[2]);

    free_csr_state(state);
}

/*******************************************************************************
 * Test: already-inactive entries (col_idx == -1) are not double-decremented
 ******************************************************************************/
void test_sentinel_skips_already_inactive(void) {
    /* Row 1 has 2 entries: col 0 active, col -1 already inactive */
    double rhs[] = {5.0, 3.0};
    char sense[] = {'<', '<'};
    int64_t row_ptr[] = {0, 0, 2};
    int col_idx[] = {0, -1};  /* second entry already inactive */

    SolverState *state = make_csr_state(2, 2, rhs, sense,
                                        row_ptr, col_idx, 2);
    state->row_status[1] = 1;
    state->col_nz_count[0] = 5;  /* start at 5 */

    int rc = cxf_simplex_crash(state, env);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    /* col 0 decremented once (from 5 to 4) */
    TEST_ASSERT_EQUAL_INT(4, state->col_nz_count[0]);

    /* Both entries now -1 */
    TEST_ASSERT_EQUAL_INT(-1, state->csr_col_idx[0]);
    TEST_ASSERT_EQUAL_INT(-1, state->csr_col_idx[1]);

    free_csr_state(state);
}

/*******************************************************************************
 * Test: model MatrixData CSR is not modified by crash
 ******************************************************************************/
void test_model_csr_untouched(void) {
    double rhs[] = {5.0, 3.0};
    char sense[] = {'<', '<'};
    int64_t row_ptr[] = {0, 0, 2};
    int col_idx[] = {0, 1};

    SolverState *state = make_csr_state(2, 2, rhs, sense,
                                        row_ptr, col_idx, 2);
    state->row_status[1] = 1;
    state->col_nz_count[0] = 1;
    state->col_nz_count[1] = 1;

    /* Set model CSR to the same data (different memory) */
    MatrixData *mat = state->model_ref->matrix;
    if (mat == NULL) {
        mat = (MatrixData *)calloc(1, sizeof(MatrixData));
        state->model_ref->matrix = mat;
    }
    mat->num_rows = 2;
    mat->num_cols = 2;
    mat->row_ptr = (int64_t *)malloc(3 * sizeof(int64_t));
    memcpy(mat->row_ptr, row_ptr, 3 * sizeof(int64_t));
    mat->col_idx = (int *)malloc(2 * sizeof(int));
    memcpy(mat->col_idx, col_idx, 2 * sizeof(int));

    int rc = cxf_simplex_crash(state, env);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    /* SolverState CSR has sentinels */
    TEST_ASSERT_EQUAL_INT(-1, state->csr_col_idx[0]);
    TEST_ASSERT_EQUAL_INT(-1, state->csr_col_idx[1]);

    /* Model CSR must be untouched */
    TEST_ASSERT_EQUAL_INT(0, mat->col_idx[0]);
    TEST_ASSERT_EQUAL_INT(1, mat->col_idx[1]);

    free_csr_state(state);
}

/*******************************************************************************
 * Test: crash reads RHS/sense from SolverState, not model
 ******************************************************************************/
void test_reads_state_rhs_not_model(void) {
    /* SolverState has feasible RHS, model has infeasible RHS.
     * Crash should see feasible because it reads state. */
    double rhs_state[] = {5.0};
    char sense_state[] = {'<'};
    int64_t row_ptr[] = {0, 0};
    int col_idx_dummy[] = {0};  /* unused, no entries */

    SolverState *state = make_csr_state(1, 1, rhs_state, sense_state,
                                        row_ptr, col_idx_dummy, 0);

    /* Set model RHS to infeasible value */
    MatrixData *mat = state->model_ref->matrix;
    if (mat == NULL) {
        mat = (MatrixData *)calloc(1, sizeof(MatrixData));
        state->model_ref->matrix = mat;
    }
    mat->num_rows = 1;
    mat->rhs = (double *)malloc(sizeof(double));
    mat->rhs[0] = -100.0;  /* Infeasible */
    mat->sense = (char *)malloc(sizeof(char));
    mat->sense[0] = '<';

    int rc = cxf_simplex_crash(state, env);

    /* Must be OK because SolverState RHS is 5.0 (feasible) */
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_EQUAL_INT(1, state->num_basic);

    free_csr_state(state);
}

/*******************************************************************************
 * Runner
 ******************************************************************************/
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_sentinel_marking_on_removal);
    RUN_TEST(test_sentinel_skips_already_inactive);
    RUN_TEST(test_model_csr_untouched);
    RUN_TEST(test_reads_state_rhs_not_model);
    return UNITY_END();
}
