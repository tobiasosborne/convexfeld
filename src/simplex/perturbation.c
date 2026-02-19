/**
 * @file perturbation.c
 * @brief EXPAND-family anti-cycling perturbation (v2 — P2.6)
 *
 * Implements cxf_simplex_perturbation using implied bound analysis
 * (Gill, Murray, Saunders & Wright 1989). Replaces the classical
 * Wolfe random perturbation with targeted removal of degenerate
 * variables from the pricing candidate set.
 *
 * Spec: docs/specs-v2/specs/algorithms/perturbation.md
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_matrix.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_types.h"
#include <math.h>
#include <string.h>

/* Implied bound gap clamping range */
#define MIN_BOUND_RANGE  CXF_PERTURB_FLOOR    /* 1e-10 */
#define MAX_PERTURBATION CXF_PERTURB_CEILING  /* 1e-6  */

/* Work accounting */
#define WORK_SCALE_CANDIDATE 1.0

/**
 * @brief Clamp value to [lo, hi].
 */
static double clamp(double val, double lo, double hi) {
    if (val < lo) return lo;
    if (val > hi) return hi;
    return val;
}

/**
 * @brief Get saved (original) lower bound for variable j.
 *
 * Structural variables use model->lb. Slack/artificial variables
 * have saved lower bound of 0.
 */
static double saved_lb(const CxfModel *model, int j, int n) {
    if (j < n && model->lb) return model->lb[j];
    return 0.0;
}

/**
 * @brief Get saved (original) upper bound for variable j.
 *
 * Structural variables use model->ub. Slack/artificial variables
 * have saved upper bound of INFINITY.
 */
static double saved_ub(const CxfModel *model, int j, int n) {
    if (j < n && model->ub) return model->ub[j];
    return CXF_INFINITY;
}

/**
 * @brief Process a basic variable via implied bound analysis.
 *
 * For basic variable x_j in constraint row r, compute the implied
 * bounds from saved bounds of other variables in the row.
 *
 * @param state   Solver state
 * @param mat     Constraint matrix with CSR
 * @param model   Model with saved bounds
 * @param row     Constraint row index
 * @param bvar    Basic variable index
 * @param n       Number of structural variables
 * @param feas_tol Feasibility tolerance
 * @return 1 if degenerate (should remove), 0 otherwise, -1 if infeasible
 */
static int analyze_basic_variable(
    const SolverState *state, const MatrixData *mat,
    const CxfModel *model, int row, int bvar, int n, double feas_tol)
{
    if (!mat->row_ptr || !mat->col_idx || !mat->row_values) {
        return 0;  /* No CSR data, can't analyze */
    }

    int64_t rstart = mat->row_ptr[row];
    int64_t rend = mat->row_ptr[row + 1];

    double impl_lower = 0.0;
    double impl_upper = 0.0;
    int count_lower_unbounded = 0;
    int count_upper_unbounded = 0;

    for (int64_t k = rstart; k < rend; k++) {
        int col = mat->col_idx[k];
        if (col < 0) continue;       /* Inactive entry */
        if (col == bvar) continue;    /* Skip the basic variable itself */

        double coeff = mat->row_values[k];
        double s_lb = saved_lb(model, col, n);
        double s_ub = saved_ub(model, col, n);

        if (coeff > 0.0) {
            /* Implied lower uses ub, implied upper uses lb */
            if (s_ub < CXF_INFINITY) {
                impl_lower += coeff * s_ub;
            } else {
                count_lower_unbounded++;
            }
            if (s_lb > -CXF_INFINITY) {
                impl_upper += coeff * s_lb;
            } else {
                count_upper_unbounded++;
            }
        } else if (coeff < 0.0) {
            /* Implied lower uses lb, implied upper uses ub */
            if (s_lb > -CXF_INFINITY) {
                impl_lower += coeff * s_lb;
            } else {
                count_lower_unbounded++;
            }
            if (s_ub < CXF_INFINITY) {
                impl_upper += coeff * s_ub;
            } else {
                count_upper_unbounded++;
            }
        }
    }

    double gap = impl_lower - impl_upper;
    double pmag = clamp(gap, MIN_BOUND_RANGE, MAX_PERTURBATION);

    /* Check constraint sense */
    char sense = (mat->sense) ? mat->sense[row] : '<';

    /* Infeasibility check for equality constraints */
    if ((sense == '=' || sense == 'E') &&
        impl_upper > pmag * feas_tol &&
        count_upper_unbounded == 0) {
        return -1;  /* Infeasible */
    }

    /* Degeneracy check for inequality constraints */
    if (impl_lower < -feas_tol * pmag && count_lower_unbounded == 0) {
        return 1;  /* Degenerate — should remove */
    }

    return 0;  /* Not degenerate */
}

