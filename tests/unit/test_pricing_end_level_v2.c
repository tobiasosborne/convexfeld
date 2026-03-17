/**
 * @file test_pricing_end_level_v2.c
 * @brief Tests for V2 cxf_pricing_end_level queue filtering.
 *
 * Verifies the spec-compliant behavior from pricing_support.md:
 * - Signature: (PricingState*, SolverState*)
 * - L0: status-based compaction (non-negative retained)
 * - L1-L2: flag-based promote/demote
 * - Work counter incremented with pre-compaction sizes
 * - Lazy activation on first call
 *
 * Beads: convexfeld-au3b (signature), convexfeld-8nlv (filtering)
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
static SolverState *make_state(int n, int m) {
    SolverState *s = calloc(1, sizeof(SolverState));
    s->num_vars = n;
    s->num_constrs = m;
    s->basis = cxf_basis_create(m, n + m);
    /* Default: all vars nonbasic at lower, slacks basic */
    for (int j = 0; j < n; j++)
        s->basis->var_status[j] = CXF_VAR_AT_LOWER;  /* -1, nonbasic */
    for (int i = 0; i < m; i++) {
        s->basis->basic_vars[i] = n + i;
        s->basis->var_status[n + i] = i;  /* >= 0, basic */
    }
    return s;
}

static void free_state(SolverState *s) {
    if (!s) return;
    cxf_basis_free(s->basis);
    free(s);
}

void setUp(void) {}
void tearDown(void) {}

/*--- Level 0: status-based compaction ---*/

void test_l0_var_queue_filters_invalid(void) {
    /* L0 var queue: retain entries with non-negative status (basic). */
    PricingState *ctx = cxf_pricing_create(5, 3);
    cxf_pricing_init(ctx, 5, 1);
    cxf_pricing_init_constrs(ctx, 3);

    SolverState *st = make_state(5, 3);
    /* var 0: basic (status=0), var 1: nonbasic (status=-1),
       var 2: basic (status=1), var 3: nonbasic (status=-2) */
    st->basis->var_status[0] = 0;
    st->basis->var_status[1] = CXF_VAR_AT_LOWER;
    st->basis->var_status[2] = 1;
    st->basis->var_status[3] = CXF_VAR_AT_UPPER;

    ctx->current_level = 0;
    ctx->level_active[0] = 1;  /* Skip lazy activation */

    /* Populate var queue with indices 0..3 */
    ctx->var_queue[0][0] = 0; ctx->var_queue[0][1] = 1;
    ctx->var_queue[0][2] = 2; ctx->var_queue[0][3] = 3;
    ctx->var_q_total[0] = 4;
    ctx->var_q_committed[0] = 4;

    cxf_pricing_end_level(ctx, st);

    /* Only vars 0, 2 have non-negative status -> retained */
    TEST_ASSERT_EQUAL_INT(2, ctx->var_q_total[0]);
    TEST_ASSERT_EQUAL_INT(2, ctx->var_q_committed[0]);
    TEST_ASSERT_EQUAL_INT(0, ctx->var_queue[0][0]);
    TEST_ASSERT_EQUAL_INT(2, ctx->var_queue[0][1]);

    free_state(st);
    cxf_pricing_free(ctx);
}

void test_l0_constr_queue_filters_invalid(void) {
    /* L0 constr queue: retain entries whose slack status is non-negative.
       Constraint i maps to status[num_vars + i]. */
    PricingState *ctx = cxf_pricing_create(4, 3);
    cxf_pricing_init(ctx, 4, 1);
    cxf_pricing_init_constrs(ctx, 3);

    SolverState *st = make_state(4, 3);
    /* constr 0 -> var_status[4] = 0 (basic, valid)
       constr 1 -> var_status[5] = -1 (nonbasic, invalid for constr)
       constr 2 -> var_status[6] = 2 (basic, valid) */
    st->basis->var_status[4] = 0;
    st->basis->var_status[5] = CXF_VAR_AT_LOWER;
    st->basis->var_status[6] = 2;

    ctx->current_level = 0;
    ctx->level_active[0] = 1;

    ctx->constr_queue[0][0] = 0;
    ctx->constr_queue[0][1] = 1;
    ctx->constr_queue[0][2] = 2;
    ctx->constr_q_total[0] = 3;
    ctx->constr_q_committed[0] = 3;

    cxf_pricing_end_level(ctx, st);

    TEST_ASSERT_EQUAL_INT(2, ctx->constr_q_total[0]);
    TEST_ASSERT_EQUAL_INT(2, ctx->constr_q_committed[0]);
    TEST_ASSERT_EQUAL_INT(0, ctx->constr_queue[0][0]);
    TEST_ASSERT_EQUAL_INT(2, ctx->constr_queue[0][1]);

    free_state(st);
    cxf_pricing_free(ctx);
}

/*--- Level 1: promote/demote ---*/

