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
#define STEP_CLAMP         1e15   /* numerical_stability.md Section C */

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
                    const double *pivotCol, double stepSize,
                    double reducedCost, int direction) {
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
                                entering, leaving, leave_status,
                                reducedCost, direction);
    state->eta_count = basis->eta_count;
    return rc;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Compute tau_j = rho^T a_j for a single nonbasic variable.
 *
 * Common kernel for RC and weight updates — factors the CSC dot product
 * so it appears exactly once in the codebase (revised_simplex.md Step 6).
 */
static double compute_tau(const SolverState *state, int j,
                          const double *rho) {
    int n = state->num_vars;
    int m = state->num_constrs;
    BasisState *basis = state->basis;

    if (j < n && state->csc_col_ptr != NULL) {
        int64_t s = state->csc_col_ptr[j];
        int64_t e = state->csc_col_ptr[j + 1];
        double t = 0.0;
        for (int64_t k = s; k < e; k++)
            t += rho[state->csc_row_idx[k]] * state->csc_values[k];
        return t;
    }
    if (j >= n && j < n + m) {
        int row = j - n;
        double coeff = (basis->diag_coeff) ?
            basis->diag_coeff[row] :
            get_auxiliary_coeff_fallback(state, row);
        return rho[row] * coeff;
    }
    return 0.0;
}

/**
 * @brief Incremental reduced cost update via BTRAN result.
 */
static void update_reduced_costs(SolverState *state, int entering,
                                 int leaving, double d_entering,
                                 double pivotElement, const double *rho) {
    int total = state->num_vars + state->num_constrs;
    BasisState *basis = state->basis;
    double step_dual = d_entering / pivotElement;

    state->work_dj[entering] = 0.0;
    state->work_dj[leaving] = -step_dual;

    for (int j = 0; j < total; j++) {
        if (j == entering || j == leaving) continue;
        if (basis->var_status[j] >= 0) continue;
        state->work_dj[j] -= step_dual * compute_tau(state, j, rho);
    }
}

/**
 * @brief Fused reduced cost + steepest edge weight update.
 *
 * Both formulas use tau_j = rho^T a_j (revised_simplex.md Step 6).
 * Computing it once per nonbasic variable eliminates a full CSC traversal.
 *
 * RC:  d_j' = d_j - (d_q / alpha_{q,r}) * tau_j
 * DSE: gamma_j' = gamma_j + (tau_j / alpha_{q,r})^2 * (gamma_q - 2*alpha_{q,r} + 1)
 * Devex: gamma_j' = max(0.99 * gamma_j, (tau_j / alpha_{q,r})^2 + 1)
 */
