/**
 * @file test_error_fallback.c
 * @brief Tests for cxf_error_message fallback: unknown error codes must
 *        include the numeric code value (V2 error_handling.md).
 *
 * Spec: "Error codes that do not appear in the table receive a generic
 *        fallback message that includes the numeric error code value."
 */

#include "unity.h"
#include <string.h>

/* Function under test */
extern const char *cxf_error_message(int error_code);

void setUp(void) {}
void tearDown(void) {}

/*============================================================================
 * Fallback format tests
 *===========================================================================*/

void test_fallback_includes_positive_code(void) {
    const char *msg = cxf_error_message(42);
    TEST_ASSERT_EQUAL_STRING("Unknown error (code 42)", msg);
}

void test_fallback_includes_large_code(void) {
    const char *msg = cxf_error_message(99999);
    TEST_ASSERT_EQUAL_STRING("Unknown error (code 99999)", msg);
}

void test_fallback_includes_negative_code(void) {
    const char *msg = cxf_error_message(-1);
    TEST_ASSERT_EQUAL_STRING("Unknown error (code -1)", msg);
}

void test_fallback_zero_code(void) {
    /* Zero is not a known error; fallback should include "0" */
    const char *msg = cxf_error_message(0);
    TEST_ASSERT_EQUAL_STRING("Unknown error (code 0)", msg);
}

void test_fallback_contains_substring_code(void) {
    /* Verify the numeric value appears somewhere in the string */
    const char *msg = cxf_error_message(77777);
    TEST_ASSERT_NOT_NULL(strstr(msg, "77777"));
}

void test_known_code_not_fallback(void) {
    /* Known codes must NOT use the fallback format */
    const char *msg = cxf_error_message(10001);
    TEST_ASSERT_EQUAL_STRING("Out of memory", msg);
    TEST_ASSERT_NULL(strstr(msg, "code"));
}

/*============================================================================
 * Main
 *===========================================================================*/

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_fallback_includes_positive_code);
    RUN_TEST(test_fallback_includes_large_code);
    RUN_TEST(test_fallback_includes_negative_code);
    RUN_TEST(test_fallback_zero_code);
    RUN_TEST(test_fallback_contains_substring_code);
    RUN_TEST(test_known_code_not_fallback);

    return UNITY_END();
}
