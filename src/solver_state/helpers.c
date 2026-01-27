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
#include <string.h>
#include <math.h>

#define MAX_PASSES 10
#define BOUND_TOL 1e-10

/**
 * @brief Worklist-based bound propagation for simplex cleanup.
 *
 * Iteratively tightens variable bounds through constraint activity analysis.
 * Returns 0 on success, CXF_INFEASIBLE if infeasible, or error code.
 */
int cxf_cleanup_helper(
    void *env,
    SolverContext *state,
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

    int *worklist = (int *)malloc((size_t)num_vars * sizeof(int));
    uint8_t *inWorklist = (uint8_t *)calloc((size_t)num_vars, sizeof(uint8_t));

    if (!worklist || !inWorklist) {
        free(worklist);
        free(inWorklist);
        return CXF_ERROR_OUT_OF_MEMORY;
    }

    int worklistCount = 0;
    for (int j = 0; j < num_vars; j++) {
        if (state->basis && state->basis->var_status &&
            state->basis->var_status[j] != CXF_BASIC) {
            worklist[worklistCount++] = j;
            inWorklist[j] = 1;
        }
    }

    if (worklistCount == 0) {
        free(worklist);
        free(inWorklist);
        return CXF_OK;
    }

    int head = 0, tail = worklistCount, passCount = 0, processed = 0;
    SparseMatrix *matrix = state->model_ref->matrix;

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
            free(worklist);
            free(inWorklist);
            return CXF_INFEASIBLE;
        }

        if ((sense == CXF_GREATER_EQUAL || sense == CXF_EQUAL) &&
            lb_count[varIdx] == 0 && lb_delta[varIdx] < -lb_threshold) {
            free(worklist);
            free(inWorklist);
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
            double newLB = lb_working[colIdx];
            double newUB = ub_working[colIdx];

            if (newLB > lb_working[colIdx] + BOUND_TOL) {
                if (newLB > newUB + BOUND_TOL) {
                    free(worklist);
                    free(inWorklist);
                    return CXF_INFEASIBLE;
                }

                lb_working[colIdx] = newLB;

                if (matrix->col_ptr && matrix->row_idx) {
                    int64_t col_start = matrix->col_ptr[colIdx];
                    int64_t col_end = matrix->col_ptr[colIdx + 1];

                    for (int64_t p = col_start; p < col_end; p++) {
                        int affectedRow = matrix->row_idx[p];
                        if (affectedRow >= 0 && affectedRow < num_vars &&
                            !inWorklist[affectedRow]) {
                            worklist[tail] = affectedRow;
                            tail = (tail + 1) % num_vars;
                            inWorklist[affectedRow] = 1;
                        }
                    }
                }

                if (varIdx < num_constrs) {
                    lb_count[varIdx]++;
                    lb_delta[varIdx] += (newLB - lb_working[colIdx]);
                }
            }

            if (newUB < ub_working[colIdx] - BOUND_TOL) {
                if (newUB < newLB - BOUND_TOL) {
                    free(worklist);
                    free(inWorklist);
                    return CXF_INFEASIBLE;
                }

                ub_working[colIdx] = newUB;

                if (matrix->col_ptr && matrix->row_idx) {
                    int64_t col_start = matrix->col_ptr[colIdx];
                    int64_t col_end = matrix->col_ptr[colIdx + 1];

                    for (int64_t p = col_start; p < col_end; p++) {
                        int affectedRow = matrix->row_idx[p];
                        if (affectedRow >= 0 && affectedRow < num_vars &&
                            !inWorklist[affectedRow]) {
                            worklist[tail] = affectedRow;
                            tail = (tail + 1) % num_vars;
                            inWorklist[affectedRow] = 1;
                        }
                    }
                }

                if (varIdx < num_constrs) {
                    ub_count[varIdx]++;
                    ub_delta[varIdx] += (ub_working[colIdx] - newUB);
                }
            }
        }

        inWorklist[varIdx] = 0;
    }

    free(worklist);
    free(inWorklist);
    return CXF_OK;
}
