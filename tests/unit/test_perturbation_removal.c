/** @file test_perturbation_removal.c
 *  @brief T2.2: pricing pool removal — Case A/B exclusion, filter, unperturb. */
#include "unity.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_pricing.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_basis.h"
#include <stdlib.h>
#include <string.h>

/* Flag bit that T2.2 implementation will add to pricing_internal.h */
#define PRICING_EXCLUDED 0x20

PricingState *cxf_pricing_create(int num_vars, int max_levels);
void cxf_pricing_free(PricingState *ctx);
int cxf_pricing_init(PricingState *ctx, int num_vars, int strategy);
int cxf_pricing_init_constrs(PricingState *ctx, int num_constrs);
extern BasisState *cxf_basis_create(int m, int n);
extern void cxf_basis_free(BasisState *basis);
extern int cxf_simplex_perturbation(SolverState *state, CxfEnv *env);
extern int cxf_simplex_unperturb(SolverState *state, CxfEnv *env);
extern int cxf_loadenv(CxfEnv **envP, const char *logfilename);
extern int cxf_freeenv(CxfEnv *env);

static CxfEnv *g_env;

void setUp(void) { cxf_loadenv(&g_env, NULL); }
void tearDown(void) { cxf_freeenv(g_env); g_env = NULL; }

/* Minimal solver state for perturbation tests (n vars, m constrs). */
static SolverState *make_state(int n, int m) {
    int total = n + m;
    SolverState *s = calloc(1, sizeof(SolverState));
    s->num_vars = n;
    s->num_constrs = m;
    s->basis = cxf_basis_create(m, total);
    s->work_lb = calloc((size_t)total, sizeof(double));
    s->work_ub = calloc((size_t)total, sizeof(double));
    s->work_dj = calloc((size_t)total, sizeof(double));
    s->work_x  = calloc((size_t)total, sizeof(double));
    s->work_obj = calloc((size_t)total, sizeof(double));
    s->saved_lb = calloc((size_t)total, sizeof(double));
    s->saved_ub = calloc((size_t)total, sizeof(double));
    for (int j = 0; j < total; j++) {
        s->work_lb[j] = 0.0;
        s->work_ub[j] = 10.0;
        s->saved_lb[j] = 0.0;
        s->saved_ub[j] = 10.0;
    }
    /* Default: structurals nonbasic at lower, slacks basic */
    for (int j = 0; j < n; j++)
        s->basis->var_status[j] = CXF_VAR_AT_LOWER;
    for (int i = 0; i < m; i++) {
        s->basis->basic_vars[i] = n + i;
        s->basis->var_status[n + i] = i;
    }
    /* Pricing context with var_flags */
    PricingState *p = cxf_pricing_create(n, 3);
    cxf_pricing_init(p, n, 1);
    cxf_pricing_init_constrs(p, m);
    s->pricing = p;
    return s;
}

static void free_state(SolverState *s) {
    if (!s) return;
    cxf_pricing_free(s->pricing);
    cxf_basis_free(s->basis);
    free(s->work_lb); free(s->work_ub);
    free(s->work_dj); free(s->work_x);
    free(s->work_obj);
    free(s->saved_lb); free(s->saved_ub);
    free(s->csr_row_ptr); free(s->csr_col_idx); free(s->csr_values);
    free(s->csc_col_ptr); free(s->csc_row_idx); free(s->csc_values);
    free(s);
}

/* T2.2-1: Case A -- nonbasic degenerate var gets PRICING_EXCLUDED */
void test_case_a_nonbasic_excluded(void) {
    SolverState *s = make_state(3, 1);
    /* Var 0: degenerate (near-zero negative RC at lower) */
    s->work_dj[0] = -1e-9;
    /* Var 1: not degenerate (large positive RC) */
    s->work_dj[1] = 5.0;
    s->iteration = 1;

    cxf_simplex_perturbation(s, g_env);

    uint8_t *vf = s->pricing->var_flags;
    TEST_ASSERT_NOT_NULL(vf);
    /* Degenerate var 0 should have exclusion bit set */
    TEST_ASSERT_TRUE_MESSAGE(vf[0] & PRICING_EXCLUDED,
        "Case A: degenerate var should have PRICING_EXCLUDED");
    /* Non-degenerate var 1 should NOT have bit set */
    TEST_ASSERT_FALSE_MESSAGE(vf[1] & PRICING_EXCLUDED,
        "Non-degenerate var should not be excluded");
    free_state(s);
}

