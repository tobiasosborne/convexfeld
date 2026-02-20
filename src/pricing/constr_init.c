/**
 * @file constr_init.c
 * @brief Initialize constraint-side pricing queues (F1)
 */

#include "convexfeld/cxf_pricing.h"
#include "convexfeld/cxf_types.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief Set constraint dimension and allocate constraint queues.
 *
 * Called after cxf_pricing_init when constraint count is known.
 */
int cxf_pricing_init_constrs(PricingState *ctx, int num_constrs) {
    if (ctx == NULL) return CXF_ERROR_NULL_ARGUMENT;
    if (num_constrs < 0) return CXF_ERROR_INVALID_ARGUMENT;

    ctx->num_constrs = num_constrs;

    free(ctx->constr_dirty);
    free(ctx->constr_candidates);
    ctx->constr_dirty = NULL;
    ctx->constr_candidates = NULL;
    ctx->num_constr_dirty = 0;
    ctx->num_constr_candidates = 0;

    if (num_constrs > 0) {
        ctx->constr_dirty = (int *)calloc((size_t)num_constrs, sizeof(int));
        ctx->constr_candidates = (int *)calloc((size_t)num_constrs, sizeof(int));
        if (ctx->constr_dirty == NULL || ctx->constr_candidates == NULL) {
            return CXF_ERROR_OUT_OF_MEMORY;
        }
    }

    return CXF_OK;
}
