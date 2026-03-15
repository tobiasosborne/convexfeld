/**
 * @file test_work_counter.c
 * @brief Tests: work counter uses pre-compaction (scanned) queue sizes.
 *
 * Verifies that cxf_pricing_update_queues increments the work counter
 * by the number of entries SCANNED (pre-compaction), not entries
 * surviving compaction, per pricing_support.md Work Counter Integration.
 *
 * Beads: convexfeld-xlam
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

/* Create a test solver state with n vars and m constraints.
 * By default: vars 0..n-1 are nonbasic (AT_LOWER), slacks n..n+m-1 basic. */
static SolverState *create_test_state(int n, int m) {
    SolverState *s = calloc(1, sizeof(SolverState));
    s->num_vars = n;
    s->num_constrs = m;
    s->basis = cxf_basis_create(m, n + m);
    for (int j = 0; j < n; j++)
        s->basis->var_status[j] = CXF_VAR_AT_LOWER;
    for (int i = 0; i < m; i++) {
        s->basis->basic_vars[i] = n + i;
        s->basis->var_status[n + i] = i;  /* basic in row i */
    }
    return s;
}

static void free_test_state(SolverState *s) {
    if (!s) return;
    cxf_basis_free(s->basis);
    free(s);
}

void setUp(void) {}
void tearDown(void) {}

/*--- Level 0: work counter uses pre-compaction counts ---*/

void test_l0_work_counter_no_filtering(void) {
    /* When all entries survive, pre == post, counter should equal total */
    int n = 3, m = 4;
    PricingState *ctx = cxf_pricing_create(n, 3);
    cxf_pricing_init(ctx, n, 1);
    cxf_pricing_init_constrs(ctx, m);
    SolverState *s = create_test_state(n, m);

    double work = 0.0;
    s->work_counter = &work;

    /* Populate L0 constraint queue: all 4 constraints */
    for (int i = 0; i < m; i++)
        ctx->constr_queue[0][i] = i;
    ctx->constr_q_committed[0] = m;
    ctx->constr_q_total[0] = m;

    /* Populate L0 variable queue: all 3 variables (nonbasic) */
    for (int j = 0; j < n; j++)
        ctx->var_queue[0][j] = j;
    ctx->var_q_committed[0] = n;
    ctx->var_q_total[0] = n;

    ctx->current_level = 0;
    ctx->level_active[0] = 1;

    cxf_pricing_update_queues(ctx, s);

    /* All survive => work = m + n = 7 (pre == post) */
    TEST_ASSERT_EQUAL_DOUBLE(7.0, work);

    cxf_pricing_free(ctx);
    free_test_state(s);
}

void test_l0_work_counter_with_filtering(void) {
    /* Some entries filtered out: work counter must reflect pre-compaction
     * count (entries scanned), not post-compaction (survivors). */
    int n = 4, m = 4;
    PricingState *ctx = cxf_pricing_create(n, 3);
    cxf_pricing_init(ctx, n, 1);
    cxf_pricing_init_constrs(ctx, m);
    SolverState *s = create_test_state(n, m);

    double work = 0.0;
    s->work_counter = &work;

    /* Make constraint 1 and 3's slacks nonbasic => they get filtered */
    s->basis->var_status[n + 1] = CXF_VAR_AT_LOWER;
    s->basis->var_status[n + 3] = CXF_VAR_AT_LOWER;

    /* Make var 2 basic => it gets filtered from var queue */
    s->basis->var_status[2] = 0;

    /* Constraint queue: 4 entries, 2 will be filtered */
    for (int i = 0; i < m; i++)
        ctx->constr_queue[0][i] = i;
    ctx->constr_q_committed[0] = m;
    ctx->constr_q_total[0] = m;

    /* Variable queue: 4 entries, 1 will be filtered */
    for (int j = 0; j < n; j++)
        ctx->var_queue[0][j] = j;
    ctx->var_q_committed[0] = n;
    ctx->var_q_total[0] = n;

    ctx->current_level = 0;
    ctx->level_active[0] = 1;

    cxf_pricing_update_queues(ctx, s);

    /* Post-compaction: constr=2, var=3 (total 5)
     * Pre-compaction:  constr=4, var=4 (total 8)
     * Work counter must be 8 (pre), NOT 5 (post) */
    TEST_ASSERT_EQUAL_DOUBLE(8.0, work);

    /* Verify the actual queue sizes are correct (post-compaction) */
    TEST_ASSERT_EQUAL_INT(2, ctx->constr_q_total[0]);
    TEST_ASSERT_EQUAL_INT(3, ctx->var_q_total[0]);

    cxf_pricing_free(ctx);
    free_test_state(s);
}

void test_l0_work_counter_all_filtered(void) {
    /* All entries filtered: work counter should still reflect scanned */
    int n = 3, m = 3;
    PricingState *ctx = cxf_pricing_create(n, 3);
    cxf_pricing_init(ctx, n, 1);
    cxf_pricing_init_constrs(ctx, m);
    SolverState *s = create_test_state(n, m);

    double work = 0.0;
    s->work_counter = &work;

    /* Make all slacks nonbasic => all constraint entries filtered */
    for (int i = 0; i < m; i++)
        s->basis->var_status[n + i] = CXF_VAR_AT_LOWER;
    /* Make all vars basic => all var entries filtered */
    for (int j = 0; j < n; j++)
        s->basis->var_status[j] = j;

    for (int i = 0; i < m; i++)
        ctx->constr_queue[0][i] = i;
    ctx->constr_q_committed[0] = m;
    ctx->constr_q_total[0] = m;

    for (int j = 0; j < n; j++)
        ctx->var_queue[0][j] = j;
    ctx->var_q_committed[0] = n;
    ctx->var_q_total[0] = n;

    ctx->current_level = 0;
    ctx->level_active[0] = 1;

    cxf_pricing_update_queues(ctx, s);

    /* Post-compaction: 0. Pre-compaction: 6. Work = 6, NOT 0. */
    TEST_ASSERT_EQUAL_DOUBLE(6.0, work);
    TEST_ASSERT_EQUAL_INT(0, ctx->constr_q_total[0]);
    TEST_ASSERT_EQUAL_INT(0, ctx->var_q_total[0]);

    cxf_pricing_free(ctx);
    free_test_state(s);
}

