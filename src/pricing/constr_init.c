/**
 * @file constr_init.c
 * @brief Initialize constraint-side pricing queues (F1)
 */

#include "convexfeld/cxf_pricing.h"
#include "convexfeld/cxf_types.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/**
 * @brief Set constraint dimension and allocate constraint queues.
 *
 * Called after cxf_pricing_init when constraint count is known.
 * Allocates both v1 dirty arrays and v2 multi-level queue system.
 */
int cxf_pricing_init_constrs(PricingState *ctx, int num_constrs) {
    if (ctx == NULL) return CXF_ERROR_NULL_ARGUMENT;
    if (num_constrs < 0) return CXF_ERROR_INVALID_ARGUMENT;

    ctx->num_constrs = num_constrs;

    /* V1 constraint arrays */
    free(ctx->constr_dirty);
    free(ctx->constr_candidates);
    ctx->constr_dirty = NULL;
    ctx->constr_candidates = NULL;
    ctx->num_constr_dirty = 0;
    ctx->num_constr_candidates = 0;

    if (num_constrs > 0) {
        ctx->constr_dirty = (int *)calloc((size_t)num_constrs, sizeof(int));
        ctx->constr_candidates = (int *)calloc((size_t)num_constrs, sizeof(int));
        if (ctx->constr_dirty == NULL || ctx->constr_candidates == NULL)
            return CXF_ERROR_OUT_OF_MEMORY;
    }

    /* V2 (P4.1): Constraint-side queue system */
    free(ctx->constr_flags);
    ctx->constr_flags = NULL;
    for (int i = 0; i < CXF_MAX_PRICING_LEVELS; i++) {
        free(ctx->constr_queue[i]);
        free(ctx->constr_output_buf[i]);
        ctx->constr_queue[i] = NULL;
        ctx->constr_output_buf[i] = NULL;
        ctx->constr_queue_cap[i] = 0;
        ctx->constr_q_committed[i] = 0;
        ctx->constr_q_total[i] = 0;
        ctx->cached_constr_count[i] = -1;
        ctx->cached_constr_count2[i] = -1;
        ctx->cached_constr_count3[i] = -1;
    }

    if (num_constrs > 0) {
        ctx->constr_flags = (uint8_t *)calloc((size_t)num_constrs,
                                               sizeof(uint8_t));
        if (ctx->constr_flags == NULL) return CXF_ERROR_OUT_OF_MEMORY;

        for (int i = 0; i < CXF_MAX_PRICING_LEVELS; i++) {
            ctx->constr_queue[i] = (int *)calloc((size_t)num_constrs,
                                                  sizeof(int));
            ctx->constr_output_buf[i] = (int *)calloc((size_t)num_constrs,
                                                       sizeof(int));
            if (ctx->constr_queue[i] == NULL ||
                ctx->constr_output_buf[i] == NULL)
                return CXF_ERROR_OUT_OF_MEMORY;
            ctx->constr_queue_cap[i] = num_constrs;
        }
    }

    return CXF_OK;
}
