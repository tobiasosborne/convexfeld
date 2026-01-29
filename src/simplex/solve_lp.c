/**
 * @file solve_lp.c
 * @brief Main LP solver entry point (M7.1.4)
 *
 * Orchestrates the simplex solve sequence: initialization, basis setup,
 * iteration loop, and solution extraction. Implements two-phase method.
 *
 * Spec: docs/specs/functions/simplex/cxf_solve_lp.md
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_matrix.h"
#include "convexfeld/cxf_types.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* Iteration result codes from iterate.c */
#define ITERATE_CONTINUE   0
#define ITERATE_OPTIMAL    1
#define ITERATE_INFEASIBLE 2
#define ITERATE_UNBOUNDED  3

/* External declarations */
extern int cxf_simplex_init(CxfModel *model, SolverContext **stateP);
extern void cxf_simplex_final(SolverContext *state);
extern int cxf_simplex_iterate(SolverContext *state, CxfEnv *env);
extern int cxf_extract_solution(SolverContext *state, CxfModel *model);
extern int cxf_simplex_perturbation(SolverContext *state, CxfEnv *env);
extern int cxf_simplex_unperturb(SolverContext *state, CxfEnv *env);
extern int cxf_simplex_refine(SolverContext *state, CxfEnv *env);

/**
 * @brief Set up Phase I with slack/artificial variables.
 *
 * Creates initial basis using slack and artificial variables:
 * - Original vars (0 to n-1): set at lower bounds, nonbasic
 * - For <= constraints: slack variable (can be positive at optimality)
 * - For >= constraints: surplus + artificial if needed
 * - For = constraints: artificial variable (must be zero for feasibility)
 * - Phase I objective: minimize sum of ARTIFICIAL variables only
 *
 * Key insight: Slacks for <= constraints have obj coeff = 0 because
 * they CAN be positive at optimality. Only true artificials (for =
 * and problematic >= constraints) need obj coeff = 1.
 *
 * @param state Solver context
 * @return CXF_OK on success
 */
