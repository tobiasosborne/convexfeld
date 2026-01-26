/**
 * @file test_timing.c
 * @brief TDD tests for timing functions (M4.2.1)
 *
 * Tests for timing module functions:
 * - cxf_get_timestamp
 * - cxf_timing_start
 * - cxf_timing_end
 * - cxf_timing_update
 */

#include "unity.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_timing.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_env.h"
#include <math.h>
#include <string.h>

/* Forward declarations for operation timing functions */
void cxf_timing_pivot(SolverContext *state,
                      double pricing_work,
                      double ratio_work,
                      double update_work);
int cxf_timing_refactor(SolverContext *state, CxfEnv *env);

/* API functions */
int cxf_loadenv(CxfEnv **envP, const char *logfilename);
void cxf_freeenv(CxfEnv *env);

/* Test fixtures */
static TimingState timing;

void setUp(void) {
    /* Initialize timing structure to zero */
    timing.start_time = 0.0;
    timing.elapsed = 0.0;
    timing.current_section = 0;
    timing.iteration_rate = 0.0;

    for (int i = 0; i < CXF_MAX_TIMING_SECTIONS; i++) {
        timing.total_time[i] = 0.0;
        timing.operation_count[i] = 0;
        timing.last_elapsed[i] = 0.0;
        timing.avg_time[i] = 0.0;
    }
}

void tearDown(void) {
    /* Nothing to clean up */
}

/*============================================================================
 * cxf_get_timestamp Tests
 *===========================================================================*/

void test_get_timestamp_returns_positive(void) {
    double ts = cxf_get_timestamp();
    TEST_ASSERT_GREATER_THAN(0.0, ts);
}

void test_get_timestamp_monotonic(void) {
    double ts1 = cxf_get_timestamp();
    double ts2 = cxf_get_timestamp();
    TEST_ASSERT_GREATER_OR_EQUAL(ts1, ts2);
}

void test_get_timestamp_elapsed_reasonable(void) {
    double start = cxf_get_timestamp();

    /* Busy loop to consume some time - result is used to prevent optimization */
    volatile double sum = 0.0;
    for (int i = 0; i < 1000000; i++) {
        sum += (double)i * 0.001;
    }

    double end = cxf_get_timestamp();
    double elapsed = end - start;

    /* Use sum to prevent optimization */
    TEST_ASSERT_TRUE(sum > 0.0);

    /* Elapsed should be non-negative and less than 1 second */
    TEST_ASSERT_GREATER_OR_EQUAL(0.0, elapsed);
    TEST_ASSERT_LESS_THAN(1.0, elapsed);
}

/*============================================================================
 * cxf_timing_start Tests
 *===========================================================================*/

void test_timing_start_records_timestamp(void) {
    cxf_timing_start(&timing);
    TEST_ASSERT_GREATER_THAN(0.0, timing.start_time);
}

void test_timing_start_null_safe(void) {
    cxf_timing_start(NULL);
    TEST_PASS();  /* Should not crash */
}

void test_timing_start_overwrites_previous(void) {
    cxf_timing_start(&timing);
    double first = timing.start_time;

    /* Wait a tiny bit - use result to prevent optimization */
    volatile double sum = 0.0;
    for (int i = 0; i < 1000000; i++) {
        sum += (double)i * 0.001;
    }
    TEST_ASSERT_TRUE(sum > 0.0);

    cxf_timing_start(&timing);
    /* Second timestamp should be >= first (monotonic) */
    TEST_ASSERT_GREATER_OR_EQUAL(first, timing.start_time);
}

/*============================================================================
 * cxf_timing_end Tests
 *===========================================================================*/

void test_timing_end_calculates_elapsed(void) {
    cxf_timing_start(&timing);

    /* Do some work - use result to prevent optimization */
    volatile double sum = 0.0;
    for (int i = 0; i < 1000000; i++) {
        sum += (double)i * 0.001;
    }
    TEST_ASSERT_TRUE(sum > 0.0);

    cxf_timing_end(&timing);
    /* Elapsed should be non-negative */
    TEST_ASSERT_GREATER_OR_EQUAL(0.0, timing.elapsed);
}

