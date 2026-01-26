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
};

#endif /* CXF_SOLVER_H */
