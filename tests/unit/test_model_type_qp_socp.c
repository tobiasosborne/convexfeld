/**
 * @file test_model_type_qp_socp.c
 * @brief Tests for V2-compliant cxf_is_quadratic and cxf_is_socp
 *
 * Per model_type_checking.md:
 * - cxf_is_quadratic: gen_constr_data present + no disqualifiers -> 1
 * - cxf_is_socp: any SOCP presence indicator -> 1
 */

#include "unity.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"

/* Functions under test */
int cxf_is_quadratic(CxfModel *model);
int cxf_is_socp(CxfModel *model);

/* API functions for setup */
int cxf_loadenv(CxfEnv **envP, const char *logfilename);
int cxf_freeenv(CxfEnv *env);
int cxf_newmodel(CxfEnv *env, CxfModel **modelP, const char *name,
                 int numvars, double *obj, double *lb, double *ub,
                 char *vtype, char **varnames);
void cxf_freemodel(CxfModel *model);
int cxf_addvar(CxfModel *model, int numnz, int *vind, double *vval,
               double obj, double lb, double ub, char vtype,
               const char *varname);

static CxfEnv *env = NULL;

void setUp(void) { cxf_loadenv(&env, NULL); }
void tearDown(void) { cxf_freeenv(env); env = NULL; }

static CxfModel *make_model(void) {
    CxfModel *m = NULL;
    cxf_newmodel(env, &m, "qp_socp", 0, NULL, NULL, NULL, NULL, NULL);
    return m;
}

/*==========================================================================
 * cxf_is_quadratic: NULL safety
 *=========================================================================*/

void test_quadratic_null_returns_zero(void) {
    TEST_ASSERT_EQUAL_INT(0, cxf_is_quadratic(NULL));
}

/*==========================================================================
 * cxf_is_quadratic: no gen_constr_data -> 0 (pure LP)
 *=========================================================================*/

