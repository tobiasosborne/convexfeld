/**
 * @file test_pool_capacity.c
 * @brief Tests for V2 eta pool capacity formula (simplex_lifecycle.md Phase 2).
 *
 * Spec: poolCapacity = max(MIN_POOL_SIZE=10000, maxDimension, nnz/10)
 *       etaPoolEntries = poolCapacity + nnz
 *       etaPoolBytes   = etaPoolEntries * 32
 *
 * Beads: convexfeld-gm35
 */

#include "unity.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_basis.h"

/* Internal function under test */
size_t cxf_eta_pool_capacity(int m, int n, int64_t nnz);

void setUp(void) {}
void tearDown(void) {}

/* V2 spec: MIN_POOL_SIZE = 10000 dominates for tiny problems */
void test_tiny_problem_uses_min_pool_size(void) {
    /* m=5, n=3, nnz=10: all dimensions < 10000 */
    size_t cap = cxf_eta_pool_capacity(5, 3, 10);
    /* poolCapacity = max(10000, 5, 1) = 10000 */
    /* etaPoolEntries = 10000 + 10 = 10010 */
    /* etaPoolBytes = 10010 * 32 = 320320 */
    TEST_ASSERT_EQUAL_size_t(320320, cap);
}

/* V2 spec: maxDimension = max(m, n) dominates when > 10000 */
void test_large_m_dominates(void) {
    /* m=20000, n=100, nnz=500 */
    size_t cap = cxf_eta_pool_capacity(20000, 100, 500);
    /* poolCapacity = max(10000, 20000, 50) = 20000 */
    /* etaPoolEntries = 20000 + 500 = 20500 */
    /* etaPoolBytes = 20500 * 32 = 656000 */
    TEST_ASSERT_EQUAL_size_t(656000, cap);
}

/* V2 spec: n can dominate maxDimension */
void test_large_n_dominates(void) {
    /* m=100, n=15000, nnz=200 */
    size_t cap = cxf_eta_pool_capacity(100, 15000, 200);
    /* poolCapacity = max(10000, 15000, 20) = 15000 */
    /* etaPoolEntries = 15000 + 200 = 15200 */
    /* etaPoolBytes = 15200 * 32 = 486400 */
    TEST_ASSERT_EQUAL_size_t(486400, cap);
}

/* V2 spec: nnz/10 dominates for sparse-but-large problems */
void test_nnz_dominates(void) {
    /* m=100, n=200, nnz=500000 */
    size_t cap = cxf_eta_pool_capacity(100, 200, 500000);
    /* poolCapacity = max(10000, 200, 50000) = 50000 */
    /* etaPoolEntries = 50000 + 500000 = 550000 */
    /* etaPoolBytes = 550000 * 32 = 17600000 */
    TEST_ASSERT_EQUAL_size_t(17600000, cap);
}

/* Edge case: zero dimensions (degenerate problem) */
void test_zero_dimensions(void) {
    size_t cap = cxf_eta_pool_capacity(0, 0, 0);
    /* poolCapacity = max(10000, 0, 0) = 10000 */
    /* etaPoolEntries = 10000 + 0 = 10000 */
    /* etaPoolBytes = 10000 * 32 = 320000 */
    TEST_ASSERT_EQUAL_size_t(320000, cap);
}

/* Netlib-scale: afiro (m=27, n=32, nnz=83) */
void test_afiro_scale(void) {
    size_t cap = cxf_eta_pool_capacity(27, 32, 83);
    /* poolCapacity = max(10000, 32, 8) = 10000 */
    /* etaPoolEntries = 10000 + 83 = 10083 */
    /* etaPoolBytes = 10083 * 32 = 322656 */
    TEST_ASSERT_EQUAL_size_t(322656, cap);
}

/* Netlib-scale: share2b (m=96, n=79, nnz=730) */
void test_share2b_scale(void) {
    size_t cap = cxf_eta_pool_capacity(96, 79, 730);
    /* poolCapacity = max(10000, 96, 73) = 10000 */
    /* etaPoolEntries = 10000 + 730 = 10730 */
    /* etaPoolBytes = 10730 * 32 = 343360 */
    TEST_ASSERT_EQUAL_size_t(343360, cap);
}

/* Result must be >= CXF_MIN_CHUNK_SIZE (the pool create enforces this) */
void test_result_exceeds_min_chunk(void) {
    size_t cap = cxf_eta_pool_capacity(1, 1, 1);
    TEST_ASSERT_TRUE(cap >= CXF_MIN_CHUNK_SIZE);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_tiny_problem_uses_min_pool_size);
    RUN_TEST(test_large_m_dominates);
    RUN_TEST(test_large_n_dominates);
    RUN_TEST(test_nnz_dominates);
    RUN_TEST(test_zero_dimensions);
    RUN_TEST(test_afiro_scale);
    RUN_TEST(test_share2b_scale);
    RUN_TEST(test_result_exceeds_min_chunk);
    return UNITY_END();
}
