/**
 * @file test_analysis.c
 * @brief TDD tests for analysis operations (M4.3.1, M4.3.2)
 *
 * Tests for model type classification functions.
 * Tests MUST be written BEFORE implementation (TDD).
 */

#include "unity.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"

/* Forward declarations for functions under test */

/* Model Type Checks (M4.3.2) */
int cxf_is_mip_model(CxfModel *model);
int cxf_is_quadratic(CxfModel *model);
int cxf_is_socp(CxfModel *model);

/* Presolve Statistics (M4.3.4) */
void cxf_presolve_stats(CxfModel *model);

/* API functions for setup/teardown */
int cxf_loadenv(CxfEnv **envP, const char *logfilename);
int cxf_freeenv(CxfEnv *env);
int cxf_newmodel(CxfEnv *env, CxfModel **modelP, const char *name, int numvars, double *obj, double *lb, double *ub, char *vtype, char **varnames);
void cxf_freemodel(CxfModel *model);
int cxf_addvar(CxfModel *model, int numnz, int *vind, double *vval, double obj, double lb, double ub, char vtype, const char *varname);

/* Test fixtures */
static CxfEnv *env = NULL;

void setUp(void) {
    cxf_loadenv(&env, NULL);
}

void tearDown(void) {
    cxf_freeenv(env);
    env = NULL;
}

/*============================================================================
 * cxf_is_mip_model Tests
 *===========================================================================*/

void test_is_mip_null_model(void) {
    int result = cxf_is_mip_model(NULL);
    TEST_ASSERT_EQUAL_INT(0, result);  /* NULL returns 0 */
}

void test_is_mip_empty_model(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_NOT_NULL(model);

    int result = cxf_is_mip_model(model);
    TEST_ASSERT_EQUAL_INT(0, result);  /* Empty is not MIP */

    cxf_freemodel(model);
}

void test_is_mip_all_continuous(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_NOT_NULL(model);

    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x0");
    cxf_addvar(model, 0, NULL, NULL, 2.0, 0.0, 10.0, 'C', "x1");
    cxf_addvar(model, 0, NULL, NULL, 3.0, 0.0, 10.0, 'C', "x2");

    int result = cxf_is_mip_model(model);
    TEST_ASSERT_EQUAL_INT(0, result);  /* All continuous, not MIP */

    cxf_freemodel(model);
}

void test_is_mip_with_binary(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_NOT_NULL(model);

    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x0");
    cxf_addvar(model, 0, NULL, NULL, 2.0, 0.0, 1.0, 'B', "y");  /* Binary variable */

    int result = cxf_is_mip_model(model);
    TEST_ASSERT_EQUAL_INT(1, result);  /* Has binary, is MIP */

    cxf_freemodel(model);
}

void test_is_mip_with_integer(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_NOT_NULL(model);

    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x0");
    cxf_addvar(model, 0, NULL, NULL, 2.0, 0.0, 100.0, 'I', "n");  /* Integer variable */

    int result = cxf_is_mip_model(model);
    TEST_ASSERT_EQUAL_INT(1, result);  /* Has integer, is MIP */

    cxf_freemodel(model);
}

void test_is_mip_with_semi_continuous(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_NOT_NULL(model);

    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x0");
    cxf_addvar(model, 0, NULL, NULL, 2.0, 5.0, 100.0, 'S', "s");  /* Semi-continuous */

    int result = cxf_is_mip_model(model);
    TEST_ASSERT_EQUAL_INT(1, result);  /* Has semi-continuous, is MIP */

    cxf_freemodel(model);
}

void test_is_mip_with_semi_integer(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_NOT_NULL(model);

    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x0");
    cxf_addvar(model, 0, NULL, NULL, 2.0, 5.0, 100.0, 'N', "n");  /* Semi-integer */

    int result = cxf_is_mip_model(model);
    TEST_ASSERT_EQUAL_INT(1, result);  /* Has semi-integer, is MIP */

    cxf_freemodel(model);
}

/*============================================================================
 * cxf_is_quadratic Tests
 *===========================================================================*/

void test_is_quadratic_null_model(void) {
    int result = cxf_is_quadratic(NULL);
    TEST_ASSERT_EQUAL_INT(0, result);  /* NULL returns 0 */
}

void test_is_quadratic_linear_model(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_NOT_NULL(model);

    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x0");
    cxf_addvar(model, 0, NULL, NULL, 2.0, 0.0, 10.0, 'C', "x1");

    int result = cxf_is_quadratic(model);
    TEST_ASSERT_EQUAL_INT(0, result);  /* Pure linear, not QP */

    cxf_freemodel(model);
}

/*============================================================================
 * cxf_is_socp Tests
 *===========================================================================*/

void test_is_socp_null_model(void) {
    int result = cxf_is_socp(NULL);
    TEST_ASSERT_EQUAL_INT(0, result);  /* NULL returns 0 */
}

void test_is_socp_linear_model(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_NOT_NULL(model);

    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x0");
    cxf_addvar(model, 0, NULL, NULL, 2.0, 0.0, 10.0, 'C', "x1");

    int result = cxf_is_socp(model);
    TEST_ASSERT_EQUAL_INT(0, result);  /* Pure linear, not SOCP */

    cxf_freemodel(model);
}

/*============================================================================
 * cxf_presolve_stats Tests
 *===========================================================================*/

void test_presolve_stats_null_model(void) {
    /* Should not crash with NULL */
    cxf_presolve_stats(NULL);
    TEST_PASS();
}

void test_presolve_stats_empty_model(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "empty", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_NOT_NULL(model);

    /* Should not crash with empty model */
    cxf_presolve_stats(model);
    TEST_PASS();

    cxf_freemodel(model);
}

void test_presolve_stats_with_vars(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test_lp", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_NOT_NULL(model);

    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x0");
    cxf_addvar(model, 0, NULL, NULL, 2.0, 0.0, 10.0, 'C', "x1");
    cxf_addvar(model, 0, NULL, NULL, 3.0, 0.0, 10.0, 'C', "x2");

    /* Should log model dimensions */
    cxf_presolve_stats(model);
    TEST_PASS();

    cxf_freemodel(model);
}

/*============================================================================
 * Main
 *===========================================================================*/

int main(void) {
    UNITY_BEGIN();

    /* cxf_is_mip_model tests */
    RUN_TEST(test_is_mip_null_model);
    RUN_TEST(test_is_mip_empty_model);
    RUN_TEST(test_is_mip_all_continuous);
    RUN_TEST(test_is_mip_with_binary);
    RUN_TEST(test_is_mip_with_integer);
    RUN_TEST(test_is_mip_with_semi_continuous);
    RUN_TEST(test_is_mip_with_semi_integer);

    /* cxf_is_quadratic tests */
    RUN_TEST(test_is_quadratic_null_model);
    RUN_TEST(test_is_quadratic_linear_model);

    /* cxf_is_socp tests */
    RUN_TEST(test_is_socp_null_model);
    RUN_TEST(test_is_socp_linear_model);

    /* cxf_presolve_stats tests */
    RUN_TEST(test_presolve_stats_null_model);
    RUN_TEST(test_presolve_stats_empty_model);
    RUN_TEST(test_presolve_stats_with_vars);

    return UNITY_END();
}
