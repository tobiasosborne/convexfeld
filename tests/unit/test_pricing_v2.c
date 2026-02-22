/**
 * @file test_pricing_v2.c
 * @brief Tests for V2 pricing queue system (P4.2-P4.5)
 *
 * Tests queue insertion, update_var, update_constr, update_queues,
 * and candidates_v2 against the v2 spec.
 */

#include "unity.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_pricing.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_basis.h"
#include <stdlib.h>
#include <string.h>

/* Forward declarations */
PricingState *cxf_pricing_create(int num_vars, int max_levels);
void cxf_pricing_free(PricingState *ctx);
int cxf_pricing_init(PricingState *ctx, int num_vars, int strategy);
int cxf_pricing_init_constrs(PricingState *ctx, int num_constrs);
extern BasisState *cxf_basis_create(int m, int n);
extern void cxf_basis_free(BasisState *basis);

/* Minimal solver state for testing */
static SolverState *create_test_state(int n, int m) {
    SolverState *s = calloc(1, sizeof(SolverState));
    s->num_vars = n;
    s->num_constrs = m;
    s->basis = cxf_basis_create(m, n + m);
    /* Set all vars as nonbasic at lower bound */
    for (int j = 0; j < n; j++)
        s->basis->var_status[j] = CXF_VAR_AT_LOWER;
    for (int i = 0; i < m; i++) {
        s->basis->basic_vars[i] = n + i;
        s->basis->var_status[n + i] = i;
    }
    return s;
}

static void free_test_state(SolverState *s) {
    if (!s) return;
    cxf_basis_free(s->basis);
    free(s->csc_col_ptr);
    free(s->csc_row_idx);
    free(s->csc_values);
    free(s->csr_row_ptr);
    free(s->csr_col_idx);
    free(s->csr_values);
    free(s);
}

/* Build a simple 2x3 CSC matrix:
 * Col 0: rows {0,1}, Col 1: rows {0}, Col 2: rows {1} */
static void add_test_matrix(SolverState *s) {
    int n = 3, m = 2;
    /* CSC: col_ptr = [0, 2, 3, 4], row_idx = [0,1, 0, 1] */
    s->csc_col_ptr = malloc(4 * sizeof(int64_t));
    s->csc_col_ptr[0] = 0; s->csc_col_ptr[1] = 2;
    s->csc_col_ptr[2] = 3; s->csc_col_ptr[3] = 4;
    s->csc_row_idx = malloc(4 * sizeof(int));
    s->csc_row_idx[0] = 0; s->csc_row_idx[1] = 1;
    s->csc_row_idx[2] = 0; s->csc_row_idx[3] = 1;
    s->csc_values = calloc(4, sizeof(double));
    s->num_nonzeros = 4;
    /* CSR: row_ptr = [0, 2, 4], col_idx = [0,1, 0,2] */
    s->csr_row_ptr = malloc(3 * sizeof(int64_t));
    s->csr_row_ptr[0] = 0; s->csr_row_ptr[1] = 2; s->csr_row_ptr[2] = 4;
    s->csr_col_idx = malloc(4 * sizeof(int));
    s->csr_col_idx[0] = 0; s->csr_col_idx[1] = 1;
    s->csr_col_idx[2] = 0; s->csr_col_idx[3] = 2;
    s->csr_values = calloc(4, sizeof(double));
    (void)n; (void)m;
}

void setUp(void) {}
void tearDown(void) {}

/*--- v2_insert_constr / v2_insert_var tests ---*/

void test_v2_insert_constr_basic(void) {
    PricingState *ctx = cxf_pricing_create(5, 3);
    cxf_pricing_init(ctx, 5, 1);
    cxf_pricing_init_constrs(ctx, 3);

    v2_insert_constr(ctx, 0);
    TEST_ASSERT_EQUAL_INT(1, ctx->constr_q_committed[1]);
    TEST_ASSERT_EQUAL_INT(1, ctx->constr_q_total[1]);
    TEST_ASSERT_EQUAL_INT(1, ctx->constr_q_committed[2]);
    TEST_ASSERT_EQUAL_INT(1, ctx->constr_q_total[2]);
    TEST_ASSERT_EQUAL_INT(0, ctx->constr_queue[1][0]);
    TEST_ASSERT_EQUAL_INT(0, ctx->constr_queue[2][0]);
    /* Flag bits should be set */
    TEST_ASSERT_TRUE(ctx->constr_flags[0] & 0x01);
    TEST_ASSERT_TRUE(ctx->constr_flags[0] & 0x04);

    cxf_pricing_free(ctx);
}

