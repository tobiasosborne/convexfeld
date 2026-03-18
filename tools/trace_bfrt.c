/**
 * @file trace_bfrt.c
 * @brief Trace BFRT flip effects on bound violations per iteration.
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "convexfeld/convexfeld.h"
#include "convexfeld/cxf_mps.h"

extern int cxf_simplex_init(CxfModel *model, SolverState **stateP);
extern void cxf_state_free(SolverState *state);
extern int cxf_simplex_crash(SolverState *state, CxfEnv *env);
extern int cxf_setup_phase_one(SolverState *state);
extern int cxf_compute_reduced_costs(SolverState *state);
extern void cxf_simplex_setup(SolverState *state, CxfEnv *env, int c, int *i);
extern int cxf_simplex_preprocess(SolverState *state, CxfEnv *env, int f);
extern int cxf_simplex_step(SolverState *state, CxfEnv *env);
extern int cxf_simplex_perturbation(SolverState *state, CxfEnv *env);
extern void cxf_progress_snapshot(SolverState *state, int *snapshot);

static int count_bound_violations(const SolverState *state, double *worst) {
    int n = state->num_vars, m = state->num_constrs, total = n + m;
    int count = 0; *worst = 0;
    for (int i = 0; i < m; i++) {
        int bv = state->basis->basic_vars[i];
        if (bv < 0 || bv >= total) continue;
        double x = state->work_x[bv];
        double lb = state->work_lb[bv], ub = state->work_ub[bv];
        if (x < lb - 1e-6) { count++; double v = lb-x; if (v > *worst) *worst = v; }
        if (x > ub + 1e-6) { count++; double v = x-ub; if (v > *worst) *worst = v; }
    }
    return count;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "Usage: %s <mps>\n", argv[0]); return 1; }
    CxfEnv *env = NULL; CxfModel *model = NULL;
    cxf_loadenv(&env, NULL);
    cxf_newmodel(env, &model, "t", 0, NULL, NULL, NULL, NULL, NULL);
    cxf_readmps(model, argv[1]);
    SolverState *state = NULL;
    cxf_simplex_init(model, &state);
    cxf_simplex_crash(state, env);
    cxf_setup_phase_one(state);
    cxf_compute_reduced_costs(state);
    cxf_simplex_setup(state, env, 0, NULL);
    cxf_simplex_preprocess(state, env, 0);
    {
        int snap_buf[CXF_SNAPSHOT_SIZE];
        cxf_progress_snapshot(state, snap_buf);
    }

    int prev_flips = 0;
    for (int iter = 0; iter < 200; iter++) {
        if (iter <= 2) cxf_simplex_perturbation(state, env);

        /* Check bounds BEFORE step */
        double worst_before = 0;
        int viol_before = count_bound_violations(state, &worst_before);

        int rc = cxf_simplex_step(state, env);

        /* Check bounds AFTER step */
        double worst_after = 0;
        int viol_after = count_bound_violations(state, &worst_after);

        int new_flips = state->flip_count - prev_flips;
        prev_flips = state->flip_count;

        /* Only print if bound violations changed or new flips */
        if (viol_after != viol_before || new_flips > 0 || rc != 0) {
            fprintf(stderr, "ITER %d: phase=%d obj=%.4e status=%d "
                    "flips_this_iter=%d viol=%d->%d worst=%.2e->%.2e\n",
                    iter, state->phase, state->obj_value, rc,
                    new_flips, viol_before, viol_after,
                    worst_before, worst_after);

            /* If new violations appeared, show which variables */
            if (viol_after > viol_before) {
                int n = state->num_vars, m = state->num_constrs, total = n+m;
                int shown = 0;
                for (int i = 0; i < m && shown < 5; i++) {
                    int bv = state->basis->basic_vars[i];
                    if (bv < 0 || bv >= total) continue;
                    double x = state->work_x[bv];
                    double ub = state->work_ub[bv];
                    double lb = state->work_lb[bv];
                    if (x > ub + 1e-6) {
                        fprintf(stderr, "  past_ub: row=%d bv=%d x=%.4e ub=%.4e "
                                "excess=%.4e\n", i, bv, x, ub, x-ub);
                        shown++;
                    } else if (x < lb - 1e-6) {
                        fprintf(stderr, "  past_lb: row=%d bv=%d x=%.4e lb=%.4e "
                                "excess=%.4e\n", i, bv, x, lb, lb-x);
                        shown++;
                    }
                }
            }
        }

        if (rc == 1 && state->phase == 1) {
            extern int cxf_check_phase_one_end(SolverState*, CxfModel*, CxfEnv*);
            cxf_check_phase_one_end(state, model, env);
            continue;
        }
        if (rc != 0) break;
    }
    cxf_state_free(state); cxf_freemodel(model); cxf_freeenv(env);
    return 0;
}
