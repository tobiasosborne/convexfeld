/**
 * @file context.c
 * @brief SolverContext lifecycle functions (M7.1.1)
 *
 * Implements creation and destruction of solver context for simplex method.
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_pricing.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_env.h"
#include <stdlib.h>
#include <string.h>

/* Default iteration limit */
#define DEFAULT_MAX_ITERATIONS 1000000

/* Default optimality tolerance */
#define DEFAULT_TOLERANCE 1e-6

/* Forward declare basis creation */
extern BasisState *cxf_basis_create(int m, int n);
extern void cxf_basis_free(BasisState *basis);

/**
 * @brief Create and initialize solver context.
 *
 * @param model Model to solve (must be valid)
 * @param stateP Output pointer for solver context
 * @return CXF_OK on success, error code otherwise
 */
int cxf_simplex_init(CxfModel *model, SolverContext **stateP) {
    SolverContext *ctx;
    int n, m;

    if (model == NULL || stateP == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    *stateP = NULL;

    n = model->num_vars;
    m = model->num_constrs;

    ctx = (SolverContext *)calloc(1, sizeof(SolverContext));
    if (ctx == NULL) {
        return CXF_ERROR_OUT_OF_MEMORY;
    }

    /* Store reference and dimensions */
    ctx->model_ref = model;
    ctx->num_vars = n;
    ctx->num_constrs = m;
    ctx->num_nonzeros = 0;  /* Will be set from matrix */

    /* Initialize state */
    ctx->phase = 0;
    ctx->solve_mode = 0;  /* Primal simplex */
    ctx->max_iterations = DEFAULT_MAX_ITERATIONS;
    ctx->tolerance = DEFAULT_TOLERANCE;
    ctx->obj_value = 0.0;
    ctx->iteration = 0;
    ctx->last_refactor_iter = 0;

    /* Allocate working arrays for variables
     * Size is n + m to accommodate artificial variables for Phase I.
     * Original vars: indices [0, n-1]
     * Artificial vars: indices [n, n+m-1]
     */
    int total_vars = n + m;
    ctx->num_artificials = 0;  /* Set during Phase I setup */

    if (total_vars > 0) {
        ctx->work_lb = (double *)malloc((size_t)total_vars * sizeof(double));
        ctx->work_ub = (double *)malloc((size_t)total_vars * sizeof(double));
        ctx->work_obj = (double *)malloc((size_t)total_vars * sizeof(double));
        ctx->work_x = (double *)calloc((size_t)total_vars, sizeof(double));
        ctx->work_dj = (double *)calloc((size_t)total_vars, sizeof(double));

        if (ctx->work_lb == NULL || ctx->work_ub == NULL ||
            ctx->work_obj == NULL || ctx->work_x == NULL ||
            ctx->work_dj == NULL) {
            cxf_simplex_final(ctx);
            return CXF_ERROR_OUT_OF_MEMORY;
        }

        /* Copy bounds and objective from model for original variables */
        if (n > 0) {
            memcpy(ctx->work_lb, model->lb, (size_t)n * sizeof(double));
            memcpy(ctx->work_ub, model->ub, (size_t)n * sizeof(double));
            memcpy(ctx->work_obj, model->obj_coeffs, (size_t)n * sizeof(double));
        }

        /* Initialize artificial variable slots with default values */
        for (int i = n; i < total_vars; i++) {
            ctx->work_lb[i] = 0.0;       /* Artificials have lb = 0 */
            ctx->work_ub[i] = CXF_INFINITY;  /* Artificials unbounded above */
            ctx->work_obj[i] = 0.0;      /* Set during Phase I */
        }
    }

    /* Allocate dual values array for constraints */
    if (m > 0) {
        ctx->work_pi = (double *)calloc((size_t)m, sizeof(double));
        if (ctx->work_pi == NULL) {
            cxf_simplex_final(ctx);
            return CXF_ERROR_OUT_OF_MEMORY;
        }

        /* Allocate iteration work arrays (preallocated to avoid malloc per iter) */
        ctx->work_column = (double *)malloc((size_t)m * sizeof(double));
        ctx->work_cB = (double *)malloc((size_t)m * sizeof(double));
        if (ctx->work_column == NULL || ctx->work_cB == NULL) {
            cxf_simplex_final(ctx);
            return CXF_ERROR_OUT_OF_MEMORY;
        }
    }

    /* Create basis state with space for artificial variables */
    ctx->basis = cxf_basis_create(m, total_vars);
    if (ctx->basis == NULL && (m > 0 || total_vars > 0)) {
        cxf_simplex_final(ctx);
        return CXF_ERROR_OUT_OF_MEMORY;
    }

    /* Pricing context created on demand */
    ctx->pricing = NULL;

    /* Initialize tracking fields */
    ctx->eta_count = 0;
    ctx->eta_memory = 0;
    ctx->total_ftran_time = 0.0;
    ctx->ftran_count = 0;
    ctx->baseline_ftran = 0.0;

    *stateP = ctx;
    return CXF_OK;
}

/**
 * @brief Free solver context and all resources.
 *
 * @param state Context to free (may be NULL)
 */
void cxf_simplex_final(SolverContext *state) {
    if (state == NULL) {
        return;
    }

    /* Free working arrays */
    free(state->work_lb);
    free(state->work_ub);
    free(state->work_obj);
    free(state->work_x);
    free(state->work_pi);
    free(state->work_dj);
    free(state->work_counter);
    free(state->work_column);
    free(state->work_cB);

    /* Free basis */
    cxf_basis_free(state->basis);

    /* Free pricing context if allocated */
    /* Note: pricing context free not implemented yet */

    /* Free timing if allocated */
    free(state->timing);

    /* Free the context itself */
    free(state);
}

/* cxf_simplex_setup is implemented in setup.c */

/**
 * @brief Get solver status (stub - to be implemented).
 *
 * @param state Solver context
 * @return Status code or error
 */
int cxf_simplex_get_status(SolverContext *state) {
    if (state == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }
    return 0;  /* Stub: not yet solved */
}

/**
 * @brief Get iteration count (stub - to be implemented).
 *
 * @param state Solver context
 * @return Iteration count or error
 */
int cxf_simplex_get_iteration(SolverContext *state) {
    if (state == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }
    return state->iteration;
}

/**
 * @brief Get solver phase (stub - to be implemented).
 *
 * @param state Solver context
 * @return Phase (0, 1, or 2) or error
 */
int cxf_simplex_get_phase(SolverContext *state) {
    if (state == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }
    return state->phase;
}

/* cxf_simplex_iterate is implemented in iterate.c */

/* cxf_simplex_phase_end is implemented in post.c */

/* cxf_simplex_post_iterate is implemented in post.c */

/**
 * @brief Get current objective value (stub - to be implemented).
 *
 * @param state Solver context
 * @return Objective value or NaN on error
 */
double cxf_simplex_get_objval(SolverContext *state) {
    if (state == NULL) {
        return 0.0 / 0.0;  /* NaN */
    }
    return state->obj_value;
}

/**
 * @brief Set iteration limit (stub - to be implemented).
 *
 * @param state Solver context
 * @param limit Iteration limit (must be >= 0)
 * @return CXF_OK on success, error code otherwise
 */
int cxf_simplex_set_iteration_limit(SolverContext *state, int limit) {
    if (state == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }
    if (limit < 0) {
        return CXF_ERROR_INVALID_ARGUMENT;
    }
    state->max_iterations = limit;
    return CXF_OK;
}

/**
 * @brief Get iteration limit (stub - to be implemented).
 *
 * @param state Solver context
 * @return Iteration limit or error code
 */
int cxf_simplex_get_iteration_limit(SolverContext *state) {
    if (state == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }
    return state->max_iterations;
}

/**
 * @brief Apply perturbation for degeneracy handling (stub - to be implemented in M7.1.3).
 *
 * @param state Solver context
 * @param env Environment
 * @return CXF_OK on success, error code otherwise
 */
int cxf_simplex_perturbation(SolverContext *state, CxfEnv *env) {
    if (state == NULL || env == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }
    return CXF_OK;
}

/**
 * @brief Remove perturbation (stub - to be implemented in M7.1.3).
 *
 * @param state Solver context
 * @param env Environment
 * @return CXF_OK on success, error code otherwise
 */
int cxf_simplex_unperturb(SolverContext *state, CxfEnv *env) {
    if (state == NULL || env == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }
    return CXF_OK;
}