/**
 * @brief Apply EXPAND anti-cycling perturbation (P2.6).
 *
 * Analyzes pricing candidates and basic variables for degeneracy
 * using implied bound analysis. Degenerate nonbasic variables are
 * marked as fixed (removed from pricing). Degenerate basic variables
 * are tracked via the perturbation counter.
 *
 * @param state Solver state with basis, bounds, reduced costs
 * @param env   Environment with tolerances
 * @return CXF_OK on success, CXF_INFEASIBLE on bound violation
 */
int cxf_simplex_perturbation(SolverState *state, CxfEnv *env) {
    if (state == NULL || env == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    int n = state->num_vars;
    int m = state->num_constrs;
    if (n == 0 || m == 0) {
        return CXF_OK;
    }

    CxfModel *model = state->model_ref;
    if (model == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    /* EXPAND is stall-triggered (P2.6 Phase 1): only operate during
     * active iteration. Before the first iteration, reduced costs are
     * all zero, and analyzing them would falsely mark every nonbasic
     * variable as degenerate. The caller (unified loop, ypf9) will
     * invoke this when stalling is actually detected. */
    if (state->iteration == 0) {
        return CXF_OK;
    }

    BasisState *basis = state->basis;
    if (basis == NULL || basis->var_status == NULL) {
        return CXF_OK;  /* No basis yet, nothing to perturb */
    }

    MatrixData *mat = model->matrix;
    double feas_tol = env->feasibility_tol;
    if (feas_tol <= 0.0) feas_tol = CXF_FEASIBILITY_TOL;

    int *var_status = basis->var_status;
    double *dj = state->work_dj;
    int total_vars = n + m;
    int perturbed = 0;

    /*
     * Case A: Nonbasic variables at lower bound.
     * Remove degenerate candidates with near-zero or contradictory
     * reduced costs from future pricing consideration.
     */
    if (dj != NULL) {
        for (int j = 0; j < total_vars; j++) {
            if (var_status[j] != CXF_VAR_AT_LOWER) continue;

            double rc = dj[j];

            if (rc < -feas_tol) {
                /* Severely negative RC at lower bound — contradictory.
                 * For equality constraints: potential infeasibility. */
                char sense = '?';
                if (mat && mat->sense) {
                    /* Find which constraint this variable appears in.
                     * For slack variables, index i = j - n. */
                    if (j >= n && (j - n) < m) {
                        sense = mat->sense[j - n];
                    }
                }
                if (sense == '=' || sense == 'E') {
                    state->problem_row_index = (j >= n) ? j - n : -1;
                    state->perturb_count += perturbed;
                    return CXF_INFEASIBLE;
                }
                /* Mark as fixed to remove from pricing */
                var_status[j] = CXF_VAR_FIXED;
                perturbed++;
            } else if (fabs(rc) <= feas_tol) {
                /* Near-zero RC at lower bound — degenerate.
                 * Pivoting this variable gives zero progress. */
                var_status[j] = CXF_VAR_FIXED;
                perturbed++;
            }
        }
    }

    /*
     * Case B: Basic variables — implied bound analysis.
     * For each basic structural variable, compute implied bounds
     * from the constraint row using saved (original) bounds.
     */
    if (basis->basic_vars != NULL && mat != NULL &&
        mat->row_ptr != NULL) {
        for (int i = 0; i < m; i++) {
            int bvar = basis->basic_vars[i];
            if (bvar < 0 || bvar >= n) continue;  /* Skip slacks */

            int result = analyze_basic_variable(
                state, mat, model, i, bvar, n, feas_tol);

            if (result == -1) {
                /* Infeasible */
                state->problem_row_index = i;
                state->perturb_count += perturbed;
                return CXF_INFEASIBLE;
            }
            if (result == 1) {
                /* Degenerate basic variable tracked */
                perturbed++;
            }
        }
    }

    /* Update work counter */
    if (state->work_counter) {
        *state->work_counter += (double)total_vars * WORK_SCALE_CANDIDATE;
    }

    state->perturb_count += perturbed;
    return CXF_OK;
}

/**
 * @brief Remove perturbations and restore original bounds (P2.6).
 *
 * Restores working bounds from saved (model) bounds, undoing
 * any status changes from perturbation. Basic variable recovery
 * is deferred to cxf_simplex_refine.
 *
 * @param state Solver state
 * @param env   Environment
 * @return CXF_OK on success, 1 if nothing to restore
 */
int cxf_simplex_unperturb(SolverState *state, CxfEnv *env) {
    if (state == NULL || env == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    /* Nothing to undo if no perturbation was applied */
    if (state->perturb_count == 0) {
        return 1;
    }

    /* Restore original bounds from model */
    int n = state->num_vars;
    if (n > 0 && state->model_ref != NULL) {
        CxfModel *model = state->model_ref;
        if (state->work_lb && model->lb) {
            memcpy(state->work_lb, model->lb, (size_t)n * sizeof(double));
        }
        if (state->work_ub && model->ub) {
            memcpy(state->work_ub, model->ub, (size_t)n * sizeof(double));
        }
    }

    /* Reset perturbation counter */
    state->perturb_count = 0;

    return CXF_OK;
}
