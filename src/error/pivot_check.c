/**
 * @file pivot_check.c
 * @brief Pivot validation functions (M3.1.7)
 *
 * Functions for validating pivot operations:
 * - cxf_validate_pivot_element: validate pivot element magnitude
 * - cxf_special_check: validate variable for special pivot handling
 *
 * Specs:
 * - docs/specs-v2/specs/modules/data_validation.md (cxf_special_check)
 */

#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_solver.h"
#include <math.h>

/* Flag bits for variable structural flags (V2 solver_state.md) */
#define VARFLAG_UPPER_FINITE   0x04
#define VARFLAG_HAS_QUADRATIC  0x08
#define VARFLAG_RESERVED_MASK  0xFFFFFFB0

/* Threshold values — aligned with CXF_INFINITY (1e100) */
#define NEG_INFINITY_THRESHOLD (-CXF_INFINITY)
#define POS_INFINITY_THRESHOLD (CXF_INFINITY)

/**
 * @brief Check if a pivot element is numerically acceptable.
 *
 * @param pivot_elem The pivot element value
 * @param tolerance Minimum acceptable magnitude
 * @return 1 if valid, 0 if invalid
 */
int cxf_validate_pivot_element(double pivot_elem, double tolerance) {
    if (isnan(pivot_elem)) return 0;
    if (fabs(pivot_elem) < tolerance) return 0;
    return 1;
}

/**
 * @brief Check if a variable qualifies for special pivot handling.
 *
 * V2 spec (data_validation.md): takes (state, varIdx), reads bounds/flags
 * from SolverState arrays.  Multi-stage qualification with early reject:
 *   Stage 1 — lower bound >= -infinity threshold
 *   Stage 2 — no reserved flag bits
 *   Stage 3 — finite-UB flag consistent with LB
 *   Stage 4 — quadratic structure validation (LP-only: reject if set)
 *
 * @param state  Solver runtime state (non-NULL)
 * @param varIdx Variable index in [0, num_vars + num_constrs)
 * @return 1 if qualifies for special pivot, 0 otherwise
 */
int cxf_special_check(SolverState *state, int varIdx) {
    double lb;
    uint32_t flags;

    if (state == NULL) return 0;

    lb    = state->work_lb[varIdx];
    flags = (state->var_flags != NULL) ? state->var_flags[varIdx] : 0;

    /* Stage 1: Lower bound must be finite (>= neg infinity threshold) */
    if (lb < NEG_INFINITY_THRESHOLD) return 0;

    /* Stage 2: Reserved flag bits must not be set */
    if ((flags & VARFLAG_RESERVED_MASK) != 0) return 0;

    /* Stage 3: If finite-UB flag, LB must not exceed +infinity */
    if ((flags & VARFLAG_UPPER_FINITE) != 0) {
        if (lb > POS_INFINITY_THRESHOLD) return 0;
    }

    /* Stage 4: Quadratic structure (LP-only: reject) */
    if ((flags & VARFLAG_HAS_QUADRATIC) != 0) return 0;

    return 1;
}
