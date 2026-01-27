/**
 * @file phase_steps.c
 * @brief Extended pivot operations for primal and dual simplex (M7.1.10)
 *
 * cxf_simplex_step2: Extended primal pivot with bound flip and dual update
 * cxf_simplex_step3: Dual simplex pivot operation
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_types.h"
#include <math.h>

/* External declarations */
extern int cxf_simplex_step(SolverContext *state, int entering, int leavingRow,
                            const double *pivotCol, double stepSize);
extern int cxf_pivot_with_eta(BasisState *basis, int pivotRow,
                              const double *pivotCol, int enteringVar,
                              int leavingVar);

/**
 * @brief Extended primal pivot operation with bound flip and dual update.
 *
 * This function extends cxf_simplex_step by:
 * 1. Checking for bound flip case (entering variable reaches opposite bound)
 * 2. Updating dual values after the pivot
 * 3. Handling primal bound flips without basis change
 *
 * @param state Solver context containing basis, bounds, and current solution
 * @param entering Index of variable entering the basis
 * @param leavingRow Row index of leaving variable (0 <= row < m)
 * @param pivotCol FTRAN result: B^(-1) * a_entering in dense format
 * @param pivotRow BTRAN result: B^(-T) * c_B[leavingRow] in dense format
 * @param stepSize Primal step length (distance to bound)
 * @param dualStepSize Dual step length for dual solution update
 * @return 0 on normal pivot, 1 on bound flip, CXF_ERROR_NULL_ARGUMENT on NULL input
 */
int cxf_simplex_step2(SolverContext *state, int entering, int leavingRow,
                      const double *pivotCol, const double *pivotRow,
                      double stepSize, double dualStepSize) {
    int i, result;
    double lb, ub, range, flipStep;
    int currentStatus;

    /* Validate inputs */
    if (state == NULL || pivotCol == NULL || pivotRow == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    if (state->basis == NULL) {
        return CXF_ERROR_INVALID_ARGUMENT;
    }

    /* Get bounds for entering variable */
    lb = state->work_lb[entering];
    ub = state->work_ub[entering];
    range = ub - lb;

    /* Check for bound flip: can entering variable reach opposite bound
     * before ratio test limit? */
    currentStatus = state->basis->var_status[entering];

    /* If variable is at a bound and has a finite opposite bound */
    if ((currentStatus == -1 || currentStatus == -2) &&
        range < CXF_INFINITY && range > 0.0) {

        /* Calculate step to opposite bound */
        flipStep = range;

        /* If flip is possible before ratio test limit, perform bound flip */
        if (flipStep < stepSize) {
            /* Bound flip: move to opposite bound without basis change */
            if (currentStatus == -1) {
                /* Currently at lower bound, flip to upper */
                state->work_x[entering] = ub;
                state->basis->var_status[entering] = -2;
            } else {
                /* Currently at upper bound, flip to lower */
                state->work_x[entering] = lb;
                state->basis->var_status[entering] = -1;
            }

            /* Update objective value: obj += reduced_cost * step */
            if (state->work_dj != NULL) {
                state->obj_value += state->work_dj[entering] * flipStep;
            }

            /* Return 1 to indicate bound flip occurred */
            return 1;
        }
    }

    /* No bound flip: perform standard pivot */
    result = cxf_simplex_step(state, entering, leavingRow, pivotCol, stepSize);
    if (result != CXF_OK) {
        return result;
    }

    /* Update dual values: y_new = y_old + dualStepSize * pivotRow */
    if (state->work_pi != NULL) {
        for (i = 0; i < state->num_constrs; i++) {
            state->work_pi[i] += dualStepSize * pivotRow[i];
        }
    }

    return 0;
}

/**
 * @brief Dual simplex pivot operation.
 *
 * In dual simplex, the leaving variable is chosen first (by dual feasibility
 * violation), then the entering variable is selected via dual ratio test.
 * This function:
 * 1. Validates the pivot element
 * 2. Updates dual values
 * 3. Creates eta vector for basis update
 * 4. Updates basis header and variable status
 *
 * @param state Solver context containing basis and dual values
 * @param leavingRow Row index of leaving variable
 * @param entering Index of entering variable (from dual ratio test)
 * @param pivotCol FTRAN result: B^(-1) * a_entering
 * @param pivotRow BTRAN result: B^(-T) * c_B[leavingRow]
 * @param dualStepSize Dual step length
 * @return CXF_OK on success, CXF_ERROR_NULL_ARGUMENT on NULL input, -1 on pivot too small
 */
int cxf_simplex_step3(SolverContext *state, int leavingRow, int entering,
                      const double *pivotCol, const double *pivotRow,
                      double dualStepSize) {
    int i, leaving, result;
    double pivot;

    /* Validate inputs */
    if (state == NULL || pivotCol == NULL || pivotRow == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    if (state->basis == NULL) {
        return CXF_ERROR_INVALID_ARGUMENT;
    }

    /* Get leaving variable from basis header */
    leaving = state->basis->basic_vars[leavingRow];

    /* Get pivot element and validate magnitude */
    pivot = pivotCol[leavingRow];
    if (fabs(pivot) < CXF_PIVOT_TOL) {
        /* Pivot too small, indicates numerical instability */
        return -1;
    }

    /* Update dual values: y_new = y_old + dualStepSize * pivotRow */
    if (state->work_pi != NULL) {
        for (i = 0; i < state->num_constrs; i++) {
            state->work_pi[i] += dualStepSize * pivotRow[i];
        }
    }

    /* Create eta vector and update basis state
     * This handles:
     * - Creating the eta vector
     * - Updating basis->basic_vars[leavingRow] = entering
     * - Updating basis->var_status[entering] = leavingRow (basic)
     * - Updating basis->var_status[leaving] to nonbasic status
     */
    result = cxf_pivot_with_eta(state->basis, leavingRow, pivotCol,
                                entering, leaving);

    return result;
}
