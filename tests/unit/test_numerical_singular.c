/**
 * @file test_numerical_singular.c
 * @brief Numerical stability tests: near-singular and ill-conditioned matrices.
 *
 * Exercises the solver with nearly parallel constraints, nearly dependent
 * rows, and ill-conditioned basis matrices. Verifies the solver either
 * returns a correct result or an appropriate status (not crashes/garbage).
 */

#include "unity.h"
#include "convexfeld/convexfeld.h"
#include <math.h>

int cxf_addconstr(CxfModel *model, int numnz, const int *cind,
                  const double *cval, char sense, double rhs, const char *name);

static CxfEnv *env = NULL;
static CxfModel *model = NULL;

void setUp(void) {
    cxf_loadenv(&env, NULL);
    cxf_setintparam(env, "OutputFlag", 0);
    cxf_newmodel(env, &model, "singular", 0, NULL, NULL, NULL, NULL, NULL);
}

void tearDown(void) {
    cxf_freemodel(model); model = NULL;
    cxf_freeenv(env);     env = NULL;
}

/* Nearly parallel constraints differing by 1e-8 in one coefficient.
 * The solver should still produce a valid result or signal NUMERIC. */
void test_nearly_parallel_constraints(void) {
    cxf_addvar(model, 0, NULL, NULL, -1.0, 0.0, CXF_INFINITY, 'C', "x");
    cxf_addvar(model, 0, NULL, NULL, -1.0, 0.0, CXF_INFINITY, 'C', "y");

    int ind[] = {0, 1};
    double v1[] = {1.0, 1.0};
    double v2[] = {1.0, 1.0 + 1e-8};
    cxf_addconstr(model, 2, ind, v1, '<', 10.0, "c1");
    cxf_addconstr(model, 2, ind, v2, '<', 10.0 + 1e-8, "c2");

    int s = cxf_optimize(model);
    int status;
    cxf_getintattr(model, "Status", &status);
    /* Must not crash — any valid status is acceptable */
    TEST_ASSERT_TRUE_MESSAGE(
        status == CXF_OPTIMAL || status == CXF_NUMERIC ||
        status == CXF_ITERATION_LIMIT || status == CXF_INFEASIBLE ||
        status == CXF_UNBOUNDED || status == CXF_INF_OR_UNBD,
        "Expected a valid solver status for near-parallel constraints");
    (void)s;
}

/* Two constraints that are exactly linearly dependent (redundant).
 * The solver should handle this gracefully. */
void test_redundant_constraints(void) {
    cxf_addvar(model, 0, NULL, NULL, -1.0, 0.0, CXF_INFINITY, 'C', "x");
    cxf_addvar(model, 0, NULL, NULL, -1.0, 0.0, CXF_INFINITY, 'C', "y");

    int ind[] = {0, 1};
    double v1[] = {1.0, 1.0};
    double v2[] = {2.0, 2.0};
    cxf_addconstr(model, 2, ind, v1, '<', 10.0, "c1");
    cxf_addconstr(model, 2, ind, v2, '<', 20.0, "c2_redundant");

    (void)cxf_optimize(model);
    int status;
    cxf_getintattr(model, "Status", &status);
    /* Redundant constraints should not prevent finding optimal */
    TEST_ASSERT_EQUAL_INT_MESSAGE(CXF_OPTIMAL, status,
        "Redundant constraints should still yield OPTIMAL");
    double objval;
    cxf_getdblattr(model, "ObjVal", &objval);
    /* x+y <= 10, 2(x+y) <= 20 are redundant; obj = -(x+y) = -10 */
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, -10.0, objval);
}

/* Near-singular 3x3 system: third row nearly a linear combination of first
 * two. Tests basis factorization robustness. */
void test_nearly_singular_3x3(void) {
    cxf_addvar(model, 0, NULL, NULL, -1.0, 0.0, CXF_INFINITY, 'C', "x");
    cxf_addvar(model, 0, NULL, NULL, -1.0, 0.0, CXF_INFINITY, 'C', "y");
    cxf_addvar(model, 0, NULL, NULL, -1.0, 0.0, CXF_INFINITY, 'C', "z");

    int ind[] = {0, 1, 2};
    double v1[] = {1.0, 0.0, 0.0};
    double v2[] = {0.0, 1.0, 0.0};
    /* Third row: nearly the sum of rows 1 and 2 */
    double v3[] = {1.0, 1.0, 1e-10};
    cxf_addconstr(model, 3, ind, v1, '<', 5.0, "r1");
    cxf_addconstr(model, 3, ind, v2, '<', 5.0, "r2");
    cxf_addconstr(model, 3, ind, v3, '<', 10.0, "r3_neardep");

    (void)cxf_optimize(model);
    int status;
    cxf_getintattr(model, "Status", &status);
    /* Near-singular systems may trigger various recovery paths */
    TEST_ASSERT_TRUE_MESSAGE(
        status == CXF_OPTIMAL || status == CXF_NUMERIC ||
        status == CXF_ITERATION_LIMIT || status == CXF_INFEASIBLE ||
        status == CXF_UNBOUNDED || status == CXF_INF_OR_UNBD,
        "Near-singular 3x3 should yield a valid solver status");
}

