/**
 * @file test_callback_terminate_v2.c
 * @brief V2 spec compliance tests for cxf_callback_terminate.
 *
 * Tests per callbacks.md V2 spec:
 * - Returns int (0 on success, error code on failure)
 * - Traverses to root environment via master_env chain
 * - Sets termination flag on root environment
 * - Sets external terminate_flag_ptr if configured
 * - V2: termination via env->terminate_flag (not ctx->terminate_requested)
 * - Returns CXF_ERROR_NULL_ARGUMENT for NULL model or NULL env
 */

#include "unity.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_callback.h"
#include <string.h>

/* Function under test */
int cxf_callback_terminate(CxfModel *model);

/* Setup helpers */
int cxf_loadenv(CxfEnv **envP, const char *logfilename);
int cxf_freeenv(CxfEnv *env);
int cxf_newmodel(CxfEnv *env, CxfModel **modelP, const char *name,
                 int numvars, double *obj, double *lb, double *ub,
                 char *vtype, char **varnames);
void cxf_freemodel(CxfModel *model);
CallbackContext *cxf_callback_create(void);
void cxf_callback_free(CallbackContext *ctx);

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

/*=========================================================================
 * Return value tests
 *=========================================================================*/

void test_returns_zero_on_success(void) {
    int rc = cxf_callback_terminate(model);
    TEST_ASSERT_EQUAL_INT(0, rc);
}

void test_null_model_returns_error(void) {
    int rc = cxf_callback_terminate(NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, rc);
}

void test_null_env_returns_error(void) {
    CxfModel fake;
    memset(&fake, 0, sizeof(fake));
    fake.env = NULL;
    int rc = cxf_callback_terminate(&fake);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, rc);
}

/*=========================================================================
 * Root environment traversal tests
 *=========================================================================*/

void test_sets_flag_on_direct_env(void) {
    env->terminate_flag = 0;
    cxf_callback_terminate(model);
    TEST_ASSERT_EQUAL_INT(1, env->terminate_flag);
}

void test_traverses_to_root_env(void) {
    /* Create a child env that chains back to the root */
    CxfEnv *child = NULL;
    cxf_loadenv(&child, NULL);
    child->master_env = env;
    child->terminate_flag = 0;
    env->terminate_flag = 0;

    /* Point model at child env */
    CxfEnv *orig_env = model->env;
    model->env = child;

    int rc = cxf_callback_terminate(model);
    TEST_ASSERT_EQUAL_INT(0, rc);

    /* Root env should have flag set */
    TEST_ASSERT_EQUAL_INT(1, env->terminate_flag);
    /* Child env should NOT have flag set (only root gets it) */
    TEST_ASSERT_EQUAL_INT(0, child->terminate_flag);

    /* Restore and cleanup */
    model->env = orig_env;
    child->master_env = NULL;
    cxf_freeenv(child);
}

void test_traverses_two_level_chain(void) {
    /* root <- mid <- leaf, model points at leaf */
    CxfEnv *mid = NULL, *leaf = NULL;
    cxf_loadenv(&mid, NULL);
    cxf_loadenv(&leaf, NULL);
    mid->master_env = env;
    leaf->master_env = mid;
    env->terminate_flag = 0;
    mid->terminate_flag = 0;
    leaf->terminate_flag = 0;

    CxfEnv *orig = model->env;
    model->env = leaf;

    int rc = cxf_callback_terminate(model);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_INT(1, env->terminate_flag);
    TEST_ASSERT_EQUAL_INT(0, mid->terminate_flag);
    TEST_ASSERT_EQUAL_INT(0, leaf->terminate_flag);

    model->env = orig;
    leaf->master_env = NULL;
    mid->master_env = NULL;
    cxf_freeenv(leaf);
    cxf_freeenv(mid);
}

/*=========================================================================
 * External flag and callback state tests
 *=========================================================================*/

void test_sets_external_flag_ptr(void) {
    volatile int ext_flag = 0;
    env->terminate_flag_ptr = &ext_flag;
    cxf_callback_terminate(model);
    TEST_ASSERT_EQUAL_INT(1, ext_flag);
    env->terminate_flag_ptr = NULL;
}

void test_null_flag_ptr_silently_skipped(void) {
    env->terminate_flag_ptr = NULL;
    int rc = cxf_callback_terminate(model);
    TEST_ASSERT_EQUAL_INT(0, rc);
    /* No crash, no error */
}

void test_terminate_uses_env_flag_with_callback_state(void) {
    /* V2: termination goes through env->terminate_flag, not ctx */
    CallbackContext *ctx = cxf_callback_create();
    TEST_ASSERT_NOT_NULL(ctx);
    env->callback_state = ctx;
    env->terminate_flag = 0;

    cxf_callback_terminate(model);
    TEST_ASSERT_EQUAL_INT(1, env->terminate_flag);

    env->callback_state = NULL;
    cxf_callback_free(ctx);
}

void test_null_callback_state_silently_skipped(void) {
    env->callback_state = NULL;
    int rc = cxf_callback_terminate(model);
    TEST_ASSERT_EQUAL_INT(0, rc);
}

void test_idempotent_double_call(void) {
    env->terminate_flag = 0;
    cxf_callback_terminate(model);
    cxf_callback_terminate(model);
    TEST_ASSERT_EQUAL_INT(1, env->terminate_flag);
}

/*=========================================================================
 * Runner
 *=========================================================================*/

int main(void) {
    UNITY_BEGIN();

    /* Return value */
    RUN_TEST(test_returns_zero_on_success);
    RUN_TEST(test_null_model_returns_error);
    RUN_TEST(test_null_env_returns_error);

    /* Root traversal */
    RUN_TEST(test_sets_flag_on_direct_env);
    RUN_TEST(test_traverses_to_root_env);
    RUN_TEST(test_traverses_two_level_chain);

    /* External flag and callback state */
    RUN_TEST(test_sets_external_flag_ptr);
    RUN_TEST(test_null_flag_ptr_silently_skipped);
    RUN_TEST(test_terminate_uses_env_flag_with_callback_state);
    RUN_TEST(test_null_callback_state_silently_skipped);
    RUN_TEST(test_idempotent_double_call);

    return UNITY_END();
}