static void update_rc_and_weights(SolverState *state,
                                   int entering, int leaving, int leavingRow,
                                   double d_entering, double pivotElement,
                                   const double *rho) {
    int total = state->num_vars + state->num_constrs;
    BasisState *basis = state->basis;
    PricingState *pricing = state->pricing;
    double step_dual = d_entering / pivotElement;
    double inv_pivot = 1.0 / pivotElement;

    double gamma_q = pricing->weights[entering];
    if (gamma_q < 1e-10) gamma_q = 1.0;
    double dse_factor = gamma_q - 2.0 * pivotElement + 1.0;
    int is_devex = (pricing->strategy == 3);

    state->work_dj[entering] = 0.0;
    state->work_dj[leaving] = -step_dual;

    for (int j = 0; j < total; j++) {
        if (j == entering || j == leaving) continue;
        if (basis->var_status[j] >= 0) continue;

        double tau_j = compute_tau(state, j, rho);

        /* RC update */
        state->work_dj[j] -= step_dual * tau_j;

        /* Weight update */
        if (j < pricing->num_vars) {
            double ratio = tau_j * inv_pivot;
            double r2 = ratio * ratio;
            if (is_devex) {
                double dj = (pricing->ref_framework &&
                             pricing->ref_framework[j]) ? 1.0 : 0.0;
                double dv = r2 + dj;
                double dc = 0.99 * pricing->weights[j];
                pricing->weights[j] = (dv > dc) ? dv : dc;
            } else {
                double nw = pricing->weights[j] + r2 * dse_factor;
                pricing->weights[j] = (nw > 1e-10) ? nw : 1e-10;
            }
        }
    }

    /* Leaving variable weight: gamma_p' = gamma_q / alpha_{q,r}^2 */
    if (leaving >= 0 && leaving < pricing->num_vars) {
        double nw = gamma_q / (pivotElement * pivotElement);
        pricing->weights[leaving] = (nw > 1e-10) ? nw : 1e-10;
    }

    /* Update Devex reference framework: entering var left nonbasic set */
    if (pricing->ref_framework && entering < pricing->num_vars &&
        pricing->ref_framework[entering]) {
        pricing->ref_framework[entering] = 0;
        pricing->ref_framework_count--;
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
                             double *out_stepSize,
                             int *out_flip_rows, int *out_num_flips) {
    BasisState *basis = state->basis;
    int m = state->num_constrs;
    int n = state->num_vars;
    int total = n + m;
    double *pivotCol = basis->work;
    double *column = state->work_column;

    /* Multi-level pricing (v2 P2.3 + P3.20 + P4.4/P4.5) */
    int *candidates = (int *)malloc((size_t)total * sizeof(int));
    if (candidates == NULL) return CXF_ERROR_OUT_OF_MEMORY;
    int num_cand = 0;
    double pricing_tol = env->optimality_tol;  /* Updated per level */

    if (state->pricing)
        cxf_pricing_update(state->pricing, state);

    for (int level = 0; level <= 2; level++) {
        if (state->pricing)
            cxf_pricing_set_level(state->pricing, level);

        /* tolerances_constants.md §4: adaptive pricing tolerance */
        if (level == 0)      pricing_tol = env->optimality_tol;         /* Fast ~1e-6  */
        else if (level == 1) pricing_tol = env->optimality_tol * 1e-4;  /* Standard ~1e-10 */
        else                 pricing_tol = env->optimality_tol * 1e-3;  /* Aggressive ~1e-9 */

        num_cand = 0;
        if (state->use_bland) {
            for (int j = 0; j < total; j++) {
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
            cxf_pricing_candidates(state->pricing, state,
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
                        candidates[num_cand++] = j;
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
            cxf_pricing_end_level(state->pricing, state);
            state->pricing->level_escalations++;
        }
    }

    if (num_cand == 0) { free(candidates); return ITERATE_OPTIMAL; }

    /* Per-candidate evaluation: FTRAN + quality check + ratio test */
    int entering = -1, leavingRow = -1;
    double pivotElement = 0.0, stepSize = 0.0;
    int entering_sign = 1;
    int ret = CXF_OK;
    int rc;

    for (int ci = 0; ci < num_cand; ci++) {
        entering = candidates[ci];
        entering_sign = (basis->var_status[entering] == CXF_VAR_AT_UPPER) ? -1
            : (basis->var_status[entering] == CXF_VAR_SUPERBASIC &&
               state->work_dj[entering] > 0.0) ? -1 : 1;

        if (state->work_lb[entering] >
            state->work_ub[entering] + env->feasibility_tol) {
            ret = ITERATE_INFEASIBLE; goto cleanup;
        }

        /* Phase 3.2: Tight-bound handling (simplex_iteration.md).
         * Variables with bound range at or below pricing tolerance
         * are routed to cxf_pivot_primal for safe elimination.
         * Only structural variables — slacks are handled by constraint logic. */
        if (entering < n) {
            double bound_range = state->work_ub[entering] - state->work_lb[entering];
            if (bound_range >= 0 && bound_range <= pricing_tol) {
                int pp_rc = cxf_pivot_primal(env, state, entering, pricing_tol);
                if (pp_rc == CXF_INFEASIBLE) { ret = ITERATE_INFEASIBLE; goto cleanup; }
                if (pp_rc != CXF_OK && pp_rc != 0) { ret = pp_rc; goto cleanup; }
                state->iteration++;
                ret = ITERATE_CONTINUE; goto cleanup;
            }
        }

        extract_column_ext(state, entering, column);
        rc = cxf_ftran(basis, column, pivotCol);

        /* FTRAN quality: error recovery, NaN/Inf, residual monitoring.
         * A degraded eta chain can cause FTRAN to fail (e.g. non-finite
         * pivot element after many degenerate pivots). Treat any FTRAN
         * error the same as NaN detection: refactorize and retry. */
        {
            int need_refactor = (rc != CXF_OK);
            if (!need_refactor)
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
                if (state->pricing)
                    cxf_pricing_recompute_weights(state->pricing, state);
                extract_column_ext(state, entering, column);
                rc = cxf_ftran(basis, column, pivotCol);
                if (rc != CXF_OK) { ret = rc; goto cleanup; }
                /* numerical_stability.md §C: NaN/Inf scan after retry FTRAN */
                for (int ri = 0; ri < m; ri++)
                    if (!isfinite(pivotCol[ri])) { ret = CXF_NUMERIC; goto cleanup; }
            }
        }

        int rt_status = CXF_RT_NORMAL_PIVOT;
        double rt_theta = 0.0;
        int rt_nflips = 0;
        rc = cxf_ratio_test(state, env, entering, pivotCol, m,
                            &leavingRow, &pivotElement, &rt_status,
                            &rt_theta,
                            out_flip_rows, MAX_BFRT_FLIPS, &rt_nflips);
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
                ret = ITERATE_CONTINUE; goto cleanup;
            }
            ret = ITERATE_UNBOUNDED; goto cleanup;
        }
        if (rc != CXF_OK) { ret = rc; goto cleanup; }
        if (fabs(pivotElement) < CXF_PIVOT_TOL) {
            if (state->use_bland && ci + 1 < num_cand) continue;
            /* numerical_stability.md §A.4: rejected pivot triggers
             * refactorization recovery — retry once after fresh factors. */
            cxf_solver_refactor(state, env);
            cxf_recompute_xB(state);
            cxf_recompute_objective(state);
            cxf_compute_reduced_costs(state);
            if (state->pricing)
                cxf_pricing_recompute_weights(state->pricing, state);
            extract_column_ext(state, entering, column);
            rc = cxf_ftran(basis, column, pivotCol);
            if (rc != CXF_OK) { ret = CXF_NUMERIC; goto cleanup; }
            /* numerical_stability.md §C: NaN/Inf scan after recovery FTRAN */
            for (int ri = 0; ri < m; ri++)
                if (!isfinite(pivotCol[ri])) { ret = CXF_NUMERIC; goto cleanup; }
            rt_status = CXF_RT_NORMAL_PIVOT;
            rt_theta = 0.0;
            rt_nflips = 0;
            rc = cxf_ratio_test(state, env, entering, pivotCol, m,
                                &leavingRow, &pivotElement, &rt_status,
                                &rt_theta,
                                out_flip_rows, MAX_BFRT_FLIPS, &rt_nflips);
            if (rc != CXF_OK) { ret = rc; goto cleanup; }
            if (fabs(pivotElement) < CXF_PIVOT_TOL) {
                ret = CXF_NUMERIC; goto cleanup;
            }
        }

        /* Use theta from ratio_test (includes BFRT), cap by entering bound */
        stepSize = rt_theta;
        if (basis->var_status[entering] == CXF_VAR_AT_LOWER) {
            double max_s = state->work_ub[entering] - state->work_x[entering];
            if (max_s < stepSize) stepSize = max_s;
        } else if (basis->var_status[entering] == CXF_VAR_AT_UPPER) {
            double max_s = state->work_x[entering] - state->work_lb[entering];
            if (max_s < stepSize) stepSize = max_s;
        }
        if (stepSize < 0) stepSize = 0;
        if (state->use_bland && stepSize < 1e-8 && ci + 1 < num_cand)
            continue;
        if (out_num_flips) *out_num_flips = rt_nflips;
        break;
    }

    *out_entering = entering;
    *out_entering_sign = entering_sign;
    *out_leavingRow = leavingRow;
    *out_pivotElement = pivotElement;
    *out_stepSize = stepSize;

cleanup:
    free(candidates);
    return ret;
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
    } else if (num_flips > 0) {
        /* BFRT flips snap basic vars to bounds, breaking the linear
         * objective model. Recompute from scratch: obj = c^T x.
         * Only nonbasic contributions matter (basic c_B absorbed into
         * reduced costs), but the simplest correct formula is c^T x
         * over all variables. This is O(n) but only runs when flips
         * occur (harris_ratio_test.md Stage 3, step 6). */
        double obj = 0.0;
        for (int j = 0; j < total; j++)
            obj += state->work_obj[j] * state->work_x[j];
        state->obj_value = obj;
    } else {
        state->obj_value += entering_sign * d_entering * stepSize;
    }

    /* Phase 7+7b: Reduced cost + weight update.
     * Both use tau_j = rho^T a_j (revised_simplex.md Step 6).
     * When BTRAN succeeded and weights are active, fuse into one pass
     * to avoid traversing CSC columns twice per nonbasic variable. */
    {
        int have_weights = (state->pricing != NULL &&
                            state->pricing->weights != NULL &&
                            (state->pricing->strategy == 2 ||
                             state->pricing->strategy == 3));
        if (state->phase != 1 && btran_ok && have_weights) {
            update_rc_and_weights(state, entering, leaving, leavingRow,
                                  d_entering, pivotElement, rho);
        } else {
            /* Separate paths */
            if (state->phase == 1)
                cxf_compute_reduced_costs(state);
            else if (btran_ok)
                update_reduced_costs(state, entering, leaving,
                                     d_entering, pivotElement, rho);
            else
                cxf_compute_reduced_costs(state);

            if (state->pricing && state->pricing->weights != NULL)
                cxf_pricing_update_weights(state->pricing, state, entering,
                                           leavingRow, pivotCol,
                                           btran_ok ? rho : NULL);
        }
    }

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

    /* Phase 9: Refactorization check.
     * Force immediate refactorization on large steps to prevent
     * catastrophic precision loss (numerical_stability.md Section C). */
    int force_refactor = (stepSize > 1e15);
    if (force_refactor || cxf_refactor_check(state, env) > 0) {
        cxf_solver_refactor(state, env);
        cxf_recompute_xB(state);
        cxf_recompute_objective(state);
        cxf_compute_reduced_costs(state);
        /* Recompute SE/Devex weights from scratch (revised_simplex.md Step 6) */
        if (state->pricing)
            cxf_pricing_recompute_weights(state->pricing, state);
        if (force_refactor) {
            /* Tighten adaptive threshold: future refactorizations sooner */
            int limit = state->eta_count > 25 ? state->eta_count : 25;
            if (state->thresholds[5] <= 0 || limit < state->thresholds[5])
                state->thresholds[5] = limit;
        }
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

    /* Phase 1+2+3: Pricing, FTRAN, ratio test with BFRT (harris_ratio_test.md) */
    int entering = -1, entering_sign = 1, leavingRow = -1;
    double pivotElement = 0.0, stepSize = 0.0;
    int flipped_rows[MAX_BFRT_FLIPS];
    int num_flips = 0;
    int rc = pricing_and_ftran(state, env, &entering, &entering_sign,
                               &leavingRow, &pivotElement, &stepSize,
                               flipped_rows, &num_flips);
    if (rc != CXF_OK) return rc;

    /* V2 numerical_stability.md §C: NaN/Inf detection after step length */
    if (!isfinite(stepSize)) {
        cxf_solver_refactor(state, env);
        cxf_recompute_xB(state);
        cxf_recompute_objective(state);
        cxf_compute_reduced_costs(state);
        if (state->pricing)
            cxf_pricing_recompute_weights(state->pricing, state);
        rc = pricing_and_ftran(state, env, &entering, &entering_sign,
                               &leavingRow, &pivotElement, &stepSize,
                               flipped_rows, &num_flips);
        if (rc != CXF_OK) return rc;
        if (!isfinite(stepSize))
            return CXF_NUMERIC;
    }

    double *pivotCol = basis->work;
    state->flip_count += num_flips;

    /* Cycling detection */
    if (stepSize < 1e-8) {
        state->degenerate_count++;
        state->cumulative_degenerate++;  /* Never resets on good pivots */
        if (!state->use_bland && state->degenerate_count > 50)
            state->use_bland = 1;
    } else {
        state->degenerate_count = 0;
        state->mechanism_a_applied = 0;  /* Reset: stalling episode ended */
    }

    /* Step length clamping (numerical_stability.md Section C) */
    if (stepSize > STEP_CLAMP)
        stepSize = STEP_CLAMP;

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
        /* harris_ratio_test.md Stage 3, steps 6b-6d (primal simplex):
         * Flipped vars remain basic — their values are already set above.
         * (6b) Activity bounds depend on nonbasic ranges, not basic values;
         *      no explicit update needed (recomputed at refactorization).
         * (6c) Row negation is a dual simplex operation; not applicable.
         * (6d) Basic var status is row index, not AT_LOWER/AT_UPPER; no-op.
         * Objective correction handled in post_pivot_updates Phase 6. */
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
                                entering, leaving, lv_status,
                                d_entering, entering_sign);
        if (rc != CXF_OK) return rc;
        state->eta_count = basis->eta_count;
    } else {
        rc = cxf_apply_pivot(state, entering, leavingRow, pivotCol, stepSize,
                             d_entering, entering_sign);
        if (rc != CXF_OK) return rc;
    }

    /* Post-pivot bound projection (numerical_stability.md Section C):
     * "basic variable values should be checked against their bounds
     *  and projected back to the nearest bound if they have overshot."
     * Phase II only — Phase I basic vars are legitimately outside bounds
     * (the Phase I objective tracks violations; projecting defeats it). */
    if (state->phase == 2) {
        for (int i = 0; i < m; i++) {
            int bv = basis->basic_vars[i];
            if (bv < 0 || bv >= total) continue;
            double x = state->work_x[bv];
            if (x < state->work_lb[bv]) state->work_x[bv] = state->work_lb[bv];
            if (x > state->work_ub[bv]) state->work_x[bv] = state->work_ub[bv];
        }
    }

    /* Phases 6-9: Objective, RC, weights, pricing cascade, refactor */
    post_pivot_updates(state, env, entering, leaving, leavingRow,
                       entering_sign, d_entering, pivotElement, stepSize,
                       pivotCol, rho, btran_ok, flipped_rows, num_flips);

    state->iteration++;
    return ITERATE_CONTINUE;
}