/* Equality constraints forming a nearly singular system.
 * x + y = 10, x + (1+eps)y = 10+eps => y ~ 1, x ~ 9.
 * Small eps makes the system ill-conditioned. */
void test_ill_conditioned_equality(void) {
    double eps = 1e-7;
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, CXF_INFINITY, 'C', "x");
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, CXF_INFINITY, 'C', "y");

    int ind[] = {0, 1};
    double v1[] = {1.0, 1.0};
    double v2[] = {1.0, 1.0 + eps};
    cxf_addconstr(model, 2, ind, v1, '=', 10.0, "eq1");
    cxf_addconstr(model, 2, ind, v2, '=', 10.0 + eps, "eq2");

    (void)cxf_optimize(model);
    int status;
    cxf_getintattr(model, "Status", &status);
    TEST_ASSERT_TRUE_MESSAGE(
        status == CXF_OPTIMAL || status == CXF_NUMERIC ||
        status == CXF_INFEASIBLE,
        "Ill-conditioned equality should yield valid status");

    if (status == CXF_OPTIMAL) {
        double objval;
        cxf_getdblattr(model, "ObjVal", &objval);
        /* y ~ 1, x ~ 9, obj ~ 10 */
        TEST_ASSERT_DOUBLE_WITHIN(1e-2, 10.0, objval);
    }
}

/* Hilbert-like 2x2 constraint matrix: notoriously ill-conditioned.
 * H = [[1, 1/2], [1/2, 1/3]], condition number ~ 19. */
void test_hilbert_like_2x2(void) {
    cxf_addvar(model, 0, NULL, NULL, -1.0, 0.0, CXF_INFINITY, 'C', "x");
    cxf_addvar(model, 0, NULL, NULL, -1.0, 0.0, CXF_INFINITY, 'C', "y");

    int ind[] = {0, 1};
    double v1[] = {1.0, 0.5};
    double v2[] = {0.5, 1.0 / 3.0};
    cxf_addconstr(model, 2, ind, v1, '<', 10.0, "h1");
    cxf_addconstr(model, 2, ind, v2, '<', 5.0, "h2");

    (void)cxf_optimize(model);
    int status;
    cxf_getintattr(model, "Status", &status);
    TEST_ASSERT_TRUE_MESSAGE(
        status == CXF_OPTIMAL || status == CXF_NUMERIC,
        "Hilbert-like system should solve or report NUMERIC");
}

/* All-zero row in constraint matrix (degenerate). */
void test_zero_row_constraint(void) {
    cxf_addvar(model, 0, NULL, NULL, -1.0, 0.0, 100.0, 'C', "x");
    cxf_addvar(model, 0, NULL, NULL, -1.0, 0.0, 100.0, 'C', "y");

    int ind[] = {0, 1};
    double v_normal[] = {1.0, 1.0};
    double v_zero[] = {0.0, 0.0};
    cxf_addconstr(model, 2, ind, v_normal, '<', 50.0, "real");
    cxf_addconstr(model, 2, ind, v_zero, '<', 10.0, "zero_row");

    (void)cxf_optimize(model);
    int status;
    cxf_getintattr(model, "Status", &status);
    /* Zero-row with non-negative RHS is always satisfied */
    TEST_ASSERT_TRUE_MESSAGE(
        status == CXF_OPTIMAL || status == CXF_NUMERIC,
        "Zero-row constraint should not cause crash");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_nearly_parallel_constraints);
    RUN_TEST(test_redundant_constraints);
    RUN_TEST(test_nearly_singular_3x3);
    RUN_TEST(test_ill_conditioned_equality);
    RUN_TEST(test_hilbert_like_2x2);
    RUN_TEST(test_zero_row_constraint);
    return UNITY_END();
}
