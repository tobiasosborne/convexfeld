/**
 * @file test_bound_prop_tol.c
 * @brief Tests that bound propagation uses CXF_MIN_PIVOT (1e-13) not
 *        CXF_PIVOT_TOL (1e-9) per tolerances_constants.md Section 4 and
 *        numerical_stability.md Section D.
 *
 * Coefficients between 1e-13 and 1e-9 must be processed (not skipped)
 * in step2 and step3 bound propagation.
 *
 * Beads: convexfeld-2tl9
 */

#include "unity.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_pricing.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

extern BasisState *cxf_basis_create(int m, int n);
extern void cxf_basis_free(BasisState *basis);
extern PricingState *cxf_pricing_create(int num_vars, int max_levels);
extern void cxf_pricing_free(PricingState *ctx);
extern int cxf_pricing_init(PricingState *ctx, int num_vars, int strategy);
extern int cxf_pricing_init_constrs(PricingState *ctx, int num_constrs);
extern int cxf_simplex_step2(SolverState *state, CxfEnv *env);
extern int cxf_simplex_step3(SolverState *state, CxfEnv *env);

/* Build a 1-constraint, 1-variable state with given coefficient. */
static SolverState *make_state(double a, char sense, double rhs,
                               double lb, double ub) {
    int n = 1, m = 1, total = n + m;
    SolverState *s = calloc(1, sizeof(SolverState));
    s->num_vars = n;
    s->num_constrs = m;

    s->basis = cxf_basis_create(m, total);
    s->basis->var_status[0] = CXF_VAR_AT_LOWER;
    s->basis->basic_vars[0] = 1;
    s->basis->var_status[1] = 0;

    s->work_lb = calloc(total, sizeof(double));
    s->work_ub = calloc(total, sizeof(double));
    s->work_lb[0] = lb;  s->work_ub[0] = ub;
    s->work_lb[1] = -CXF_INFINITY;  s->work_ub[1] = CXF_INFINITY;
    s->work_obj = calloc(total, sizeof(double));
    s->work_x = calloc(total, sizeof(double));
    s->work_dj = calloc(total, sizeof(double));

    s->work_rhs = calloc(m, sizeof(double));
    s->work_rhs[0] = rhs;
    s->work_sense = calloc(m, sizeof(char));
    s->work_sense[0] = sense;

    double v0 = a * lb, v1 = a * ub;
    s->min_activity = calloc(m, sizeof(double));
    s->max_activity = calloc(m, sizeof(double));
    s->min_activity[0] = (v0 < v1) ? v0 : v1;
    s->max_activity[0] = (v0 > v1) ? v0 : v1;

    /* CSR for step3 */
    s->csr_row_ptr = calloc(m + 1, sizeof(int64_t));
    s->csr_row_ptr[0] = 0;  s->csr_row_ptr[1] = 1;
    s->csr_col_idx = calloc(1, sizeof(int));
    s->csr_col_idx[0] = 0;
    s->csr_values = calloc(1, sizeof(double));
    s->csr_values[0] = a;

    /* CSC for step2 */
    s->csc_col_ptr = calloc(total + 1, sizeof(int64_t));
    s->csc_col_ptr[0] = 0;  s->csc_col_ptr[1] = 1;
    for (int i = 2; i <= total; i++) s->csc_col_ptr[i] = 1;
    s->csc_row_idx = calloc(1, sizeof(int));
    s->csc_row_idx[0] = 0;
    s->csc_values = calloc(1, sizeof(double));
    s->csc_values[0] = a;

    /* Pricing: mark var dirty (step2), mark constr dirty (step3) */
    s->pricing = cxf_pricing_create(n, 3);
    cxf_pricing_init(s->pricing, n, 1);
    cxf_pricing_init_constrs(s->pricing, m);
    s->pricing->current_level = 0;
    s->pricing->var_dirty[0] = 1;
    s->pricing->num_dirty = 1;
    s->pricing->constr_queue[0][0] = 0;
    s->pricing->constr_q_committed[0] = 1;
    s->pricing->constr_q_total[0] = 1;
    return s;
}