void test_l1_promote_pending(void) {
    /* L1: entry with pending bit set should be promoted
       (committed bit set, pending cleared, retained). */
    PricingState *ctx = cxf_pricing_create(4, 3);
    cxf_pricing_init(ctx, 4, 1);
    cxf_pricing_init_constrs(ctx, 3);

    SolverState *st = make_state(4, 3);
    /* Make var 0 basic (non-negative) so it's not status-discarded */
    st->basis->var_status[0] = 0;

    ctx->current_level = 1;
    ctx->level_active[1] = 1;

    /* Set pending bit for var 0 at level 1 */
    ctx->var_flags[0] = 0x02;  /* L1_PENDING */
    ctx->var_queue[1][0] = 0;
    ctx->var_q_total[1] = 1;

    cxf_pricing_end_level(ctx, st);

    TEST_ASSERT_EQUAL_INT(1, ctx->var_q_total[1]);
    TEST_ASSERT_EQUAL_INT(0, ctx->var_queue[1][0]);
    /* Flags: committed bit set (0x01), pending cleared */
    TEST_ASSERT_EQUAL_INT(0x01, ctx->var_flags[0] & 0x03);

    free_state(st);
    cxf_pricing_free(ctx);
}

void test_l1_demote_stale(void) {
    /* L1: entry with committed bit but NO pending bit -> stale -> demoted. */
    PricingState *ctx = cxf_pricing_create(4, 3);
    cxf_pricing_init(ctx, 4, 1);
    cxf_pricing_init_constrs(ctx, 3);

    SolverState *st = make_state(4, 3);
    st->basis->var_status[1] = 0;  /* basic (valid) */

    ctx->current_level = 1;
    ctx->level_active[1] = 1;

    ctx->var_flags[1] = 0x01;  /* L1_COMMITTED only (stale) */
    ctx->var_queue[1][0] = 1;
    ctx->var_q_total[1] = 1;

    cxf_pricing_end_level(ctx, st);

    /* Entry demoted: queue should be empty */
    TEST_ASSERT_EQUAL_INT(0, ctx->var_q_total[1]);
    /* Flags: all L1 bits cleared */
    TEST_ASSERT_EQUAL_INT(0x00, ctx->var_flags[1] & 0x03);

    free_state(st);
    cxf_pricing_free(ctx);
}

void test_l1_discard_invalid_status(void) {
    /* L1: entry with negative status -> discarded regardless of flags. */
    PricingState *ctx = cxf_pricing_create(4, 3);
    cxf_pricing_init(ctx, 4, 1);
    cxf_pricing_init_constrs(ctx, 3);

    SolverState *st = make_state(4, 3);
    st->basis->var_status[2] = CXF_VAR_AT_LOWER;  /* -1, nonbasic */

    ctx->current_level = 1;
    ctx->level_active[1] = 1;

    ctx->var_flags[2] = 0x02;  /* L1_PENDING */
    ctx->var_queue[1][0] = 2;
    ctx->var_q_total[1] = 1;

    cxf_pricing_end_level(ctx, st);

    TEST_ASSERT_EQUAL_INT(0, ctx->var_q_total[1]);
    /* Flags cleared for this level */
    TEST_ASSERT_EQUAL_INT(0x00, ctx->var_flags[2] & 0x03);

    free_state(st);
    cxf_pricing_free(ctx);
}

/*--- Cache invalidation at L1 ---*/

void test_l1_invalidates_caches(void) {
    PricingState *ctx = cxf_pricing_create(4, 3);
    cxf_pricing_init(ctx, 4, 1);
    cxf_pricing_init_constrs(ctx, 3);

    SolverState *st = make_state(4, 3);

    ctx->current_level = 1;
    ctx->level_active[1] = 1;
    ctx->cached_var_count[1] = 10;
    ctx->cached_constr_count3[1] = 20;

    cxf_pricing_end_level(ctx, st);

    TEST_ASSERT_EQUAL_INT(-1, ctx->cached_var_count[1]);
    TEST_ASSERT_EQUAL_INT(-1, ctx->cached_constr_count3[1]);

    free_state(st);
    cxf_pricing_free(ctx);
}

/*--- Work counter ---*/

void test_work_counter_incremented(void) {
    PricingState *ctx = cxf_pricing_create(4, 3);
    cxf_pricing_init(ctx, 4, 1);
    cxf_pricing_init_constrs(ctx, 3);

    double wc = 0.0;
    SolverState *st = make_state(4, 3);
    st->work_counter = &wc;

    ctx->current_level = 0;
    ctx->level_active[0] = 1;
    ctx->var_q_total[0] = 5;
    ctx->constr_q_total[0] = 3;

    cxf_pricing_end_level(ctx, st);

    /* Work counter should be incremented by pre-compaction total (5+3=8) */
    TEST_ASSERT_EQUAL_DOUBLE(8.0, wc);

    free_state(st);
    cxf_pricing_free(ctx);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_l0_var_queue_filters_invalid);
    RUN_TEST(test_l0_constr_queue_filters_invalid);
    RUN_TEST(test_l1_promote_pending);
    RUN_TEST(test_l1_demote_stale);
    RUN_TEST(test_l1_discard_invalid_status);
    RUN_TEST(test_l1_invalidates_caches);
    RUN_TEST(test_work_counter_incremented);
    return UNITY_END();
}
