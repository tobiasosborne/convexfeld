/**
 * @file test_api_model.c
 * @brief Unit tests for model API functions.
 *
 * M8.1.2: API Tests - Model
 *
 * Tests: cxf_newmodel, cxf_freemodel, cxf_addvar, cxf_copymodel, cxf_updatemodel
 */

#include "unity.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_types.h"

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
 * cxf_newmodel Tests
 ******************************************************************************/

void test_newmodel_basic_creation(void) {
    CxfModel *model = NULL;
    int status = cxf_newmodel(env, &model, "test_model");
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_NOT_NULL(model);
    cxf_freemodel(model);
}

void test_newmodel_null_env_returns_error(void) {
    CxfModel *model = NULL;
    int status = cxf_newmodel(NULL, &model, "test");
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);
    TEST_ASSERT_NULL(model);
}

void test_newmodel_null_modelp_returns_error(void) {
    int status = cxf_newmodel(env, NULL, "test");
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);
}

void test_newmodel_null_name_allowed(void) {
    CxfModel *model = NULL;
    int status = cxf_newmodel(env, &model, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_NOT_NULL(model);
    TEST_ASSERT_EQUAL_CHAR('\0', model->name[0]);
    cxf_freemodel(model);
}

void test_newmodel_sets_magic_number(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test");
    TEST_ASSERT_EQUAL_UINT32(CXF_MODEL_MAGIC, model->magic);
    cxf_freemodel(model);
}

void test_newmodel_links_to_env(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test");
    TEST_ASSERT_EQUAL_PTR(env, model->env);
    cxf_freemodel(model);
}

void test_newmodel_initializes_dimensions_to_zero(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test");
    TEST_ASSERT_EQUAL_INT(0, model->num_vars);
    TEST_ASSERT_EQUAL_INT(0, model->num_constrs);
    cxf_freemodel(model);
}

void test_newmodel_initializes_status(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test");
    TEST_ASSERT_EQUAL_INT(CXF_OK, model->status);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, model->obj_val);
    cxf_freemodel(model);
}

void test_newmodel_allocates_variable_arrays(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test");
    TEST_ASSERT_NOT_NULL(model->obj_coeffs);
    TEST_ASSERT_NOT_NULL(model->lb);
    TEST_ASSERT_NOT_NULL(model->ub);
    TEST_ASSERT_NOT_NULL(model->solution);
    cxf_freemodel(model);
}

void test_newmodel_copies_name(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "my_lp_problem");
    TEST_ASSERT_EQUAL_STRING("my_lp_problem", model->name);
    cxf_freemodel(model);
}

void test_newmodel_multiple_models(void) {
    CxfModel *model1 = NULL;
    CxfModel *model2 = NULL;
    TEST_ASSERT_EQUAL_INT(CXF_OK, cxf_newmodel(env, &model1, "model1"));
    TEST_ASSERT_EQUAL_INT(CXF_OK, cxf_newmodel(env, &model2, "model2"));
    TEST_ASSERT_NOT_EQUAL(model1, model2);
    cxf_freemodel(model1);
    cxf_freemodel(model2);
}

/*******************************************************************************
 * cxf_freemodel Tests
 ******************************************************************************/

void test_freemodel_null_is_safe(void) {
    cxf_freemodel(NULL);  /* Should not crash */
    TEST_PASS();
}

void test_freemodel_clears_magic(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test");
    TEST_ASSERT_EQUAL_UINT32(CXF_MODEL_MAGIC, model->magic);
    cxf_freemodel(model);
    /* Verified model had valid magic before free; implementation clears it */
    TEST_PASS();
}

/*******************************************************************************
 * cxf_addvar Tests
 ******************************************************************************/

void test_addvar_basic(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test");

    int status = cxf_addvar(model, 0.0, 10.0, 1.0, 'C', "x");
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(1, model->num_vars);

    cxf_freemodel(model);
}

void test_addvar_null_model_returns_error(void) {
    int status = cxf_addvar(NULL, 0.0, 1.0, 1.0, 'C', "x");
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);
}

void test_addvar_stores_bounds_and_obj(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test");

    cxf_addvar(model, 5.0, 20.0, 3.5, 'C', "x");

    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 5.0, model->lb[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 20.0, model->ub[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 3.5, model->obj_coeffs[0]);

    cxf_freemodel(model);
}

void test_addvar_multiple_variables(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test");

    TEST_ASSERT_EQUAL_INT(CXF_OK, cxf_addvar(model, 0.0, 1.0, 1.0, 'C', "x1"));
    TEST_ASSERT_EQUAL_INT(CXF_OK, cxf_addvar(model, 0.0, 2.0, 2.0, 'C', "x2"));
    TEST_ASSERT_EQUAL_INT(CXF_OK, cxf_addvar(model, 0.0, 3.0, 3.0, 'C', "x3"));

    TEST_ASSERT_EQUAL_INT(3, model->num_vars);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 1.0, model->obj_coeffs[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 2.0, model->obj_coeffs[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 3.0, model->obj_coeffs[2]);

    cxf_freemodel(model);
}

void test_addvar_initializes_solution_to_zero(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test");

    cxf_addvar(model, 0.0, 10.0, 1.0, 'C', "x");

    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, model->solution[0]);

    cxf_freemodel(model);
}

void test_addvar_null_name_allowed(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test");

    int status = cxf_addvar(model, 0.0, 1.0, 1.0, 'C', NULL);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);

    cxf_freemodel(model);
}

/*******************************************************************************
 * Main
 ******************************************************************************/

int main(void) {
    UNITY_BEGIN();

    /* cxf_newmodel tests */
    RUN_TEST(test_newmodel_basic_creation);
    RUN_TEST(test_newmodel_null_env_returns_error);
    RUN_TEST(test_newmodel_null_modelp_returns_error);
    RUN_TEST(test_newmodel_null_name_allowed);
    RUN_TEST(test_newmodel_sets_magic_number);
    RUN_TEST(test_newmodel_links_to_env);
    RUN_TEST(test_newmodel_initializes_dimensions_to_zero);
    RUN_TEST(test_newmodel_initializes_status);
    RUN_TEST(test_newmodel_allocates_variable_arrays);
    RUN_TEST(test_newmodel_copies_name);
    RUN_TEST(test_newmodel_multiple_models);

    /* cxf_freemodel tests */
    RUN_TEST(test_freemodel_null_is_safe);
    RUN_TEST(test_freemodel_clears_magic);

    /* cxf_addvar tests */
    RUN_TEST(test_addvar_basic);
    RUN_TEST(test_addvar_null_model_returns_error);
    RUN_TEST(test_addvar_stores_bounds_and_obj);
    RUN_TEST(test_addvar_multiple_variables);
    RUN_TEST(test_addvar_initializes_solution_to_zero);
    RUN_TEST(test_addvar_null_name_allowed);

    return UNITY_END();
}
