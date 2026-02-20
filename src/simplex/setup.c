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
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_matrix.h"
#include "convexfeld/cxf_pricing.h"
#include "convexfeld/cxf_types.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Default parameters */
#define DEFAULT_MAX_ITERATIONS 10000000
#define DEFAULT_FEASIBILITY_TOL 1e-6
#define DEFAULT_OPTIMALITY_TOL 1e-6
#define MAX_PREPROCESS_PASSES 10
#define SCALE_CLAMP_MIN 1e-6
#define SCALE_CLAMP_MAX 1e6

/* Forward declarations */
extern PricingState *cxf_pricing_create(int num_vars, int max_levels);
extern int cxf_pricing_init(PricingState *ctx, int num_vars, int strategy);

/**
 * @brief Clamp a value to [min, max].
 */
static double clamp(double val, double min_val, double max_val) {
    if (val < min_val) return min_val;
    if (val > max_val) return max_val;
    return val;
}

/**
 * @brief Check if any bounds are infeasible (lb > ub).
 */
static int has_bound_violation(const double *lb, const double *ub,
                               int n, double tol) {
    for (int j = 0; j < n; j++) {
        if (lb[j] > ub[j] + tol) {
            return 1;
        }
    }
    return 0;
}

/**
 * @brief Initialize reduced costs from objective coefficients.
 */
static void init_reduced_costs(SolverState *state) {
    int n = state->num_vars;
    if (n > 0 && state->work_dj != NULL && state->work_obj != NULL) {
        memcpy(state->work_dj, state->work_obj, (size_t)n * sizeof(double));
    }
}

/**
 * @brief Zero-initialize dual values.
 */
static void init_dual_values(SolverState *state) {
    int m = state->num_constrs;
    if (m > 0 && state->work_pi != NULL) {
        memset(state->work_pi, 0, (size_t)m * sizeof(double));
    }
}

/**
 * @brief Initialize pricing context.
 */
