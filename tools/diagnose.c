/**
 * @file diagnose.c
 * @brief Diagnostic harness for tracing simplex iteration failures.
 *
 * Replicates the solve_lp.c main loop but with per-iteration tracing.
 * Does NOT modify any solver source code.
 *
 * Usage: ./diagnose <path_to_mps_file>
 *
 * Traces per iteration:
 *   - Phase, iteration, objective value
 *   - Entering variable, direction, reduced cost
 *   - Ratio test result (leaving row, pivot element, or UNBOUNDED reason)
 *   - BFRT flip count
 *   - Number of basic vars past bounds (infeasibility count)
 *   - Number of free basic vars (infinite-bound count)
 *   - Pivot column statistics (max, min nonzero, nnz)
 *   - diag_coeff consistency check vs get_auxiliary_coeff
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "convexfeld/convexfeld.h"
#include "convexfeld/cxf_mps.h"

#define ITERATE_CONTINUE   0
#define ITERATE_OPTIMAL    1
#define ITERATE_INFEASIBLE 2
#define ITERATE_UNBOUNDED  3

/* Internal functions we call directly */
extern int cxf_simplex_init(CxfModel *model, SolverState **stateP);
extern void cxf_simplex_final(SolverState *state);
extern int cxf_extract_solution(SolverState *state, CxfModel *model);
extern int cxf_simplex_crash(SolverState *state, CxfEnv *env);
extern int cxf_setup_phase_one(SolverState *state);
extern int cxf_compute_reduced_costs(SolverState *state);
extern void cxf_simplex_setup(SolverState *state, CxfEnv *env,
                              int count, int *indices);
extern int cxf_simplex_preprocess(SolverState *state, CxfEnv *env, int flags);
extern int cxf_simplex_step(SolverState *state, CxfEnv *env);
extern int cxf_simplex_perturbation(SolverState *state, CxfEnv *env);
extern int cxf_simplex_phase_end(SolverState *state, CxfEnv *env, int doScan);
extern void cxf_progress_snapshot(SolverState *state, int *snapshot);
extern int cxf_ftran(BasisState *basis, const double *column, double *result);

/*---------------------------------------------------------------------------*/

/**
 * @brief Audit basic variable feasibility.
 *
 * For each basic variable, check if it's within bounds and count
 * free variables (both bounds infinite).
 */
static void audit_basic_vars(const SolverState *state) {
    int m = state->num_constrs;
    int n = state->num_vars;
    int total = n + m;
    const BasisState *basis = state->basis;
    int past_lb = 0, past_ub = 0, free_count = 0, inf_lb = 0, inf_ub = 0;
    double worst_infeas = 0.0;

    for (int i = 0; i < m; i++) {
        int bv = basis->basic_vars[i];
        if (bv < 0 || bv >= total) continue;
        double x = state->work_x[bv];
        double lb = state->work_lb[bv];
        double ub = state->work_ub[bv];

        if (lb <= -CXF_INFINITY) inf_lb++;
        if (ub >= CXF_INFINITY) inf_ub++;
        if (lb <= -CXF_INFINITY && ub >= CXF_INFINITY) free_count++;

        if (x < lb - CXF_FEASIBILITY_TOL) {
            past_lb++;
            double v = lb - x;
            if (v > worst_infeas) worst_infeas = v;
        }
        if (x > ub + CXF_FEASIBILITY_TOL) {
            past_ub++;
            double v = x - ub;
            if (v > worst_infeas) worst_infeas = v;
        }
    }

    if (past_lb + past_ub > 0 || free_count > 0) {
        fprintf(stderr, "  AUDIT: past_lb=%d past_ub=%d free=%d "
                "inf_lb=%d inf_ub=%d worst_infeas=%.2e\n",
                past_lb, past_ub, free_count, inf_lb, inf_ub, worst_infeas);
    }
}

/**
 * @brief Count nonbasic variables with attractive reduced costs.
 */
