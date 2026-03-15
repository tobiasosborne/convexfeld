/**
 * @file context.c
 * @brief SolverState lifecycle functions (M7.1.1)
 *
 * Implements creation and destruction of solver context for simplex method.
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_matrix.h"
#include "convexfeld/cxf_pricing.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_env.h"
#include <stdlib.h>
#include <string.h>

#include "../basis/basis_internal.h"

/* Default iteration limit */
#define DEFAULT_MAX_ITERATIONS 1000000

/* Default optimality tolerance */
#define DEFAULT_TOLERANCE 1e-6

/* Forward declare basis creation */

/**
 * @brief Create and initialize solver context.
 *
 * @param model Model to solve (must be valid)
 * @param stateP Output pointer for solver context
 * @return CXF_OK on success, error code otherwise
 */
int cxf_simplex_init(CxfModel *model, SolverState **stateP) {
    SolverState *ctx;
    int n, m;

    if (model == NULL || stateP == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    *stateP = NULL;

    n = model->num_vars;
    m = model->num_constrs;

    ctx = (SolverState *)calloc(1, sizeof(SolverState));
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

    /* Allocate working arrays for variables.
     * Size is n + m (structural + slack/surplus). No artificial variable slots.
     * Phase I uses implicit bound-violation approach per two_phase_method.md.
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

        /* Initialize slack/surplus and artificial slots */
        for (int i = n; i < total_vars; i++) {
            ctx->work_lb[i] = 0.0;
            ctx->work_ub[i] = CXF_INFINITY;
            ctx->work_obj[i] = 0.0;
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

    /* V2: Per-variable structural flags (solver_state.md Variable Flags).
     * Zero-initialized: LP-only has no quadratic/SOS flags. */
    if (total_vars > 0) {
        ctx->var_flags = (uint32_t *)calloc((size_t)total_vars,
                                            sizeof(uint32_t));
        if (ctx->var_flags == NULL) {
            cxf_simplex_final(ctx);
            return CXF_ERROR_OUT_OF_MEMORY;
        }
    }

    /* B1: Saved bounds (copy of initial bounds for EXPAND perturbation) */
    if (total_vars > 0) {
        ctx->saved_lb = (double *)malloc((size_t)total_vars * sizeof(double));
        ctx->saved_ub = (double *)malloc((size_t)total_vars * sizeof(double));
        if (ctx->saved_lb == NULL || ctx->saved_ub == NULL) {
            cxf_simplex_final(ctx);
            return CXF_ERROR_OUT_OF_MEMORY;
        }
        memcpy(ctx->saved_lb, ctx->work_lb, (size_t)total_vars * sizeof(double));
        memcpy(ctx->saved_ub, ctx->work_ub, (size_t)total_vars * sizeof(double));
    }

    /* B2: Activity bounds (per-constraint min/max LHS values) */
    if (m > 0) {
        ctx->min_activity = (double *)calloc((size_t)m, sizeof(double));
        ctx->max_activity = (double *)calloc((size_t)m, sizeof(double));
        ctx->negUnbdCount = (int *)calloc((size_t)m, sizeof(int));
        ctx->posUnbdCount = (int *)calloc((size_t)m, sizeof(int));
        if (ctx->min_activity == NULL || ctx->max_activity == NULL ||
            ctx->negUnbdCount == NULL || ctx->posUnbdCount == NULL) {
            cxf_simplex_final(ctx);
            return CXF_ERROR_OUT_OF_MEMORY;
        }
    }

    /* B3: Progress counters (zero-initialized by calloc) */
    ctx->obj_at_last_refactor = 0.0;
    ctx->iteration_mode = 0;
    ctx->rows_eliminated = 0;
    ctx->cols_eliminated = 0;
    ctx->bounds_propagated = 0;
    ctx->flip_count = 0;
    memset(ctx->progress_snapshot, 0, sizeof(ctx->progress_snapshot));

    /* Crash basis arrays (v2 — P2.5) */
    ctx->num_basic = 0;
    ctx->problem_var_index = -1;
    if (m > 0) {
        ctx->row_status = (int *)calloc((size_t)m, sizeof(int));
        if (ctx->row_status == NULL) {
            cxf_simplex_final(ctx);
            return CXF_ERROR_OUT_OF_MEMORY;
        }
    }
    if (total_vars > 0) {
        ctx->col_nz_count = (int *)calloc((size_t)total_vars, sizeof(int));
        if (ctx->col_nz_count == NULL) {
            cxf_simplex_final(ctx);
            return CXF_ERROR_OUT_OF_MEMORY;
        }
        /* col_nz_count populated after CSC copy below */
    }

    /* P3.1: Copy constraint matrix into SolverState-owned arrays.
     * This prevents BFRT row negation from corrupting the original model. */
    if (model->matrix != NULL) {
        MatrixData *mat = model->matrix;
        int64_t nnz = mat->nnz;
        ctx->num_nonzeros = nnz;

        /* CSC (column-major) — always present */
        if (mat->col_ptr != NULL && n > 0) {
            ctx->csc_col_ptr = (int64_t *)malloc((size_t)(n + 1) * sizeof(int64_t));
            if (ctx->csc_col_ptr == NULL) {
                cxf_simplex_final(ctx); return CXF_ERROR_OUT_OF_MEMORY;
            }
            memcpy(ctx->csc_col_ptr, mat->col_ptr,
                   (size_t)(n + 1) * sizeof(int64_t));
        }
        if (mat->row_idx != NULL && nnz > 0) {
            ctx->csc_row_idx = (int *)malloc((size_t)nnz * sizeof(int));
            if (ctx->csc_row_idx == NULL) {
                cxf_simplex_final(ctx); return CXF_ERROR_OUT_OF_MEMORY;
            }
            memcpy(ctx->csc_row_idx, mat->row_idx, (size_t)nnz * sizeof(int));
        }
        if (mat->values != NULL && nnz > 0) {
            ctx->csc_values = (double *)malloc((size_t)nnz * sizeof(double));
            if (ctx->csc_values == NULL) {
                cxf_simplex_final(ctx); return CXF_ERROR_OUT_OF_MEMORY;
            }
            memcpy(ctx->csc_values, mat->values, (size_t)nnz * sizeof(double));
        }

        /* CSR (row-major) — optional, may be NULL */
        if (mat->row_ptr != NULL && m > 0) {
            ctx->csr_row_ptr = (int64_t *)malloc((size_t)(m + 1) * sizeof(int64_t));
            if (ctx->csr_row_ptr == NULL) {
                cxf_simplex_final(ctx); return CXF_ERROR_OUT_OF_MEMORY;
            }
            memcpy(ctx->csr_row_ptr, mat->row_ptr,
                   (size_t)(m + 1) * sizeof(int64_t));
        }
        if (mat->col_idx != NULL && nnz > 0) {
            ctx->csr_col_idx = (int *)malloc((size_t)nnz * sizeof(int));
            if (ctx->csr_col_idx == NULL) {
                cxf_simplex_final(ctx); return CXF_ERROR_OUT_OF_MEMORY;
            }
            memcpy(ctx->csr_col_idx, mat->col_idx, (size_t)nnz * sizeof(int));
        }
        if (mat->row_values != NULL && nnz > 0) {
            ctx->csr_values = (double *)malloc((size_t)nnz * sizeof(double));
            if (ctx->csr_values == NULL) {
                cxf_simplex_final(ctx); return CXF_ERROR_OUT_OF_MEMORY;
            }
            memcpy(ctx->csr_values, mat->row_values,
                   (size_t)nnz * sizeof(double));
        }

        /* Constraint metadata */
        if (mat->rhs != NULL && m > 0) {
            ctx->work_rhs = (double *)malloc((size_t)m * sizeof(double));
            if (ctx->work_rhs == NULL) {
                cxf_simplex_final(ctx); return CXF_ERROR_OUT_OF_MEMORY;
            }
            memcpy(ctx->work_rhs, mat->rhs, (size_t)m * sizeof(double));
        }
        if (mat->sense != NULL && m > 0) {
            ctx->work_sense = (char *)malloc((size_t)m * sizeof(char));
            if (ctx->work_sense == NULL) {
                cxf_simplex_final(ctx); return CXF_ERROR_OUT_OF_MEMORY;
            }
            memcpy(ctx->work_sense, mat->sense, (size_t)m * sizeof(char));
        }
    }

    /* Populate col_nz_count from owned CSC copy (must run AFTER CSC copy).
     * The CSC copy above creates ctx->csc_col_ptr; col_nz_count was
     * allocated earlier but left zero-initialized until the copy exists. */
    if (ctx->col_nz_count != NULL && ctx->csc_col_ptr != NULL) {
        for (int j = 0; j < n; j++) {
            ctx->col_nz_count[j] = (int)(ctx->csc_col_ptr[j + 1]
                                          - ctx->csc_col_ptr[j]);
        }
        for (int i = 0; i < m; i++) {
            ctx->col_nz_count[n + i] = 1;  /* slack: 1 nonzero (diagonal) */
        }
    }

    /* Create basis state with space for artificial variables */
    ctx->basis = cxf_basis_create(m, total_vars);
    if (ctx->basis == NULL && (m > 0 || total_vars > 0)) {
        cxf_simplex_final(ctx);
        return CXF_ERROR_OUT_OF_MEMORY;
    }

    /* P2.3: Create eta memory pool for arena allocation */
    if (ctx->basis != NULL) {
        ctx->basis->eta_pool = cxf_eta_pool_create(CXF_MIN_CHUNK_SIZE);
        /* Pool creation failure is non-fatal — falls back to calloc */
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
 * P6.2: Before freeing, performs dual-feasibility variable fixing
 * (complementary slackness) for nonbasic variables. This ensures
 * the solution satisfies CS conditions before extraction.
 *
 * @param state Context to free (may be NULL)
 */
void cxf_simplex_final(SolverState *state) {
    if (state == NULL) {
        return;
    }

    /* P6.2: CS fix moved to solve_lp.c (before extract_solution) */

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

    /* Free variable structural flags (V2) */
    free(state->var_flags);

    /* Free saved bounds (B1) */
    free(state->saved_lb);
    free(state->saved_ub);

    /* Free activity bounds (B2) */
    free(state->min_activity);
    free(state->max_activity);
    free(state->negUnbdCount);
    free(state->posUnbdCount);

    /* Free crash basis arrays (v2) */
    free(state->row_status);
    free(state->col_nz_count);

    /* Free P3.1 matrix working copies */
    free(state->csc_col_ptr);
    free(state->csc_row_idx);
    free(state->csc_values);
    free(state->csr_row_ptr);
    free(state->csr_col_idx);
    free(state->csr_values);
    free(state->work_rhs);
    free(state->work_sense);

    /* Free scaling factors */
    free(state->row_scale);
    free(state->col_scale);

    /* Free basis */
    cxf_basis_free(state->basis);

    /* Free pricing context if allocated */
    {
        if (state->pricing != NULL)
            cxf_pricing_free(state->pricing);
    }

    /* Free timing if allocated */
    free(state->timing);

    /* Free the context itself */
    free(state);
}

/* cxf_simplex_setup is implemented in setup.c */

/**
 * @brief Get solver status.
 *
 * @param state Solver context
 * @return Status code or error
 */
int cxf_simplex_get_status(SolverState *state) {
    if (state == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }
    return 0;  /* Not yet solved */
}

/**
 * @brief Get iteration count.
 *
 * @param state Solver context
 * @return Iteration count or error
 */
int cxf_simplex_get_iteration(SolverState *state) {
    if (state == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }
    return state->iteration;
}

/**
 * @brief Get solver phase.
 *
 * @param state Solver context
 * @return Phase (0, 1, or 2) or error
 */
int cxf_simplex_get_phase(SolverState *state) {
    if (state == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }
    return state->phase;
}

/* cxf_simplex_iterate (logging-only) is in iterate.c */
/* cxf_simplex_step (iteration engine) is in step.c */

/* cxf_simplex_phase_end is implemented in post.c */

/* cxf_simplex_post_iterate is implemented in post.c */

/**
 * @brief Get current objective value.
 *
 * @param state Solver context
 * @return Objective value or NaN on error
 */
double cxf_simplex_get_objval(SolverState *state) {
    if (state == NULL) {
        return 0.0 / 0.0;  /* NaN */
    }
    return state->obj_value;
}

/**
 * @brief Set iteration limit.
 *
 * @param state Solver context
 * @param limit Iteration limit (must be >= 0)
 * @return CXF_OK on success, error code otherwise
 */
int cxf_simplex_set_iteration_limit(SolverState *state, int limit) {
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
 * @brief Get iteration limit.
 *
 * @param state Solver context
 * @return Iteration limit or error code
 */
int cxf_simplex_get_iteration_limit(SolverState *state) {
    if (state == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }
    return state->max_iterations;
}

/* cxf_simplex_perturbation and cxf_simplex_unperturb are in perturbation.c */
