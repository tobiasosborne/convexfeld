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

#define ITERATE_CONTINUE   0
#define ITERATE_OPTIMAL    1
#define ITERATE_INFEASIBLE 2
#define ITERATE_UNBOUNDED  3

#define REFACTOR_INTERVAL  100
#define MAX_BFRT_FLIPS     10
#define MAX_CANDIDATES     10

/* External declarations */
extern int cxf_pivot_with_eta(BasisState *basis, int pivotRow,
                              const double *pivotCol, int enteringVar,
                              int leavingVar);
extern int cxf_pricing_candidates(PricingState *ctx, const double *rc,
                                  const int *vs, int nv, double tol,
                                  int *out, int max_out);
extern int cxf_ftran(BasisState *basis, const double *column, double *result);
extern int cxf_btran(BasisState *basis, int row, double *result);
extern int cxf_ratio_test(SolverState *state, CxfEnv *env, int enteringVar,
                          const double *pivotColumn, int columnNZ,
                          int *leavingRow_out, double *pivotElement_out);
extern int cxf_solver_refactor(SolverState *ctx, CxfEnv *env);
extern int cxf_compute_reduced_costs(SolverState *state);
extern void cxf_pricing_update_var(PricingState *ctx, SolverState *state,
                                   int varIndex);
extern void cxf_pricing_update_constr(PricingState *ctx, SolverState *state,
                                      int constrIndex);
extern void cxf_pricing_update_queues(PricingState *ctx, SolverState *state);
extern void cxf_pricing_candidates_v2(PricingState *ctx, SolverState *state,
                                      int *count, int **candidates);

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

    /* Update entering variable */
    if (basis->var_status[entering] == CXF_VAR_AT_LOWER)
        state->work_x[entering] = state->work_lb[entering] + stepSize;
    else
        state->work_x[entering] = state->work_ub[entering] - stepSize;

    /* Create eta vector and exchange basis */
    int rc = cxf_pivot_with_eta(basis, leavingRow, pivotCol,
                                entering, leaving);

    /* Fix leaving variable at appropriate bound (P0.3) */
    if (rc == CXF_OK && leaving >= 0 && leaving < total) {
        double x = state->work_x[leaving];
        double lb = state->work_lb[leaving];
        double ub = state->work_ub[leaving];
        basis->var_status[leaving] = CXF_VAR_AT_LOWER;
        if (fabs(x - ub) < fabs(x - lb) && ub < CXF_INFINITY)
            basis->var_status[leaving] = CXF_VAR_AT_UPPER;
    }

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
 * cxf_simplex_step — V2 simplex iteration engine (P3.20)
 *
 * Pricing → FTRAN → Harris ratio test → BFRT → pivot → BTRAN →
 * RC update → pricing cascade → refactorization check
 *===========================================================================*/