/*--- Level 1: work counter uses pre-compaction counts ---*/

void test_l1_work_counter_with_filtering(void) {
    /* At level 1, entries get promoted/demoted. Work counter must
     * reflect pre-compaction (scanned) count. */
    int n = 3, m = 3;
    PricingState *ctx = cxf_pricing_create(n, 3);
    cxf_pricing_init(ctx, n, 1);
    cxf_pricing_init_constrs(ctx, m);
    SolverState *s = create_test_state(n, m);

    double work = 0.0;
    s->work_counter = &work;

    /* Make constraint 0's slack nonbasic => discarded */
    s->basis->var_status[n + 0] = CXF_VAR_AT_LOWER;

    /* Set up L1 constraint queue with pending flags */
    ctx->constr_queue[1][0] = 0;
    ctx->constr_queue[1][1] = 1;
    ctx->constr_queue[1][2] = 2;
    ctx->constr_q_total[1] = 3;
    ctx->constr_flags[0] = 0x02;  /* L1_PENDING */
    ctx->constr_flags[1] = 0x02;
    ctx->constr_flags[2] = 0x02;

    /* Set up L1 variable queue: 3 vars, var 1 basic => filtered */
    s->basis->var_status[1] = 0;  /* basic */
    ctx->var_queue[1][0] = 0;
    ctx->var_queue[1][1] = 1;
    ctx->var_queue[1][2] = 2;
    ctx->var_q_total[1] = 3;
    ctx->var_flags[0] = 0x02;
    ctx->var_flags[1] = 0x02;
    ctx->var_flags[2] = 0x02;

    ctx->current_level = 1;
    ctx->level_active[1] = 1;

    cxf_pricing_update_queues(ctx, s);

    /* Post-compaction: constr=2, var=2 (total=4)
     * Pre-compaction:  constr=3, var=3 (total=6)
     * Work counter must be 6 (pre), NOT 4 (post) */
    TEST_ASSERT_EQUAL_DOUBLE(6.0, work);
    TEST_ASSERT_EQUAL_INT(2, ctx->constr_q_total[1]);
    TEST_ASSERT_EQUAL_INT(2, ctx->var_q_total[1]);

    cxf_pricing_free(ctx);
    free_test_state(s);
}

void test_work_counter_null_noop(void) {
    /* Null work counter pointer should not crash */
    int n = 2, m = 2;
    PricingState *ctx = cxf_pricing_create(n, 3);
    cxf_pricing_init(ctx, n, 1);
    cxf_pricing_init_constrs(ctx, m);
    SolverState *s = create_test_state(n, m);

    s->work_counter = NULL;  /* disabled */

    ctx->constr_queue[0][0] = 0;
    ctx->constr_q_total[0] = 1;
    ctx->var_queue[0][0] = 0;
    ctx->var_q_total[0] = 1;
    ctx->current_level = 0;
    ctx->level_active[0] = 1;

    /* Should not crash */
    cxf_pricing_update_queues(ctx, s);

    cxf_pricing_free(ctx);
    free_test_state(s);
}

void test_work_counter_accumulates(void) {
    /* Work counter should accumulate across multiple calls */
    int n = 3, m = 2;
    PricingState *ctx = cxf_pricing_create(n, 3);
    cxf_pricing_init(ctx, n, 1);
    cxf_pricing_init_constrs(ctx, m);
    SolverState *s = create_test_state(n, m);

    double work = 5.0;  /* start nonzero */
    s->work_counter = &work;

    ctx->constr_queue[0][0] = 0;
    ctx->constr_queue[0][1] = 1;
    ctx->constr_q_committed[0] = 2;
    ctx->constr_q_total[0] = 2;
    ctx->var_queue[0][0] = 0;
    ctx->var_q_committed[0] = 1;
    ctx->var_q_total[0] = 1;

    ctx->current_level = 0;
    ctx->level_active[0] = 1;

    cxf_pricing_update_queues(ctx, s);

    /* 5.0 + (2 + 1) = 8.0 */
    TEST_ASSERT_EQUAL_DOUBLE(8.0, work);

    cxf_pricing_free(ctx);
    free_test_state(s);
}

int main(void) {
    UNITY_BEGIN();

    /* L0 work counter */
    RUN_TEST(test_l0_work_counter_no_filtering);
    RUN_TEST(test_l0_work_counter_with_filtering);
    RUN_TEST(test_l0_work_counter_all_filtered);

    /* L1 work counter */
    RUN_TEST(test_l1_work_counter_with_filtering);

    /* Edge cases */
    RUN_TEST(test_work_counter_null_noop);
    RUN_TEST(test_work_counter_accumulates);

    return UNITY_END();
}
