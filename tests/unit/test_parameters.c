/**
 * @file test_parameters.c
 * @brief TDD tests for parameters module (M2.2.1)
 *
 * Tests for parameter getter functions:
 * - cxf_get_feasibility_tol
 * - cxf_get_optimality_tol
 * - cxf_get_infinity
 */

#include "unity.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_env.h"

/* Forward declarations for parameter functions */
int cxf_getdblparam(CxfEnv *env, const char *paramname, double *valueP);
double cxf_get_feasibility_tol(CxfEnv *env);
double cxf_get_optimality_tol(CxfEnv *env);
double cxf_get_infinity(void);

/* API functions */
int cxf_loadenv(CxfEnv **envP, const char *logfilename);
void cxf_freeenv(CxfEnv *env);

/* Test fixtures */
static CxfEnv *env = NULL;

void setUp(void) {
    cxf_loadenv(&env, NULL);
}

void tearDown(void) {
    if (env) {
        cxf_freeenv(env);
        env = NULL;
    }
}

/*============================================================================
 * cxf_get_feasibility_tol Tests
 *===========================================================================*/

void test_feasibility_tol_returns_default(void) {
    double tol = cxf_get_feasibility_tol(env);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, CXF_FEASIBILITY_TOL, tol);
}

void test_feasibility_tol_null_env_returns_default(void) {
    double tol = cxf_get_feasibility_tol(NULL);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, CXF_FEASIBILITY_TOL, tol);
}

void test_feasibility_tol_positive(void) {
    double tol = cxf_get_feasibility_tol(env);
    TEST_ASSERT_TRUE(tol > 0.0);
}

void test_feasibility_tol_in_valid_range(void) {
    double tol = cxf_get_feasibility_tol(env);
    TEST_ASSERT_TRUE(tol >= 1e-9 && tol <= 1e-2);
}

void test_feasibility_tol_idempotent(void) {
    double tol1 = cxf_get_feasibility_tol(env);
    double tol2 = cxf_get_feasibility_tol(env);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, tol1, tol2);
}

/*============================================================================
 * cxf_get_optimality_tol Tests
 *===========================================================================*/

void test_optimality_tol_returns_default(void) {
    double tol = cxf_get_optimality_tol(env);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, CXF_OPTIMALITY_TOL, tol);
}

void test_optimality_tol_null_env_returns_default(void) {
    double tol = cxf_get_optimality_tol(NULL);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, CXF_OPTIMALITY_TOL, tol);
}

void test_optimality_tol_positive(void) {
    double tol = cxf_get_optimality_tol(env);
    TEST_ASSERT_TRUE(tol > 0.0);
}

void test_optimality_tol_in_valid_range(void) {
    double tol = cxf_get_optimality_tol(env);
    TEST_ASSERT_TRUE(tol >= 1e-9 && tol <= 1e-2);
}

void test_optimality_tol_idempotent(void) {
    double tol1 = cxf_get_optimality_tol(env);
    double tol2 = cxf_get_optimality_tol(env);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, tol1, tol2);
}

/*============================================================================
 * cxf_get_infinity Tests
 *===========================================================================*/

void test_infinity_returns_constant(void) {
    double inf = cxf_get_infinity();
    TEST_ASSERT_DOUBLE_WITHIN(1e90, CXF_INFINITY, inf);
}

void test_infinity_is_positive(void) {
    double inf = cxf_get_infinity();
    TEST_ASSERT_TRUE(inf > 0.0);
}

void test_infinity_is_finite(void) {
    double inf = cxf_get_infinity();
    TEST_ASSERT_TRUE(inf < 1e101);  /* Not IEEE infinity */
}

void test_infinity_idempotent(void) {
    double inf1 = cxf_get_infinity();
    double inf2 = cxf_get_infinity();
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, inf1, inf2);
}

void test_infinity_usable_in_comparisons(void) {
    double inf = cxf_get_infinity();
    double large = 1e50;
    TEST_ASSERT_TRUE(large < inf);
    TEST_ASSERT_TRUE(-inf < -large);
}

/*============================================================================
 * cxf_getdblparam Tests
 *===========================================================================*/

void test_getdblparam_feasibility_tol(void) {
    double value;
    int result = cxf_getdblparam(env, "FeasibilityTol", &value);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, CXF_FEASIBILITY_TOL, value);
}

void test_getdblparam_optimality_tol(void) {
    double value;
    int result = cxf_getdblparam(env, "OptimalityTol", &value);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, CXF_OPTIMALITY_TOL, value);
}

void test_getdblparam_infinity(void) {
    double value;
    int result = cxf_getdblparam(env, "Infinity", &value);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);
    TEST_ASSERT_DOUBLE_WITHIN(1e90, CXF_INFINITY, value);
}

void test_getdblparam_case_insensitive(void) {
    double value;
    int result = cxf_getdblparam(env, "feasibilitytol", &value);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);

    result = cxf_getdblparam(env, "FEASIBILITYTOL", &value);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);
}

void test_getdblparam_null_env(void) {
    double value;
    int result = cxf_getdblparam(NULL, "FeasibilityTol", &value);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, result);
}

void test_getdblparam_null_paramname(void) {
    double value;
    int result = cxf_getdblparam(env, NULL, &value);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, result);
}

void test_getdblparam_null_valueP(void) {
    int result = cxf_getdblparam(env, "FeasibilityTol", NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, result);
}

void test_getdblparam_unknown_param(void) {
    double value;
    int result = cxf_getdblparam(env, "NonExistentParam", &value);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, result);
}

/*============================================================================
 * Main
 *===========================================================================*/

int main(void) {
    UNITY_BEGIN();

    /* cxf_get_feasibility_tol tests */
    RUN_TEST(test_feasibility_tol_returns_default);
    RUN_TEST(test_feasibility_tol_null_env_returns_default);
    RUN_TEST(test_feasibility_tol_positive);
    RUN_TEST(test_feasibility_tol_in_valid_range);
    RUN_TEST(test_feasibility_tol_idempotent);

    /* cxf_get_optimality_tol tests */
    RUN_TEST(test_optimality_tol_returns_default);
    RUN_TEST(test_optimality_tol_null_env_returns_default);
    RUN_TEST(test_optimality_tol_positive);
    RUN_TEST(test_optimality_tol_in_valid_range);
    RUN_TEST(test_optimality_tol_idempotent);

    /* cxf_get_infinity tests */
    RUN_TEST(test_infinity_returns_constant);
    RUN_TEST(test_infinity_is_positive);
    RUN_TEST(test_infinity_is_finite);
    RUN_TEST(test_infinity_idempotent);
    RUN_TEST(test_infinity_usable_in_comparisons);

    /* cxf_getdblparam tests */
    RUN_TEST(test_getdblparam_feasibility_tol);
    RUN_TEST(test_getdblparam_optimality_tol);
    RUN_TEST(test_getdblparam_infinity);
    RUN_TEST(test_getdblparam_case_insensitive);
    RUN_TEST(test_getdblparam_null_env);
    RUN_TEST(test_getdblparam_null_paramname);
    RUN_TEST(test_getdblparam_null_valueP);
    RUN_TEST(test_getdblparam_unknown_param);

    return UNITY_END();
}
