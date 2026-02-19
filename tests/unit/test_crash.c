/**
 * @file test_crash.c
 * @brief Tests for crash basis construction (v2 — P2.5)
 *
 * Tests cxf_simplex_crash row-scanning algorithm:
 * feasibility detection, candidate removal, edge cases.
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
extern int cxf_sparse_build_csr(MatrixData *mat);

static CxfEnv *env = NULL;

void setUp(void) {
    cxf_loadenv(&env, NULL);
}

void tearDown(void) {
    cxf_freeenv(env);
    env = NULL;
}

/*******************************************************************************
 * Helper: build a minimal SolverState with crash arrays for testing
 ******************************************************************************/
static SolverState *make_crash_state(int n, int m, double *rhs, char *sense) {
    CxfModel *model;
    cxf_newmodel(env, &model, "crash_test", 0, NULL, NULL, NULL, NULL, NULL);

    /* Add n dummy variables */
    for (int j = 0; j < n; j++) {
        cxf_addvar(model, 0, NULL, NULL, 0.0, 0.0, 10.0, 'C', NULL);
    }

    SolverState *state = NULL;
    cxf_simplex_init(model, &state);

    /* Override constraint count and set up crash arrays */
    state->num_constrs = m;

    /* Reallocate crash arrays for overridden m (init used model's m=0) */
    if (m > 0) {
        free(state->row_status);
        state->row_status = (int *)calloc((size_t)m, sizeof(int));
    }
    {
        int total = n + m;
        free(state->col_nz_count);
        state->col_nz_count = (int *)calloc((size_t)total, sizeof(int));
    }

    /* Set up matrix with RHS and sense */
    if (model->matrix == NULL) {
        model->matrix = (MatrixData *)calloc(1, sizeof(MatrixData));
        model->matrix->num_rows = m;
        model->matrix->num_cols = n;
    }
    MatrixData *mat = model->matrix;
    mat->num_rows = m;
    if (m > 0) {
        mat->rhs = (double *)malloc((size_t)m * sizeof(double));
        mat->sense = (char *)malloc((size_t)m * sizeof(char));
        memcpy(mat->rhs, rhs, (size_t)m * sizeof(double));
        memcpy(mat->sense, sense, (size_t)m * sizeof(char));
    }

    state->num_basic = 0;
    state->problem_row_index = -1;

    return state;
}

static void free_crash_state(SolverState *state) {
    cxf_freemodel(state->model_ref);
    /* state is freed by freemodel -> final chain, but we allocated
     * extra arrays. The simplex_final should handle row_status/col_nz_count
     * since they're now part of the struct cleanup. */
    cxf_simplex_final(state);
}

/*******************************************************************************
 * Null argument tests
 ******************************************************************************/
void test_crash_null_state(void) {
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT,
                          cxf_simplex_crash(NULL, env));
}

void test_crash_null_env(void) {
    SolverState dummy;
    memset(&dummy, 0, sizeof(dummy));
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT,
                          cxf_simplex_crash(&dummy, NULL));
}

/*******************************************************************************
 * Empty problem
 ******************************************************************************/
void test_crash_empty_problem(void) {
    SolverState dummy;
    memset(&dummy, 0, sizeof(dummy));
    dummy.num_constrs = 0;
    TEST_ASSERT_EQUAL_INT(CXF_OK, cxf_simplex_crash(&dummy, env));
}

/*******************************************************************************
 * Feasible inequality constraints (all <= with non-negative RHS)
 ******************************************************************************/
void test_crash_feasible_inequalities(void) {
    double rhs[] = {5.0, 10.0, 0.0};
    char sense[] = {'<', '<', '<'};
    SolverState *state = make_crash_state(2, 3, rhs, sense);

    int rc = cxf_simplex_crash(state, env);

    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_EQUAL_INT(3, state->num_basic);
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT_EQUAL_INT(CXF_ROW_BASIC_LOWER, state->row_status[i]);
    }
    TEST_ASSERT_EQUAL_INT(-1, state->problem_row_index);

    free_crash_state(state);
}

/*******************************************************************************
 * Infeasible inequality (negative RHS beyond tolerance)
 ******************************************************************************/
void test_crash_infeasible_inequality(void) {
    double rhs[] = {5.0, -1.0, 3.0};
    char sense[] = {'<', '<', '<'};
    SolverState *state = make_crash_state(2, 3, rhs, sense);

    int rc = cxf_simplex_crash(state, env);

    TEST_ASSERT_EQUAL_INT(CXF_INFEASIBLE, rc);
    TEST_ASSERT_EQUAL_INT(1, state->problem_row_index);
    /* First row was processed before infeasible one */
    TEST_ASSERT_EQUAL_INT(1, state->num_basic);

    free_crash_state(state);
}

/*******************************************************************************
 * Infeasible equality (non-zero RHS)
 ******************************************************************************/
void test_crash_infeasible_equality(void) {
    double rhs[] = {0.0, 5.0};
    char sense[] = {'=', '='};
    SolverState *state = make_crash_state(2, 2, rhs, sense);

    int rc = cxf_simplex_crash(state, env);

    TEST_ASSERT_EQUAL_INT(CXF_INFEASIBLE, rc);
    TEST_ASSERT_EQUAL_INT(1, state->problem_row_index);
    TEST_ASSERT_EQUAL_INT(1, state->num_basic);  /* Row 0 OK, row 1 fails */

    free_crash_state(state);
}

/*******************************************************************************
 * Feasible equality (near-zero RHS within tolerance)
 ******************************************************************************/
