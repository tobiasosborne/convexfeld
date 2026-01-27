/**
 * @file setup.c
 * @brief Simplex setup and preprocessing (M7.1.6)
 *
 * Implements cxf_simplex_setup and cxf_simplex_preprocess.
 * Setup prepares working arrays and determines initial phase.
 * Preprocessing reduces problem size via bound tightening and scaling.
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"
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
extern PricingContext *cxf_pricing_create(int num_vars, int max_levels);
extern int cxf_pricing_init(PricingContext *ctx, int num_vars, int strategy);

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
static void init_reduced_costs(SolverContext *state) {
    int n = state->num_vars;
    if (n > 0 && state->work_dj != NULL && state->work_obj != NULL) {
        memcpy(state->work_dj, state->work_obj, (size_t)n * sizeof(double));
    }
}

/**
 * @brief Zero-initialize dual values.
 */
static void init_dual_values(SolverContext *state) {
    int m = state->num_constrs;
    if (m > 0 && state->work_pi != NULL) {
        memset(state->work_pi, 0, (size_t)m * sizeof(double));
    }
}

/**
 * @brief Initialize pricing context.
 */
static int init_pricing(SolverContext *state) {
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
 * @brief Set up solver context for iteration.
 *
 * Initializes reduced costs, dual values, pricing, and determines
 * initial phase based on bound feasibility.
 *
 * @param state Solver context (must be initialized via cxf_simplex_init)
 * @param env Environment containing solver parameters
 * @return CXF_OK on success, error code otherwise
 */
int cxf_simplex_setup(SolverContext *state, CxfEnv *env) {
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
 * @brief Preprocess the LP problem.
 *
 * Performs preprocessing reductions:
 * - Fixed variable elimination (lb = ub)
 * - Bound propagation (if matrix available)
 * - Geometric mean scaling
 *
 * @param state Solver context
 * @param env Environment
 * @param flags Control flags (bit 0: skip if set)
 * @return CXF_OK on success, 3=infeasible
 */
int cxf_simplex_preprocess(SolverContext *state, CxfEnv *env, int flags) {
    if (state == NULL || env == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    /* Check if preprocessing disabled via flags */
    if (flags & 1) {
        return CXF_OK;
    }

    int n = state->num_vars;
    double feas_tol = env->feasibility_tol;
    if (feas_tol <= 0.0) feas_tol = DEFAULT_FEASIBILITY_TOL;

    double *lb = state->work_lb;
    double *ub = state->work_ub;

    if (n == 0 || lb == NULL || ub == NULL) {
        return CXF_OK;
    }

    /* Fixed variable handling: check for infeasibility */
    for (int j = 0; j < n; j++) {
        if (lb[j] > ub[j] + feas_tol) {
            return 3;  /* Infeasible */
        }
    }

    /*
     * Note: Full preprocessing (singleton elimination, bound propagation)
     * requires constraint matrix access. The current codebase has stub
     * constraint handling. For now, we do minimal preprocessing.
     *
     * Future implementation should:
     * 1. Eliminate fixed variables (lb = ub within tolerance)
     * 2. Process singleton rows
     * 3. Propagate bounds iteratively
     * 4. Apply geometric mean scaling
     */

    return CXF_OK;
}
