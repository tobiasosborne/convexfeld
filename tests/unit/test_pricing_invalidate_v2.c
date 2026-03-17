/**
 * @file test_pricing_invalidate_v2.c
 * @brief Tests for V2 cxf_pricing_invalidate (single-var dirty marker)
 *
 * Spec: pricing_core.md cxf_pricing_invalidate
 * Beads: convexfeld-u82g
 */

#include "unity.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_pricing.h"
#include <stdlib.h>
#include <string.h>

/* Forward declarations */
PricingState *cxf_pricing_create(int num_vars, int max_levels);
void cxf_pricing_free(PricingState *ctx);
int cxf_pricing_init(PricingState *ctx, int num_vars, int strategy);

/* Queue flag bits (from pricing_internal.h) */
#define L1_COMMITTED 0x01
#define L1_PENDING   0x02
#define L1_MASK      0x03
#define L2_COMMITTED 0x04
#define L2_PENDING   0x08
#define L2_MASK      0x0C

void setUp(void) {}
void tearDown(void) {}

/*============================================================================
 * Helper: create initialized pricing state with V2 queues
 *===========================================================================*/
static PricingState *make_ctx(int n) {
    PricingState *ctx = cxf_pricing_create(n, CXF_MAX_PRICING_LEVELS);
    TEST_ASSERT_NOT_NULL(ctx);
    int rc = cxf_pricing_init(ctx, n, 1);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_NOT_NULL(ctx->var_flags);
    TEST_ASSERT_NOT_NULL(ctx->var_dirty);
    return ctx;
}

/*============================================================================
 * Tests
 *===========================================================================*/

/** Variable is inserted into both L1 and L2 queues. */
void test_invalidate_inserts_both_levels(void) {
    PricingState *ctx = make_ctx(10);

    cxf_pricing_invalidate(ctx, 3);

    /* L1 queue should have var 3 */
    TEST_ASSERT_EQUAL_INT(1, ctx->var_q_committed[1]);
    TEST_ASSERT_EQUAL_INT(1, ctx->var_q_total[1]);
    TEST_ASSERT_EQUAL_INT(3, ctx->var_queue[1][0]);

    /* L2 queue should have var 3 */
    TEST_ASSERT_EQUAL_INT(1, ctx->var_q_committed[2]);
    TEST_ASSERT_EQUAL_INT(1, ctx->var_q_total[2]);
    TEST_ASSERT_EQUAL_INT(3, ctx->var_queue[2][0]);

    /* Flag bits: committed in both levels */
    TEST_ASSERT_BITS(L1_COMMITTED, L1_COMMITTED, ctx->var_flags[3]);
    TEST_ASSERT_BITS(L2_COMMITTED, L2_COMMITTED, ctx->var_flags[3]);

    cxf_pricing_free(ctx);
}

/** V1 dirty flag is also set. */
void test_invalidate_sets_v1_dirty(void) {
    PricingState *ctx = make_ctx(10);

    TEST_ASSERT_EQUAL_INT(0, ctx->var_dirty[5]);
    TEST_ASSERT_EQUAL_INT(0, ctx->num_dirty);

    cxf_pricing_invalidate(ctx, 5);

    TEST_ASSERT_EQUAL_INT(1, ctx->var_dirty[5]);
    TEST_ASSERT_EQUAL_INT(1, ctx->num_dirty);

    cxf_pricing_free(ctx);
}

/** Duplicate calls do not produce duplicate queue entries. */
void test_invalidate_duplicate_prevention(void) {
    PricingState *ctx = make_ctx(10);

    cxf_pricing_invalidate(ctx, 7);
    cxf_pricing_invalidate(ctx, 7);
    cxf_pricing_invalidate(ctx, 7);

    /* Only one entry in each queue */
    TEST_ASSERT_EQUAL_INT(1, ctx->var_q_committed[1]);
    TEST_ASSERT_EQUAL_INT(1, ctx->var_q_total[1]);
    TEST_ASSERT_EQUAL_INT(1, ctx->var_q_committed[2]);
    TEST_ASSERT_EQUAL_INT(1, ctx->var_q_total[2]);

    /* V1 dirty count should also be 1 */
    TEST_ASSERT_EQUAL_INT(1, ctx->num_dirty);

    cxf_pricing_free(ctx);
}

