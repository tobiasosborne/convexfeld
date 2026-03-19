/**
 * @file update_var.c
 * @brief V2 producer: mark constraints dirty after variable change (P4.2)
 *
 * After a variable enters the basis (or changes status), traverse its
 * CSC column to find all structurally adjacent constraints and insert
 * them into the V2 constraint queues at levels 1-2. Then expand via
 * eta vectors for dynamic neighbors from recent pivots.
 *
 * Spec: pricing_core.md — cxf_pricing_update_var
 * Beads: 95ny
 */

#include "convexfeld/cxf_pricing.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_basis.h"

#include "pricing_internal.h"

/**
 * @brief Mark structurally adjacent constraints dirty after variable change.
 *
 * Traverses the CSC column for varIndex, inserting each row index into
 * the V2 constraint queues at levels 1 and 2 with flag-based duplicate
 * prevention. Then expands via eta vectors to capture dynamic neighbors
 * from recent pivots. Also maintains V1 dirty flags for backward compat.
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
    int m = state->num_constrs;

    /* Matrix mode: traverse CSC column for structural variables */
    if (varIndex < n && state->csc_col_ptr != NULL &&
        state->csc_row_idx != NULL) {
        int64_t start = state->csc_col_ptr[varIndex];
        int64_t end = state->csc_col_ptr[varIndex + 1];

        for (int64_t k = start; k < end; k++) {
            int row = state->csc_row_idx[k];
            if (row >= 0 && row < m)
                v2_insert_constr(ctx, row);
        }

        if (state->work_counter != NULL)
            *state->work_counter += (double)(end - start);

    } else if (varIndex >= n) {
        /* Auxiliary/slack variable: single diagonal entry */
        int row = varIndex - n;
        if (row >= 0 && row < m)
            v2_insert_constr(ctx, row);

        if (state->work_counter != NULL)
            *state->work_counter += 1.0;
    }

    /* Eta-mode expansion: traverse eta linked list for dynamic neighbors */
    if (state->basis && state->basis->eta_head) {
        int eta_work = 0;
        EtaVector *eta = state->basis->eta_head;
        while (eta != NULL) {
            if (eta->type == CXF_ETA_PIVOT &&
                (eta->entering_var == varIndex ||
                 eta->leaving_var == varIndex)) {
                /* Pivot row is a neighbor */
                if (eta->pivot_row >= 0 && eta->pivot_row < m)
                    v2_insert_constr(ctx, eta->pivot_row);
                /* All rows in eta vector are neighbors */
                for (int i = 0; i < eta->nnz; i++) {
                    int row = eta->indices[i];
                    if (row >= 0 && row < m)
                        v2_insert_constr(ctx, row);
                }
            }
            eta_work++;
            eta = eta->next;
        }
        if (state->work_counter != NULL)
            *state->work_counter += (double)eta_work;
    }

    /* Also maintain V1 dirty marking for backward compat */
    if (ctx->var_dirty != NULL && varIndex < ctx->num_vars &&
        !ctx->var_dirty[varIndex]) {
        ctx->var_dirty[varIndex] = 1;
        ctx->num_dirty++;
    }
}
