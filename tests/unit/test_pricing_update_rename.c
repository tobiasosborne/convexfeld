/**
 * @file test_pricing_update_rename.c
 * @brief Confirms cxf_pricing_update (V2 spec name) is callable and works.
 *
 * Spec: pricing_support.md — function is cxf_pricing_update, not
 *       cxf_pricing_update_queues. This test verifies the rename.
 *
 * Beads: convexfeld-5ox8
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
int cxf_pricing_init_constrs(PricingState *ctx, int num_constrs);
extern BasisState *cxf_basis_create(int m, int n);
extern void cxf_basis_free(BasisState *basis);

static SolverState *create_test_state(int n, int m) {
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

static void free_test_state(SolverState *s) {
    if (!s) return;
    cxf_basis_free(s->basis);
    free(s);
}

void setUp(void) {}
void tearDown(void) {}

/* Verify the renamed function exists and is callable via the V2 spec name. */
void test_cxf_pricing_update_exists(void) {
    int n = 3, m = 2;
    PricingState *ctx = cxf_pricing_create(n, 3);
    cxf_pricing_init(ctx, n, 1);
    cxf_pricing_init_constrs(ctx, m);
    SolverState *s = create_test_state(n, m);

    /* Populate L0 queue with one variable entry */
    ctx->var_queue[0][0] = 0;
    ctx->var_q_committed[0] = 1;
    ctx->var_q_total[0] = 1;
    ctx->current_level = 0;
    ctx->level_active[0] = 1;

    double work = 0.0;
    s->work_counter = &work;

    /* Call the V2 spec-named function */
    cxf_pricing_update(ctx, s);

    /* Function ran: work counter incremented, queue survived */
    TEST_ASSERT_EQUAL_DOUBLE(1.0, work);
    TEST_ASSERT_EQUAL_INT(1, ctx->var_q_committed[0]);

    cxf_pricing_free(ctx);
    free_test_state(s);
}

/* Null-safety: no crash on NULL inputs. */
void test_cxf_pricing_update_null_safety(void) {
    cxf_pricing_update(NULL, NULL);
    /* No crash = pass */
    TEST_PASS();
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_cxf_pricing_update_exists);
    RUN_TEST(test_cxf_pricing_update_null_safety);
    return UNITY_END();
}
