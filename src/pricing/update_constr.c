/**
 * @file update_constr.c
 * @brief V2 producer: mark variables dirty after constraint change (P4.3)
 *
 * After a constraint leaves the basis (or changes status), traverse its
 * CSR row to find all structurally adjacent variables and insert them
 * into the V2 variable queues at levels 1-2.
 *
 * Spec: pricing_core.md — cxf_pricing_update_constr
 * Beads: bjy8
 */

#include "convexfeld/cxf_pricing.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_types.h"

#include "pricing_internal.h"

/* V2 queue insertion helper (from queue_insert.c) */

/**
 * @brief Mark structurally adjacent variables dirty after constraint change.
 *
 * Traverses the CSR row for constrIndex, inserting each column index into
 * the V2 variable queues at levels 1 and 2 with flag-based duplicate
 * prevention. Also maintains V1 dirty flags for backward compatibility.
 *
 * @param ctx   PricingState with V2 queue arrays
 * @param state SolverState with CSR matrix and work counter
 * @param constrIndex Constraint that changed (leaving row)
 */
void cxf_pricing_update_constr(PricingState *ctx, SolverState *state,
                               int constrIndex) {
    if (ctx == NULL || state == NULL) return;
    if (constrIndex < 0 || constrIndex >= state->num_constrs) return;

    /* Matrix mode: traverse CSR row for structural adjacency */
    if (state->csr_row_ptr != NULL && state->csr_col_idx != NULL) {
        int64_t start = state->csr_row_ptr[constrIndex];
        int64_t end = state->csr_row_ptr[constrIndex + 1];

        for (int64_t k = start; k < end; k++) {
            int col = state->csr_col_idx[k];
            if (col >= 0 && col < state->num_vars)
                v2_insert_var(ctx, col);
        }

        /* Work counter: proportional to row length */
        if (state->work_counter != NULL)
            *state->work_counter += (double)(end - start);
    } else if (state->csc_col_ptr != NULL) {
        /* Fallback: scan all CSC columns for entries in this row */
        int n = state->num_vars;
        for (int j = 0; j < n; j++) {
            int64_t cs = state->csc_col_ptr[j];
            int64_t ce = state->csc_col_ptr[j + 1];
            for (int64_t k = cs; k < ce; k++) {
                if (state->csc_row_idx[k] == constrIndex) {
                    v2_insert_var(ctx, j);
                    break;
                }
            }
        }

        if (state->work_counter != NULL)
            *state->work_counter += (double)state->num_nonzeros;
    }

    /* Also maintain V1 constraint dirty marking for backward compat */
    if (ctx->constr_dirty != NULL && constrIndex < ctx->num_constrs &&
        !ctx->constr_dirty[constrIndex]) {
        ctx->constr_dirty[constrIndex] = 1;
        ctx->num_constr_dirty++;
    }
}
