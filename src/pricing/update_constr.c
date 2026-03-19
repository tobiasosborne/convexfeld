/**
 * @file update_constr.c
 * @brief V2 producer: mark variables dirty after constraint change (P4.3)
 *
 * After a constraint leaves the basis (or changes status), traverse its
 * CSR row to find all structurally adjacent variables and insert them
 * into the V2 variable queues at levels 1-2. Then expand via eta vectors
 * for dynamic neighbors from recent pivots.
 *
 * Spec: pricing_core.md — cxf_pricing_update_constr
 * Beads: bjy8
 */

#include "convexfeld/cxf_pricing.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_basis.h"

#include "pricing_internal.h"

/**
 * @brief Mark structurally adjacent variables dirty after constraint change.
 *
 * Traverses the CSR row for constrIndex, inserting each column index into
 * the V2 variable queues at levels 1 and 2 with flag-based duplicate
 * prevention. Then expands via eta vectors to capture dynamic neighbors
 * from recent pivots. Also maintains V1 dirty flags for backward compat.
 *
 * @param ctx   PricingState with V2 queue arrays
 * @param state SolverState with CSR matrix and work counter
 * @param constrIndex Constraint that changed (leaving row)
 */
void cxf_pricing_update_constr(PricingState *ctx, SolverState *state,
                               int constrIndex) {
    if (ctx == NULL || state == NULL) return;
    if (constrIndex < 0 || constrIndex >= state->num_constrs) return;

    int n = state->num_vars;

    /* Matrix mode: traverse CSR row for structural adjacency */
    if (state->csr_row_ptr != NULL && state->csr_col_idx != NULL) {
        int64_t start = state->csr_row_ptr[constrIndex];
        int64_t end = state->csr_row_ptr[constrIndex + 1];

        for (int64_t k = start; k < end; k++) {
            int col = state->csr_col_idx[k];
            if (col >= 0 && col < n)
                v2_insert_var(ctx, col);
        }

        if (state->work_counter != NULL)
            *state->work_counter += (double)(end - start);
    } else if (state->csc_col_ptr != NULL) {
        /* Fallback: scan all CSC columns for entries in this row */
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

    /* Eta-mode expansion: traverse eta linked list for dynamic neighbors */
    if (state->basis && state->basis->eta_head) {
        int eta_work = 0;
        EtaVector *eta = state->basis->eta_head;
        while (eta != NULL) {
            if (eta->type == CXF_ETA_PIVOT) {
                int match = (eta->pivot_row == constrIndex);
                /* Also check if constrIndex appears in eta indices */
                if (!match) {
                    for (int i = 0; i < eta->nnz; i++) {
                        if (eta->indices[i] == constrIndex) {
                            match = 1;
                            break;
                        }
                    }
                }
                if (match) {
                    if (eta->entering_var >= 0 && eta->entering_var < n)
                        v2_insert_var(ctx, eta->entering_var);
                    if (eta->leaving_var >= 0 && eta->leaving_var < n)
                        v2_insert_var(ctx, eta->leaving_var);
                }
            }
            eta_work++;
            eta = eta->next;
        }
        if (state->work_counter != NULL)
            *state->work_counter += (double)eta_work;
    }

    /* Also maintain V1 constraint dirty marking for backward compat */
    if (ctx->constr_dirty != NULL && constrIndex < ctx->num_constrs &&
        !ctx->constr_dirty[constrIndex]) {
        ctx->constr_dirty[constrIndex] = 1;
        ctx->num_constr_dirty++;
    }
}
