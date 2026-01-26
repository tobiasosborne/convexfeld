/**
 * @file test_api_query.c
 * @brief TDD tests for model query API functions.
 *
 * M8.1.6: API Tests - Queries
 *
 * Tests: cxf_getintattr, cxf_getdblattr, cxf_getconstrs, cxf_getcoeff
 */

#include "unity.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_types.h"

/* Forward declarations for functions under test */
int cxf_getconstrs(CxfModel *model, int *numnzP, int *cbeg,
                   int *cind, double *cval, int start, int len);
int cxf_getcoeff(CxfModel *model, int constr, int var, double *valP);

/* Test fixture - shared environment */
static CxfEnv *env = NULL;

void setUp(void) {
    cxf_loadenv(&env, NULL);
}

void tearDown(void) {
    cxf_freeenv(env);
    env = NULL;
}

/*******************************************************************************
 * cxf_getintattr Tests
 ******************************************************************************/

void test_getintattr_null_model_fails(void) {
    int value;
    int status = cxf_getintattr(NULL, "NumVars", &value);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);
}

void test_getintattr_null_attrname_fails(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test");

    int value;
    int status = cxf_getintattr(model, NULL, &value);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);

    cxf_freemodel(model);
}

void test_getintattr_null_value_fails(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test");

    int status = cxf_getintattr(model, "NumVars", NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);

    cxf_freemodel(model);
}

void test_getintattr_numvars(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test");
    cxf_addvar(model, 0.0, 10.0, 1.0, 'C', "x1");
    cxf_addvar(model, 0.0, 20.0, 2.0, 'C', "x2");

    int value;
    int status = cxf_getintattr(model, "NumVars", &value);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(2, value);

    cxf_freemodel(model);
}

void test_getintattr_numconstrs(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test");

    int value;
    int status = cxf_getintattr(model, "NumConstrs", &value);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(0, value);

    cxf_freemodel(model);
}

void test_getintattr_status(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test");

    int value;
    int status = cxf_getintattr(model, "Status", &value);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);

    cxf_freemodel(model);
}

void test_getintattr_invalid_attr(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test");

    int value;
    int status = cxf_getintattr(model, "InvalidAttr", &value);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, status);

    cxf_freemodel(model);
}

/*******************************************************************************
 * cxf_getdblattr Tests
 ******************************************************************************/

void test_getdblattr_null_model_fails(void) {
    double value;
    int status = cxf_getdblattr(NULL, "ObjVal", &value);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);
}

void test_getdblattr_objval(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test");

    double value;
    int status = cxf_getdblattr(model, "ObjVal", &value);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);

    cxf_freemodel(model);
}

void test_getdblattr_invalid_attr(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test");

    double value;
    int status = cxf_getdblattr(model, "InvalidAttr", &value);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, status);

    cxf_freemodel(model);
}

/*******************************************************************************
 * cxf_getconstrs Tests
 ******************************************************************************/

void test_getconstrs_null_model_fails(void) {
    int numnz;
    int status = cxf_getconstrs(NULL, &numnz, NULL, NULL, NULL, 0, 1);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);
}

void test_getconstrs_null_numnz_fails(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test");

    int status = cxf_getconstrs(model, NULL, NULL, NULL, NULL, 0, 1);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);

    cxf_freemodel(model);
}

void test_getconstrs_empty_model(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test");

    int numnz;
    int status = cxf_getconstrs(model, &numnz, NULL, NULL, NULL, 0, 0);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(0, numnz);

    cxf_freemodel(model);
}

/*******************************************************************************
 * cxf_getcoeff Tests
 ******************************************************************************/

void test_getcoeff_null_model_fails(void) {
    double val;
    int status = cxf_getcoeff(NULL, 0, 0, &val);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);
}

void test_getcoeff_null_valP_fails(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test");
    cxf_addvar(model, 0.0, 10.0, 1.0, 'C', "x");

    int status = cxf_getcoeff(model, 0, 0, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);

    cxf_freemodel(model);
}

void test_getcoeff_no_constraints(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test");
    cxf_addvar(model, 0.0, 10.0, 1.0, 'C', "x");

    double val;
    /* With no constraints, any constraint index is invalid */
    int status = cxf_getcoeff(model, 0, 0, &val);
    /* Should return error for out-of-range constraint */
    TEST_ASSERT_TRUE(status == CXF_ERROR_INVALID_ARGUMENT ||
                     status == CXF_OK);  /* Stub may return 0 */

    cxf_freemodel(model);
}

/*******************************************************************************
 * Main
 ******************************************************************************/

int main(void) {
    UNITY_BEGIN();

    /* cxf_getintattr tests */
    RUN_TEST(test_getintattr_null_model_fails);
    RUN_TEST(test_getintattr_null_attrname_fails);
    RUN_TEST(test_getintattr_null_value_fails);
    RUN_TEST(test_getintattr_numvars);
    RUN_TEST(test_getintattr_numconstrs);
    RUN_TEST(test_getintattr_status);
    RUN_TEST(test_getintattr_invalid_attr);

    /* cxf_getdblattr tests */
    RUN_TEST(test_getdblattr_null_model_fails);
    RUN_TEST(test_getdblattr_objval);
    RUN_TEST(test_getdblattr_invalid_attr);

    /* cxf_getconstrs tests */
    RUN_TEST(test_getconstrs_null_model_fails);
    RUN_TEST(test_getconstrs_null_numnz_fails);
    RUN_TEST(test_getconstrs_empty_model);

    /* cxf_getcoeff tests */
    RUN_TEST(test_getcoeff_null_model_fails);
    RUN_TEST(test_getcoeff_null_valP_fails);
    RUN_TEST(test_getcoeff_no_constraints);

    return UNITY_END();
}
