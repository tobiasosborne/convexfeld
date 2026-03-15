/**
 * @file test_model_flags_v2.c
 * @brief V2 spec tests for cxf_check_model_flags1/flags2.
 * Spec: docs/specs-v2/specs/modules/model_type_checking.md
 */
#include "unity.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"

int cxf_check_model_flags1(CxfModel *model);
int cxf_check_model_flags2(CxfModel *model);

static CxfEnv *env = NULL;
void setUp(void) { cxf_loadenv(&env, NULL); }
void tearDown(void) { cxf_freeenv(env); env = NULL; }

static CxfModel *make_model(void) {
    CxfModel *m = NULL;
    cxf_newmodel(env, &m, "t", 0, NULL, NULL, NULL, NULL, NULL);
    return m;
}

/*=== flags1: active optimization state ===*/

void test_flags1_null(void) {
    TEST_ASSERT_EQUAL_INT(0, cxf_check_model_flags1(NULL));
}

void test_flags1_fresh(void) {
    CxfModel *m = make_model();
    TEST_ASSERT_EQUAL_INT(0, cxf_check_model_flags1(m));
    cxf_freemodel(m);
}

void test_flags1_concurrent_solve(void) {
    CxfModel *m = make_model();
    m->self_ptr = m;  /* simulates active concurrent solve */
    TEST_ASSERT_EQUAL_INT(1, cxf_check_model_flags1(m));
    m->self_ptr = NULL;
    cxf_freemodel(m);
}

void test_flags1_blocked_with_factorization(void) {
    CxfModel *m = make_model();
    int dummy = 0;
    m->modification_blocked = 1;
    m->solution_data = &dummy;
    TEST_ASSERT_EQUAL_INT(1, cxf_check_model_flags1(m));
    m->modification_blocked = 0;
    m->solution_data = NULL;
    cxf_freemodel(m);
}

void test_flags1_blocked_no_factorization(void) {
    CxfModel *m = make_model();
    m->modification_blocked = 1;
    m->solution_data = NULL;
    TEST_ASSERT_EQUAL_INT(0, cxf_check_model_flags1(m));
    m->modification_blocked = 0;
    cxf_freemodel(m);
}

void test_flags1_not_blocked_with_factorization(void) {
    CxfModel *m = make_model();
    int dummy = 0;
    m->solution_data = &dummy;
    /* blocked=0 and self_ptr=NULL => no active state */
    TEST_ASSERT_EQUAL_INT(0, cxf_check_model_flags1(m));
    m->solution_data = NULL;
    cxf_freemodel(m);
}

void test_flags1_both_conditions_met(void) {
    CxfModel *m = make_model();
    int dummy = 0;
    m->self_ptr = m;
    m->modification_blocked = 1;
    m->solution_data = &dummy;
    TEST_ASSERT_EQUAL_INT(1, cxf_check_model_flags1(m));
    m->self_ptr = NULL;
    m->modification_blocked = 0;
    m->solution_data = NULL;
    cxf_freemodel(m);
}

/*=== flags2: dual data availability ===*/

void test_flags2_null(void) {
    TEST_ASSERT_EQUAL_INT(0, cxf_check_model_flags2(NULL));
}

void test_flags2_fresh(void) {
    CxfModel *m = make_model();
    TEST_ASSERT_EQUAL_INT(0, cxf_check_model_flags2(m));
    cxf_freemodel(m);
}

void test_flags2_no_self_ptr(void) {
    CxfModel *m = make_model();
    double pi = 1.0;
    m->pi = &pi;
    m->status = CXF_OPTIMAL;
    m->modification_blocked = 1;
    TEST_ASSERT_EQUAL_INT(0, cxf_check_model_flags2(m));
    m->pi = NULL; m->modification_blocked = 0;
    cxf_freemodel(m);
}

