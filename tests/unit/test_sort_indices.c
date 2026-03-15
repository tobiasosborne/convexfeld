/**
 * @file test_sort_indices.c
 * @brief Tests for cxf_sort_indices (V2 spec: values ascending, indices satellite)
 *
 * Verifies that cxf_sort_indices sorts the VALUES array in ascending order
 * and permutes the INDICES array identically, per matrix_core.md spec.
 */

#include "unity.h"
#include <stdint.h>

/* V2 spec function under test */
void cxf_sort_indices(int64_t count, double *values, int *indices);

void setUp(void) {}
void tearDown(void) {}

/*============================================================================
 * Basic functionality
 *===========================================================================*/

void test_sort_indices_values_ascending(void) {
    double values[]  = {3.0, 1.0, 2.0};
    int    indices[] = {10,  20,  30};

    cxf_sort_indices(3, values, indices);

    /* Values sorted ascending */
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 1.0, values[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 2.0, values[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 3.0, values[2]);

    /* Indices follow their original values */
    TEST_ASSERT_EQUAL_INT(20, indices[0]);  /* was with 1.0 */
    TEST_ASSERT_EQUAL_INT(30, indices[1]);  /* was with 2.0 */
    TEST_ASSERT_EQUAL_INT(10, indices[2]);  /* was with 3.0 */
}

void test_sort_indices_already_sorted(void) {
    double values[]  = {1.0, 2.0, 3.0, 4.0};
    int    indices[] = {0,   1,   2,   3};

    cxf_sort_indices(4, values, indices);

    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_DOUBLE_WITHIN(1e-15, (double)(i + 1), values[i]);
        TEST_ASSERT_EQUAL_INT(i, indices[i]);
    }
}

void test_sort_indices_reverse_order(void) {
    double values[]  = {5.0, 4.0, 3.0, 2.0, 1.0};
    int    indices[] = {50,  40,  30,  20,  10};

    cxf_sort_indices(5, values, indices);

    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 1.0, values[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 5.0, values[4]);
    TEST_ASSERT_EQUAL_INT(10, indices[0]);
    TEST_ASSERT_EQUAL_INT(50, indices[4]);
}

/*============================================================================
 * Edge cases
 *===========================================================================*/

void test_sort_indices_single_element(void) {
    double values[]  = {42.0};
    int    indices[] = {7};

    cxf_sort_indices(1, values, indices);

    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 42.0, values[0]);
    TEST_ASSERT_EQUAL_INT(7, indices[0]);
}

void test_sort_indices_count_zero(void) {
    double values[]  = {1.0};
    int    indices[] = {1};

    cxf_sort_indices(0, values, indices);

    /* Arrays unchanged */
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 1.0, values[0]);
    TEST_ASSERT_EQUAL_INT(1, indices[0]);
}

void test_sort_indices_null_values(void) {
    int indices[] = {1, 2, 3};
    cxf_sort_indices(3, NULL, indices);
    /* No crash — graceful no-op */
    TEST_ASSERT_EQUAL_INT(1, indices[0]);
}

void test_sort_indices_null_indices(void) {
    double values[] = {3.0, 1.0, 2.0};
    cxf_sort_indices(3, values, NULL);
    /* No crash — graceful no-op */
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 3.0, values[0]);
}

/*============================================================================
 * Duplicate values
 *===========================================================================*/

void test_sort_indices_duplicate_values(void) {
    double values[]  = {2.0, 1.0, 2.0, 1.0};
    int    indices[] = {100, 200, 300, 400};

    cxf_sort_indices(4, values, indices);

    /* Values sorted: 1.0, 1.0, 2.0, 2.0 */
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 1.0, values[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 1.0, values[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 2.0, values[2]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 2.0, values[3]);

    /* Each index must be one of the originals for that value */
    TEST_ASSERT_TRUE(indices[0] == 200 || indices[0] == 400);
    TEST_ASSERT_TRUE(indices[1] == 200 || indices[1] == 400);
    TEST_ASSERT_TRUE(indices[2] == 100 || indices[2] == 300);
    TEST_ASSERT_TRUE(indices[3] == 100 || indices[3] == 300);
}

/*============================================================================
 * Negative and mixed-sign values
 *===========================================================================*/

void test_sort_indices_negative_values(void) {
    double values[]  = {0.0, -3.0, 5.0, -1.0, 2.0};
    int    indices[] = {0,    1,   2,    3,   4};

    cxf_sort_indices(5, values, indices);

    TEST_ASSERT_DOUBLE_WITHIN(1e-15, -3.0, values[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, -1.0, values[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15,  0.0, values[2]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15,  2.0, values[3]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15,  5.0, values[4]);

    TEST_ASSERT_EQUAL_INT(1, indices[0]);
    TEST_ASSERT_EQUAL_INT(3, indices[1]);
    TEST_ASSERT_EQUAL_INT(0, indices[2]);
    TEST_ASSERT_EQUAL_INT(4, indices[3]);
    TEST_ASSERT_EQUAL_INT(2, indices[4]);
}

/*============================================================================
 * Larger array (triggers introsort, not just insertion sort)
 *===========================================================================*/

void test_sort_indices_large_array(void) {
    /* 20 elements — exceeds INTROSORT_THRESHOLD of 16 */
    double values[20];
    int    indices[20];

    for (int i = 0; i < 20; i++) {
        values[i] = (double)(20 - i);  /* 20, 19, ..., 1 */
        indices[i] = i * 10;
    }

    cxf_sort_indices(20, values, indices);

    for (int i = 0; i < 20; i++) {
        TEST_ASSERT_DOUBLE_WITHIN(1e-15, (double)(i + 1), values[i]);
        TEST_ASSERT_EQUAL_INT((19 - i) * 10, indices[i]);
    }
}

/*============================================================================
 * Main
 *===========================================================================*/

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_sort_indices_values_ascending);
    RUN_TEST(test_sort_indices_already_sorted);
    RUN_TEST(test_sort_indices_reverse_order);
    RUN_TEST(test_sort_indices_single_element);
    RUN_TEST(test_sort_indices_count_zero);
    RUN_TEST(test_sort_indices_null_values);
    RUN_TEST(test_sort_indices_null_indices);
    RUN_TEST(test_sort_indices_duplicate_values);
    RUN_TEST(test_sort_indices_negative_values);
    RUN_TEST(test_sort_indices_large_array);

    return UNITY_END();
}
