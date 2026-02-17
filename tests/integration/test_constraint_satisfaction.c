/**
 * @file test_constraint_satisfaction.c
 * @brief Verify constraint satisfaction in LP solutions.
 *
 * Tests that the simplex solver produces solutions satisfying all constraints,
 * not just the correct objective value. Covers mixed constraint senses
 * (<= , >=, =) and multi-constraint scenarios.
 */

#include "unity.h"
#include "convexfeld/convexfeld.h"

int cxf_addconstr(CxfModel *model, int numnz, const int *cind,
                  const double *cval, char sense, double rhs, const char *name);

static CxfEnv *env = NULL;
static CxfModel *model = NULL;

void setUp(void) {
    cxf_loadenv(&env, NULL);
    cxf_newmodel(env, &model, "constr_sat", 0, NULL, NULL, NULL, NULL, NULL);
}

void tearDown(void) {
    cxf_freemodel(model);
    model = NULL;
    cxf_freeenv(env);
    env = NULL;
}

/* ---------- helpers ---------- */

/** Compute row_i dot x using CSC matrix */
static double row_dot_x(const MatrixData *A, int row, const double *x) {
    double sum = 0.0;
    for (int col = 0; col < A->num_cols; col++) {
        for (int64_t k = A->col_ptr[col]; k < A->col_ptr[col + 1]; k++) {
            if (A->row_idx[k] == row) {
                sum += A->values[k] * x[col];
            }
        }
    }
    return sum;
}

/** Assert every constraint is satisfied within tolerance */
static void assert_constraints_satisfied(CxfModel *m, double tol) {
    const MatrixData *A = m->matrix;
    const double *x = m->solution;
    TEST_ASSERT_NOT_NULL_MESSAGE(x, "solution array is NULL");
    TEST_ASSERT_NOT_NULL_MESSAGE(A, "matrix is NULL");

    for (int i = 0; i < A->num_rows; i++) {
        double ax = row_dot_x(A, i, x);
        double rhs = A->rhs[i];
        char sense = A->sense[i];
        char msg[128];

        switch (sense) {
        case '<': /* <= */
            snprintf(msg, sizeof(msg),
                     "row %d: %.8g > %.8g (<=)", i, ax, rhs);
            TEST_ASSERT_TRUE_MESSAGE(ax <= rhs + tol, msg);
            break;
        case '>': /* >= */
            snprintf(msg, sizeof(msg),
                     "row %d: %.8g < %.8g (>=)", i, ax, rhs);
            TEST_ASSERT_TRUE_MESSAGE(ax >= rhs - tol, msg);
            break;
        case '=': /* == */
            snprintf(msg, sizeof(msg),
                     "row %d: |%.8g - %.8g| > tol (=)", i, ax, rhs);
            TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(tol, rhs, ax, msg);
            break;
        default:
            TEST_FAIL_MESSAGE("unknown constraint sense");
        }
    }
}

/** Assert variable bounds are satisfied */
static void assert_bounds_satisfied(CxfModel *m, double tol) {
    const double *x = m->solution;
    TEST_ASSERT_NOT_NULL_MESSAGE(x, "solution array is NULL");

    for (int j = 0; j < m->num_vars; j++) {
        char msg[128];
        snprintf(msg, sizeof(msg), "var %d: %.8g < lb %.8g", j, x[j], m->lb[j]);
        TEST_ASSERT_TRUE_MESSAGE(x[j] >= m->lb[j] - tol, msg);
        if (m->ub[j] < 1e20) {
            snprintf(msg, sizeof(msg), "var %d: %.8g > ub %.8g", j, x[j], m->ub[j]);
            TEST_ASSERT_TRUE_MESSAGE(x[j] <= m->ub[j] + tol, msg);
        }
    }
}

/* ---------- tests ---------- */

/**
 * Mixed senses: <= , >=, =
 *
 *   min  -x - 2y
 *   s.t. x + y <= 10    (c1)
 *        x     >= 2     (c2)
 *            y  = 5     (c3)
 *        x, y >= 0
 *
 * Optimal: x=2, y=5, obj=-12
 */
