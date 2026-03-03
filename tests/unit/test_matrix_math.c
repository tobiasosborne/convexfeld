/**
 * @file test_matrix_math.c
 * @brief TDD tests for dot product and vector norm operations (M4.1.4)
 *
 * Split from test_matrix.c for 200 LOC limit.
 */

#include "unity.h"
#include "convexfeld/cxf_matrix.h"
#include "convexfeld/cxf_types.h"
#include <math.h>

/*******************************************************************************
 * External function declarations (to be implemented)
 ******************************************************************************/

/* Functions to be implemented in M4.1.4 */
double cxf_dot_product(const double *x, const double *y, int n);
double cxf_dot_product_sparse(const int *x_indices, const double *x_values,
                              int x_nnz, const double *y_dense);
double cxf_vector_norm(const double *x, int n, int norm_type);

/*******************************************************************************
 * Test fixtures
 ******************************************************************************/

void setUp(void) {}
void tearDown(void) {}

/*******************************************************************************
 * cxf_dot_product tests
 ******************************************************************************/

void test_dot_product_basic(void) {
    double x[] = {1.0, 2.0, 3.0};
    double y[] = {4.0, 5.0, 6.0};

    /* Expected: 1*4 + 2*5 + 3*6 = 4 + 10 + 18 = 32 */
    double result = cxf_dot_product(x, y, 3);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 32.0, result);
}

void test_dot_product_single_element(void) {
    double x[] = {5.0};
    double y[] = {3.0};

    double result = cxf_dot_product(x, y, 1);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 15.0, result);
}

void test_dot_product_orthogonal(void) {
    double x[] = {1.0, 0.0, 0.0};
    double y[] = {0.0, 1.0, 0.0};

    double result = cxf_dot_product(x, y, 3);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 0.0, result);
}

void test_dot_product_self(void) {
    double x[] = {3.0, 4.0};  /* 3^2 + 4^2 = 25 */

    double result = cxf_dot_product(x, x, 2);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 25.0, result);
}

void test_dot_product_sparse_dense(void) {
    int x_indices[] = {0, 2, 4};
    double x_values[] = {1.0, 2.0, 3.0};
    double y_dense[] = {10.0, 20.0, 30.0, 40.0, 50.0};

    /* Expected: 1*10 + 2*30 + 3*50 = 10 + 60 + 150 = 220 */
    double result = cxf_dot_product_sparse(x_indices, x_values, 3, y_dense);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 220.0, result);
}

void test_dot_product_sparse_empty(void) {
    double y_dense[] = {10.0, 20.0, 30.0};

    double result = cxf_dot_product_sparse(NULL, NULL, 0, y_dense);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 0.0, result);
}

/*******************************************************************************
 * cxf_vector_norm tests
 ******************************************************************************/

void test_vector_norm_l1(void) {
    double x[] = {1.0, -2.0, 3.0, -4.0};

    /* L1 = |1| + |-2| + |3| + |-4| = 10 */
    double result = cxf_vector_norm(x, 4, 1);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 10.0, result);
}

void test_vector_norm_l2(void) {
    double x[] = {3.0, 4.0};

    /* L2 = sqrt(9 + 16) = 5 */
    double result = cxf_vector_norm(x, 2, 2);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 5.0, result);
}

void test_vector_norm_linf(void) {
    double x[] = {1.0, -5.0, 3.0, -2.0};

    /* Linf = max(|1|, |-5|, |3|, |-2|) = 5 */
    double result = cxf_vector_norm(x, 4, 0);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 5.0, result);
}

void test_vector_norm_zero_vector(void) {
    double x[] = {0.0, 0.0, 0.0};

    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 0.0, cxf_vector_norm(x, 3, 0));
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 0.0, cxf_vector_norm(x, 3, 1));
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 0.0, cxf_vector_norm(x, 3, 2));
}

void test_vector_norm_single_element(void) {
    double x[] = {-7.0};

    /* All norms of single element = |element| */
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 7.0, cxf_vector_norm(x, 1, 0));
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 7.0, cxf_vector_norm(x, 1, 1));
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 7.0, cxf_vector_norm(x, 1, 2));
}

/*******************************************************************************
 * Main test runner
 ******************************************************************************/

int main(void) {
    UNITY_BEGIN();

    /* Dot product tests */
    RUN_TEST(test_dot_product_basic);
    RUN_TEST(test_dot_product_single_element);
    RUN_TEST(test_dot_product_orthogonal);
    RUN_TEST(test_dot_product_self);
    RUN_TEST(test_dot_product_sparse_dense);
    RUN_TEST(test_dot_product_sparse_empty);

    /* Vector norm tests */
    RUN_TEST(test_vector_norm_l1);
    RUN_TEST(test_vector_norm_l2);
    RUN_TEST(test_vector_norm_linf);
    RUN_TEST(test_vector_norm_zero_vector);
    RUN_TEST(test_vector_norm_single_element);

    return UNITY_END();
}