static int setup_phase_one(SolverContext *state) {
    BasisState *basis = state->basis;
    CxfModel *model = state->model_ref;
    SparseMatrix *mat = model->matrix;
    int m = state->num_constrs;
    int n = state->num_vars;

    /* Initialize: all original variables at lower bound (nonbasic) */
    for (int j = 0; j < n; j++) {
        double lb = state->work_lb[j];
        if (lb <= -CXF_INFINITY) lb = 0.0;  /* Free vars start at 0 */
        basis->var_status[j] = -1;  /* At lower bound */
        state->work_x[j] = lb;
    }

    /* Set up slack/artificial variables as initial basis.
     * Variable at index (n + i) corresponds to constraint i.
     *
     * For <= constraints: this is a SLACK (obj coeff = 0)
     * For >= constraints: this is a SURPLUS, may need artificial
     * For = constraints: this is an ARTIFICIAL (obj coeff = 1)
     */
    state->num_artificials = 0;  /* Count true artificials */

    for (int i = 0; i < m; i++) {
        int var_idx = n + i;  /* Slack/artificial var for row i */

        /* Variable is basic in row i */
        basis->basic_vars[i] = var_idx;
        basis->var_status[var_idx] = i;  /* Basic in row i */

        /* Compute slack value = RHS - sum(a_ij * x_j) for original vars */
        double rhs = mat->rhs ? mat->rhs[i] : 0.0;
        double row_sum = 0.0;

        /* Sum contributions from original variables at their bounds */
        for (int j = 0; j < n; j++) {
            double aij = 0.0;
            int64_t start = mat->col_ptr[j];
            int64_t end = mat->col_ptr[j + 1];
            for (int64_t k = start; k < end; k++) {
                if (mat->row_idx[k] == i) {
                    aij = mat->values[k];
                    break;
                }
            }
            row_sum += aij * state->work_x[j];
        }

        double slack_val = rhs - row_sum;
        char sense = mat->sense ? mat->sense[i] : '<';

        /* Set bounds (always non-negative) */
        state->work_lb[var_idx] = 0.0;
        state->work_ub[var_idx] = CXF_INFINITY;

        /* Determine auxiliary coefficient and value based on constraint sense
         * and initial feasibility. The coefficient must make aux >= 0.
         *
         * For Ax + coeff*aux = rhs:
         *   aux = (rhs - Ax) / coeff = slack_val / coeff
         * We need aux >= 0, so choose coeff to have same sign as slack_val.
         *
         * For <= or =: slack_val = rhs - row_sum
         *   - If slack_val >= 0: coeff = +1, aux = slack_val (feasible)
         *   - If slack_val < 0:  coeff = -1, aux = -slack_val (artificial)
         *
         * For >=: surplus_val = row_sum - rhs = -slack_val
         *   - If surplus_val >= 0: coeff = -1, aux = surplus_val (feasible)
         *   - If surplus_val < 0:  coeff = +1, aux = -surplus_val (artificial)
         */
        double diag = 1.0;  /* Default: coeff = +1 */

        if (sense == '<' || sense == 'L') {
            if (slack_val >= 0) {
                /* Feasible: slack variable with coeff = +1 */
                diag = 1.0;
                state->work_x[var_idx] = slack_val;
                state->work_obj[var_idx] = 0.0;  /* Slack, not artificial */
            } else {
                /* Infeasible: artificial with coeff = -1 to make aux positive */
                diag = -1.0;
                state->work_x[var_idx] = -slack_val;
                state->work_obj[var_idx] = 1.0;
                state->num_artificials++;
            }
        } else if (sense == '>' || sense == 'G') {
            double surplus_val = row_sum - rhs;  /* = -slack_val */
            if (surplus_val >= 0) {
                /* Feasible: surplus variable with coeff = -1 */
                diag = -1.0;
                state->work_x[var_idx] = surplus_val;
                state->work_obj[var_idx] = 0.0;  /* Surplus, not artificial */
            } else {
                /* Infeasible: artificial with coeff = +1 to make aux positive */
                diag = 1.0;
                state->work_x[var_idx] = -surplus_val;
                state->work_obj[var_idx] = 1.0;
                state->num_artificials++;
            }
        } else {
            /* = constraint: always needs artificial if not satisfied */
            if (slack_val >= 0) {
                diag = 1.0;
                state->work_x[var_idx] = slack_val;
            } else {
                diag = -1.0;
                state->work_x[var_idx] = -slack_val;
            }
            state->work_obj[var_idx] = 1.0;  /* Always artificial for = */
            if (fabs(slack_val) > CXF_FEASIBILITY_TOL) {
                state->num_artificials++;
            }
        }

        if (basis->diag_coeff != NULL) {
            basis->diag_coeff[i] = diag;
        }
    }

    /* Set original variables' Phase I objective to 0 */
    for (int j = 0; j < n; j++) {
        state->work_obj[j] = 0.0;
    }

    /* Compute initial Phase I objective = sum of artificial values only */
    state->obj_value = 0.0;
    for (int i = 0; i < m; i++) {
        int var_idx = n + i;
        if (state->work_obj[var_idx] > 0.5) {  /* Is artificial (obj coeff = 1) */
            state->obj_value += state->work_x[var_idx];
        }
    }

    state->phase = 1;
    return CXF_OK;
}

/**
 * @brief Transition from Phase I to Phase II.
 *
 * After Phase I finds a feasible basis (sum of artificials = 0):
 * - Restore original objective coefficients
 * - Set artificial objective coefficients to 0 (they should stay at zero)
 * - Recompute objective value with original coefficients
 *
 * @param state Solver context
 * @param model Original model with true objective
 * @return CXF_OK on success
 */
