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
#include <math.h>

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

    return UNITY_END();
}
