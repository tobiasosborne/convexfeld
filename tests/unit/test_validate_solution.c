/**
 * @file test_validate_solution.c
 * @brief Tests for cxf_validate_solution (V2 data_validation.md).
 *
 * Validates:
 * - NULL model returns CXF_ERROR_NULL_ARGUMENT
 * - Empty model (no vars) returns 0 violations
 * - Feasible solution returns 0 violations
 * - Bound violation detected
 * - Constraint violation detected (LE, GE, EQ)
 * - Multiple violations counted correctly
 * - Tolerance respected
 */

#include "unity.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_matrix.h"
#include "convexfeld/cxf_types.h"

/* Forward declarations */
int cxf_addvar(CxfModel *model, int numnz, int *vind, double *vval,
               double obj, double lb, double ub, char vtype,
               const char *varname);
int cxf_addconstr(CxfModel *model, int numnz, const int *cind,
                  const double *cval, char sense, double rhs,
                  const char *constrname);
int cxf_validate_solution(CxfModel *model, double tolerance,
                          int *num_violations_out);

static CxfEnv *env = NULL;

void setUp(void) {
    cxf_loadenv(&env, NULL);
}

void tearDown(void) {
    cxf_freeenv(env);
    env = NULL;
}

/*******************************************************************************
 * NULL / Edge Case Handling
 ******************************************************************************/

void test_null_model_returns_error(void) {
    int violations = -1;
    int rc = cxf_validate_solution(NULL, 1e-6, &violations);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, rc);
}

void test_empty_model_returns_zero_violations(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "empty", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_NOT_NULL(model);

    int violations = -1;
    int rc = cxf_validate_solution(model, 1e-6, &violations);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_EQUAL_INT(0, violations);

    cxf_freemodel(model);
}

void test_null_violations_out_is_ok(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);

    int rc = cxf_validate_solution(model, 1e-6, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    cxf_freemodel(model);
}

/*******************************************************************************
 * Feasible Solution
 ******************************************************************************/

void test_feasible_solution_returns_zero_violations(void) {
    /* Model: min x0 + x1, 0 <= x0 <= 10, 0 <= x1 <= 10
     * Constraint: x0 + x1 <= 15 */
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "feas", 0, NULL, NULL, NULL, NULL, NULL);
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x0");
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x1");

    int cind[] = {0, 1};
    double cval[] = {1.0, 1.0};
    cxf_addconstr(model, 2, cind, cval, '<', 15.0, "c1");

    /* Set feasible solution: x0=3, x1=4 => activity=7 <= 15 */
    model->solution[0] = 3.0;
    model->solution[1] = 4.0;

    int violations = -1;
    int rc = cxf_validate_solution(model, 1e-6, &violations);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_EQUAL_INT(0, violations);

    cxf_freemodel(model);
}

/*******************************************************************************
 * Bound Violations
 ******************************************************************************/

void test_lower_bound_violation_detected(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "lb_viol", 0, NULL, NULL, NULL, NULL, NULL);
    cxf_addvar(model, 0, NULL, NULL, 1.0, 2.0, 10.0, 'C', "x0");

    /* x0 = 1.0 violates lb = 2.0 */
    model->solution[0] = 1.0;

    int violations = -1;
    int rc = cxf_validate_solution(model, 1e-6, &violations);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_EQUAL_INT(1, violations);

    cxf_freemodel(model);
}

void test_upper_bound_violation_detected(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "ub_viol", 0, NULL, NULL, NULL, NULL, NULL);
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 5.0, 'C', "x0");

    /* x0 = 7.0 violates ub = 5.0 */
    model->solution[0] = 7.0;

    int violations = -1;
    int rc = cxf_validate_solution(model, 1e-6, &violations);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_EQUAL_INT(1, violations);

    cxf_freemodel(model);
}

void test_bound_within_tolerance_is_feasible(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "tol", 0, NULL, NULL, NULL, NULL, NULL);
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x0");

    /* x0 = -1e-8 is within tolerance 1e-6 of lb=0 */
    model->solution[0] = -1e-8;

    int violations = -1;
    int rc = cxf_validate_solution(model, 1e-6, &violations);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_EQUAL_INT(0, violations);

    cxf_freemodel(model);
}

/*******************************************************************************
 * Constraint Violations
 ******************************************************************************/

