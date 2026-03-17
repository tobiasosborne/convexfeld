/**
 * @file test_simplex_cleanup_call.c
 * @brief Tests for cxf_simplex_cleanup (simplex_lifecycle.md step 9).
 */

#include "unity.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_types.h"
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/*--- Helper: build a minimal SolverState with constraint senses ---*/
static SolverState *make_state(int m, const char *senses,
                               const double *min_act,
                               const double *max_act) {
    SolverState *s = (SolverState *)calloc(1, sizeof(SolverState));
    s->num_constrs = m;
    s->num_vars = 0;

    s->work_sense = (char *)malloc((size_t)m * sizeof(char));
    memcpy(s->work_sense, senses, (size_t)m);

    s->min_activity = (double *)malloc((size_t)m * sizeof(double));
    memcpy(s->min_activity, min_act, (size_t)m * sizeof(double));

    s->max_activity = (double *)malloc((size_t)m * sizeof(double));
    memcpy(s->max_activity, max_act, (size_t)m * sizeof(double));

    return s;
}

static void free_state(SolverState *s) {
    free(s->work_sense);
    free(s->min_activity);
    free(s->max_activity);
    free(s);
}

/*--- Test 1: null-safe ---*/
void test_cleanup_null_state(void) {
    int rc = cxf_simplex_cleanup(NULL, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
}

/*--- Test 2: null env uses default tolerance ---*/
void test_cleanup_null_env(void) {
    double min_a[] = {0.0}, max_a[] = {0.0};
    SolverState *s = make_state(1, "<", min_a, max_a);
    int rc = cxf_simplex_cleanup(s, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_EQUAL_CHAR('=', s->work_sense[0]);
    free_state(s);
}

/*--- Test 3: tight LEQ converted to equality ---*/
void test_cleanup_tight_leq_converted(void) {
    CxfEnv *env = NULL;
    cxf_loadenv(&env, NULL);
    double min_a[] = {0.0}, max_a[] = {0.0};
    SolverState *s = make_state(1, "<", min_a, max_a);
    int rc = cxf_simplex_cleanup(s, env);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_EQUAL_CHAR('=', s->work_sense[0]);
    free_state(s);
    cxf_freeenv(env);
}

/*--- Test 4: tight GEQ converted ---*/
void test_cleanup_tight_geq_converted(void) {
    CxfEnv *env = NULL;
    cxf_loadenv(&env, NULL);
    double min_a[] = {0.0}, max_a[] = {0.0};
    SolverState *s = make_state(1, ">", min_a, max_a);
    int rc = cxf_simplex_cleanup(s, env);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_EQUAL_CHAR('=', s->work_sense[0]);
    free_state(s);
    cxf_freeenv(env);
}

/*--- Test 5: non-tight inequality NOT converted ---*/
void test_cleanup_nontight_not_converted(void) {
    CxfEnv *env = NULL;
    cxf_loadenv(&env, NULL);
    double min_a[] = {-1.0}, max_a[] = {10.0};
    SolverState *s = make_state(1, "<", min_a, max_a);
    int rc = cxf_simplex_cleanup(s, env);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_EQUAL_CHAR('<', s->work_sense[0]);
    free_state(s);
    cxf_freeenv(env);
}

/*--- Test 6: existing equality NOT touched ---*/
void test_cleanup_equality_unchanged(void) {
    CxfEnv *env = NULL;
    cxf_loadenv(&env, NULL);
    double min_a[] = {0.0}, max_a[] = {0.0};
    SolverState *s = make_state(1, "=", min_a, max_a);
    int rc = cxf_simplex_cleanup(s, env);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_EQUAL_CHAR('=', s->work_sense[0]);
    free_state(s);
    cxf_freeenv(env);
}

/*--- Test 7: mixed constraints --- only tight ones convert ---*/
void test_cleanup_mixed_constraints(void) {
    CxfEnv *env = NULL;
    cxf_loadenv(&env, NULL);
    /* [0] '<' tight->'=', [1] '>' loose, [2] '=' stays, [3] '<' loose, [4] '>' tight->'=' */
    char senses[] = {'<', '>', '=', '<', '>'};
    double min_a[] = {0.0, -5.0, 0.0, -2.0, 0.0};
    double max_a[] = {0.0,  3.0, 0.0,  8.0, 0.0};
    SolverState *s = make_state(5, senses, min_a, max_a);
    int rc = cxf_simplex_cleanup(s, env);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_EQUAL_CHAR('=', s->work_sense[0]);
    TEST_ASSERT_EQUAL_CHAR('>', s->work_sense[1]);
    TEST_ASSERT_EQUAL_CHAR('=', s->work_sense[2]);
    TEST_ASSERT_EQUAL_CHAR('<', s->work_sense[3]);
    TEST_ASSERT_EQUAL_CHAR('=', s->work_sense[4]);
    free_state(s);
    cxf_freeenv(env);
}

/*--- Test 8: missing arrays gracefully handled ---*/
void test_cleanup_missing_arrays(void) {
    SolverState s;
    memset(&s, 0, sizeof(s));
    s.num_constrs = 3;
    /* work_sense, min_activity, max_activity all NULL */

    int rc = cxf_simplex_cleanup(&s, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
}

/*--- Test 9: E-sense equality also skipped ---*/
void test_cleanup_E_sense_skipped(void) {
    CxfEnv *env = NULL;
    cxf_loadenv(&env, NULL);
    double min_a[] = {0.0}, max_a[] = {0.0};
    SolverState *s = make_state(1, "E", min_a, max_a);
    int rc = cxf_simplex_cleanup(s, env);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_EQUAL_CHAR('E', s->work_sense[0]);
    free_state(s);
    cxf_freeenv(env);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_cleanup_null_state);
    RUN_TEST(test_cleanup_null_env);
    RUN_TEST(test_cleanup_tight_leq_converted);
    RUN_TEST(test_cleanup_tight_geq_converted);
    RUN_TEST(test_cleanup_nontight_not_converted);
    RUN_TEST(test_cleanup_equality_unchanged);
    RUN_TEST(test_cleanup_mixed_constraints);
    RUN_TEST(test_cleanup_missing_arrays);
    RUN_TEST(test_cleanup_E_sense_skipped);

    return UNITY_END();
}
