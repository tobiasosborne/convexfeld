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

/* Forward declarations for module-specific free functions */
extern void cxf_basis_free(BasisState *basis);
extern void cxf_pricing_free(PricingContext *ctx);

/*============================================================================
 * cxf_free_solver_state - SolverContext Cleanup
 *===========================================================================*/

/**
 * @brief Free a SolverContext and all associated memory.
 *
 * Deallocates:
 * - All working arrays (work_lb, work_ub, work_obj, work_x, work_pi, work_dj)
 * - BasisState subcomponent (via cxf_basis_free)
 * - PricingContext subcomponent (via cxf_pricing_free)
 * - The SolverContext structure itself
 *
 * Does NOT free the model_ref (owned by caller).
 *
 * @param ctx SolverContext to free (may be NULL)
 */
void cxf_free_solver_state(SolverContext *ctx) {
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

    /* Free subcomponents */
    cxf_basis_free(ctx->basis);
    cxf_pricing_free(ctx->pricing);

    /* Clear pointers before freeing */
    ctx->model_ref = NULL;
    ctx->basis = NULL;
    ctx->pricing = NULL;

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