static void audit_pricing(const SolverState *state, double tol) {
    int n = state->num_vars;
    int m = state->num_constrs;
    int total = n + m;
    const BasisState *basis = state->basis;
    int at_lower_neg = 0, at_upper_pos = 0, fixed = 0;
    double most_neg = 0, most_pos = 0;

    for (int j = 0; j < total; j++) {
        if (basis->var_status[j] >= 0) continue;
        double rc = state->work_dj[j];
        double range = state->work_ub[j] - state->work_lb[j];
        if (range < CXF_FEASIBILITY_TOL) { fixed++; continue; }

        if (basis->var_status[j] == CXF_VAR_AT_LOWER && rc < -tol) {
            at_lower_neg++;
            if (rc < most_neg) most_neg = rc;
        }
        if (basis->var_status[j] == CXF_VAR_AT_UPPER && rc > tol) {
            at_upper_pos++;
            if (rc > most_pos) most_pos = rc;
        }
    }

    fprintf(stderr, "  PRICING: attractive_lower=%d(best=%.2e) "
            "attractive_upper=%d(best=%.2e) fixed=%d\n",
            at_lower_neg, most_neg, at_upper_pos, most_pos, fixed);
}

/**
 * @brief Check diag_coeff consistency with constraint sense.
 */
static void audit_diag_coeff(const SolverState *state) {
    int m = state->num_constrs;
    if (!state->basis || !state->basis->diag_coeff || !state->work_sense)
        return;

    int mismatches = 0;
    for (int i = 0; i < m; i++) {
        double dc = state->basis->diag_coeff[i];
        char sense = state->work_sense[i];
        double expected;

        if (sense == '>' || sense == 'G') expected = -1.0;
        else if (sense == '<' || sense == 'L') {
            double rhs = state->work_rhs ? state->work_rhs[i] : 0.0;
            expected = (rhs < 0) ? -1.0 : 1.0;
        }
        else if (sense == '=') {
            double rhs = state->work_rhs ? state->work_rhs[i] : 0.0;
            expected = (rhs < 0) ? -1.0 : 1.0;
        }
        else expected = 1.0;

        if (fabs(dc - expected) > 0.01) {
            if (mismatches < 5)
                fprintf(stderr, "  DIAG_MISMATCH: row=%d sense=%c "
                        "diag=%.1f expected=%.1f\n",
                        i, sense, dc, expected);
            mismatches++;
        }
    }
    if (mismatches > 0)
        fprintf(stderr, "  DIAG_COEFF: %d mismatches out of %d rows\n",
                mismatches, m);
}

/**
 * @brief Simulate ratio test to diagnose WHY it returns UNBOUNDED.
 *
 * This replicates the ratio test logic and reports what's happening
 * at each row.
 */