void test_mixed_senses(void) {
    int s;
    int opt;
    double objval;

    s = cxf_addvar(model, 0, NULL, NULL, -1.0, 0.0, CXF_INFINITY, CXF_CONTINUOUS, "x");
    TEST_ASSERT_EQUAL_INT(CXF_OK, s);
    s = cxf_addvar(model, 0, NULL, NULL, -2.0, 0.0, CXF_INFINITY, CXF_CONTINUOUS, "y");
    TEST_ASSERT_EQUAL_INT(CXF_OK, s);

    int i01[] = {0, 1}; double v11[] = {1.0, 1.0};
    s = cxf_addconstr(model, 2, i01, v11, '<', 10.0, "c1");
    TEST_ASSERT_EQUAL_INT(CXF_OK, s);

    int i0[] = {0}; double v1[] = {1.0};
    s = cxf_addconstr(model, 1, i0, v1, '>', 2.0, "c2");
    TEST_ASSERT_EQUAL_INT(CXF_OK, s);

    int i1[] = {1}; double vy[] = {1.0};
    s = cxf_addconstr(model, 1, i1, vy, '=', 5.0, "c3");
    TEST_ASSERT_EQUAL_INT(CXF_OK, s);

    s = cxf_optimize(model);
    TEST_ASSERT_EQUAL_INT(CXF_OK, s);
    cxf_getintattr(model, "Status", &opt);
    TEST_ASSERT_EQUAL_INT(CXF_OPTIMAL, opt);

    cxf_getdblattr(model, "ObjVal", &objval);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, -15.0, objval);

    /* solution values: x=5 (pushed to x+y=10 limit), y=5 (equality) */
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 5.0, model->solution[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 5.0, model->solution[1]);

    assert_constraints_satisfied(model, 1e-6);
    assert_bounds_satisfied(model, 1e-6);
}

/**
 * Multiple <= constraints (3 vars, 4 constraints).
 *
 *   min  -3x - 2y - z
 *   s.t. x + y + z <= 10   (c1)
 *        2x + y     <= 14  (c2)
 *             y + z <= 8   (c3)
 *        x          <= 6   (c4)
 *        x, y, z >= 0
 *
 * Optimal: x=6, y=2, z=2, obj=-26
 */
void test_multi_leq_constraints(void) {
    int s;
    int opt;
    double objval;

    s = cxf_addvar(model, 0, NULL, NULL, -3.0, 0.0, CXF_INFINITY, CXF_CONTINUOUS, "x");
    TEST_ASSERT_EQUAL_INT(CXF_OK, s);
    s = cxf_addvar(model, 0, NULL, NULL, -2.0, 0.0, CXF_INFINITY, CXF_CONTINUOUS, "y");
    TEST_ASSERT_EQUAL_INT(CXF_OK, s);
    s = cxf_addvar(model, 0, NULL, NULL, -1.0, 0.0, CXF_INFINITY, CXF_CONTINUOUS, "z");
    TEST_ASSERT_EQUAL_INT(CXF_OK, s);

    int i012[] = {0, 1, 2}; double v111[] = {1.0, 1.0, 1.0};
    s = cxf_addconstr(model, 3, i012, v111, '<', 10.0, "c1");
    TEST_ASSERT_EQUAL_INT(CXF_OK, s);

    int i01[] = {0, 1}; double v21[] = {2.0, 1.0};
    s = cxf_addconstr(model, 2, i01, v21, '<', 14.0, "c2");
    TEST_ASSERT_EQUAL_INT(CXF_OK, s);

    int i12[] = {1, 2}; double v1b[] = {1.0, 1.0};
    s = cxf_addconstr(model, 2, i12, v1b, '<', 8.0, "c3");
    TEST_ASSERT_EQUAL_INT(CXF_OK, s);

    int i0[] = {0}; double v1c[] = {1.0};
    s = cxf_addconstr(model, 1, i0, v1c, '<', 6.0, "c4");
    TEST_ASSERT_EQUAL_INT(CXF_OK, s);

    s = cxf_optimize(model);
    TEST_ASSERT_EQUAL_INT(CXF_OK, s);
    cxf_getintattr(model, "Status", &opt);
    TEST_ASSERT_EQUAL_INT(CXF_OPTIMAL, opt);

    cxf_getdblattr(model, "ObjVal", &objval);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, -24.0, objval);

    assert_constraints_satisfied(model, 1e-6);
    assert_bounds_satisfied(model, 1e-6);
}