/* T2.2-2: Case B -- basic degenerate strips entire row */
void test_case_b_row_stripping(void) {
    SolverState *s = make_state(4, 2);
    /* Make var 0 basic in row 0 */
    s->basis->var_status[0] = 0;
    s->basis->basic_vars[0] = 0;
    s->basis->var_status[4] = CXF_VAR_AT_LOWER;

    /* CSR: row 0 has cols {1, 2, 3} */
    s->csr_row_ptr = calloc(3, sizeof(int64_t));
    s->csr_row_ptr[0] = 0; s->csr_row_ptr[1] = 3; s->csr_row_ptr[2] = 3;
    s->csr_col_idx = calloc(3, sizeof(int));
    s->csr_col_idx[0] = 1; s->csr_col_idx[1] = 2; s->csr_col_idx[2] = 3;
    s->csr_values = calloc(3, sizeof(double));
    s->csr_values[0] = 1.0; s->csr_values[1] = 1.0; s->csr_values[2] = 1.0;
    s->iteration = 1;

    cxf_simplex_perturbation(s, g_env);

    uint8_t *vf = s->pricing->var_flags;
    TEST_ASSERT_NOT_NULL(vf);
    /* All columns in row 0 should be excluded */
    TEST_ASSERT_TRUE_MESSAGE(vf[1] & PRICING_EXCLUDED,
        "Row-strip: col 1 should be excluded");
    TEST_ASSERT_TRUE_MESSAGE(vf[2] & PRICING_EXCLUDED,
        "Row-strip: col 2 should be excluded");
    TEST_ASSERT_TRUE_MESSAGE(vf[3] & PRICING_EXCLUDED,
        "Row-strip: col 3 should be excluded");
    free_state(s);
}

/* T2.2-3: Pricing scan skips PRICING_EXCLUDED variables */
void test_pricing_filter_excluded(void) {
    SolverState *s = make_state(4, 2);
    /* All 4 vars nonbasic at lower (eligible for pricing) */
    s->pricing->var_flags[1] |= PRICING_EXCLUDED;
    s->pricing->var_flags[3] |= PRICING_EXCLUDED;

    /* Force full scan at level 1 */
    s->pricing->current_level = 1;
    s->pricing->cached_var_count[1] = -1;

    int count = 0; int *cands = NULL;
    cxf_pricing_candidates(s->pricing, s, &count, &cands);

    for (int i = 0; i < count; i++) {
        TEST_ASSERT_FALSE_MESSAGE(cands[i] == 1,
            "Excluded var 1 should not appear in candidates");
        TEST_ASSERT_FALSE_MESSAGE(cands[i] == 3,
            "Excluded var 3 should not appear in candidates");
    }
    free_state(s);
}

/* T2.2-4: Unperturb clears all PRICING_EXCLUDED bits */
void test_unperturb_clears_excluded(void) {
    SolverState *s = make_state(4, 2);
    s->pricing->var_flags[0] |= PRICING_EXCLUDED;
    s->pricing->var_flags[2] |= PRICING_EXCLUDED;
    s->pricing->var_flags[3] |= PRICING_EXCLUDED;
    s->perturb_count = 5;

    cxf_simplex_unperturb(s, g_env);

    uint8_t *vf = s->pricing->var_flags;
    for (int j = 0; j < 4; j++) {
        TEST_ASSERT_FALSE_MESSAGE(vf[j] & PRICING_EXCLUDED,
            "Unperturb should clear all PRICING_EXCLUDED bits");
    }
    free_state(s);
}

/* T2.2-5: NULL pricing state does not crash */
void test_null_pricing_no_crash(void) {
    SolverState *s = make_state(2, 1);
    cxf_pricing_free(s->pricing);
    s->pricing = NULL;
    s->work_dj[0] = -1e-9;
    s->iteration = 1;

    int rc = cxf_simplex_perturbation(s, g_env);
    TEST_ASSERT_TRUE(rc == CXF_OK || rc == CXF_INFEASIBLE);

    s->perturb_count = 1;
    rc = cxf_simplex_unperturb(s, g_env);
    TEST_ASSERT_TRUE(rc == CXF_OK || rc == 1);
    free_state(s);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_case_a_nonbasic_excluded);
    RUN_TEST(test_case_b_row_stripping);
    RUN_TEST(test_pricing_filter_excluded);
    RUN_TEST(test_unperturb_clears_excluded);
    RUN_TEST(test_null_pricing_no_crash);
    return UNITY_END();
}
