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
#include "convexfeld/cxf_model.h"
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

/*---------------------------------------------------------------------------*/

static double get_auxiliary_coeff_fallback(const MatrixData *matrix, int row) {
    if (matrix == NULL || matrix->sense == NULL) return 1.0;
    char sense = matrix->sense[row];
    double rhs = (matrix->rhs != NULL) ? matrix->rhs[row] : 0.0;
    /* P0.4: unified with get_auxiliary_coeff in reduced_costs.c */
    if (sense == '>' || sense == 'G') return -1.0;
    if (sense == '<' || sense == 'L') return (rhs < 0) ? -1.0 : 1.0;
    if (sense == '=')                 return (rhs < 0) ? -1.0 : 1.0;
    return 1.0;
}

static void extract_column_ext(const MatrixData *matrix, BasisState *basis,
                               int col, int n, int m, double *dense) {
    memset(dense, 0, (size_t)m * sizeof(double));
    if (col < n) {
        if (matrix == NULL) return;
        int64_t start = matrix->col_ptr[col];
        int64_t end = matrix->col_ptr[col + 1];
        for (int64_t k = start; k < end; k++)
            dense[matrix->row_idx[k]] = matrix->values[k];
    } else {
        int row = col - n;
        if (row >= 0 && row < m) {
            double coeff = (basis && basis->diag_coeff) ?
                basis->diag_coeff[row] :
                get_auxiliary_coeff_fallback(matrix, row);
            dense[row] = coeff;
        }
    }
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
 * @brief BFRT: find next blocking variable after a flip.
 *
 * Scans basic variables for the minimum ratio > current step among
 * rows not already flipped. Ties broken by largest |pivot element|.
 *
 * @return Row index of next blocker, or -1 if none.
 */
static int find_next_blocker(SolverState *state, const double *pivotCol,
                             const int *flipped, int num_flipped,
                             double cur_step, int entering_sign,
                             double *out_pivot) {
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

        /* Must be beyond current step (with tolerance) */
        if (ratio < cur_step - CXF_FEASIBILITY_TOL) continue;

        /* Harris Pass 2 logic: among near-minimum, pick largest pivot */
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
     * which bound the leaving variable hits */
    double sp = entering_sign * pivotElement;
    if (sp > 0)
        step = (x_l - state->work_lb[leaving]) / sp;
    else
        step = (x_l - state->work_ub[leaving]) / sp;
    if (step < 0) step = 0;

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
    CxfModel *model = state->model_ref;
    double step_dual = d_entering / pivotElement;

    state->work_dj[entering] = 0.0;
    state->work_dj[leaving] = -step_dual;

    for (int j = 0; j < total; j++) {
        if (j == entering || j == leaving) continue;
        if (basis->var_status[j] >= 0) continue;

        double rho_aj = 0.0;
        if (j < n && model->matrix != NULL) {
            int64_t s = model->matrix->col_ptr[j];
            int64_t e = model->matrix->col_ptr[j + 1];
            for (int64_t k = s; k < e; k++)
                rho_aj += rho[model->matrix->row_idx[k]]
                        * model->matrix->values[k];
        } else if (j >= n) {
            int row = j - n;
            if (row >= 0 && row < m) {
                double coeff = (basis->diag_coeff) ?
                    basis->diag_coeff[row] :
                    get_auxiliary_coeff_fallback(model->matrix, row);
                rho_aj = rho[row] * coeff;
            }
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
    CxfModel *model = state->model_ref;
    if (model == NULL || basis == NULL || model->matrix == NULL)
        return CXF_ERROR_NULL_ARGUMENT;

    int m = state->num_constrs;
    int n = state->num_vars;
    int total = n + m;

    if (m == 0) { state->iteration++; return ITERATE_OPTIMAL; }

    double *pivotCol = basis->work;
    double *column = state->work_column;
    if (!pivotCol || !column) return CXF_ERROR_OUT_OF_MEMORY;

    /*--- Phase 1+2: Multi-level pricing with tolerance escalation ---
     * v2 P2.3 Phase 5 + P3.20 Phase 1-2
     *
     * Level 0 (loose):    optimality_tol * 10 — fast, only strong RCs
     * Level 1 (standard): optimality_tol      — moderate
     * Level 2 (tight):    optimality_tol * 0.1 — catches weak RCs
     *
     * Only declare ITERATE_OPTIMAL when ALL levels return 0 candidates.
     *-------------------------------------------------------------------*/
    int candidates[MAX_CANDIDATES];
    int num_cand = 0;

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
            num_cand = cxf_pricing_candidates(
                state->pricing, state->work_dj, basis->var_status,
                total, pricing_tol, candidates, MAX_CANDIDATES);
            int k2 = 0;
            for (int k = 0; k < num_cand; k++) {
                int j = candidates[k];
                if (state->work_ub[j] >
                    state->work_lb[j] + CXF_FEASIBILITY_TOL)
                    candidates[k2++] = j;
            }
            num_cand = k2;
        } else {
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
        extract_column_ext(model->matrix, basis, entering, n, m, column);
        rc = cxf_ftran(basis, column, pivotCol);
        if (rc != CXF_OK) return rc;

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

    if (!state->use_bland) {  /* BFRT disabled under Bland's rule */
        while (num_flips < MAX_BFRT_FLIPS) {
            int lv = basis->basic_vars[leavingRow];
            double lb_l = state->work_lb[lv];
            double ub_l = state->work_ub[lv];

            /* Can flip? Needs finite bounds on BOTH sides */
            if (lb_l <= -CXF_INFINITY || ub_l >= CXF_INFINITY) break;
            double range = ub_l - lb_l;
            if (range < CXF_FEASIBILITY_TOL) break;

            /* Record flip and extend step */
            flipped_rows[num_flips++] = leavingRow;
            stepSize += range / fabs(pivotCol[leavingRow]);

            /* Find next blocking variable */
            double next_pivot = 0.0;
            int next_row = find_next_blocker(
                state, pivotCol, flipped_rows, num_flips,
                stepSize, entering_sign, &next_pivot);
            if (next_row < 0) {
                /* No more blockers — undo last flip, use it as
                 * the true leaving variable instead */
                num_flips--;
                stepSize -= range / fabs(pivotCol[leavingRow]);
                break;
            }

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
    /* entering_sign accounts for direction: +1 from lower, -1 from upper.
     * Spec: revised_simplex.md Step 4 item 4 */
    state->obj_value += entering_sign * d_entering * stepSize;

    /*--- Phase 7: Incremental reduced cost update ---*/
    if (btran_ok) {
        update_reduced_costs(state, entering, leaving,
                             d_entering, pivotElement, rho);
    } else {
        cxf_compute_reduced_costs(state);
    }

    /*--- Phase 8: Pricing cascade notification (v2 P3.18) ---*/
    if (state->pricing) {
        cxf_pricing_cascade_update(state->pricing, state, entering);
        cxf_pricing_cascade_update(state->pricing, state, leaving);
        /* P0.9: Also notify BFRT-flipped variables */
        for (int f = 0; f < num_flips; f++) {
            int bv = basis->basic_vars[flipped_rows[f]];
            if (bv >= 0 && bv < total)
                cxf_pricing_cascade_update(state->pricing, state, bv);
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
