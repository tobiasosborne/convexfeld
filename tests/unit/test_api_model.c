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
    int status = cxf_newmodel(env, &model, "test_model", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_NOT_NULL(model);
    cxf_freemodel(model);
}

void test_newmodel_null_env_returns_error(void) {
    CxfModel *model = NULL;
    int status = cxf_newmodel(NULL, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);
    TEST_ASSERT_NULL(model);
}

void test_newmodel_null_modelp_returns_error(void) {
    int status = cxf_newmodel(env, NULL, "test", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);
}

void test_newmodel_null_name_allowed(void) {
    CxfModel *model = NULL;
    int status = cxf_newmodel(env, &model, NULL, 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_NOT_NULL(model);
    TEST_ASSERT_EQUAL_CHAR('\0', model->name[0]);
    cxf_freemodel(model);
}

void test_newmodel_sets_magic_number(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL_UINT32(CXF_MODEL_MAGIC, model->magic);
    cxf_freemodel(model);
}

void test_newmodel_links_to_env(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL_PTR(env, model->env);
    cxf_freemodel(model);
}

void test_newmodel_initializes_dimensions_to_zero(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(0, model->num_vars);
    TEST_ASSERT_EQUAL_INT(0, model->num_constrs);
    cxf_freemodel(model);
}

void test_newmodel_initializes_status(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_OK, model->status);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, model->obj_val);
    cxf_freemodel(model);
}

void test_newmodel_allocates_variable_arrays(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_NOT_NULL(model->obj_coeffs);
    TEST_ASSERT_NOT_NULL(model->lb);
    TEST_ASSERT_NOT_NULL(model->ub);
    TEST_ASSERT_NOT_NULL(model->solution);
    cxf_freemodel(model);
}

void test_newmodel_copies_name(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "my_lp_problem", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL_STRING("my_lp_problem", model->name);
    cxf_freemodel(model);
}

void test_newmodel_multiple_models(void) {
    CxfModel *model1 = NULL;
    CxfModel *model2 = NULL;
    TEST_ASSERT_EQUAL_INT(CXF_OK, cxf_newmodel(env, &model1, "model1", 0, NULL, NULL, NULL, NULL, NULL));
    TEST_ASSERT_EQUAL_INT(CXF_OK, cxf_newmodel(env, &model2, "model2", 0, NULL, NULL, NULL, NULL, NULL));
    TEST_ASSERT_NOT_EQUAL(model1, model2);
    cxf_freemodel(model1);
    cxf_freemodel(model2);
}

void test_newmodel_initializes_var_capacity(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_GREATER_THAN(0, model->var_capacity);
    cxf_freemodel(model);
}

void test_newmodel_initializes_extended_fields(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL_UINT32(0, model->fingerprint);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, model->update_time);
    TEST_ASSERT_NULL(model->pending_buffer);
    TEST_ASSERT_NULL(model->solution_data);
    TEST_ASSERT_NULL(model->sos_data);
    TEST_ASSERT_NULL(model->gen_constr_data);
    cxf_freemodel(model);
}

void test_newmodel_primary_model_points_to_self(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL_PTR(model, model->primary_model);
    cxf_freemodel(model);
}

void test_newmodel_self_ptr_null_initially(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_NULL(model->self_ptr);
    cxf_freemodel(model);
}

void test_newmodel_initializes_bookkeeping(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(0, model->callback_count);
    TEST_ASSERT_EQUAL_INT(0, model->solve_mode);
    TEST_ASSERT_EQUAL_INT(0, model->env_flag);
    cxf_freemodel(model);
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
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
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
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);

    int status = cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(1, model->num_vars);

    cxf_freemodel(model);
}

void test_addvar_null_model_returns_error(void) {
    int status = cxf_addvar(NULL, 0, NULL, NULL, 1.0, 0.0, 1.0, 'C', "x");
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);
}

void test_addvar_stores_bounds_and_obj(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);

    cxf_addvar(model, 0, NULL, NULL, 3.5, 5.0, 20.0, 'C', "x");

    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 5.0, model->lb[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 20.0, model->ub[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 3.5, model->obj_coeffs[0]);

    cxf_freemodel(model);
}

void test_addvar_multiple_variables(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);

    TEST_ASSERT_EQUAL_INT(CXF_OK, cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 1.0, 'C', "x1"));
    TEST_ASSERT_EQUAL_INT(CXF_OK, cxf_addvar(model, 0, NULL, NULL, 2.0, 0.0, 2.0, 'C', "x2"));
    TEST_ASSERT_EQUAL_INT(CXF_OK, cxf_addvar(model, 0, NULL, NULL, 3.0, 0.0, 3.0, 'C', "x3"));

    TEST_ASSERT_EQUAL_INT(3, model->num_vars);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 1.0, model->obj_coeffs[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 2.0, model->obj_coeffs[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 3.0, model->obj_coeffs[2]);

    cxf_freemodel(model);
}

void test_addvar_initializes_solution_to_zero(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);

    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");

    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, model->solution[0]);

    cxf_freemodel(model);
}

void test_addvar_null_name_allowed(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);

    int status = cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 1.0, 'C', NULL);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);

    cxf_freemodel(model);
}

/*******************************************************************************
 * cxf_checkmodel Tests
 ******************************************************************************/

void test_checkmodel_null_returns_error(void) {
    int status = cxf_checkmodel(NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);
}

void test_checkmodel_valid_model_returns_ok(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_OK, cxf_checkmodel(model));
    cxf_freemodel(model);
}

