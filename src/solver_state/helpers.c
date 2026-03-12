/**
 * @file helpers.c
 * @brief Helper functions for solver state management.
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_matrix.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_types.h"
#include <stdlib.h>
#include <math.h>
#include "../memory/memory_internal.h"

#define MAX_PASSES 10
#define BOUND_TOL 1e-10

/** Update constraint activities after tightening a variable bound.
 *  pos_arr gets delta for positive coefficients, neg_arr for negative. */
static void update_activities(
    MatrixData *matrix, int colIdx, double delta,
    double *pos_arr, double *neg_arr,
    int num_constrs, int num_vars,
    int *worklist, int *tail, uint8_t *inWorklist)
{
    if (!matrix->col_ptr || !matrix->row_idx || !matrix->values) return;
    int64_t cs = matrix->col_ptr[colIdx];
    int64_t ce = matrix->col_ptr[colIdx + 1];
    for (int64_t p = cs; p < ce; p++) {
        int r = matrix->row_idx[p];
        if (r < 0 || r >= num_constrs) continue;
        double a_r = matrix->values[p];
        if (a_r > 0.0) pos_arr[r] += a_r * delta;
        else            neg_arr[r] += a_r * delta;
        if (r < num_vars && !inWorklist[r]) {
            worklist[*tail] = r;
            *tail = (*tail + 1) % num_vars;
            inWorklist[r] = 1;
        }
    }
}

/** Worklist-based FBBT bound propagation for simplex cleanup.
 *  Returns 0 on success, CXF_INFEASIBLE if infeasible, or error code. */
int cxf_propagate_bounds(
    void *env,
    SolverState *state,
    double *lb_working,
    double *ub_working,
    uint8_t *constrSenses,
    double *lb_delta,
    double *ub_delta,
    int32_t *lb_count,
    int32_t *ub_count,
    double lb_threshold,
    double ub_threshold
) {
    (void)env;
    if (!state || !lb_working || !ub_working || !constrSenses ||
        !lb_delta || !ub_delta || !lb_count || !ub_count) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    int num_vars = state->num_vars;
    int num_constrs = state->num_constrs;

    if (num_vars == 0) return CXF_OK;

    int *worklist = (int *)cxf_malloc((size_t)num_vars * sizeof(int));
    uint8_t *inWorklist = (uint8_t *)cxf_calloc((size_t)num_vars, sizeof(uint8_t));

    if (!worklist || !inWorklist) {
        cxf_free(worklist);
        cxf_free(inWorklist);
        return CXF_ERROR_OUT_OF_MEMORY;
    }

    int worklistCount = 0;
    for (int j = 0; j < num_vars; j++) {
        if (state->basis && state->basis->var_status &&
            state->basis->var_status[j] < 0) {
            worklist[worklistCount++] = j;
            inWorklist[j] = 1;
        }
    }

    if (worklistCount == 0) {
        cxf_free(worklist);
        cxf_free(inWorklist);
        return CXF_OK;
    }

    int head = 0, tail = worklistCount, passCount = 0, processed = 0;
    MatrixData *matrix = state->model_ref->matrix;

    while (head != tail && passCount <= MAX_PASSES) {
        int varIdx = worklist[head];
        head = (head + 1) % num_vars;
        processed++;

        if (processed >= worklistCount && head == 0) passCount++;

        if (varIdx >= num_constrs) {
            inWorklist[varIdx] = 0;
            continue;
        }

        uint8_t sense = constrSenses[varIdx];

        if ((sense == CXF_LESS_EQUAL || sense == CXF_EQUAL) &&
            ub_count[varIdx] == 0 && ub_delta[varIdx] > ub_threshold) {
            cxf_free(worklist);
            cxf_free(inWorklist);
            return CXF_INFEASIBLE;
        }

        if ((sense == CXF_GREATER_EQUAL || sense == CXF_EQUAL) &&
            lb_count[varIdx] == 0 && lb_delta[varIdx] < -lb_threshold) {
            cxf_free(worklist);
            cxf_free(inWorklist);
            return CXF_INFEASIBLE;
        }

        if (!matrix || !matrix->row_ptr || !matrix->col_idx || !matrix->row_values) {
            inWorklist[varIdx] = 0;
            continue;
        }

        int64_t row_start = matrix->row_ptr[varIdx];
        int64_t row_end = matrix->row_ptr[varIdx + 1];

        for (int64_t k = row_start; k < row_end; k++) {
            int colIdx = matrix->col_idx[k];
            if (colIdx < 0 || colIdx >= num_vars) continue;

            double coeff = matrix->row_values[k];
            if (fabs(coeff) < 1e-15) continue;

            double rhs_val = matrix->rhs ? matrix->rhs[varIdx] : 0.0;

            /* FBBT implied bounds (Savelsbergh 1994):
             * Subtract this variable's contribution from constraint activity,
             * then derive what range x_j must lie in. */
            double newLB = lb_working[colIdx];
            double newUB = ub_working[colIdx];

            if (coeff > 0.0) {
                double other_min = lb_delta[varIdx] - coeff * lb_working[colIdx];
                double other_max = ub_delta[varIdx] - coeff * ub_working[colIdx];
                if ((sense == CXF_LESS_EQUAL || sense == CXF_EQUAL) &&
                    ub_count[varIdx] == 0)
                    newUB = fmin(newUB, (rhs_val - other_min) / coeff);
                if ((sense == CXF_GREATER_EQUAL || sense == CXF_EQUAL) &&
                    lb_count[varIdx] == 0)
                    newLB = fmax(newLB, (rhs_val - other_max) / coeff);
            } else {
                double other_min = lb_delta[varIdx] - coeff * ub_working[colIdx];
                double other_max = ub_delta[varIdx] - coeff * lb_working[colIdx];
                if ((sense == CXF_LESS_EQUAL || sense == CXF_EQUAL) &&
                    ub_count[varIdx] == 0)
                    newLB = fmax(newLB, (rhs_val - other_min) / coeff);
                if ((sense == CXF_GREATER_EQUAL || sense == CXF_EQUAL) &&
                    lb_count[varIdx] == 0)
                    newUB = fmin(newUB, (rhs_val - other_max) / coeff);
            }

            if (newLB > lb_working[colIdx] + BOUND_TOL) {
                if (newLB > ub_working[colIdx] + BOUND_TOL) {
                    cxf_free(worklist);
                    cxf_free(inWorklist);
                    return CXF_INFEASIBLE;
                }
                double old_lb = lb_working[colIdx];
                lb_working[colIdx] = newLB;
                update_activities(matrix, colIdx, newLB - old_lb,
                    lb_delta, ub_delta, num_constrs, num_vars,
                    worklist, &tail, inWorklist);
            }

            if (newUB < ub_working[colIdx] - BOUND_TOL) {
                if (newUB < lb_working[colIdx] - BOUND_TOL) {
                    cxf_free(worklist);
                    cxf_free(inWorklist);
                    return CXF_INFEASIBLE;
                }
                double old_ub = ub_working[colIdx];
                ub_working[colIdx] = newUB;
                update_activities(matrix, colIdx, newUB - old_ub,
                    ub_delta, lb_delta, num_constrs, num_vars,
                    worklist, &tail, inWorklist);
            }
        }

        inWorklist[varIdx] = 0;
    }

    cxf_free(worklist);
    cxf_free(inWorklist);
    return CXF_OK;
}
