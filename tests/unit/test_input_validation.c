/**
 * @file test_input_validation.c
 * @brief Tests for NaN/Inf rejection at API entry points.
 *
 * V2 spec (numerical_stability.md Section C): "On input data during model
 * loading, to reject models with NaN or Inf coefficients."
 *
 * Covers: cxf_addvar, cxf_addvars, cxf_addconstr, cxf_addconstrs, cxf_chgcoeffs
 */

#include <math.h>
#include "unity.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_types.h"

/* Forward declarations */
int cxf_addvars(CxfModel *model, int numvars, int numnz,
                const int *vbeg, const int *vind, const double *vval,
                const double *obj, const double *lb, const double *ub,
                const char *vtype, const char **varnames);
int cxf_addconstr(CxfModel *model, int numnz, const int *cind,
                  const double *cval, char sense, double rhs,
                  const char *constrname);
int cxf_addconstrs(CxfModel *model, int numconstrs, int numnz,
                   const int *cbeg, const int *cind, const double *cval,
                   const char *sense, const double *rhs,
                   const char **constrnames);
int cxf_chgcoeffs(CxfModel *model, int cnt, const int *cind,
                  const int *vind, const double *val);

static CxfEnv *env = NULL;
static CxfModel *model = NULL;

void setUp(void) {
    cxf_loadenv(&env, NULL);
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    /* Pre-add 2 vars so constraint APIs have valid column indices */
    double obj2[] = {1.0, 2.0};
    double lb2[] = {0.0, 0.0};
    double ub2[] = {10.0, 20.0};
    cxf_addvars(model, 2, 0, NULL, NULL, NULL, obj2, lb2, ub2, NULL, NULL);
}

void tearDown(void) {
    cxf_freemodel(model);
    model = NULL;
    cxf_freeenv(env);
    env = NULL;
}

/*=== cxf_addvar NaN/Inf tests ============================================*/

void test_addvar_nan_obj_rejected(void) {
    int s = cxf_addvar(model, 0, NULL, NULL, NAN, 0.0, 1.0, 'C', NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, s);
}

void test_addvar_inf_obj_rejected(void) {
    int s = cxf_addvar(model, 0, NULL, NULL, INFINITY, 0.0, 1.0, 'C', NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, s);
}

void test_addvar_nan_lb_rejected(void) {
    int s = cxf_addvar(model, 0, NULL, NULL, 1.0, NAN, 1.0, 'C', NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, s);
}

void test_addvar_inf_ub_rejected(void) {
    int s = cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, INFINITY, 'C', NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, s);
}

void test_addvar_neg_inf_lb_rejected(void) {
    int s = cxf_addvar(model, 0, NULL, NULL, 1.0, -INFINITY, 1.0, 'C', NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, s);
}

void test_addvar_finite_bounds_accepted(void) {
    /* CXF_INFINITY (1e100) is finite per IEEE 754 -- must be accepted */
    int s = cxf_addvar(model, 0, NULL, NULL, 1.0, -CXF_INFINITY, CXF_INFINITY,
                       'C', NULL);
    TEST_ASSERT_EQUAL_INT(CXF_OK, s);
}

/*=== cxf_addvars NaN/Inf tests ===========================================*/

void test_addvars_nan_obj_rejected(void) {
    double obj[] = {1.0, NAN};
    double lb[] = {0.0, 0.0};
    double ub[] = {1.0, 1.0};
    int s = cxf_addvars(model, 2, 0, NULL, NULL, NULL, obj, lb, ub, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, s);
}

void test_addvars_inf_lb_rejected(void) {
    double obj[] = {1.0, 2.0};
    double lb[] = {0.0, -INFINITY};
    double ub[] = {1.0, 1.0};
    int s = cxf_addvars(model, 2, 0, NULL, NULL, NULL, obj, lb, ub, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, s);
}

void test_addvars_nan_ub_rejected(void) {
    double obj[] = {1.0};
    double lb[] = {0.0};
    double ub[] = {NAN};
    int s = cxf_addvars(model, 1, 0, NULL, NULL, NULL, obj, lb, ub, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, s);
}

void test_addvars_clean_data_accepted(void) {
    double obj[] = {1.0, 2.0};
    double lb[] = {0.0, 0.0};
    double ub[] = {10.0, 20.0};
    int s = cxf_addvars(model, 2, 0, NULL, NULL, NULL, obj, lb, ub, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_OK, s);
}

/*=== cxf_addconstr NaN/Inf tests =========================================*/

void test_addconstr_nan_rhs_rejected(void) {
    int cind[] = {0};
    double cval[] = {1.0};
    int s = cxf_addconstr(model, 1, cind, cval, '<', NAN, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, s);
}

void test_addconstr_inf_rhs_rejected(void) {
    int cind[] = {0};
    double cval[] = {1.0};
    int s = cxf_addconstr(model, 1, cind, cval, '<', INFINITY, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, s);
}

