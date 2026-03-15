/**
 * @file cxf_solver.h
 * @brief SolverState structure - runtime solver state.
 *
 * The solver context holds all working state during LP optimization.
 * It isolates mutable solver data from the immutable model definition.
 */

#ifndef CXF_SOLVER_H
#define CXF_SOLVER_H

#include "cxf_types.h"
#include "cxf_timing.h"

/**
 * @brief Solution data container (P3.2 — per work_arrays.md).
 *
 * Model-level container for optimization results, iteration statistics,
 * adaptive thresholds, and anti-cycling history.
 */
typedef struct SolutionData {
    /* Iteration statistics */
    int total_iterations;     /**< Total simplex iterations */
    int phase1_iterations;    /**< Phase I iterations */
    int phase2_iterations;    /**< Phase II iterations */
    int refactor_count;       /**< Number of refactorizations */

    /* Objective tracking */
    double best_obj;          /**< Best objective found */
    double obj_bound;         /**< Objective bound (for gap computation) */

    /* Anti-cycling history */
    int prev_entering;        /**< Previous entering variable index */
    int prev_leaving;         /**< Previous leaving variable index */
    int prev_pivot_row;       /**< Previous pivot row */
    int cycle_detect_flag;    /**< 1 if cycling suspected */

    /* Adaptive thresholds (populated during solve) */
    double thresholds[6];     /**< [pricing, ratio, pivot, degen, stall, refact] */
} SolutionData;

/**
 * @brief Solver context for LP optimization.
 *
 * Contains problem data copies, algorithmic state, and working arrays.
 * Created at solve start, destroyed after completion.
 */
struct SolverState {
    CxfModel *model_ref;      /**< Back-pointer to model */

    /* Problem dimensions */
    int num_vars;             /**< Number of original variables */
    int num_constrs;          /**< Number of constraints */
    int num_artificials;      /**< Number of artificial variables (for Phase I) */
    int64_t num_nonzeros;     /**< Number of non-zeros */

    /* Solver state */
    int phase;                /**< 0=setup, 1=phase I, 2=phase II */
    int solve_mode;           /**< 0=primal, 1=dual, 2=barrier */
    int max_iterations;       /**< Iteration limit */
    double tolerance;         /**< Optimality tolerance */
    double obj_value;         /**< Current objective value */

    /* Working arrays — sized for num_vars + num_constrs (structural + slack).
     * Phase I uses implicit bound violations (no artificial variable slots). */
    double *work_lb;          /**< Working lower bounds [num_vars + num_constrs] */
    double *work_ub;          /**< Working upper bounds [num_vars + num_constrs] */
    double *work_obj;         /**< Working objective / Phase I w-coefficients [num_vars + num_constrs] */
    double *work_x;           /**< Current solution [num_vars + num_constrs] */
    double *work_pi;          /**< Dual values [num_constrs] */
    double *work_dj;          /**< Reduced costs [num_vars + num_constrs] */

    /* Subcomponents */
    BasisState *basis;        /**< Current basis state */
    PricingState *pricing;  /**< Pricing context */

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
    double last_log_time;     /**< Timestamp of last log message (throttling) */

    /* Preallocated iteration work arrays (size num_constrs)
     * Allocated once in init, reused across iterations to avoid malloc/free */
    double *work_column;      /**< Column extraction buffer [num_constrs] */
    double *work_cB;          /**< Basic variable costs [num_constrs] */

    /* Anti-cycling */
    int use_bland;            /**< If nonzero, use Bland's rule for pricing */
    int degenerate_count;     /**< Consecutive degenerate pivots (stepSize=0) */

    /* P3.3: Missing SolverState fields per solver_state.md */
    int num_slacks;           /**< Number of slack variables (= num_constrs) */
    int solve_mode_alt;       /**< Alternative solve mode (0=none, 1=crossover) */
    int init_mode;            /**< Initialization mode (0=cold, 1=warm, 2=hot) */
    int sol_status;           /**< Solution status (independent of model->status) */
    double thresholds[6];     /**< Adaptive thresholds [pricing, ratio, pivot,
                                   degeneracy, stall, refactor] */

    /* B1: Saved (unperturbed) bounds (v2 — P2.6 EXPAND) */
    double *saved_lb;         /**< Original lower bounds [num_vars + num_constrs] */
    double *saved_ub;         /**< Original upper bounds [num_vars + num_constrs] */

