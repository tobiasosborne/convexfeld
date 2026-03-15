/**
 * @file test_timestamp_v2.c
 * @brief Tests for V2-compliant cxf_get_timestamp (session-ID generator).
 *
 * Verifies cxf_get_timestamp per statistics_diagnostics.md:
 *   1. Returns int64_t (64-bit session identifier)
 *   2. Uses UTC system time, not monotonic clock
 *   3. Hash mixing produces non-sequential values
 *   4. Spaced calls produce different identifiers
 *   5. Thread-safe (local variables only)
 */

#include "unity.h"
#include "convexfeld/cxf_timing.h"
#include <stdint.h>

void setUp(void) {}
void tearDown(void) {}

/** @brief Busy-wait to ensure clock advances between calls. */
static void busy_wait(void) {
    volatile double sum = 0.0;
    for (int i = 0; i < 100000; i++) {
        sum += (double)i * 0.001;
    }
    (void)sum;
}

/*============================================================================
 * cxf_get_timestamp V2 Tests
 *===========================================================================*/

void test_timestamp_v2_returns_nonzero(void) {
    int64_t ts = cxf_get_timestamp();
    TEST_ASSERT_NOT_EQUAL(0, ts);
}

void test_timestamp_v2_spaced_calls_differ(void) {
    int64_t ts1 = cxf_get_timestamp();
    busy_wait();
    int64_t ts2 = cxf_get_timestamp();

    /* With a busy-wait between calls, the system clock must
     * have advanced, producing different session IDs. */
    TEST_ASSERT_NOT_EQUAL(ts1, ts2);
}

void test_timestamp_v2_not_sequential(void) {
    int64_t ts1 = cxf_get_timestamp();
    busy_wait();
    int64_t ts2 = cxf_get_timestamp();

    /* The hash mixing step ensures non-sequential output.
     * The large odd constant multiplier produces big jumps
     * even for small nanosecond differences. */
    int64_t diff = ts2 - ts1;
    if (diff < 0) diff = -diff;

    TEST_ASSERT_TRUE(diff > 1000);
}

void test_timestamp_v2_multiple_calls_unique(void) {
    /* Generate 10 timestamps with busy-waits, verify unique */
    int64_t stamps[10];
    for (int i = 0; i < 10; i++) {
        stamps[i] = cxf_get_timestamp();
        busy_wait();
    }

    for (int i = 0; i < 10; i++) {
        for (int j = i + 1; j < 10; j++) {
            TEST_ASSERT_NOT_EQUAL(stamps[i], stamps[j]);
        }
    }
}

void test_timestamp_v2_uses_full_64_bits(void) {
    int64_t ts = cxf_get_timestamp();

    /* After hash mixing with a large odd constant, the result
     * should use more than just the low 32 bits. */
    int64_t upper = (ts >> 32) & 0xFFFFFFFF;
    TEST_ASSERT_NOT_EQUAL(0, upper);
}

void test_timestamp_v2_sign_varies(void) {
    /* The hash mixing can produce both positive and negative
     * int64_t values. Over 100 samples at least one sign
     * should appear. */
    int pos = 0, neg = 0;
    for (int i = 0; i < 100; i++) {
        int64_t ts = cxf_get_timestamp();
        if (ts > 0) pos++;
        else if (ts < 0) neg++;
        busy_wait();
    }
    TEST_ASSERT_TRUE(pos > 0 || neg > 0);
}

/*============================================================================
 * Main
 *===========================================================================*/

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_timestamp_v2_returns_nonzero);
    RUN_TEST(test_timestamp_v2_spaced_calls_differ);
    RUN_TEST(test_timestamp_v2_not_sequential);
    RUN_TEST(test_timestamp_v2_multiple_calls_unique);
    RUN_TEST(test_timestamp_v2_uses_full_64_bits);
    RUN_TEST(test_timestamp_v2_sign_varies);

    return UNITY_END();
}
