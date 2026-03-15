/**
 * @file test_eta_fields.c
 * @brief Tests for V2-compliant EtaVector fields.
 *
 * Validates that EtaVector struct has all spec-required fields
 * (enteringVar, leavingVar, direction, reducedCost) and that
 * cxf_pivot_with_eta populates them correctly.
 *
 * Spec: docs/specs-v2/specs/data-model/eta_vector.md
 * Issues: convexfeld-nhyx, convexfeld-x3vg, convexfeld-lhmd,
 *         convexfeld-nb1z, convexfeld-un17
 */

#include "unity.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_types.h"
#include <stdlib.h>
#include <math.h>

/* Forward declarations */
BasisState *cxf_basis_create(int m, int n);
void cxf_basis_free(BasisState *basis);
int cxf_pivot_with_eta(BasisState *basis, int pivotRow, const double *pivotCol,
                       int enteringVar, int leavingVar, int leavingStatus,
                       double reducedCost, int direction);
EtaVector *cxf_eta_create(int type, int pivot_row, int nnz);
void cxf_eta_free(EtaVector *eta);
int cxf_eta_validate(const EtaVector *eta, int max_rows);

static BasisState *basis = NULL;

void setUp(void) {
    basis = cxf_basis_create(3, 6);
    TEST_ASSERT_NOT_NULL(basis);
    basis->basic_vars[0] = 3;
    basis->basic_vars[1] = 4;
    basis->basic_vars[2] = 5;
    for (int j = 0; j < 3; j++)
        basis->var_status[j] = CXF_VAR_AT_LOWER;
    basis->var_status[3] = 0;
    basis->var_status[4] = 1;
    basis->var_status[5] = 2;
}

void tearDown(void) {
    if (basis) { cxf_basis_free(basis); basis = NULL; }
}

/*--- Type constant tests (convexfeld-un17) ---*/

void test_type_pivot_constant(void) {
    TEST_ASSERT_EQUAL_INT(2, CXF_ETA_PIVOT);
}

void test_type_variable_fix_constant(void) {
    TEST_ASSERT_EQUAL_INT(3, CXF_ETA_VARIABLE_FIX);
}

void test_type_warm_start_constant(void) {
    TEST_ASSERT_EQUAL_INT(4, CXF_ETA_WARM_START);
}

void test_pivot_eta_uses_spec_type(void) {
    double col[] = {2.0, 0.5, 0.1};
    cxf_pivot_with_eta(basis, 0, col, 0, 3, CXF_VAR_AT_LOWER, -1.5, 1);
    TEST_ASSERT_EQUAL_INT(CXF_ETA_PIVOT, basis->eta_head->type);
}

/*--- entering_var field (convexfeld-nhyx) ---*/

void test_entering_var_stored(void) {
    double col[] = {2.0, 0.5, 0.1};
    cxf_pivot_with_eta(basis, 0, col, 2, 3, CXF_VAR_AT_LOWER, 0.0, 1);
    TEST_ASSERT_EQUAL_INT(2, basis->eta_head->entering_var);
}

void test_entering_var_zero(void) {
    double col[] = {2.0, 0.5, 0.1};
    cxf_pivot_with_eta(basis, 0, col, 0, 3, CXF_VAR_AT_LOWER, 0.0, 1);
    TEST_ASSERT_EQUAL_INT(0, basis->eta_head->entering_var);
}

/*--- leaving_var field (convexfeld-x3vg) ---*/

void test_leaving_var_stored(void) {
    double col[] = {2.0, 0.5, 0.1};
    cxf_pivot_with_eta(basis, 0, col, 0, 3, CXF_VAR_AT_LOWER, 0.0, 1);
    TEST_ASSERT_EQUAL_INT(3, basis->eta_head->leaving_var);
}

void test_leaving_var_different_row(void) {
    double col[] = {0.1, 1.5, 0.3};
    cxf_pivot_with_eta(basis, 1, col, 0, 4, CXF_VAR_AT_LOWER, 0.0, 1);
    TEST_ASSERT_EQUAL_INT(4, basis->eta_head->leaving_var);
}

/*--- direction field (convexfeld-lhmd) ---*/

void test_direction_from_lower(void) {
    double col[] = {2.0, 0.5, 0.1};
    cxf_pivot_with_eta(basis, 0, col, 0, 3, CXF_VAR_AT_LOWER, 0.0, 1);
    TEST_ASSERT_TRUE(basis->eta_head->direction >= 0);
}

void test_direction_from_upper(void) {
    double col[] = {2.0, 0.5, 0.1};
    cxf_pivot_with_eta(basis, 0, col, 0, 3, CXF_VAR_AT_LOWER, 0.0, -1);
    TEST_ASSERT_TRUE(basis->eta_head->direction < 0);
}

/*--- reduced_cost field (convexfeld-nb1z) ---*/

