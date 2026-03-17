/**
 * @file test_optimize_callbacks.c
 * @brief Tests for cxf_pre_optimize_callback / cxf_post_optimize_callback
 *
 * V2 callbacks.md: these are internal lifecycle hooks that manage the
 * error buffer lock. Pre sets error_buf_locked = 1, post clears to 0.
 * Both handle NULL model gracefully.
 *
 * Issues: convexfeld-aqa5, convexfeld-trfq
 */

#include "unity.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_callback.h"
#include <string.h>

/* API functions */
int cxf_loadenv(CxfEnv **envP, const char *logfilename);
int cxf_freeenv(CxfEnv *env);
int cxf_newmodel(CxfEnv *env, CxfModel **modelP, const char *name,
                 int numvars, double *obj, double *lb, double *ub,
                 char *vtype, char **varnames);
void cxf_freemodel(CxfModel *model);

/* Functions under test */
void cxf_pre_optimize_callback(CxfModel *model);
void cxf_post_optimize_callback(CxfModel *model);

/* Test state */
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
 * cxf_pre_optimize_callback
 *=========================================================================*/

void test_pre_sets_error_buf_locked(void) {
    env->error_buf_locked = 0;
    cxf_pre_optimize_callback(model);
    TEST_ASSERT_EQUAL_INT(1, env->error_buf_locked);
}

void test_pre_null_model_no_crash(void) {
    /* NULL model must be handled gracefully (silent return) */
    cxf_pre_optimize_callback(NULL);
    TEST_PASS();
}

void test_pre_invalid_model_no_change(void) {
    /* Model with zeroed magic is invalid — should not set lock */
    CxfModel fake;
    memset(&fake, 0, sizeof(fake));
    fake.env = env;
    env->error_buf_locked = 0;
    cxf_pre_optimize_callback(&fake);
    TEST_ASSERT_EQUAL_INT(0, env->error_buf_locked);
}

/*==========================================================================
 * cxf_post_optimize_callback
 *=========================================================================*/

void test_post_clears_error_buf_locked(void) {
    env->error_buf_locked = 1;
    cxf_post_optimize_callback(model);
    TEST_ASSERT_EQUAL_INT(0, env->error_buf_locked);
}

void test_post_null_model_no_crash(void) {
    /* NULL model must be handled gracefully (silent return) */
    cxf_post_optimize_callback(NULL);
    TEST_PASS();
}

void test_post_idempotent(void) {
    /* Per spec: clearing an already-cleared lock has no ill effects */
    env->error_buf_locked = 0;
    cxf_post_optimize_callback(model);
    TEST_ASSERT_EQUAL_INT(0, env->error_buf_locked);
}

void test_post_invalid_model_no_change(void) {
    /* Model with zeroed magic is invalid — should not clear lock */
    CxfModel fake;
    memset(&fake, 0, sizeof(fake));
    fake.env = env;
    env->error_buf_locked = 1;
    cxf_post_optimize_callback(&fake);
    TEST_ASSERT_EQUAL_INT(1, env->error_buf_locked);
}

/*==========================================================================
 * Round-trip: pre then post restores original state
 *=========================================================================*/

void test_round_trip_restores_state(void) {
    env->error_buf_locked = 0;
    cxf_pre_optimize_callback(model);
    TEST_ASSERT_EQUAL_INT(1, env->error_buf_locked);
    cxf_post_optimize_callback(model);
    TEST_ASSERT_EQUAL_INT(0, env->error_buf_locked);
}

void test_round_trip_double_pre(void) {
    /* Pre is also idempotent */
    env->error_buf_locked = 0;
    cxf_pre_optimize_callback(model);
    cxf_pre_optimize_callback(model);
    TEST_ASSERT_EQUAL_INT(1, env->error_buf_locked);
    cxf_post_optimize_callback(model);
    TEST_ASSERT_EQUAL_INT(0, env->error_buf_locked);
}

/*==========================================================================
 * Main
 *=========================================================================*/

int main(void) {
    UNITY_BEGIN();

    /* Pre-optimize */
    RUN_TEST(test_pre_sets_error_buf_locked);
    RUN_TEST(test_pre_null_model_no_crash);
    RUN_TEST(test_pre_invalid_model_no_change);

    /* Post-optimize */
    RUN_TEST(test_post_clears_error_buf_locked);
    RUN_TEST(test_post_null_model_no_crash);
    RUN_TEST(test_post_idempotent);
    RUN_TEST(test_post_invalid_model_no_change);

    /* Round-trip */
    RUN_TEST(test_round_trip_restores_state);
    RUN_TEST(test_round_trip_double_pre);

    return UNITY_END();
}