static void diagnose_ratio_test(const SolverState *state, int enteringVar,
                                 const double *pivotCol) {
    int m = state->num_constrs;
    int n = state->num_vars;
    int total = n + m;
    const BasisState *basis = state->basis;

    int s = 1;
    if (basis->var_status[enteringVar] == CXF_VAR_AT_UPPER) s = -1;

    int too_small = 0, inf_bound = 0, neg_ratio = 0, valid = 0;
    int skip_invalid = 0;
    double min_ratio = CXF_INFINITY;

    for (int i = 0; i < m; i++) {
        double d_i = pivotCol[i];
        if (fabs(d_i) <= CXF_PIVOT_TOL) { too_small++; continue; }

        int bv = basis->basic_vars[i];
        if (bv < 0 || bv >= total) { skip_invalid++; continue; }

        double x = state->work_x[bv];
        double lb = state->work_lb[bv];
        double ub = state->work_ub[bv];
        double sd = s * d_i;

        if (sd > CXF_PIVOT_TOL) {
            if (lb <= -CXF_INFINITY) { inf_bound++; continue; }
            double ratio = (x - lb) / sd;
            if (ratio < -CXF_FEASIBILITY_TOL) { neg_ratio++; continue; }
            valid++;
            if (ratio < min_ratio) min_ratio = ratio;
        } else if (sd < -CXF_PIVOT_TOL) {
            if (ub >= CXF_INFINITY) { inf_bound++; continue; }
            double ratio = (x - ub) / sd;
            if (ratio < -CXF_FEASIBILITY_TOL) { neg_ratio++; continue; }
            valid++;
            if (ratio < min_ratio) min_ratio = ratio;
        }
    }

    fprintf(stderr, "  RATIO_DIAG: entering=%d dir=%d "
            "too_small=%d inf_bound=%d neg_ratio=%d valid=%d "
            "min_ratio=%.2e\n",
            enteringVar, s, too_small, inf_bound, neg_ratio, valid,
            valid > 0 ? min_ratio : -1.0);

    /* Detailed dump for first few infinite-bound rows */
    if (inf_bound > 0 && valid == 0) {
        int shown = 0;
        for (int i = 0; i < m && shown < 5; i++) {
            double d_i = pivotCol[i];
            if (fabs(d_i) <= CXF_PIVOT_TOL) continue;
            int bv = basis->basic_vars[i];
            if (bv < 0 || bv >= total) continue;
            double sd = s * d_i;
            double lb = state->work_lb[bv];
            double ub = state->work_ub[bv];
            if ((sd > CXF_PIVOT_TOL && lb <= -CXF_INFINITY) ||
                (sd < -CXF_PIVOT_TOL && ub >= CXF_INFINITY)) {
                fprintf(stderr, "    inf_row=%d bv=%d x=%.4e lb=%.4e ub=%.4e "
                        "d=%.4e sd=%.4e\n",
                        i, bv, state->work_x[bv], lb, ub, d_i, sd);
                shown++;
            }
        }
    }

    /* Detailed dump for negative ratio rows */
    if (neg_ratio > 0 && valid == 0) {
        int shown = 0;
        for (int i = 0; i < m && shown < 5; i++) {
            double d_i = pivotCol[i];
            if (fabs(d_i) <= CXF_PIVOT_TOL) continue;
            int bv = basis->basic_vars[i];
            if (bv < 0 || bv >= total) continue;
            double sd = s * d_i;
            double lb = state->work_lb[bv];
            double ub = state->work_ub[bv];
            double ratio;
            if (sd > CXF_PIVOT_TOL) {
                if (lb <= -CXF_INFINITY) continue;
                ratio = (state->work_x[bv] - lb) / sd;
            } else if (sd < -CXF_PIVOT_TOL) {
                if (ub >= CXF_INFINITY) continue;
                ratio = (state->work_x[bv] - ub) / sd;
            } else continue;
            if (ratio < -CXF_FEASIBILITY_TOL) {
                fprintf(stderr, "    neg_row=%d bv=%d x=%.4e lb=%.4e ub=%.4e "
                        "ratio=%.4e d=%.4e\n",
                        i, bv, state->work_x[bv], lb, ub, ratio, d_i);
                shown++;
            }
        }
    }
}

/**
 * @brief Analyze pivot column statistics.
 */
static void audit_pivot_column(const double *pivotCol, int m) {
    double max_abs = 0, min_abs = CXF_INFINITY;
    int nnz = 0;
    for (int i = 0; i < m; i++) {
        double a = fabs(pivotCol[i]);
        if (a > CXF_PIVOT_TOL) {
            nnz++;
            if (a > max_abs) max_abs = a;
            if (a < min_abs) min_abs = a;
        }
    }
    fprintf(stderr, "  PIVCOL: nnz=%d max=%.2e min=%.2e\n",
            nnz, max_abs, nnz > 0 ? min_abs : 0.0);
}

/**
 * @brief Check what constraint senses exist in this problem.
 */
static void audit_constraint_senses(const SolverState *state) {
    int leq = 0, geq = 0, eq = 0, other = 0;
    for (int i = 0; i < state->num_constrs; i++) {
        char s = state->work_sense[i];
        if (s == '<' || s == 'L') leq++;
        else if (s == '>' || s == 'G') geq++;
        else if (s == '=') eq++;
        else other++;
    }
    fprintf(stderr, "SENSES: <= %d, >= %d, = %d, other %d\n",
            leq, geq, eq, other);
}

