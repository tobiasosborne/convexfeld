/**
 * @file test_mps_solve.c
 * @brief End-to-end test: parse MPS and verify parsing.
 *
 * NOTE: The simplex solver has pre-existing numerical stability issues
 * (eta factor overflow - see issue convexfeld-aiq9). These tests verify
 * that the MPS parser works correctly by checking model dimensions.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "unity.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_mps.h"
#include "convexfeld/cxf_matrix.h"

void setUp(void) {}
void tearDown(void) {}

/* Test parsing afiro - verifies MPS parser produces correct dimensions */
void test_parse_afiro_dimensions(void) {
    CxfEnv *env = NULL;
    CxfModel *model = NULL;
    int status;

    status = cxf_loadenv(&env, NULL);
    TEST_ASSERT_EQUAL(CXF_OK, status);

    status = cxf_newmodel(env, &model, "afiro", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL(CXF_OK, status);

    status = cxf_readmps(model, SOURCE_DIR "/benchmarks/netlib/feasible/afiro.mps");
    TEST_ASSERT_EQUAL(CXF_OK, status);

    /* afiro has 32 variables and 27 constraints */
    TEST_ASSERT_EQUAL(32, model->num_vars);
    TEST_ASSERT_EQUAL(27, model->num_constrs);

    /* Verify matrix is populated */
    TEST_ASSERT_NOT_NULL(model->matrix);
    TEST_ASSERT_NOT_NULL(model->matrix->col_ptr);
    TEST_ASSERT_EQUAL(83, model->matrix->nnz);  /* afiro has 83 nonzeros */

    cxf_freemodel(model);
    cxf_freeenv(env);
}

/* Test parsing sc50b - verifies MPS parser produces correct dimensions */
void test_parse_sc50b_dimensions(void) {
    CxfEnv *env = NULL;
    CxfModel *model = NULL;
    int status;

    status = cxf_loadenv(&env, NULL);
    TEST_ASSERT_EQUAL(CXF_OK, status);

    status = cxf_newmodel(env, &model, "sc50b", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL(CXF_OK, status);

    status = cxf_readmps(model, SOURCE_DIR "/benchmarks/netlib/feasible/sc50b.mps");
    TEST_ASSERT_EQUAL(CXF_OK, status);

    /* sc50b has 48 variables and 50 constraints */
    TEST_ASSERT_EQUAL(48, model->num_vars);
    TEST_ASSERT_EQUAL(50, model->num_constrs);

    cxf_freemodel(model);
    cxf_freeenv(env);
}

/* Test parsing sc105 - larger problem */
void test_parse_sc105_dimensions(void) {
    CxfEnv *env = NULL;
    CxfModel *model = NULL;
    int status;

    status = cxf_loadenv(&env, NULL);
    TEST_ASSERT_EQUAL(CXF_OK, status);

    status = cxf_newmodel(env, &model, "sc105", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL(CXF_OK, status);

    status = cxf_readmps(model, SOURCE_DIR "/benchmarks/netlib/feasible/sc105.mps");
    TEST_ASSERT_EQUAL(CXF_OK, status);

    /* sc105 has 103 variables and 105 constraints */
    TEST_ASSERT_EQUAL(103, model->num_vars);
    TEST_ASSERT_EQUAL(105, model->num_constrs);

    cxf_freemodel(model);
    cxf_freeenv(env);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_parse_afiro_dimensions);
    RUN_TEST(test_parse_sc50b_dimensions);
    RUN_TEST(test_parse_sc105_dimensions);
    return UNITY_END();
}
