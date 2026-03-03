/**
 * @file test_basis_warm.c
 * @brief Tests for basis warm start operations
 *        (split from test_basis.c).
 */

#include "unity.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_types.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* BasisState lifecycle */
BasisState *cxf_basis_create(int m, int n);
void cxf_basis_free(BasisState *basis);

/* BasisSnapshot API */
int cxf_progress_snapshot_create(BasisState *basis, BasisSnapshot *snapshot,
                              int includeFactors);
void cxf_progress_snapshot_free(BasisSnapshot *snapshot);

/* Warm start */
int cxf_basis_warm(BasisState *basis, const int *basic_vars, int m);
int cxf_basis_warm_snapshot(BasisState *basis, const BasisSnapshot *snapshot);

void setUp(void) {}
void tearDown(void) {}

void test_basis_warm_loads_basis(void) {
    BasisState *basis = cxf_basis_create(3, 5);
    int warm_vars[] = {1, 3, 4};

    int status = cxf_basis_warm(basis, warm_vars, 3);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(1, basis->basic_vars[0]);
    TEST_ASSERT_EQUAL_INT(3, basis->basic_vars[1]);
    TEST_ASSERT_EQUAL_INT(4, basis->basic_vars[2]);

    cxf_basis_free(basis);
}

void test_basis_warm_clears_eta_list(void) {
    BasisState *basis = cxf_basis_create(2, 4);
    basis->eta_count = 10;
    int warm_vars[] = {0, 2};

    int status = cxf_basis_warm(basis, warm_vars, 2);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(0, basis->eta_count);

    cxf_basis_free(basis);
}

void test_basis_warm_null_args(void) {
    BasisState *basis = cxf_basis_create(2, 4);
    int warm_vars[] = {0, 1};

    int status = cxf_basis_warm(NULL, warm_vars, 2);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);

    status = cxf_basis_warm(basis, NULL, 2);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);

    cxf_basis_free(basis);
}

void test_basis_warm_size_mismatch(void) {
    BasisState *basis = cxf_basis_create(2, 4);
    int warm_vars[] = {0, 1, 2};  /* 3 vars, but basis has m=2 */

    int status = cxf_basis_warm(basis, warm_vars, 3);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, status);

    cxf_basis_free(basis);
}

void test_basis_warm_resets_pivot_count(void) {
    BasisState *basis = cxf_basis_create(2, 4);
    basis->pivots_since_refactor = 50;
    int warm_vars[] = {0, 2};

    int status = cxf_basis_warm(basis, warm_vars, 2);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(0, basis->pivots_since_refactor);

    cxf_basis_free(basis);
}

void test_basis_warm_snapshot_copies_basis(void) {
    BasisState *source = cxf_basis_create(3, 5);
    source->basic_vars[0] = 1;
    source->basic_vars[1] = 3;
    source->basic_vars[2] = 4;
    source->var_status[1] = 0;
    source->var_status[3] = 1;
    source->var_status[4] = 2;

    BasisSnapshot snap;
    cxf_progress_snapshot_create(source, &snap, 0);

    BasisState *target = cxf_basis_create(3, 5);
    int status = cxf_basis_warm_snapshot(target, &snap);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);

    TEST_ASSERT_EQUAL_INT(1, target->basic_vars[0]);
    TEST_ASSERT_EQUAL_INT(3, target->basic_vars[1]);
    TEST_ASSERT_EQUAL_INT(4, target->basic_vars[2]);

    TEST_ASSERT_EQUAL_INT(CXF_VAR_AT_LOWER, target->var_status[0]);
    TEST_ASSERT_EQUAL_INT(0, target->var_status[1]);
    TEST_ASSERT_EQUAL_INT(CXF_VAR_AT_LOWER, target->var_status[2]);

    cxf_progress_snapshot_free(&snap);
    cxf_basis_free(source);
    cxf_basis_free(target);
}

void test_basis_warm_snapshot_null_args(void) {
    BasisState *basis = cxf_basis_create(2, 3);
    BasisSnapshot snap;
    snap.valid = 1;

    int status = cxf_basis_warm_snapshot(NULL, &snap);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);

    status = cxf_basis_warm_snapshot(basis, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);

    cxf_basis_free(basis);
}

void test_basis_warm_snapshot_invalid_snap(void) {
    BasisState *basis = cxf_basis_create(2, 3);
    BasisSnapshot snap;
    snap.valid = 0;

    int status = cxf_basis_warm_snapshot(basis, &snap);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, status);

    cxf_basis_free(basis);
}

void test_basis_warm_snapshot_dimension_mismatch(void) {
    BasisState *source = cxf_basis_create(3, 5);
    source->basic_vars[0] = 0;
    source->basic_vars[1] = 1;
    source->basic_vars[2] = 2;

    BasisSnapshot snap;
    cxf_progress_snapshot_create(source, &snap, 0);

    BasisState *target = cxf_basis_create(2, 4);
    int status = cxf_basis_warm_snapshot(target, &snap);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, status);

    cxf_progress_snapshot_free(&snap);
    cxf_basis_free(source);
    cxf_basis_free(target);
}

void test_basis_warm_snapshot_clears_etas(void) {
    BasisState *source = cxf_basis_create(2, 3);
    source->basic_vars[0] = 0;
    source->basic_vars[1] = 1;

    BasisSnapshot snap;
    cxf_progress_snapshot_create(source, &snap, 0);

    BasisState *target = cxf_basis_create(2, 3);
    target->eta_count = 15;
    target->pivots_since_refactor = 25;

    int status = cxf_basis_warm_snapshot(target, &snap);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(0, target->eta_count);
    TEST_ASSERT_EQUAL_INT(0, target->pivots_since_refactor);

    cxf_progress_snapshot_free(&snap);
    cxf_basis_free(source);
    cxf_basis_free(target);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_basis_warm_loads_basis);
    RUN_TEST(test_basis_warm_clears_eta_list);
    RUN_TEST(test_basis_warm_null_args);
    RUN_TEST(test_basis_warm_size_mismatch);
    RUN_TEST(test_basis_warm_resets_pivot_count);
    RUN_TEST(test_basis_warm_snapshot_copies_basis);
    RUN_TEST(test_basis_warm_snapshot_null_args);
    RUN_TEST(test_basis_warm_snapshot_invalid_snap);
    RUN_TEST(test_basis_warm_snapshot_dimension_mismatch);
    RUN_TEST(test_basis_warm_snapshot_clears_etas);
    return UNITY_END();
}