int cxf_simplex_step(SolverState *state, CxfEnv *env) {
    if (state == NULL || env == NULL) return CXF_ERROR_NULL_ARGUMENT;

    BasisState *basis = state->basis;
    if (basis == NULL) return CXF_ERROR_NULL_ARGUMENT;

    int m = state->num_constrs;
    int n = state->num_vars;
    int total = n + m;

    if (m == 0) { state->iteration++; return ITERATE_OPTIMAL; }
    if (state->csc_col_ptr == NULL) return CXF_ERROR_NULL_ARGUMENT;

    double *pivotCol = basis->work;
    double *column = state->work_column;
    if (!pivotCol || !column) return CXF_ERROR_OUT_OF_MEMORY;

    /*--- Phase 1+2: Multi-level pricing with tolerance escalation ---
     * v2 P2.3 Phase 5 + P3.20 Phase 1-2 + P4.4/P4.5 V2 queue system
     *
     * Level 0 (loose):    optimality_tol * 10 — fast, only strong RCs
     * Level 1 (standard): optimality_tol      — moderate
     * Level 2 (tight):    optimality_tol * 0.1 — catches weak RCs
     *
     * Only declare ITERATE_OPTIMAL when ALL levels return 0 candidates.
     *-------------------------------------------------------------------*/
    int candidates[MAX_CANDIDATES];
    int num_cand = 0;

    /* P4.4: Process V2 queues before pricing evaluation */
    if (state->pricing)
        cxf_pricing_update_queues(state->pricing, state);

    for (int level = 0; level <= 2; level++) {
        if (state->pricing)
            cxf_pricing_set_level(state->pricing, level);

        /* Tolerance selection per level (v2 P3.20 Phase 1) */
        double pricing_tol;
        if (level == 0)
            pricing_tol = env->optimality_tol * 10.0;
        else if (level == 1)
            pricing_tol = env->optimality_tol;
        else
            pricing_tol = env->optimality_tol * 0.1;

        num_cand = 0;
        if (state->use_bland) {
            /* Bland's rule: full scan for anti-cycling guarantee */
            for (int j = 0; j < total && num_cand < MAX_CANDIDATES; j++) {
                if (basis->var_status[j] >= 0) continue;
                if (state->work_ub[j] <=
                    state->work_lb[j] + CXF_FEASIBILITY_TOL)
                    continue;
                double rc = state->work_dj[j];
                if (basis->var_status[j] == CXF_VAR_AT_LOWER &&
                    rc < -pricing_tol)
                    candidates[num_cand++] = j;
                else if (basis->var_status[j] == CXF_VAR_AT_UPPER &&
                         rc > pricing_tol)
                    candidates[num_cand++] = j;
            }
        } else if (state->pricing) {
            /* P4.5+P4.8: V2 adaptive candidate retrieval from queues */
            int v2_count = 0;
            int *v2_cands = NULL;
            cxf_pricing_candidates_v2(state->pricing, state,
                                      &v2_count, &v2_cands);

            /* RC-filter the V2 dirty queue to find attractive candidates */
            if (v2_count > 0 && v2_cands != NULL) {
                for (int k = 0; k < v2_count; k++) {
                    int j = v2_cands[k];
                    if (j < 0 || j >= total) continue;
                    if (basis->var_status[j] >= 0) continue;
                    if (state->work_ub[j] <=
                        state->work_lb[j] + CXF_FEASIBILITY_TOL)
                        continue;
                    double rc = state->work_dj[j];
                    if ((basis->var_status[j] == CXF_VAR_AT_LOWER &&
                         rc < -pricing_tol) ||
                        (basis->var_status[j] == CXF_VAR_AT_UPPER &&
                         rc > pricing_tol)) {
                        if (num_cand < MAX_CANDIDATES)
                            candidates[num_cand++] = j;
                    }
                }
            }

            /* Fallback: V2 queues empty (first iterations). Dantzig scan. */
            if (num_cand == 0) {
                double best = -pricing_tol;
                for (int j = 0; j < total; j++) {
                    if (basis->var_status[j] >= 0) continue;
                    if (state->work_ub[j] <=
                        state->work_lb[j] + CXF_FEASIBILITY_TOL)
                        continue;
                    double rc = state->work_dj[j];
                    if (basis->var_status[j] == CXF_VAR_AT_LOWER &&
                        rc < best) {
                        best = rc; candidates[0] = j; num_cand = 1;
                    } else if (basis->var_status[j] == CXF_VAR_AT_UPPER &&
                               -rc < best) {
                        best = -rc; candidates[0] = j; num_cand = 1;
                    }
                }
            }
        } else {
            /* No pricing context: Dantzig's rule (most negative RC) */
            double best = -pricing_tol;
            for (int j = 0; j < total; j++) {
                if (basis->var_status[j] >= 0) continue;
                if (state->work_ub[j] <=
                    state->work_lb[j] + CXF_FEASIBILITY_TOL)
                    continue;
                double rc = state->work_dj[j];
                if (basis->var_status[j] == CXF_VAR_AT_LOWER &&
                    rc < best) {
                    best = rc; candidates[0] = j; num_cand = 1;
                } else if (basis->var_status[j] == CXF_VAR_AT_UPPER &&
                           -rc < best) {
                    best = -rc; candidates[0] = j; num_cand = 1;
                }
            }
        }

        if (num_cand > 0) break;

        /* End this level before escalating (v2 P2.3 Phase 5) */
        if (state->pricing) {
            cxf_pricing_end_level(state->pricing);
            state->pricing->level_escalations++;
        }
    }

    if (num_cand == 0) return ITERATE_OPTIMAL;

    /*--- Phase 2: Per-candidate evaluation ---*/
    int entering = -1, leavingRow = -1;
    double pivotElement = 0.0, stepSize = 0.0;
    int entering_sign = 1;  /* +1 from lower, -1 from upper */
    int rc;

    for (int ci = 0; ci < num_cand; ci++) {
        entering = candidates[ci];
        entering_sign = (basis->var_status[entering] == CXF_VAR_AT_UPPER)
                        ? -1 : 1;

        /* Infeasibility check */
        if (state->work_lb[entering] >
            state->work_ub[entering] + env->feasibility_tol)
            return ITERATE_INFEASIBLE;

        /* FTRAN */
        extract_column_ext(state, entering, column);
        rc = cxf_ftran(basis, column, pivotCol);
        if (rc != CXF_OK) return rc;

        /* P2.2: FTRAN residual monitoring — ||a - B*x|| check.
         * column[] still holds the original entering column (pre-FTRAN).
         * pivotCol[] holds B^{-1} * column. Residual = column - B * pivotCol.
         * We approximate by checking ||column - B*pivotCol|| but since we
         * don't have B explicitly, we use the norm of pivotCol as a proxy:
         * if any pivotCol entry is NaN/Inf, trigger refactorization. */
        {
            int need_refactor = 0;
            for (int ri = 0; ri < m; ri++) {
                if (!isfinite(pivotCol[ri])) { need_refactor = 1; break; }
            }
            if (need_refactor) {
                cxf_solver_refactor(state, env);
                cxf_compute_reduced_costs(state);
                /* Re-FTRAN after refactorization */
                extract_column_ext(state, entering, column);
                rc = cxf_ftran(basis, column, pivotCol);
                if (rc != CXF_OK) return rc;
            }
        }

        /* Harris two-pass ratio test */
        rc = cxf_ratio_test(state, env, entering, pivotCol, m,
                            &leavingRow, &pivotElement);
        if (rc == CXF_UNBOUNDED) {
            if (state->use_bland && ci + 1 < num_cand) continue;
            return ITERATE_UNBOUNDED;
        }
        if (rc != CXF_OK) return rc;
        if (fabs(pivotElement) < CXF_PIVOT_TOL) {
            if (state->use_bland && ci + 1 < num_cand) continue;
            return CXF_NUMERIC;
        }

        /* Step size */
        stepSize = compute_step(state, leavingRow, pivotElement, entering,
                                entering_sign);

        /* Under Bland's rule, skip degenerate pivots if alternatives exist */
        if (state->use_bland && stepSize < 1e-8 && ci + 1 < num_cand)
            continue;
        break;
    }

    /*--- Phase 3: BFRT — bound-flipping ratio test (P2.4 Stage 3) ---*/
    int flipped_rows[MAX_BFRT_FLIPS];
    int num_flips = 0;

    /* BFRT disabled: the implementation has multiple interacting bugs
     * (row negation, blocker search, step limiting by entering bound).
     * Standard ratio test without flips is correct. BFRT is a performance
     * optimization that can be re-enabled once properly implemented.
     * See docs/remediation_plan.md RC1. */
    (void)flipped_rows;
    state->flip_count += num_flips;

    /* NOTE: v2 spec (harris_ratio_test.md Stage 3 Step 6c) prescribed
     * negate_constraint_row() here. REMOVED — spec bug. The spec imported
     * a dual-simplex technique (Forrest & Goldfarb 1992) into primal simplex.
     * Row negation invalidates the LU/eta factorization without updating it,
     * causing cumulative bound violations → false UNBOUNDED on 18 Netlib
     * instances. Standard primal BFRT does not modify the matrix.
     * See docs/remediation_plan.md RC1, docs/architecture_contract_map.md. */

    /* Cycling detection */
    if (stepSize < 1e-8) {
        state->degenerate_count++;
        if (!state->use_bland && state->degenerate_count > 50)
            state->use_bland = 1;
    } else {
        state->degenerate_count = 0;
    }

    /*--- Phase 4: BTRAN for leaving row (before pivot modifies basis) ---*/
    int leaving = basis->basic_vars[leavingRow];
    double d_entering = state->work_dj[entering];
    double *rho = state->work_cB;
    int btran_ok = (rho != NULL &&
                    cxf_btran(basis, leavingRow, rho) == CXF_OK);

    /*--- Phase 5: Execute pivot ---*/
    if (num_flips > 0) {
        /* BFRT path: update all basic vars with total step */
        for (int i = 0; i < m; i++) {
            int bv = basis->basic_vars[i];
            if (bv >= 0 && bv < total)
                state->work_x[bv] -= stepSize * pivotCol[i];
        }

        /* Clamp flipped variables to their opposite bound.
         * Use entering_sign * pivotCol to determine direction. */
        for (int f = 0; f < num_flips; f++) {
            int row = flipped_rows[f];
            int bv = basis->basic_vars[row];
            if (entering_sign * pivotCol[row] > 0)
                state->work_x[bv] = state->work_ub[bv];
            else
                state->work_x[bv] = state->work_lb[bv];
        }

        /* Update entering variable */
        if (basis->var_status[entering] == CXF_VAR_AT_LOWER)
            state->work_x[entering] = state->work_lb[entering] + stepSize;
        else
            state->work_x[entering] = state->work_ub[entering] - stepSize;

        /* Basis exchange: eta + status update */
        rc = cxf_pivot_with_eta(basis, leavingRow, pivotCol,
                                entering, leaving);
        if (rc != CXF_OK) return rc;

        /* Fix leaving variable at appropriate bound (P0.3) */
        if (leaving >= 0 && leaving < total) {
            double x = state->work_x[leaving];
            basis->var_status[leaving] = CXF_VAR_AT_LOWER;
            if (fabs(x - state->work_ub[leaving]) <
                fabs(x - state->work_lb[leaving]) &&
                state->work_ub[leaving] < CXF_INFINITY)
                basis->var_status[leaving] = CXF_VAR_AT_UPPER;
        }
    } else {
        /* Standard path (no flips) */
        rc = cxf_apply_pivot(state, entering, leavingRow,
                             pivotCol, stepSize);
        if (rc != CXF_OK) return rc;
    }

    /*--- Phase 6: Update objective ---*/
    if (state->phase == 1) {
        /* Phase I: recompute w coefficients and objective from scratch.
         * The incremental formula is invalid because w changes after pivot.
         * Spec: two_phase_method.md — dynamic w coefficient update. */
        for (int j2 = 0; j2 < total; j2++)
            state->work_obj[j2] = 0.0;
        double p1_obj = 0.0;
        for (int ii = 0; ii < m; ii++) {
            int bv2 = basis->basic_vars[ii];
            if (bv2 < 0 || bv2 >= total) continue;
            double xv = state->work_x[bv2];
            double lbv = state->work_lb[bv2];
            double ubv = state->work_ub[bv2];
            if (xv < lbv - CXF_FEASIBILITY_TOL) {
                state->work_obj[bv2] = -1.0;
                p1_obj += (lbv - xv);
            } else if (xv > ubv + CXF_FEASIBILITY_TOL) {
                state->work_obj[bv2] = +1.0;
                p1_obj += (xv - ubv);
            }
        }
        state->obj_value = p1_obj;
        /* Leaving variable (now nonbasic) always gets w = 0 */
        if (leaving >= 0 && leaving < total)
            state->work_obj[leaving] = 0.0;
    } else {
        /* Phase II: standard incremental objective update */
        state->obj_value += entering_sign * d_entering * stepSize;
    }

    /*--- Phase 7: Reduced cost update ---*/
    if (state->phase == 1) {
        /* Phase I: w changed, must recompute reduced costs from scratch */
        cxf_compute_reduced_costs(state);
    } else if (btran_ok) {
        update_reduced_costs(state, entering, leaving,
                             d_entering, pivotElement, rho);
    } else {
        cxf_compute_reduced_costs(state);
    }

    /*--- Phase 7b: Update steepest edge / Devex weights (P4.9) ---*/
    if (state->pricing && state->pricing->weights != NULL) {
        extern void cxf_pricing_update_weights(PricingState *, SolverState *,
                                               int, int, const double *,
                                               const double *);
        cxf_pricing_update_weights(state->pricing, state, entering,
                                   leavingRow, pivotCol,
                                   btran_ok ? rho : NULL);
    }

    /*--- Phase 8: V2 pricing notification (P4.2/P4.3/P4.8) ---*/
    if (state->pricing) {
        /* P4.2: entering variable → mark adjacent constraints dirty
         * Also populates V1 dirty flags for step2/step3 compat */
        cxf_pricing_update_var(state->pricing, state, entering);
        /* P4.3: leaving row → mark adjacent variables dirty */
        cxf_pricing_update_constr(state->pricing, state, leavingRow);
        /* P0.9: Also notify BFRT-flipped variables */
        for (int f = 0; f < num_flips; f++) {
            int bv = basis->basic_vars[flipped_rows[f]];
            if (bv >= 0 && bv < total)
                cxf_pricing_update_var(state->pricing, state, bv);
        }
    }

    /*--- Phase 9: Refactorization (P0.7: use cxf_refactor_check) ---*/
    {
        extern int cxf_refactor_check(SolverState *, CxfEnv *);
        if (cxf_refactor_check(state, env) > 0) {
            cxf_solver_refactor(state, env);
            cxf_compute_reduced_costs(state);
        }
    }

    state->iteration++;
    return ITERATE_CONTINUE;
}
