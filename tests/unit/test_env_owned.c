/**
 * @file test_env_owned.c
 * @brief Tests for Model.environment_owned field (V2 spec compliance).
 *
 * V2 spec (data-model/model.md): environment_owned is an int indicating
 * whether the model owns its Environment (private child that must be
 * freed on destruction) versus borrowing a shared Environment.
 * Values: 0 (borrowed/shared), 1 (owned/child).
 */

#include "unity.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"
#include <stddef.h>

/* API functions */
int cxf_loadenv(CxfEnv **envP, const char *logfilename);
int cxf_freeenv(CxfEnv *env);
int cxf_newmodel(CxfEnv *env, CxfModel **modelP, const char *name,
                 int numvars, double *obj, double *lb, double *ub,
                 char *vtype, char **varnames);
void cxf_freemodel(CxfModel *model);
CxfModel *cxf_copymodel(CxfModel *model);

static CxfEnv *env = NULL;
static CxfModel *model = NULL;

void setUp(void) {
    cxf_loadenv(&env, NULL);
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
}

void tearDown(void) {
    if (model) { cxf_freemodel(model); model = NULL; }
    if (env) { cxf_freeenv(env); env = NULL; }
}

/*==========================================================================
 * V2: environment_owned field exists and is correctly initialized
 *=========================================================================*/

void test_environment_owned_exists_in_struct(void) {
    /* Verify field is accessible at compile time (would fail to compile
       if the field did not exist). */
    int val = model->environment_owned;
    TEST_ASSERT_EQUAL_INT(0, val);
}

void test_environment_owned_default_zero(void) {
    /* V2 spec: default is 0 (borrowed/shared environment). */
    TEST_ASSERT_EQUAL_INT(0, model->environment_owned);
}

void test_environment_owned_settable_to_one(void) {
    /* V2 spec: 1 means model owns a private child environment. */
    model->environment_owned = 1;
    TEST_ASSERT_EQUAL_INT(1, model->environment_owned);
}

void test_environment_owned_roundtrip(void) {
    model->environment_owned = 1;
    TEST_ASSERT_EQUAL_INT(1, model->environment_owned);
    model->environment_owned = 0;
    TEST_ASSERT_EQUAL_INT(0, model->environment_owned);
}

void test_environment_owned_not_copied(void) {
    /* V2 spec: a copy borrows the same env, so environment_owned
       must be 0 even if the original owned its environment.
       Only the original model owns the child env. */
    model->environment_owned = 1;
    CxfModel *copy = cxf_copymodel(model);
    TEST_ASSERT_NOT_NULL(copy);
    TEST_ASSERT_EQUAL_INT(0, copy->environment_owned);
    cxf_freemodel(copy);
}

void test_second_model_also_zero(void) {
    /* Creating a second model on the same env should also default to 0. */
    CxfModel *m2 = NULL;
    cxf_newmodel(env, &m2, "m2", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_NOT_NULL(m2);
    TEST_ASSERT_EQUAL_INT(0, m2->environment_owned);
    cxf_freemodel(m2);
}

/*==========================================================================
 * Main
 *=========================================================================*/

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_environment_owned_exists_in_struct);
    RUN_TEST(test_environment_owned_default_zero);
    RUN_TEST(test_environment_owned_settable_to_one);
    RUN_TEST(test_environment_owned_roundtrip);
    RUN_TEST(test_environment_owned_not_copied);
    RUN_TEST(test_second_model_also_zero);

    return UNITY_END();
}
