/**
 * @file phase_loop.c
 * @brief Phase I and Phase II iteration loops.
 *
 * Extracted from solve_lp.c. Contains the main iteration loop bodies
 * for Phase I (feasibility) and Phase II (optimality), plus helpers
 * for infeasibility computation and basic variable correction.
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_matrix.h"
#include "convexfeld/cxf_types.h"
#include <math.h>
#include <stdio.h>

#define ITERATE_CONTINUE   0
#define ITERATE_OPTIMAL    1
#define ITERATE_INFEASIBLE 2
#define ITERATE_UNBOUNDED  3

extern int cxf_simplex_step(SolverState *state, CxfEnv *env);
extern void cxf_compute_reduced_costs(SolverState *state);

/** @brief Compute total constraint violation (true infeasibility). */
static double compute_true_infeasibility(SolverState *state, CxfModel *model) {
    MatrixData *mat = model->matrix;
    double total = 0.0;
    for (int i = 0; i < state->num_constrs; i++) {
        double ax = 0.0;
        for (int j = 0; j < state->num_vars; j++) {
            int64_t start = mat->col_ptr[j];
            int64_t end = mat->col_ptr[j + 1];
            for (int64_t k = start; k < end; k++) {
                if (mat->row_idx[k] == i) {
                    ax += mat->values[k] * state->work_x[j];
                    break;
                }
            }
        }
        double rhs = mat->rhs ? mat->rhs[i] : 0.0;
        char sense = mat->sense ? mat->sense[i] : '<';
        double viol = 0.0;
        if (sense == '<' || sense == 'L') {
            if (ax > rhs + CXF_FEASIBILITY_TOL) viol = ax - rhs;
        } else if (sense == '>' || sense == 'G') {
            if (ax < rhs - CXF_FEASIBILITY_TOL) viol = rhs - ax;
        } else {
            viol = fabs(ax - rhs);
            if (viol < CXF_FEASIBILITY_TOL) viol = 0.0;
        }
        total += viol;
    }
    return total;
}

/**
 * @brief Correct basic variable values to reduce constraint violations.
 *
 * For each row, recomputes the basic variable value to satisfy the
 * constraint exactly. Multiple iterations handle coupling between rows.
 *
 * @return Final total infeasibility after corrections.
 */
static double correct_basic_variables(SolverState *state, CxfModel *model) {
    MatrixData *mat = model->matrix;
    double infeas = compute_true_infeasibility(state, model);
    int iters = 0;

    while (infeas > CXF_FEASIBILITY_TOL && iters < 10) {
        iters++;
        for (int i = 0; i < state->num_constrs; i++) {
            int bv = state->basis->basic_vars[i];
            double rhs = mat->rhs ? mat->rhs[i] : 0.0;

            if (bv < state->num_vars) {
                /* Original variable is basic — find its coefficient */
                double A_bv = 0.0;
                int64_t s = mat->col_ptr[bv], e = mat->col_ptr[bv + 1];
                for (int64_t k = s; k < e; k++) {
                    if (mat->row_idx[k] == i) { A_bv = mat->values[k]; break; }
                }
                if (fabs(A_bv) > CXF_ZERO_TOL) {
                    /* Compute Ax excluding basic variable */
                    double ax_excl = 0.0;
                    for (int j = 0; j < state->num_vars; j++) {
                        if (j == bv) continue;
                        int64_t js = mat->col_ptr[j], je = mat->col_ptr[j + 1];
                        for (int64_t k = js; k < je; k++) {
                            if (mat->row_idx[k] == i) {
                                ax_excl += mat->values[k] * state->work_x[j];
                                break;
                            }
                        }
                    }
                    /* Include nonbasic auxiliary contribution */
                    int aux = state->num_vars + i;
                    if (state->basis->var_status[aux] < 0) {
                        double d = (state->basis->diag_coeff != NULL) ?
                                   state->basis->diag_coeff[i] : 1.0;
                        ax_excl += d * state->work_x[aux];
                    }
                    state->work_x[bv] = (rhs - ax_excl) / A_bv;
                }
            } else {
                /* Auxiliary is basic */
                double ax = 0.0;
                for (int j = 0; j < state->num_vars; j++) {
                    int64_t js = mat->col_ptr[j], je = mat->col_ptr[j + 1];
                    for (int64_t k = js; k < je; k++) {
                        if (mat->row_idx[k] == i) {
                            ax += mat->values[k] * state->work_x[j];
                            break;
                        }
                    }
                }
                double d = (state->basis->diag_coeff != NULL) ?
                           state->basis->diag_coeff[i] : 1.0;
                double nv = (rhs - ax) / d;
                state->work_x[bv] = (nv > 0) ? nv : 0.0;
            }
        }
        infeas = compute_true_infeasibility(state, model);
#ifdef DEBUG_PHASE1
        fprintf(stderr, "  Correction iter %d: infeas=%.10f\n", iters, infeas);
#endif
    }
    return infeas;
}

