/**
 * @file test_smoke.c
 * @brief Smoke test to verify Unity framework is working.
 */

#include "unity.h"

void setUp(void) {
    /* Called before each test */
}

void tearDown(void) {
    /* Called after each test */
}

void test_unity_works(void) {
    TEST_ASSERT_TRUE(1);
}

void test_integer_equality(void) {
    TEST_ASSERT_EQUAL_INT(42, 42);
}

void test_double_comparison(void) {
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 3.14159, 3.14159);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_unity_works);
    RUN_TEST(test_integer_equality);
    RUN_TEST(test_double_comparison);
    return UNITY_END();
}
