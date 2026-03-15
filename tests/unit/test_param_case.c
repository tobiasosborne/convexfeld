/**
 * @file test_param_case.c
 * @brief Tests for case-insensitive parameter name lookup.
 *
 * V2 spec (parameter_system.md, environment_lifecycle.md) requires
 * parameter names converted to uppercase in the lookup structure,
 * meaning all lookups must be case-insensitive.
 */

#include "unity.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_types.h"

static CxfEnv *env;

void setUp(void) {
    env = NULL;
    cxf_loadenv(&env, NULL);
}

void tearDown(void) {
    cxf_freeenv(env);
    env = NULL;
}

/*---------------------------------------------------------------------------
 * cxf_setintparam case-insensitive tests
 *---------------------------------------------------------------------------*/

void test_setintparam_lowercase_outputflag(void) {
    int status = cxf_setintparam(env, "outputflag", 0);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(0, env->output_flag);
}

void test_setintparam_uppercase_outputflag(void) {
    int status = cxf_setintparam(env, "OUTPUTFLAG", 0);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(0, env->output_flag);
}

void test_setintparam_mixedcase_outputflag(void) {
    int status = cxf_setintparam(env, "oUtPuTfLaG", 0);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(0, env->output_flag);
}

void test_setintparam_lowercase_verbosity(void) {
    int status = cxf_setintparam(env, "verbosity", 2);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(2, env->verbosity);
}

void test_setintparam_uppercase_verbosity(void) {
    int status = cxf_setintparam(env, "VERBOSITY", 2);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(2, env->verbosity);
}

void test_setintparam_lowercase_refactorinterval(void) {
    int status = cxf_setintparam(env, "refactorinterval", 200);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(200, env->refactor_interval);
}

void test_setintparam_uppercase_maxetacount(void) {
    int status = cxf_setintparam(env, "MAXETACOUNT", 50);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(50, env->max_eta_count);
}

/*---------------------------------------------------------------------------
 * cxf_getintparam case-insensitive tests
 *---------------------------------------------------------------------------*/

void test_getintparam_lowercase_outputflag(void) {
    int value = -1;
    int status = cxf_getintparam(env, "outputflag", &value);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(1, value); /* default */
}

void test_getintparam_uppercase_outputflag(void) {
    int value = -1;
    int status = cxf_getintparam(env, "OUTPUTFLAG", &value);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(1, value);
}

void test_getintparam_mixedcase_verbosity(void) {
    int value = -1;
    int status = cxf_getintparam(env, "vErBoSiTy", &value);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(1, value);
}

void test_getintparam_uppercase_refactorinterval(void) {
    int value = -1;
    int status = cxf_getintparam(env, "REFACTORINTERVAL", &value);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(50, value);
}

void test_getintparam_lowercase_maxetacount(void) {
    int value = -1;
    int status = cxf_getintparam(env, "maxetacount", &value);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(100, value);
}

/*---------------------------------------------------------------------------
 * Round-trip: set with one case, get with another
 *---------------------------------------------------------------------------*/

void test_roundtrip_set_lower_get_upper(void) {
    int value = -1;
    cxf_setintparam(env, "outputflag", 0);
    int status = cxf_getintparam(env, "OUTPUTFLAG", &value);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(0, value);
}

void test_roundtrip_set_upper_get_lower(void) {
    int value = -1;
    cxf_setintparam(env, "VERBOSITY", 2);
    int status = cxf_getintparam(env, "verbosity", &value);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(2, value);
}

/*---------------------------------------------------------------------------
 * cxf_getdblparam case-insensitive tests (params.c already uses
 * strcasecmp_local -- verify it works)
 *---------------------------------------------------------------------------*/

void test_getdblparam_lowercase_feasibilitytol(void) {
    double value = -1.0;
    int status = cxf_getdblparam(env, "feasibilitytol", &value);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, CXF_FEASIBILITY_TOL, value);
}

void test_getdblparam_uppercase_optimalitytol(void) {
    double value = -1.0;
    int status = cxf_getdblparam(env, "OPTIMALITYTOL", &value);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, CXF_OPTIMALITY_TOL, value);
}

void test_getdblparam_mixedcase_infinity(void) {
    double value = -1.0;
    int status = cxf_getdblparam(env, "iNfInItY", &value);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 1e100, value);
}

/*---------------------------------------------------------------------------
 * Test Runner
 *---------------------------------------------------------------------------*/

int main(void) {
    UNITY_BEGIN();

    /* setintparam case-insensitive */
    RUN_TEST(test_setintparam_lowercase_outputflag);
    RUN_TEST(test_setintparam_uppercase_outputflag);
    RUN_TEST(test_setintparam_mixedcase_outputflag);
    RUN_TEST(test_setintparam_lowercase_verbosity);
    RUN_TEST(test_setintparam_uppercase_verbosity);
    RUN_TEST(test_setintparam_lowercase_refactorinterval);
    RUN_TEST(test_setintparam_uppercase_maxetacount);

    /* getintparam case-insensitive */
    RUN_TEST(test_getintparam_lowercase_outputflag);
    RUN_TEST(test_getintparam_uppercase_outputflag);
    RUN_TEST(test_getintparam_mixedcase_verbosity);
    RUN_TEST(test_getintparam_uppercase_refactorinterval);
    RUN_TEST(test_getintparam_lowercase_maxetacount);

    /* Round-trip with different cases */
    RUN_TEST(test_roundtrip_set_lower_get_upper);
    RUN_TEST(test_roundtrip_set_upper_get_lower);

    /* getdblparam case-insensitive (params.c) */
    RUN_TEST(test_getdblparam_lowercase_feasibilitytol);
    RUN_TEST(test_getdblparam_uppercase_optimalitytol);
    RUN_TEST(test_getdblparam_mixedcase_infinity);

    return UNITY_END();
}
