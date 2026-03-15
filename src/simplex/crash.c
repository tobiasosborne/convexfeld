/**
 * @file crash.c
 * @brief Crash basis construction (v2 — P2.5)
 *
 * Implements cxf_simplex_crash: a single-pass row-scanning crash procedure
 * that classifies constraints for initial basis construction. Detects
 * trivial infeasibility early and removes sparse candidate rows from
 * the active constraint set.
 *
 * Algorithm: Gould & Reid (1989), row-scanning variant.
 * Spec: docs/specs-v2/specs/algorithms/crash_basis.md
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_matrix.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_types.h"
#include <math.h>

/* Work accounting scale factors */
#define WORK_SCALE_ROW    1.0
#define WORK_SCALE_COLUMN 1.0

/**
 * @brief Construct initial crash basis via row-scanning (P2.5).
 *
 * Single-pass scan of all constraints:
 * - Unassigned rows (status=0): feasibility check, assign BASIC_LOWER
 * - Candidate rows (status>0): conditional removal, assign BASIC_UPPER
 *
 * @param state Solver state with row_status, col_nz_count, and matrix
 * @param env   Environment with feasibility tolerance
 * @return CXF_OK on success, CXF_INFEASIBLE if a constraint fails
 */
int cxf_simplex_crash(SolverState *state, CxfEnv *env) {
    if (state == NULL || env == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    int m = state->num_constrs;

    /* Edge case: no constraints */
    if (m == 0) {
        return CXF_OK;
    }

    /* Require crash arrays */
    if (state->row_status == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    /* Use SolverState's working copies per crash_basis.md spec.
     * The spec says S.rowStart, S.rowColIndices, S.constraintRHS,
     * S.constraintSense — these map to state->csr_row_ptr,
     * state->csr_col_idx, state->work_rhs, state->work_sense. */
    double *rhs_arr = state->work_rhs;
    char *sense_arr = state->work_sense;

    /* Fallback to model if SolverState copies not yet populated */
    MatrixData *mat = state->model_ref ? state->model_ref->matrix : NULL;
    if (rhs_arr == NULL && mat != NULL) rhs_arr = mat->rhs;
    if (sense_arr == NULL && mat != NULL) sense_arr = mat->sense;

    int64_t *row_ptr = state->csr_row_ptr;
    int *col_idx = state->csr_col_idx;
    if (row_ptr == NULL && mat != NULL) row_ptr = mat->row_ptr;
    if (col_idx == NULL && mat != NULL) col_idx = mat->col_idx;

    double epsilon_feas = env->feasibility_tol;
    double epsilon_tiny = CXF_ZERO_TOL;  /* Much smaller than epsilon_feas */

    int basic_count = 0;

    for (int i = 0; i < m; i++) {

        if (state->row_status[i] == CXF_ROW_UNASSIGNED) {
            /* --- Unassigned row: feasibility check --- */
            double rhs = rhs_arr ? rhs_arr[i] : 0.0;
            char sense = sense_arr ? sense_arr[i] : '<';

            if (sense == '=' || sense == 'E') {
                /* Equality: |rhs| must be small */
                if (fabs(rhs) >= epsilon_feas) {
                    state->problem_var_index = state->num_vars + i;
                    state->num_basic += basic_count;
                    return CXF_INFEASIBLE;
                }
            } else {
                /* Inequality: rhs must not be too negative */
                if (rhs < -epsilon_feas) {
                    state->problem_var_index = state->num_vars + i;
                    state->num_basic += basic_count;
                    return CXF_INFEASIBLE;
                }
            }

            /* Row is feasible: assign slack to basis at lower bound */
            state->row_status[i] = CXF_ROW_BASIC_LOWER;
            basic_count++;

        } else if (state->row_status[i] > 0) {
            /* --- Candidate row: conditional removal --- */
            char sense = sense_arr ? sense_arr[i] : '<';
            double rhs = rhs_arr ? rhs_arr[i] : 0.0;

            if (sense != '=' && sense != 'E' && rhs >= epsilon_tiny) {
                /* Remove all column entries in this row via CSR */
                if (row_ptr && col_idx) {
                    int64_t start = row_ptr[i];
                    int64_t end = row_ptr[i + 1];

                    for (int64_t k = start; k < end; k++) {
                        int col = col_idx[k];
                        if (col >= 0 && state->col_nz_count) {
                            state->col_nz_count[col]--;
                        }
                        /* Mark column entry as inactive per spec:
                         * S.rowColIndices[k] := -1 */
                        col_idx[k] = -1;
                    }

                    /* Track computational work */
                    if (state->work_counter) {
                        *state->work_counter += (double)(end - start)
                                                * WORK_SCALE_COLUMN;
                    }
                }

                state->row_status[i] = CXF_ROW_BASIC_UPPER;
                basic_count++;
            }
            /* Else: candidate doesn't meet criteria; skip */
        }
        /* Else: row has other status (already processed); skip */
    }

    /* Account for per-row overhead */
    if (state->work_counter) {
        *state->work_counter += (double)m * WORK_SCALE_ROW;
    }

    state->num_basic += basic_count;
    return CXF_OK;
}