static int transition_to_phase_two(SolverContext *state, CxfModel *model) {
    int n = state->num_vars;
    int m = state->num_constrs;
    SparseMatrix *mat = model->matrix;

    /* Restore original objective coefficients */
    for (int j = 0; j < n; j++) {
        state->work_obj[j] = model->obj_coeffs[j];
    }

    /* Set auxiliary objective coefficients to 0 for slacks/surpluses.
     * For equality constraints, the auxiliary is an ARTIFICIAL variable
     * that MUST stay at zero for feasibility. Fix these at 0 by setting
     * both bounds to 0, preventing them from re-entering the basis.
     */
    for (int i = 0; i < m; i++) {
        int var_idx = n + i;
        state->work_obj[var_idx] = 0.0;

        /* Check constraint sense */
        char sense = (mat != NULL && mat->sense != NULL) ? mat->sense[i] : '<';
        if (sense == '=' || sense == 'E') {
            /* Equality constraint: auxiliary is an artificial variable.
             * Fix at zero by setting ub = 0 (lb is already 0).
             * This prevents it from re-entering the basis in Phase II.
             */
            state->work_ub[var_idx] = 0.0;
        }
        /* For <= and >= constraints, slacks/surpluses can be positive */
    }

    /* Recompute objective value with original objective */
    state->obj_value = 0.0;
    for (int j = 0; j < n; j++) {
        state->obj_value += state->work_obj[j] * state->work_x[j];
    }

    state->phase = 2;
    return CXF_OK;
}

/* External declaration for cxf_btran_vec */
extern int cxf_btran_vec(BasisState *basis, const double *input, double *result);

/**
 * @brief Get the coefficient for slack/surplus/artificial variable.
 *
 * Standard form conversion uses Ax + coeff*s = b where s >= 0.
 * The coefficient must ensure s >= 0 at the initial point (x at bounds).
 *
 * For <= constraints (Ax <= b):
 *   - Normal: Ax + s = b, s = b - Ax. At x=lb, s = b - Ax(lb).
 *   - If RHS >= 0 and Ax(lb) small: s >= 0, use coeff = +1
 *   - If RHS < 0 (violated at origin): s = rhs < 0, need coeff = -1
 *     so Ax - s = b gives s = Ax - b = -rhs > 0 ✓
 *
 * For >= constraints (Ax >= b):
 *   - Standard: Ax - s = b (surplus), s = Ax - b
 *   - Always use coeff = -1
 *
 * For = constraints (Ax = b):
 *   - Need artificial a = |b - Ax(lb)|
 *   - If RHS >= 0: Ax + a = b, a = b at x=0 ✓
 *   - If RHS < 0: Ax - a = b, -a = b, a = -b > 0 ✓
 */
static double get_auxiliary_coeff(const SparseMatrix *mat, int row) {
    if (mat == NULL || mat->sense == NULL) return 1.0;
    char sense = mat->sense[row];
    double rhs = (mat->rhs != NULL) ? mat->rhs[row] : 0.0;

    if (sense == '>' || sense == 'G') {
        return -1.0;
    }
    if (sense == '<' || sense == 'L') {
        /* For <= with negative RHS, the constraint is violated at x=0
         * and we need coeff = -1 to make the artificial positive */
        return (rhs < 0) ? -1.0 : 1.0;
    }
    if (sense == '=') {
        return (rhs < 0) ? -1.0 : 1.0;
    }
    return 1.0;
}

/**
 * @brief Compute reduced costs: dj = cj - pi^T * Aj
 *
 * For correct simplex pricing, reduced costs must account for the
 * dual prices (shadow prices) from the current basis.
 *
 * Dual prices are computed as: π = B^(-T) * c_B
 * where c_B is the objective coefficients of basic variables.
 *
 * @param state Solver context
 */
