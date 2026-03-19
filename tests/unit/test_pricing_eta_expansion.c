/** @file test_pricing_eta_expansion.c
 *  @brief Tests for eta-mode pricing expansion in candidates.c */
#include "unity.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_pricing.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_basis.h"
#include <stdlib.h>
#include <string.h>

PricingState *cxf_pricing_create(int num_vars, int max_levels);
void cxf_pricing_free(PricingState *ctx);
int cxf_pricing_init(PricingState *ctx, int num_vars, int strategy);
int cxf_pricing_init_constrs(PricingState *ctx, int num_constrs);
extern BasisState *cxf_basis_create(int m, int n);
extern void cxf_basis_free(BasisState *basis);

static EtaVector *make_eta(int pivot_row, int entering, int leaving,
                           int nnz) {
    EtaVector *e = calloc(1, sizeof(EtaVector));
    e->type = CXF_ETA_PIVOT;
    e->pivot_row = pivot_row;
    e->entering_var = entering;
    e->leaving_var = leaving;
    e->nnz = nnz;
    e->next = NULL;
    return e;
}

static SolverState *create_state(int n, int m) {
    SolverState *s = calloc(1, sizeof(SolverState));
    s->num_vars = n;
    s->num_constrs = m;
    s->basis = cxf_basis_create(m, n + m);
    for (int j = 0; j < n; j++)
        s->basis->var_status[j] = CXF_VAR_AT_LOWER;
    for (int i = 0; i < m; i++) {
        s->basis->basic_vars[i] = n + i;
        s->basis->var_status[n + i] = i;
    }
    return s;
}

static void add_matrix(SolverState *s) { /* 3x4: R0={0,1}, R1={0,2}, R2={3} */
    s->csc_col_ptr = malloc(5 * sizeof(int64_t));
    s->csc_col_ptr[0] = 0; s->csc_col_ptr[1] = 2;
    s->csc_col_ptr[2] = 3; s->csc_col_ptr[3] = 4;
    s->csc_col_ptr[4] = 5;
    s->csc_row_idx = malloc(5 * sizeof(int));
    s->csc_row_idx[0] = 0; s->csc_row_idx[1] = 1;
    s->csc_row_idx[2] = 0; s->csc_row_idx[3] = 1;
    s->csc_row_idx[4] = 2;
    s->csc_values = calloc(5, sizeof(double));
    s->num_nonzeros = 5;
    s->csr_row_ptr = malloc(4 * sizeof(int64_t));
    s->csr_row_ptr[0] = 0; s->csr_row_ptr[1] = 2;
    s->csr_row_ptr[2] = 4; s->csr_row_ptr[3] = 5;
    s->csr_col_idx = malloc(5 * sizeof(int));
    s->csr_col_idx[0] = 0; s->csr_col_idx[1] = 1;
    s->csr_col_idx[2] = 0; s->csr_col_idx[3] = 2;
    s->csr_col_idx[4] = 3;
    s->csr_values = calloc(5, sizeof(double));
}

static void free_state(SolverState *s) {
    if (!s) return;
    if (s->basis) {
        EtaVector *e = s->basis->eta_head;
        s->basis->eta_head = NULL;
        s->basis->eta_count = 0;
        while (e) { EtaVector *nx = e->next; free(e); e = nx; }
    }
    cxf_basis_free(s->basis);
    free(s->csc_col_ptr); free(s->csc_row_idx); free(s->csc_values);
    free(s->csr_row_ptr); free(s->csr_col_idx); free(s->csr_values);
    free(s);
}

void setUp(void) {}
void tearDown(void) {}