/**
 * @brief Check variable bounds: how many are free, bounded, fixed.
 */
static void audit_bounds(const SolverState *state) {
    int n = state->num_vars;
    int m = state->num_constrs;
    int total = n + m;
    int free_vars = 0, fixed = 0, lb_inf = 0, ub_inf = 0, bounded = 0;

    for (int j = 0; j < total; j++) {
        double lb = state->work_lb[j];
        double ub = state->work_ub[j];
        int li = (lb <= -CXF_INFINITY);
        int ui = (ub >= CXF_INFINITY);
        if (li && ui) free_vars++;
        else if (li) lb_inf++;
        else if (ui) ub_inf++;
        else if (fabs(ub - lb) < CXF_FEASIBILITY_TOL) fixed++;
        else bounded++;
    }
    fprintf(stderr, "BOUNDS: free=%d lb_inf=%d ub_inf=%d bounded=%d fixed=%d "
            "(total=%d)\n",
            free_vars, lb_inf, ub_inf, bounded, fixed, total);
}

/*---------------------------------------------------------------------------*/

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <mps_file> [max_iter]\n", argv[0]);
        return 1;
    }

    const char *mps_path = argv[1];
    int max_diag_iters = (argc > 2) ? atoi(argv[2]) : 200;

    /* Load model */
    CxfEnv *env = NULL;
    CxfModel *model = NULL;
    int rc;

    rc = cxf_loadenv(&env, NULL);
    if (rc != CXF_OK) { fprintf(stderr, "loadenv failed: %d\n", rc); return 1; }

    rc = cxf_newmodel(env, &model, "diag", 0, NULL, NULL, NULL, NULL, NULL);
    if (rc != CXF_OK) { fprintf(stderr, "newmodel failed: %d\n", rc); return 1; }

    rc = cxf_readmps(model, mps_path);
    if (rc != CXF_OK) { fprintf(stderr, "readmps failed: %d\n", rc); return 1; }

    fprintf(stderr, "Loaded: %d vars, %d constrs\n",
            model->num_vars, model->num_constrs);

    /* Initialize solver state (same as solve_lp.c) */
    SolverState *state = NULL;
    rc = cxf_simplex_init(model, &state);
    if (rc != CXF_OK) { fprintf(stderr, "init failed: %d\n", rc); return 1; }

    cxf_simplex_crash(state, env);
    rc = cxf_setup_phase_one(state);
    if (rc != CXF_OK) { fprintf(stderr, "phase_one failed: %d\n", rc); return 1; }
    cxf_compute_reduced_costs(state);
    cxf_simplex_setup(state, env, 0, NULL);
    cxf_simplex_preprocess(state, env, 0);

    /* Initial audit */
    fprintf(stderr, "\n=== INITIAL STATE ===\n");
    fprintf(stderr, "Phase: %d, Obj: %.6e, Artificials: %d\n",
            state->phase, state->obj_value, state->num_artificials);
    audit_constraint_senses(state);
    audit_bounds(state);
    audit_diag_coeff(state);
    audit_basic_vars(state);
    audit_pricing(state, env->optimality_tol);

    /* Main diagnostic loop */
    fprintf(stderr, "\n=== ITERATION TRACE ===\n");
    {
        int snap_buf[CXF_SNAPSHOT_SIZE];
        cxf_progress_snapshot(state, snap_buf);
    }

    for (int iter = 0; iter < max_diag_iters; iter++) {
        /* Pre-step state */
        int phase_before = state->phase;
        double obj_before = state->obj_value;

        /* Perturbation on first iters */
        if (iter <= 2) cxf_simplex_perturbation(state, env);

        /* Phase end check */
        int pe = cxf_simplex_phase_end(state, env, 0);
        if (pe == CXF_INFEASIBLE) {
            fprintf(stderr, "ITER %d: phase_end returned INFEASIBLE\n", iter);
            break;
        }

        /* --- Pre-step: find entering variable manually to trace --- */
        int entering = -1;
        double best_rc = 0;
        int n = state->num_vars;
        int m = state->num_constrs;
        int total = n + m;
        BasisState *basis = state->basis;

        for (int j = 0; j < total; j++) {
            if (basis->var_status[j] >= 0) continue;
            double range = state->work_ub[j] - state->work_lb[j];
            if (range < CXF_FEASIBILITY_TOL) continue;
            double rc2 = state->work_dj[j];
            if (basis->var_status[j] == CXF_VAR_AT_LOWER && rc2 < best_rc) {
                best_rc = rc2; entering = j;
            } else if (basis->var_status[j] == CXF_VAR_AT_UPPER && -rc2 < best_rc) {
                best_rc = -rc2; entering = j;
            }
        }

        /* If we found a candidate, do FTRAN and diagnose ratio test */
        if (entering >= 0) {
            double *column = (double *)calloc((size_t)m, sizeof(double));
            double *pivotCol = (double *)calloc((size_t)m, sizeof(double));

            /* Extract column */
            if (entering < n) {
                if (state->csc_col_ptr) {
                    int64_t cs = state->csc_col_ptr[entering];
                    int64_t ce = state->csc_col_ptr[entering + 1];
                    for (int64_t k = cs; k < ce; k++)
                        column[state->csc_row_idx[k]] = state->csc_values[k];
                }
            } else {
                int row = entering - n;
                if (row >= 0 && row < m && basis->diag_coeff)
                    column[row] = basis->diag_coeff[row];
            }

            rc = cxf_ftran(basis, column, pivotCol);
            if (rc == CXF_OK) {
                audit_pivot_column(pivotCol, m);
                diagnose_ratio_test(state, entering, pivotCol);
            } else {
                fprintf(stderr, "  FTRAN failed: %d\n", rc);
            }
            free(column);
            free(pivotCol);
        }

        /* Now actually do the step */
        int status = cxf_simplex_step(state, env);

        fprintf(stderr, "ITER %d: phase=%d obj=%.6e->%.6e status=%d "
                "entering=%d(rc=%.2e) bland=%d degen=%d flips=%d\n",
                iter, phase_before, obj_before, state->obj_value,
                status, entering, best_rc,
                state->use_bland, state->degenerate_count, state->flip_count);

        if (status == ITERATE_OPTIMAL) {
            if (state->phase == 1) {
                fprintf(stderr, "Phase I optimal — checking transition\n");
                extern int cxf_check_phase_one_end(SolverState *, CxfModel *,
                                                    CxfEnv *);
                rc = cxf_check_phase_one_end(state, model, env);
                fprintf(stderr, "  Transition result: %d, phase now: %d\n",
                        rc, state->phase);
                if (rc == CXF_OK || rc == 1) {
                    /* Transition succeeded or continue Phase I */
                    audit_diag_coeff(state);
                    audit_basic_vars(state);
                    continue;
                }
                fprintf(stderr, "RESULT: INFEASIBLE (Phase I failed)\n");
                break;
            }
            fprintf(stderr, "RESULT: OPTIMAL, obj=%.10e\n", state->obj_value);
            break;
        }
        if (status == ITERATE_UNBOUNDED) {
            fprintf(stderr, "\n=== UNBOUNDED DIAGNOSIS ===\n");
            audit_basic_vars(state);
            audit_diag_coeff(state);
            fprintf(stderr, "RESULT: UNBOUNDED at iter %d\n", iter);
            break;
        }
        if (status == ITERATE_INFEASIBLE) {
            fprintf(stderr, "RESULT: INFEASIBLE at iter %d\n", iter);
            break;
        }
        if (status < 0) {
            fprintf(stderr, "RESULT: ERROR %d at iter %d\n", status, iter);
            break;
        }

        /* Post-step audit every 50 iters */
        if (iter % 50 == 49) {
            fprintf(stderr, "\n--- Periodic audit at iter %d ---\n", iter);
            audit_basic_vars(state);
            audit_diag_coeff(state);
        }
    }

    cxf_simplex_final(state);
    cxf_freemodel(model);
    cxf_freeenv(env);
    return 0;
}
