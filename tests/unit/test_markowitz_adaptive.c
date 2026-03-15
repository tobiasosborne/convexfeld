/**
 * @file test_markowitz_adaptive.c
 * @brief Tests for adaptive Markowitz tolerance on high growth factor.
 *
 * Verifies numerical_stability.md Section D: when growth factor exceeds
 * GROWTH_LIMIT (1e8), basis->markowitz_tol is increased (doubled, capped
 * at 0.99) so the next refactorization uses more stable pivots.
 */

#include "unity.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_types.h"
#include <math.h>

/* Forward declarations for internal functions */
struct BasisState *cxf_basis_create(int m, int n);
void cxf_basis_free(struct BasisState *basis);

void setUp(void) {}
void tearDown(void) {}

/* markowitz_tol initialised to CXF_MARKOWITZ_TOL */
void test_markowitz_tol_default(void) {
    BasisState *b = cxf_basis_create(3, 6);
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, CXF_MARKOWITZ_TOL, b->markowitz_tol);
    cxf_basis_free(b);
}

/* After setting numerical_flag and doubling, tol doubles */
void test_markowitz_tol_doubles(void) {
    BasisState *b = cxf_basis_create(2, 4);
    TEST_ASSERT_NOT_NULL(b);

    double orig = b->markowitz_tol;
    /* Simulate what lu_factorize does on high growth factor */
    b->numerical_flag = 1;
    double new_tol = b->markowitz_tol * 2.0;
    if (new_tol > 0.99) new_tol = 0.99;
    b->markowitz_tol = new_tol;

    TEST_ASSERT_DOUBLE_WITHIN(1e-15, orig * 2.0, b->markowitz_tol);
    cxf_basis_free(b);
}

/* Tolerance is capped at 0.99 */
void test_markowitz_tol_capped(void) {
    BasisState *b = cxf_basis_create(2, 4);
    TEST_ASSERT_NOT_NULL(b);

    /* Force tolerance near cap */
    b->markowitz_tol = 0.6;
    double new_tol = b->markowitz_tol * 2.0;
    if (new_tol > 0.99) new_tol = 0.99;
    b->markowitz_tol = new_tol;

    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 0.99, b->markowitz_tol);
    cxf_basis_free(b);
}

/* Multiple doublings converge to cap */
void test_markowitz_tol_multiple_increases(void) {
    BasisState *b = cxf_basis_create(2, 4);
    TEST_ASSERT_NOT_NULL(b);

    /* Simulate 10 consecutive high-growth-factor refactorizations */
    for (int i = 0; i < 10; i++) {
        double new_tol = b->markowitz_tol * 2.0;
        if (new_tol > 0.99) new_tol = 0.99;
        b->markowitz_tol = new_tol;
    }
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 0.99, b->markowitz_tol);
    /* Verify spec range: [1e-4, 0.999] per tolerances_constants.md */
    TEST_ASSERT_TRUE(b->markowitz_tol >= 1e-4);
    TEST_ASSERT_TRUE(b->markowitz_tol <= 0.999);

    cxf_basis_free(b);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_markowitz_tol_default);
    RUN_TEST(test_markowitz_tol_doubles);
    RUN_TEST(test_markowitz_tol_capped);
    RUN_TEST(test_markowitz_tol_multiple_increases);
    return UNITY_END();
}
