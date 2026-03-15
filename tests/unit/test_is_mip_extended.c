/**
 * @file test_is_mip_extended.c
 * @brief Tests for V2-compliant cxf_is_mip_model (convexfeld-4047)
 *
 * Per model_type_checking.md, cxf_is_mip_model must check:
 * - MIP solve flag (solve_mode)
 * - Integer/binary variables (vtype 'I'/'B')
 * - Semi-continuous/semi-integer variables (vtype 'S'/'N')
 * - SOS constraint data (sos_data non-NULL)
 * - General constraint / indicator data (gen_constr_data non-NULL)
 */

#include "unity.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"
#include <string.h>

/* Function under test */
int cxf_is_mip_model(CxfModel *model);

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

/* Helper: create empty model */
static CxfModel *make_model(void) {
    CxfModel *m = NULL;
    cxf_newmodel(env, &m, "mip_test", 0, NULL, NULL, NULL, NULL, NULL);
    return m;
}

/*==========================================================================
 * V2: solve_mode flag triggers MIP
 *=========================================================================*/

void test_solve_mode_flag_triggers_mip(void) {
    CxfModel *m = make_model();
    TEST_ASSERT_NOT_NULL(m);

    /* Pure continuous with no flags -> not MIP */
    cxf_addvar(m, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");
    TEST_ASSERT_EQUAL_INT(0, cxf_is_mip_model(m));

    /* Set solve_mode -> MIP even with only continuous vars */
    m->solve_mode = 1;
    TEST_ASSERT_EQUAL_INT(1, cxf_is_mip_model(m));

    m->solve_mode = 0;
    TEST_ASSERT_EQUAL_INT(0, cxf_is_mip_model(m));

    cxf_freemodel(m);
}

/*==========================================================================
 * V2: SOS constraint data triggers MIP
 *=========================================================================*/

void test_sos_data_triggers_mip(void) {
    CxfModel *m = make_model();
    TEST_ASSERT_NOT_NULL(m);

    cxf_addvar(m, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");
    TEST_ASSERT_EQUAL_INT(0, cxf_is_mip_model(m));

    /* Non-NULL sos_data signals SOS constraints present */
    int dummy_sos = 42;
    m->sos_data = &dummy_sos;
    TEST_ASSERT_EQUAL_INT(1, cxf_is_mip_model(m));

    m->sos_data = NULL;
    TEST_ASSERT_EQUAL_INT(0, cxf_is_mip_model(m));

    cxf_freemodel(m);
}

/*==========================================================================
 * V2: General constraint (indicator) data triggers MIP
 *=========================================================================*/

void test_gen_constr_data_triggers_mip(void) {
    CxfModel *m = make_model();
    TEST_ASSERT_NOT_NULL(m);

    cxf_addvar(m, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");
    TEST_ASSERT_EQUAL_INT(0, cxf_is_mip_model(m));

    /* Non-NULL gen_constr_data signals indicator constraints */
    int dummy_gen = 99;
    m->gen_constr_data = &dummy_gen;
    TEST_ASSERT_EQUAL_INT(1, cxf_is_mip_model(m));

    m->gen_constr_data = NULL;
    TEST_ASSERT_EQUAL_INT(0, cxf_is_mip_model(m));

    cxf_freemodel(m);
}

/*==========================================================================
 * V2: Priority ordering — flags checked before vtype scan
 *=========================================================================*/

void test_flag_priority_over_vtype(void) {
    CxfModel *m = make_model();
    TEST_ASSERT_NOT_NULL(m);

    /* No vars at all, but solve_mode set -> MIP */
    m->solve_mode = 1;
    TEST_ASSERT_EQUAL_INT(1, cxf_is_mip_model(m));

    /* No vars, but sos_data set -> MIP */
    m->solve_mode = 0;
    int dummy = 1;
    m->sos_data = &dummy;
    TEST_ASSERT_EQUAL_INT(1, cxf_is_mip_model(m));

    m->sos_data = NULL;
    cxf_freemodel(m);
}

/*==========================================================================
 * Existing vtype checks still work (regression)
 *=========================================================================*/

void test_vtype_integer_still_detected(void) {
    CxfModel *m = make_model();
    TEST_ASSERT_NOT_NULL(m);

    cxf_addvar(m, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");
    cxf_addvar(m, 0, NULL, NULL, 2.0, 0.0, 100.0, 'I', "n");
    TEST_ASSERT_EQUAL_INT(1, cxf_is_mip_model(m));

    cxf_freemodel(m);
}

void test_vtype_binary_still_detected(void) {
    CxfModel *m = make_model();
    TEST_ASSERT_NOT_NULL(m);

    cxf_addvar(m, 0, NULL, NULL, 1.0, 0.0, 1.0, 'B', "b");
    TEST_ASSERT_EQUAL_INT(1, cxf_is_mip_model(m));

    cxf_freemodel(m);
}

void test_vtype_semicont_still_detected(void) {
    CxfModel *m = make_model();
    TEST_ASSERT_NOT_NULL(m);

    cxf_addvar(m, 0, NULL, NULL, 1.0, 5.0, 100.0, 'S', "s");
    TEST_ASSERT_EQUAL_INT(1, cxf_is_mip_model(m));

    cxf_freemodel(m);
}

void test_vtype_semiint_still_detected(void) {
    CxfModel *m = make_model();
    TEST_ASSERT_NOT_NULL(m);

    cxf_addvar(m, 0, NULL, NULL, 1.0, 5.0, 100.0, 'N', "n");
    TEST_ASSERT_EQUAL_INT(1, cxf_is_mip_model(m));

    cxf_freemodel(m);
}

/*==========================================================================
 * Pure continuous model stays non-MIP
 *=========================================================================*/

void test_all_continuous_not_mip(void) {
    CxfModel *m = make_model();
    TEST_ASSERT_NOT_NULL(m);

    cxf_addvar(m, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x0");
    cxf_addvar(m, 0, NULL, NULL, 2.0, 0.0, 10.0, 'C', "x1");
    cxf_addvar(m, 0, NULL, NULL, 3.0, 0.0, 10.0, 'C', "x2");

    /* All flags clear, all vars continuous */
    TEST_ASSERT_EQUAL_INT(0, cxf_is_mip_model(m));

    cxf_freemodel(m);
}

/*==========================================================================
 * Main
 *=========================================================================*/

int main(void) {
    UNITY_BEGIN();

    /* V2 flag checks */
    RUN_TEST(test_solve_mode_flag_triggers_mip);
    RUN_TEST(test_sos_data_triggers_mip);
    RUN_TEST(test_gen_constr_data_triggers_mip);
    RUN_TEST(test_flag_priority_over_vtype);

    /* Regression: existing vtype detection */
    RUN_TEST(test_vtype_integer_still_detected);
    RUN_TEST(test_vtype_binary_still_detected);
    RUN_TEST(test_vtype_semicont_still_detected);
    RUN_TEST(test_vtype_semiint_still_detected);

    /* Negative case */
    RUN_TEST(test_all_continuous_not_mip);

    return UNITY_END();
}