void test_timing_end_null_safe(void) {
    cxf_timing_end(NULL);
    TEST_PASS();  /* Should not crash */
}

void test_timing_end_updates_section_stats(void) {
    timing.current_section = 0;
    cxf_timing_start(&timing);

    /* Do some work - use result to prevent optimization */
    volatile double sum = 0.0;
    for (int i = 0; i < 1000000; i++) {
        sum += (double)i * 0.001;
    }
    TEST_ASSERT_TRUE(sum > 0.0);

    cxf_timing_end(&timing);

    /* Section 0 should have accumulated time (non-negative) and count */
    TEST_ASSERT_GREATER_OR_EQUAL(0.0, timing.total_time[0]);
    TEST_ASSERT_EQUAL_INT(1, timing.operation_count[0]);
}

void test_timing_end_invalid_section_safe(void) {
    timing.current_section = 100;  /* Invalid section */
    cxf_timing_start(&timing);
    cxf_timing_end(&timing);

    /* Should not crash; stats should be unchanged */
    for (int i = 0; i < CXF_MAX_TIMING_SECTIONS; i++) {
        TEST_ASSERT_EQUAL_INT(0, timing.operation_count[i]);
    }
}

/*============================================================================
 * cxf_timing_update Tests
 *===========================================================================*/

void test_timing_update_accumulates_time(void) {
    timing.elapsed = 0.5;  /* 500 ms */
    cxf_timing_update(&timing, 0);

    TEST_ASSERT_DOUBLE_WITHIN(0.001, 0.5, timing.total_time[0]);
    TEST_ASSERT_EQUAL_INT(1, timing.operation_count[0]);
}

void test_timing_update_multiple_accumulations(void) {
    timing.elapsed = 0.1;
    cxf_timing_update(&timing, 0);

    timing.elapsed = 0.2;
    cxf_timing_update(&timing, 0);

    timing.elapsed = 0.3;
    cxf_timing_update(&timing, 0);

    TEST_ASSERT_DOUBLE_WITHIN(0.001, 0.6, timing.total_time[0]);
    TEST_ASSERT_EQUAL_INT(3, timing.operation_count[0]);
}

void test_timing_update_computes_average(void) {
    timing.elapsed = 0.1;
    cxf_timing_update(&timing, 0);

    timing.elapsed = 0.3;
    cxf_timing_update(&timing, 0);

    /* Average should be (0.1 + 0.3) / 2 = 0.2 */
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 0.2, timing.avg_time[0]);
}

void test_timing_update_null_safe(void) {
    cxf_timing_update(NULL, 0);
    TEST_PASS();  /* Should not crash */
}

void test_timing_update_invalid_category_safe(void) {
    timing.elapsed = 0.5;
    cxf_timing_update(&timing, -1);
    cxf_timing_update(&timing, 100);

    /* Should not crash; no stats should be modified */
    for (int i = 0; i < CXF_MAX_TIMING_SECTIONS; i++) {
        TEST_ASSERT_EQUAL_INT(0, timing.operation_count[i]);
    }
}

void test_timing_update_stores_last_elapsed(void) {
    timing.elapsed = 0.123;
    cxf_timing_update(&timing, 2);

    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.123, timing.last_elapsed[2]);
}

void test_timing_update_different_categories(void) {
    timing.elapsed = 0.1;
    cxf_timing_update(&timing, 0);

    timing.elapsed = 0.2;
    cxf_timing_update(&timing, 1);

    timing.elapsed = 0.3;
    cxf_timing_update(&timing, 2);

    TEST_ASSERT_DOUBLE_WITHIN(0.001, 0.1, timing.total_time[0]);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 0.2, timing.total_time[1]);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 0.3, timing.total_time[2]);
}

/*============================================================================
 * cxf_timing_pivot Tests
 *===========================================================================*/

void test_timing_pivot_null_safe(void) {
    cxf_timing_pivot(NULL, 1.0, 2.0, 3.0);
    TEST_PASS();  /* Should not crash */
}

