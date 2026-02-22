/**
 * @file update_var.c
 * @brief V2 producer: mark constraints dirty after variable change (P4.2)
 *
 * After a variable enters the basis (or changes status), traverse its
 * CSC column to find all structurally adjacent constraints and insert
 * them into the V2 constraint queues at levels 1-2.
 *
 * Spec: pricing_core.md — cxf_pricing_update_var
 * Beads: 95ny
 */

#include "convexfeld/cxf_pricing.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_types.h"

/* V2 queue insertion helper (from queue_insert.c) */
extern void v2_insert_constr(PricingState *ctx, int constr_idx);

/**
 * @brief Mark structurally adjacent constraints dirty after variable change.
 *
 * Traverses the CSC column for varIndex, inserting each row index into
 * the V2 constraint queues at levels 1 and 2 with flag-based duplicate
 * prevention. Also maintains V1 dirty flags for backward compatibility.
 *
 * @param ctx   PricingState with V2 queue arrays
 * @param state SolverState with CSC matrix and work counter
 * @param varIndex Variable that changed (entering variable)
 */
void cxf_pricing_update_var(PricingState *ctx, SolverState *state,
                            int varIndex) {
    if (ctx == NULL || state == NULL) return;
    if (varIndex < 0) return;

    int n = state->num_vars;

    /* Matrix mode: traverse CSC column for structural variables */
    if (varIndex < n && state->csc_col_ptr != NULL &&
        state->csc_row_idx != NULL) {
        int64_t start = state->csc_col_ptr[varIndex];
        int64_t end = state->csc_col_ptr[varIndex + 1];

        for (int64_t k = start; k < end; k++) {
            int row = state->csc_row_idx[k];
            if (row >= 0 && row < state->num_constrs)
                v2_insert_constr(ctx, row);
        }

        /* Work counter: proportional to column length */
        if (state->work_counter != NULL)
            *state->work_counter += (double)(end - start);

    } else if (varIndex >= n) {
        /* Auxiliary/slack variable: single diagonal entry */
        int row = varIndex - n;
        if (row >= 0 && row < state->num_constrs)
            v2_insert_constr(ctx, row);

        if (state->work_counter != NULL)
            *state->work_counter += 1.0;
    }

    /* Also maintain V1 dirty marking for backward compat */
    if (ctx->var_dirty != NULL && varIndex < ctx->num_vars &&
        !ctx->var_dirty[varIndex]) {
        ctx->var_dirty[varIndex] = 1;
        ctx->num_dirty++;
    }
}