void test_eta_expansion_adds_dynamic_neighbors(void) {
    SolverState *s = create_state(4, 3);
    add_matrix(s);
    /* Eta pivot_row=2, entering=1, leaving=3. Row 2 CSR has col 3 only. */
    EtaVector *eta = make_eta(2, 1, 3, 1);
    s->basis->eta_head = eta;
    s->basis->eta_count = 1;

    PricingState *ctx = cxf_pricing_create(4, 3);
    cxf_pricing_init(ctx, 4, 1);
    cxf_pricing_init_constrs(ctx, 3);
    ctx->current_level = 1;
    ctx->cached_var_count[1] = -1;
    v2_insert_constr(ctx, 2);

    int count = 0; int *cands = NULL;
    cxf_pricing_candidates(ctx, s, &count, &cands);

    int found1 = 0, found3 = 0;
    for (int i = 0; i < count; i++) {
        if (cands[i] == 1) found1 = 1;
        if (cands[i] == 3) found3 = 1;
    }
    TEST_ASSERT_TRUE_MESSAGE(found3, "CSR neighbor col 3 missing");
    TEST_ASSERT_TRUE_MESSAGE(found1, "Eta entering var 1 missing");

    cxf_pricing_free(ctx);
    free_state(s);
}

void test_eta_cost_increases_estimate(void) {
    /* Many etas * constraints triggers full scan. */
    SolverState *s = create_state(4, 3);
    add_matrix(s);
    EtaVector *head = NULL;
    for (int i = 0; i < 10; i++) {
        EtaVector *e = make_eta(0, 1, 2, 5);
        e->next = head; head = e;
    }
    s->basis->eta_head = head;
    s->basis->eta_count = 10;

    PricingState *ctx = cxf_pricing_create(4, 3);
    cxf_pricing_init(ctx, 4, 1);
    cxf_pricing_init_constrs(ctx, 3);
    ctx->current_level = 1;
    ctx->cached_var_count[1] = -1;
    v2_insert_constr(ctx, 0);
    v2_insert_constr(ctx, 1);
    v2_insert_constr(ctx, 2);

    int count = 0; int *cands = NULL;
    cxf_pricing_candidates(ctx, s, &count, &cands);
    TEST_ASSERT_EQUAL_INT(4, count); /* Full scan: all nonbasic */

    cxf_pricing_free(ctx);
    free_state(s);
}

void test_null_basis_no_crash(void) {
    SolverState *s = calloc(1, sizeof(SolverState));
    s->num_vars = 4; s->num_constrs = 2;
    s->basis = NULL; /* No basis at all */
    s->csc_col_ptr = malloc(5 * sizeof(int64_t));
    memset(s->csc_col_ptr, 0, 5 * sizeof(int64_t));

    PricingState *ctx = cxf_pricing_create(4, 3);
    cxf_pricing_init(ctx, 4, 1);
    cxf_pricing_init_constrs(ctx, 2);
    ctx->current_level = 1;
    ctx->cached_var_count[1] = -1;

    int count = 0; int *cands = NULL;
    cxf_pricing_candidates(ctx, s, &count, &cands);
    TEST_ASSERT_TRUE(count >= 0);

    cxf_pricing_free(ctx);
    free(s->csc_col_ptr);
    free(s);
}

void test_empty_eta_list_csr_only(void) {
    SolverState *s = create_state(4, 3);
    add_matrix(s);
    s->basis->eta_head = NULL;
    s->basis->eta_count = 0;

    PricingState *ctx = cxf_pricing_create(4, 3);
    cxf_pricing_init(ctx, 4, 1);
    cxf_pricing_init_constrs(ctx, 3);
    ctx->current_level = 1;
    ctx->cached_var_count[1] = -1;
    v2_insert_constr(ctx, 2);
    int count = 0; int *cands = NULL;
    cxf_pricing_candidates(ctx, s, &count, &cands);
    TEST_ASSERT_EQUAL_INT(1, count); /* Only CSR col 3 */
    TEST_ASSERT_EQUAL_INT(3, cands[0]);

    cxf_pricing_free(ctx);
    free_state(s);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_eta_expansion_adds_dynamic_neighbors);
    RUN_TEST(test_eta_cost_increases_estimate);
    RUN_TEST(test_null_basis_no_crash);
    RUN_TEST(test_empty_eta_list_csr_only);
    return UNITY_END();
}
