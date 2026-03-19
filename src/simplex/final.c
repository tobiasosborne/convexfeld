/**
 * @file final.c
 * @brief Dual-feasibility variable fixing (simplex_lifecycle.md).
 *
 * 5-phase post-solve variable fixing per V2 spec cxf_simplex_final:
 *   Phase 1: Target value determination from reduced costs
 *   Phase 2: Equality constraint verification
 *   Phase 3: Activity propagation for non-equality constraints
 *   Phase 4: Constraint feasibility check
 *   Phase 5: Apply verified fixings via cxf_pivot_bound
 *
 * Called after iteration loop terminates, before cxf_simplex_cleanup.
 * Spec: docs/specs-v2/specs/modules/simplex_lifecycle.md
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_pricing.h"
#include "convexfeld/cxf_types.h"
#include <math.h>
#include <stdlib.h>

#include "simplex_internal.h"
#include "../basis/basis_internal.h"

/* Sentinel: variable has no fixing target assigned */
#define NO_TARGET (-1.0e+300)

/**
 * @brief Dual-feasibility variable fixing per simplex_lifecycle.md.
 *
 * @param state   Solver state (dual values, bounds, status, matrix)
 * @param env     Environment (tolerances, infinity threshold)
 * @param workOut Optional work accumulator (may be NULL)
 * @return CXF_OK on success, error code on failure
 */