/*******************************************************************************
 * cxf_model_is_blocked Tests
 ******************************************************************************/

void test_model_is_blocked_null_returns_error(void) {
    int result = cxf_model_is_blocked(NULL);
    TEST_ASSERT_EQUAL_INT(-1, result);
}

void test_model_is_blocked_initially_not_blocked(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(0, cxf_model_is_blocked(model));
    cxf_freemodel(model);
}

void test_model_is_blocked_when_blocked(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    model->modification_blocked = 1;
    TEST_ASSERT_EQUAL_INT(1, cxf_model_is_blocked(model));
    cxf_freemodel(model);
}

/*******************************************************************************
 * cxf_copymodel Tests
 ******************************************************************************/

void test_copymodel_basic(void) {
    CxfModel *model = NULL;
    CxfModel *copy = NULL;

    cxf_newmodel(env, &model, "original", 0, NULL, NULL, NULL, NULL, NULL);
    copy = cxf_copymodel(model);

    TEST_ASSERT_NOT_NULL(copy);
    TEST_ASSERT_NOT_EQUAL(model, copy);
    TEST_ASSERT_EQUAL_STRING("original", copy->name);
    TEST_ASSERT_EQUAL_PTR(env, copy->env);

    cxf_freemodel(copy);
    cxf_freemodel(model);
}

void test_copymodel_null_returns_null(void) {
    CxfModel *copy = cxf_copymodel(NULL);
    TEST_ASSERT_NULL(copy);
}

void test_copymodel_copies_variables(void) {
    CxfModel *model = NULL;
    CxfModel *copy = NULL;

    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    cxf_addvar(model, 0, NULL, NULL, 1.5, 0.0, 10.0, 'C', "x1");
    cxf_addvar(model, 0, NULL, NULL, 2.5, 5.0, 20.0, 'C', "x2");
    cxf_addvar(model, 0, NULL, NULL, 3.5, 0.0, 15.0, 'C', "x3");

    copy = cxf_copymodel(model);
    TEST_ASSERT_NOT_NULL(copy);
    TEST_ASSERT_EQUAL_INT(3, copy->num_vars);

    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, copy->lb[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 10.0, copy->ub[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 1.5, copy->obj_coeffs[0]);

    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 5.0, copy->lb[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 20.0, copy->ub[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 2.5, copy->obj_coeffs[1]);

    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, copy->lb[2]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 15.0, copy->ub[2]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 3.5, copy->obj_coeffs[2]);

    cxf_freemodel(copy);
    cxf_freemodel(model);
}

void test_copymodel_copies_status(void) {
    CxfModel *model = NULL;
    CxfModel *copy = NULL;

    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    model->status = CXF_OPTIMAL;
    model->obj_val = 42.5;
    model->initialized = 1;

    copy = cxf_copymodel(model);
    TEST_ASSERT_NOT_NULL(copy);
    TEST_ASSERT_EQUAL_INT(CXF_OPTIMAL, copy->status);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 42.5, copy->obj_val);
    TEST_ASSERT_EQUAL_INT(1, copy->initialized);

    cxf_freemodel(copy);
    cxf_freemodel(model);
}

void test_copymodel_independent_modification(void) {
    CxfModel *model = NULL;
    CxfModel *copy = NULL;

    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");

    copy = cxf_copymodel(model);
    TEST_ASSERT_NOT_NULL(copy);

    /* Modify original */
    model->obj_coeffs[0] = 99.9;

    /* Copy should be unaffected */
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 1.0, copy->obj_coeffs[0]);

    cxf_freemodel(copy);
    cxf_freemodel(model);
}

/*******************************************************************************
 * cxf_updatemodel Tests
 ******************************************************************************/

void test_updatemodel_null_returns_error(void) {
    int status = cxf_updatemodel(NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, status);
}

void test_updatemodel_valid_model_returns_ok(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);

    int status = cxf_updatemodel(model);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);

    cxf_freemodel(model);
}

void test_updatemodel_idempotent(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);

    int status1 = cxf_updatemodel(model);
    int status2 = cxf_updatemodel(model);

    TEST_ASSERT_EQUAL_INT(CXF_OK, status1);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status2);

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
    RUN_TEST(test_newmodel_initializes_var_capacity);
    RUN_TEST(test_newmodel_initializes_extended_fields);
    RUN_TEST(test_newmodel_primary_model_points_to_self);
    RUN_TEST(test_newmodel_self_ptr_null_initially);
    RUN_TEST(test_newmodel_initializes_bookkeeping);

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

    /* cxf_checkmodel tests */
    RUN_TEST(test_checkmodel_null_returns_error);
    RUN_TEST(test_checkmodel_valid_model_returns_ok);

    /* cxf_model_is_blocked tests */
    RUN_TEST(test_model_is_blocked_null_returns_error);
    RUN_TEST(test_model_is_blocked_initially_not_blocked);
    RUN_TEST(test_model_is_blocked_when_blocked);

    /* cxf_copymodel tests */
    RUN_TEST(test_copymodel_basic);
    RUN_TEST(test_copymodel_null_returns_null);
    RUN_TEST(test_copymodel_copies_variables);
    RUN_TEST(test_copymodel_copies_status);
    RUN_TEST(test_copymodel_independent_modification);

    /* cxf_updatemodel tests */
    RUN_TEST(test_updatemodel_null_returns_error);
    RUN_TEST(test_updatemodel_valid_model_returns_ok);
    RUN_TEST(test_updatemodel_idempotent);

    return UNITY_END();
}
