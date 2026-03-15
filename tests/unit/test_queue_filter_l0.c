/**
 * @file test_queue_filter_l0.c
 * @brief Tests for constraint queue status filtering in update_queues.
 *
 * Verifies that cxf_pricing_update_queues filters constraint queue entries
 * by constraint status (var_status[num_vars + constr_idx] >= 0) at all
 * levels, per pricing_support.md Level 0 and pricing_core.md Phase 3.
 *
 * Beads: convexfeld-yvbq
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

/*--- Level 0: constraint queue status filtering ---*/

void test_l0_constr_queue_keeps_basic_slacks(void) {
    /* All slacks basic (default) => all constraint entries should survive */
    int n = 3, m = 4;
    PricingState *ctx = cxf_pricing_create(n, 3);
    cxf_pricing_init(ctx, n, 1);
    cxf_pricing_init_constrs(ctx, m);
    SolverState *s = create_test_state(n, m);

    /* Populate L0 constraint queue with all constraint indices */
    for (int i = 0; i < m; i++)
        ctx->constr_queue[0][i] = i;
    ctx->constr_q_committed[0] = m;
    ctx->constr_q_total[0] = m;
    ctx->current_level = 0;

    /* Activate level first (phase guard) */
    ctx->level_active[0] = 1;

    cxf_pricing_update_queues(ctx, s);

    /* All slacks are basic (status >= 0) => all kept */
    TEST_ASSERT_EQUAL_INT(m, ctx->constr_q_committed[0]);
    TEST_ASSERT_EQUAL_INT(m, ctx->constr_q_total[0]);

    cxf_pricing_free(ctx);
    free_test_state(s);
}

void test_l0_constr_queue_discards_nonbasic_slacks(void) {
    /* Make constraint 1's slack nonbasic => entry 1 should be filtered out */
    int n = 3, m = 4;
    PricingState *ctx = cxf_pricing_create(n, 3);
    cxf_pricing_init(ctx, n, 1);
    cxf_pricing_init_constrs(ctx, m);
    SolverState *s = create_test_state(n, m);

    /* Make slack for constraint 1 nonbasic (AT_LOWER) */
    s->basis->var_status[n + 1] = CXF_VAR_AT_LOWER;  /* -1 */

    /* Queue: [0, 1, 2, 3] */
    for (int i = 0; i < m; i++)
        ctx->constr_queue[0][i] = i;
    ctx->constr_q_committed[0] = m;
    ctx->constr_q_total[0] = m;
    ctx->current_level = 0;
    ctx->level_active[0] = 1;

    cxf_pricing_update_queues(ctx, s);

    /* Constraint 1 should be gone (slack nonbasic), 3 remain */
    TEST_ASSERT_EQUAL_INT(3, ctx->constr_q_committed[0]);
    TEST_ASSERT_EQUAL_INT(3, ctx->constr_q_total[0]);

    /* Verify surviving entries are 0, 2, 3 */
    int found[4] = {0};
    for (int i = 0; i < 3; i++) {
        int ci = ctx->constr_queue[0][i];
        TEST_ASSERT_TRUE(ci >= 0 && ci < m);
        found[ci] = 1;
    }
    TEST_ASSERT_EQUAL_INT(1, found[0]);
    TEST_ASSERT_EQUAL_INT(0, found[1]);  /* filtered */
    TEST_ASSERT_EQUAL_INT(1, found[2]);
    TEST_ASSERT_EQUAL_INT(1, found[3]);

    cxf_pricing_free(ctx);
    free_test_state(s);
}

void test_l0_constr_queue_all_nonbasic_empties(void) {
    /* All slacks nonbasic => queue should become empty */
    int n = 3, m = 3;
    PricingState *ctx = cxf_pricing_create(n, 3);
    cxf_pricing_init(ctx, n, 1);
    cxf_pricing_init_constrs(ctx, m);
    SolverState *s = create_test_state(n, m);

    /* Make all slacks nonbasic */
    for (int i = 0; i < m; i++)
        s->basis->var_status[n + i] = CXF_VAR_AT_LOWER;

    for (int i = 0; i < m; i++)
        ctx->constr_queue[0][i] = i;
    ctx->constr_q_committed[0] = m;
    ctx->constr_q_total[0] = m;
    ctx->current_level = 0;
    ctx->level_active[0] = 1;

    cxf_pricing_update_queues(ctx, s);

    TEST_ASSERT_EQUAL_INT(0, ctx->constr_q_committed[0]);
    TEST_ASSERT_EQUAL_INT(0, ctx->constr_q_total[0]);

    cxf_pricing_free(ctx);
    free_test_state(s);
}