int cxf_simplex_final(SolverState *state, CxfEnv *env, double *workOut) {
    if (state == NULL || env == NULL) return CXF_OK;
    if (state->basis == NULL || state->basis->var_status == NULL)
        return CXF_OK;
    if (state->work_dj == NULL || state->work_x == NULL)
        return CXF_OK;

    int n = state->num_vars;
    int m = state->num_constrs;
    int total = n + m;
    double opt_tol = env->optimality_tol;
    double feas_tol = env->feasibility_tol;
    int *var_status = state->basis->var_status;

    /* Allocate temporary arrays */
    double *target = (double *)malloc((size_t)total * sizeof(double));
    if (target == NULL) return CXF_ERROR_OUT_OF_MEMORY;

    int *affected = (int *)malloc((size_t)m * sizeof(int));
    uint8_t *visited = (uint8_t *)calloc((size_t)m, sizeof(uint8_t));
    if (affected == NULL || visited == NULL) {
        free(target); free(affected); free(visited);
        return CXF_ERROR_OUT_OF_MEMORY;
    }

    /* ---- Phase 1: Target value determination ---- */
    int fix_count = 0;
    int abort_early = 0;
    for (int j = 0; j < total; j++) {
        target[j] = NO_TARGET;
        if (var_status[j] < 0) continue; /* skip nonbasic */

        /* Spec: abort if variable has special flags */
        if (state->var_flags != NULL && state->var_flags[j] != 0) {
            abort_early = 1; break;
        }

        double dj = state->work_dj[j];
        double lb = state->work_lb[j];
        double ub = state->work_ub[j];

        if (dj > opt_tol) {
            if (lb <= -CXF_INFINITY) { abort_early = 1; break; }
            target[j] = lb;
            fix_count++;
        } else if (dj < -opt_tol) {
            if (ub >= CXF_INFINITY) { abort_early = 1; break; }
            target[j] = ub;
            fix_count++;
        }
        /* Zero RC: skip (keep unfixed) */
    }

    if (abort_early || fix_count == 0) {
        free(target); free(affected); free(visited);
        return CXF_OK;
    }

    if (workOut != NULL) *workOut += (double)total;

    /* ---- Phase 2: Equality constraint verification ---- */
    int eq_ok = 1;
    if (state->work_sense != NULL && state->csc_col_ptr != NULL &&
        state->csc_row_idx != NULL && state->csc_values != NULL) {
        for (int i = 0; i < m && eq_ok; i++) {
            char sense = state->work_sense[i];
            if (sense != '=' && sense != 'E') continue;

            /* Compute activity for this equality using target values */
            double activity = 0.0;
            if (state->csr_row_ptr != NULL && state->csr_col_idx != NULL
                && state->csr_values != NULL) {
                int64_t rs = state->csr_row_ptr[i];
                int64_t re = state->csr_row_ptr[i + 1];
                for (int64_t k = rs; k < re; k++) {
                    int j = state->csr_col_idx[k];
                    if (j < 0 || j >= n) continue;
                    double val = (target[j] != NO_TARGET)
                        ? target[j] : state->work_x[j];
                    activity += state->csr_values[k] * val;
                }
            }
            /* Add slack contribution */
            int slack = n + i;
            if (slack < total) {
                double val = (target[slack] != NO_TARGET)
                    ? target[slack] : state->work_x[slack];
                activity += val; /* slack coefficient = 1 */
            }

            double rhs = (state->work_rhs != NULL) ? state->work_rhs[i] : 0.0;
            if (fabs(activity - rhs) > feas_tol) {
                eq_ok = 0; /* Equality violated — skip all fixings */
            }
        }
    }

    if (!eq_ok) {
        free(target); free(affected); free(visited);
        return CXF_OK; /* Constraint violation → no fixings applied */
    }

    if (workOut != NULL) *workOut += (double)m;

    /* ---- Phase 3: Activity propagation ---- */
    int aff_count = 0;
    for (int j = 0; j < total && aff_count < m; j++) {
        if (target[j] == NO_TARGET) continue;
        if (j >= n) continue; /* slacks handled implicitly */

        if (state->csc_col_ptr == NULL) continue;
        int64_t cs = state->csc_col_ptr[j];
        int64_t ce = state->csc_col_ptr[j + 1];
        for (int64_t k = cs; k < ce; k++) {
            int row = state->csc_row_idx[k];
            if (row < 0 || row >= m) continue;
            if (state->work_sense != NULL) {
                char sense = state->work_sense[row];
                if (sense == '=' || sense == 'E') continue;
            }
            if (!visited[row]) {
                affected[aff_count++] = row;
                visited[row] = 1;
                if (aff_count >= m) break;
            }
        }
    }

    if (workOut != NULL) *workOut += (double)aff_count;

    /* ---- Phase 4: Constraint feasibility check ---- */
    int all_feasible = 1;
    if (state->csr_row_ptr != NULL && state->csr_col_idx != NULL &&
        state->csr_values != NULL && state->work_rhs != NULL) {
        for (int a = 0; a < aff_count && all_feasible; a++) {
            int i = affected[a];
            double activity = 0.0;
            int64_t rs = state->csr_row_ptr[i];
            int64_t re = state->csr_row_ptr[i + 1];
            for (int64_t k = rs; k < re; k++) {
                int j = state->csr_col_idx[k];
                if (j < 0 || j >= n) continue;
                double val = (target[j] != NO_TARGET)
                    ? target[j] : state->work_x[j];
                activity += state->csr_values[k] * val;
            }
            /* Add slack */
            int slack = n + i;
            if (slack < total) {
                double val = (target[slack] != NO_TARGET)
                    ? target[slack] : state->work_x[slack];
                activity += val;
            }

            double rhs = state->work_rhs[i];
            char sense = state->work_sense[i];
            if ((sense == '<' || sense == 'L') && activity > rhs + feas_tol)
                all_feasible = 0;
            else if ((sense == '>' || sense == 'G') && activity < rhs - feas_tol)
                all_feasible = 0;
        }
    }

    if (!all_feasible) {
        free(target); free(affected); free(visited);
        return CXF_OK; /* Inequality violated → no fixings applied */
    }

    if (workOut != NULL) *workOut += (double)fix_count;

    /* ---- Phase 5: Apply fixings via cxf_pivot_bound ---- */
    /* cxf_pivot_bound adds work_obj[j]*new_value to obj_value, but after
     * cxf_recompute_objective the full c^T x is already in obj_value.
     * Subtract the current contribution first to avoid double-counting. */
    for (int j = 0; j < total; j++) {
        if (target[j] == NO_TARGET) continue;
        state->obj_value -= state->work_obj[j] * state->work_x[j];
        int rc = cxf_pivot_bound(env, state, j, target[j],
                                 state->work_ub[j], 0);
        if (rc != CXF_OK) {
            free(target); free(affected); free(visited);
            return rc;
        }
    }

    /* Refresh pricing after all fixings (simplex_lifecycle.md Phase 5) */
    if (state->pricing != NULL)
        cxf_pricing_update(state->pricing, state);

    /* QA Q9: Post-fixing objective recomputation diagnostic.
     * Catch accumulated drift from incremental updates. */
    {
        double recomputed = 0.0;
        for (int j = 0; j < total; j++)
            recomputed += state->work_obj[j] * state->work_x[j];
        double thr = 1e-6 * (1.0 + fabs(recomputed));
        if (fabs(recomputed - state->obj_value) > thr)
            state->obj_value = recomputed;  /* Correct drift */
    }

    free(target);
    free(affected);
    free(visited);
    return CXF_OK;
}