    /* Perturbation state (v2 — P2.6 EXPAND) */
    int perturb_count;        /**< Cumulative count of variables perturbed/removed */
    int perturb_expand_active; /**< Nonzero if EXPAND bound widening (Mechanism B) is active */
    int mechanism_a_applied;  /**< Nonzero if Mechanism A (pricing restriction) was applied
                                   in this stalling episode. Mechanism B only fires after A
                                   has been tried (perturbation.md lines 224-225). */

    /* B2: Activity bounds (v2 — P3.21 cxf_simplex_setup) */
    double *min_activity;     /**< Per-constraint min activity [num_constrs] */
    double *max_activity;     /**< Per-constraint max activity [num_constrs] */
    int *negUnbdCount;        /**< Per-constraint -inf contribution count [num_constrs] */
    int *posUnbdCount;        /**< Per-constraint +inf contribution count [num_constrs] */

    /* B3: Progress tracking (v2 — P3.20, P3.16, P3.25) */
#define CXF_SNAPSHOT_SIZE 10
    int progress_snapshot[CXF_SNAPSHOT_SIZE]; /**< Snapshot buffer for basis diff */
    double obj_at_last_refactor; /**< Objective at last refactor (stagnation) */
    int iteration_mode;       /**< Strategy flag (0=normal, 1=stagnation) */
    int rows_eliminated;      /**< Row elimination counter */
    int cols_eliminated;      /**< Column elimination counter */
    int bounds_propagated;    /**< Bounds propagation counter */
    int flip_count;           /**< Bound flip counter */

    /* Crash basis state (v2 simplex phases — P2.5) */
    int *row_status;          /**< Per-row crash status [num_constrs]:
                                   0 = unassigned, >0 = candidate,
                                   -1 = BASIC_LOWER, -2 = BASIC_UPPER */
    int *col_nz_count;        /**< Active column nonzero counts [num_vars + num_constrs] */
    int num_basic;            /**< Count of basic rows assigned by crash */
    int problem_row_index;    /**< Diagnostic: constraint index causing infeasibility */

    /* P3.1: Working copies of constraint matrix (owned by SolverState).
     * Prevents mutation of model->matrix during solving (e.g. BFRT). */
    int64_t *csc_col_ptr;     /**< CSC column pointers [num_vars + 1] */
    int *csc_row_idx;         /**< CSC row indices [nnz] */
    double *csc_values;       /**< CSC coefficient values [nnz] */
    int64_t *csr_row_ptr;     /**< CSR row pointers [num_constrs + 1] (NULL if N/A) */
    int *csr_col_idx;         /**< CSR column indices [nnz] (NULL if N/A) */
    double *csr_values;       /**< CSR coefficient values [nnz] (NULL if N/A) */
    double *work_rhs;         /**< RHS working copy [num_constrs] */
    char *work_sense;         /**< Constraint senses working copy [num_constrs] */

    /* Per-variable type flags (V2 solver_state.md Variable Flags).
     * Bitmask of structural properties for special pivot routing.
     * Set during init; read-only during simplex iterations. */
    uint32_t *var_flags;      /**< Variable structural flags [num_vars + num_constrs] */

    /* Row/column scaling factors (NULL if no scaling applied).
     * Per matrix_finalization.md Strategy 3 (Ruiz equilibration). */
    double *row_scale;        /**< Row scaling D_r [num_constrs] (NULL = unscaled) */
    double *col_scale;        /**< Column scaling D_c [num_vars] (NULL = unscaled) */

    /* (art_coeff removed — Phase I now uses implicit bound-violation approach
     * per two_phase_method.md spec. No explicit artificial variables.) */
};

/*******************************************************************************
 * SolverState Lifecycle API
 ******************************************************************************/

/**
 * @brief Create and initialize solver context from model.
 * @param model Model to solve
 * @param stateP Output pointer for new context
 * @return CXF_OK on success, error code otherwise
 */
int cxf_simplex_init(CxfModel *model, SolverState **stateP);

/**
 * @brief Free solver context and all resources.
 * @param state Context to free (may be NULL)
 */
void cxf_simplex_final(SolverState *state);

