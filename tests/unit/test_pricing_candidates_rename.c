/**
 * @file test_pricing_candidates_rename.c
 * @brief Verify cxf_pricing_candidates has the V2 spec signature.
 *
 * After rename (convexfeld-o8ez): the spec name cxf_pricing_candidates
 * now belongs to the V2 adaptive retrieval (pricing_core.md P4.5).
 * V1 RC-scan lives under cxf_pricing_candidates_v1.
 */

#include "unity.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_pricing.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_basis.h"
#include <stdlib.h>
#include <string.h>

/* Forward declarations */
PricingState *cxf_pricing_create(int num_vars, int max_levels);
void cxf_pricing_free(PricingState *ctx);
int cxf_pricing_init(PricingState *ctx, int num_vars, int strategy);
extern BasisState *cxf_basis_create(int m, int n);
extern void cxf_basis_free(BasisState *basis);

void setUp(void) {}
void tearDown(void) {}

/*--- Helper: minimal solver state ---*/
static SolverState *make_state(int n, int m) {
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

static void free_state(SolverState *s) {
    if (!s) return;
    cxf_basis_free(s->basis);
    free(s);
}

/*==========================================================================
 * Tests: cxf_pricing_candidates has V2 (spec) signature
 *=========================================================================*/

void test_spec_name_level0_returns_queue(void) {
    PricingState *ctx = cxf_pricing_create(5, 3);
    cxf_pricing_init(ctx, 5, 1);
    SolverState *s = make_state(5, 3);

    ctx->var_queue[0][0] = 2;
    ctx->var_queue[0][1] = 4;
    ctx->var_q_committed[0] = 2;
    ctx->var_q_total[0] = 2;
    ctx->current_level = 0;

    int count = -1;
    int *cands = NULL;
    /* This is the V2 spec signature: (PricingState*, SolverState*,
     * int*, int**) returning void. */
    cxf_pricing_candidates(ctx, s, &count, &cands);

    TEST_ASSERT_EQUAL_INT(2, count);
    TEST_ASSERT_NOT_NULL(cands);
    TEST_ASSERT_EQUAL_INT(2, cands[0]);
    TEST_ASSERT_EQUAL_INT(4, cands[1]);

    cxf_pricing_free(ctx);
    free_state(s);
}

void test_spec_name_null_safety(void) {
    int count = 99;
    int *cands = (int *)1;
    cxf_pricing_candidates(NULL, NULL, &count, &cands);
    TEST_ASSERT_EQUAL_INT(0, count);
    TEST_ASSERT_NULL(cands);
}

void test_spec_name_cache_hit_path(void) {
    PricingState *ctx = cxf_pricing_create(5, 3);
    cxf_pricing_init(ctx, 5, 1);
    SolverState *s = make_state(5, 3);

    ctx->current_level = 1;
    ctx->cached_var_count[1] = 2;
    ctx->var_output_buf[1][0] = 1;
    ctx->var_output_buf[1][1] = 3;

    int count = -1;
    int *cands = NULL;
    cxf_pricing_candidates(ctx, s, &count, &cands);

    TEST_ASSERT_EQUAL_INT(2, count);
    TEST_ASSERT_EQUAL_PTR(ctx->var_output_buf[1], cands);

    cxf_pricing_free(ctx);
    free_state(s);
}

/*==========================================================================
 * Main
 *=========================================================================*/

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_spec_name_level0_returns_queue);
    RUN_TEST(test_spec_name_null_safety);
    RUN_TEST(test_spec_name_cache_hit_path);
    return UNITY_END();
}