void test_v2_insert_constr_no_duplicate(void) {
    PricingState *ctx = cxf_pricing_create(5, 3);
    cxf_pricing_init(ctx, 5, 1);
    cxf_pricing_init_constrs(ctx, 3);

    v2_insert_constr(ctx, 1);
    v2_insert_constr(ctx, 1);  /* duplicate */
    TEST_ASSERT_EQUAL_INT(1, ctx->constr_q_committed[1]);
    TEST_ASSERT_EQUAL_INT(1, ctx->constr_q_total[1]);

    cxf_pricing_free(ctx);
}

void test_v2_insert_var_pending_when_active(void) {
    PricingState *ctx = cxf_pricing_create(5, 3);
    cxf_pricing_init(ctx, 5, 1);

    ctx->level_active[1] = 1;  /* Activate level 1 */
    v2_insert_var(ctx, 2);

    /* Should be in pending (total only, not committed) */
    TEST_ASSERT_EQUAL_INT(0, ctx->var_q_committed[1]);
    TEST_ASSERT_EQUAL_INT(1, ctx->var_q_total[1]);
    /* Pending bit set */
    TEST_ASSERT_TRUE(ctx->var_flags[2] & 0x02);

    cxf_pricing_free(ctx);
}

/*--- cxf_pricing_update_var tests (P4.2) ---*/

void test_update_var_csc_traversal(void) {
    SolverState *s = create_test_state(3, 2);
    add_test_matrix(s);
    PricingState *ctx = cxf_pricing_create(3, 3);
    cxf_pricing_init(ctx, 3, 1);
    cxf_pricing_init_constrs(ctx, 2);

    /* Variable 0 has nonzeros in rows {0, 1} */
    cxf_pricing_update_var(ctx, s, 0);

    TEST_ASSERT_EQUAL_INT(2, ctx->constr_q_committed[1]);
    TEST_ASSERT_EQUAL_INT(2, ctx->constr_q_total[1]);
    /* Rows 0 and 1 should be in queue */
    int found0 = 0, found1 = 0;
    for (int i = 0; i < ctx->constr_q_committed[1]; i++) {
        if (ctx->constr_queue[1][i] == 0) found0 = 1;
        if (ctx->constr_queue[1][i] == 1) found1 = 1;
    }
    TEST_ASSERT_TRUE(found0);
    TEST_ASSERT_TRUE(found1);

    cxf_pricing_free(ctx);
    free_test_state(s);
}

void test_update_var_single_column(void) {
    SolverState *s = create_test_state(3, 2);
    add_test_matrix(s);
    PricingState *ctx = cxf_pricing_create(3, 3);
    cxf_pricing_init(ctx, 3, 1);
    cxf_pricing_init_constrs(ctx, 2);

    /* Variable 1 has nonzeros in rows {0} only */
    cxf_pricing_update_var(ctx, s, 1);
    TEST_ASSERT_EQUAL_INT(1, ctx->constr_q_committed[1]);
    TEST_ASSERT_EQUAL_INT(0, ctx->constr_queue[1][0]);

    cxf_pricing_free(ctx);
    free_test_state(s);
}

/*--- cxf_pricing_update_constr tests (P4.3) ---*/

void test_update_constr_csr_traversal(void) {
    SolverState *s = create_test_state(3, 2);
    add_test_matrix(s);
    PricingState *ctx = cxf_pricing_create(3, 3);
    cxf_pricing_init(ctx, 3, 1);
    cxf_pricing_init_constrs(ctx, 2);

    /* Constraint 0 has nonzeros in cols {0, 1} */
    cxf_pricing_update_constr(ctx, s, 0);

    TEST_ASSERT_EQUAL_INT(2, ctx->var_q_committed[1]);
    int found0 = 0, found1 = 0;
    for (int i = 0; i < ctx->var_q_committed[1]; i++) {
        if (ctx->var_queue[1][i] == 0) found0 = 1;
        if (ctx->var_queue[1][i] == 1) found1 = 1;
    }
    TEST_ASSERT_TRUE(found0);
    TEST_ASSERT_TRUE(found1);

    cxf_pricing_free(ctx);
    free_test_state(s);
}

/*--- cxf_pricing_update_queues tests (P4.4) ---*/

