/**
 * @file cxf_solver.h
 * @brief SolverContext structure - runtime solver state.
 *
 * The solver context holds all working state during LP optimization.
 * It isolates mutable solver data from the immutable model definition.
 */

#ifndef CXF_SOLVER_H
#define CXF_SOLVER_H

#include "cxf_types.h"
#include "cxf_timing.h"

/**
 * @brief Solver context for LP optimization.
 *
 * Contains problem data copies, algorithmic state, and working arrays.
 * Created at solve start, destroyed after completion.
 */
struct SolverContext {
    CxfModel *model_ref;      /**< Back-pointer to model */

    /* Problem dimensions */
    int num_vars;             /**< Number of variables */
    int num_constrs;          /**< Number of constraints */
    int64_t num_nonzeros;     /**< Number of non-zeros */

    /* Solver state */
    int phase;                /**< 0=setup, 1=phase I, 2=phase II */
    int solve_mode;           /**< 0=primal, 1=dual, 2=barrier */
    int max_iterations;       /**< Iteration limit */
    double tolerance;         /**< Optimality tolerance */
    double obj_value;         /**< Current objective value */

    /* Working arrays */
    double *work_lb;          /**< Working lower bounds [num_vars] */
    double *work_ub;          /**< Working upper bounds [num_vars] */
    double *work_obj;         /**< Working objective [num_vars] */
    double *work_x;           /**< Current solution [num_vars] */
    double *work_pi;          /**< Dual values [num_constrs] */
    double *work_dj;          /**< Reduced costs [num_vars] */

    /* Subcomponents */
    BasisState *basis;        /**< Current basis state */
    PricingContext *pricing;  /**< Pricing context */

    /* Work tracking for refactorization decisions */
    double *work_counter;     /**< Accumulated work counter (NULL to disable) */
    double scale_factor;      /**< Work scaling factor */
    TimingState *timing;      /**< Timing state (NULL to disable) */

    /* Refactorization tracking */
    int eta_count;            /**< Number of eta vectors since last refactor */
    int64_t eta_memory;       /**< Memory used by eta vectors (bytes) */
    double total_ftran_time;  /**< Accumulated FTRAN time */
    int ftran_count;          /**< Number of FTRAN operations */
    double baseline_ftran;    /**< Baseline FTRAN time (after refactor) */
    int iteration;            /**< Current iteration number */
    int last_refactor_iter;   /**< Iteration of last refactorization */
};

/*******************************************************************************
 * SolverContext Lifecycle API
 ******************************************************************************/

/**
 * @brief Create and initialize solver context from model.
 * @param model Model to solve
 * @param stateP Output pointer for new context
 * @return CXF_OK on success, error code otherwise
 */
int cxf_simplex_init(CxfModel *model, SolverContext **stateP);

/**
 * @brief Free solver context and all resources.
 * @param state Context to free (may be NULL)
 */
void cxf_simplex_final(SolverContext *state);

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
int cxf_simplex_setup(SolverContext *state, CxfEnv *env);

/**
 * @brief Preprocess the LP problem.
 *
 * Performs preprocessing reductions including fixed variable elimination,
 * bound propagation, and scaling.
 *
 * @param state Solver context
 * @param env Environment
 * @param flags Control flags (bit 0: skip preprocessing if set)
 * @return CXF_OK on success, 3=infeasible detected
 */
int cxf_simplex_preprocess(SolverContext *state, CxfEnv *env, int flags);

/**
 * @brief Get solver status (stub - to be implemented).
 * @param state Solver context
 * @return Status code or error
 */
int cxf_simplex_get_status(SolverContext *state);

/**
 * @brief Get iteration count (stub - to be implemented).
 * @param state Solver context
 * @return Iteration count or error
 */
int cxf_simplex_get_iteration(SolverContext *state);

/**
 * @brief Get solver phase (stub - to be implemented).
 * @param state Solver context
 * @return Phase (0, 1, or 2) or error
 */
int cxf_simplex_get_phase(SolverContext *state);

/**
 * @brief Perform one simplex iteration (stub - to be implemented in M7.1.2).
 * @param state Solver context
 * @param env Environment
 * @return CXF_OK on success, error code otherwise
 */
int cxf_simplex_iterate(SolverContext *state, CxfEnv *env);

/**
 * @brief Handle phase end transition (stub - to be implemented in M7.1.2).
 * @param state Solver context
 * @param env Environment
 * @return CXF_OK on success, error code otherwise
 */
int cxf_simplex_phase_end(SolverContext *state, CxfEnv *env);

/**
 * @brief Post-iteration processing (stub - to be implemented in M7.1.2).
 * @param state Solver context
 * @param env Environment
 * @return CXF_OK on success, error code otherwise
 */
int cxf_simplex_post_iterate(SolverContext *state, CxfEnv *env);

/**
 * @brief Get current objective value (stub - to be implemented).
 * @param state Solver context
 * @return Objective value or NaN on error
 */
double cxf_simplex_get_objval(SolverContext *state);

/**
 * @brief Set iteration limit (stub - to be implemented).
 * @param state Solver context
 * @param limit Iteration limit (must be >= 0)
 * @return CXF_OK on success, error code otherwise
 */
int cxf_simplex_set_iteration_limit(SolverContext *state, int limit);

/**
 * @brief Get iteration limit (stub - to be implemented).
 * @param state Solver context
 * @return Iteration limit or error code
 */
int cxf_simplex_get_iteration_limit(SolverContext *state);

/**
 * @brief Apply perturbation for degeneracy handling (stub - to be implemented in M7.1.3).
 * @param state Solver context
 * @param env Environment
 * @return CXF_OK on success, error code otherwise
 */
int cxf_simplex_perturbation(SolverContext *state, CxfEnv *env);

/**
 * @brief Remove perturbation (stub - to be implemented in M7.1.3).
 * @param state Solver context
 * @param env Environment
 * @return CXF_OK on success, error code otherwise
 */
int cxf_simplex_unperturb(SolverContext *state, CxfEnv *env);

/**
 * @brief Post-solve cleanup to restore original problem space.
 *
 * Unscales values and restores eliminated variables after preprocessing.
 * Current implementation is a minimal stub for future expansion when
 * full preprocessing is implemented.
 *
 * @param state Solver context containing solution arrays
 * @param env Environment
 * @return CXF_OK on success, error code otherwise
 */
int cxf_simplex_cleanup(SolverContext *state, CxfEnv *env);

/**
 * @brief Adjust reduced costs for quadratic programming.
 *
 * Updates reduced costs to include quadratic term contribution (Qx).
 * For convex QP with objective c'x + 0.5*x'Qx, the gradient is c + Qx.
 *
 * Current implementation is a stub since full QP support (Q matrix)
 * is not yet implemented.
 *
 * @param state Solver context with QP data and solution
 * @param varIndex Variable index to adjust (-1 for all nonbasic variables)
 * @return CXF_OK on success, error code otherwise
 */
int cxf_quadratic_adjust(SolverContext *state, int varIndex);

/**
 * @brief Extract solution from solver state to model.
 *
 * Copies primal solution (x), dual values (pi), and objective value
 * from the solver's working arrays to the model's solution arrays.
 * Allocates solution arrays if they are NULL.
 *
 * @param state Solver context with solution (non-NULL)
 * @param model Model to receive solution (non-NULL)
 * @return CXF_OK on success, error code otherwise
 */
int cxf_extract_solution(SolverContext *state, CxfModel *model);

#endif /* CXF_SOLVER_H */
