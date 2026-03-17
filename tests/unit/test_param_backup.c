/**
 * @file test_param_backup.c
 * @brief Tests for parameter backup/restore around cxf_optimize.
 *
 * V2 parameter_system.md Phase 5: all user-settable parameters must be
 * backed up before solver dispatch and restored afterward, so solver-internal
 * modifications never leak into the user's parameter state.
 */

#include "unity.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_callback.h"

/* Test fixture */
static CxfEnv *env = NULL;
static CxfModel *model = NULL;

void setUp(void) {
    cxf_loadenv(&env, NULL);
    cxf_newmodel(env, &model, "param_backup_test",
                 0, NULL, NULL, NULL, NULL, NULL);
    /* Add one variable so the solver actually runs */
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");
}

void tearDown(void) {
    if (model) { cxf_freemodel(model); model = NULL; }
    if (env)   { cxf_freeenv(env); env = NULL; }
}

/*******************************************************************************
 * Double parameters preserved after optimize
 ******************************************************************************/

void test_feasibility_tol_preserved(void) {
    double original = env->feasibility_tol;
    int rc = cxf_optimize(model);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_EQUAL_DOUBLE(original, env->feasibility_tol);
}

void test_optimality_tol_preserved(void) {
    double original = env->optimality_tol;
    int rc = cxf_optimize(model);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_EQUAL_DOUBLE(original, env->optimality_tol);
}

void test_infinity_preserved(void) {
    double original = env->infinity;
    int rc = cxf_optimize(model);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_EQUAL_DOUBLE(original, env->infinity);
}

/*******************************************************************************
 * Integer parameters preserved after optimize
 ******************************************************************************/

void test_output_flag_preserved(void) {
    cxf_setintparam(env, "OutputFlag", 0);
    int rc = cxf_optimize(model);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_EQUAL_INT(0, env->output_flag);
}

void test_verbosity_preserved(void) {
    cxf_setintparam(env, "Verbosity", 2);
    int rc = cxf_optimize(model);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_EQUAL_INT(2, env->verbosity);
}

void test_method_preserved(void) {
    /* Method 0 = primal simplex (the only one implemented) */
    cxf_setintparam(env, "Method", 0);
    int rc = cxf_optimize(model);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_EQUAL_INT(0, env->method);
}

void test_threads_preserved(void) {
    cxf_setintparam(env, "Threads", 4);
    int rc = cxf_optimize(model);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_EQUAL_INT(4, env->threads);
}

void test_refactor_interval_preserved(void) {
    cxf_setintparam(env, "RefactorInterval", 75);
    int rc = cxf_optimize(model);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_EQUAL_INT(75, env->refactor_interval);
}

void test_max_eta_count_preserved(void) {
    cxf_setintparam(env, "MaxEtaCount", 200);
    int rc = cxf_optimize(model);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_EQUAL_INT(200, env->max_eta_count);
}

/*******************************************************************************
 * Parameters preserved across repeated optimize calls
 ******************************************************************************/

void test_params_stable_across_two_solves(void) {
    cxf_setintparam(env, "Verbosity", 2);
    cxf_setintparam(env, "RefactorInterval", 99);
    double feas = env->feasibility_tol;
    double opt  = env->optimality_tol;

    /* First solve */
    int rc = cxf_optimize(model);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_EQUAL_INT(2, env->verbosity);
    TEST_ASSERT_EQUAL_INT(99, env->refactor_interval);
    TEST_ASSERT_EQUAL_DOUBLE(feas, env->feasibility_tol);
    TEST_ASSERT_EQUAL_DOUBLE(opt, env->optimality_tol);

    /* Second solve — same model, same env */
    rc = cxf_optimize(model);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_EQUAL_INT(2, env->verbosity);
    TEST_ASSERT_EQUAL_INT(99, env->refactor_interval);
    TEST_ASSERT_EQUAL_DOUBLE(feas, env->feasibility_tol);
    TEST_ASSERT_EQUAL_DOUBLE(opt, env->optimality_tol);
}

/*******************************************************************************
 * Main
 ******************************************************************************/

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_feasibility_tol_preserved);
    RUN_TEST(test_optimality_tol_preserved);
    RUN_TEST(test_infinity_preserved);
    RUN_TEST(test_output_flag_preserved);
    RUN_TEST(test_verbosity_preserved);
    RUN_TEST(test_method_preserved);
    RUN_TEST(test_threads_preserved);
    RUN_TEST(test_refactor_interval_preserved);
    RUN_TEST(test_max_eta_count_preserved);
    RUN_TEST(test_params_stable_across_two_solves);

    return UNITY_END();
}
