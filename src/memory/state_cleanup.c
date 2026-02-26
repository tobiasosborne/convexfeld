/**
 * @file state_cleanup.c
 * @brief State structure deallocators (M2.1.4)
 *
 * Provides cleanup functions for complex solver state structures.
 * These wrap the module-specific free functions and provide a
 * consistent interface for memory deallocation.
 *
 * All functions are NULL-safe.
 */

#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_pricing.h"
#include "convexfeld/cxf_callback.h"
#include <stdlib.h>

#include "../basis/basis_internal.h"
#include "../pricing/pricing_internal.h"

/* Forward declarations for module-specific free functions */

/*============================================================================
 * cxf_free_attribute_table - SolverState Cleanup
 *===========================================================================*/

/**
 * @brief Free a SolverState and all associated memory.
 *
 * Deallocates all dynamically allocated fields:
 * - Working arrays (work_lb/ub/obj/x/pi/dj, work_counter/column/cB)
 * - Saved bounds (saved_lb/ub), activity bounds (min/max_activity)
 * - Crash arrays (row_status, col_nz_count)
 * - Matrix copies (csc_col_ptr/row_idx/values, csr_row_ptr/col_idx/values)
 * - Constraint metadata (work_rhs, work_sense)
 * - Scaling factors (row_scale, col_scale)
 * - Subcomponents: BasisState, PricingState, TimingState
 *
 * Does NOT free model_ref (owned by caller).
 * Does NOT perform complementary slackness fixing (use cxf_simplex_final).
 *
 * @param ctx SolverState to free (may be NULL)
 */
void cxf_free_attribute_table(SolverState *ctx) {
    if (ctx == NULL) {
        return;
    }

    /* Free working arrays */
    free(ctx->work_lb);
    free(ctx->work_ub);
    free(ctx->work_obj);
    free(ctx->work_x);
    free(ctx->work_pi);
    free(ctx->work_dj);
    free(ctx->work_counter);
    free(ctx->work_column);
    free(ctx->work_cB);

    /* Free saved bounds (B1: EXPAND perturbation) */
    free(ctx->saved_lb);
    free(ctx->saved_ub);

    /* Free activity bounds (B2) */
    free(ctx->min_activity);
    free(ctx->max_activity);
    free(ctx->negUnbdCount);
    free(ctx->posUnbdCount);

    /* Free crash basis arrays */
    free(ctx->row_status);
    free(ctx->col_nz_count);

    /* Free matrix working copies (P3.1) */
    free(ctx->csc_col_ptr);
    free(ctx->csc_row_idx);
    free(ctx->csc_values);
    free(ctx->csr_row_ptr);
    free(ctx->csr_col_idx);
    free(ctx->csr_values);
    free(ctx->work_rhs);
    free(ctx->work_sense);

    /* Free scaling factors */
    free(ctx->row_scale);
    free(ctx->col_scale);

    /* Free subcomponents */
    cxf_basis_free(ctx->basis);
    if (ctx->pricing != NULL)
        cxf_pricing_free(ctx->pricing);

    /* Free timing */
    free(ctx->timing);

    free(ctx);
}

/*============================================================================
 * cxf_free_basis_state - BasisState Cleanup (wrapper)
 *===========================================================================*/

/**
 * @brief Free a BasisState and all associated memory.
 *
 * This is a wrapper around cxf_basis_free for API consistency.
 * Deallocates the eta linked list, basic_vars, var_status, and work arrays.
 *
 * @param basis BasisState to free (may be NULL)
 */
void cxf_free_basis_state(BasisState *basis) {
    cxf_basis_free(basis);
}

/*============================================================================
 * cxf_free_callback_state - CallbackContext Cleanup
 *===========================================================================*/

/**
 * @brief Free a CallbackContext structure.
 *
 * Deallocates the CallbackContext but NOT the user_data pointer,
 * which is owned by the user. Clears magic numbers before freeing.
 *
 * @param ctx CallbackContext to free (may be NULL)
 */
void cxf_free_callback_state(CallbackContext *ctx) {
    if (ctx == NULL) {
        return;
    }

    /* Clear magic numbers for safety */
    ctx->magic = 0;
    ctx->safety_magic = 0;

    /* Do NOT free user_data - owned by caller */
    ctx->callback_func = NULL;
    ctx->user_data = NULL;

    free(ctx);
}