static void free_state(SolverState *s) {
    if (!s) return;
    cxf_basis_free(s->basis);
    cxf_pricing_free(s->pricing);
    free(s->work_lb);    free(s->work_ub);
    free(s->work_obj);   free(s->work_x);   free(s->work_dj);
    free(s->work_rhs);   free(s->work_sense);
    free(s->min_activity); free(s->max_activity);
    free(s->csr_row_ptr);  free(s->csr_col_idx);  free(s->csr_values);
    free(s->csc_col_ptr);  free(s->csc_row_idx);  free(s->csc_values);
    free(s);
}

void setUp(void) {}
void tearDown(void) {}

/**
 * step3 (constraint elimination): small range*coeff → row eliminated.
 * coeff=0.01, range=1e-8, |0.01*1e-8|=1e-10 < 1e-6 → eligible.
 * Variable fixed at lb (positive coeff, <= constraint).
 */
void test_step3_small_coeff_processed(void) {
    CxfEnv env = {0};
    env.feasibility_tol = 1e-6;
    SolverState *s = make_state(0.01, '<', 0.0, 5.0, 5.0 + 1e-8);
    int rc = cxf_simplex_step3(s, &env);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_INT(-2, s->basis->basic_vars[0]); /* eliminated */
    free_state(s);
}

/**
 * step2: coefficient 1e-10 (between 1e-13 and 1e-9).
 * Same setup: 1e-10 * x <= 5e-10, x in [0, 100].
 * Old code skipped; now should tighten ub to ~5.
 */
void test_step2_small_coeff_processed(void) {
    CxfEnv env = {0};
    env.feasibility_tol = 1e-8;
    double a = 1e-10;
    SolverState *s = make_state(a, '<', 5e-10, 0.0, 100.0);
    double orig_ub = s->work_ub[0];
    int rc = cxf_simplex_step2(s, &env);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT(s->work_ub[0] < orig_ub - 1.0);
    TEST_ASSERT_DOUBLE_WITHIN(1e-4, 5.0, s->work_ub[0]);
    free_state(s);
}

/**
 * step3: large range*coeff → NOT eliminated.
 * coeff=2.0, range=100, |2*100|=200 >= 1e-6 → not eligible.
 */
void test_step3_below_min_pivot_skipped(void) {
    CxfEnv env = {0};
    env.feasibility_tol = 1e-6;
    SolverState *s = make_state(2.0, '<', 0.0, 0.0, 100.0);
    int rc = cxf_simplex_step3(s, &env);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_NOT_EQUAL(-2, s->basis->basic_vars[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 100.0, s->work_ub[0]); /* unchanged */
    free_state(s);
}

/**
 * step2: coefficient below CXF_MIN_PIVOT (1e-14) skipped.
 */
void test_step2_below_min_pivot_skipped(void) {
    CxfEnv env = {0};
    env.feasibility_tol = 1e-8;
    double a = 1e-14;
    SolverState *s = make_state(a, '<', 1e-14, 0.0, 100.0);
    int rc = cxf_simplex_step2(s, &env);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 100.0, s->work_ub[0]);
    free_state(s);
}

/**
 * step3: returns 0 always (never CXF_INFEASIBLE).
 */
void test_step3_at_old_threshold_still_works(void) {
    CxfEnv env = {0};
    env.feasibility_tol = 1e-6;
    SolverState *s = make_state(2.0, '<', 10.0, 0.0, 100.0);
    int rc = cxf_simplex_step3(s, &env);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_NOT_EQUAL(CXF_INFEASIBLE, rc);
    free_state(s);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_step3_small_coeff_processed);
    RUN_TEST(test_step2_small_coeff_processed);
    RUN_TEST(test_step3_below_min_pivot_skipped);
    RUN_TEST(test_step2_below_min_pivot_skipped);
    RUN_TEST(test_step3_at_old_threshold_still_works);
    return UNITY_END();
}
