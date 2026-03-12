/**
 * @file simplex_internal.h
 * @brief Internal declarations for cross-file simplex module functions.
 *
 * Replaces scattered extern declarations with a single shared header.
 * Include this from any simplex .c file that calls functions defined
 * in other simplex .c files.
 */

#ifndef SIMPLEX_INTERNAL_H
#define SIMPLEX_INTERNAL_H

#include "convexfeld/cxf_types.h"

/* Forward declarations */
struct SolverState;
struct CxfEnv;
struct CxfModel;
struct BasisState;
struct EtaBuffer;

/* Iteration status codes (used by step.c and solve_lp.c) */
#define ITERATE_CONTINUE   0
#define ITERATE_OPTIMAL    1
#define ITERATE_INFEASIBLE 2
#define ITERATE_UNBOUNDED  3

/* --- Presolve (presolve.c) --- */
int cxf_check_obvious_infeasibility(CxfModel *model);
int cxf_check_obvious_unboundedness(CxfModel *model);
int cxf_solve_unconstrained(CxfModel *model);

/* --- Crash (crash.c) --- */
int cxf_simplex_crash(SolverState *state, CxfEnv *env);

/* --- Phase I (phase_one.c) --- */
int cxf_setup_phase_one(SolverState *state);
int cxf_transition_to_phase_two(SolverState *state, CxfModel *model);

/* --- Phase loop (phase_loop.c) --- */
int cxf_check_phase_one_end(SolverState *state, CxfModel *model,
                            CxfEnv *env);

/* --- Reduced costs (reduced_costs.c) --- */
int cxf_compute_reduced_costs(SolverState *state);

/* --- Step2/Step3 bound propagation (step2.c, step3.c) --- */
int cxf_simplex_step2(SolverState *state, CxfEnv *env);
int cxf_simplex_step3(SolverState *state, CxfEnv *env);

/* --- Ratio test (ratio_test.c) --- */
int cxf_ratio_test(SolverState *state, CxfEnv *env, int enteringVar,
                   const double *pivotColumn, int columnNZ,
                   int *leavingRow_out, double *pivotElement_out,
                   int *status_out, double *theta_out);

/* --- Recomputation (recompute.c) --- */
int cxf_recompute_xB(SolverState *state);
void cxf_recompute_objective(SolverState *state);
double cxf_ftran_residual(SolverState *state, const double *a,
                          const double *x);

/* --- Pivot update (pivot_update.c) --- */
void cxf_pivot_update(SolverState *state, int col,
                      double oldLB, double newLB,
                      double oldUB, double newUB,
                      double infinityThreshold);

/* --- Pivot primal (pivot_primal.c) --- */
int cxf_pivot_primal(void *env, void *state, int var, double tol);

/* --- Activity bounds (setup.c) --- */
void cxf_compute_activity_bounds(SolverState *state, int count,
                                 const int *indices);

/* --- Refine (refine.c) --- */
int cxf_simplex_refine(SolverState *state, CxfEnv *env);

/* --- Scaling (scaling.c) --- */
void cxf_scale_problem(SolverState *state, double *rowScale,
                       double *colScale);

/* --- EXPAND analysis (expand.c) --- */
int cxf_expand_analyze_basic(SolverState *state, int row, int bvar,
                             double feas_tol);
int cxf_expand_widen_bounds(SolverState *state, CxfEnv *env,
                            double feas_tol, BasisState *basis,
                            int m, int total);

/* --- Solve driver (solve_lp.c) --- */
int cxf_solve_lp(CxfModel *model);

#endif /* SIMPLEX_INTERNAL_H */