void test_quadratic_no_gen_constr_returns_zero(void) {
    CxfModel *m = make_model();
    TEST_ASSERT_NOT_NULL(m);
    cxf_addvar(m, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");
    TEST_ASSERT_NULL(m->gen_constr_data);
    TEST_ASSERT_EQUAL_INT(0, cxf_is_quadratic(m));
    cxf_freemodel(m);
}

/*==========================================================================
 * cxf_is_quadratic: gen_constr_data present, all continuous -> 1
 *=========================================================================*/

void test_quadratic_gen_constr_continuous_returns_one(void) {
    CxfModel *m = make_model();
    TEST_ASSERT_NOT_NULL(m);
    cxf_addvar(m, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");
    int dummy = 1;
    m->gen_constr_data = &dummy;
    TEST_ASSERT_EQUAL_INT(1, cxf_is_quadratic(m));
    m->gen_constr_data = NULL;
    cxf_freemodel(m);
}

/*==========================================================================
 * cxf_is_quadratic: disqualifier - binary variable
 *=========================================================================*/

void test_quadratic_binary_disqualifies(void) {
    CxfModel *m = make_model();
    TEST_ASSERT_NOT_NULL(m);
    cxf_addvar(m, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");
    cxf_addvar(m, 0, NULL, NULL, 2.0, 0.0, 1.0, 'B', "b");
    int dummy = 1;
    m->gen_constr_data = &dummy;
    TEST_ASSERT_EQUAL_INT(0, cxf_is_quadratic(m));
    m->gen_constr_data = NULL;
    cxf_freemodel(m);
}

/*==========================================================================
 * cxf_is_quadratic: disqualifier - semi-continuous variable
 *=========================================================================*/

void test_quadratic_semicont_disqualifies(void) {
    CxfModel *m = make_model();
    TEST_ASSERT_NOT_NULL(m);
    cxf_addvar(m, 0, NULL, NULL, 1.0, 5.0, 100.0, 'S', "s");
    int dummy = 1;
    m->gen_constr_data = &dummy;
    TEST_ASSERT_EQUAL_INT(0, cxf_is_quadratic(m));
    m->gen_constr_data = NULL;
    cxf_freemodel(m);
}

/*==========================================================================
 * cxf_is_quadratic: disqualifier - semi-integer variable
 *=========================================================================*/

void test_quadratic_semiint_disqualifies(void) {
    CxfModel *m = make_model();
    TEST_ASSERT_NOT_NULL(m);
    cxf_addvar(m, 0, NULL, NULL, 1.0, 5.0, 100.0, 'N', "n");
    int dummy = 1;
    m->gen_constr_data = &dummy;
    TEST_ASSERT_EQUAL_INT(0, cxf_is_quadratic(m));
    m->gen_constr_data = NULL;
    cxf_freemodel(m);
}

/*==========================================================================
 * cxf_is_quadratic: disqualifier - SOS data present
 *=========================================================================*/

void test_quadratic_sos_data_disqualifies(void) {
    CxfModel *m = make_model();
    TEST_ASSERT_NOT_NULL(m);
    cxf_addvar(m, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");
    int dummy_gen = 1, dummy_sos = 2;
    m->gen_constr_data = &dummy_gen;
    m->sos_data = &dummy_sos;
    TEST_ASSERT_EQUAL_INT(0, cxf_is_quadratic(m));
    m->gen_constr_data = NULL;
    m->sos_data = NULL;
    cxf_freemodel(m);
}

/*==========================================================================
 * cxf_is_socp: NULL safety
 *=========================================================================*/

void test_socp_null_returns_zero(void) {
    TEST_ASSERT_EQUAL_INT(0, cxf_is_socp(NULL));
}

/*==========================================================================
 * cxf_is_socp: pure LP with no indicators -> 0
 *=========================================================================*/

void test_socp_pure_lp_returns_zero(void) {
    CxfModel *m = make_model();
    TEST_ASSERT_NOT_NULL(m);
    cxf_addvar(m, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");
    TEST_ASSERT_EQUAL_INT(0, cxf_is_socp(m));
    cxf_freemodel(m);
}

/*==========================================================================
 * cxf_is_socp: gen_constr_data -> 1
 *=========================================================================*/

void test_socp_gen_constr_triggers(void) {
    CxfModel *m = make_model();
    TEST_ASSERT_NOT_NULL(m);
    int dummy = 1;
    m->gen_constr_data = &dummy;
    TEST_ASSERT_EQUAL_INT(1, cxf_is_socp(m));
    m->gen_constr_data = NULL;
    cxf_freemodel(m);
}

/*==========================================================================
 * cxf_is_socp: sos_data -> 1
 *=========================================================================*/

void test_socp_sos_data_triggers(void) {
    CxfModel *m = make_model();
    TEST_ASSERT_NOT_NULL(m);
    int dummy = 1;
    m->sos_data = &dummy;
    TEST_ASSERT_EQUAL_INT(1, cxf_is_socp(m));
    m->sos_data = NULL;
    cxf_freemodel(m);
}

/*==========================================================================
 * cxf_is_socp: semi-continuous variable -> 1
 *=========================================================================*/

void test_socp_semicont_triggers(void) {
    CxfModel *m = make_model();
    TEST_ASSERT_NOT_NULL(m);
    cxf_addvar(m, 0, NULL, NULL, 1.0, 5.0, 100.0, 'S', "s");
    TEST_ASSERT_EQUAL_INT(1, cxf_is_socp(m));
    cxf_freemodel(m);
}

/*==========================================================================
 * cxf_is_socp: semi-integer variable -> 1
 *=========================================================================*/

void test_socp_semiint_triggers(void) {
    CxfModel *m = make_model();
    TEST_ASSERT_NOT_NULL(m);
    cxf_addvar(m, 0, NULL, NULL, 1.0, 5.0, 100.0, 'N', "n");
    TEST_ASSERT_EQUAL_INT(1, cxf_is_socp(m));
    cxf_freemodel(m);
}

/*==========================================================================
 * Main
 *=========================================================================*/

int main(void) {
    UNITY_BEGIN();

    /* cxf_is_quadratic */
    RUN_TEST(test_quadratic_null_returns_zero);
    RUN_TEST(test_quadratic_no_gen_constr_returns_zero);
    RUN_TEST(test_quadratic_gen_constr_continuous_returns_one);
    RUN_TEST(test_quadratic_binary_disqualifies);
    RUN_TEST(test_quadratic_semicont_disqualifies);
    RUN_TEST(test_quadratic_semiint_disqualifies);
    RUN_TEST(test_quadratic_sos_data_disqualifies);

    /* cxf_is_socp */
    RUN_TEST(test_socp_null_returns_zero);
    RUN_TEST(test_socp_pure_lp_returns_zero);
    RUN_TEST(test_socp_gen_constr_triggers);
    RUN_TEST(test_socp_sos_data_triggers);
    RUN_TEST(test_socp_semicont_triggers);
    RUN_TEST(test_socp_semiint_triggers);

    return UNITY_END();
}
