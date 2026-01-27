/**
 * @file test_attrs_api.c
 * @brief Tests for attribute API functions (M8.1.15).
 *
 * Tests cxf_getintattr and cxf_getdblattr with all supported attributes.
 */

#include "unity.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_types.h"

/* Test fixture - shared environment and model */
static CxfEnv *env = NULL;
static CxfModel *model = NULL;

void setUp(void) {
    cxf_loadenv(&env, NULL);
    cxf_newmodel(env, &model, "test_model");
}

void tearDown(void) {
    cxf_freemodel(model);
    cxf_freeenv(env);
    model = NULL;
    env = NULL;
}

/*******************************************************************************
 * cxf_getintattr Tests
 ******************************************************************************/

void test_getintattr_null_model(void) {
    int value;
    int status = cxf_getintattr(NULL, "NumVars", &value);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);
}

void test_getintattr_null_attrname(void) {
    int value;
    int status = cxf_getintattr(model, NULL, &value);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);
}

void test_getintattr_null_value(void) {
    int status = cxf_getintattr(model, "NumVars", NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);
}

void test_getintattr_invalid_attribute(void) {
    int value;
    int status = cxf_getintattr(model, "InvalidAttr", &value);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, status);
}

void test_getintattr_status(void) {
    int value;
    int status = cxf_getintattr(model, "Status", &value);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(CXF_OK, value);
}

void test_getintattr_numvars(void) {
    /* Add 3 variables */
    cxf_addvar(model, 0.0, 10.0, 1.0, 'C', "x1");
    cxf_addvar(model, 0.0, 20.0, 2.0, 'C', "x2");
    cxf_addvar(model, 0.0, 5.0, 0.5, 'C', "x3");

    int value;
    int status = cxf_getintattr(model, "NumVars", &value);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(3, value);
}

void test_getintattr_numconstrs(void) {
    int value;
    int status = cxf_getintattr(model, "NumConstrs", &value);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(0, value);
}

void test_getintattr_modelsense(void) {
    int value;
    int status = cxf_getintattr(model, "ModelSense", &value);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(1, value); /* Default: minimize */
}

void test_getintattr_ismip(void) {
    int value;
    int status = cxf_getintattr(model, "IsMIP", &value);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(0, value); /* LP only for now */
}

/*******************************************************************************
 * cxf_getdblattr Tests
 ******************************************************************************/

void test_getdblattr_null_model(void) {
    double value;
    int status = cxf_getdblattr(NULL, "ObjVal", &value);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);
}

void test_getdblattr_null_attrname(void) {
    double value;
    int status = cxf_getdblattr(model, NULL, &value);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);
}

void test_getdblattr_null_value(void) {
    int status = cxf_getdblattr(model, "ObjVal", NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);
}

void test_getdblattr_invalid_attribute(void) {
    double value;
    int status = cxf_getdblattr(model, "InvalidAttr", &value);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, status);
}

void test_getdblattr_objval(void) {
    model->obj_val = 42.5;

    double value;
    int status = cxf_getdblattr(model, "ObjVal", &value);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_DOUBLE(42.5, value);
}

void test_getdblattr_runtime(void) {
    model->update_time = 1.234;

    double value;
    int status = cxf_getdblattr(model, "Runtime", &value);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_DOUBLE(1.234, value);
}

void test_getdblattr_objbound(void) {
    model->obj_val = 100.0;

    double value;
    int status = cxf_getdblattr(model, "ObjBound", &value);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_DOUBLE(100.0, value);
}

void test_getdblattr_objboundc(void) {
    model->obj_val = 200.0;

    double value;
    int status = cxf_getdblattr(model, "ObjBoundC", &value);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_DOUBLE(200.0, value);
}

void test_getdblattr_maxcoeff(void) {
    double value;
    int status = cxf_getdblattr(model, "MaxCoeff", &value);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_DOUBLE(1.0, value); /* Stub value */
}

void test_getdblattr_mincoeff(void) {
    double value;
    int status = cxf_getdblattr(model, "MinCoeff", &value);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_DOUBLE(1.0, value); /* Stub value */
}

/*******************************************************************************
 * Test Runner
 ******************************************************************************/

int main(void) {
    UNITY_BEGIN();

    /* Integer attribute tests */
    RUN_TEST(test_getintattr_null_model);
    RUN_TEST(test_getintattr_null_attrname);
    RUN_TEST(test_getintattr_null_value);
    RUN_TEST(test_getintattr_invalid_attribute);
    RUN_TEST(test_getintattr_status);
    RUN_TEST(test_getintattr_numvars);
    RUN_TEST(test_getintattr_numconstrs);
    RUN_TEST(test_getintattr_modelsense);
    RUN_TEST(test_getintattr_ismip);

    /* Double attribute tests */
    RUN_TEST(test_getdblattr_null_model);
    RUN_TEST(test_getdblattr_null_attrname);
    RUN_TEST(test_getdblattr_null_value);
    RUN_TEST(test_getdblattr_invalid_attribute);
    RUN_TEST(test_getdblattr_objval);
    RUN_TEST(test_getdblattr_runtime);
    RUN_TEST(test_getdblattr_objbound);
    RUN_TEST(test_getdblattr_objboundc);
    RUN_TEST(test_getdblattr_maxcoeff);
    RUN_TEST(test_getdblattr_mincoeff);

    return UNITY_END();
}
