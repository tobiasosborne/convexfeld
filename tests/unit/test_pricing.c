/**
 * @file test_pricing.c
 * @brief TDD tests for pricing operations (M6.1.1)
 *
 * Tests for all pricing strategies including partial pricing,
 * steepest edge, Dantzig, and Devex.
 *
 * Tests MUST be written BEFORE implementation (TDD).
 */

#include "unity.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_pricing.h"

/* Forward declarations for functions under test */
/* These will be implemented in M6.1.2-M6.1.7 */

/* PricingContext lifecycle */
PricingContext *cxf_pricing_create(int num_vars, int max_levels);
void cxf_pricing_free(PricingContext *ctx);
int cxf_pricing_init(PricingContext *ctx, int num_vars, int strategy);

/* Candidate selection */
int cxf_pricing_candidates(PricingContext *ctx, const double *reduced_costs,
                           const int *var_status, int num_vars, double tolerance,
                           int *candidates, int max_candidates);

/* Steepest edge */
int cxf_pricing_steepest(PricingContext *ctx, const double *reduced_costs,
                         const double *weights, const int *var_status,
                         int num_vars, double tolerance);

/* Post-pivot update */
int cxf_pricing_update(PricingContext *ctx, int entering_var, int leaving_row,
                       const double *pivot_column, const double *pivot_row,
                       int num_rows);

/* Cache management */
void cxf_pricing_invalidate(PricingContext *ctx, int flags);

/* Two-phase pricing */
int cxf_pricing_step2(PricingContext *ctx, const double *reduced_costs,
                      const int *var_status, int num_vars, double tolerance);

/* Invalidation flags */
#define CXF_INVALID_CANDIDATES     0x01
#define CXF_INVALID_REDUCED_COSTS  0x02
#define CXF_INVALID_WEIGHTS        0x04
#define CXF_INVALID_ALL            0xFF

/* Variable status codes for testing */
#define VAR_BASIC        0  /* Basic variable (row index) */
#define VAR_AT_LOWER    -1  /* Nonbasic at lower bound */
#define VAR_AT_UPPER    -2  /* Nonbasic at upper bound */
#define VAR_FREE        -3  /* Free (superbasic) */

void setUp(void) {}
void tearDown(void) {}

/*============================================================================
 * PricingContext Creation/Free Tests
 *===========================================================================*/

void test_pricing_create_basic(void) {
    PricingContext *ctx = cxf_pricing_create(100, 3);
    TEST_ASSERT_NOT_NULL(ctx);
    TEST_ASSERT_EQUAL_INT(3, ctx->max_levels);
    cxf_pricing_free(ctx);
}

void test_pricing_create_null_on_zero_vars(void) {
    PricingContext *ctx = cxf_pricing_create(0, 3);
    TEST_ASSERT_NULL(ctx);
}

void test_pricing_create_single_level(void) {
    PricingContext *ctx = cxf_pricing_create(50, 1);
    TEST_ASSERT_NOT_NULL(ctx);
    TEST_ASSERT_EQUAL_INT(1, ctx->max_levels);
    cxf_pricing_free(ctx);
}

void test_pricing_free_null_safe(void) {
    cxf_pricing_free(NULL);  /* Should not crash */
    TEST_PASS();
}

/*============================================================================
 * cxf_pricing_init Tests
 *===========================================================================*/

void test_pricing_init_basic(void) {
    PricingContext *ctx = cxf_pricing_create(100, 3);
    TEST_ASSERT_NOT_NULL(ctx);

    int result = cxf_pricing_init(ctx, 100, 1);  /* Strategy 1 = partial */
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);
    TEST_ASSERT_EQUAL_INT(1, ctx->current_level);
    TEST_ASSERT_NOT_NULL(ctx->candidate_counts);
    TEST_ASSERT_NOT_NULL(ctx->candidate_arrays);

    cxf_pricing_free(ctx);
}

void test_pricing_init_null_context(void) {
    int result = cxf_pricing_init(NULL, 100, 1);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, result);
}

void test_pricing_init_steepest_edge(void) {
    PricingContext *ctx = cxf_pricing_create(100, 3);
    TEST_ASSERT_NOT_NULL(ctx);

    int result = cxf_pricing_init(ctx, 100, 2);  /* Strategy 2 = steepest edge */
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);

    cxf_pricing_free(ctx);
}

void test_pricing_init_small_problem(void) {
    /* Small problems (n < 1000) should use full pricing */
    PricingContext *ctx = cxf_pricing_create(50, 3);
    TEST_ASSERT_NOT_NULL(ctx);

    int result = cxf_pricing_init(ctx, 50, 0);  /* Strategy 0 = auto */
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);

    cxf_pricing_free(ctx);
}

