/**
 * @file test_api_constrs.c
 * @brief TDD tests for constraint API functions.
 *
 * M8.1.4: API Tests - Constraints
 *
 * Tests: cxf_addconstr, cxf_addconstrs, cxf_addqconstr, cxf_addgenconstrIndicator
 */

#include "unity.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_types.h"

/* Forward declarations for functions under test */
int cxf_addconstr(CxfModel *model, int numnz, const int *cind,
                  const double *cval, char sense, double rhs,
                  const char *constrname);
int cxf_addconstrs(CxfModel *model, int numconstrs, int numnz,
                   const int *cbeg, const int *cind, const double *cval,
                   const char *sense, const double *rhs,
                   const char **constrnames);
int cxf_addqconstr(CxfModel *model, int numlnz, const int *lind,
                   const double *lval, int numqnz, const int *qrow,
                   const int *qcol, const double *qval, char sense,
                   double rhs, const char *name);
int cxf_addgenconstrIndicator(CxfModel *model, const char *name,
                              int binvar, int binval, int nvars,
                              const int *ind, const double *val,
                              char sense, double rhs);

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
 * cxf_addconstr Tests - Single Constraint Addition
 ******************************************************************************/

void test_addconstr_null_model_fails(void) {
    int cind[] = {0};
    double cval[] = {1.0};
    int status = cxf_addconstr(NULL, 1, cind, cval, '<', 10.0, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);
}

void test_addconstr_basic_le_constraint(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test");
    cxf_addvar(model, 0.0, 10.0, 1.0, 'C', "x0");
    cxf_addvar(model, 0.0, 10.0, 2.0, 'C', "x1");

    int cind[] = {0, 1};
    double cval[] = {1.0, 2.0};
    int status = cxf_addconstr(model, 2, cind, cval, '<', 20.0, "c1");
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(1, model->num_constrs);

    cxf_freemodel(model);
}

void test_addconstr_equality_constraint(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test");
    cxf_addvar(model, 0.0, 10.0, 1.0, 'C', "x0");

    int cind[] = {0};
    double cval[] = {1.0};
    int status = cxf_addconstr(model, 1, cind, cval, '=', 5.0, "eq1");
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);

    cxf_freemodel(model);
}

void test_addconstr_ge_constraint(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test");
    cxf_addvar(model, 0.0, 10.0, 1.0, 'C', "x0");

    int cind[] = {0};
    double cval[] = {1.0};
    int status = cxf_addconstr(model, 1, cind, cval, '>', 3.0, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);

    cxf_freemodel(model);
}

void test_addconstr_empty_constraint(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test");

    /* 0 <= 5 is always true */
    int status = cxf_addconstr(model, 0, NULL, NULL, '<', 5.0, "empty");
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(1, model->num_constrs);

    cxf_freemodel(model);
}

void test_addconstr_negative_rhs(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test");
    cxf_addvar(model, -10.0, 10.0, 1.0, 'C', "x0");

    int cind[] = {0};
    double cval[] = {1.0};
    int status = cxf_addconstr(model, 1, cind, cval, '>', -5.0, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);

    cxf_freemodel(model);
}

/*******************************************************************************
 * cxf_addconstrs Tests - Batch Constraint Addition
 ******************************************************************************/

void test_addconstrs_null_model_fails(void) {
    int status = cxf_addconstrs(NULL, 1, 0, NULL, NULL, NULL, "<", NULL, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);
}

void test_addconstrs_zero_count_succeeds(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test");

    int status = cxf_addconstrs(model, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(0, model->num_constrs);

    cxf_freemodel(model);
}

void test_addconstrs_basic_batch(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test");
    cxf_addvar(model, 0.0, 10.0, 1.0, 'C', "x0");
    cxf_addvar(model, 0.0, 10.0, 2.0, 'C', "x1");

    /* Two constraints: x0 + x1 <= 10, x0 - x1 >= -5 */
    int cbeg[] = {0, 2};
    int cind[] = {0, 1, 0, 1};
    double cval[] = {1.0, 1.0, 1.0, -1.0};
    char sense[] = {'<', '>'};
    double rhs[] = {10.0, -5.0};

    int status = cxf_addconstrs(model, 2, 4, cbeg, cind, cval, sense, rhs, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(2, model->num_constrs);

    cxf_freemodel(model);
}

void test_addconstrs_null_rhs_uses_zero(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test");
    cxf_addvar(model, 0.0, 10.0, 1.0, 'C', "x0");

    int cbeg[] = {0};
    int cind[] = {0};
    double cval[] = {1.0};
    char sense[] = {'<'};

    int status = cxf_addconstrs(model, 1, 1, cbeg, cind, cval, sense, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);

    cxf_freemodel(model);
}

/*******************************************************************************
 * cxf_addqconstr Tests - Quadratic Constraint (Stub)
 ******************************************************************************/

void test_addqconstr_returns_not_supported(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test");
    cxf_addvar(model, 0.0, 10.0, 1.0, 'C', "x0");

    /* x0^2 <= 100 */
    int qrow[] = {0};
    int qcol[] = {0};
    double qval[] = {1.0};

    int status = cxf_addqconstr(model, 0, NULL, NULL, 1, qrow, qcol, qval,
                                '<', 100.0, "qc1");
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NOT_SUPPORTED, status);

    cxf_freemodel(model);
}

/*******************************************************************************
 * cxf_addgenconstrIndicator Tests - Indicator Constraint (Stub)
 ******************************************************************************/

void test_addgenconstr_indicator_returns_not_supported(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test");
    cxf_addvar(model, 0.0, 1.0, 0.0, 'B', "y");  /* Binary indicator */
    cxf_addvar(model, 0.0, 10.0, 1.0, 'C', "x");

    /* y = 1 => x <= 5 */
    int ind[] = {1};
    double val[] = {1.0};

    int status = cxf_addgenconstrIndicator(model, "ind1", 0, 1, 1, ind, val,
                                            '<', 5.0);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NOT_SUPPORTED, status);

    cxf_freemodel(model);
}

/*******************************************************************************
 * Main
 ******************************************************************************/

int main(void) {
    UNITY_BEGIN();

    /* cxf_addconstr tests */
    RUN_TEST(test_addconstr_null_model_fails);
    RUN_TEST(test_addconstr_basic_le_constraint);
    RUN_TEST(test_addconstr_equality_constraint);
    RUN_TEST(test_addconstr_ge_constraint);
    RUN_TEST(test_addconstr_empty_constraint);
    RUN_TEST(test_addconstr_negative_rhs);

    /* cxf_addconstrs tests */
    RUN_TEST(test_addconstrs_null_model_fails);
    RUN_TEST(test_addconstrs_zero_count_succeeds);
    RUN_TEST(test_addconstrs_basic_batch);
    RUN_TEST(test_addconstrs_null_rhs_uses_zero);

    /* cxf_addqconstr tests */
    RUN_TEST(test_addqconstr_returns_not_supported);

    /* cxf_addgenconstrIndicator tests */
    RUN_TEST(test_addgenconstr_indicator_returns_not_supported);

    return UNITY_END();
}