/** When level phase is active, insertion goes to pending section. */
void test_invalidate_pending_when_active(void) {
    PricingState *ctx = make_ctx(10);

    /* Activate level 1 and 2 phases */
    ctx->level_active[1] = 1;
    ctx->level_active[2] = 1;

    cxf_pricing_invalidate(ctx, 4);

    /* Committed counts unchanged (still 0) */
    TEST_ASSERT_EQUAL_INT(0, ctx->var_q_committed[1]);
    TEST_ASSERT_EQUAL_INT(0, ctx->var_q_committed[2]);

    /* Total counts incremented (pending section) */
    TEST_ASSERT_EQUAL_INT(1, ctx->var_q_total[1]);
    TEST_ASSERT_EQUAL_INT(1, ctx->var_q_total[2]);

    /* Pending bits set */
    TEST_ASSERT_BITS(L1_PENDING, L1_PENDING, ctx->var_flags[4]);
    TEST_ASSERT_BITS(L2_PENDING, L2_PENDING, ctx->var_flags[4]);

    cxf_pricing_free(ctx);
}

/** Committed insertion when phase is inactive. */
void test_invalidate_committed_when_inactive(void) {
    PricingState *ctx = make_ctx(10);

    /* Both levels inactive (default after init) */
    TEST_ASSERT_EQUAL_INT(0, ctx->level_active[1]);
    TEST_ASSERT_EQUAL_INT(0, ctx->level_active[2]);

    cxf_pricing_invalidate(ctx, 2);

    /* Committed and total should both be 1 */
    TEST_ASSERT_EQUAL_INT(1, ctx->var_q_committed[1]);
    TEST_ASSERT_EQUAL_INT(1, ctx->var_q_total[1]);
    TEST_ASSERT_EQUAL_INT(1, ctx->var_q_committed[2]);
    TEST_ASSERT_EQUAL_INT(1, ctx->var_q_total[2]);

    /* Committed bit set, pending bit clear */
    TEST_ASSERT_BITS(L1_COMMITTED, L1_COMMITTED, ctx->var_flags[2]);
    TEST_ASSERT_BITS(L1_PENDING, 0, ctx->var_flags[2]);
    TEST_ASSERT_BITS(L2_COMMITTED, L2_COMMITTED, ctx->var_flags[2]);
    TEST_ASSERT_BITS(L2_PENDING, 0, ctx->var_flags[2]);

    cxf_pricing_free(ctx);
}

/** Multiple distinct variables each get their own entry. */
void test_invalidate_multiple_vars(void) {
    PricingState *ctx = make_ctx(10);

    cxf_pricing_invalidate(ctx, 0);
    cxf_pricing_invalidate(ctx, 5);
    cxf_pricing_invalidate(ctx, 9);

    TEST_ASSERT_EQUAL_INT(3, ctx->var_q_committed[1]);
    TEST_ASSERT_EQUAL_INT(3, ctx->var_q_total[1]);
    TEST_ASSERT_EQUAL_INT(3, ctx->var_q_committed[2]);
    TEST_ASSERT_EQUAL_INT(3, ctx->var_q_total[2]);

    /* V1 dirty count matches */
    TEST_ASSERT_EQUAL_INT(3, ctx->num_dirty);

    cxf_pricing_free(ctx);
}

/** NULL context does not crash. */
void test_invalidate_null_safe(void) {
    cxf_pricing_invalidate(NULL, 0);
    TEST_PASS();
}

/** Out-of-range varIndex does not corrupt state. */
void test_invalidate_out_of_range(void) {
    PricingState *ctx = make_ctx(5);

    cxf_pricing_invalidate(ctx, -1);
    cxf_pricing_invalidate(ctx, 5);
    cxf_pricing_invalidate(ctx, 999);

    /* No entries added */
    TEST_ASSERT_EQUAL_INT(0, ctx->var_q_total[1]);
    TEST_ASSERT_EQUAL_INT(0, ctx->var_q_total[2]);
    TEST_ASSERT_EQUAL_INT(0, ctx->num_dirty);

    cxf_pricing_free(ctx);
}

/*============================================================================
 * Test runner
 *===========================================================================*/
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_invalidate_inserts_both_levels);
    RUN_TEST(test_invalidate_sets_v1_dirty);
    RUN_TEST(test_invalidate_duplicate_prevention);
    RUN_TEST(test_invalidate_pending_when_active);
    RUN_TEST(test_invalidate_committed_when_inactive);
    RUN_TEST(test_invalidate_multiple_vars);
    RUN_TEST(test_invalidate_null_safe);
    RUN_TEST(test_invalidate_out_of_range);
    return UNITY_END();
}