void test_crash_feasible_equality_near_zero(void) {
    double rhs[] = {1e-8, -1e-9};
    char sense[] = {'=', '='};
    SolverState *state = make_crash_state(2, 2, rhs, sense);

    int rc = cxf_simplex_crash(state, env);

    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_EQUAL_INT(2, state->num_basic);

    free_crash_state(state);
}

/*******************************************************************************
 * Mixed constraint senses
 ******************************************************************************/
void test_crash_mixed_senses(void) {
    double rhs[] = {5.0, 1e-9, 3.0};
    char sense[] = {'<', '=', '>'};
    SolverState *state = make_crash_state(2, 3, rhs, sense);

    int rc = cxf_simplex_crash(state, env);

    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_EQUAL_INT(3, state->num_basic);

    free_crash_state(state);
}

/*******************************************************************************
 * Inequality at tolerance boundary: rhs = -epsilon_feas is feasible
 ******************************************************************************/
void test_crash_inequality_at_boundary(void) {
    double rhs[] = {-1e-6};  /* Exactly -epsilon_feas */
    char sense[] = {'<'};
    SolverState *state = make_crash_state(1, 1, rhs, sense);

    int rc = cxf_simplex_crash(state, env);

    /* rhs < -epsilon_feas is infeasible; rhs == -epsilon_feas is feasible */
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_EQUAL_INT(1, state->num_basic);

    free_crash_state(state);
}

/*******************************************************************************
 * Candidate row removal with CSR
 ******************************************************************************/
void test_crash_candidate_removal(void) {
    /* 2 vars, 2 constraints. Row 1 is a candidate (status > 0). */
    double rhs[] = {5.0, 3.0};
    char sense[] = {'<', '<'};
    SolverState *state = make_crash_state(2, 2, rhs, sense);

    /* Build a minimal CSR for row 1: row 1 has entries in cols 0 and 1 */
    MatrixData *mat = state->model_ref->matrix;

    /* CSC: col 0 has row {1}, col 1 has row {1}
     * This means row 1 has 2 nonzeros. */
    free(mat->col_ptr);
    free(mat->row_idx);
    free(mat->values);
    mat->num_cols = 2;
    mat->num_rows = 2;
    mat->nnz = 2;
    mat->col_ptr = (int64_t *)calloc(3, sizeof(int64_t));
    mat->row_idx = (int *)malloc(2 * sizeof(int));
    mat->values = (double *)malloc(2 * sizeof(double));
    mat->col_ptr[0] = 0; mat->col_ptr[1] = 1; mat->col_ptr[2] = 2;
    mat->row_idx[0] = 1; mat->row_idx[1] = 1;
    mat->values[0] = 2.0; mat->values[1] = 3.0;

    /* Build CSR from CSC */
    cxf_sparse_build_csr(mat);

    /* Set col_nz_count: col 0 has 1 nz, col 1 has 1 nz */
    state->col_nz_count[0] = 1;
    state->col_nz_count[1] = 1;

    /* Mark row 1 as candidate */
    state->row_status[0] = CXF_ROW_UNASSIGNED;
    state->row_status[1] = 1;  /* Candidate */

    int rc = cxf_simplex_crash(state, env);

    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_EQUAL_INT(2, state->num_basic);
    TEST_ASSERT_EQUAL_INT(CXF_ROW_BASIC_LOWER, state->row_status[0]);
    TEST_ASSERT_EQUAL_INT(CXF_ROW_BASIC_UPPER, state->row_status[1]);

    /* Column nonzero counts should be decremented */
    TEST_ASSERT_EQUAL_INT(0, state->col_nz_count[0]);
    TEST_ASSERT_EQUAL_INT(0, state->col_nz_count[1]);

    /* CSR col_idx entries should be marked inactive */
    int64_t r1_start = mat->row_ptr[1];
    int64_t r1_end = mat->row_ptr[2];
    for (int64_t k = r1_start; k < r1_end; k++) {
        TEST_ASSERT_EQUAL_INT(-1, mat->col_idx[k]);
    }

    free_crash_state(state);
}

/*******************************************************************************
 * Candidate row with equality sense should NOT be removed
 ******************************************************************************/
void test_crash_candidate_equality_not_removed(void) {
    double rhs[] = {5.0};
    char sense[] = {'='};
    SolverState *state = make_crash_state(1, 1, rhs, sense);
    state->row_status[0] = 1;  /* Candidate, but equality */

    int rc = cxf_simplex_crash(state, env);

    /* Candidate with '=' sense: not removed, not assigned BASIC_LOWER either */
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_EQUAL_INT(0, state->num_basic);
    TEST_ASSERT_EQUAL_INT(1, state->row_status[0]);  /* Unchanged */

    free_crash_state(state);
}

/*******************************************************************************
 * Runner
 ******************************************************************************/
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_crash_null_state);
    RUN_TEST(test_crash_null_env);
    RUN_TEST(test_crash_empty_problem);
    RUN_TEST(test_crash_feasible_inequalities);
    RUN_TEST(test_crash_infeasible_inequality);
    RUN_TEST(test_crash_infeasible_equality);
    RUN_TEST(test_crash_feasible_equality_near_zero);
    RUN_TEST(test_crash_mixed_senses);
    RUN_TEST(test_crash_inequality_at_boundary);
    RUN_TEST(test_crash_candidate_removal);
    RUN_TEST(test_crash_candidate_equality_not_removed);
    return UNITY_END();
}
