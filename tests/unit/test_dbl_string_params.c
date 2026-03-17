/**
 * @file test_dbl_string_params.c
 * @brief Tests for cxf_setdblparam, cxf_getdblparam,
 *        cxf_setstringparam, cxf_getstringparam.
 */

#include "unity.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_types.h"
#include <string.h>

static CxfEnv *env;

void setUp(void) {
    env = NULL;
    cxf_loadenv(&env, NULL);
}

void tearDown(void) {
    cxf_freeenv(env);
    env = NULL;
}

/*******************************************************************************
 * cxf_setdblparam / cxf_getdblparam round-trip
 ******************************************************************************/

void test_dblparam_round_trip_feasibility_tol(void) {
    double val = 0.0;
    TEST_ASSERT_EQUAL_INT(CXF_OK, cxf_setdblparam(env, "FeasibilityTol", 1e-8));
    TEST_ASSERT_EQUAL_INT(CXF_OK, cxf_getdblparam(env, "FeasibilityTol", &val));
    TEST_ASSERT_EQUAL_DOUBLE(1e-8, val);
}

void test_dblparam_round_trip_optimality_tol(void) {
    double val = 0.0;
    TEST_ASSERT_EQUAL_INT(CXF_OK, cxf_setdblparam(env, "OptimalityTol", 1e-4));
    TEST_ASSERT_EQUAL_INT(CXF_OK, cxf_getdblparam(env, "OptimalityTol", &val));
    TEST_ASSERT_EQUAL_DOUBLE(1e-4, val);
}

void test_dblparam_round_trip_infinity(void) {
    double val = 0.0;
    TEST_ASSERT_EQUAL_INT(CXF_OK, cxf_setdblparam(env, "Infinity", 1e20));
    TEST_ASSERT_EQUAL_INT(CXF_OK, cxf_getdblparam(env, "Infinity", &val));
    TEST_ASSERT_EQUAL_DOUBLE(1e20, val);
}

/*******************************************************************************
 * cxf_setdblparam range validation
 ******************************************************************************/

void test_dblparam_feasibility_tol_too_small(void) {
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_VALUE_OUT_OF_RANGE,
                          cxf_setdblparam(env, "FeasibilityTol", 1e-13));
}

void test_dblparam_feasibility_tol_too_large(void) {
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_VALUE_OUT_OF_RANGE,
                          cxf_setdblparam(env, "FeasibilityTol", 0.1));
}

void test_dblparam_optimality_tol_too_small(void) {
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_VALUE_OUT_OF_RANGE,
                          cxf_setdblparam(env, "OptimalityTol", 1e-13));
}

void test_dblparam_optimality_tol_too_large(void) {
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_VALUE_OUT_OF_RANGE,
                          cxf_setdblparam(env, "OptimalityTol", 0.1));
}

void test_dblparam_infinity_too_small(void) {
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_VALUE_OUT_OF_RANGE,
                          cxf_setdblparam(env, "Infinity", 1e10));
}

void test_dblparam_infinity_too_large(void) {
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_VALUE_OUT_OF_RANGE,
                          cxf_setdblparam(env, "Infinity", 1e31));
}

/*******************************************************************************
 * cxf_setdblparam error handling
 ******************************************************************************/

void test_dblparam_null_env(void) {
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT,
                          cxf_setdblparam(NULL, "FeasibilityTol", 1e-6));
}

void test_dblparam_null_paramname(void) {
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT,
                          cxf_setdblparam(env, NULL, 1e-6));
}

void test_dblparam_unknown_name(void) {
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT,
                          cxf_setdblparam(env, "NoSuchParam", 1.0));
}

/*******************************************************************************
 * cxf_setdblparam case-insensitive matching
 ******************************************************************************/

void test_dblparam_case_insensitive(void) {
    TEST_ASSERT_EQUAL_INT(CXF_OK,
                          cxf_setdblparam(env, "feasibilitytol", 1e-7));
    double val = 0.0;
    cxf_getdblparam(env, "FEASIBILITYTOL", &val);
    TEST_ASSERT_EQUAL_DOUBLE(1e-7, val);
}

/*******************************************************************************
 * cxf_setstringparam / cxf_getstringparam round-trip
 ******************************************************************************/

