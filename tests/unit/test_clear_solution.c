/**
 * @file test_clear_solution.c
 * @brief Tests for cxf_clear_solution (state_cleanup_buffers.md)
 *
 * Verifies:
 * - NULL model handled gracefully (no crash, returns CXF_OK)
 * - Fresh model clear (no crash, status reset)
 * - Clear after setting solution data: arrays zeroed, status reset
 * - clearHints=1 frees extended data and resets fingerprint
 */

#include "unity.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_types.h"
#include <stddef.h>

/* Forward declarations for functions not in public headers */
int cxf_addconstr(CxfModel *model, int numnz, const int *cind,
                  const double *cval, char sense, double rhs,
                  const char *constrname);
void *cxf_malloc(size_t size);

static CxfEnv *env = NULL;

void setUp(void) {
    cxf_loadenv(&env, NULL);
}

void tearDown(void) {
    cxf_freeenv(env);
    env = NULL;
}

/**
 * NULL model returns CXF_OK without crashing.
 */
void test_clear_solution_null_model(void) {
    int rc = cxf_clear_solution(NULL, 0);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
}

/**
 * Clearing a fresh model (no solve performed) does not crash.
 * Status is set to CXF_LOADED and obj_val is 0.
 */
void test_clear_solution_fresh_model(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "fresh", 2, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_NOT_NULL(model);

    int rc = cxf_clear_solution(model, 0);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_EQUAL_INT(CXF_LOADED, model->status);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 0.0, model->obj_val);
    TEST_ASSERT_EQUAL_INT(1, model->initialized);

    cxf_freemodel(model);
}

/**
 * After injecting solution data, clear zeros the arrays and
 * resets status/obj_val.
 */
void test_clear_solution_after_data(void) {
    CxfModel *model = NULL;
    double obj[] = {1.0, 2.0};
    double lb[]  = {0.0, 0.0};
    double ub[]  = {10.0, 10.0};
    cxf_newmodel(env, &model, "data", 2, obj, lb, ub, NULL, NULL);
    TEST_ASSERT_NOT_NULL(model);

    /* Simulate solution data from a solve */
    model->solution[0] = 5.0;
    model->solution[1] = 3.0;
    model->status = CXF_OPTIMAL;
    model->obj_val = 11.0;

    int rc = cxf_clear_solution(model, 0);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    /* Primal solution must be zeroed */
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 0.0, model->solution[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 0.0, model->solution[1]);

    /* Status and obj_val must be reset */
    TEST_ASSERT_EQUAL_INT(CXF_LOADED, model->status);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 0.0, model->obj_val);

    cxf_freemodel(model);
}

/**
 * When clearHints=1, extended data pointers are freed and
 * fingerprint is cleared.
 */
void test_clear_solution_with_hints(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "hints", 1, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_NOT_NULL(model);

    /* Set a fingerprint to verify it gets cleared */
    model->fingerprint = 0xDEADBEEF;
    model->status = CXF_OPTIMAL;
    model->obj_val = 42.0;

    int rc = cxf_clear_solution(model, 1);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    /* Fingerprint must be cleared */
    TEST_ASSERT_EQUAL_UINT32(0, model->fingerprint);

    /* Extended data pointers must be NULL */
    TEST_ASSERT_NULL(model->solution_data);
    TEST_ASSERT_NULL(model->sos_data);
    TEST_ASSERT_NULL(model->gen_constr_data);

    /* Status and obj must still be reset */
    TEST_ASSERT_EQUAL_INT(CXF_LOADED, model->status);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 0.0, model->obj_val);

    cxf_freemodel(model);
}

/**
 * clearHints=0 does NOT clear extended data or fingerprint.
 */
void test_clear_solution_without_hints_preserves_extended(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "nohints", 1, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_NOT_NULL(model);

    model->fingerprint = 0xCAFEBABE;
    model->status = CXF_OPTIMAL;

    int rc = cxf_clear_solution(model, 0);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    /* Fingerprint must be preserved when clearHints=0 */
    TEST_ASSERT_EQUAL_UINT32(0xCAFEBABE, model->fingerprint);

    /* Status still cleared */
    TEST_ASSERT_EQUAL_INT(CXF_LOADED, model->status);

    cxf_freemodel(model);
}

/**
 * Dual (pi) array is zeroed when it exists.
 */
void test_clear_solution_zeros_dual_values(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "dual", 1, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_NOT_NULL(model);

    /* Add a constraint so num_constrs > 0, then allocate pi */
    int idx = 0;
    double val = 1.0;
    double rhs = 5.0;
    cxf_addconstr(model, 1, &idx, &val, '<', rhs, "c1");

    /* Manually allocate and set pi (normally done by extract_solution) */
    if (model->pi == NULL) {
        model->pi = (double *)cxf_malloc(
            (size_t)model->num_constrs * sizeof(double));
    }
    TEST_ASSERT_NOT_NULL(model->pi);
    model->pi[0] = 99.0;

    int rc = cxf_clear_solution(model, 0);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    /* Dual value must be zeroed */
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 0.0, model->pi[0]);

    cxf_freemodel(model);
}

/*******************************************************************************
 * Main
 ******************************************************************************/

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_clear_solution_null_model);
    RUN_TEST(test_clear_solution_fresh_model);
    RUN_TEST(test_clear_solution_after_data);
    RUN_TEST(test_clear_solution_with_hints);
    RUN_TEST(test_clear_solution_without_hints_preserves_extended);
    RUN_TEST(test_clear_solution_zeros_dual_values);

    return UNITY_END();
}
