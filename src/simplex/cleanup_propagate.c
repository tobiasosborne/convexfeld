/**
 * @file cleanup_propagate.c
 * @brief Post-solve implied-bound propagation and variable fixing.
 *
 * Phases 3, 4, 8 of cxf_simplex_cleanup per bound_propagation.md §6.
 * Single-pass constraint scan computes implied bounds for nonbasic
 * variables using activity arrays (Savelsbergh 1994). Variables
 * whose implied bounds converge within feas_tol are fixed.
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_types.h"
#include <math.h>
#include <stdlib.h>

#include "simplex_internal.h"

#define CLEANUP_TINY  1e-40

int cxf_cleanup_propagate(SolverState *state, CxfEnv *env) {
    if (!state || !env) return CXF_OK;
    if (!state->basis || !state->basis->var_status) return CXF_OK;
    if (!state->csr_row_ptr || !state->csr_col_idx || !state->csr_values)
        return CXF_OK;
    if (!state->min_activity || !state->max_activity || !state->work_rhs)
        return CXF_OK;

    int n = state->num_vars;
    int m = state->num_constrs;
    int total = n + m;
    double ftol = env->feasibility_tol;
    int *vs = state->basis->var_status;

    double *best_lb = (double *)malloc((size_t)total * sizeof(double));
    double *best_ub = (double *)malloc((size_t)total * sizeof(double));
    if (!best_lb || !best_ub) {
        free(best_lb); free(best_ub);
        return CXF_ERROR_OUT_OF_MEMORY;
    }
    for (int j = 0; j < total; j++) {
        best_lb[j] = state->work_lb[j];
        best_ub[j] = state->work_ub[j];
    }

    int *neg_c = state->negUnbdCount;
    int *pos_c = state->posUnbdCount;

    /* Phase 4: Implied bound computation — single pass over CSR rows.
     *
     * For <= constraint with a > 0:
     *   implied_ub = lb + (rhs - min_act) / a   [Savelsbergh 1994]
     * For <= constraint with a < 0:
     *   implied_lb = ub + (rhs - max_act) / a
     * For >= constraint: roles swap (ub from max_act, lb from min_act).
     * For = constraint: both directions apply.
     *
     * Valid when unbounded count is 0, or 1 and this var is the sole
     * unbounded contributor in that direction. */
    for (int i = 0; i < m; i++) {
        double rhs = state->work_rhs[i];
        char sense = state->work_sense ? state->work_sense[i] : '<';
        double min_a = state->min_activity[i];
        double max_a = state->max_activity[i];
        int nc = neg_c ? neg_c[i] : 0;
        int pc = pos_c ? pos_c[i] : 0;
        int leq = (sense == '<' || sense == 'L' || sense == '=' || sense == 'E');
        int geq = (sense == '>' || sense == 'G' || sense == '=' || sense == 'E');

        int64_t rs = state->csr_row_ptr[i];
        int64_t re = state->csr_row_ptr[i + 1];
        for (int64_t k = rs; k < re; k++) {
            int j = state->csr_col_idx[k];
            if (j < 0 || j >= total) continue;
            if (vs[j] >= 0 || vs[j] == CXF_VAR_FIXED) continue;

            double a = state->csr_values[k];
            if (fabs(a) < CLEANUP_TINY) continue;
            double lb = state->work_lb[j];
            double ub = state->work_ub[j];

            /* Implied UB: leq+a>0 or geq+a<0 */
            if ((leq && a > 0.0) || (geq && a < 0.0)) {
                /* Sole-unbounded check: j contributes to neg direction? */
                int sole = (a > 0.0) ? (lb <= -CXF_INFINITY)
                                     : (ub >= CXF_INFINITY);
                if (nc == 0 || (nc == 1 && sole)) {
                    double imp = lb + (rhs - min_a) / a;
                    if (imp < best_ub[j]) best_ub[j] = imp;
                }
            }

            /* Implied LB: leq+a<0 or geq+a>0 */
            if ((leq && a < 0.0) || (geq && a > 0.0)) {
                int sole = (a > 0.0) ? (ub >= CXF_INFINITY)
                                     : (lb <= -CXF_INFINITY);
                if (pc == 0 || (pc == 1 && sole)) {
                    double imp = ub + (rhs - max_a) / a;
                    if (imp > best_lb[j]) best_lb[j] = imp;
                }
            }
        }
    }

    /* Phase 8: Fix variables whose implied bounds converge */
    int fixed = 0;
    for (int j = 0; j < total; j++) {
        if (vs[j] >= 0 || vs[j] == CXF_VAR_FIXED) continue;
        double lb = state->work_lb[j];
        double ub = state->work_ub[j];

        if (best_ub[j] - lb < ftol && best_ub[j] < ub - ftol) {
            int rc = cxf_pivot_bound(env, state, j, lb, ub, 0);
            if (rc != CXF_OK) { free(best_lb); free(best_ub); return rc; }
            state->bounds_propagated++;
            fixed++;
        } else if (ub - best_lb[j] < ftol && best_lb[j] > lb + ftol) {
            int rc = cxf_pivot_bound(env, state, j, ub, ub, 0);
            if (rc != CXF_OK) { free(best_lb); free(best_ub); return rc; }
            state->bounds_propagated++;
            fixed++;
        }
    }

    if (fixed > 0)
        cxf_compute_activity_bounds(state, 0, NULL);

    free(best_lb);
    free(best_ub);
    return CXF_OK;
}