static void compute_reduced_costs(SolverContext *state) {
    CxfModel *model = state->model_ref;
    SparseMatrix *mat = model->matrix;
    BasisState *basis = state->basis;
    int n = state->num_vars;
    int m = state->num_constrs;
    int total_vars = n + m;

    /* Step 1: Compute dual prices π = B^(-T) * c_B
     * c_B[i] = objective coefficient of basic variable in row i
     * Then solve B^T * π = c_B using BTRAN
     */

    /* Build c_B vector (objective coeffs of basic variables) */
    double *cB = (double *)calloc((size_t)m, sizeof(double));
    if (cB == NULL) {
        /* Fallback to simple approximation if allocation fails */
        for (int i = 0; i < m; i++) {
            int basic_var = basis->basic_vars[i];
            if (basic_var >= 0 && basic_var < total_vars) {
                state->work_pi[i] = state->work_obj[basic_var];
            } else {
                state->work_pi[i] = 0.0;
            }
        }
    } else {
        for (int i = 0; i < m; i++) {
            int basic_var = basis->basic_vars[i];
            if (basic_var >= 0 && basic_var < total_vars) {
                cB[i] = state->work_obj[basic_var];
            } else {
                cB[i] = 0.0;
            }
        }

        /* Compute π = B^(-T) * c_B using BTRAN */
        int rc = cxf_btran_vec(basis, cB, state->work_pi);
        if (rc != CXF_OK) {
            /* Fallback to simple approximation if BTRAN fails */
            for (int i = 0; i < m; i++) {
                state->work_pi[i] = cB[i];
            }
        }
        free(cB);
    }

    /* Step 2: Compute reduced costs for all variables */
    for (int j = 0; j < total_vars; j++) {
        if (basis->var_status[j] >= 0) {
            /* Basic variable: reduced cost = 0 */
            state->work_dj[j] = 0.0;
        } else {
            /* Nonbasic variable: dj = cj - pi^T * Aj */
            double dj = state->work_obj[j];

            if (j < n && mat != NULL) {
                /* Original variable: subtract pi^T * column_j */
                int64_t start = mat->col_ptr[j];
                int64_t end = mat->col_ptr[j + 1];
                for (int64_t k = start; k < end; k++) {
                    int row = mat->row_idx[k];
                    dj -= state->work_pi[row] * mat->values[k];
                }
            } else if (j >= n) {
                /* Auxiliary variable j corresponds to row (j - n) */
                /* Use diag_coeff from basis if available */
                int row = j - n;
                if (row >= 0 && row < m) {
                    double coeff = (basis->diag_coeff != NULL) ?
                        basis->diag_coeff[row] :
                        get_auxiliary_coeff(mat, row);
                    dj -= state->work_pi[row] * coeff;
                }
            }

            state->work_dj[j] = dj;
        }
    }
}

/**
 * @brief Solve unconstrained LP (no constraints).
 */
static int solve_unconstrained(CxfModel *model) {
    for (int j = 0; j < model->num_vars; j++) {
        if (model->lb[j] > model->ub[j] + CXF_FEASIBILITY_TOL) {
            model->status = CXF_INFEASIBLE;
            return CXF_INFEASIBLE;
        }
    }

    double obj_val = 0.0;
    for (int j = 0; j < model->num_vars; j++) {
        double c = model->obj_coeffs[j];
        double lb = model->lb[j], ub = model->ub[j];

        if (c < 0) {
            if (ub >= CXF_INFINITY) { model->status = CXF_UNBOUNDED; return CXF_UNBOUNDED; }
            if (model->solution) model->solution[j] = ub;
            obj_val += c * ub;
        } else if (c > 0) {
            if (lb <= -CXF_INFINITY) { model->status = CXF_UNBOUNDED; return CXF_UNBOUNDED; }
            if (model->solution) model->solution[j] = lb;
            obj_val += c * lb;
        } else {
            double val = (lb > 0.0) ? lb : ((ub < 0.0) ? ub : 0.0);
            if (model->solution) model->solution[j] = val;
        }
    }
    model->obj_val = obj_val;
    model->status = CXF_OPTIMAL;
    return CXF_OPTIMAL;
}

/**
 * @brief Get row coefficients as dense array.
 */
static void get_row_coeffs(SparseMatrix *mat, int row, int n, double *coeffs) {
    memset(coeffs, 0, (size_t)n * sizeof(double));
    for (int j = 0; j < n; j++) {
        int64_t start = mat->col_ptr[j];
        int64_t end = mat->col_ptr[j + 1];
        for (int64_t k = start; k < end; k++) {
            if (mat->row_idx[k] == row) { coeffs[j] = mat->values[k]; break; }
        }
    }
}

/**
 * @brief Check if two rows are parallel (same direction).
 */
