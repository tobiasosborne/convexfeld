/**
 * @file test_cache_level0.c
 * @brief Tests that V2 cache invalidation does NOT occur at level 0.
 *
 * Spec: pricing_core.md -- "No cache invalidation occurs at level 0,
 * because level 0 returns the base dirty list directly without caching."
 *
 * Bug: convexfeld-l0w2 -- cxf_pricing_end_level unconditionally
 * invalidated caches at the current level, including level 0.
 * Fix: guard cache invalidation with `if (level > 0)`.
 */

#include "unity.h"
#include "convexfeld/cxf_pricing.h"
#include <stdlib.h>

/* Forward declarations */
PricingState *cxf_pricing_create(int num_vars, int max_levels);
void cxf_pricing_free(PricingState *ctx);
int cxf_pricing_init(PricingState *ctx, int num_vars, int strategy);
int cxf_pricing_init_constrs(PricingState *ctx, int num_constrs);

void setUp(void) {}
void tearDown(void) {}

/**
 * Core test: end_level at level 0 must NOT invalidate V2 caches.
 * Pre-seed all 6 cache slots with sentinel values, call end_level
 * at level 0, verify all slots are preserved.
 */
void test_end_level_level0_preserves_caches(void) {
    PricingState *ctx = cxf_pricing_create(5, 3);
    cxf_pricing_init(ctx, 5, 1);
    cxf_pricing_init_constrs(ctx, 3);

    ctx->current_level = 0;
    ctx->level_active[0] = 1;  /* Already active (skip activation guard) */

    /* Seed all 6 V2 cache slots at level 0 with sentinel values */
    ctx->cached_var_count[0] = 10;
    ctx->cached_var_count2[0] = 20;
    ctx->cached_var_count3[0] = 30;
    ctx->cached_constr_count[0] = 40;
    ctx->cached_constr_count2[0] = 50;
    ctx->cached_constr_count3[0] = 60;

    cxf_pricing_end_level(ctx);

    /* All 6 slots at level 0 must be preserved (NOT set to -1) */
    TEST_ASSERT_EQUAL_INT(10, ctx->cached_var_count[0]);
    TEST_ASSERT_EQUAL_INT(20, ctx->cached_var_count2[0]);
    TEST_ASSERT_EQUAL_INT(30, ctx->cached_var_count3[0]);
    TEST_ASSERT_EQUAL_INT(40, ctx->cached_constr_count[0]);
    TEST_ASSERT_EQUAL_INT(50, ctx->cached_constr_count2[0]);
    TEST_ASSERT_EQUAL_INT(60, ctx->cached_constr_count3[0]);

    cxf_pricing_free(ctx);
}

/**
 * Contrast test: end_level at level 1 MUST invalidate V2 caches.
 * Ensures the guard does not accidentally suppress all invalidation.
 */
void test_end_level_level1_invalidates_caches(void) {
    PricingState *ctx = cxf_pricing_create(5, 3);
    cxf_pricing_init(ctx, 5, 1);
    cxf_pricing_init_constrs(ctx, 3);

    ctx->current_level = 1;
    ctx->level_active[1] = 1;  /* Already active */

    /* Seed all 6 V2 cache slots at level 1 */
    ctx->cached_var_count[1] = 10;
    ctx->cached_var_count2[1] = 20;
    ctx->cached_var_count3[1] = 30;
    ctx->cached_constr_count[1] = 40;
    ctx->cached_constr_count2[1] = 50;
    ctx->cached_constr_count3[1] = 60;

    cxf_pricing_end_level(ctx);

    /* All 6 slots at level 1 must be invalidated (-1) */
    TEST_ASSERT_EQUAL_INT(-1, ctx->cached_var_count[1]);
    TEST_ASSERT_EQUAL_INT(-1, ctx->cached_var_count2[1]);
    TEST_ASSERT_EQUAL_INT(-1, ctx->cached_var_count3[1]);
    TEST_ASSERT_EQUAL_INT(-1, ctx->cached_constr_count[1]);
    TEST_ASSERT_EQUAL_INT(-1, ctx->cached_constr_count2[1]);
    TEST_ASSERT_EQUAL_INT(-1, ctx->cached_constr_count3[1]);

    cxf_pricing_free(ctx);
}

/**
 * Cross-level isolation: end_level at level 0 must not touch level 1/2
 * caches, and vice versa.
 */
void test_end_level_level0_does_not_touch_other_levels(void) {
    PricingState *ctx = cxf_pricing_create(5, 3);
    cxf_pricing_init(ctx, 5, 1);
    cxf_pricing_init_constrs(ctx, 3);

    ctx->current_level = 0;
    ctx->level_active[0] = 1;

    /* Seed level 1 and level 2 caches */
    ctx->cached_var_count[1] = 77;
    ctx->cached_constr_count[2] = 88;

    cxf_pricing_end_level(ctx);

    /* Other levels' caches must be untouched */
    TEST_ASSERT_EQUAL_INT(77, ctx->cached_var_count[1]);
    TEST_ASSERT_EQUAL_INT(88, ctx->cached_constr_count[2]);

    cxf_pricing_free(ctx);
}

/**
 * Level 0 + activation guard: first call at inactive level 0 should
 * just activate, not touch caches at all (regression guard).
 */
void test_end_level_level0_inactive_activates_only(void) {
    PricingState *ctx = cxf_pricing_create(5, 3);
    cxf_pricing_init(ctx, 5, 1);
    cxf_pricing_init_constrs(ctx, 3);

    ctx->current_level = 0;
    ctx->level_active[0] = 0;  /* Not yet active */

    ctx->cached_var_count[0] = 99;
    ctx->cached_constr_count[0] = 42;

    cxf_pricing_end_level(ctx);

    /* Should just activate, not invalidate */
    TEST_ASSERT_EQUAL_INT(1, ctx->level_active[0]);
    TEST_ASSERT_EQUAL_INT(99, ctx->cached_var_count[0]);
    TEST_ASSERT_EQUAL_INT(42, ctx->cached_constr_count[0]);

    cxf_pricing_free(ctx);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_end_level_level0_preserves_caches);
    RUN_TEST(test_end_level_level1_invalidates_caches);
    RUN_TEST(test_end_level_level0_does_not_touch_other_levels);
    RUN_TEST(test_end_level_level0_inactive_activates_only);
    return UNITY_END();
}
