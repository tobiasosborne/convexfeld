# Module: Solve LP Core

## Purpose

The Solve LP Core module contains the two central orchestration functions for LP solving: the LP solve pipeline and the solver algorithm dispatch. Together they form the heart of the optimization flow, sitting between the entry chain (P3.24) and the simplex iteration engine (P3.20). The LP solve pipeline manages the complete lifecycle of an LP solve from initialization through solution extraction, coordinating the crash basis construction, crossover from barrier solutions, the two-level simplex iteration loop, piecewise-linear constraint handling, and result status mapping. The solver algorithm dispatch determines which algorithm to apply (simplex, barrier, concurrent, or first-order method) based on model structure and user parameters, manages the presolve-solve-uncrush cycle, and handles parameter backup and restore to protect the environment from solver-internal modifications.

These two functions are the most complex in the LP subsystem and implement the orchestration aspects of the revised simplex method (Dantzig, 1963; Maros, 2003), barrier-to-simplex crossover (Megiddo, 1991; Bixby and Saltzman, 1994), and the general algorithm selection logic for LP and QP problems.

## Functions

### cxf_solve_lp

**Purpose:** Orchestrate the complete LP simplex solve pipeline, managing initialization, crash basis construction, barrier crossover, the two-level simplex iteration loop with anti-cycling, piecewise-linear constraint processing, solution extraction, and status mapping.

**Signature:**
- Input: `model` : pointer-to-Model - The model to solve, with valid matrix data, environment, and optional barrier solution
- Input: `timing` : pointer-to-double-array - Timing accumulator for performance instrumentation
- Input: `status_out` : pointer-to-int - Output location for the solve status code
- Input: `mode` : int - Solve mode flag controlling iteration limits and tolerance scaling (values: normal, crossover, or primal mode)
- Output: int - Zero on success, or an error code on failure (out-of-memory, numeric failure, or propagated sub-function error)

**Preconditions:**
- The model must be valid (validity sentinel verified by caller in P3.24)
- The model's matrix data must be populated with constraint dimensions, nonzero counts, and coefficient arrays
- The model's environment must be accessible with solver parameters set
- If a barrier solution is present in the solver state, crossover parameters must be configured

**Postconditions:**
- On success: the solve status has been written to `status_out`, the model's solution data has been populated (primal values, dual values, reduced costs, slack values, objective value), timing accumulators have been updated, solver state has been deallocated, and environment parameters have been restored to their pre-solve values
- On error: partial cleanup has been performed (solver state deallocated if allocated), the error code has been propagated, and environment parameters have been restored

**Side Effects:**
- Allocates and deallocates SolverState (via cxf_simplex_init and cxf_solver_state_cleanup)
- Modifies model solution data (primal, dual, slack, reduced costs, objective)
- Temporarily modifies environment parameters (iteration limits, tolerances) during the solve, restoring them before return
- Writes to timing accumulators throughout the solve
- Issues callbacks at solve milestones (solution found, iteration progress)
- Logs solve progress messages to the environment's log facility

**Error Conditions:**
- cxf_simplex_init allocation failure -> returns out-of-memory code
- Any sub-function (crash, crossover, iteration, step, phase_end) returns error -> propagates immediately after cleanup
- Iteration limit reached without convergence -> sets iteration-limit status (not an error return; zero return with limit status)
- Time limit reached -> sets time-limit status (not an error return)
- Numeric difficulties during iteration -> sets numeric status

**Behavioral Description:**

This function implements the complete LP simplex solve pipeline as the central orchestration point between the solve entry chain (P3.24) and the individual simplex components (P3.20, P3.21, P3.22, P3.23). It manages nine phases that together implement the revised simplex method (Dantzig, 1963; Maros, 2003) with modern enhancements for crossover and anti-cycling.

**Phase 1: Parameter extraction.** The function reads solver parameters from the model's environment: simplex mode (primal, dual, or auto), method override, crossover mode, iteration limits, refactoring frequency, and algorithm selection flags. A subset of these parameters are saved for later restoration, since the solve process may modify them internally.

**Phase 2: Solver state initialization.** The function delegates to cxf_simplex_init (P3.22) to allocate and populate the SolverState structure. This includes copying problem dimensions from the model's matrix data, allocating working arrays for bounds, reduced costs, basis tracking, eta vectors, and pricing state. A work estimate is computed from the problem's nonzero count, variable count, and constraint count, which is used to calibrate tolerances for the specific problem size. The tolerance calculation scales the work estimate by a mode-dependent multiplier: the dual simplex mode uses a base tolerance, the primal mode uses a tighter tolerance, and the auto mode uses an intermediate value. This scaling ensures that larger problems receive appropriately relaxed tolerances to avoid premature termination due to accumulated numerical error.

**Phase 3: Method selection.** The simplex method (primal or dual) is selected based on a priority chain:
1. User-specified method override (if set in the environment)
2. Simplex mode parameter (primal=0, dual=1, auto=2)
3. Concurrent solve indicator (overrides to auto selection)
4. Barrier status flag (non-zero indicates QP/SOCP, which constrains method choice)