static int rows_parallel(double *r1, double *r2, int n, double *scale) {
    double s = 0.0;
    int found = 0;
    for (int j = 0; j < n; j++) {
        if (fabs(r1[j]) < CXF_ZERO_TOL && fabs(r2[j]) < CXF_ZERO_TOL) continue;
        if (fabs(r1[j]) < CXF_ZERO_TOL || fabs(r2[j]) < CXF_ZERO_TOL) return 0;
        double ratio = r1[j] / r2[j];
        if (!found) { s = ratio; found = 1; }
        else if (fabs(ratio - s) > CXF_FEASIBILITY_TOL) return 0;
    }
    *scale = s;
    return found;
}

/**
 * @brief Check if problem is obviously infeasible via simple analysis.
 *
 * Two checks:
 * 1. Single constraint infeasibility (bound propagation)
 * 2. Parallel constraint contradiction (e.g., x+y<=1 and x+y>=3)
 */
static int check_obvious_infeasibility(CxfModel *model) {
    SparseMatrix *mat = model->matrix;
    if (mat == NULL) return 0;

    int m = mat->num_rows;
    int n = mat->num_cols;

    double *row1 = (double *)malloc((size_t)n * sizeof(double));
    double *row2 = (double *)malloc((size_t)n * sizeof(double));
    if (row1 == NULL || row2 == NULL) { free(row1); free(row2); return 0; }

    /* Check 1: Single constraint infeasibility via bound propagation */
    for (int i = 0; i < m; i++) {
        double row_min = 0.0, row_max = 0.0;
        get_row_coeffs(mat, i, n, row1);
        for (int j = 0; j < n; j++) {
            double aij = row1[j];
            if (aij == 0.0) continue;
            double lb = model->lb[j], ub = model->ub[j];
            if (aij > 0) {
                row_min += aij * lb;
                row_max += (ub >= CXF_INFINITY) ? CXF_INFINITY : aij * ub;
            } else {
                row_min += (ub >= CXF_INFINITY) ? -CXF_INFINITY : aij * ub;
                row_max += aij * lb;
            }
        }
        double rhs = mat->rhs ? mat->rhs[i] : 0.0;
        char sense = mat->sense ? mat->sense[i] : '<';
        if ((sense == '<' || sense == 'L') && row_min > rhs + CXF_FEASIBILITY_TOL) {
            free(row1); free(row2); return 1;
        }
        if ((sense == '>' || sense == 'G') && row_max < rhs - CXF_FEASIBILITY_TOL) {
            free(row1); free(row2); return 1;
        }
        if (sense == '=') {
            if (row_min > rhs + CXF_FEASIBILITY_TOL || row_max < rhs - CXF_FEASIBILITY_TOL) {
                free(row1); free(row2); return 1;
            }
        }
    }

    /* Check 2: Parallel constraint contradiction */
    for (int i = 0; i < m; i++) {
        get_row_coeffs(mat, i, n, row1);
        double rhs1 = mat->rhs ? mat->rhs[i] : 0.0;
        char sense1 = mat->sense ? mat->sense[i] : '<';

        for (int j = i + 1; j < m; j++) {
            get_row_coeffs(mat, j, n, row2);
            double scale = 0.0;
            if (!rows_parallel(row1, row2, n, &scale)) continue;

            double rhs2 = mat->rhs ? mat->rhs[j] : 0.0;
            char sense2 = mat->sense ? mat->sense[j] : '<';

            /* Scale row2 to match row1: row2 * scale = row1 */
            double scaled_rhs2 = rhs2 * scale;
            char scaled_sense2 = sense2;
            if (scale < 0) {
                if (sense2 == '<') scaled_sense2 = '>';
                else if (sense2 == '>') scaled_sense2 = '<';
            }

            /* Now check: row1 sense1 rhs1 and row1 scaled_sense2 scaled_rhs2 */
            double lower = -CXF_INFINITY, upper = CXF_INFINITY;
            if (sense1 == '<' || sense1 == 'L') upper = fmin(upper, rhs1);
            else if (sense1 == '>' || sense1 == 'G') lower = fmax(lower, rhs1);
            else { lower = rhs1; upper = rhs1; }

            if (scaled_sense2 == '<' || scaled_sense2 == 'L') upper = fmin(upper, scaled_rhs2);
            else if (scaled_sense2 == '>' || scaled_sense2 == 'G') lower = fmax(lower, scaled_rhs2);
            else { lower = fmax(lower, scaled_rhs2); upper = fmin(upper, scaled_rhs2); }

            if (lower > upper + CXF_FEASIBILITY_TOL) {
                free(row1); free(row2); return 1;
            }
        }
    }

    free(row1); free(row2);
    return 0;
}