void test_stringparam_round_trip_logfile(void) {
    char buf[256];
    TEST_ASSERT_EQUAL_INT(CXF_OK,
                          cxf_setstringparam(env, "LogFile", "/tmp/test.log"));
    TEST_ASSERT_EQUAL_INT(CXF_OK,
                          cxf_getstringparam(env, "LogFile", buf, 256));
    TEST_ASSERT_EQUAL_STRING("/tmp/test.log", buf);
}

void test_stringparam_clear_logfile(void) {
    char buf[256];
    cxf_setstringparam(env, "LogFile", "/tmp/test.log");
    TEST_ASSERT_EQUAL_INT(CXF_OK,
                          cxf_setstringparam(env, "LogFile", NULL));
    TEST_ASSERT_EQUAL_INT(CXF_OK,
                          cxf_getstringparam(env, "LogFile", buf, 256));
    TEST_ASSERT_EQUAL_STRING("", buf);
}

void test_stringparam_overwrite_logfile(void) {
    char buf[256];
    cxf_setstringparam(env, "LogFile", "/tmp/a.log");
    cxf_setstringparam(env, "LogFile", "/tmp/b.log");
    cxf_getstringparam(env, "LogFile", buf, 256);
    TEST_ASSERT_EQUAL_STRING("/tmp/b.log", buf);
}

/*******************************************************************************
 * cxf_getstringparam truncation
 ******************************************************************************/

void test_stringparam_truncation(void) {
    char buf[8];
    cxf_setstringparam(env, "LogFile", "/tmp/longpath.log");
    cxf_getstringparam(env, "LogFile", buf, 8);
    TEST_ASSERT_EQUAL_STRING("/tmp/lo", buf);
}

/*******************************************************************************
 * cxf_setstringparam / cxf_getstringparam error handling
 ******************************************************************************/

void test_stringparam_null_env(void) {
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT,
                          cxf_setstringparam(NULL, "LogFile", "x"));
}

void test_stringparam_null_paramname(void) {
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT,
                          cxf_setstringparam(env, NULL, "x"));
}

void test_stringparam_unknown_name(void) {
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT,
                          cxf_setstringparam(env, "NoSuchParam", "x"));
}

void test_getstringparam_null_env(void) {
    char buf[32];
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT,
                          cxf_getstringparam(NULL, "LogFile", buf, 32));
}

void test_getstringparam_null_valuep(void) {
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT,
                          cxf_getstringparam(env, "LogFile", NULL, 32));
}

void test_getstringparam_zero_bufsize(void) {
    char buf[32];
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT,
                          cxf_getstringparam(env, "LogFile", buf, 0));
}

/*******************************************************************************
 * Test Runner
 ******************************************************************************/

int main(void) {
    UNITY_BEGIN();

    /* double param round-trip */
    RUN_TEST(test_dblparam_round_trip_feasibility_tol);
    RUN_TEST(test_dblparam_round_trip_optimality_tol);
    RUN_TEST(test_dblparam_round_trip_infinity);

    /* double param range validation */
    RUN_TEST(test_dblparam_feasibility_tol_too_small);
    RUN_TEST(test_dblparam_feasibility_tol_too_large);
    RUN_TEST(test_dblparam_optimality_tol_too_small);
    RUN_TEST(test_dblparam_optimality_tol_too_large);
    RUN_TEST(test_dblparam_infinity_too_small);
    RUN_TEST(test_dblparam_infinity_too_large);

    /* double param error handling */
    RUN_TEST(test_dblparam_null_env);
    RUN_TEST(test_dblparam_null_paramname);
    RUN_TEST(test_dblparam_unknown_name);
    RUN_TEST(test_dblparam_case_insensitive);

    /* string param round-trip */
    RUN_TEST(test_stringparam_round_trip_logfile);
    RUN_TEST(test_stringparam_clear_logfile);
    RUN_TEST(test_stringparam_overwrite_logfile);
    RUN_TEST(test_stringparam_truncation);

    /* string param error handling */
    RUN_TEST(test_stringparam_null_env);
    RUN_TEST(test_stringparam_null_paramname);
    RUN_TEST(test_stringparam_unknown_name);
    RUN_TEST(test_getstringparam_null_env);
    RUN_TEST(test_getstringparam_null_valuep);
    RUN_TEST(test_getstringparam_zero_bufsize);

    return UNITY_END();
}
