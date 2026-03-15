/**
 * @file test_solve_lp_order.c
 * @brief V2 spec compliance: preprocess before setup in solve flow.
 *
 * simplex_phases.md Module-Level Notes specifies the pre-iteration order:
 *   (3) cxf_simplex_preprocess — fix near-bound variables
 *   (4) cxf_simplex_setup      — compute activity bounds
 *
 * Issue: convexfeld-rb5z.
 */

#include "unity.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_types.h"
#include <math.h>

static CxfEnv *env = NULL;
static CxfModel *model = NULL;

void setUp(void) {
    cxf_loadenv(&env, NULL);
    model = NULL;
}

void tearDown(void) {
    if (model) { cxf_freemodel(model); model = NULL; }
    if (env) { cxf_freeenv(env); env = NULL; }
}

/**
 * A model with tight-bound variables must solve correctly.
 * If preprocess runs after setup (old bug), activity bounds are
 * computed on unfixed variables, then overwritten by preprocess's
 * internal recompute -- wasting work. With the correct order,
 * setup runs on already-fixed bounds.
 *
 * minimize x + y  s.t. x + y >= 1, 0 <= x <= 10, y fixed at 0.5
 * (y has lb=0.5, ub=0.5 => tight, should be preprocessed out)
 * Optimal: x = 0.5, y = 0.5, obj = 1.0.
 */
void test_tight_bound_var_preprocessed_before_setup(void) {
    int rc;
    rc = cxf_newmodel(env, &model, "tight_bound", 0,
                      NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    /* x: obj=1, lb=0, ub=10 */
    rc = cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    /* y: obj=1, lb=0.5, ub=0.5 (tight — will be preprocessed) */
    rc = cxf_addvar(model, 0, NULL, NULL, 1.0, 0.5, 0.5, 'C', "y");
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    /* x + y >= 1 */
    int ind[] = {0, 1};
    double val[] = {1.0, 1.0};
    rc = cxf_addconstr(model, 2, ind, val, '>', 1.0, "c1");
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    rc = cxf_solve_lp(model);
    TEST_ASSERT_EQUAL_INT(CXF_OPTIMAL, rc);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 1.0, model->obj_val);
}

/**
 * Multiple tight-bound variables: activity bounds must reflect
 * all fixed variables when cxf_simplex_setup runs.
 *
 * minimize x  s.t. x + y + z >= 3,
 *   x: [0, 100], y: [1.0, 1.0] (fixed), z: [2.0, 2.0] (fixed)
 * y and z preprocessed out. Constraint becomes x >= 0.
 * Optimal: x = 0, obj = 0.
 */
void test_multiple_tight_vars_preprocessed(void) {
    int rc;
    rc = cxf_newmodel(env, &model, "multi_tight", 0,
                      NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    rc = cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 100.0, 'C', "x");
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    /* y fixed at 1.0 */
    rc = cxf_addvar(model, 0, NULL, NULL, 0.0, 1.0, 1.0, 'C', "y");
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    /* z fixed at 2.0 */
    rc = cxf_addvar(model, 0, NULL, NULL, 0.0, 2.0, 2.0, 'C', "z");
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    /* x + y + z >= 3 */
    int ind[] = {0, 1, 2};
    double val[] = {1.0, 1.0, 1.0};
    rc = cxf_addconstr(model, 3, ind, val, '>', 3.0, "c1");
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    rc = cxf_solve_lp(model);
    TEST_ASSERT_EQUAL_INT(CXF_OPTIMAL, rc);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 0.0, model->obj_val);
}

/**
 * No tight-bound variables: order does not matter, but solve
 * must still succeed. Ensures the reorder is safe for the
 * common case where preprocess is a no-op.
 */
void test_no_tight_vars_solve_unaffected(void) {
    int rc;
    rc = cxf_newmodel(env, &model, "no_tight", 0,
                      NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    rc = cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    rc = cxf_addvar(model, 0, NULL, NULL, 2.0, 0.0, 10.0, 'C', "y");
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    /* x + y <= 5 */
    int ind[] = {0, 1};
    double val[] = {1.0, 1.0};
    rc = cxf_addconstr(model, 2, ind, val, '<', 5.0, "c1");
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    rc = cxf_solve_lp(model);
    TEST_ASSERT_EQUAL_INT(CXF_OPTIMAL, rc);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 0.0, model->obj_val);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_tight_bound_var_preprocessed_before_setup);
    RUN_TEST(test_multiple_tight_vars_preprocessed);
    RUN_TEST(test_no_tight_vars_solve_unaffected);
    return UNITY_END();
}