The default selection for pure LP problems prefers dual simplex, consistent with the well-established practice that dual simplex is generally faster for LP (Maros, 2003, Chapter 9). For problems with a barrier solution present, the method is configured for crossover cleanup.

**Phase 4: Crash basis construction.** If an initial basis is needed (no warm start and no barrier solution), the function calls cxf_simplex_crash (P3.21) to construct a crash basis. For certain problem types and modes, an additional primal crash variant is also invoked. The crash basis reduces the number of Phase I simplex iterations by providing a better starting point than the trivial all-slack basis (Gould and Reid, 1989).

**Phase 5: Crossover setup.** If a barrier (interior-point) solution exists in the solver state, the function performs barrier-to-simplex crossover. This involves:
1. Copying the barrier solution bounds to working storage
2. Calling cxf_crossover (P3.23) to classify variables and push them toward bounds
3. Calling cxf_crossover_bounds (P3.23) to finalize the bound assignments

This crossover procedure converts the interior-point solution to a basic feasible solution suitable for simplex continuation, following the framework of Megiddo (1991) and Bixby and Saltzman (1994).

**Phase 6: Two-level iteration loop.** This is the core of the LP solve. The function implements a two-level loop structure to ensure convergence while preventing cycling:

*Outer loop:* Controls the total number of iteration rounds. The maximum round count depends on the solve mode: normal mode allows up to approximately 100 rounds, crossover mode allows approximately 10, and primal mode allows approximately 5. Each outer round performs a full inner loop cycle and then checks whether the basis has changed sufficiently to warrant another round.

*Inner loop:* Within each outer round, the function executes the following sequence repeatedly until the basis stabilizes:

1. **Basis snapshot** (cxf_basis_snapshot, P3.16): Capture the current basis state for later comparison.
2. **Simplex iterate** (cxf_simplex_iterate, P3.20): Execute progress logging and bookkeeping for the current iteration batch.
3. **Iterate variant**: Execute a post-iteration variant for additional processing (phase transition checks).
4. **Perturbation** (cxf_simplex_perturbation, P3.21): On early iterations only, apply anti-cycling perturbation if the EXPAND procedure (Gill, Murray, Saunders, and Wright, 1989) determines that the solver is stalling.
5. **Step** (cxf_simplex_step, P3.20): Execute the primary simplex pivot operation (pricing, ratio test, basis update).
6. **Step2** (cxf_simplex_step2, P3.20): Execute variable-side bound propagation to tighten bounds based on the most recent pivot.
7. **Step3** (cxf_simplex_step3, P3.20): Execute constraint-side bound propagation (LP problems only; skipped for QP/piecewise-linear problems).
8. **Phase end** (cxf_simplex_phase_end, P3.21): Process constraint cleanup and phase transitions.
9. **Basis diff** (cxf_basis_diff, P3.16): Compare the current basis to the snapshot. If the basis has changed by less than a convergence threshold (scaled by the iteration count), the inner loop terminates for this round.

The two-level structure prevents cycling by limiting the number of times the solver can restart with the same (or nearly the same) basis. If the outer loop's basis diff check also shows insufficient progress, the solver terminates with the best solution found.

**Phase 7: Piecewise-linear constraint processing.** If the model contains piecewise-linear (PWL) constraints, the function processes them within the iteration loop. PWL handling involves:
1. Iterating over active PWL variables (those with a nonzero segment count in the PWL index array)
2. Computing breakpoint transitions via a ratio test specific to PWL segments
3. Updating objective coefficients when a variable crosses a breakpoint (the objective slope changes at each breakpoint)
4. Handling segment shifting as variables move through their piecewise-linear ranges

The breakpoint transition mechanism operates as follows:

- **Range validation:** For each active PWL variable, the function first checks whether the variable's current value is within the valid PWL range (bounded by the variable's working lower and upper bounds). Variables outside the PWL range are skipped for that iteration.

- **Pending coefficient delta application:** If a previous iteration left a pending coefficient delta for the variable (stored in a working array shared with the eta vector storage), the delta is applied to all segment slope values for that variable before proceeding. This delta application uses a vectorized batch update for efficiency, processing multiple segments per pass.

- **Breakpoint ratio test:** The function evaluates the variable's position relative to the PWL breakpoints by computing the weighted combination of slope and intercept at each segment boundary. It determines the minimum and maximum function values across the segments and compares these against the function value at the current breakpoint. This ratio test identifies whether the variable has crossed a breakpoint boundary (moved from one linear segment to the next).

- **Direction determination:** Based on the ratio test result, the function determines the direction of the breakpoint transition. If the variable's value has moved below the first breakpoint, it is marked for a forward transition; if it has exceeded the last breakpoint, it is marked for a backward transition. The transition direction is recorded in the working array for use by the subsequent column selection step.

- **Column selection and objective update:** After determining the transition direction, the function invokes a column selection operation to integrate the PWL coefficient change into the simplex basis. If the affected variable is nonbasic after the column selection, the objective value is immediately adjusted by the difference between the old and new segment's contribution (computed from the segment's slope and intercept evaluated at the variable's current value). The variable's PWL segment count is then decremented or zeroed, and the overall active PWL count is updated.