/**
 * @brief Check for obvious unboundedness via ray analysis.
 *
 * For each unbounded variable with improving objective direction,
 * check if constraints allow infinite increase.
 */
static int check_obvious_unboundedness(CxfModel *model) {
    SparseMatrix *mat = model->matrix;
    if (mat == NULL) return 0;

    int m = mat->num_rows;
    int n = mat->num_cols;

    /* For each variable with infinite bound in improving direction */
    for (int j = 0; j < n; j++) {
        double c = model->obj_coeffs[j];
        double lb = model->lb[j], ub = model->ub[j];

        /* Check if variable wants to go to +infinity (c < 0) */
        if (c < -CXF_FEASIBILITY_TOL && ub >= CXF_INFINITY) {
            /* Can we increase j without violating any constraint? */
            int can_increase = 1;
            for (int i = 0; i < m && can_increase; i++) {
                /* Get coefficient of variable j in constraint i */
                double aij = 0.0;
                int64_t start = mat->col_ptr[j];
                int64_t end = mat->col_ptr[j + 1];
                for (int64_t k = start; k < end; k++) {
                    if (mat->row_idx[k] == i) { aij = mat->values[k]; break; }
                }
                if (fabs(aij) < CXF_ZERO_TOL) continue;

                char sense = mat->sense ? mat->sense[i] : '<';
                /* For <= constraint with positive coefficient, increasing j violates */
                if ((sense == '<' || sense == 'L') && aij > CXF_ZERO_TOL) {
                    can_increase = 0;
                }
                /* For >= constraint with negative coefficient, increasing j violates */
                if ((sense == '>' || sense == 'G') && aij < -CXF_ZERO_TOL) {
                    can_increase = 0;
                }
                /* For = constraint with any nonzero coefficient, can't go to infinity */
                if (sense == '=') {
                    can_increase = 0;
                }
            }
            if (can_increase) return 1;  /* Unbounded: can increase j to +inf */
        }

        /* Check if variable wants to go to -infinity (c > 0) */
        if (c > CXF_FEASIBILITY_TOL && lb <= -CXF_INFINITY) {
            int can_decrease = 1;
            for (int i = 0; i < m && can_decrease; i++) {
                double aij = 0.0;
                int64_t start = mat->col_ptr[j];
                int64_t end = mat->col_ptr[j + 1];
                for (int64_t k = start; k < end; k++) {
                    if (mat->row_idx[k] == i) { aij = mat->values[k]; break; }
                }
                if (fabs(aij) < CXF_ZERO_TOL) continue;

                char sense = mat->sense ? mat->sense[i] : '<';
                if ((sense == '<' || sense == 'L') && aij < -CXF_ZERO_TOL) {
                    can_decrease = 0;
                }
                if ((sense == '>' || sense == 'G') && aij > CXF_ZERO_TOL) {
                    can_decrease = 0;
                }
                if (sense == '=') {
                    can_decrease = 0;
                }
            }
            if (can_decrease) return 1;  /* Unbounded: can decrease j to -inf */
        }
    }
    return 0;
}

/**
 * @brief Solve an LP using the simplex method.
 */