void test_flags2_no_dual_data(void) {
    CxfModel *m = make_model();
    m->self_ptr = m;
    m->status = CXF_OPTIMAL;
    m->modification_blocked = 1;
    TEST_ASSERT_EQUAL_INT(0, cxf_check_model_flags2(m));
    m->self_ptr = NULL; m->modification_blocked = 0;
    cxf_freemodel(m);
}

void test_flags2_optimal(void) {
    CxfModel *m = make_model();
    double pi = 1.0;
    m->self_ptr = m; m->pi = &pi;
    m->status = CXF_OPTIMAL; m->modification_blocked = 1;
    TEST_ASSERT_EQUAL_INT(1, cxf_check_model_flags2(m));
    m->self_ptr = NULL; m->pi = NULL; m->modification_blocked = 0;
    cxf_freemodel(m);
}

void test_flags2_infeasible(void) {
    CxfModel *m = make_model();
    double pi = 1.0;
    m->self_ptr = m; m->pi = &pi;
    m->status = CXF_INFEASIBLE; m->modification_blocked = 1;
    TEST_ASSERT_EQUAL_INT(1, cxf_check_model_flags2(m));
    m->self_ptr = NULL; m->pi = NULL; m->modification_blocked = 0;
    cxf_freemodel(m);
}

void test_flags2_inf_or_unbd(void) {
    CxfModel *m = make_model();
    double pi = 1.0;
    m->self_ptr = m; m->pi = &pi;
    m->status = CXF_INF_OR_UNBD; m->modification_blocked = 1;
    TEST_ASSERT_EQUAL_INT(1, cxf_check_model_flags2(m));
    m->self_ptr = NULL; m->pi = NULL; m->modification_blocked = 0;
    cxf_freemodel(m);
}

void test_flags2_unbounded_rejected(void) {
    CxfModel *m = make_model();
    double pi = 1.0;
    m->self_ptr = m; m->pi = &pi;
    m->status = CXF_UNBOUNDED; m->modification_blocked = 1;
    TEST_ASSERT_EQUAL_INT(0, cxf_check_model_flags2(m));
    m->self_ptr = NULL; m->pi = NULL; m->modification_blocked = 0;
    cxf_freemodel(m);
}

void test_flags2_iter_limit_rejected(void) {
    CxfModel *m = make_model();
    double pi = 1.0;
    m->self_ptr = m; m->pi = &pi;
    m->status = CXF_ITERATION_LIMIT; m->modification_blocked = 1;
    TEST_ASSERT_EQUAL_INT(0, cxf_check_model_flags2(m));
    m->self_ptr = NULL; m->pi = NULL; m->modification_blocked = 0;
    cxf_freemodel(m);
}

void test_flags2_not_blocked_rejected(void) {
    CxfModel *m = make_model();
    double pi = 1.0;
    m->self_ptr = m; m->pi = &pi;
    m->status = CXF_OPTIMAL; m->modification_blocked = 0;
    TEST_ASSERT_EQUAL_INT(0, cxf_check_model_flags2(m));
    m->self_ptr = NULL; m->pi = NULL;
    cxf_freemodel(m);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_flags1_null);
    RUN_TEST(test_flags1_fresh);
    RUN_TEST(test_flags1_concurrent_solve);
    RUN_TEST(test_flags1_blocked_with_factorization);
    RUN_TEST(test_flags1_blocked_no_factorization);
    RUN_TEST(test_flags1_not_blocked_with_factorization);
    RUN_TEST(test_flags1_both_conditions_met);
    RUN_TEST(test_flags2_null);
    RUN_TEST(test_flags2_fresh);
    RUN_TEST(test_flags2_no_self_ptr);
    RUN_TEST(test_flags2_no_dual_data);
    RUN_TEST(test_flags2_optimal);
    RUN_TEST(test_flags2_infeasible);
    RUN_TEST(test_flags2_inf_or_unbd);
    RUN_TEST(test_flags2_unbounded_rejected);
    RUN_TEST(test_flags2_iter_limit_rejected);
    RUN_TEST(test_flags2_not_blocked_rejected);
    return UNITY_END();
}