- **Segment shifting (finalization):** At the end of the iteration round, a finalization pass processes all remaining active PWL variables. For variables that have moved past multiple breakpoints, the segment arrays are shifted: consumed segments are removed by copying later segments down in the array, and the segment count is updated accordingly. If all segments have been consumed (the variable has traversed its entire piecewise-linear range), the final segment's slope becomes the variable's permanent objective coefficient, and the variable is removed from the active PWL set.

The PWL coefficient updates use a batch processing approach for efficiency, processing multiple variables per pass and applying vectorized updates to the segment slope arrays. This is a solver-specific extension beyond the standard simplex method, implementing the piecewise-linear objective handling technique described in general terms by Fourer (1992).

**Phase 8: Solution extraction.** After the iteration loop terminates, the function:
1. Calls cxf_solution_extract to copy the solution from the solver state to the model's solution data structures
2. Handles alternative model processing (if the model was presolved, the solution is mapped back to the original variables)
3. Performs final status checks and issues solution-found callbacks

**Phase 9: Cleanup and status mapping.** The function:
1. Stores the solve status in the model
2. Logs presolve summary information (variables fixed, constraints removed)
3. Calls cxf_solver_state_cleanup (P3.22) to deallocate the solver state
4. Maps internal status codes to public status codes. Key mappings include:
   - When solving with a working (presolved) matrix, infeasible and unbounded statuses may be swapped (because presolve can transform infeasibility into unboundedness and vice versa)
   - For crossover-origin solves, an ambiguous status is mapped to "infeasible-or-unbounded" rather than committing to one interpretation
5. Restores all environment parameters that were modified during the solve

**Thread Safety:** Not thread-safe. Each concurrent solve must use an independent model with its own solver state. Thread safety for concurrent LP solving is achieved at the model level (P3.24, P3.25 cxf_solver_dispatch concurrent dispatch).

**Dependencies:**
- P3.22 (Simplex Lifecycle) - cxf_simplex_init for state allocation, cxf_simplex_cleanup for bound tightening and deallocation, cxf_solver_state_cleanup for state deallocation
- P3.21 (Simplex Phases) - cxf_simplex_crash for crash basis, cxf_simplex_perturbation for anti-cycling, cxf_simplex_phase_end for phase transition processing
- P3.20 (Simplex Iteration) - cxf_simplex_iterate for logging, cxf_simplex_step/step2/step3 for pivoting and bound propagation, cxf_simplex_post_iterate for stall detection
- P3.23 (Crossover) - cxf_crossover and cxf_crossover_bounds for barrier-to-simplex crossover
- P3.16 (Basis Factorization) - cxf_basis_refactor for LU factorization, cxf_basis_snapshot and cxf_basis_diff for convergence detection
- P1.02 (Model) - model structure access for matrix data, environment, solution data
- P1.04 (SolverState) - solver state structure for all working data
- P1.01 (Environment) - parameter access for solver configuration

---

### cxf_solver_dispatch

**Purpose:** Determine the appropriate solving algorithm for the model based on its structure (LP, QP, SOCP) and user parameters, dispatch to the selected solver, manage the presolve-solve-uncrush cycle, and report optimization results.

**Signature:**
- Input: `model` : pointer-to-Model - The model to solve, with valid matrix data, environment, and optional warm-start or hint information
- Input: `timing` : pointer-to-double-array - Timing accumulator for performance instrumentation
- Output: int - Zero on success, or an error code on failure (out-of-memory, Q-not-PSD, worker pool error, or propagated solver error)

**Preconditions:**
- The model must be valid (validity sentinel verified by caller in P3.24)
- The model's matrix data must be fully populated
- The model's environment must have configured parameters
- For QP models: the Q matrix must be populated
- For concurrent solving: concurrent environment count must be set

**Postconditions:**
- On success: the model's status has been set, solution data has been populated (primal values, objective value, iteration count), timing accumulators have been updated, result summary has been logged, and all environment parameters have been restored to their pre-dispatch values
- On error: partial cleanup has been performed, the error code has been propagated, and environment parameters have been restored