void test_l0_var_queue_still_works(void) {
    /* Verify variable queue filtering still works correctly after changes */
    int n = 4, m = 2;
    PricingState *ctx = cxf_pricing_create(n, 3);
    cxf_pricing_init(ctx, n, 1);
    cxf_pricing_init_constrs(ctx, m);
    SolverState *s = create_test_state(n, m);

    /* Vars 0,1 nonbasic, var 2 basic => var 2 should be filtered */
    s->basis->var_status[2] = 0;  /* basic in row 0 */

    ctx->var_queue[0][0] = 0;
    ctx->var_queue[0][1] = 1;
    ctx->var_queue[0][2] = 2;
    ctx->var_q_committed[0] = 3;
    ctx->var_q_total[0] = 3;
    ctx->current_level = 0;
    ctx->level_active[0] = 1;

    cxf_pricing_update_queues(ctx, s);

    TEST_ASSERT_EQUAL_INT(2, ctx->var_q_committed[0]);

    cxf_pricing_free(ctx);
    free_test_state(s);
}

/*--- Levels 1-2: constraint queue status filtering ---*/

void test_lx_constr_queue_discards_nonbasic_slack(void) {
    /* At level 1, constraint entries with nonbasic slack get discarded */
    int n = 3, m = 3;
    PricingState *ctx = cxf_pricing_create(n, 3);
    cxf_pricing_init(ctx, n, 1);
    cxf_pricing_init_constrs(ctx, m);
    SolverState *s = create_test_state(n, m);

    /* Make slack for constraint 2 nonbasic */
    s->basis->var_status[n + 2] = CXF_VAR_AT_UPPER;  /* -2 */

    /* Set up L1 constraint queue with pending flags */
    ctx->constr_queue[1][0] = 0;
    ctx->constr_queue[1][1] = 1;
    ctx->constr_queue[1][2] = 2;
    ctx->constr_q_total[1] = 3;
    /* Set pending flag bits for L1 */
    ctx->constr_flags[0] = 0x02;  /* L1_PENDING */
    ctx->constr_flags[1] = 0x02;
    ctx->constr_flags[2] = 0x02;

    ctx->current_level = 1;
    ctx->level_active[1] = 1;

    cxf_pricing_update_queues(ctx, s);

    /* Constraint 2 should be discarded (nonbasic slack) */
    TEST_ASSERT_EQUAL_INT(2, ctx->constr_q_committed[1]);

    /* Constraint 2's flags should be cleared */
    TEST_ASSERT_EQUAL_UINT8(0x00, ctx->constr_flags[2] & 0x03);

    cxf_pricing_free(ctx);
    free_test_state(s);
}

void test_lx_constr_queue_keeps_basic_slack_with_pending(void) {
    /* At level 1, constraint entries with basic slack + pending flag
     * should be promoted (committed bit set, pending cleared) */
    int n = 2, m = 2;
    PricingState *ctx = cxf_pricing_create(n, 3);
    cxf_pricing_init(ctx, n, 1);
    cxf_pricing_init_constrs(ctx, m);
    SolverState *s = create_test_state(n, m);

    /* Both slacks basic (default) */
    ctx->constr_queue[1][0] = 0;
    ctx->constr_queue[1][1] = 1;
    ctx->constr_q_total[1] = 2;
    ctx->constr_flags[0] = 0x02;  /* L1_PENDING */
    ctx->constr_flags[1] = 0x02;  /* L1_PENDING */

    ctx->current_level = 1;
    ctx->level_active[1] = 1;

    cxf_pricing_update_queues(ctx, s);

    /* Both kept and promoted */
    TEST_ASSERT_EQUAL_INT(2, ctx->constr_q_committed[1]);

    /* Pending cleared, committed set */
    TEST_ASSERT_EQUAL_UINT8(0x01, ctx->constr_flags[0] & 0x03);
    TEST_ASSERT_EQUAL_UINT8(0x01, ctx->constr_flags[1] & 0x03);

    cxf_pricing_free(ctx);
    free_test_state(s);
}

int main(void) {
    UNITY_BEGIN();

    /* Level 0 constraint queue status check */
    RUN_TEST(test_l0_constr_queue_keeps_basic_slacks);
    RUN_TEST(test_l0_constr_queue_discards_nonbasic_slacks);
    RUN_TEST(test_l0_constr_queue_all_nonbasic_empties);
    RUN_TEST(test_l0_var_queue_still_works);

    /* Level 1-2 constraint queue status check */
    RUN_TEST(test_lx_constr_queue_discards_nonbasic_slack);
    RUN_TEST(test_lx_constr_queue_keeps_basic_slack_with_pending);

    return UNITY_END();
}
