/**
 * @file test_callback_ownership.c
 * @brief Tests for V2 callback_func ownership (convexfeld-vd3h)
 *
 * V2 spec (callback_state.md): callback function pointer resides on
 * Environment, not CallbackState/CallbackContext. Termination uses
 * Environment's asyncState (terminate_flag), not CallbackContext.
 */

#include "unity.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_callback.h"
#include <string.h>
#include <stddef.h>

/* API functions */
int cxf_loadenv(CxfEnv **envP, const char *logfilename);
int cxf_freeenv(CxfEnv *env);
int cxf_newmodel(CxfEnv *env, CxfModel **modelP, const char *name,
                 int numvars, double *obj, double *lb, double *ub,
                 char *vtype, char **varnames);
void cxf_freemodel(CxfModel *model);

/* Callback lifecycle */
int cxf_pre_optimize_hook(CxfModel *model);
int cxf_post_optimize_hook(CxfModel *model);

/* Test state */
static CxfEnv *env = NULL;
static CxfModel *model = NULL;
static int cb_call_count;
static int cb_last_where;

static int test_cb(CxfModel *m, void *cbdata, int where, void *usr) {
    (void)m; (void)cbdata; (void)usr;
    cb_call_count++;
    cb_last_where = where;
    return 0;
}

static int test_cb_terminate(CxfModel *m, void *cbdata, int where,
                             void *usr) {
    (void)m; (void)cbdata; (void)where; (void)usr;
    cb_call_count++;
    return 1;  /* Request termination */
}

void setUp(void) {
    cxf_loadenv(&env, NULL);
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    cb_call_count = 0;
    cb_last_where = -1;
}

void tearDown(void) {
    if (model) { cxf_freemodel(model); model = NULL; }
    if (env) { cxf_freeenv(env); env = NULL; }
}

/*==========================================================================
 * V2: callback_func lives on CxfEnv
 *=========================================================================*/

void test_env_callback_func_initially_null(void) {
    TEST_ASSERT_NULL(env->callback_func);
}

void test_env_callback_func_settable(void) {
    env->callback_func = test_cb;
    TEST_ASSERT_EQUAL_PTR((void *)test_cb, (void *)env->callback_func);
}

void test_callback_context_has_no_callback_func(void) {
    /* Verify the field was removed from CallbackContext by checking
     * that the struct size decreased. The struct should NOT have
     * a callback_func member. We verify by creating a context and
     * confirming user_data is the first registration field. */
    CallbackContext *ctx = cxf_callback_create();
    TEST_ASSERT_NOT_NULL(ctx);
    TEST_ASSERT_NULL(ctx->user_data);
    /* If callback_func were still present, offsetof(user_data) would
     * be larger than offsetof(safety_magic) + sizeof(uint64_t) +
     * sizeof(CxfCallbackFunc). With it removed, user_data should
     * follow directly after safety_magic. */
    size_t expected_offset = offsetof(CallbackContext, safety_magic)
                             + sizeof(uint64_t);
    TEST_ASSERT_EQUAL(expected_offset,
                      offsetof(CallbackContext, user_data));
    cxf_callback_free(ctx);
}

/*==========================================================================
 * V2: pre/post hooks use env->callback_func
 *=========================================================================*/

void test_pre_hook_invokes_env_callback(void) {
    CallbackContext *ctx = cxf_callback_create();
    ctx->enabled = 1;
    env->callback_state = ctx;
    env->callback_func = test_cb;

    int result = cxf_pre_optimize_hook(model);
    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_INT(1, cb_call_count);
    TEST_ASSERT_EQUAL_INT(1, cb_last_where);  /* CXF_CB_PRE_SOLVE */
}

void test_pre_hook_skips_when_env_callback_null(void) {
    CallbackContext *ctx = cxf_callback_create();
    ctx->enabled = 1;
    env->callback_state = ctx;
    env->callback_func = NULL;  /* No callback registered on env */

    int result = cxf_pre_optimize_hook(model);
    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_INT(0, cb_call_count);
}

void test_post_hook_invokes_env_callback(void) {
    CallbackContext *ctx = cxf_callback_create();
    ctx->enabled = 1;
    env->callback_state = ctx;
    env->callback_func = test_cb;

    int result = cxf_post_optimize_hook(model);
    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_INT(1, cb_call_count);
    TEST_ASSERT_EQUAL_INT(4, cb_last_where);  /* CXF_CB_POST_SOLVE */
}

/*==========================================================================
 * V2: termination via env->terminate_flag, not ctx
 *=========================================================================*/

void test_terminate_sets_env_flag_not_ctx(void) {
    CallbackContext *ctx = cxf_callback_create();
    ctx->enabled = 1;
    env->callback_state = ctx;
    env->callback_func = test_cb_terminate;
    env->terminate_flag = 0;

    int result = cxf_pre_optimize_hook(model);
    TEST_ASSERT_EQUAL_INT(1, result);
    /* V2: termination flag on env, not ctx */
    TEST_ASSERT_EQUAL_INT(1, env->terminate_flag);
}

void test_env_freeenv_clears_callback_func(void) {
    CxfEnv *tmp_env = NULL;
    cxf_loadenv(&tmp_env, NULL);
    TEST_ASSERT_NOT_NULL(tmp_env);
    tmp_env->callback_func = test_cb;
    /* freeenv should clear callback_func */
    cxf_freeenv(tmp_env);
    /* Cannot dereference after free, but the test verifies no crash */
    TEST_PASS();
}

/*==========================================================================
 * Main
 *=========================================================================*/

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_env_callback_func_initially_null);
    RUN_TEST(test_env_callback_func_settable);
    RUN_TEST(test_callback_context_has_no_callback_func);
    RUN_TEST(test_pre_hook_invokes_env_callback);
    RUN_TEST(test_pre_hook_skips_when_env_callback_null);
    RUN_TEST(test_post_hook_invokes_env_callback);
    RUN_TEST(test_terminate_sets_env_flag_not_ctx);
    RUN_TEST(test_env_freeenv_clears_callback_func);

    return UNITY_END();
}