void test_addconstr_nan_coeff_rejected(void) {
    int cind[] = {0};
    double cval[] = {NAN};
    int s = cxf_addconstr(model, 1, cind, cval, '<', 5.0, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, s);
}

void test_addconstr_inf_coeff_rejected(void) {
    int cind[] = {0};
    double cval[] = {INFINITY};
    int s = cxf_addconstr(model, 1, cind, cval, '=', 5.0, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, s);
}

void test_addconstr_clean_data_accepted(void) {
    int cind[] = {0, 1};
    double cval[] = {1.0, 2.0};
    int s = cxf_addconstr(model, 2, cind, cval, '<', 5.0, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_OK, s);
}

/*=== cxf_addconstrs NaN/Inf tests ========================================*/

void test_addconstrs_nan_rhs_rejected(void) {
    int cbeg[] = {0, 1};
    int cind[] = {0, 1};
    double cval[] = {1.0, 2.0};
    char sense[] = {'<', '<'};
    double rhs[] = {5.0, NAN};
    int s = cxf_addconstrs(model, 2, 2, cbeg, cind, cval, sense, rhs, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, s);
}

void test_addconstrs_inf_coeff_rejected(void) {
    int cbeg[] = {0};
    int cind[] = {0};
    double cval[] = {-INFINITY};
    char sense[] = {'='};
    double rhs[] = {1.0};
    int s = cxf_addconstrs(model, 1, 1, cbeg, cind, cval, sense, rhs, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, s);
}

/*=== cxf_chgcoeffs NaN/Inf tests =========================================*/

void test_chgcoeffs_nan_val_rejected(void) {
    /* Add a constraint first so we have valid indices */
    int ci[] = {0};
    double cv[] = {1.0};
    cxf_addconstr(model, 1, ci, cv, '<', 5.0, NULL);

    int cind[] = {0};
    int vind[] = {0};
    double val[] = {NAN};
    int s = cxf_chgcoeffs(model, 1, cind, vind, val);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, s);
}

void test_chgcoeffs_inf_val_rejected(void) {
    int ci[] = {0};
    double cv[] = {1.0};
    cxf_addconstr(model, 1, ci, cv, '<', 5.0, NULL);

    int cind[] = {0};
    int vind[] = {0};
    double val[] = {INFINITY};
    int s = cxf_chgcoeffs(model, 1, cind, vind, val);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, s);
}

/*=== No model corruption on rejection ====================================*/

void test_addvar_nan_does_not_modify_model(void) {
    int before = model->num_vars;
    cxf_addvar(model, 0, NULL, NULL, NAN, 0.0, 1.0, 'C', NULL);
    TEST_ASSERT_EQUAL_INT(before, model->num_vars);
}

void test_addvars_nan_does_not_modify_model(void) {
    int before = model->num_vars;
    double obj[] = {NAN};
    cxf_addvars(model, 1, 0, NULL, NULL, NULL, obj, NULL, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(before, model->num_vars);
}

/*=== Main ================================================================*/

int main(void) {
    UNITY_BEGIN();

    /* cxf_addvar */
    RUN_TEST(test_addvar_nan_obj_rejected);
    RUN_TEST(test_addvar_inf_obj_rejected);
    RUN_TEST(test_addvar_nan_lb_rejected);
    RUN_TEST(test_addvar_inf_ub_rejected);
    RUN_TEST(test_addvar_neg_inf_lb_rejected);
    RUN_TEST(test_addvar_finite_bounds_accepted);

    /* cxf_addvars */
    RUN_TEST(test_addvars_nan_obj_rejected);
    RUN_TEST(test_addvars_inf_lb_rejected);
    RUN_TEST(test_addvars_nan_ub_rejected);
    RUN_TEST(test_addvars_clean_data_accepted);

    /* cxf_addconstr */
    RUN_TEST(test_addconstr_nan_rhs_rejected);
    RUN_TEST(test_addconstr_inf_rhs_rejected);
    RUN_TEST(test_addconstr_nan_coeff_rejected);
    RUN_TEST(test_addconstr_inf_coeff_rejected);
    RUN_TEST(test_addconstr_clean_data_accepted);

    /* cxf_addconstrs */
    RUN_TEST(test_addconstrs_nan_rhs_rejected);
    RUN_TEST(test_addconstrs_inf_coeff_rejected);

    /* cxf_chgcoeffs */
    RUN_TEST(test_chgcoeffs_nan_val_rejected);
    RUN_TEST(test_chgcoeffs_inf_val_rejected);

    /* No model corruption */
    RUN_TEST(test_addvar_nan_does_not_modify_model);
    RUN_TEST(test_addvars_nan_does_not_modify_model);

    return UNITY_END();
}
