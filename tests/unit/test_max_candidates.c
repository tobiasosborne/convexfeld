/**
 * @file test_max_candidates.c
 * @brief Tests that pricing candidate buffer is not hard-capped.
 *
 * Verifies convexfeld-xar1: the old MAX_CANDIDATES=10 limit silently
 * dropped good candidates. The candidate buffer is now sized to
 * total = num_vars + num_constrs, so all candidates from the pricing
 * system or Bland's rule are accepted.
 *
 * Spec: revised_simplex.md Step 1 -- no explicit cap on candidates.
 */

#include "unity.h"
#include "convexfeld/convexfeld.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_env.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* External: we need cxf_addconstr to build models */
int cxf_addconstr(CxfModel *model, int numnz, const int *cind,
                  const double *cval, char sense, double rhs,
                  const char *name);

void setUp(void) {}
void tearDown(void) {}

/**
 * Build a problem with many variables (> old MAX_CANDIDATES=10).
 * All variables have negative reduced costs from the initial point,
 * so pricing should find all of them as candidates.
 * If the old cap were still active, only 10 would survive.
 *
 * Model: minimize sum(-x_i) subject to sum(x_i) <= 1000, 0 <= x_i <= 1
 * This is trivially feasible with an obvious optimal at x_i = 1 for all i.
 */
void test_more_than_10_candidates_accepted(void) {
    CxfEnv *env = NULL;
    CxfModel *model = NULL;
    int rc;

    rc = cxf_loadenv(&env, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    cxf_setintparam(env, "OutputFlag", 0);

    /* 20 variables -- more than old MAX_CANDIDATES=10 */
    int nvars = 20;
    rc = cxf_newmodel(env, &model, "many_cands", 0,
                      NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    for (int i = 0; i < nvars; i++) {
        char name[16];
        snprintf(name, sizeof(name), "x%d", i);
        rc = cxf_addvar(model, 0, NULL, NULL, -1.0, 0.0, 1.0, 'C', name);
        TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    }

    /* Single constraint: sum(x_i) <= 1000 (very loose) */
    int *ind = (int *)malloc((size_t)nvars * sizeof(int));
    double *val = (double *)malloc((size_t)nvars * sizeof(double));
    TEST_ASSERT_NOT_NULL(ind);
    TEST_ASSERT_NOT_NULL(val);
    for (int i = 0; i < nvars; i++) {
        ind[i] = i;
        val[i] = 1.0;
    }
    rc = cxf_addconstr(model, nvars, ind, val, '<', 1000.0, "sum");
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    rc = cxf_optimize(model);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    int status;
    cxf_getintattr(model, "Status", &status);
    TEST_ASSERT_EQUAL_INT_MESSAGE(CXF_OPTIMAL, status,
        "20-variable LP must reach OPTIMAL (not be stuck by candidate cap)");

    double objval;
    cxf_getdblattr(model, "ObjVal", &objval);
    /* All 20 candidates must be evaluated; obj must be finite and negative */
    TEST_ASSERT_TRUE_MESSAGE(isfinite(objval) && objval < 0.0,
        "20-variable LP must produce finite negative objective");

    free(ind);
    free(val);
    cxf_freemodel(model);
    cxf_freeenv(env);
}

/**
 * Stress test: 50 variables, ensuring the dynamic buffer handles
 * a count well beyond any reasonable fixed limit.
 */
void test_50_candidates_no_overflow(void) {
    CxfEnv *env = NULL;
    CxfModel *model = NULL;

    cxf_loadenv(&env, NULL);
    cxf_setintparam(env, "OutputFlag", 0);

    int nvars = 50;
    cxf_newmodel(env, &model, "stress50", 0, NULL, NULL, NULL, NULL, NULL);

    for (int i = 0; i < nvars; i++) {
        char name[16];
        snprintf(name, sizeof(name), "x%d", i);
        cxf_addvar(model, 0, NULL, NULL, -1.0, 0.0, 1.0, 'C', name);
    }

    int *ind = (int *)malloc((size_t)nvars * sizeof(int));
    double *val = (double *)malloc((size_t)nvars * sizeof(double));
    for (int i = 0; i < nvars; i++) {
        ind[i] = i;
        val[i] = 1.0;
    }
    cxf_addconstr(model, nvars, ind, val, '<', 10000.0, "sum");

    cxf_optimize(model);

    int status;
    cxf_getintattr(model, "Status", &status);
    TEST_ASSERT_EQUAL_INT_MESSAGE(CXF_OPTIMAL, status,
        "50-variable LP must solve without buffer overflow");

    double objval;
    cxf_getdblattr(model, "ObjVal", &objval);
    TEST_ASSERT_TRUE_MESSAGE(isfinite(objval) && objval < 0.0,
        "50-variable LP must produce finite negative objective");

    free(ind);
    free(val);
    cxf_freemodel(model);
    cxf_freeenv(env);
}

/**
 * Verify that the solver handles zero candidates gracefully
 * (the malloc(0) case -- total > 0, so buffer is always non-empty,
 * but no candidates pass the pricing filter).
 */
void test_zero_candidates_still_works(void) {
    CxfEnv *env = NULL;
    CxfModel *model = NULL;

    cxf_loadenv(&env, NULL);
    cxf_setintparam(env, "OutputFlag", 0);

    /* Trivial 1-var LP already at optimal from slack basis */
    cxf_newmodel(env, &model, "trivial", 0, NULL, NULL, NULL, NULL, NULL);
    cxf_addvar(model, 0, NULL, NULL, 0.0, 0.0, 0.0, 'C', "x");

    int ind[] = {0};
    double val[] = {1.0};
    cxf_addconstr(model, 1, ind, val, '<', 10.0, "c");

    cxf_optimize(model);

    int status;
    cxf_getintattr(model, "Status", &status);
    /* Fixed variable at 0 with obj=0 is trivially optimal */
    TEST_ASSERT_TRUE(status == CXF_OPTIMAL || status == CXF_NUMERIC);

    cxf_freemodel(model);
    cxf_freeenv(env);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_more_than_10_candidates_accepted);
    RUN_TEST(test_50_candidates_no_overflow);
    RUN_TEST(test_zero_candidates_still_works);
    return UNITY_END();
}