void test_timing_pivot_updates_work_counter(void) {
    /* Create a solver context with work tracking */
    SolverContext ctx;
    memset(&ctx, 0, sizeof(ctx));

    double work_counter = 0.0;
    ctx.work_counter = &work_counter;
    ctx.scale_factor = 1.0;
    ctx.timing = NULL;  /* Disable timing stats */

    cxf_timing_pivot(&ctx, 10.0, 20.0, 30.0);

    /* Total work = 10 + 20 + 30 = 60, scaled by 1.0 */
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 60.0, work_counter);
}

void test_timing_pivot_scales_work(void) {
    SolverContext ctx;
    memset(&ctx, 0, sizeof(ctx));

    double work_counter = 0.0;
    ctx.work_counter = &work_counter;
    ctx.scale_factor = 0.5;  /* Half scale */
    ctx.timing = NULL;

    cxf_timing_pivot(&ctx, 10.0, 10.0, 10.0);

    /* Total work = 30, scaled by 0.5 = 15 */
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 15.0, work_counter);
}

void test_timing_pivot_updates_timing_stats(void) {
    SolverContext ctx;
    memset(&ctx, 0, sizeof(ctx));

    TimingState ts;
    memset(&ts, 0, sizeof(ts));

    ctx.work_counter = NULL;  /* Disable work tracking */
    ctx.timing = &ts;

    cxf_timing_pivot(&ctx, 1.0, 2.0, 3.0);

    /* Check category 0 (total) operation count */
    TEST_ASSERT_EQUAL_INT(1, ts.operation_count[0]);

    /* Check phase times accumulated (categories 1, 2, 3) */
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 1.0, ts.total_time[1]);  /* Pricing */
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 2.0, ts.total_time[2]);  /* Ratio */
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 3.0, ts.total_time[3]);  /* Update */
}

void test_timing_pivot_accumulates_multiple_calls(void) {
    SolverContext ctx;
    memset(&ctx, 0, sizeof(ctx));

    double work_counter = 0.0;
    ctx.work_counter = &work_counter;
    ctx.scale_factor = 1.0;
    ctx.timing = NULL;

    cxf_timing_pivot(&ctx, 10.0, 10.0, 10.0);
    cxf_timing_pivot(&ctx, 5.0, 5.0, 5.0);
    cxf_timing_pivot(&ctx, 2.0, 2.0, 2.0);

    /* Total = 30 + 15 + 6 = 51 */
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 51.0, work_counter);
}

/*============================================================================
 * cxf_timing_refactor Tests
 *===========================================================================*/

void test_timing_refactor_null_state(void) {
    CxfEnv *env = NULL;
    cxf_loadenv(&env, NULL);

    int result = cxf_timing_refactor(NULL, env);
    TEST_ASSERT_EQUAL_INT(0, result);  /* NULL returns 0 */

    cxf_freeenv(env);
}

void test_timing_refactor_null_env(void) {
    SolverContext ctx;
    memset(&ctx, 0, sizeof(ctx));

    int result = cxf_timing_refactor(&ctx, NULL);
    TEST_ASSERT_EQUAL_INT(0, result);  /* NULL returns 0 */
}

void test_timing_refactor_not_needed(void) {
    CxfEnv *env = NULL;
    cxf_loadenv(&env, NULL);
    env->max_eta_count = 100;
    env->max_eta_memory = 1000000;
    env->refactor_interval = 100;

    SolverContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.eta_count = 0;  /* No eta vectors */
    ctx.iteration = 0;
    ctx.last_refactor_iter = 0;

    int result = cxf_timing_refactor(&ctx, env);
    TEST_ASSERT_EQUAL_INT(0, result);  /* Not needed */

    cxf_freeenv(env);
}

void test_timing_refactor_required_eta_count(void) {
    CxfEnv *env = NULL;
    cxf_loadenv(&env, NULL);
    env->max_eta_count = 100;
    env->max_eta_memory = 1000000;
    env->refactor_interval = 200;

    SolverContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.eta_count = 150;  /* Exceeds limit */

    int result = cxf_timing_refactor(&ctx, env);
    TEST_ASSERT_EQUAL_INT(2, result);  /* Required */

    cxf_freeenv(env);
}

