/**
 * @file solve_lp.c
 * @brief Main LP solver entry point — orchestrator (M7.1.4)
 *
 * Coordinates the simplex solve sequence: validation, presolve,
 * initialization, Phase I, Phase II, and post-processing.
 * Implementation details are in presolve.c, phase_one.c,
 * reduced_costs.c, and phase_loop.c.
 *
 * Spec: docs/specs/functions/simplex/cxf_solve_lp.md
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_matrix.h"
#include "convexfeld/cxf_types.h"

/* External declarations — lifecycle and post-processing */
extern int cxf_simplex_init(CxfModel *model, SolverState **stateP);
extern void cxf_simplex_final(SolverState *state);
extern int cxf_extract_solution(SolverState *state, CxfModel *model);
extern int cxf_simplex_perturbation(SolverState *state, CxfEnv *env);
extern int cxf_simplex_unperturb(SolverState *state, CxfEnv *env);
extern int cxf_simplex_refine(SolverState *state, CxfEnv *env);

/* External declarations — decomposed solve_lp components */
extern int cxf_check_obvious_infeasibility(CxfModel *model);
extern int cxf_check_obvious_unboundedness(CxfModel *model);
extern int cxf_solve_unconstrained(CxfModel *model);
extern int cxf_setup_phase_one(SolverState *state);
extern int cxf_transition_to_phase_two(SolverState *state, CxfModel *model);
extern void cxf_compute_reduced_costs(SolverState *state);
extern int cxf_run_phase_one(SolverState *state, CxfModel *model, CxfEnv *env);
extern int cxf_run_phase_two(SolverState *state, CxfModel *model, CxfEnv *env);

int cxf_solve_lp(CxfModel *model) {
    SolverState *state = NULL;
    int rc;

    /* Validation and special cases */
    if (model == NULL) return CXF_ERROR_NULL_ARGUMENT;
    if (model->num_vars == 0) {
        model->obj_val = 0.0;
        model->status = CXF_OPTIMAL;
        return CXF_OPTIMAL;
    }
    if (model->num_constrs == 0) return cxf_solve_unconstrained(model);

    CxfEnv *env = model->env;
    if (env == NULL) return CXF_ERROR_NULL_ARGUMENT;
    if (model->matrix == NULL || model->matrix->col_ptr == NULL) {
        model->status = CXF_ERROR_NOT_SUPPORTED;
        return CXF_ERROR_NOT_SUPPORTED;
    }

    /* Presolve: detect obvious infeasibility/unboundedness */
    if (cxf_check_obvious_infeasibility(model)) {
        model->status = CXF_INFEASIBLE;
        return CXF_INFEASIBLE;
    }
    if (cxf_check_obvious_unboundedness(model)) {
        model->status = CXF_UNBOUNDED;
        return CXF_UNBOUNDED;
    }

    /* Initialize solver state */
    rc = cxf_simplex_init(model, &state);
    if (rc != CXF_OK) { model->status = rc; return rc; }

    /* Phase I: find feasible basis */
    rc = cxf_setup_phase_one(state);
    if (rc != CXF_OK) { model->status = rc; cxf_simplex_final(state); return rc; }
    cxf_simplex_perturbation(state, env);
    cxf_compute_reduced_costs(state);
    rc = cxf_run_phase_one(state, model, env);
    if (rc != CXF_OK) { model->status = rc; cxf_simplex_final(state); return rc; }

    /* Phase II: optimize original objective */
    rc = cxf_transition_to_phase_two(state, model);
    if (rc != CXF_OK) { model->status = rc; cxf_simplex_final(state); return rc; }
    cxf_compute_reduced_costs(state);
    cxf_run_phase_two(state, model, env);

    /* Post-solve: unperturb, refine, extract */
    cxf_simplex_unperturb(state, env);
    cxf_simplex_refine(state, env);
    if (model->status == CXF_OPTIMAL) cxf_extract_solution(state, model);

    cxf_simplex_final(state);
    return model->status;
}
