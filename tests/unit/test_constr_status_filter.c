/**
 * @file test_constr_status_filter.c
 * @brief Constraint status validity filtering (pricing_support.md)
 * Beads: convexfeld-u1qe, convexfeld-rcul
 */
#include "unity.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_pricing.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_basis.h"
#include <stdlib.h>
#include <string.h>

PricingState *cxf_pricing_create(int num_vars, int max_levels);
void cxf_pricing_free(PricingState *ctx);
int cxf_pricing_init(PricingState *ctx, int num_vars, int strategy);
int cxf_pricing_init_constrs(PricingState *ctx, int num_constrs);
extern BasisState *cxf_basis_create(int m, int n);
extern void cxf_basis_free(BasisState *basis);

/* All constraints valid by default; set var_status[n+i] < 0 for invalid */
static SolverState *create_test_state(int n, int m) {
    SolverState *s = calloc(1, sizeof(SolverState));
    s->num_vars = n;
    s->num_constrs = m;
    s->basis = cxf_basis_create(m, n + m);
    for (int j = 0; j < n; j++)
        s->basis->var_status[j] = CXF_VAR_AT_LOWER;
    for (int i = 0; i < m; i++) {
        s->basis->basic_vars[i] = n + i;
        s->basis->var_status[n + i] = i; /* non-negative = valid */
    }
    return s;
}

static void free_test_state(SolverState *s) {
    if (!s) return;
    cxf_basis_free(s->basis);
    free(s->csc_col_ptr); free(s->csc_row_idx); free(s->csc_values);
    free(s);
}
void setUp(void) {}
void tearDown(void) {}

/* convexfeld-rcul: Full scan must check constraint status */

void test_full_scan_excludes_negative_status(void) {
    int n = 5, m = 4;
    PricingState *ctx = cxf_pricing_create(n, 3);
    cxf_pricing_init(ctx, n, 1);
    cxf_pricing_init_constrs(ctx, m);
    SolverState *s = create_test_state(n, m);

    s->basis->var_status[n + 1] = -1; /* invalid */
    s->basis->var_status[n + 3] = -2; /* invalid */
    /* Force full scan: level 1, cache miss, large cross-queue */
    ctx->current_level = 1;
    ctx->cached_constr_count[1] = -1;
    ctx->var_q_committed[1] = 100;
    int count = 0;
    int *cands = NULL;
    cxf_pricing_constr_candidates_v2(ctx, s, &count, &cands);

    TEST_ASSERT_EQUAL_INT(2, count); /* only 0 and 2 valid */
    TEST_ASSERT_NOT_NULL(cands);
    TEST_ASSERT_EQUAL_INT(0, cands[0]);
    TEST_ASSERT_EQUAL_INT(2, cands[1]);

    cxf_pricing_free(ctx);
    free_test_state(s);
}

void test_full_scan_keeps_all_valid(void) {
    int n = 3, m = 3;
    PricingState *ctx = cxf_pricing_create(n, 3);
    cxf_pricing_init(ctx, n, 1);
    cxf_pricing_init_constrs(ctx, m);
    SolverState *s = create_test_state(n, m);

    ctx->current_level = 1;
    ctx->cached_constr_count[1] = -1;
    ctx->var_q_committed[1] = 100;
    int count = 0; int *cands = NULL;
    cxf_pricing_constr_candidates_v2(ctx, s, &count, &cands);
    TEST_ASSERT_EQUAL_INT(3, count);
    cxf_pricing_free(ctx); free_test_state(s);
}

void test_full_scan_excludes_all_invalid(void) {
    int n = 3, m = 2;
    PricingState *ctx = cxf_pricing_create(n, 3);
    cxf_pricing_init(ctx, n, 1);
    cxf_pricing_init_constrs(ctx, m);
    SolverState *s = create_test_state(n, m);

    /* All constraints invalid */
    s->basis->var_status[n + 0] = -1;
    s->basis->var_status[n + 1] = -1;

    ctx->current_level = 1;
    ctx->cached_constr_count[1] = -1;
    ctx->var_q_committed[1] = 100;

    int count = 0;
    int *cands = NULL;
    cxf_pricing_constr_candidates_v2(ctx, s, &count, &cands);

    TEST_ASSERT_EQUAL_INT(0, count);

    cxf_pricing_free(ctx);
    free_test_state(s);
}

/* convexfeld-u1qe: Partial expansion filter must check constraint status */

void test_partial_expansion_excludes_negative_status(void) {
    int n = 5, m = 100;
    PricingState *ctx = cxf_pricing_create(n, 3);
    cxf_pricing_init(ctx, n, 1);
    cxf_pricing_init_constrs(ctx, m);
    SolverState *s = create_test_state(n, m);

    /* Force partial expansion: large m, small queues, no CSC */
    ctx->current_level = 1;
    ctx->cached_constr_count[1] = -1;
    ctx->var_q_committed[1] = 1;
    ctx->constr_q_total[1] = 2;
    ctx->constr_q_committed[1] = 3;
    ctx->constr_queue[1][0] = 5;
    ctx->constr_queue[1][1] = 10;
    ctx->constr_queue[1][2] = 20;

    /* Mark constraint 10 as invalid */
    s->basis->var_status[n + 10] = -1;

    int count = 0;
    int *cands = NULL;
    cxf_pricing_constr_candidates_v2(ctx, s, &count, &cands);

    /* Constraint 10 should be excluded, 5 and 20 retained */
    TEST_ASSERT_EQUAL_INT(2, count);
    int found_5 = 0, found_20 = 0, found_10 = 0;
    for (int i = 0; i < count; i++) {
        if (cands[i] == 5)  found_5 = 1;
        if (cands[i] == 10) found_10 = 1;
        if (cands[i] == 20) found_20 = 1;
    }
    TEST_ASSERT_TRUE(found_5);
    TEST_ASSERT_TRUE(found_20);
    TEST_ASSERT_FALSE(found_10);

    cxf_pricing_free(ctx);
    free_test_state(s);
}

void test_partial_expansion_keeps_valid(void) {
    int n = 5, m = 100;
    PricingState *ctx = cxf_pricing_create(n, 3);
    cxf_pricing_init(ctx, n, 1);
    cxf_pricing_init_constrs(ctx, m);
    SolverState *s = create_test_state(n, m);

    /* All constraints valid (default) */
    ctx->current_level = 1;
    ctx->cached_constr_count[1] = -1;
    ctx->var_q_committed[1] = 1;
    ctx->constr_q_total[1] = 2;
    ctx->constr_q_committed[1] = 2;
    ctx->constr_queue[1][0] = 3;
    ctx->constr_queue[1][1] = 7;

    int count = 0;
    int *cands = NULL;
    cxf_pricing_constr_candidates_v2(ctx, s, &count, &cands);

    TEST_ASSERT_EQUAL_INT(2, count);

    cxf_pricing_free(ctx);
    free_test_state(s);
}

int main(void) {
    UNITY_BEGIN();

    /* convexfeld-rcul: full scan status filtering */
    RUN_TEST(test_full_scan_excludes_negative_status);
    RUN_TEST(test_full_scan_keeps_all_valid);
    RUN_TEST(test_full_scan_excludes_all_invalid);

    /* convexfeld-u1qe: partial expansion status filtering */
    RUN_TEST(test_partial_expansion_excludes_negative_status);
    RUN_TEST(test_partial_expansion_keeps_valid);

    return UNITY_END();
}