static int init_pricing(SolverState *state) {
    int n = state->num_vars;

    if (n == 0) {
        state->pricing = NULL;
        return CXF_OK;
    }

    /* Create pricing context with 3 levels */
    state->pricing = cxf_pricing_create(n, 3);
    if (state->pricing == NULL) {
        return CXF_ERROR_OUT_OF_MEMORY;
    }

    /* Initialize with auto strategy (0) */
    int status = cxf_pricing_init(state->pricing, n, 0);
    if (status != CXF_OK) {
        return status;
    }

    return CXF_OK;
}

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
static void compute_activity_bounds(SolverState *state, int count,
                                    const int *indices) {
    CxfModel *model = state->model_ref;
    if (model == NULL || model->matrix == NULL) return;
    if (state->min_activity == NULL || state->max_activity == NULL) return;

    MatrixData *mat = model->matrix;
    int m = state->num_constrs;
    int n = state->num_vars;

    /* Determine which constraints to compute */
    int do_all = (count == 0 || indices == NULL);
    if (do_all) {
        memset(state->min_activity, 0, (size_t)m * sizeof(double));
        memset(state->max_activity, 0, (size_t)m * sizeof(double));
    } else {
        for (int k = 0; k < count; k++) {
            int i = indices[k];
            if (i >= 0 && i < m) {
                state->min_activity[i] = 0.0;
                state->max_activity[i] = 0.0;
            }
        }
    }

    /* Accumulate contributions from each variable column (CSC) */
    if (mat->col_ptr == NULL || mat->row_idx == NULL || mat->values == NULL)
        return;

    for (int j = 0; j < n; j++) {
        double lb_j = state->work_lb[j];
        double ub_j = state->work_ub[j];
        int64_t start = mat->col_ptr[j];
        int64_t end = mat->col_ptr[j + 1];

        for (int64_t k = start; k < end; k++) {
            int row = mat->row_idx[k];
            double a = mat->values[k];

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
                /* Infinite bound: set activity to infinity */
                if (a > 0) {
                    if (lb_j <= -CXF_INFINITY)
                        state->min_activity[row] = -CXF_INFINITY;
                    if (ub_j >= CXF_INFINITY)
                        state->max_activity[row] = CXF_INFINITY;
                    if (lb_j > -CXF_INFINITY)
                        state->min_activity[row] += prod_lb;
                    if (ub_j < CXF_INFINITY)
                        state->max_activity[row] += prod_ub;
                } else {
                    if (ub_j >= CXF_INFINITY)
                        state->min_activity[row] = -CXF_INFINITY;
                    if (lb_j <= -CXF_INFINITY)
                        state->max_activity[row] = CXF_INFINITY;
                    if (ub_j < CXF_INFINITY)
                        state->min_activity[row] += prod_ub;
                    if (lb_j > -CXF_INFINITY)
                        state->max_activity[row] += prod_lb;
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
 * @brief Set up solver context for iteration.
 *
 * Initializes reduced costs, dual values, pricing, and determines
 * initial phase based on bound feasibility.
 *
 * @param state Solver context (must be initialized via cxf_simplex_init)
 * @param env Environment containing solver parameters
 * @return CXF_OK on success, error code otherwise
 */
int cxf_simplex_setup(SolverState *state, CxfEnv *env) {
    if (state == NULL || env == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    int n = state->num_vars;

    /* Read parameters from environment */
    double feas_tol = env->feasibility_tol;
    double opt_tol = env->optimality_tol;

    if (feas_tol <= 0.0) feas_tol = DEFAULT_FEASIBILITY_TOL;
    if (opt_tol <= 0.0) opt_tol = DEFAULT_OPTIMALITY_TOL;

    /* Store parameters in state */
    state->tolerance = opt_tol;
    /* max_iterations is already set by cxf_simplex_init */

    /* Initialize reduced costs from objective coefficients */
    init_reduced_costs(state);

    /* Zero-initialize dual values */
    init_dual_values(state);

    /* Initialize pricing context if not already done */
    if (state->pricing == NULL && n > 0) {
        int status = init_pricing(state);
        if (status != CXF_OK) {
            return status;
        }
    }

    /* Compute initial activity bounds (v2 C1) */
    compute_activity_bounds(state, 0, NULL);

    /* Reset eta tracking */
    state->eta_count = 0;
    state->eta_memory = 0;

    /* Reset iteration tracking */
    state->iteration = 0;
    state->last_refactor_iter = 0;
    state->obj_value = 0.0;

    /* Determine initial phase based on bound feasibility */
    if (has_bound_violation(state->work_lb, state->work_ub, n, feas_tol)) {
        state->phase = 1;  /* Phase I needed */
    } else {
        state->phase = 2;  /* Go directly to Phase II */
    }

    return CXF_OK;
}

/**
 * @brief Preprocess: fix near-bound variables (v2 P3.21).
 *
 * Scans variables with tight bound range (ub - lb < threshold).
 * Fixes them at the closest bound, updates activity bounds.
 *
 * @param state Solver context
 * @param env   Environment with feasibility_tol
 * @param flags Control flags (bit 0: skip if set)
 * @return 0 on success, CXF_INFEASIBLE if lb > ub
 */
int cxf_simplex_preprocess(SolverState *state, CxfEnv *env, int flags) {
    if (state == NULL || env == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    if (flags & 1) return CXF_OK;

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

        /* Fix at closest bound */
        if (state->basis != NULL && state->basis->var_status != NULL &&
            state->basis->var_status[j] < 0) {
            double x_j = (state->work_x != NULL) ? state->work_x[j] : lb[j];
            double target = (fabs(x_j - lb[j]) <= fabs(x_j - ub[j])) ?
                            lb[j] : ub[j];
            if (state->work_x != NULL) state->work_x[j] = target;
            lb[j] = target;
            ub[j] = target;
            state->cols_eliminated++;
        }
    }

    /* Recompute activity bounds after fixing */
    if (state->cols_eliminated > 0) {
        compute_activity_bounds(state, 0, NULL);
    }

    return CXF_OK;
}
