/**
 * @file setup.c
 * @brief Simplex setup and preprocessing (M7.1.6)
 *
 * Implements cxf_simplex_setup and cxf_simplex_preprocess.
 * Setup prepares working arrays and determines initial phase.
 * Preprocessing reduces problem size via bound tightening and scaling.
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_matrix.h"
#include "convexfeld/cxf_pricing.h"
#include "convexfeld/cxf_types.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../basis/basis_internal.h"

/* Default parameters */
#define DEFAULT_FEASIBILITY_TOL 1e-6

/**
 * @brief Compute per-constraint activity bounds (v2 P3.21).
 *
 * For each constraint i, compute:
 *   min_activity[i] = sum over j of min(a_ij*lb_j, a_ij*ub_j)
 *   max_activity[i] = sum over j of max(a_ij*lb_j, a_ij*ub_j)
 * Handles infinite bounds: any infinite contribution sets activity to +/-inf.
 * Applies rounding correction when min/max are very close.
 *
 * @param state  Solver state with CSC matrix and bounds
 * @param count  Number of constraint indices to update (0 = all)
 * @param indices  Constraint indices to update (NULL = all)
 */
void cxf_compute_activity_bounds(SolverState *state, int count,
                                 const int *indices) {
    if (state->csc_col_ptr == NULL) return;
    if (state->min_activity == NULL || state->max_activity == NULL) return;

    int m = state->num_constrs;
    int n = state->num_vars;

    /* Determine which constraints to compute */
    int do_all = (count == 0 || indices == NULL);
    if (do_all) {
        memset(state->min_activity, 0, (size_t)m * sizeof(double));
        memset(state->max_activity, 0, (size_t)m * sizeof(double));
        if (state->negUnbdCount)
            memset(state->negUnbdCount, 0, (size_t)m * sizeof(int));
        if (state->posUnbdCount)
            memset(state->posUnbdCount, 0, (size_t)m * sizeof(int));
    } else {
        for (int k = 0; k < count; k++) {
            int i = indices[k];
            if (i >= 0 && i < m) {
                state->min_activity[i] = 0.0;
                state->max_activity[i] = 0.0;
                if (state->negUnbdCount) state->negUnbdCount[i] = 0;
                if (state->posUnbdCount) state->posUnbdCount[i] = 0;
            }
        }
    }

    /* Accumulate contributions from each variable column (CSC) */
    if (state->csc_row_idx == NULL || state->csc_values == NULL)
        return;

    for (int j = 0; j < n; j++) {
        double lb_j = state->work_lb[j];
        double ub_j = state->work_ub[j];
        int64_t start = state->csc_col_ptr[j];
        int64_t end = state->csc_col_ptr[j + 1];

        for (int64_t k = start; k < end; k++) {
            int row = state->csc_row_idx[k];
            double a = state->csc_values[k];

            /* Skip if not in the target set */
            if (!do_all) {
                int found = 0;
                for (int c = 0; c < count; c++) {
                    if (indices[c] == row) { found = 1; break; }
                }
                if (!found) continue;
            }

            /* Compute min/max contribution */
            double prod_lb = a * lb_j;
            double prod_ub = a * ub_j;

            if (lb_j <= -CXF_INFINITY || ub_j >= CXF_INFINITY) {
                /* Infinite bound: track count, accumulate finite part */
                if (a > 0) {
                    if (lb_j <= -CXF_INFINITY) {
                        if (state->negUnbdCount)
                            state->negUnbdCount[row]++;
                    } else {
                        state->min_activity[row] += prod_lb;
                    }
                    if (ub_j >= CXF_INFINITY) {
                        if (state->posUnbdCount)
                            state->posUnbdCount[row]++;
                    } else {
                        state->max_activity[row] += prod_ub;
                    }
                } else {
                    if (ub_j >= CXF_INFINITY) {
                        if (state->negUnbdCount)
                            state->negUnbdCount[row]++;
                    } else {
                        state->min_activity[row] += prod_ub;
                    }
                    if (lb_j <= -CXF_INFINITY) {
                        if (state->posUnbdCount)
                            state->posUnbdCount[row]++;
                    } else {
                        state->max_activity[row] += prod_lb;
                    }
                }
            } else {
                /* Finite bounds */
                if (prod_lb < prod_ub) {
                    state->min_activity[row] += prod_lb;
                    state->max_activity[row] += prod_ub;
                } else {
                    state->min_activity[row] += prod_ub;
                    state->max_activity[row] += prod_lb;
                }
            }
        }
    }

    /* Rounding correction: when min ≈ max, snap to prevent false infeasibility */
    for (int i = 0; i < m; i++) {
        if (!do_all) {
            int found = 0;
            for (int c = 0; c < count; c++) {
                if (indices[c] == i) { found = 1; break; }
            }
            if (!found) continue;
        }
        double gap = state->max_activity[i] - state->min_activity[i];
        if (gap > 0 && gap < CXF_FEASIBILITY_TOL) {
            double mid = 0.5 * (state->min_activity[i] + state->max_activity[i]);
            state->min_activity[i] = mid;
            state->max_activity[i] = mid;
        }
    }
}

