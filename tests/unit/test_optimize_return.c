/**
 * @file test_optimize_return.c
 * @brief Tests V2 return value semantics for cxf_optimize.
 *
 * Per solve_entry.md: cxf_optimize returns 0 on success, with solver
 * outcome stored on model->status. Non-zero return means error only.
 * Solver status codes (OPTIMAL, INFEASIBLE, UNBOUNDED, ITERATION_LIMIT,
 * TIME_LIMIT) are NOT errors and must not leak as return values.
 */

#include "unity.h"
#include "convexfeld/convexfeld.h"

/* cxf_addconstr is not in the public header yet */
int cxf_addconstr(CxfModel *model, int numnz, const int *cind,
                  const double *cval, char sense, double rhs,
                  const char *name);

static CxfEnv *env = NULL;
static CxfModel *model = NULL;

void setUp(void) {
    cxf_loadenv(&env, NULL);
    cxf_newmodel(env, &model, "ret_test", 0,
                 NULL, NULL, NULL, NULL, NULL);
}

void tearDown(void) {
    cxf_freemodel(model); model = NULL;
    cxf_freeenv(env);     env = NULL;
}

/* Optimal LP: return CXF_OK, model->status == CXF_OPTIMAL */
void test_optimal_returns_ok(void) {
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");

    int rc = cxf_optimize(model);
    TEST_ASSERT_EQUAL_INT_MESSAGE(CXF_OK, rc,
        "cxf_optimize must return CXF_OK for optimal LP");

    int st;
    cxf_getintattr(model, "Status", &st);
    TEST_ASSERT_EQUAL_INT(CXF_OPTIMAL, st);
}

/* Infeasible LP: return CXF_OK, model->status == CXF_INFEASIBLE */
void test_infeasible_returns_ok(void) {
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, CXF_INFINITY, 'C', "x");

    int i0[] = {0};
    double v[] = {1.0};
    cxf_addconstr(model, 1, i0, v, '<', 1.0, "c1");
    cxf_addconstr(model, 1, i0, v, '>', 3.0, "c2");

    int rc = cxf_optimize(model);
    TEST_ASSERT_EQUAL_INT_MESSAGE(CXF_OK, rc,
        "cxf_optimize must return CXF_OK for infeasible LP");

    int st;
    cxf_getintattr(model, "Status", &st);
    TEST_ASSERT_EQUAL_INT(CXF_INFEASIBLE, st);
}

/* Unbounded LP: return CXF_OK, model->status is UNBOUNDED or INF_OR_UNBD */
void test_unbounded_returns_ok(void) {
    cxf_addvar(model, 0, NULL, NULL, -1.0, 0.0, CXF_INFINITY, 'C', "x");
    cxf_addvar(model, 0, NULL, NULL, 0.0, 0.0, CXF_INFINITY, 'C', "y");

    int i01[] = {0, 1};
    double v[] = {1.0, -1.0};
    cxf_addconstr(model, 2, i01, v, '<', 0.0, "c1");

    int rc = cxf_optimize(model);
    TEST_ASSERT_EQUAL_INT_MESSAGE(CXF_OK, rc,
        "cxf_optimize must return CXF_OK for unbounded LP");

    int st;
    cxf_getintattr(model, "Status", &st);
    TEST_ASSERT_MESSAGE(st == CXF_UNBOUNDED || st == CXF_INF_OR_UNBD,
        "model->status must be UNBOUNDED or INF_OR_UNBD");
}

/* Constrained optimal LP: return CXF_OK, status OPTIMAL */
void test_constrained_optimal_returns_ok(void) {
    cxf_addvar(model, 0, NULL, NULL, -5.0, 0.0, CXF_INFINITY, 'C', "x");
    cxf_addvar(model, 0, NULL, NULL, -4.0, 0.0, CXF_INFINITY, 'C', "y");

    int i01[] = {0, 1};
    double v1[] = {6.0, 4.0};
    cxf_addconstr(model, 2, i01, v1, '<', 24.0, "c1");
    double v2[] = {1.0, 2.0};
    cxf_addconstr(model, 2, i01, v2, '<', 6.0, "c2");

    int rc = cxf_optimize(model);
    TEST_ASSERT_EQUAL_INT_MESSAGE(CXF_OK, rc,
        "cxf_optimize must return CXF_OK for constrained optimal LP");

    int st;
    cxf_getintattr(model, "Status", &st);
    TEST_ASSERT_EQUAL_INT(CXF_OPTIMAL, st);
}

/* NULL model: return error code, NOT CXF_OK */
void test_null_model_returns_error(void) {
    int rc = cxf_optimize(NULL);
    TEST_ASSERT_TRUE_MESSAGE(CXF_IS_ERROR(rc),
        "cxf_optimize(NULL) must return an error code");
    TEST_ASSERT_NOT_EQUAL(CXF_OK, rc);
}

/* Return value must not be a solver status code (1-19 range) */
void test_return_never_solver_status(void) {
    /* Infeasible LP — solver sets CXF_INFEASIBLE on model */
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, CXF_INFINITY, 'C', "x");
    int i0[] = {0};
    double v[] = {1.0};
    cxf_addconstr(model, 1, i0, v, '<', 1.0, "c1");
    cxf_addconstr(model, 1, i0, v, '>', 3.0, "c2");

    int rc = cxf_optimize(model);

    /* Return value must be either 0 (CXF_OK) or an error (10001+).
     * It must NEVER be in the solver status range (1-19). */
    TEST_ASSERT_MESSAGE(rc == CXF_OK || CXF_IS_ERROR(rc),
        "Return value must be CXF_OK or error, never solver status");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_optimal_returns_ok);
    RUN_TEST(test_infeasible_returns_ok);
    RUN_TEST(test_unbounded_returns_ok);
    RUN_TEST(test_constrained_optimal_returns_ok);
    RUN_TEST(test_null_model_returns_error);
    RUN_TEST(test_return_never_solver_status);
    return UNITY_END();
}
