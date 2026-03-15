/**
 * @file test_step3_implied_bound.c
 * @brief Tests for step3.c implied bound formula (Savelsbergh 1994).
 *
 * Validates cxf_simplex_step3 uses: x_k <= l_k + (b_i - MinAct) / a_ik
 * The previous code omitted the RHS term, computing: l_k - MinAct / a_ik
 *
 * Beads: convexfeld-cm5w
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
extern int cxf_simplex_step3(SolverState *state, CxfEnv *env);

/**
 * Build a 1-constraint, 1-variable state. Activity bounds are computed
 * consistently from variable bounds to avoid triggering infeasibility
 * recomputation.
 */
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

    s->work_rhs = calloc(m, sizeof(double));
    s->work_rhs[0] = rhs;
    s->work_sense = calloc(m, sizeof(char));
    s->work_sense[0] = sense;

    double v0 = a * lb, v1 = a * ub;
    s->min_activity = calloc(m, sizeof(double));
    s->max_activity = calloc(m, sizeof(double));
    s->min_activity[0] = (v0 < v1) ? v0 : v1;
    s->max_activity[0] = (v0 > v1) ? v0 : v1;

    s->csr_row_ptr = calloc(m + 1, sizeof(int64_t));
    s->csr_row_ptr[0] = 0;  s->csr_row_ptr[1] = 1;
    s->csr_col_idx = calloc(1, sizeof(int));
    s->csr_col_idx[0] = 0;
    s->csr_values = calloc(1, sizeof(double));
    s->csr_values[0] = a;

    s->csc_col_ptr = calloc(total + 1, sizeof(int64_t));
    s->csc_col_ptr[0] = 0;  s->csc_col_ptr[1] = 1;
    for (int i = 2; i <= total; i++) s->csc_col_ptr[i] = 1;
    s->csc_row_idx = calloc(1, sizeof(int));
    s->csc_row_idx[0] = 0;
    s->csc_values = calloc(1, sizeof(double));
    s->csc_values[0] = a;

    s->pricing = cxf_pricing_create(n, 3);
    cxf_pricing_init(s->pricing, n, 1);
    cxf_pricing_init_constrs(s->pricing, m);
    s->pricing->current_level = 0;
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
    free(s->work_rhs);   free(s->work_sense);
    free(s->min_activity); free(s->max_activity);
    free(s->csr_row_ptr);  free(s->csr_col_idx);  free(s->csr_values);
    free(s->csc_col_ptr);  free(s->csc_row_idx);  free(s->csc_values);
    free(s);
}

void setUp(void) {}
void tearDown(void) {}

/**
 * 2*x <= 10, x in [0,100]. Acts: min=0, max=200.
 * New: impl_ub = 0 + (10-0)/2 = 5. Old: 0 - 0/2 = 0 (wrong).
 */
void test_leq_positive_coeff_ub_tightened(void) {
    CxfEnv env = {0};
    env.feasibility_tol = 1e-8;
    SolverState *s = make_state(2.0, '<', 10.0, 0.0, 100.0);
    int rc = cxf_simplex_step3(s, &env);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 5.0, s->work_ub[0]);
    free_state(s);
}

/**
 * 4*x >= 20, x in [0,100]. Acts: min=0, max=400.
 * New: impl_lb = 100 + (20-400)/4 = 5. Old: 100 - 400/4 = 0 (wrong).
 */
void test_geq_positive_coeff_lb_tightened(void) {
    CxfEnv env = {0};
    env.feasibility_tol = 1e-8;
    SolverState *s = make_state(4.0, '>', 20.0, 0.0, 100.0);
    int rc = cxf_simplex_step3(s, &env);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 5.0, s->work_lb[0]);
    free_state(s);
}

/**
 * 5*x <= 25, x in [0,100]. Acts: min=0, max=500.
 * New: impl_ub = 0 + (25-0)/5 = 5. Old: 0 - 0/5 = 0 (wrong).
 */
void test_nonzero_rhs_discriminates(void) {
    CxfEnv env = {0};
    env.feasibility_tol = 1e-8;
    SolverState *s = make_state(5.0, '<', 25.0, 0.0, 100.0);
    int rc = cxf_simplex_step3(s, &env);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 5.0, s->work_ub[0]);
    free_state(s);
}

/**
 * 3*x <= -6, x in [-100,100]. Acts: min=-300, max=300.
 * New: impl_ub = -100 + (-6-(-300))/3 = -2. Old: -100 - (-300)/3 = 0 (wrong).
 */
void test_negative_rhs(void) {
    CxfEnv env = {0};
    env.feasibility_tol = 1e-8;
    SolverState *s = make_state(3.0, '<', -6.0, -100.0, 100.0);
    int rc = cxf_simplex_step3(s, &env);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, -2.0, s->work_ub[0]);
    free_state(s);
}

/**
 * 5*x <= 0, x in [-10,10]. Acts: min=-50, max=50.
 * New: -10 + (0-(-50))/5 = 0. Old: -10 - (-50)/5 = 0. Same (rhs=0).
 */
void test_zero_rhs_both_agree(void) {
    CxfEnv env = {0};
    env.feasibility_tol = 1e-8;
    SolverState *s = make_state(5.0, '<', 0.0, -10.0, 10.0);
    int rc = cxf_simplex_step3(s, &env);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 0.0, s->work_ub[0]);
    free_state(s);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_leq_positive_coeff_ub_tightened);
    RUN_TEST(test_geq_positive_coeff_lb_tightened);
    RUN_TEST(test_nonzero_rhs_discriminates);
    RUN_TEST(test_negative_rhs);
    RUN_TEST(test_zero_rhs_both_agree);
    return UNITY_END();
}