**Side Effects:**
- Backs up approximately 30 environment parameters to local storage and restores them before return
- Initializes and clears solve state fields on the model (status, method indicator, warm-start state)
- May allocate a presolved model (stored at the model's presolve model field) and deallocate it before return
- Modifies model's solution arrays (primal, dual, slack, reduced costs, objective, bounds)
- Computes and stores a model fingerprint (hash) for change detection
- Logs detailed optimization results (iterations, nodes, time, objective, gap, status)
- May create and destroy concurrent solver threads

**Error Conditions:**
- Out-of-memory during solution allocation -> returns out-of-memory code
- QP model with non-positive-semidefinite Q matrix -> returns Q-not-PSD code (with diagnostic message logged)
- Worker pool communication failure (distributed solving) -> returns worker pool error code
- Any dispatched solver returns error -> propagated after cleanup and parameter restore

**Behavioral Description:**

This function is the algorithm routing and lifecycle management layer, sitting between the solve entry chain (P3.24) and the individual solver implementations (cxf_solve_lp for simplex, cxf_solve_barrier for interior point, and others). It handles the complexity of selecting the right algorithm, managing presolve, and extracting solutions for all model types.

**Phase 1: Parameter backup.** The function saves approximately 30 environment parameters to a local backup structure. These parameters span method selection, tolerances, heuristic settings, thread counts, and algorithm tuning. This backup is essential because solver sub-functions may modify environment parameters during the solve (for example, adjusting tolerances when encountering numerical difficulty or overriding method selection for sub-problems). All parameters are restored from this backup before the function returns, regardless of success or failure.

**Phase 2: Solve state initialization.** The function clears and initializes solve state fields on the model:
- The Q-matrix positive-semidefiniteness adjustment flag is cleared
- The solve status is set to its initial value
- The method indicator is set to the user-requested method
- Warm-start state is cleared if a fresh solve is indicated

**Phase 3: Model type detection.** The function inspects the model to determine its structure and select the appropriate solving algorithm:

1. **Quadratic detection.** If the model has a quadratic objective (Q matrix with nonzero terms), it is classified as QP. For QP models, the default method is barrier (interior point), since simplex methods for QP require specialized extensions. If the user requested PDHG (first-order method) for a QP, a warning is logged and the method is adjusted.

2. **SOCP detection.** If the model contains second-order cone constraints, it is classified as SOCP. SOCP models are restricted to the barrier method, since simplex and first-order methods do not natively handle conic constraints.

3. **Non-convex QP handling.** If the model is a non-convex QP (the Q matrix is indefinite), the function checks the NonConvex parameter. If non-convex solving is not enabled, the function returns the Q-not-PSD error with a diagnostic message. If enabled, the function adjusts the solving strategy (typically linearizing the quadratic terms via the PreQLinearize parameter).

4. **Concurrent restrictions.** Concurrent solving is restricted for certain model types: QP models cannot use concurrent simplex (only concurrent barrier variants), and SOCP models cannot use concurrent solving at all.

**Phase 4: Method selection.** For LP models with AUTO method selection, the function applies a heuristic scoring procedure to choose between simplex and barrier:
- Problem structure indicators (sparsity, dimension ratio, constraint types) are evaluated via a multi-factor scoring system
- The scoring favors barrier for large, sparse problems and simplex for smaller or denser problems. Specifically, the following standard LP solver design criteria are used:
  - **Constraint-to-variable ratio:** Problems with significantly more variables than constraints (wide, flat problems) tend to favor barrier methods, while problems closer to square or with more constraints than variables tend to favor simplex [RECOMMENDED] (Bixby, 2002)
  - **Sparsity:** Highly sparse problems (low nonzero density in the constraint matrix) favor barrier, since interior-point methods exploit sparsity in the Cholesky factorization of the normal equations. Denser problems favor simplex, where the per-iteration cost is less sensitive to overall density [RECOMMENDED] (Maros, 2003, Chapter 1)
  - **Problem scale:** Larger problems (tens of thousands of constraints or more) tend to favor barrier, which has polynomial worst-case complexity, while smaller problems often solve faster with simplex due to lower per-iteration overhead [RECOMMENDED]
  - **Warm-start availability:** The presence of a previous basis or variable start values strongly favors simplex, since barrier methods cannot directly exploit warm-start information (Bixby, 2002)
- The scoring accumulates evidence from multiple indicators: each factor contributes a positive or negative score, and the aggregate score determines whether barrier, simplex, or concurrent is selected
- Concurrent methods (methods 3, 4, 5) are selected based on the ConcurrentMethod and ConcurrentJobs parameters, and may be chosen when the scoring is ambiguous (no strong preference for either method) and sufficient computational resources are available

**Phase 5: Model fingerprinting.** Before dispatching to a solver, the function computes a fingerprint (hash) of the model's current state. This fingerprint is stored for later comparison to detect whether the model was modified between solves.

**Phase 6: Solver dispatch.** The function routes to the appropriate solver based on the selected method and model type:

- **Concurrent LP (methods 3, 4, 5):** Dispatches to the concurrent solver, which launches multiple solver instances (simplex and/or barrier) in parallel threads and returns the first result. Deterministic concurrent mode (method 4) ensures reproducible results by synchronizing thread scheduling. For distributed concurrent, the function dispatches to the distributed solver variant that communicates with remote worker nodes.

- **Barrier (method 2):** Dispatches to the barrier (interior point) solver. If crossover is enabled (the default), the barrier result is followed by a crossover phase to obtain a vertex solution.

- **PDHG (method 6):** Dispatches to the primal-dual hybrid gradient solver, a first-order method suitable for large LP problems where moderate accuracy is acceptable. This method is LP-only and does not support QP.

- **Simplex (methods 0, 1):** Dispatches to the simplex solver (cxf_solve_lp, this module). Method 0 selects primal simplex and method 1 selects dual simplex. If warm-start information or variable hints are available, they are passed to the simplex solver for initialization.

**Phase 7: Warm-start and hint processing.** Before dispatching simplex solves, the function processes available warm-start information:
- If a previous basis is available, it is passed as the starting basis
- If variable hints are provided (hint values and hint validity indicators), they are used to guide the initial solution
- If partition data is available, it is used by the concurrent solver for workload distribution

**Phase 8: Solution extraction and uncrushing.** After the solver returns:
1. Statistics (iteration count, node count, runtime) are copied from the solver's internal accounting to the model
2. For presolved models, the solution is uncrushed to the original variable space
3. Primal solution values are allocated (if not already) and populated
4. The objective value is computed from the primal solution and the original objective coefficients (as a verification against the solver's reported objective)

**Phase 9: Result reporting.** The function logs a detailed summary of the optimization results:
- For LP: iteration count, runtime, objective value, and status message
- Q-not-PSD errors receive a specific diagnostic message explaining the issue
- The status is translated to a human-readable message (Optimal, Infeasible, Unbounded, Time limit, etc.)

**Phase 10: Parameter restore.** All approximately 30 environment parameters saved in Phase 1 are restored from the backup structure. This occurs on both the success and error paths, ensuring the environment is always returned to its pre-dispatch state.

**Thread Safety:** Not thread-safe at the function level. However, the function may internally launch concurrent solver threads when concurrent methods are selected. Thread safety for the concurrent solvers is managed internally by creating independent model copies for each thread. The caller must ensure that the model is not modified by other threads during the dispatch.

**Dependencies:**
- P3.25 (this module) - cxf_solve_lp for LP simplex solving
- P3.24 (Solve Entry) - called from the solve entry chain
- P1.02 (Model) - model structure access for matrix data, environment, solution data, presolve data, concurrent environments
- P1.01 (Environment) - parameter access for method selection, tolerances, algorithm control, concurrent settings
- P1.04 (SolverState) - solver state for solution tracking
- P2.1 (Revised Simplex Method) - simplex method selection logic
- P2.7 (Barrier-to-Simplex Crossover) - crossover configuration when barrier method is selected

---

## Module-Level Behavioral Notes

### Role in the Solve Chain

The two functions in this module occupy the central position in the optimization call chain:

```
cxf_optimize (P3.24, public API)
  -> cxf_optimize_internal (P3.24)
    -> cxf_solve_entry (P3.24)
      -> cxf_solve_dispatch (P3.24, multi-scenario)
        -> cxf_solve_with_callbacks / cxf_solve_no_callbacks (P3.24)
          -> cxf_solver_dispatch (THIS MODULE) -- algorithm routing
            -> cxf_solve_lp (THIS MODULE) -- LP simplex pipeline
              -> cxf_simplex_init (P3.22) -- state allocation
              -> cxf_simplex_crash (P3.21) -- crash basis
              -> cxf_crossover (P3.23) -- barrier crossover
              -> [iteration loop]:
                -> cxf_simplex_iterate (P3.20)
                -> cxf_simplex_step (P3.20)
                -> cxf_simplex_step2 (P3.20)
                -> cxf_simplex_step3 (P3.20)
                -> cxf_simplex_phase_end (P3.21)
                -> cxf_simplex_perturbation (P3.21)
              -> cxf_solution_extract
              -> cxf_simplex_cleanup (P3.22)
```

cxf_solver_dispatch handles the branching point where the solve flow splits by algorithm type (simplex vs. barrier vs. concurrent vs. PDHG). cxf_solve_lp handles the LP-specific pipeline after the algorithm has been selected.

### Parameter Backup and Restore Pattern

Both functions implement a parameter save/restore pattern to protect the environment:

- **cxf_solve_lp** saves and restores a small set of LP-specific parameters (iteration limits, tolerances, mode flags) that may be modified by the inner loop or sub-functions.

- **cxf_solver_dispatch** saves and restores approximately 30 parameters spanning the full range of solver configuration (method, tolerances, threading, heuristics, presolve settings). This broader backup is necessary because the dispatch function may invoke solvers that modify any of these settings.

Both functions restore parameters on all exit paths (success, error, early termination), ensuring the environment is never left in a modified state after a solve completes. This invariant is critical for users who call cxf_optimize repeatedly on the same model.

### The Two-Level Iteration Loop Design

The two-level loop in cxf_solve_lp is a distinctive architectural feature. Its purpose is to handle the convergence challenges of the simplex method, particularly cycling and stalling on degenerate problems:

**Inner loop (basis stabilization):** Executes simplex pivots until the basis stabilizes (measured by cxf_basis_diff returning a value below a threshold). When the basis stabilizes, no further progress is being made, and the inner loop yields control to the outer loop.

**Outer loop (round control):** Decides whether to restart the inner loop. After each inner loop termination, the outer loop takes a new basis snapshot and checks whether sufficient overall progress has been made since the last outer snapshot. If progress is sufficient, a new inner loop round begins. If progress is insufficient (or the round limit is reached), the solve terminates.

The round limits (approximately 5/10/100 depending on mode) serve as an ultimate backstop against infinite cycling. The threshold for "sufficient progress" in the basis diff comparison is scaled by the iteration count, becoming more permissive as more iterations accumulate. This adaptive threshold prevents premature termination on problems where progress is slow but steady.

This two-level structure is related to the long-step/short-step pivot strategies discussed in the literature (Maros, 2003, Chapter 10), though the specific implementation with basis snapshot comparisons appears to be a solver-specific design choice.

### Algorithm Selection Heuristics

cxf_solver_dispatch implements a multi-factor heuristic for automatic method selection when the user specifies Method=AUTO:

1. **Model type constraints:** SOCP forces barrier; QP defaults to barrier.

2. **Problem size and structure:** For LP models, the heuristic evaluates multiple problem characteristics and computes an aggregate score. The scoring system works as follows:

   - **Sparsity indicators:** The ratio of nonzeros to total matrix entries (density) and the absolute nonzero count are evaluated. Very sparse problems (below a density threshold) receive a score increment toward barrier. This reflects the well-known advantage of interior-point methods on sparse problems, where the Cholesky factorization of the normal equations A*A^T remains sparse (Maros, 2003, Chapter 1; Bixby, 2002).

   - **Dimension ratio:** The constraint-to-variable ratio is assessed. Problems with many more variables than constraints (common in network flow, supply chain, and covering formulations) tend to favor barrier. Problems closer to square favor simplex. [RECOMMENDED: as a general guideline, barrier tends to outperform simplex when the variable-to-constraint ratio exceeds approximately 5:1 to 10:1, though this depends on sparsity and other factors.]

   - **Objective and bound characteristics:** The range and distribution of objective coefficients and variable bounds are evaluated. Problems with well-scaled coefficients and tight bounds may favor simplex, while problems with highly variable coefficient scales may benefit from barrier's global convergence properties.

   - **Score aggregation:** Each indicator contributes a positive score (favoring concurrent or barrier) or a negative score (favoring simplex). A base score is added, and the final aggregate determines the selection:
     - High aggregate score -> concurrent (if resources allow) or barrier
     - Low aggregate score -> barrier alone
     - Very low or negative aggregate score -> simplex (typically dual simplex for LP)

3. **Warm-start availability:** Warm-start information (a previous basis or variable hints) strongly favors simplex selection, since interior-point methods cannot directly exploit warm-start data. When a warm start is available and the thread count is below a moderate threshold, the system may additionally adjust the concurrent configuration to include a warm-started simplex instance (Bixby, 2002).

4. **Concurrent fallback:** When concurrent methods are available (sufficient thread count), the dispatch may select concurrent solving to hedge between methods, running simplex and barrier simultaneously and returning whichever finishes first. The concurrent option is preferred when the scoring is ambiguous (no strong preference for either method), since it hedges against the risk of choosing the slower algorithm.

The specific scoring weights and thresholds for the auto-selection heuristic are solver-tuning decisions that vary across solver versions. The general framework of evaluating problem structure to select between simplex and barrier is a well-established practice in the LP solver literature (Bixby, 2002; Maros, 2003).

### Solver Method Codes

| Value | Method | Description | Model Types |
|-------|--------|-------------|-------------|
| -1 | AUTO | Automatic selection via heuristic | All |
| 0 | PRIMAL_SIMPLEX | Primal simplex method | LP, QP |
| 1 | DUAL_SIMPLEX | Dual simplex method (default for LP) | LP, QP |
| 2 | BARRIER | Interior-point method | LP, QP, SOCP |
| 3 | CONCURRENT | Run multiple solvers concurrently | LP |
| 4 | DETERMINISTIC_CONCURRENT | Deterministic concurrent (reproducible) | LP |
| 5 | CONCURRENT_3 | Concurrent with three methods | LP |
| 6 | PDHG | Primal-Dual Hybrid Gradient | LP only |

### Solve Status Codes

Both functions produce and interpret the following status codes:

| Status | Meaning | Notes |
|--------|---------|-------|
| Optimal | Optimal solution found | Feasible and optimal within tolerances |
| Infeasible | Model is infeasible | No feasible solution exists |
| Infeasible-or-Unbounded | Cannot determine | Presolve detected issue but cannot classify |
| Unbounded | Model is unbounded | Objective can be improved without bound |
| Time limit | Time limit reached | Best solution so far returned if available |
| Iteration limit | Iteration limit reached | Best solution so far returned if available |
| Numeric | Numerical difficulties | Solution may be unreliable |
| Suboptimal | Solution found but not proven optimal | Solution satisfies feasibility but not optimality |

cxf_solve_lp performs status code remapping in its cleanup phase: when solving with a presolved (working) matrix, infeasible and unbounded statuses may be swapped because the presolve transformations can interchange these conditions. For crossover-origin solves, an ambiguous infeasible/unbounded result is conservatively mapped to the "infeasible-or-unbounded" status.

### Piecewise-Linear Constraint Processing

cxf_solve_lp contains specialized handling for piecewise-linear (PWL) constraints that is integrated into the main iteration loop. PWL constraints define an objective or constraint function as a sequence of linear segments connected at breakpoints. The implementation:

1. **Representation:** PWL data is stored in five parallel arrays on the solver state: a segment-start index array (mapping each variable to its first segment), a segment count array (number of segments per variable), two value arrays storing the slope and intercept for each segment, and a storage array holding the breakpoint coordinate values. Each PWL variable has a current segment count indicating how many linear pieces remain active.

2. **Breakpoint transitions:** During each iteration round, the function checks whether any PWL variable has moved past a breakpoint. The transition detection uses a ratio test that evaluates the variable's current position against the minimum and maximum function values across the segment boundaries. For each segment, the function computes the weighted combination of slope and intercept evaluated at the breakpoint coordinates, building a min/max envelope across all segments. A transition is detected when the current function value falls outside this envelope, indicating the variable has moved into a different segment's domain.

3. **Coefficient updates:** When a breakpoint is crossed, the objective coefficients for the affected variable change to reflect the slope of the new segment. Pending coefficient deltas are accumulated in a working array (shared with the eta vector storage) and applied in batch before each ratio test. The batch application uses vectorized updates for efficiency. After a transition is confirmed and the column selection step completes, the objective value is immediately adjusted by the contribution difference between the old and new segments.

4. **Segment consumption and shifting:** As variables traverse their PWL ranges, consumed segments are removed. A finalization pass at the end of each iteration round shifts remaining segments down in the parallel arrays and updates segment counts. When all segments for a variable are consumed, the final segment's slope becomes the variable's permanent linear objective coefficient, and the variable is removed from the active PWL tracking set (its segment count is set to zero and the active PWL counter is decremented).

5. **Interaction with simplex:** PWL processing disables step3 (constraint-side bound propagation) because the changing objective coefficients invalidate the assumptions of that propagation step. The remaining simplex operations (pricing, ratio test, basis update) continue normally with the updated coefficients.

This PWL handling extends the standard simplex method to handle piecewise-linear objectives without reformulating them as additional variables and constraints, following the approach described by Fourer (1992). The key advantage of this integrated approach is that it avoids the increase in problem size that would result from introducing auxiliary variables and constraints for each PWL segment.

### Presolve-Solve-Uncrush Cycle

cxf_solver_dispatch manages the full presolve cycle:

1. **Presolve:** The presolve phase creates a reduced copy of the model by applying reductions (variable fixing, constraint elimination, bound tightening, coefficient strengthening). The reduced model is stored at the model's presolve model field.

2. **Solve:** The reduced model is solved instead of the original.

3. **Uncrush:** The solution from the reduced model is mapped back to the original model's variable space. This uncrushing reverses each presolve reduction in reverse order: re-introducing eliminated variables by computing their values from the remaining solution, relaxing tightened bounds, and restoring eliminated constraints.

4. **Verification:** The objective value is recomputed from the original model's coefficients and the uncrushed solution as a consistency check.

This three-phase pattern is standard in modern LP solvers (Andersen and Andersen, 1995).

### Concurrent Solving Architecture

When a concurrent method is selected, cxf_solver_dispatch creates multiple solver instances that run in parallel:

- **Concurrent LP (method 3):** Launches simplex and barrier solvers concurrently. The first solver to find an optimal solution wins, and the others are terminated.
- **Deterministic concurrent (method 4):** Same as concurrent but with deterministic thread scheduling to ensure reproducible results across runs.
- **Concurrent 3 (method 5):** Launches three solver methods concurrently (typically primal simplex, dual simplex, and barrier).
- **Distributed concurrent:** When worker pool connections are configured, solver instances are dispatched to remote worker nodes for distributed parallel solving.

Each concurrent solver instance operates on an independent copy of the model with its own solver state, ensuring thread safety without fine-grained locking.

### Error Propagation Pattern

Both functions follow a consistent error propagation pattern:

1. Each sub-function call is checked for a non-zero return (indicating error).
2. On error, control transfers to a cleanup section that:
   a. Deallocates any allocated solver state
   b. Restores environment parameters from backup
   c. Propagates the error code to the caller
3. Resource deallocation is idempotent (null-safe), so partial initialization is handled correctly.

This pattern ensures that no resources leak and the environment is always restored, even when errors occur deep in the call chain.

### Numerical Considerations

- **Tolerance scaling:** cxf_solve_lp calibrates tolerances to the problem size using a work estimate derived from nonzero count and dimension counts. This adaptive tolerance prevents false convergence claims on large problems and unnecessary precision demands on small problems.

- **Basis diff threshold:** The convergence threshold for the basis snapshot comparison is scaled by the iteration count, becoming more permissive over time. This prevents the solver from terminating prematurely on problems that make slow but steady progress.

- **Status ambiguity after presolve:** Presolve reductions can make it impossible to distinguish infeasibility from unboundedness. cxf_solve_lp handles this by mapping ambiguous results to the "infeasible-or-unbounded" status rather than guessing.

- **Objective verification:** cxf_solver_dispatch recomputes the objective value from the original coefficients after solution extraction as a numerical consistency check. If the recomputed value differs significantly from the solver's reported value, the discrepancy is logged.

- **Markowitz tolerance:** cxf_solver_dispatch reads and may adjust the Markowitz tolerance parameter, which controls the tradeoff between sparsity and numerical stability in basis factorization. The default value follows standard practice for LP solvers (approximately 0.0078 to 0.5 range, as discussed in Duff, Erisman, and Reid, 1986).

### Return Code Conventions

| Code | Meaning | Source |
|------|---------|--------|
| Success (zero) | Operation completed normally | Both functions |
| Out-of-memory code | Memory allocation failed | Both functions |
| Q-not-PSD code | Quadratic matrix is not positive-semidefinite | cxf_solver_dispatch only |
| Worker pool error code | Distributed solver communication failure | cxf_solver_dispatch only |
| Propagated solver error | Error from sub-function passed through | Both functions |

Note: iteration limit, time limit, and numeric difficulty are communicated via the status output parameter, not the return code. A zero return code with a non-optimal status indicates a successful (but non-optimal) termination.

### Thread Safety Summary

| Function | Thread Safety | Notes |
|----------|---------------|-------|
| cxf_solve_lp | Not thread-safe | Modifies model solution data, solver state |
| cxf_solver_dispatch | Not thread-safe | Modifies model state, may launch internal threads for concurrent |

Both functions require exclusive access to the model during the solve. Concurrent solving of the same model from multiple user threads is not supported. Concurrent solver methods create internal model copies and manage their own thread safety.

---

## Out-of-Scope Components

### Presolve Reduction Techniques

The presolve system is **outside the scope of this specification**. While cxf_solver_dispatch orchestrates the presolve-solve-uncrush cycle (creating a reduced model, solving it, and mapping the solution back to the original variable space), the presolve reduction techniques themselves -- the specific transformations that simplify the model before solving -- are not specified here.

Presolve for LP problems encompasses a wide range of reduction techniques including:
- Bound tightening (variable and constraint)
- Redundant constraint detection and removal
- Variable fixing (singleton columns, forcing constraints)
- Coefficient strengthening
- Substitution of implied free variables
- Parallel row and column detection

These techniques are extensively documented in the published literature:
- Andersen, E.D. and Andersen, K.D. (1995). "Presolving in Linear Programming." *Mathematical Programming*, 71(2):221-245. (Foundational LP presolve techniques.)

The solve pipeline's interaction with presolve is documented in this specification:
- cxf_solver_dispatch Phase 8 describes solution uncrushing (reversing presolve reductions to recover the original-space solution).
- cxf_solve_lp Phase 9 describes status code remapping that accounts for presolve's potential to interchange infeasible and unbounded statuses.

A separate presolve module specification would be required to cover the reduction techniques in detail.

---

## Verification Checklist

```
[x] No hex addresses or offsets
[x] No Ghidra artifacts (param_, local_, iVar, DAT_, FUN_, thunk_)
[x] No binary-specific constants or magic numbers
[x] No copied code fragments from analyzed source
[x] All descriptions are behavioral, not implementational
[x] All data structures described semantically using Layer 1/2 references
[x] Explicit cross-references to P1.01, P1.02, P1.04 (data model), P2.1, P2.7 (algorithms), and P3.16, P3.20-P3.24 (module specs)
[x] All algorithms cite published sources
[x] Passes the Clean Room Test: could be written without seeing the binary
```

## References

- Andersen, E.D. and Andersen, K.D. (1995). "Presolving in Linear Programming." *Mathematical Programming*, 71(2):221-245.
- Bixby, R.E. (2002). "Solving Real-World Linear Programs: A Decade and More of Progress." *Operations Research*, 50(1):3-15.
- Bixby, R.E. and Saltzman, M.J. (1994). "Recovering an Optimal LP Basis from an Interior Point Solution." *Operations Research Letters*, 15(4):169-178.
- Dantzig, G.B. (1963). *Linear Programming and Extensions*. Princeton University Press.
- Duff, I.S., Erisman, A.M., and Reid, J.K. (1986). *Direct Methods for Sparse Matrices*. Oxford University Press.
- Fourer, R. (1992). "A Simplex Algorithm for Piecewise-Linear Programming III: Computational Analysis and Applications." *Mathematical Programming*, 53(1-3):213-235.
- Gill, P.E., Murray, W., Saunders, M.A., and Wright, M.H. (1989). "A practical anti-cycling procedure for linearly constrained optimization." *Mathematical Programming*, 45(1-3):437-474.
- Gould, N.I.M. and Reid, J.K. (1989). "New crash procedures for large systems of linear constraints." *Mathematical Programming*, 45(1-3):475-501.
- Maros, I. (2003). *Computational Techniques of the Simplex Method*. Springer. International Series in Operations Research and Management Science, Vol. 61.
- Megiddo, N. (1991). "On Finding Primal- and Dual-Optimal Bases." *ORSA Journal on Computing*, 3(1):63-65.
