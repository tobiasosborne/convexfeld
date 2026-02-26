/**
 * @file step.c
 * @brief Simplex iteration engine (v2 P3.20)
 *
 * cxf_simplex_step:  Full iteration with Harris ratio test + BFRT
 * cxf_apply_pivot:   Low-level pivot operation (primal update + eta)
 *
 * Spec: docs/specs-v2/specs/modules/simplex_iteration.md
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_pricing.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_matrix.h"
#include "convexfeld/cxf_types.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "simplex_internal.h"
#include "../basis/basis_internal.h"


#define REFACTOR_INTERVAL  100
#define MAX_BFRT_FLIPS     10
#define MAX_CANDIDATES     10

/* External declarations */

/*---------------------------------------------------------------------------*/

static double get_auxiliary_coeff_fallback(const SolverState *state, int row) {
    if (state == NULL || state->work_sense == NULL) return 1.0;
    char sense = state->work_sense[row];
    /* Unconditional per natural form — matches phase_one.c diag_coeff init */
    if (sense == '>' || sense == 'G') return -1.0;
    return 1.0;  /* <= and = always +1 */
}

static void extract_column_ext(const SolverState *state, int col,
                               double *dense) {
    int n = state->num_vars;
    int m = state->num_constrs;
    BasisState *basis = state->basis;
    memset(dense, 0, (size_t)m * sizeof(double));
    if (col < n) {
        /* Structural variable: extract from CSC */
        if (state->csc_col_ptr == NULL) return;
        int64_t start = state->csc_col_ptr[col];
        int64_t end = state->csc_col_ptr[col + 1];
        for (int64_t k = start; k < end; k++)
            dense[state->csc_row_idx[k]] = state->csc_values[k];
    } else if (col < n + m) {
        /* Slack/surplus: diagonal with natural sign */
        int row = col - n;
        if (row >= 0 && row < m) {
            double coeff = (basis && basis->diag_coeff) ?
                basis->diag_coeff[row] :
                get_auxiliary_coeff_fallback(state, row);
            dense[row] = coeff;
        }
    }
    /* No artificial variable range — implicit Phase I has no artificials */
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Low-level pivot: update primal values, create eta, fix leaving status.
 */
int cxf_apply_pivot(SolverState *state, int entering, int leavingRow,
                    const double *pivotCol, double stepSize) {
    if (state == NULL || pivotCol == NULL) return CXF_ERROR_NULL_ARGUMENT;
    if (state->basis == NULL) return CXF_ERROR_INVALID_ARGUMENT;

    BasisState *basis = state->basis;
    int leaving = basis->basic_vars[leavingRow];
    int total = state->num_vars + state->num_constrs;

    /* Update basic variable values */
    for (int i = 0; i < state->num_constrs; i++) {
        int bv = basis->basic_vars[i];
        if (bv >= 0 && bv < total)
            state->work_x[bv] -= stepSize * pivotCol[i];
    }

    /* Update entering variable value.
     * Use current x (not lb/ub) as starting point — handles free variables
     * where x=0 but lb=-inf correctly. For bounded vars, x==lb or x==ub
     * so this is equivalent to the standard formula. */
    if (basis->var_status[entering] == CXF_VAR_AT_LOWER)
        state->work_x[entering] = state->work_x[entering] + stepSize;
    else
        state->work_x[entering] = state->work_x[entering] - stepSize;

    /* Determine leaving variable's nonbasic status before pivot */
    int leave_status = CXF_VAR_AT_LOWER;
    if (leaving >= 0 && leaving < total) {
        double x = state->work_x[leaving];
        if (fabs(x - state->work_ub[leaving]) <
            fabs(x - state->work_lb[leaving]) &&
            state->work_ub[leaving] < CXF_INFINITY)
            leave_status = CXF_VAR_AT_UPPER;
    }

    /* Create eta vector and exchange basis */
    int rc = cxf_pivot_with_eta(basis, leavingRow, pivotCol,
                                entering, leaving, leave_status);
    state->eta_count = basis->eta_count;
    return rc;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief BFRT: find minimum-ratio non-flipped row.
 *
 * Scans ALL non-flipped basic variables for the minimum non-negative
 * ratio. This is the next variable that would block the entering
 * variable from moving further. Ties broken by largest |pivot element|.
 *
 * Unlike the previous version, this does NOT skip ratios below cur_step.
 * The caller decides whether the blocker falls before or after the flip
 * extension point.
 *
 * @return Row index of next blocker, or -1 if none.
 */
static int find_next_blocker(SolverState *state, const double *pivotCol,
                             const int *flipped, int num_flipped,
                             int entering_sign,
                             double *out_ratio, double *out_pivot) {
    int best_row = -1;
    double best_ratio = CXF_INFINITY;
    double best_pivot = 0.0;
    int m = state->num_constrs;
    BasisState *basis = state->basis;

    for (int i = 0; i < m; i++) {
        /* Skip flipped rows */
        int skip = 0;
        for (int f = 0; f < num_flipped; f++) {
            if (flipped[f] == i) { skip = 1; break; }
        }
        if (skip) continue;

        double d_i = pivotCol[i];
        double sd_i = entering_sign * d_i;
        if (fabs(sd_i) < CXF_PIVOT_TOL) continue;

        int bv = basis->basic_vars[i];
        if (bv < 0) continue;

        double x = state->work_x[bv];
        double ratio;
        if (sd_i > 0)
            ratio = (x - state->work_lb[bv]) / sd_i;
        else
            ratio = (x - state->work_ub[bv]) / sd_i;

        if (ratio < -CXF_FEASIBILITY_TOL) continue;

        /* Standard minimum-ratio selection with pivot tie-breaking */
        if (ratio < best_ratio - CXF_FEASIBILITY_TOL) {
            best_ratio = ratio;
            best_row = i;
            best_pivot = d_i;
        } else if (ratio <= best_ratio + CXF_FEASIBILITY_TOL) {
            if (fabs(d_i) > fabs(best_pivot)) {
                best_row = i;
                best_pivot = d_i;
            }
        }
    }

    if (out_ratio) *out_ratio = best_ratio;
    if (out_pivot) *out_pivot = best_pivot;
    return best_row;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Compute step size from ratio test result.
 */
static double compute_step(SolverState *state, int leavingRow,
                           double pivotElement, int entering,
                           int entering_sign) {
    BasisState *basis = state->basis;
    int leaving = basis->basic_vars[leavingRow];
    double x_l = state->work_x[leaving];
    double step;

    /* Use effective pivot direction s * pivotElement to determine
     * which bound the leaving variable hits.
     * Phase I: basic vars can be outside [lb, ub], check both bounds. */
    double sp = entering_sign * pivotElement;
    double lb_l = state->work_lb[leaving];
    double ub_l = state->work_ub[leaving];
    step = CXF_INFINITY;
    if (sp > 0 && lb_l > -CXF_INFINITY) {
        double r = (x_l - lb_l) / sp;
        if (r >= 0 && r < step) step = r;
    }
    if (sp > 0 && ub_l < CXF_INFINITY && x_l > ub_l + CXF_FEASIBILITY_TOL) {
        double r = (x_l - ub_l) / sp;
        if (r >= 0 && r < step) step = r;
    }
    if (sp < 0 && ub_l < CXF_INFINITY) {
        double r = (x_l - ub_l) / sp;
        if (r >= 0 && r < step) step = r;
    }
    if (sp < 0 && lb_l > -CXF_INFINITY && x_l < lb_l - CXF_FEASIBILITY_TOL) {
        double r = (x_l - lb_l) / sp;
        if (r >= 0 && r < step) step = r;
    }
    if (step >= CXF_INFINITY) step = 0;

    /* Limit by entering variable's bound range */
    if (basis->var_status[entering] == CXF_VAR_AT_LOWER) {
        double max_s = state->work_ub[entering] - state->work_x[entering];
        if (max_s < step) step = max_s;
    } else if (basis->var_status[entering] == CXF_VAR_AT_UPPER) {
        double max_s = state->work_x[entering] - state->work_lb[entering];
        if (max_s < step) step = max_s;
    }
    if (step < 0) step = 0;
    return step;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Incremental reduced cost update via BTRAN result.
 */
static void update_reduced_costs(SolverState *state, int entering,
                                 int leaving, double d_entering,
                                 double pivotElement, const double *rho) {
    int n = state->num_vars;
    int m = state->num_constrs;
    int total = n + m;
    BasisState *basis = state->basis;
    double step_dual = d_entering / pivotElement;

    state->work_dj[entering] = 0.0;
    state->work_dj[leaving] = -step_dual;

    for (int j = 0; j < total; j++) {
        if (j == entering || j == leaving) continue;
        if (basis->var_status[j] >= 0) continue;

        double rho_aj = 0.0;
        if (j < n && state->csc_col_ptr != NULL) {
            int64_t s = state->csc_col_ptr[j];
            int64_t e = state->csc_col_ptr[j + 1];
            for (int64_t k = s; k < e; k++)
                rho_aj += rho[state->csc_row_idx[k]]
                        * state->csc_values[k];
        } else if (j >= n && j < n + m) {
            /* Slack/surplus: use diag_coeff */
            int row = j - n;
            double coeff = (basis->diag_coeff) ?
                basis->diag_coeff[row] :
                get_auxiliary_coeff_fallback(state, row);
            rho_aj = rho[row] * coeff;
        }
        state->work_dj[j] -= step_dual * rho_aj;
    }
}

/*===========================================================================
 * Phase 1+2: Pricing selection → FTRAN → quality check → ratio test
 *
 * Selects entering variable via multi-level pricing, computes pivot
 * column, runs Harris ratio test, and computes step size.
 *
 * Returns ITERATE_OPTIMAL if no entering variable found,
 * ITERATE_UNBOUNDED/ITERATE_INFEASIBLE on early termination,
 * ITERATE_CONTINUE on successful bound flip (no pivot needed),
 * or CXF_OK when a pivot should proceed.
 *===========================================================================*/
static int pricing_and_ftran(SolverState *state, CxfEnv *env,
                             int *out_entering, int *out_entering_sign,
                             int *out_leavingRow, double *out_pivotElement,
                             double *out_stepSize) {
    BasisState *basis = state->basis;
    int m = state->num_constrs;
    int n = state->num_vars;
    int total = n + m;
    double *pivotCol = basis->work;
    double *column = state->work_column;

    /* Multi-level pricing (v2 P2.3 + P3.20 + P4.4/P4.5) */
    int candidates[MAX_CANDIDATES];
    int num_cand = 0;

    if (state->pricing)
        cxf_pricing_update_queues(state->pricing, state);

    for (int level = 0; level <= 2; level++) {
        if (state->pricing)
            cxf_pricing_set_level(state->pricing, level);

        double pricing_tol;
        if (level == 0)      pricing_tol = env->optimality_tol * 10.0;
        else if (level == 1) pricing_tol = env->optimality_tol;
        else                 pricing_tol = env->optimality_tol * 0.1;

        num_cand = 0;
        if (state->use_bland) {
            for (int j = 0; j < total && num_cand < MAX_CANDIDATES; j++) {
                if (basis->var_status[j] >= 0) continue;
                if (state->work_ub[j] <=
                    state->work_lb[j] + CXF_FEASIBILITY_TOL) continue;
                double rc = state->work_dj[j];
                if (basis->var_status[j] == CXF_VAR_AT_LOWER && rc < -pricing_tol)
                    candidates[num_cand++] = j;
                else if (basis->var_status[j] == CXF_VAR_AT_UPPER && rc > pricing_tol)
                    candidates[num_cand++] = j;
            }
        } else if (state->pricing) {
            int v2_count = 0;
            int *v2_cands = NULL;
            cxf_pricing_candidates_v2(state->pricing, state,
                                      &v2_count, &v2_cands);
            if (v2_count > 0 && v2_cands != NULL) {
                for (int k = 0; k < v2_count; k++) {
                    int j = v2_cands[k];
                    if (j < 0 || j >= total) continue;
                    if (basis->var_status[j] >= 0) continue;
                    if (state->work_ub[j] <=
                        state->work_lb[j] + CXF_FEASIBILITY_TOL) continue;
                    double rc = state->work_dj[j];
                    if ((basis->var_status[j] == CXF_VAR_AT_LOWER && rc < -pricing_tol) ||
                        (basis->var_status[j] == CXF_VAR_AT_UPPER && rc > pricing_tol)) {
                        if (num_cand < MAX_CANDIDATES) candidates[num_cand++] = j;
                    }
                }
            }
            if (num_cand == 0) {
                double best = -pricing_tol;
                for (int j = 0; j < total; j++) {
                    if (basis->var_status[j] >= 0) continue;
                    if (state->work_ub[j] <=
                        state->work_lb[j] + CXF_FEASIBILITY_TOL) continue;
                    double rc = state->work_dj[j];
                    if (basis->var_status[j] == CXF_VAR_AT_LOWER && rc < best) {
                        best = rc; candidates[0] = j; num_cand = 1;
                    } else if (basis->var_status[j] == CXF_VAR_AT_UPPER && -rc < best) {
                        best = -rc; candidates[0] = j; num_cand = 1;
                    }
                }
            }
        } else {
            double best = -pricing_tol;
            for (int j = 0; j < total; j++) {
                if (basis->var_status[j] >= 0) continue;
                if (state->work_ub[j] <=
                    state->work_lb[j] + CXF_FEASIBILITY_TOL) continue;
                double rc = state->work_dj[j];
                if (basis->var_status[j] == CXF_VAR_AT_LOWER && rc < best) {
                    best = rc; candidates[0] = j; num_cand = 1;
                } else if (basis->var_status[j] == CXF_VAR_AT_UPPER && -rc < best) {
                    best = -rc; candidates[0] = j; num_cand = 1;
                }
            }
        }
        if (num_cand > 0) break;
        if (state->pricing) {
            cxf_pricing_end_level(state->pricing);
            state->pricing->level_escalations++;
        }
    }

    if (num_cand == 0) return ITERATE_OPTIMAL;

    /* Per-candidate evaluation: FTRAN + quality check + ratio test */
    int entering = -1, leavingRow = -1;
    double pivotElement = 0.0, stepSize = 0.0;
    int entering_sign = 1;
    int rc;

    for (int ci = 0; ci < num_cand; ci++) {
        entering = candidates[ci];
        entering_sign = (basis->var_status[entering] == CXF_VAR_AT_UPPER) ? -1
            : (basis->var_status[entering] == CXF_VAR_SUPERBASIC &&
               state->work_dj[entering] > 0.0) ? -1 : 1;

        if (state->work_lb[entering] >
            state->work_ub[entering] + env->feasibility_tol)
            return ITERATE_INFEASIBLE;

        extract_column_ext(state, entering, column);
        rc = cxf_ftran(basis, column, pivotCol);
        if (rc != CXF_OK) return rc;

        /* FTRAN quality: NaN/Inf + residual monitoring */
        {
            int need_refactor = 0;
            for (int ri = 0; ri < m; ri++)
                if (!isfinite(pivotCol[ri])) { need_refactor = 1; break; }

            if (!need_refactor && state->eta_count > 10 &&
                state->iteration % 20 == 0) {
                double residual = cxf_ftran_residual(state, column, pivotCol);
                if (residual > 10.0 * env->feasibility_tol) {
                    need_refactor = 1;
                    int new_limit = state->eta_count;
                    if (new_limit < 25) new_limit = 25;
                    if (state->thresholds[5] <= 0 ||
                        new_limit < state->thresholds[5])
                        state->thresholds[5] = new_limit;
                }
            }
            if (need_refactor) {
                cxf_solver_refactor(state, env);
                cxf_recompute_xB(state);
                cxf_recompute_objective(state);
                cxf_compute_reduced_costs(state);
                extract_column_ext(state, entering, column);
                rc = cxf_ftran(basis, column, pivotCol);
                if (rc != CXF_OK) return rc;
            }
        }

        rc = cxf_ratio_test(state, env, entering, pivotCol, m,
                            &leavingRow, &pivotElement);
        if (rc == CXF_UNBOUNDED) {
            if (ci + 1 < num_cand) continue;
            double lb_e = state->work_lb[entering];
            double ub_e = state->work_ub[entering];
            if (lb_e > -CXF_INFINITY && ub_e < CXF_INFINITY) {
                double old_x = state->work_x[entering];
                double new_x = (entering_sign > 0) ? ub_e : lb_e;
                double delta = new_x - old_x;
                state->work_x[entering] = new_x;
                basis->var_status[entering] = (entering_sign > 0)
                    ? CXF_VAR_AT_UPPER : CXF_VAR_AT_LOWER;
                for (int ii = 0; ii < m; ii++)
                    state->work_x[basis->basic_vars[ii]] -= delta * pivotCol[ii];
                state->obj_value += state->work_dj[entering] * delta;
                state->iteration++;
                return ITERATE_CONTINUE;
            }
            if (state->phase == 1) {
                cxf_solver_refactor(state, env);
                cxf_recompute_xB(state);
                cxf_recompute_objective(state);
                cxf_compute_reduced_costs(state);
                state->iteration++;
                return ITERATE_CONTINUE;
            }
            return ITERATE_UNBOUNDED;
        }
        if (rc != CXF_OK) return rc;
        if (fabs(pivotElement) < CXF_PIVOT_TOL) {
            if (state->use_bland && ci + 1 < num_cand) continue;
            return CXF_NUMERIC;
        }

        stepSize = compute_step(state, leavingRow, pivotElement, entering,
                                entering_sign);
        if (state->use_bland && stepSize < 1e-8 && ci + 1 < num_cand)
            continue;
        break;
    }

    *out_entering = entering;
    *out_entering_sign = entering_sign;
    *out_leavingRow = leavingRow;
    *out_pivotElement = pivotElement;
    *out_stepSize = stepSize;
    return CXF_OK;
}

/*===========================================================================
 * Phases 6-9: Post-pivot updates (objective, RC, weights, pricing, refactor)
 *===========================================================================*/
static void post_pivot_updates(SolverState *state, CxfEnv *env,
                               int entering, int leaving, int leavingRow,
                               int entering_sign, double d_entering,
                               double pivotElement, double stepSize,
                               const double *pivotCol, const double *rho,
                               int btran_ok,
                               const int *flipped_rows, int num_flips) {
    BasisState *basis = state->basis;
    int m = state->num_constrs;
    int total = state->num_vars + m;

    /* Phase 6: Update objective */
    if (state->phase == 1) {
        for (int j = 0; j < total; j++) state->work_obj[j] = 0.0;
        double p1_obj = 0.0;
        for (int i = 0; i < m; i++) {
            int bv = basis->basic_vars[i];
            if (bv < 0 || bv >= total) continue;
            double xv = state->work_x[bv];
            double lbv = state->work_lb[bv], ubv = state->work_ub[bv];
            if (xv < lbv - CXF_FEASIBILITY_TOL) {
                state->work_obj[bv] = -1.0; p1_obj += (lbv - xv);
            } else if (xv > ubv + CXF_FEASIBILITY_TOL) {
                state->work_obj[bv] = +1.0; p1_obj += (xv - ubv);
            }
        }
        state->obj_value = p1_obj;
        if (leaving >= 0 && leaving < total) state->work_obj[leaving] = 0.0;
    } else {
        state->obj_value += entering_sign * d_entering * stepSize;
    }

    /* Phase 7: Reduced cost update */
    if (state->phase == 1) {
        cxf_compute_reduced_costs(state);
    } else if (btran_ok) {
        update_reduced_costs(state, entering, leaving,
                             d_entering, pivotElement, rho);
    } else {
        cxf_compute_reduced_costs(state);
    }

    /* Phase 7b: Steepest edge / Devex weight update */
    if (state->pricing && state->pricing->weights != NULL)
        cxf_pricing_update_weights(state->pricing, state, entering,
                                   leavingRow, pivotCol,
                                   btran_ok ? rho : NULL);

    /* Phase 8: V2 pricing notification */
    if (state->pricing) {
        cxf_pricing_update_var(state->pricing, state, entering);
        cxf_pricing_update_constr(state->pricing, state, leavingRow);
        for (int f = 0; f < num_flips; f++) {
            int bv = basis->basic_vars[flipped_rows[f]];
            if (bv >= 0 && bv < total)
                cxf_pricing_update_var(state->pricing, state, bv);
        }
    }

    /* Phase 9: Refactorization check */
    if (cxf_refactor_check(state, env) > 0) {
        cxf_solver_refactor(state, env);
        cxf_recompute_xB(state);
        cxf_recompute_objective(state);
        cxf_compute_reduced_costs(state);
    }
}

/*===========================================================================
 * cxf_simplex_step — V2 simplex iteration engine (P3.20)
 *
 * Orchestrates: pricing → FTRAN → ratio test → BFRT → BTRAN → pivot →
 *               objective → RC → weights → pricing cascade → refactor
 *===========================================================================*/
int cxf_simplex_step(SolverState *state, CxfEnv *env) {
    if (state == NULL || env == NULL) return CXF_ERROR_NULL_ARGUMENT;
    BasisState *basis = state->basis;
    if (basis == NULL) return CXF_ERROR_NULL_ARGUMENT;
    int m = state->num_constrs;
    int total = state->num_vars + m;
    if (m == 0) { state->iteration++; return ITERATE_OPTIMAL; }
    if (state->csc_col_ptr == NULL) return CXF_ERROR_NULL_ARGUMENT;
    if (!basis->work || !state->work_column) return CXF_ERROR_OUT_OF_MEMORY;

    /* Phase 1+2: Select entering variable, FTRAN, ratio test, step size */
    int entering = -1, entering_sign = 1, leavingRow = -1;
    double pivotElement = 0.0, stepSize = 0.0;
    int rc = pricing_and_ftran(state, env, &entering, &entering_sign,
                               &leavingRow, &pivotElement, &stepSize);
    if (rc != CXF_OK) return rc;

    double *pivotCol = basis->work;

    /* Phase 3: BFRT — extend step via bound flips (Koberstein 2005) */
    int flipped_rows[MAX_BFRT_FLIPS];
    int num_flips = 0;
    if (!state->use_bland) {
        int blocker_row = leavingRow;
        while (num_flips < MAX_BFRT_FLIPS) {
            int bv = basis->basic_vars[blocker_row];
            if (state->work_lb[bv] <= -CXF_INFINITY ||
                state->work_ub[bv] >= CXF_INFINITY ||
                (state->work_ub[bv] - state->work_lb[bv]) < CXF_FEASIBILITY_TOL)
                break;
            flipped_rows[num_flips++] = blocker_row;
            double sd = fabs(entering_sign * pivotCol[blocker_row]);
            if (sd < CXF_PIVOT_TOL) break;
            stepSize += (state->work_ub[bv] - state->work_lb[bv]) / sd;
            double next_ratio, next_pivot;
            int next_row = find_next_blocker(state, pivotCol, flipped_rows,
                                             num_flips, entering_sign,
                                             &next_ratio, &next_pivot);
            if (next_row < 0) break;
            if (next_ratio < stepSize) stepSize = next_ratio;
            blocker_row = next_row;
            leavingRow = next_row;
            pivotElement = next_pivot;
        }
    }
    state->flip_count += num_flips;

    /* Cycling detection */
    if (stepSize < 1e-8) {
        state->degenerate_count++;
        if (!state->use_bland && state->degenerate_count > 50)
            state->use_bland = 1;
    } else {
        state->degenerate_count = 0;
    }

    /* Phase 4: BTRAN (before pivot modifies basis) */
    int leaving = basis->basic_vars[leavingRow];
    double d_entering = state->work_dj[entering];
    double *rho = state->work_cB;
    int btran_ok = (rho != NULL && cxf_btran(basis, leavingRow, rho) == CXF_OK);

    /* Phase 5: Execute pivot */
    if (num_flips > 0) {
        for (int i = 0; i < m; i++) {
            int bv = basis->basic_vars[i];
            if (bv >= 0 && bv < total)
                state->work_x[bv] -= stepSize * pivotCol[i];
        }
        for (int f = 0; f < num_flips; f++) {
            int row = flipped_rows[f];
            int bv = basis->basic_vars[row];
            if (entering_sign * pivotCol[row] > 0)
                state->work_x[bv] = state->work_ub[bv];
            else
                state->work_x[bv] = state->work_lb[bv];
        }
        if (basis->var_status[entering] == CXF_VAR_AT_LOWER)
            state->work_x[entering] += stepSize;
        else
            state->work_x[entering] -= stepSize;
        int lv_status = CXF_VAR_AT_LOWER;
        if (leaving >= 0 && leaving < total &&
            fabs(state->work_x[leaving] - state->work_ub[leaving]) <
            fabs(state->work_x[leaving] - state->work_lb[leaving]) &&
            state->work_ub[leaving] < CXF_INFINITY)
            lv_status = CXF_VAR_AT_UPPER;
        rc = cxf_pivot_with_eta(basis, leavingRow, pivotCol,
                                entering, leaving, lv_status);
        if (rc != CXF_OK) return rc;
        state->eta_count = basis->eta_count;
    } else {
        rc = cxf_apply_pivot(state, entering, leavingRow, pivotCol, stepSize);
        if (rc != CXF_OK) return rc;
    }

    /* Phases 6-9: Objective, RC, weights, pricing cascade, refactor */
    post_pivot_updates(state, env, entering, leaving, leavingRow,
                       entering_sign, d_entering, pivotElement, stepSize,
                       pivotCol, rho, btran_ok, flipped_rows, num_flips);

    state->iteration++;
    return ITERATE_CONTINUE;
}