/*============================================================================
 * cxf_pricing_candidates Tests
 *===========================================================================*/

void test_pricing_candidates_finds_negative_rc(void) {
    PricingContext *ctx = cxf_pricing_create(5, 1);
    TEST_ASSERT_NOT_NULL(ctx);
    cxf_pricing_init(ctx, 5, 1);

    /* Setup: variable 2 at lower bound with negative RC (attractive) */
    double reduced_costs[] = {0.0, 0.0, -1.5, 0.0, 0.0};
    int var_status[] = {0, 1, VAR_AT_LOWER, 2, VAR_AT_LOWER};
    int candidates[5] = {0};

    int count = cxf_pricing_candidates(ctx, reduced_costs, var_status, 5,
                                        1e-6, candidates, 5);
    TEST_ASSERT_EQUAL_INT(1, count);
    TEST_ASSERT_EQUAL_INT(2, candidates[0]);

    cxf_pricing_free(ctx);
}

void test_pricing_candidates_finds_positive_rc_at_upper(void) {
    PricingContext *ctx = cxf_pricing_create(5, 1);
    TEST_ASSERT_NOT_NULL(ctx);
    cxf_pricing_init(ctx, 5, 1);

    /* Variable at upper bound with positive RC is attractive */
    double reduced_costs[] = {0.0, 0.0, 0.0, 2.0, 0.0};
    int var_status[] = {0, 1, 2, VAR_AT_UPPER, VAR_AT_LOWER};
    int candidates[5] = {0};

    int count = cxf_pricing_candidates(ctx, reduced_costs, var_status, 5,
                                        1e-6, candidates, 5);
    TEST_ASSERT_EQUAL_INT(1, count);
    TEST_ASSERT_EQUAL_INT(3, candidates[0]);

    cxf_pricing_free(ctx);
}

void test_pricing_candidates_skips_basic_vars(void) {
    PricingContext *ctx = cxf_pricing_create(5, 1);
    TEST_ASSERT_NOT_NULL(ctx);
    cxf_pricing_init(ctx, 5, 1);

    /* Basic variable (status >= 0) should be skipped even with bad RC */
    double reduced_costs[] = {-5.0, -5.0, -5.0, 0.0, 0.0};
    int var_status[] = {0, 1, 2, VAR_AT_LOWER, VAR_AT_LOWER};  /* 0,1,2 basic */
    int candidates[5] = {0};

    int count = cxf_pricing_candidates(ctx, reduced_costs, var_status, 5,
                                        1e-6, candidates, 5);
    TEST_ASSERT_EQUAL_INT(0, count);  /* No nonbasic attractive vars */

    cxf_pricing_free(ctx);
}

void test_pricing_candidates_optimal(void) {
    PricingContext *ctx = cxf_pricing_create(5, 1);
    TEST_ASSERT_NOT_NULL(ctx);
    cxf_pricing_init(ctx, 5, 1);

    /* All nonbasic at lower with non-negative RC = optimal */
    double reduced_costs[] = {0.0, 0.0, 0.5, 0.3, 0.1};
    int var_status[] = {0, 1, VAR_AT_LOWER, VAR_AT_LOWER, VAR_AT_LOWER};
    int candidates[5] = {0};

    int count = cxf_pricing_candidates(ctx, reduced_costs, var_status, 5,
                                        1e-6, candidates, 5);
    TEST_ASSERT_EQUAL_INT(0, count);  /* Optimal - no attractive vars */

    cxf_pricing_free(ctx);
}

/*============================================================================
 * cxf_pricing_steepest Tests
 *===========================================================================*/

void test_pricing_steepest_basic(void) {
    PricingContext *ctx = cxf_pricing_create(5, 1);
    TEST_ASSERT_NOT_NULL(ctx);
    cxf_pricing_init(ctx, 5, 2);  /* Steepest edge strategy */

    /* Variable 2 has best SE ratio: |-2.0| / sqrt(1.0) = 2.0 */
    double reduced_costs[] = {0.0, 0.0, -2.0, -1.0, 0.0};
    double weights[] = {1.0, 1.0, 1.0, 1.0, 1.0};
    int var_status[] = {0, 1, VAR_AT_LOWER, VAR_AT_LOWER, 2};

    int entering = cxf_pricing_steepest(ctx, reduced_costs, weights, var_status, 5, 1e-6);
    TEST_ASSERT_EQUAL_INT(2, entering);

    cxf_pricing_free(ctx);
}