/**
 * Equality constraints only (standard form).
 *
 *   min  x + y
 *   s.t. x + y = 6     (c1)
 *        x - y = 2     (c2)
 *        x, y >= 0
 *
 * Optimal: x=4, y=2, obj=6
 */
void test_equality_constraints(void) {
    int s;
    int opt;
    double objval;

    s = cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, CXF_INFINITY, CXF_CONTINUOUS, "x");
    TEST_ASSERT_EQUAL_INT(CXF_OK, s);
    s = cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, CXF_INFINITY, CXF_CONTINUOUS, "y");
    TEST_ASSERT_EQUAL_INT(CXF_OK, s);

    int i01[] = {0, 1};
    double v_eq1[] = {1.0, 1.0};
    s = cxf_addconstr(model, 2, i01, v_eq1, '=', 6.0, "c1");
    TEST_ASSERT_EQUAL_INT(CXF_OK, s);

    double v_eq2[] = {1.0, -1.0};
    s = cxf_addconstr(model, 2, i01, v_eq2, '=', 2.0, "c2");
    TEST_ASSERT_EQUAL_INT(CXF_OK, s);

    s = cxf_optimize(model);
    TEST_ASSERT_EQUAL_INT(CXF_OK, s);
    cxf_getintattr(model, "Status", &opt);
    TEST_ASSERT_EQUAL_INT(CXF_OPTIMAL, opt);

    cxf_getdblattr(model, "ObjVal", &objval);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 6.0, objval);

    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 4.0, model->solution[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 2.0, model->solution[1]);

    assert_constraints_satisfied(model, 1e-6);
    assert_bounds_satisfied(model, 1e-6);
}

/**
 * >= constraints only.
 *
 *   min   x + y
 *   s.t.  x + y >= 5    (c1)
 *         x     >= 2    (c2)
 *         x, y >= 0
 *
 * Optimal: x=2, y=3, obj=5
 */
void test_geq_constraints(void) {
    int s;
    int opt;
    double objval;

    s = cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, CXF_INFINITY, CXF_CONTINUOUS, "x");
    TEST_ASSERT_EQUAL_INT(CXF_OK, s);
    s = cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, CXF_INFINITY, CXF_CONTINUOUS, "y");
    TEST_ASSERT_EQUAL_INT(CXF_OK, s);

    int i01[] = {0, 1}; double v11[] = {1.0, 1.0};
    s = cxf_addconstr(model, 2, i01, v11, '>', 5.0, "c1");
    TEST_ASSERT_EQUAL_INT(CXF_OK, s);

    int i0[] = {0}; double v1[] = {1.0};
    s = cxf_addconstr(model, 1, i0, v1, '>', 2.0, "c2");
    TEST_ASSERT_EQUAL_INT(CXF_OK, s);

    s = cxf_optimize(model);
    TEST_ASSERT_EQUAL_INT(CXF_OK, s);
    cxf_getintattr(model, "Status", &opt);
    TEST_ASSERT_EQUAL_INT(CXF_OPTIMAL, opt);

    cxf_getdblattr(model, "ObjVal", &objval);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 5.0, objval);

    assert_constraints_satisfied(model, 1e-6);
    assert_bounds_satisfied(model, 1e-6);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_mixed_senses);
    RUN_TEST(test_multi_leq_constraints);
    RUN_TEST(test_equality_constraints);
    RUN_TEST(test_geq_constraints);
    return UNITY_END();
}