/**
 * @brief Set up solver context for iteration.
 *
 * Initializes reduced costs, dual values, pricing, and determines
 * initial phase based on bound feasibility.
 *
 * @param state   Solver state with CSC matrix and bounds
 * @param env     Environment with tolerances
 * @param count   Number of constraints to update (0 = all)
 * @param indices Constraint indices to update (NULL = all)
 */
void cxf_simplex_setup(SolverState *state, CxfEnv *env,
                       int count, int *indices);

/**
 * @brief Preprocess the LP problem.
 *
 * Performs preprocessing reductions including fixed variable elimination,
 * bound propagation, and scaling.
 *
 * @param state Solver context
 * @param env Environment
 * @return CXF_OK on success, 3=infeasible detected
 */
int cxf_simplex_preprocess(SolverState *state, CxfEnv *env);

/**
 * @brief Get solver status (stub - to be implemented).
 * @param state Solver context
 * @return Status code or error
 */
int cxf_simplex_get_status(SolverState *state);

/**
 * @brief Get iteration count (stub - to be implemented).
 * @param state Solver context
 * @return Iteration count or error
 */
int cxf_simplex_get_iteration(SolverState *state);

/**
 * @brief Get solver phase (stub - to be implemented).
 * @param state Solver context
 * @return Phase (0, 1, or 2) or error
 */
int cxf_simplex_get_phase(SolverState *state);

/**
 * @brief Execute one simplex iteration (v2 P3.20).
 * Pricing, FTRAN, ratio test, pivot, reduced cost update.
 * @param state Solver context
 * @param env Environment
 * @return ITERATE_CONTINUE, ITERATE_OPTIMAL, ITERATE_UNBOUNDED, or error
 */
int cxf_simplex_step(SolverState *state, CxfEnv *env);

/**
 * @brief Report iteration progress (v2 P3.20 — logging only).
 * @param model Model with logging config
 * @param state Solver state with timing data
 */
void cxf_simplex_iterate(CxfModel *model, SolverState *state);

/**
 * @brief Phase-end processing: constraint cleanup + Phase I→II (v2 P3.21).
 * @param state   Solver context
 * @param env     Environment with tolerances
 * @param doScan  Enable small-contribution variable scanning
 * @return CXF_OK on success, CXF_INFEASIBLE on dual infeasibility
 */
int cxf_simplex_phase_end(SolverState *state, CxfEnv *env, int doScan);

/**
 * @brief Post-iteration monitoring: stall, stagnation, limits (v2 P3.20).
 * @param state    Solver context
 * @param env      Environment
 * @param outStall Output: 1 if stalling detected, 0 otherwise (may be NULL)
 * @return 0 to continue, termination code to stop
 */
int cxf_simplex_post_iterate(SolverState *state, CxfEnv *env,
                             int *outStall);

/**
 * @brief Get current objective value (stub - to be implemented).
 * @param state Solver context
 * @return Objective value or NaN on error
 */
double cxf_simplex_get_objval(SolverState *state);

/**
 * @brief Set iteration limit (stub - to be implemented).
 * @param state Solver context
 * @param limit Iteration limit (must be >= 0)
 * @return CXF_OK on success, error code otherwise
 */
int cxf_simplex_set_iteration_limit(SolverState *state, int limit);

/**
 * @brief Get iteration limit (stub - to be implemented).
 * @param state Solver context
 * @return Iteration limit or error code
 */
int cxf_simplex_get_iteration_limit(SolverState *state);

/**
 * @brief Apply perturbation for degeneracy handling (stub - to be implemented in M7.1.3).
 * @param state Solver context
 * @param env Environment
 * @return CXF_OK on success, error code otherwise
 */
int cxf_simplex_perturbation(SolverState *state, CxfEnv *env);

/**
 * @brief Remove perturbation (stub - to be implemented in M7.1.3).
 * @param state Solver context
 * @param env Environment
 * @return CXF_OK on success, error code otherwise
 */
int cxf_simplex_unperturb(SolverState *state, CxfEnv *env);

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
int cxf_simplex_postsolve(SolverState *state, CxfEnv *env);

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
int cxf_quadratic_adjust(SolverState *state, int varIndex);

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
int cxf_extract_solution(SolverState *state, CxfModel *model);

#endif /* CXF_SOLVER_H */