int cxf_solve_lp(CxfModel *model) {
    SolverContext *state = NULL;
    int rc, status;

    if (model == NULL) return CXF_ERROR_NULL_ARGUMENT;
    if (model->num_vars == 0) {
        model->obj_val = 0.0;
        model->status = CXF_OPTIMAL;
        return CXF_OPTIMAL;
    }
    if (model->num_constrs == 0) return solve_unconstrained(model);

    CxfEnv *env = model->env;
    if (env == NULL) return CXF_ERROR_NULL_ARGUMENT;
    if (model->matrix == NULL || model->matrix->col_ptr == NULL) {
        model->status = CXF_ERROR_NOT_SUPPORTED;
        return CXF_ERROR_NOT_SUPPORTED;
    }

    /* Check for obvious infeasibility via bound propagation */
    if (check_obvious_infeasibility(model)) {
        model->status = CXF_INFEASIBLE;
        return CXF_INFEASIBLE;
    }

    /* Check for obvious unboundedness via ray analysis */
    if (check_obvious_unboundedness(model)) {
        model->status = CXF_UNBOUNDED;
        return CXF_UNBOUNDED;
    }

    /* Initialize solver state */
    rc = cxf_simplex_init(model, &state);
    if (rc != CXF_OK) { model->status = rc; return rc; }

    int max_iter = state->max_iterations;

    /*=========================================================================
     * PHASE I: Find feasible basis using artificial variables
     *=========================================================================*/
    rc = setup_phase_one(state);
    if (rc != CXF_OK) {
        model->status = rc;
        cxf_simplex_final(state);
        return rc;
    }

    /* Apply anti-cycling perturbation (spec step 5) */
    cxf_simplex_perturbation(state, env);

    /* Compute initial Phase I reduced costs */
    compute_reduced_costs(state);

    /* Initial Phase I objective (sum of artificial values) */

    /* Phase I iteration loop */
    while (state->iteration < max_iter) {
        status = cxf_simplex_iterate(state, env);

        if (status == ITERATE_OPTIMAL) {
            /* Phase I optimal - check if feasible */
            if (state->obj_value > env->feasibility_tol) {
                /* Sum of artificials > 0: no feasible solution */
                model->status = CXF_INFEASIBLE;
                cxf_simplex_final(state);
                return CXF_INFEASIBLE;
            }
            /* Feasible basis found - proceed to Phase II */
            break;
        } else if (status == ITERATE_UNBOUNDED) {
            /* Phase I unbounded is impossible (artificials have lb=0) - bug */
            model->status = CXF_ERROR_NOT_SUPPORTED;
            cxf_simplex_final(state);
            return CXF_ERROR_NOT_SUPPORTED;
        } else if (status < 0) {
            model->status = status;
            cxf_simplex_final(state);
            return status;
        }
    }

    if (state->iteration >= max_iter) {
        model->status = CXF_ITERATION_LIMIT;
        cxf_simplex_final(state);
        return CXF_ITERATION_LIMIT;
    }

    /*=========================================================================
     * PHASE II: Optimize original objective
     *=========================================================================*/
    rc = transition_to_phase_two(state, model);
    if (rc != CXF_OK) {
        model->status = rc;
        cxf_simplex_final(state);
        return rc;
    }

    /* Recompute reduced costs with original objective */
    compute_reduced_costs(state);

    /* Phase II iteration loop */
    while (state->iteration < max_iter) {
        status = cxf_simplex_iterate(state, env);

        if (status == ITERATE_OPTIMAL) {
            model->status = CXF_OPTIMAL;
            break;
        } else if (status == ITERATE_UNBOUNDED) {
            model->status = CXF_UNBOUNDED;
            break;
        } else if (status == ITERATE_INFEASIBLE) {
            model->status = CXF_INFEASIBLE;
            break;
        } else if (status < 0) {
            model->status = status;
            break;
        }
    }

    if (state->iteration >= max_iter) model->status = CXF_ITERATION_LIMIT;

    /* Remove perturbation before extracting solution (spec step 8) */
    cxf_simplex_unperturb(state, env);

    /* Refine solution: snap near-bound values, clean zeros (spec step 9) */
    cxf_simplex_refine(state, env);

    if (model->status == CXF_OPTIMAL) cxf_extract_solution(state, model);

    cxf_simplex_final(state);
    return model->status;
}
