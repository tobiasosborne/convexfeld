/**
 * @file test_modification_blocked.c
 * @brief Tests for modification_blocked flag during optimization.
 *
 * Validates that cxf_optimize sets modification_blocked at the start
 * and clears it on all exit paths, per solve_entry.md Steps 4 and 12
 * and model.md Lifecycle/Optimization Steps 2 and 5.
 */

#include "unity.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_callback.h"

/* Forward declaration */
int cxf_addconstr(CxfModel *model, int numnz, const int *cind,
                  const double *cval, char sense, double rhs,
                  const char *constrname);

/* Test fixture */
static CxfEnv *env = NULL;

void setUp(void) {
    cxf_loadenv(&env, NULL);
}

void tearDown(void) {
    cxf_freeenv(env);
    env = NULL;
}

/*******************************************************************************
 * Flag cleared after successful optimization
 ******************************************************************************/

void test_blocked_cleared_after_optimize_success(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");

    /* Before optimize: not blocked */
    TEST_ASSERT_EQUAL_INT(0, model->modification_blocked);

    int status = cxf_optimize(model);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);

    /* After optimize: must be cleared */
    TEST_ASSERT_EQUAL_INT(0, model->modification_blocked);

    cxf_freemodel(model);
}

void test_blocked_cleared_after_optimize_empty_model(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);

    int status = cxf_optimize(model);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);

    /* Flag must be cleared regardless of model content */
    TEST_ASSERT_EQUAL_INT(0, model->modification_blocked);

    cxf_freemodel(model);
}

/*******************************************************************************
 * Flag set during optimization (callback-based verification)
 ******************************************************************************/

/* State for callback that captures modification_blocked */
static int cb_blocked_value = -1;
static int cb_call_count = 0;

static int capture_blocked_callback(CxfModel *model, void *cbdata,
                                    int where, void *usrdata) {
    (void)cbdata;
    (void)where;
    (void)usrdata;
    cb_blocked_value = model->modification_blocked;
    cb_call_count++;
    return 0;
}

void test_blocked_set_during_optimize_via_callback(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");

    /* Register callback to observe modification_blocked during solve */
    cxf_setcallbackfunc(model, capture_blocked_callback, NULL);
    cb_blocked_value = -1;
    cb_call_count = 0;

    cxf_optimize(model);

    /* Callback should have observed modification_blocked == 1 */
    if (cb_call_count > 0) {
        TEST_ASSERT_EQUAL_INT(1, cb_blocked_value);
    }

    /* After optimize completes, flag must be cleared */
    TEST_ASSERT_EQUAL_INT(0, model->modification_blocked);

    cxf_freemodel(model);
}

/*******************************************************************************
 * Addconstr blocked during optimization (indirect test)
 ******************************************************************************/

void test_addconstr_blocked_when_flag_set(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");

    /* Manually set flag (simulating mid-optimization state) */
    model->modification_blocked = 1;

    int cind[] = {0};
    double cval[] = {1.0};
    int status = cxf_addconstr(model, 1, cind, cval, '<', 5.0, "c1");
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, status);

    /* Clean up */
    model->modification_blocked = 0;
    cxf_freemodel(model);
}

void test_addconstr_allowed_after_optimize_completes(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");

    /* Optimize first */
    cxf_optimize(model);

    /* After optimize, flag should be cleared so addconstr works */
    int cind[] = {0};
    double cval[] = {1.0};
    int status = cxf_addconstr(model, 1, cind, cval, '<', 5.0, "c1");
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);

    cxf_freemodel(model);
}

/*******************************************************************************
 * cxf_model_is_blocked reflects flag state
 ******************************************************************************/

void test_model_is_blocked_cleared_after_optimize(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");

    cxf_optimize(model);

    /* cxf_model_is_blocked must return 0 after optimization */
    TEST_ASSERT_EQUAL_INT(0, cxf_model_is_blocked(model));

    cxf_freemodel(model);
}

/*******************************************************************************
 * Main
 ******************************************************************************/

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_blocked_cleared_after_optimize_success);
    RUN_TEST(test_blocked_cleared_after_optimize_empty_model);
    RUN_TEST(test_blocked_set_during_optimize_via_callback);
    RUN_TEST(test_addconstr_blocked_when_flag_set);
    RUN_TEST(test_addconstr_allowed_after_optimize_completes);
    RUN_TEST(test_model_is_blocked_cleared_after_optimize);

    return UNITY_END();
}
