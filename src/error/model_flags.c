/**
 * @file model_flags.c
 * @brief Model state query functions (V2 model_type_checking.md)
 *
 * Pure query functions inspecting solver state:
 * - cxf_check_model_flags1: Active optimization state detection
 * - cxf_check_model_flags2: Dual solution data availability
 */

#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_basis.h"

/**
 * @brief Check if model has active optimization state.
 *
 * Returns 1 if:
 * (1) The model has a non-null concurrent solve state (self_ptr), OR
 * (2) The model has an active flag (modification_blocked > 0)
 *     AND a non-null basis factorization pointer (solution_data).
 *
 * Used to determine whether model modifications are permitted or
 * whether warm-start data is available from a previous solve.
 *
 * V2 spec: model_type_checking.md, cxf_check_model_flags1
 *
 * @param model Model to check (may be NULL)
 * @return 1 if active state exists, 0 otherwise
 */
int cxf_check_model_flags1(CxfModel *model) {
    if (model == NULL) {
        return 0;
    }

    /* Condition 1: non-null concurrent solve state */
    if (model->self_ptr != NULL) {
        return 1;
    }

    /* Condition 2: active flag > 0 AND non-null basis factorization */
    if (model->modification_blocked > 0 && model->solution_data != NULL) {
        return 1;
    }

    return 0;
}

/**
 * @brief Check if model has available dual solution data.
 *
 * Returns 1 if ALL of the following are met:
 * (1) The model has a non-null solve indicator (self_ptr),
 * (2) The model has non-null dual data (pi),
 * (3) The status indicates optimal, infeasible, or inf-or-unbounded,
 * (4) The active flag (modification_blocked) is positive.
 *
 * These are the solve outcomes for which the LP solver computes valid
 * dual values as part of the optimality or infeasibility certificate.
 *
 * V2 spec: model_type_checking.md, cxf_check_model_flags2
 *
 * @param model Model to check (may be NULL)
 * @return 1 if dual data is available, 0 otherwise
 */
int cxf_check_model_flags2(CxfModel *model) {
    if (model == NULL) {
        return 0;
    }

    /* Condition 1: non-null SolverState indicator */
    if (model->self_ptr == NULL) {
        return 0;
    }

    /* Condition 2: non-null dual data */
    if (model->pi == NULL) {
        return 0;
    }

    /* Condition 3: status indicates dual-producing outcome */
    int st = model->status;
    if (st != CXF_OPTIMAL && st != CXF_INFEASIBLE && st != CXF_INF_OR_UNBD) {
        return 0;
    }

    /* Condition 4: active flag is positive */
    if (model->modification_blocked <= 0) {
        return 0;
    }

    return 1;
}
