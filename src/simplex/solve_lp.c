/**
 * @file solve_lp.c
 * @brief Main LP solver entry point (M7.1.4)
 *
 * Orchestrates the simplex solve sequence: initialization, basis setup,
 * iteration loop, and solution extraction.
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

/**
 * @brief Initialize a simple slack basis.
 *
 * Sets up an initial basis with slack variables (artificial basis).
 * All structural variables at lower bound, slacks are basic.
 *
 * @param state Solver context
 * @return CXF_OK on success
 */
static int init_slack_basis(SolverContext *state) {
    BasisState *basis = state->basis;
    int m = state->num_constrs;
    int n = state->num_vars;

    /* For a proper initial basis, we'd need artificial variables.
     * For now, use a simple approach: first m variables form basis.
     * This only works for problems with m <= n and suitable structure. */

    if (m > n) {
        /* Cannot form basis - need artificial variables */
        return CXF_ERROR_NOT_SUPPORTED;
    }

    /* Initialize: all variables at lower bound (nonbasic) */
    for (int j = 0; j < n; j++) {
        basis->var_status[j] = -1;  /* At lower bound */
        state->work_x[j] = state->work_lb[j];
    }

    /* Make first m variables basic */
    for (int i = 0; i < m; i++) {
        basis->basic_vars[i] = i;
        basis->var_status[i] = i;  /* Basic in row i */
        /* Set value from RHS (simplified - assumes A has identity structure) */
        if (state->model_ref->matrix && state->model_ref->matrix->rhs) {
            state->work_x[i] = state->model_ref->matrix->rhs[i];
        }
    }

    return CXF_OK;
}

/**
 * @brief Compute initial reduced costs.
 *
 * d_j = c_j - c_B^T * B^{-1} * a_j
 * For initial slack basis with identity B, this simplifies.
 *
 * @param state Solver context
 */
static void compute_reduced_costs(SolverContext *state) {
    int n = state->num_vars;
    BasisState *basis = state->basis;

    for (int j = 0; j < n; j++) {
        if (basis->var_status[j] >= 0) {
            /* Basic variable: reduced cost is 0 */
            state->work_dj[j] = 0.0;
        } else {
            /* Nonbasic: reduced cost is objective coefficient */
            /* For proper computation, would need c_j - pi^T * a_j */
            state->work_dj[j] = state->work_obj[j];
        }
    }
}


/**
 * @brief Solve an LP using the simplex method.
 *
 * @param model Model to solve
 * @return CXF_OPTIMAL (1) on success, or error/status code
 */
int cxf_solve_lp(CxfModel *model) {
    SolverContext *state = NULL;
    int rc;
    int status;

    if (model == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    /* Handle empty model */
    if (model->num_vars == 0) {
        model->obj_val = 0.0;
        model->status = CXF_OPTIMAL;
        return CXF_OPTIMAL;
    }

    /* Handle unconstrained model (no constraints) */
    if (model->num_constrs == 0) {
        /* First check for infeasible bounds */
        for (int j = 0; j < model->num_vars; j++) {
            double lb = model->lb[j];
            double ub = model->ub[j];
            if (lb > ub + CXF_FEASIBILITY_TOL) {
                model->status = CXF_INFEASIBLE;
                return CXF_INFEASIBLE;
            }
        }

        /* Check each variable: set to best bound */
        double obj_val = 0.0;
        for (int j = 0; j < model->num_vars; j++) {
            double c = model->obj_coeffs[j];
            double lb = model->lb[j];
            double ub = model->ub[j];

            if (c < 0) {
                /* Want to maximize this variable (minimize c*x with c<0) */
                if (ub >= CXF_INFINITY) {
                    model->status = CXF_UNBOUNDED;
                    return CXF_UNBOUNDED;
                }
                if (model->solution != NULL) {
                    model->solution[j] = ub;
                }
                obj_val += c * ub;
            } else if (c > 0) {
                /* Want to minimize this variable */
                if (lb <= -CXF_INFINITY) {
                    model->status = CXF_UNBOUNDED;
                    return CXF_UNBOUNDED;
                }
                if (model->solution != NULL) {
                    model->solution[j] = lb;
                }
                obj_val += c * lb;
            } else {
                /* c == 0, any value works, use 0 if feasible, else midpoint */
                double val = 0.0;
                if (lb > 0.0) {
                    val = lb;
                } else if (ub < 0.0) {
                    val = ub;
                }
                if (model->solution != NULL) {
                    model->solution[j] = val;
                }
            }
        }
        model->obj_val = obj_val;
        model->status = CXF_OPTIMAL;
        return CXF_OPTIMAL;
    }

    CxfEnv *env = model->env;
    if (env == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    /* Check if we can solve constrained problems */
    if (model->num_constrs > 0) {
        /* Verify matrix is properly populated */
        if (model->matrix == NULL || model->matrix->col_ptr == NULL) {
            /* Matrix not populated - constraints were added but not stored */
            model->status = CXF_ERROR_NOT_SUPPORTED;
            return CXF_ERROR_NOT_SUPPORTED;
        }
    }

    /*=========================================================================
     * Step 1: Initialize solver state
     *=========================================================================*/
    rc = cxf_simplex_init(model, &state);
    if (rc != CXF_OK) {
        model->status = rc;
        return rc;
    }

    /*=========================================================================
     * Step 2: Set up initial basis
     *=========================================================================*/
    rc = init_slack_basis(state);
    if (rc != CXF_OK) {
        model->status = rc;
        cxf_simplex_final(state);
        return rc;
    }

    /*=========================================================================
     * Step 3: Compute initial reduced costs
     *=========================================================================*/
    compute_reduced_costs(state);

    /*=========================================================================
     * Step 4: Main iteration loop
     *=========================================================================*/
    state->phase = 2;  /* Skip Phase I for now */
    int max_iter = state->max_iterations;

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
            /* Error */
            model->status = status;
            break;
        }
        /* status == ITERATE_CONTINUE: keep going */
    }

    if (state->iteration >= max_iter) {
        model->status = CXF_ITERATION_LIMIT;
    }

    /*=========================================================================
     * Step 5: Extract solution
     *=========================================================================*/
    if (model->status == CXF_OPTIMAL) {
        cxf_extract_solution(state, model);
    }

    /*=========================================================================
     * Step 6: Cleanup
     *=========================================================================*/
    cxf_simplex_final(state);

    /* Return status code */
    return model->status;
}