void test_reduced_cost_stored(void) {
    double col[] = {2.0, 0.5, 0.1};
    cxf_pivot_with_eta(basis, 0, col, 0, 3, CXF_VAR_AT_LOWER, -3.14, 1);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, -3.14, basis->eta_head->reduced_cost);
}

void test_reduced_cost_zero(void) {
    double col[] = {2.0, 0.5, 0.1};
    cxf_pivot_with_eta(basis, 0, col, 0, 3, CXF_VAR_AT_LOWER, 0.0, 1);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, basis->eta_head->reduced_cost);
}

void test_reduced_cost_positive(void) {
    double col[] = {2.0, 0.5, 0.1};
    cxf_pivot_with_eta(basis, 0, col, 0, 3, CXF_VAR_AT_LOWER, 7.5, 1);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 7.5, basis->eta_head->reduced_cost);
}

/*--- Validation accepts spec types (convexfeld-un17) ---*/

void test_validate_accepts_pivot_type(void) {
    EtaVector *eta = cxf_eta_create(CXF_ETA_PIVOT, 0, 0);
    TEST_ASSERT_NOT_NULL(eta);
    eta->pivot_elem = 1.0;
    TEST_ASSERT_EQUAL_INT(CXF_OK, cxf_eta_validate(eta, 3));
    cxf_eta_free(eta);
}

void test_validate_accepts_varfix_type(void) {
    EtaVector *eta = cxf_eta_create(CXF_ETA_VARIABLE_FIX, 0, 0);
    TEST_ASSERT_NOT_NULL(eta);
    eta->pivot_elem = 1.0;
    TEST_ASSERT_EQUAL_INT(CXF_OK, cxf_eta_validate(eta, 3));
    cxf_eta_free(eta);
}

void test_validate_accepts_warmstart_type(void) {
    EtaVector *eta = cxf_eta_create(CXF_ETA_WARM_START, 0, 0);
    TEST_ASSERT_NOT_NULL(eta);
    eta->pivot_elem = 1.0;
    TEST_ASSERT_EQUAL_INT(CXF_OK, cxf_eta_validate(eta, 3));
    cxf_eta_free(eta);
}

void test_validate_rejects_type_1(void) {
    EtaVector *eta = cxf_eta_create(1, 0, 0);
    TEST_ASSERT_NOT_NULL(eta);
    eta->pivot_elem = 1.0;
    TEST_ASSERT_NOT_EQUAL(CXF_OK, cxf_eta_validate(eta, 3));
    cxf_eta_free(eta);
}

void test_validate_rejects_type_0(void) {
    EtaVector *eta = cxf_eta_create(0, 0, 0);
    TEST_ASSERT_NOT_NULL(eta);
    eta->pivot_elem = 1.0;
    TEST_ASSERT_NOT_EQUAL(CXF_OK, cxf_eta_validate(eta, 3));
    cxf_eta_free(eta);
}

/*--- Combined: all fields populated on one pivot ---*/

void test_all_fields_populated(void) {
    double col[] = {2.0, 0.5, 0.1};
    int rc = cxf_pivot_with_eta(basis, 0, col, 1, 3,
                                 CXF_VAR_AT_LOWER, -2.5, 1);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    EtaVector *eta = basis->eta_head;
    TEST_ASSERT_NOT_NULL(eta);
    TEST_ASSERT_EQUAL_INT(CXF_ETA_PIVOT, eta->type);
    TEST_ASSERT_EQUAL_INT(0, eta->pivot_row);
    TEST_ASSERT_EQUAL_INT(1, eta->entering_var);
    TEST_ASSERT_EQUAL_INT(3, eta->leaving_var);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 2.0, eta->pivot_elem);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, -2.5, eta->reduced_cost);
    TEST_ASSERT_EQUAL_INT(1, eta->direction);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_type_pivot_constant);
    RUN_TEST(test_type_variable_fix_constant);
    RUN_TEST(test_type_warm_start_constant);
    RUN_TEST(test_pivot_eta_uses_spec_type);
    RUN_TEST(test_entering_var_stored);
    RUN_TEST(test_entering_var_zero);
    RUN_TEST(test_leaving_var_stored);
    RUN_TEST(test_leaving_var_different_row);
    RUN_TEST(test_direction_from_lower);
    RUN_TEST(test_direction_from_upper);
    RUN_TEST(test_reduced_cost_stored);
    RUN_TEST(test_reduced_cost_zero);
    RUN_TEST(test_reduced_cost_positive);
    RUN_TEST(test_validate_accepts_pivot_type);
    RUN_TEST(test_validate_accepts_varfix_type);
    RUN_TEST(test_validate_accepts_warmstart_type);
    RUN_TEST(test_validate_rejects_type_1);
    RUN_TEST(test_validate_rejects_type_0);
    RUN_TEST(test_all_fields_populated);
    return UNITY_END();
}
