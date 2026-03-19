/**
 * @file test_step3_implied_bound.c
 * @brief Tests for step3 constraint elimination (T1.2 rewrite).
 *
 * Validates the constraint elimination algorithm:
 *   - Pre-screening: constraints with large range*coeff are skipped
 *   - Row elimination: basic_vars[row] = -2 after elimination
 *   - Variable fixing: nonbasic vars fixed at lb or ub by coeff sign
 *   - Equality constraints: never eliminated
 */

#include "unity.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_pricing.h"
#include <stdlib.h>
#include <string.h>

extern BasisState *cxf_basis_create(int m, int n);
extern void cxf_basis_free(BasisState *basis);
extern PricingState *cxf_pricing_create(int num_vars, int max_levels);
extern void cxf_pricing_free(PricingState *ctx);
extern int cxf_pricing_init(PricingState *ctx, int num_vars, int strategy);
extern int cxf_pricing_init_constrs(PricingState *ctx, int num_constrs);
extern int cxf_simplex_step3(SolverState *state, CxfEnv *env);

/* Build a 1-constraint, 1-variable state for constraint elimination tests. */
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
    free(s->work_obj);   free(s->work_x);   free(s->work_dj);
    free(s->work_rhs);   free(s->work_sense);
    free(s->csr_row_ptr);  free(s->csr_col_idx);  free(s->csr_values);
    free(s->csc_col_ptr);  free(s->csc_row_idx);  free(s->csc_values);
    free(s);
}

void setUp(void) {}
void tearDown(void) {}

/* Pre-screen: small range*coeff → constraint is eliminated */
void test_small_range_eliminated(void) {
    CxfEnv env = {0};
    env.feasibility_tol = 1e-6;
    /* coeff=0.1, range=1e-8 → |0.1*1e-8| = 1e-9 < 1e-6 → eligible */
    SolverState *s = make_state(0.1, '<', 0.0, 5.0, 5.0 + 1e-8);
    int rc = cxf_simplex_step3(s, &env);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_EQUAL_INT(-2, s->basis->basic_vars[0]); /* eliminated */
    free_state(s);
}

/* Pre-screen: large range*coeff → constraint NOT eliminated */
void test_large_range_not_eliminated(void) {
    CxfEnv env = {0};
    env.feasibility_tol = 1e-6;
    /* coeff=2.0, range=100 → |2.0*100| = 200 >= 1e-6 → not eligible */
    SolverState *s = make_state(2.0, '<', 10.0, 0.0, 100.0);
    int rc = cxf_simplex_step3(s, &env);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_NOT_EQUAL(-2, s->basis->basic_vars[0]); /* NOT eliminated */
    free_state(s);
}

/* Equality constraints are never eliminated */
void test_equality_not_eliminated(void) {
    CxfEnv env = {0};
    env.feasibility_tol = 1e-6;
    /* Small range but equality → skip */
    SolverState *s = make_state(0.1, '=', 0.0, 5.0, 5.0 + 1e-8);
    int rc = cxf_simplex_step3(s, &env);
    TEST_ASSERT_EQUAL_INT(0, rc);
    TEST_ASSERT_NOT_EQUAL(-2, s->basis->basic_vars[0]);
    free_state(s);
}

/* Positive coeff in <= → variable fixed at lb */
void test_leq_positive_fixes_at_lb(void) {
    CxfEnv env = {0};
    env.feasibility_tol = 1e-6;
    SolverState *s = make_state(0.01, '<', 0.0, 3.0, 3.0 + 1e-8);
    int rc = cxf_simplex_step3(s, &env);
    TEST_ASSERT_EQUAL_INT(0, rc);
    /* Variable should be fixed at lb=3.0 (positive coeff in <=) */
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 3.0, s->work_lb[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 3.0, s->work_ub[0]);
    free_state(s);
}

/* Never returns CXF_INFEASIBLE (T1.2: fabricated infeasibility removed) */
void test_never_returns_infeasible(void) {
    CxfEnv env = {0};
    env.feasibility_tol = 1e-6;
    SolverState *s = make_state(2.0, '<', 10.0, 0.0, 100.0);
    int rc = cxf_simplex_step3(s, &env);
    TEST_ASSERT_NOT_EQUAL(CXF_INFEASIBLE, rc);
    free_state(s);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_small_range_eliminated);
    RUN_TEST(test_large_range_not_eliminated);
    RUN_TEST(test_equality_not_eliminated);
    RUN_TEST(test_leq_positive_fixes_at_lb);
    RUN_TEST(test_never_returns_infeasible);
    return UNITY_END();
}
