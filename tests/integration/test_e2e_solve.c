/**
 * @file test_e2e_solve.c
 * @brief End-to-end integration tests for LP solving through the public API.
 *
 * Covers optimal, equality-constrained, infeasible, and unbounded LPs,
 * plus attribute queries on the resulting model.
 */

#include "unity.h"
#include "convexfeld/convexfeld.h"

int cxf_addconstr(CxfModel *model, int numnz, const int *cind,
                  const double *cval, char sense, double rhs, const char *name);

static CxfEnv *env = NULL;
static CxfModel *model = NULL;

void setUp(void) {
    cxf_loadenv(&env, NULL);
    cxf_newmodel(env, &model, "e2e", 0, NULL, NULL, NULL, NULL, NULL);
}

void tearDown(void) {
    cxf_freemodel(model); model = NULL;
    cxf_freeenv(env);     env = NULL;
}

/* min -5x-4y  s.t. 6x+4y<=24, x+2y<=6, x,y>=0  =>  x=3, y=1.5, obj=-21 */
void test_e2e_two_var_optimal(void) {
    int s, opt;
    double objval;

    s = cxf_addvar(model, 0, NULL, NULL, -5.0, 0.0, CXF_INFINITY,
                   CXF_CONTINUOUS, "x");
    TEST_ASSERT_EQUAL_INT(CXF_OK, s);
    s = cxf_addvar(model, 0, NULL, NULL, -4.0, 0.0, CXF_INFINITY,
                   CXF_CONTINUOUS, "y");
    TEST_ASSERT_EQUAL_INT(CXF_OK, s);

    int i01[] = {0, 1};
    double v1[] = {6.0, 4.0};
    s = cxf_addconstr(model, 2, i01, v1, '<', 24.0, "c1");
    TEST_ASSERT_EQUAL_INT(CXF_OK, s);
    double v2[] = {1.0, 2.0};
    s = cxf_addconstr(model, 2, i01, v2, '<', 6.0, "c2");
    TEST_ASSERT_EQUAL_INT(CXF_OK, s);

    s = cxf_optimize(model);
    TEST_ASSERT_EQUAL_INT(CXF_OK, s);
    cxf_getintattr(model, "Status", &opt);
    TEST_ASSERT_EQUAL_INT(CXF_OPTIMAL, opt);
    cxf_getdblattr(model, "ObjVal", &objval);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, -21.0, objval);

    TEST_ASSERT_NOT_NULL(model->solution);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 3.0, model->solution[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 1.5, model->solution[1]);
}

/* min 2x+3y  s.t. x+y=10, 2x+y=14, x,y>=0  =>  x=4, y=6, obj=26 */
void test_e2e_equality_constraints(void) {
    int s, opt;
    double objval;

    s = cxf_addvar(model, 0, NULL, NULL, 2.0, 0.0, CXF_INFINITY,
                   CXF_CONTINUOUS, "x");
    TEST_ASSERT_EQUAL_INT(CXF_OK, s);
    s = cxf_addvar(model, 0, NULL, NULL, 3.0, 0.0, CXF_INFINITY,
                   CXF_CONTINUOUS, "y");
    TEST_ASSERT_EQUAL_INT(CXF_OK, s);

    int i01[] = {0, 1};
    double v1[] = {1.0, 1.0};
    s = cxf_addconstr(model, 2, i01, v1, '=', 10.0, "c1");
    TEST_ASSERT_EQUAL_INT(CXF_OK, s);
    double v2[] = {2.0, 1.0};
    s = cxf_addconstr(model, 2, i01, v2, '=', 14.0, "c2");
    TEST_ASSERT_EQUAL_INT(CXF_OK, s);

    s = cxf_optimize(model);
    TEST_ASSERT_EQUAL_INT(CXF_OK, s);
    cxf_getintattr(model, "Status", &opt);
    TEST_ASSERT_EQUAL_INT(CXF_OPTIMAL, opt);
    cxf_getdblattr(model, "ObjVal", &objval);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 26.0, objval);

    TEST_ASSERT_NOT_NULL(model->solution);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 4.0, model->solution[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 6.0, model->solution[1]);
}

/* min x  s.t. x<=1, x>=3, x>=0  =>  infeasible */
void test_e2e_infeasible(void) {
    int s, opt;

    s = cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, CXF_INFINITY,
                   CXF_CONTINUOUS, "x");
    TEST_ASSERT_EQUAL_INT(CXF_OK, s);

    int i0[] = {0};
    double v1[] = {1.0};
    s = cxf_addconstr(model, 1, i0, v1, '<', 1.0, "c1");
    TEST_ASSERT_EQUAL_INT(CXF_OK, s);
    s = cxf_addconstr(model, 1, i0, v1, '>', 3.0, "c2");
    TEST_ASSERT_EQUAL_INT(CXF_OK, s);

    /* cxf_optimize returns solver status directly for non-optimal cases */
    (void)cxf_optimize(model);
    cxf_getintattr(model, "Status", &opt);
    TEST_ASSERT_EQUAL_INT(CXF_INFEASIBLE, opt);
}

/* min -x  s.t. x-y<=0, x,y>=0  =>  unbounded (x grows with y=x) */
void test_e2e_unbounded(void) {
    int s, opt;

    s = cxf_addvar(model, 0, NULL, NULL, -1.0, 0.0, CXF_INFINITY,
                   CXF_CONTINUOUS, "x");
    TEST_ASSERT_EQUAL_INT(CXF_OK, s);
    s = cxf_addvar(model, 0, NULL, NULL, 0.0, 0.0, CXF_INFINITY,
                   CXF_CONTINUOUS, "y");
    TEST_ASSERT_EQUAL_INT(CXF_OK, s);

    int i01[] = {0, 1};
    double v[] = {1.0, -1.0};
    s = cxf_addconstr(model, 2, i01, v, '<', 0.0, "c1");
    TEST_ASSERT_EQUAL_INT(CXF_OK, s);

    (void)cxf_optimize(model);
    cxf_getintattr(model, "Status", &opt);
    TEST_ASSERT_MESSAGE(opt == CXF_UNBOUNDED || opt == CXF_INF_OR_UNBD,
                        "Expected UNBOUNDED or INF_OR_UNBD status");
}

/* Verify NumVars / NumConstrs attributes after building a model */
void test_e2e_dimension_attrs(void) {
    int s, nv, nc;

    s = cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0,
                   CXF_CONTINUOUS, "x");
    TEST_ASSERT_EQUAL_INT(CXF_OK, s);
    s = cxf_addvar(model, 0, NULL, NULL, 2.0, 0.0, 20.0,
                   CXF_CONTINUOUS, "y");
    TEST_ASSERT_EQUAL_INT(CXF_OK, s);

    int i01[] = {0, 1};
    double v[] = {1.0, 1.0};
    s = cxf_addconstr(model, 2, i01, v, '<', 15.0, "c1");
    TEST_ASSERT_EQUAL_INT(CXF_OK, s);

    cxf_getintattr(model, "NumVars", &nv);
    TEST_ASSERT_EQUAL_INT(2, nv);
    cxf_getintattr(model, "NumConstrs", &nc);
    TEST_ASSERT_EQUAL_INT(1, nc);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_e2e_two_var_optimal);
    RUN_TEST(test_e2e_equality_constraints);
    RUN_TEST(test_e2e_infeasible);
    RUN_TEST(test_e2e_unbounded);
    RUN_TEST(test_e2e_dimension_attrs);
    return UNITY_END();
}