void test_update_queues_activation_guard(void) {
    PricingState *ctx = cxf_pricing_create(5, 3);
    cxf_pricing_init(ctx, 5, 1);
    cxf_pricing_init_constrs(ctx, 3);
    SolverState *s = create_test_state(5, 3);

    ctx->current_level = 1;
    TEST_ASSERT_EQUAL_INT(0, ctx->level_active[1]);

    cxf_pricing_update_queues(ctx, s);
    /* First call just activates */
    TEST_ASSERT_EQUAL_INT(1, ctx->level_active[1]);

    cxf_pricing_free(ctx);
    free_test_state(s);
}

void test_update_queues_promote_pending(void) {
    PricingState *ctx = cxf_pricing_create(5, 3);
    cxf_pricing_init(ctx, 5, 1);
    cxf_pricing_init_constrs(ctx, 3);
    SolverState *s = create_test_state(5, 3);

    ctx->current_level = 1;
    ctx->level_active[1] = 1;  /* Already active */

    /* Insert var 0 with pending flag */
    ctx->var_flags[0] = 0x02;  /* L1 pending bit */
    ctx->var_queue[1][0] = 0;
    ctx->var_q_total[1] = 1;

    cxf_pricing_update_queues(ctx, s);

    /* Should be promoted: committed bit set, pending cleared */
    TEST_ASSERT_EQUAL_INT(1, ctx->var_q_committed[1]);
    TEST_ASSERT_TRUE(ctx->var_flags[0] & 0x01);
    TEST_ASSERT_FALSE(ctx->var_flags[0] & 0x02);
    /* Cache invalidated */
    TEST_ASSERT_EQUAL_INT(-1, ctx->cached_var_count[1]);

    cxf_pricing_free(ctx);
    free_test_state(s);
}

/*--- cxf_pricing_candidates_v2 tests (P4.5) ---*/

void test_candidates_v2_level0_fast_path(void) {
    PricingState *ctx = cxf_pricing_create(5, 3);
    cxf_pricing_init(ctx, 5, 1);
    SolverState *s = create_test_state(5, 3);

    /* Populate level 0 variable queue */
    ctx->var_queue[0][0] = 1;
    ctx->var_queue[0][1] = 3;
    ctx->var_q_committed[0] = 2;
    ctx->var_q_total[0] = 2;
    ctx->current_level = 0;

    int count = 0;
    int *cands = NULL;
    cxf_pricing_candidates_v2(ctx, s, &count, &cands);

    TEST_ASSERT_EQUAL_INT(2, count);
    TEST_ASSERT_NOT_NULL(cands);
    TEST_ASSERT_EQUAL_PTR(ctx->var_queue[0], cands);

    cxf_pricing_free(ctx);
    free_test_state(s);
}

void test_candidates_v2_cache_hit(void) {
    PricingState *ctx = cxf_pricing_create(5, 3);
    cxf_pricing_init(ctx, 5, 1);
    SolverState *s = create_test_state(5, 3);

    ctx->current_level = 1;
    ctx->cached_var_count[1] = 3;
    ctx->var_output_buf[1][0] = 0;
    ctx->var_output_buf[1][1] = 2;
    ctx->var_output_buf[1][2] = 4;

    int count = 0;
    int *cands = NULL;
    cxf_pricing_candidates_v2(ctx, s, &count, &cands);

    TEST_ASSERT_EQUAL_INT(3, count);
    TEST_ASSERT_EQUAL_PTR(ctx->var_output_buf[1], cands);

    cxf_pricing_free(ctx);
    free_test_state(s);
}

void test_candidates_v2_null_safety(void) {
    int count = 99;
    int *cands = (int *)1;
    cxf_pricing_candidates_v2(NULL, NULL, &count, &cands);
    TEST_ASSERT_EQUAL_INT(0, count);
    TEST_ASSERT_NULL(cands);
}

int main(void) {
    UNITY_BEGIN();

    /* V2 insertion helpers */
    RUN_TEST(test_v2_insert_constr_basic);
    RUN_TEST(test_v2_insert_constr_no_duplicate);
    RUN_TEST(test_v2_insert_var_pending_when_active);

    /* P4.2: update_var */
    RUN_TEST(test_update_var_csc_traversal);
    RUN_TEST(test_update_var_single_column);

    /* P4.3: update_constr */
    RUN_TEST(test_update_constr_csr_traversal);

    /* P4.4: update_queues */
    RUN_TEST(test_update_queues_activation_guard);
    RUN_TEST(test_update_queues_promote_pending);

    /* P4.5: candidates_v2 */
    RUN_TEST(test_candidates_v2_level0_fast_path);
    RUN_TEST(test_candidates_v2_cache_hit);
    RUN_TEST(test_candidates_v2_null_safety);

    return UNITY_END();
}
