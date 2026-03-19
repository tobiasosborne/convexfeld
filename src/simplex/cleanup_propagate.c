/**
 * @file cleanup_propagate.c
 * @brief Iterative 2-pass implied-bound propagation (T2.8).
 * Phases 3/4/7/8: classify, Savelsbergh bounds, Kahan activity,
 * conservative+aggressive fixing. Iterates up to K_MAX passes.
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_basis.h"
#include "convexfeld/cxf_types.h"
#include <math.h>
#include <stdlib.h>

#include "simplex_internal.h"

#define CLEANUP_TINY  1e-40
#define K_MAX         10

#define VCLASS_SKIP      0
#define VCLASS_BOUNDED   1
#define VCLASS_SEMIFREE  2
#define VCLASS_FREE      3

/* cleanup_kahan.c */
void cxf_kahan_row_activity(int64_t, int64_t, const int *, const double *,
    const double *, const double *, double *, double *, int *, int *, int);

static int classify_var(int status, double lb, double ub) {
    if (status >= 0 || status == CXF_VAR_FIXED) return VCLASS_SKIP;
    int lb_inf = (lb <= -CXF_INFINITY);
    int ub_inf = (ub >= CXF_INFINITY);
    if (lb_inf && ub_inf) return VCLASS_FREE;
    if (lb_inf || ub_inf) return VCLASS_SEMIFREE;
    return VCLASS_BOUNDED;
}

/* Phase 4: Savelsbergh implied bounds via CSR row scan. */
static void compute_implied_bounds(
    SolverState *state, int total, double *best_lb, double *best_ub) {
    int m = state->num_constrs;
    int *vs = state->basis->var_status;
    int *neg_c = state->negUnbdCount;
    int *pos_c = state->posUnbdCount;

    for (int i = 0; i < m; i++) {
        double rhs = state->work_rhs[i];
        char sense = state->work_sense ? state->work_sense[i] : '<';
        double min_a = state->min_activity[i];
        double max_a = state->max_activity[i];
        int nc = neg_c ? neg_c[i] : 0;
        int pc = pos_c ? pos_c[i] : 0;
        int leq = (sense == '<' || sense == 'L' ||
                   sense == '=' || sense == 'E');
        int geq = (sense == '>' || sense == 'G' ||
                   sense == '=' || sense == 'E');

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
                double act = (a > 0.0) ? min_a : max_a;
                int cnt = (a > 0.0) ? nc : pc;
                int sole = (a > 0.0) ? (lb <= -CXF_INFINITY)
                                     : (ub >= CXF_INFINITY);
                if (cnt == 0 || (cnt == 1 && sole)) {
                    double imp = lb + (rhs - act) / a;
                    if (imp < best_ub[j]) best_ub[j] = imp;
                }
            }

            /* Implied LB: leq+a<0 or geq+a>0 */
            if ((leq && a < 0.0) || (geq && a > 0.0)) {
                double act = (a > 0.0) ? max_a : min_a;
                int cnt = (a > 0.0) ? pc : nc;
                int sole = (a > 0.0) ? (ub >= CXF_INFINITY)
                                     : (lb <= -CXF_INFINITY);
                if (cnt == 0 || (cnt == 1 && sole)) {
                    double imp = ub + (rhs - act) / a;
                    if (imp > best_lb[j]) best_lb[j] = imp;
                }
            }
        }
    }
}

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

    int *vclass = (int *)malloc((size_t)total * sizeof(int));
    double *best_lb = (double *)malloc((size_t)total * sizeof(double));
    double *best_ub = (double *)malloc((size_t)total * sizeof(double));
    if (!vclass || !best_lb || !best_ub) {
        free(vclass); free(best_lb); free(best_ub);
        return CXF_ERROR_OUT_OF_MEMORY;
    }

    for (int pass = 0; pass < K_MAX; pass++) {
        /* Phase 7: Kahan activity recompute (clean baseline each pass) */
        for (int i = 0; i < m; i++) {
            cxf_kahan_row_activity(
                state->csr_row_ptr[i], state->csr_row_ptr[i + 1],
                state->csr_col_idx, state->csr_values,
                state->work_lb, state->work_ub,
                &state->min_activity[i], &state->max_activity[i],
                state->negUnbdCount ? &state->negUnbdCount[i] : NULL,
                state->posUnbdCount ? &state->posUnbdCount[i] : NULL,
                total);
        }

        /* Phase 3: Classify + reset implied bounds to current */
        for (int j = 0; j < total; j++) {
            vclass[j] = classify_var(vs[j], state->work_lb[j],
                                     state->work_ub[j]);
            best_lb[j] = state->work_lb[j];
            best_ub[j] = state->work_ub[j];
        }

        /* Phase 4: Compute implied bounds */
        compute_implied_bounds(state, total, best_lb, best_ub);

        /* Phase 8 Pass 1 (conservative): both bounds converge */
        int fixed_this_pass = 0;
        for (int j = 0; j < total; j++) {
            if (vclass[j] == VCLASS_SKIP) continue;
            double gap = best_ub[j] - best_lb[j];
            if (gap >= ftol) continue;
            double fix_val = 0.5 * (best_lb[j] + best_ub[j]);
            int rc = cxf_pivot_bound(env, state, j, fix_val,
                                     state->work_ub[j], 0);
            if (rc != CXF_OK) {
                free(vclass); free(best_lb); free(best_ub);
                return rc;
            }
            state->bounds_propagated++;
            vclass[j] = VCLASS_SKIP;
            fixed_this_pass++;
        }

        /* Phase 8 Pass 2 (aggressive): free/semi-free vars */
        for (int j = 0; j < total; j++) {
            if (vclass[j] != VCLASS_SEMIFREE && vclass[j] != VCLASS_FREE)
                continue;
            double lb = state->work_lb[j];
            double ub = state->work_ub[j];
            int has_ilb = (best_lb[j] > -CXF_INFINITY);
            int has_iub = (best_ub[j] < CXF_INFINITY);
            double fix_val;
            int do_fix = 0;
            if (vclass[j] == VCLASS_FREE && has_ilb && has_iub) {
                fix_val = 0.5 * (best_lb[j] + best_ub[j]);
                do_fix = 1;
            } else if (has_iub && lb > -CXF_INFINITY &&
                       best_ub[j] - lb < ftol) {
                fix_val = lb; do_fix = 1;
            } else if (has_ilb && ub < CXF_INFINITY &&
                       ub - best_lb[j] < ftol) {
                fix_val = ub; do_fix = 1;
            }
            if (do_fix) {
                int rc = cxf_pivot_bound(env, state, j, fix_val,
                                         state->work_ub[j], 0);
                if (rc != CXF_OK) {
                    free(vclass); free(best_lb); free(best_ub);
                    return rc;
                }
                state->bounds_propagated++;
                fixed_this_pass++;
            }
        }

        if (fixed_this_pass == 0) break;
    }

    free(vclass);
    free(best_lb);
    free(best_ub);
    return CXF_OK;
}
