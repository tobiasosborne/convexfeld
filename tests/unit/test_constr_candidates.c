/**
 * @file test_constr_candidates.c
 * @brief Tests for V2 constraint-side pricing (P3.18)
 *
 * Tests cxf_pricing_get_constr_stats and cxf_pricing_constr_candidates_v2.
 * Beads: convexfeld-ro2u
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
    free(s);
}

void setUp(void) {}
void tearDown(void) {}

/*--- cxf_pricing_get_constr_stats tests ---*/

void test_constr_stats_returns_committed(void) {
    PricingState *ctx = cxf_pricing_create(5, 3);
    cxf_pricing_init(ctx, 5, 1);
    cxf_pricing_init_constrs(ctx, 3);

    ctx->current_level = 1;
    ctx->constr_q_committed[1] = 2;
    ctx->constr_queue[1][0] = 0;
    ctx->constr_queue[1][1] = 2;

    int count = 0;
    int *queue = NULL;
    cxf_pricing_get_constr_stats(ctx, &count, &queue);

    TEST_ASSERT_EQUAL_INT(2, count);
    TEST_ASSERT_EQUAL_PTR(ctx->constr_queue[1], queue);

    cxf_pricing_free(ctx);
}

void test_constr_stats_null_safety(void) {
    int count = 99;
    int *queue = (int *)1;

    cxf_pricing_get_constr_stats(NULL, &count, &queue);
    TEST_ASSERT_EQUAL_INT(0, count);
    TEST_ASSERT_NULL(queue);

    /* NULL count_out */
    cxf_pricing_get_constr_stats(NULL, NULL, &queue);
    TEST_ASSERT_NULL(queue);

    /* NULL queue_out */
    count = 99;
    cxf_pricing_get_constr_stats(NULL, &count, NULL);
    TEST_ASSERT_EQUAL_INT(0, count);
}

/*--- cxf_pricing_constr_candidates_v2 tests ---*/

void test_constr_v2_level0_fast_path(void) {
    PricingState *ctx = cxf_pricing_create(5, 3);
    cxf_pricing_init(ctx, 5, 1);
    cxf_pricing_init_constrs(ctx, 3);
    SolverState *s = create_test_state(5, 3);

    /* Populate level 0 constraint queue */
    ctx->constr_queue[0][0] = 1;
    ctx->constr_queue[0][1] = 2;
    ctx->constr_q_committed[0] = 2;
    ctx->constr_q_total[0] = 2;
    ctx->current_level = 0;

    int count = 0;
    int *cands = NULL;
    cxf_pricing_constr_candidates_v2(ctx, s, &count, &cands);

    TEST_ASSERT_EQUAL_INT(2, count);
    TEST_ASSERT_NOT_NULL(cands);
    TEST_ASSERT_EQUAL_PTR(ctx->constr_queue[0], cands);

    cxf_pricing_free(ctx);
    free_test_state(s);
}

void test_constr_v2_cache_hit(void) {
    PricingState *ctx = cxf_pricing_create(5, 3);
    cxf_pricing_init(ctx, 5, 1);
    cxf_pricing_init_constrs(ctx, 3);
    SolverState *s = create_test_state(5, 3);

    ctx->current_level = 1;
    ctx->cached_constr_count[1] = 2;
    ctx->constr_output_buf[1][0] = 0;
    ctx->constr_output_buf[1][1] = 2;

    int count = 0;
    int *cands = NULL;
    cxf_pricing_constr_candidates_v2(ctx, s, &count, &cands);

    TEST_ASSERT_EQUAL_INT(2, count);
    TEST_ASSERT_EQUAL_PTR(ctx->constr_output_buf[1], cands);

    cxf_pricing_free(ctx);
    free_test_state(s);
}

void test_constr_v2_null_safety(void) {
    int count = 99;
    int *cands = (int *)1;
    cxf_pricing_constr_candidates_v2(NULL, NULL, &count, &cands);
    TEST_ASSERT_EQUAL_INT(0, count);
    TEST_ASSERT_NULL(cands);
}

void test_constr_v2_full_scan_small_problem(void) {
    /* With a tiny problem (m=2) and large cross-queue committed count,
     * the cross-queue density check should trigger a full scan. */
    PricingState *ctx = cxf_pricing_create(5, 3);
    cxf_pricing_init(ctx, 5, 1);
    cxf_pricing_init_constrs(ctx, 2);
    SolverState *s = create_test_state(5, 2);

    ctx->current_level = 1;
    ctx->cached_constr_count[1] = -1; /* Cache miss */
    /* Large cross-queue committed: var_q_committed[1] = 5
     * => m=2 <= 5*2.0=10 => full scan */
    ctx->var_q_committed[1] = 5;

    int count = 0;
    int *cands = NULL;
    cxf_pricing_constr_candidates_v2(ctx, s, &count, &cands);

    /* Full scan returns all m=2 constraints */
    TEST_ASSERT_EQUAL_INT(2, count);
    TEST_ASSERT_NOT_NULL(cands);
    TEST_ASSERT_EQUAL_INT(0, cands[0]);
    TEST_ASSERT_EQUAL_INT(1, cands[1]);

    /* Result should be cached */
    TEST_ASSERT_EQUAL_INT(2, ctx->cached_constr_count[1]);

    cxf_pricing_free(ctx);
    free_test_state(s);
}

int main(void) {
    UNITY_BEGIN();

    /* P3.18: get_constr_stats */
    RUN_TEST(test_constr_stats_returns_committed);
    RUN_TEST(test_constr_stats_null_safety);

    /* P3.18: constr_candidates_v2 */
    RUN_TEST(test_constr_v2_level0_fast_path);
    RUN_TEST(test_constr_v2_cache_hit);
    RUN_TEST(test_constr_v2_null_safety);
    RUN_TEST(test_constr_v2_full_scan_small_problem);

    return UNITY_END();
}