/**
 * @brief Compute constraint activity bounds (v2 P3.21).
 *
 * V2 spec: cxf_simplex_setup computes min/max activity bounds for
 * each constraint based on variable bounds and matrix coefficients.
 * Supports selective recomputation via count/indices parameters.
 *
 * This function ONLY computes activity bounds. State initialization
 * (reduced costs, pricing, phase detection) is handled by
 * cxf_simplex_init and cxf_setup_phase_one.
 *
 * @param state   Solver state with CSC matrix and bounds
 * @param env     Environment with tolerances
 * @param count   Number of constraints to update (0 = all)
 * @param indices Constraint indices to update (NULL = all)
 */
void cxf_simplex_setup(SolverState *state, CxfEnv *env,
                       int count, int *indices) {
    (void)env;  /* Tolerances used internally by compute_activity_bounds */
    if (state == NULL) return;
    cxf_compute_activity_bounds(state, count, indices);
}

/**
 * @brief Preprocess: fix near-bound variables (v2 P3.21).
 *
 * Scans variables with tight bound range (ub - lb < threshold).
 * Fixes them at the closest bound, updates activity bounds.
 *
 * V2 spec signature: (state, env) -> int.
 *
 * @param state Solver context
 * @param env   Environment with feasibility_tol
 * @return 0 on success, CXF_INFEASIBLE if lb > ub
 */
int cxf_simplex_preprocess(SolverState *state, CxfEnv *env) {
    if (state == NULL || env == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    int n = state->num_vars;
    double feas_tol = env->feasibility_tol;
    if (feas_tol <= 0.0) feas_tol = DEFAULT_FEASIBILITY_TOL;
    double tightness = 10.0 * feas_tol;

    double *lb = state->work_lb;
    double *ub = state->work_ub;
    if (n == 0 || lb == NULL || ub == NULL) return CXF_OK;

    for (int j = 0; j < n; j++) {
        /* Infeasibility check */
        if (lb[j] > ub[j] + feas_tol) {
            return CXF_INFEASIBLE;
        }

        /* Skip non-tight variables */
        double range = ub[j] - lb[j];
        if (range > tightness) continue;

        /* Fix variable: use midpoint when gap is nonzero but within
         * bound_equality_tol, per numerical_stability.md Section C.
         * Exactly-fixed variables (range == 0) stay at their bound. */
        if (state->basis != NULL && state->basis->var_status != NULL &&
            state->basis->var_status[j] < 0) {
            double target;
            if (range > 0.0 && range < CXF_BOUND_EQUALITY_TOL) {
                /* Nonzero gap within tolerance: midpoint minimizes
                 * constraint perturbation from the fixing operation. */
                target = 0.5 * (lb[j] + ub[j]);
            } else {
                /* Exactly fixed or wider gap: snap to closest bound */
                double x_j = (state->work_x != NULL) ? state->work_x[j]
                                                      : lb[j];
                target = (fabs(x_j - lb[j]) <= fabs(x_j - ub[j])) ?
                         lb[j] : ub[j];
            }
            if (state->work_x != NULL) state->work_x[j] = target;
            /* V2 simplex_phases.md Step 4.5: adjust objective for fixed var */
            if (state->work_obj != NULL)
                state->obj_value += state->work_obj[j] * target;
            /* V2 simplex_phases.md: create VARIABLE_FIX eta record */
            if (state->basis != NULL) {
                EtaVector *eta;
                if (state->basis->eta_pool != NULL)
                    eta = (EtaVector *)cxf_eta_pool_alloc(
                        state->basis->eta_pool, sizeof(EtaVector));
                else
                    eta = (EtaVector *)calloc(1, sizeof(EtaVector));
                if (eta != NULL) {
                    eta->type = CXF_ETA_VARIABLE_FIX;
                    eta->pivot_row = -1;
                    eta->entering_var = j;
                    eta->leaving_var = -1;
                    eta->pivot_elem = target;
                    eta->reduced_cost = state->work_obj[j];
                    eta->direction = 0;
                    eta->status = state->basis->var_status[j];
                    eta->nnz = 0;
                    eta->indices = NULL;
                    eta->values = NULL;
                    eta->next = state->basis->eta_head;
                    state->basis->eta_head = eta;
                    state->basis->eta_count++;
                    state->eta_count = state->basis->eta_count;
                }
            }
            lb[j] = target;
            ub[j] = target;
            state->cols_eliminated++;
        }
    }

    /* Recompute activity bounds after fixing */
    if (state->cols_eliminated > 0) {
        cxf_compute_activity_bounds(state, 0, NULL);
    }

    return CXF_OK;
}