void test_pricing_steepest_considers_weight(void) {
    PricingContext *ctx = cxf_pricing_create(5, 1);
    TEST_ASSERT_NOT_NULL(ctx);
    cxf_pricing_init(ctx, 5, 2);

    /* Variable 3 has better SE ratio despite smaller |RC|:
     * var 2: |-2.0| / sqrt(16.0) = 0.5
     * var 3: |-1.0| / sqrt(0.25) = 2.0  <- better */
    double reduced_costs[] = {0.0, 0.0, -2.0, -1.0, 0.0};
    double weights[] = {1.0, 1.0, 16.0, 0.25, 1.0};
    int var_status[] = {0, 1, VAR_AT_LOWER, VAR_AT_LOWER, 2};

    int entering = cxf_pricing_steepest(ctx, reduced_costs, weights, var_status, 5, 1e-6);
    TEST_ASSERT_EQUAL_INT(3, entering);

    cxf_pricing_free(ctx);
}

void test_pricing_steepest_optimal_returns_minus_one(void) {
    PricingContext *ctx = cxf_pricing_create(5, 1);
    TEST_ASSERT_NOT_NULL(ctx);
    cxf_pricing_init(ctx, 5, 2);

    /* No attractive variables */
    double reduced_costs[] = {0.0, 0.0, 0.5, 0.3, 0.0};
    double weights[] = {1.0, 1.0, 1.0, 1.0, 1.0};
    int var_status[] = {0, 1, VAR_AT_LOWER, VAR_AT_LOWER, 2};

    int entering = cxf_pricing_steepest(ctx, reduced_costs, weights, var_status, 5, 1e-6);
    TEST_ASSERT_EQUAL_INT(-1, entering);

    cxf_pricing_free(ctx);
}

void test_pricing_steepest_handles_zero_weight(void) {
    PricingContext *ctx = cxf_pricing_create(5, 1);
    TEST_ASSERT_NOT_NULL(ctx);
    cxf_pricing_init(ctx, 5, 2);

    /* Zero weight should be treated as 1.0 to avoid division by zero */
    double reduced_costs[] = {0.0, 0.0, -2.0, 0.0, 0.0};
    double weights[] = {1.0, 1.0, 0.0, 1.0, 1.0};  /* var 2 has zero weight */
    int var_status[] = {0, 1, VAR_AT_LOWER, 2, 3};

    int entering = cxf_pricing_steepest(ctx, reduced_costs, weights, var_status, 5, 1e-6);
    TEST_ASSERT_EQUAL_INT(2, entering);  /* Should still find var 2 */

    cxf_pricing_free(ctx);
}

/*============================================================================
 * cxf_pricing_update Tests
 *===========================================================================*/

void test_pricing_update_basic(void) {
    PricingContext *ctx = cxf_pricing_create(5, 1);
    TEST_ASSERT_NOT_NULL(ctx);
    cxf_pricing_init(ctx, 5, 1);

    double pivot_column[] = {0.5, 1.0, 0.0};
    double pivot_row[] = {0.1, 0.2, 0.0, 0.0, 0.0};

    int result = cxf_pricing_update(ctx, 2, 1, pivot_column, pivot_row, 3);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);

    cxf_pricing_free(ctx);
}

void test_pricing_update_null_context(void) {
    double pivot_column[] = {0.5, 1.0};
    double pivot_row[] = {0.1, 0.2};

    int result = cxf_pricing_update(NULL, 0, 0, pivot_column, pivot_row, 2);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, result);
}

/*============================================================================
 * cxf_pricing_invalidate Tests
 *===========================================================================*/

void test_pricing_invalidate_candidates(void) {
    PricingContext *ctx = cxf_pricing_create(5, 1);
    TEST_ASSERT_NOT_NULL(ctx);
    cxf_pricing_init(ctx, 5, 1);

    cxf_pricing_invalidate(ctx, CXF_INVALID_CANDIDATES);
    /* After invalidation, cached_counts should be -1 (invalid) */
    TEST_ASSERT_EQUAL_INT(-1, ctx->cached_counts[0]);

    cxf_pricing_free(ctx);
}

void test_pricing_invalidate_all(void) {
    PricingContext *ctx = cxf_pricing_create(5, 3);
    TEST_ASSERT_NOT_NULL(ctx);
    cxf_pricing_init(ctx, 5, 1);

    cxf_pricing_invalidate(ctx, CXF_INVALID_ALL);
    /* All cached counts should be -1 */
    for (int i = 0; i < ctx->max_levels; i++) {
        TEST_ASSERT_EQUAL_INT(-1, ctx->cached_counts[i]);
    }

    cxf_pricing_free(ctx);
}