/** @brief Check if any nonbasic variable has improving reduced cost. */
static int has_improving_direction(SolverState *state, CxfEnv *env) {
    int total = state->num_vars + state->num_constrs;
    for (int j = 0; j < total; j++) {
        if (state->basis->var_status[j] >= 0) continue;
        double lb = state->work_lb[j], ub = state->work_ub[j];
        if (ub <= lb + CXF_FEASIBILITY_TOL) continue;
        if (state->basis->var_status[j] == -1 &&
            state->work_dj[j] < -env->optimality_tol)
            return 1;
        if (state->basis->var_status[j] == -2 &&
            state->work_dj[j] > env->optimality_tol)
            return 1;
    }
    return 0;
}

/**
 * @brief Check Phase I termination and handle transition to Phase II.
 *
 * Called from the unified loop when iteration returns OPTIMAL during Phase I.
 * Corrects basic variables, checks true infeasibility, and transitions
 * to Phase II if feasible.
 *
 * @return CXF_OK if transitioned to Phase II,
 *         CXF_INFEASIBLE if truly infeasible,
 *         1 if has improving direction (continue Phase I)
 */
int cxf_check_phase_one_end(SolverState *state, CxfModel *model, CxfEnv *env) {
    double infeas = correct_basic_variables(state, model);
    state->obj_value = infeas;
    double tol = fmax(env->feasibility_tol, 0.01);

    if (infeas <= tol) {
        /* Feasible: transition to Phase II */
        extern int cxf_transition_to_phase_two(SolverState *, CxfModel *);
        int rc = cxf_transition_to_phase_two(state, model);
        if (rc != CXF_OK) return rc;
        cxf_compute_reduced_costs(state);
        return CXF_OK;
    }

    /* Not yet feasible — check for improving direction */
    cxf_compute_reduced_costs(state);
    if (has_improving_direction(state, env)) return 1;

    return CXF_INFEASIBLE;
}

int cxf_run_phase_one(SolverState *state, CxfModel *model, CxfEnv *env) {
    int max_iter = state->max_iterations;
    int start_iter = state->iteration;

    while (state->iteration < max_iter) {
        int phase_iters = state->iteration - start_iter;
        if (!state->use_bland && phase_iters > 3 * state->num_constrs)
            state->use_bland = 1;

        int status = cxf_simplex_step(state, env);

        if (status == ITERATE_OPTIMAL) {
            double infeas = correct_basic_variables(state, model);
            state->obj_value = infeas;
            double tol = fmax(env->feasibility_tol, 0.01);
#ifdef DEBUG_PHASE1
            fprintf(stderr, "[Phase I] iter=%d, infeas=%.6e, tol=%.6e\n",
                    state->iteration, infeas, tol);
#endif
            if (infeas <= tol) break;  /* Feasible — proceed to Phase II */

            /* Recompute reduced costs and check for improving directions */
            cxf_compute_reduced_costs(state);
            if (has_improving_direction(state, env)) continue;

#ifdef DEBUG_PHASE1
            fprintf(stderr, "[Phase I INFEASIBLE] infeas=%.6f > tol=%.6e\n",
                    infeas, tol);
#endif
            return CXF_INFEASIBLE;
        } else if (status == ITERATE_UNBOUNDED) {
            return CXF_ERROR_NOT_SUPPORTED;
        } else if (status < 0) {
            return status;
        }
    }

    if (state->iteration >= max_iter) return CXF_ITERATION_LIMIT;
    return CXF_OK;
}

int cxf_run_phase_two(SolverState *state, CxfModel *model, CxfEnv *env) {
    int max_iter = state->max_iterations;
    int start_iter = state->iteration;

    while (state->iteration < max_iter) {
        int phase_iters = state->iteration - start_iter;
        if (!state->use_bland && phase_iters > 3 * state->num_constrs)
            state->use_bland = 1;

        int status = cxf_simplex_step(state, env);

        if (status == ITERATE_OPTIMAL) { model->status = CXF_OPTIMAL; break; }
        else if (status == ITERATE_UNBOUNDED) { model->status = CXF_UNBOUNDED; break; }
        else if (status == ITERATE_INFEASIBLE) { model->status = CXF_INFEASIBLE; break; }
        else if (status < 0) { model->status = status; break; }
    }

    if (state->iteration >= max_iter) model->status = CXF_ITERATION_LIMIT;
    return model->status;
}
