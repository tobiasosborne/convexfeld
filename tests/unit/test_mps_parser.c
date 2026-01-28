/**
 * @file test_mps_parser.c
 * @brief Tests for MPS file parser.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "unity.h"

/* ConvexFeld headers */
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_matrix.h"
#include "convexfeld/cxf_mps.h"

void setUp(void) {}
void tearDown(void) {}

/* Helper to create test MPS file */
static void write_test_mps(const char *filename, const char *content) {
    FILE *f = fopen(filename, "w");
    TEST_ASSERT_NOT_NULL(f);
    fputs(content, f);
    fclose(f);
}

/* Test parsing simple MPS file (afiro-like) */
void test_parse_simple_mps(void) {
    CxfEnv *env = NULL;
    CxfModel *model = NULL;
    int status;

    const char *mps_content =
        "NAME          SIMPLE\n"
        "ROWS\n"
        " N  OBJ\n"
        " L  C1\n"
        " E  C2\n"
        "COLUMNS\n"
        "    X1        OBJ                1.   C1                 2.\n"
        "    X1        C2                 3.\n"
        "    X2        OBJ                4.   C1                 5.\n"
        "    X2        C2                 6.\n"
        "RHS\n"
        "    RHS1      C1                10.   C2                20.\n"
        "ENDATA\n";

    write_test_mps("/tmp/test_simple.mps", mps_content);

    status = cxf_loadenv(&env, NULL);
    TEST_ASSERT_EQUAL(CXF_OK, status);

    status = cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL(CXF_OK, status);

    /* Parse MPS file */
    status = cxf_readmps(model, "/tmp/test_simple.mps");
    TEST_ASSERT_EQUAL(CXF_OK, status);

    /* Verify dimensions */
    TEST_ASSERT_EQUAL(2, model->num_vars);
    TEST_ASSERT_EQUAL(2, model->num_constrs);

    /* Verify objective coefficients */
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 1.0, model->obj_coeffs[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 4.0, model->obj_coeffs[1]);

    /* Verify RHS */
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 10.0, model->matrix->rhs[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 20.0, model->matrix->rhs[1]);

    /* Verify constraint senses */
    TEST_ASSERT_EQUAL('<', model->matrix->sense[0]);
    TEST_ASSERT_EQUAL('=', model->matrix->sense[1]);

    cxf_freemodel(model);
    cxf_freeenv(env);
    remove("/tmp/test_simple.mps");
}

/* Test parsing MPS with bounds */
void test_parse_mps_with_bounds(void) {
    CxfEnv *env = NULL;
    CxfModel *model = NULL;
    int status;

    const char *mps_content =
        "NAME          BOUNDED\n"
        "ROWS\n"
        " N  OBJ\n"
        " L  C1\n"
        "COLUMNS\n"
        "    X1        OBJ                1.   C1                 1.\n"
        "    X2        OBJ                2.   C1                 1.\n"
        "    X3        OBJ                3.   C1                 1.\n"
        "RHS\n"
        "    RHS1      C1               100.\n"
        "BOUNDS\n"
        " LO BND1      X1                 5.\n"
        " UP BND1      X1                10.\n"
        " FX BND1      X2                 7.\n"
        " FR BND1      X3\n"
        "ENDATA\n";

    write_test_mps("/tmp/test_bounds.mps", mps_content);

    status = cxf_loadenv(&env, NULL);
    TEST_ASSERT_EQUAL(CXF_OK, status);

    status = cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL(CXF_OK, status);

    status = cxf_readmps(model, "/tmp/test_bounds.mps");
    TEST_ASSERT_EQUAL(CXF_OK, status);

    TEST_ASSERT_EQUAL(3, model->num_vars);

    /* X1: LO=5, UP=10 */
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 5.0, model->lb[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 10.0, model->ub[0]);

    /* X2: FX=7 (lb=ub=7) */
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 7.0, model->lb[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-10, 7.0, model->ub[1]);

    /* X3: FR (free: -inf to +inf) */
    TEST_ASSERT_TRUE(model->lb[2] < -1e90);
    TEST_ASSERT_TRUE(model->ub[2] > 1e90);

    cxf_freemodel(model);
    cxf_freeenv(env);
    remove("/tmp/test_bounds.mps");
}

/* Test parsing real Netlib file (afiro) */
void test_parse_netlib_afiro(void) {
    CxfEnv *env = NULL;
    CxfModel *model = NULL;
    int status;

    status = cxf_loadenv(&env, NULL);
    TEST_ASSERT_EQUAL(CXF_OK, status);

    status = cxf_newmodel(env, &model, "afiro", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL(CXF_OK, status);

    /* Parse actual Netlib afiro file (use absolute path from source dir) */
    status = cxf_readmps(model, SOURCE_DIR "/benchmarks/netlib/feasible/afiro.mps");
    TEST_ASSERT_EQUAL(CXF_OK, status);

    /* afiro has 32 variables and 27 constraints (excluding objective row) */
    TEST_ASSERT_EQUAL(32, model->num_vars);
    TEST_ASSERT_EQUAL(27, model->num_constrs);

    cxf_freemodel(model);
    cxf_freeenv(env);
}

/* Test error handling for nonexistent file */
void test_parse_nonexistent_file(void) {
    CxfEnv *env = NULL;
    CxfModel *model = NULL;
    int status;

    status = cxf_loadenv(&env, NULL);
    TEST_ASSERT_EQUAL(CXF_OK, status);

    status = cxf_newmodel(env, &model, "test", 0, NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL(CXF_OK, status);

    status = cxf_readmps(model, "/nonexistent/path/file.mps");
    TEST_ASSERT_NOT_EQUAL(CXF_OK, status);

    cxf_freemodel(model);
    cxf_freeenv(env);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_parse_simple_mps);
    RUN_TEST(test_parse_mps_with_bounds);
    RUN_TEST(test_parse_netlib_afiro);
    RUN_TEST(test_parse_nonexistent_file);
    return UNITY_END();
}