void test_pricing_invalidate_null_safe(void) {
    cxf_pricing_invalidate(NULL, CXF_INVALID_ALL);  /* Should not crash */
    TEST_PASS();
}

/*============================================================================
 * cxf_pricing_step2 Tests
 *===========================================================================*/

void test_pricing_step2_finds_after_partial_miss(void) {
    PricingContext *ctx = cxf_pricing_create(10, 3);
    TEST_ASSERT_NOT_NULL(ctx);
    cxf_pricing_init(ctx, 10, 1);  /* Partial pricing */

    /* Attractive variable in "far" section */
    double reduced_costs[] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, -2.0, 0.0, 0.0};
    int var_status[] = {0, 1, 2, 3, 4, VAR_AT_LOWER, VAR_AT_LOWER, VAR_AT_LOWER,
                        VAR_AT_LOWER, VAR_AT_LOWER};

    int entering = cxf_pricing_step2(ctx, reduced_costs, var_status, 10, 1e-6);
    TEST_ASSERT_EQUAL_INT(7, entering);

    cxf_pricing_free(ctx);
}

void test_pricing_step2_confirms_optimal(void) {
    PricingContext *ctx = cxf_pricing_create(5, 1);
    TEST_ASSERT_NOT_NULL(ctx);
    cxf_pricing_init(ctx, 5, 1);

    /* Truly optimal: no attractive variables anywhere */
    double reduced_costs[] = {0.0, 0.0, 0.5, 0.3, 0.1};
    int var_status[] = {0, 1, VAR_AT_LOWER, VAR_AT_LOWER, VAR_AT_LOWER};

    int entering = cxf_pricing_step2(ctx, reduced_costs, var_status, 5, 1e-6);
    TEST_ASSERT_EQUAL_INT(-1, entering);  /* Confirms optimal */

    cxf_pricing_free(ctx);
}

/*============================================================================
 * Statistics Tests
 *===========================================================================*/

void test_pricing_statistics_tracked(void) {
    PricingContext *ctx = cxf_pricing_create(5, 1);
    TEST_ASSERT_NOT_NULL(ctx);
    cxf_pricing_init(ctx, 5, 1);

    TEST_ASSERT_EQUAL_INT64(0, ctx->total_candidates_scanned);
    TEST_ASSERT_EQUAL_INT(0, ctx->level_escalations);

    cxf_pricing_free(ctx);
}

/*============================================================================
 * Main
 *===========================================================================*/

int main(void) {
    UNITY_BEGIN();

    /* PricingContext creation/free */
    RUN_TEST(test_pricing_create_basic);
    RUN_TEST(test_pricing_create_null_on_zero_vars);
    RUN_TEST(test_pricing_create_single_level);
    RUN_TEST(test_pricing_free_null_safe);

    /* cxf_pricing_init */
    RUN_TEST(test_pricing_init_basic);
    RUN_TEST(test_pricing_init_null_context);
    RUN_TEST(test_pricing_init_steepest_edge);
    RUN_TEST(test_pricing_init_small_problem);

    /* cxf_pricing_candidates */
    RUN_TEST(test_pricing_candidates_finds_negative_rc);
    RUN_TEST(test_pricing_candidates_finds_positive_rc_at_upper);
    RUN_TEST(test_pricing_candidates_skips_basic_vars);
    RUN_TEST(test_pricing_candidates_optimal);

    /* cxf_pricing_steepest */
    RUN_TEST(test_pricing_steepest_basic);
    RUN_TEST(test_pricing_steepest_considers_weight);
    RUN_TEST(test_pricing_steepest_optimal_returns_minus_one);
    RUN_TEST(test_pricing_steepest_handles_zero_weight);

    /* cxf_pricing_update */
    RUN_TEST(test_pricing_update_basic);
    RUN_TEST(test_pricing_update_null_context);

    /* cxf_pricing_invalidate */
    RUN_TEST(test_pricing_invalidate_candidates);
    RUN_TEST(test_pricing_invalidate_all);
    RUN_TEST(test_pricing_invalidate_null_safe);

    /* cxf_pricing_step2 */
    RUN_TEST(test_pricing_step2_finds_after_partial_miss);
    RUN_TEST(test_pricing_step2_confirms_optimal);

    /* Statistics */
    RUN_TEST(test_pricing_statistics_tracked);

    return UNITY_END();
}