void test_timing_refactor_required_eta_memory(void) {
    CxfEnv *env = NULL;
    cxf_loadenv(&env, NULL);
    env->max_eta_count = 1000;  /* High limit */
    env->max_eta_memory = 1000;  /* Low memory limit */
    env->refactor_interval = 200;

    SolverContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.eta_count = 10;
    ctx.eta_memory = 2000;  /* Exceeds memory limit */

    int result = cxf_timing_refactor(&ctx, env);
    TEST_ASSERT_EQUAL_INT(2, result);  /* Required */

    cxf_freeenv(env);
}

void test_timing_refactor_recommended_iterations(void) {
    CxfEnv *env = NULL;
    cxf_loadenv(&env, NULL);
    env->max_eta_count = 1000;
    env->max_eta_memory = 10000000;
    env->refactor_interval = 50;

    SolverContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.eta_count = 10;
    ctx.iteration = 100;
    ctx.last_refactor_iter = 0;  /* 100 iterations since refactor */

    int result = cxf_timing_refactor(&ctx, env);
    TEST_ASSERT_EQUAL_INT(1, result);  /* Recommended */

    cxf_freeenv(env);
}

void test_timing_refactor_recommended_ftran_degradation(void) {
    CxfEnv *env = NULL;
    cxf_loadenv(&env, NULL);
    env->max_eta_count = 1000;
    env->max_eta_memory = 10000000;
    env->refactor_interval = 1000;  /* High interval */

    SolverContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.eta_count = 10;
    ctx.baseline_ftran = 0.001;  /* 1 ms baseline */
    ctx.total_ftran_time = 0.5;  /* 500 ms total */
    ctx.ftran_count = 100;       /* Average = 5 ms (5x baseline) */

    int result = cxf_timing_refactor(&ctx, env);
    TEST_ASSERT_EQUAL_INT(1, result);  /* Recommended */

    cxf_freeenv(env);
}

/*============================================================================
 * Main
 *===========================================================================*/

int main(void) {
    UNITY_BEGIN();

    /* cxf_get_timestamp tests */
    RUN_TEST(test_get_timestamp_returns_positive);
    RUN_TEST(test_get_timestamp_monotonic);
    RUN_TEST(test_get_timestamp_elapsed_reasonable);

    /* cxf_timing_start tests */
    RUN_TEST(test_timing_start_records_timestamp);
    RUN_TEST(test_timing_start_null_safe);
    RUN_TEST(test_timing_start_overwrites_previous);

    /* cxf_timing_end tests */
    RUN_TEST(test_timing_end_calculates_elapsed);
    RUN_TEST(test_timing_end_null_safe);
    RUN_TEST(test_timing_end_updates_section_stats);
    RUN_TEST(test_timing_end_invalid_section_safe);

    /* cxf_timing_update tests */
    RUN_TEST(test_timing_update_accumulates_time);
    RUN_TEST(test_timing_update_multiple_accumulations);
    RUN_TEST(test_timing_update_computes_average);
    RUN_TEST(test_timing_update_null_safe);
    RUN_TEST(test_timing_update_invalid_category_safe);
    RUN_TEST(test_timing_update_stores_last_elapsed);
    RUN_TEST(test_timing_update_different_categories);

    /* cxf_timing_pivot tests */
    RUN_TEST(test_timing_pivot_null_safe);
    RUN_TEST(test_timing_pivot_updates_work_counter);
    RUN_TEST(test_timing_pivot_scales_work);
    RUN_TEST(test_timing_pivot_updates_timing_stats);
    RUN_TEST(test_timing_pivot_accumulates_multiple_calls);

    /* cxf_timing_refactor tests */
    RUN_TEST(test_timing_refactor_null_state);
    RUN_TEST(test_timing_refactor_null_env);
    RUN_TEST(test_timing_refactor_not_needed);
    RUN_TEST(test_timing_refactor_required_eta_count);
    RUN_TEST(test_timing_refactor_required_eta_memory);
    RUN_TEST(test_timing_refactor_recommended_iterations);
    RUN_TEST(test_timing_refactor_recommended_ftran_degradation);

    return UNITY_END();
}