void test_le_constraint_violation_detected(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "le_viol", 0, NULL, NULL, NULL, NULL, NULL);
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 100.0, 'C', "x0");

    int cind[] = {0};
    double cval[] = {1.0};
    cxf_addconstr(model, 1, cind, cval, '<', 5.0, "c1");

    /* x0 = 10 => activity = 10 > rhs = 5 */
    model->solution[0] = 10.0;

    int violations = -1;
    int rc = cxf_validate_solution(model, 1e-6, &violations);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_EQUAL_INT(1, violations);

    cxf_freemodel(model);
}

void test_ge_constraint_violation_detected(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "ge_viol", 0, NULL, NULL, NULL, NULL, NULL);
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 100.0, 'C', "x0");

    int cind[] = {0};
    double cval[] = {1.0};
    cxf_addconstr(model, 1, cind, cval, '>', 20.0, "c1");

    /* x0 = 5 => activity = 5 < rhs = 20 */
    model->solution[0] = 5.0;

    int violations = -1;
    int rc = cxf_validate_solution(model, 1e-6, &violations);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_EQUAL_INT(1, violations);

    cxf_freemodel(model);
}

void test_eq_constraint_violation_detected(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "eq_viol", 0, NULL, NULL, NULL, NULL, NULL);
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 100.0, 'C', "x0");

    int cind[] = {0};
    double cval[] = {1.0};
    cxf_addconstr(model, 1, cind, cval, '=', 10.0, "c1");

    /* x0 = 12 => activity = 12 != rhs = 10 */
    model->solution[0] = 12.0;

    int violations = -1;
    int rc = cxf_validate_solution(model, 1e-6, &violations);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_EQUAL_INT(1, violations);

    cxf_freemodel(model);
}

void test_constraint_within_tolerance_is_feasible(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "ctol", 0, NULL, NULL, NULL, NULL, NULL);
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 100.0, 'C', "x0");

    int cind[] = {0};
    double cval[] = {1.0};
    cxf_addconstr(model, 1, cind, cval, '<', 5.0, "c1");

    /* x0 = 5.0 + 1e-8 => activity barely exceeds rhs, within tol */
    model->solution[0] = 5.0 + 1e-8;

    int violations = -1;
    int rc = cxf_validate_solution(model, 1e-6, &violations);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_EQUAL_INT(0, violations);

    cxf_freemodel(model);
}

/*******************************************************************************
 * Multiple Violations
 ******************************************************************************/

void test_multiple_violations_counted(void) {
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "multi", 0, NULL, NULL, NULL, NULL, NULL);
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 5.0, 'C', "x0");
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 5.0, 'C', "x1");

    int cind[] = {0, 1};
    double cval[] = {1.0, 1.0};
    cxf_addconstr(model, 2, cind, cval, '<', 4.0, "c1");

    /* x0=10 violates ub=5, x1=10 violates ub=5,
     * activity=20 > rhs=4 => 3 total violations */
    model->solution[0] = 10.0;
    model->solution[1] = 10.0;

    int violations = -1;
    int rc = cxf_validate_solution(model, 1e-6, &violations);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_EQUAL_INT(3, violations);

    cxf_freemodel(model);
}

/*******************************************************************************
 * Runner
 ******************************************************************************/

int main(void) {
    UNITY_BEGIN();

    /* NULL / edge cases */
    RUN_TEST(test_null_model_returns_error);
    RUN_TEST(test_empty_model_returns_zero_violations);
    RUN_TEST(test_null_violations_out_is_ok);

    /* Feasible solution */
    RUN_TEST(test_feasible_solution_returns_zero_violations);

    /* Bound violations */
    RUN_TEST(test_lower_bound_violation_detected);
    RUN_TEST(test_upper_bound_violation_detected);
    RUN_TEST(test_bound_within_tolerance_is_feasible);

    /* Constraint violations */
    RUN_TEST(test_le_constraint_violation_detected);
    RUN_TEST(test_ge_constraint_violation_detected);
    RUN_TEST(test_eq_constraint_violation_detected);
    RUN_TEST(test_constraint_within_tolerance_is_feasible);

    /* Multiple violations */
    RUN_TEST(test_multiple_violations_counted);

    return UNITY_END();
}
