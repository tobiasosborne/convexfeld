# Optimization Pipeline

## Overview

This document describes the end-to-end integration flow for the optimization pipeline: the complete path from a user calling the public optimize function through solver dispatch, algorithm execution, solution processing, and result delivery. It is the most important integration specification because it traces the primary data and control flow through the entire solver, touching every major module.

The pipeline is not a simple linear sequence. It is a layered, branching flow with three execution paths (synchronous, callback, no-callback), multiple algorithm branches (simplex, barrier, concurrent, first-order), a two-level iteration loop at its core, and a multi-stage post-solve pipeline that transforms raw solver output into user-accessible results. Understanding how these components connect -- which module hands off to which, what state crosses module boundaries, and how errors propagate back through the layers -- is essential for reimplementation.

This document focuses exclusively on cross-module integration: data flow, state handoffs, and error propagation boundaries. It does not repeat the internal behavior of individual modules, which is fully specified in the Layer 3 module contracts.

## Components Involved

The optimization pipeline involves the following modules, listed in approximate call-order:

| Module | Spec | Role in Pipeline |
|--------|------|-----------------|
| Input Validation | P3.07 | Model and environment validation at entry |
| Optimization Preparation | P3.32 | Signal handler installation, remote solver delegation |
| Threading & Synchronization | P3.11 | Locale safety, solve lock, thread management |
| Model Lifecycle | P3.31 | Lazy update flush (pending modification application) |
| Statistics & Diagnostics | P3.33 | Pre-solve model analysis, fingerprinting, coefficient warnings |
| Solve Entry & Dispatch | P3.24 | Entry chain, execution path selection, scenario routing |
| Solve LP Core | P3.25 | Algorithm routing, LP simplex pipeline orchestration |
| Simplex Lifecycle | P3.22 | SolverState allocation, post-solve fixing, cleanup |
| Simplex Phases | P3.21 | Crash basis, perturbation, preprocessing, phase transitions |
| Simplex Iteration | P3.20 | Pivot operations, bound propagation, progress monitoring |
| Basis Operations | P3.16 | Basis factorization, snapshots, eta management |
| Pricing | P3.17-P3.18 | Candidate selection, pricing updates |
| Pivot Operations | P3.19 | Ratio test, bound flips, variable fixing |
| Crossover | P3.23 | Barrier-to-simplex solution conversion |
| Barrier & Concurrent | P3.26 | Interior-point method, concurrent solver racing |
| Solution Processing | P3.29 | Uncrush, attribute wiring, gap computation, solution pool |
| Callbacks | P3.13 | User callbacks, lifecycle hooks |
| Error Handling | P3.09 | Error state management, message propagation |
| Cleanup Utilities | P3.34 | Signal handler restoration, bound propagation |
| Logging | P3.10 | Progress messages, solver output |

## Flow Description

### End-to-End Pipeline Overview

```
USER CODE
    |
    v
+--[1. cxf_optimize]--------------------------------------------------+
|  Validate model | Setup locale | Install signal handler             |
|  Set modification-blocked | Clear status | Log version/hardware     |
+---------------------------------------------------------------------+
    |
    v
+--[2. cxf_optimize_internal]------------------------------------------+
|  Select execution path | Flush pending modifications                |
|  Detect model type | Analyze coefficients | Log dimensions          |
+---------------------------------------------------------------------+
    |                    |                     |
    v                    v                     v
 [Normal Path]     [Callback Path]      [No-Callback Path]
    |              cxf_solve_with_        cxf_solve_no_
    |              callbacks             callbacks
    |                    |                     |
    v                    v                     v
+--[3. cxf_solve_entry]-----------------------------------------------+
|  Model type detection | Scenario routing                            |
|  Non-convex QP handling (retry with non-convex treatment if needed) |
+---------------------------------------------------------------------+
    |                              |
    v                              v
 [Single Model]             [Multi-Scenario]
    |                        cxf_solve_dispatch
    |                              |
    v                              v
+--[4. cxf_solver_dispatch (P3.25)]------------------------------------+
|  Parameter backup | Method selection heuristic                      |
|  Presolve-solve-uncrush cycle | Result reporting                    |
+---------------------------------------------------------------------+
    |            |            |            |
    v            v            v            v
 Simplex     Barrier     Concurrent    PDHG
 (0,1)       (2)         (3,4,5)       (6)
    |            |            |            |
    v            v            v            v
+--[5. Solver Algorithm Execution]------------------------------------+
|  (Details per algorithm path below)                                 |
+---------------------------------------------------------------------+
    |
    v
+--[6. Solution Processing]------------------------------------------+
|  Uncrush presolved solution | Evaluate objective                    |
|  Wire result attributes | Compute optimality gap                   |
+---------------------------------------------------------------------+
    |
    v
+--[7. Cleanup & Result Delivery]-------------------------------------+
|  Restore parameters | Restore signal handler | Release locale       |
|  Clear modification-blocked | Write result files                    |
+---------------------------------------------------------------------+
    |
    v
USER CODE (query attributes for solution data)
```

### Phase 1: Entry and Initialization (cxf_optimize)

The public API entry point performs the following cross-module interactions:

**Step 1.1: Model validation.** Delegates to the Input Validation module (P3.07) to verify the model pointer, validity sentinel, and structural integrity. On failure, the error code is returned immediately without further setup.

**Step 1.2: Signal handler installation.** Delegates to the Optimization Preparation module (P3.32) to install a signal handler for graceful interrupt support. The signal handler stores a model reference in module-level state and saves the previous handler for later restoration by the Cleanup Utilities module (P3.34).

**Step 1.3: Locale safety acquisition.** Delegates to the Threading & Synchronization module (P3.11) to save the calling thread's locale and switch to the standard "C" locale. This ensures consistent numeric formatting throughout the pipeline.

**Step 1.4: State initialization.** Sets the optimization-active flag on the environment, clears message buffers (via the Logging module, P3.10), sets the modification-blocked flag on the model, and clears the model's status code.

**Step 1.5: Version and hardware logging.** On the first call per environment, logs solver version, CPU model, instruction set, and thread count through the Threading & Synchronization module (P3.11) and the Logging module (P3.10).

**Step 1.6: remote solver delegation.** If the model is configured for remote computation, delegates to the Optimization Preparation module (P3.32) for remote solver optimization. The local pipeline then waits for the remote result.

**Step 1.7: Lifecycle callbacks.** Invokes pre-optimize and post-optimize lifecycle hooks through the Callbacks module (P3.13) to manage error buffer locking.

**State handed to Phase 2:** A validated model with locale safety active, modification-blocked flag set, optimization-active flag on the environment, and signal handler installed.

### Phase 2: Internal Dispatch (cxf_optimize_internal)

This function bridges the public API and the solve chain, performing model analysis and execution path selection.

**Step 2.1: Execution path selection.** Based on the model's callback count and asynchronous mode, one of three paths is selected:

```
                    cxf_optimize_internal
                           |
           +---------------+----------------+
           |               |                |
   [No callbacks      [Callbacks       [Normal
    + async mode]      registered]      synchronous]
           |               |                |
           v               v                v
    cxf_solve_no_    cxf_solve_with_    Continue to
    callbacks        callbacks         Phase 2.2+
    (P3.24)          (P3.24)
```

The no-callback path spawns a worker thread and monitors progress via a state tracker structure. The callback path acquires a callback synchronization lock and manages a callback communication channel. Both ultimately invoke the solver through the same dispatch mechanism; they differ only in threading and progress reporting infrastructure.

**Step 2.2: Lazy update flush.** Delegates to the Model Lifecycle module (P3.31) to apply all pending model modifications to the matrix data. This is the flush step of the lazy update pattern. The matrix data must be current before any solver algorithm can operate on it.

**Step 2.3: Model analysis.** Performs several diagnostic steps:
- Logs model dimensions (via Logging, P3.10)
- Computes model fingerprint (via Statistics & Diagnostics, P3.33)
- Logs presolve statistics (via Statistics & Diagnostics, P3.33)
- Analyzes coefficient ranges and issues numerical warnings (via Statistics & Diagnostics, P3.33)

**Step 2.4: Concurrent parameter management.** For concurrent optimization, caches and clamps tolerance parameters across all concurrent environments. These cached values are restored after optimization completes.

**State handed to Phase 3:** A model with current matrix data, completed analysis, and all pre-solve diagnostics logged.

### Phase 3: Solve Chain Entry (cxf_solve_entry)

This function routes between single-model and multi-scenario optimization.

**Step 3.1: Model type detection.** Determines the model type. For continuous models with solver focus or NLP mode, marks the model for special treatment.

**Step 3.2: Routing decision.**

```
                  cxf_solve_entry
                       |
            +----------+----------+
            |                     |
     [scenario count < 1]   [scenario count >= 1]
            |                     |
            v                     v
     cxf_solver_dispatch    cxf_solve_dispatch
     (P3.25)               (P3.24, multi-scenario)
                                  |
                                  v
                           Clone model for
                           scenario, solve via
                           cxf_solve_entry
                           (recursive), copy
                           results back
```

**Step 3.3: Non-convex QP handling.** If the solver returns a non-positive-semidefinite error for a continuous model, the non-convex handling parameter is consulted. If automatic conversion is enabled, the model is marked as non-convex and re-dispatched through the solver with non-convex treatment. This creates a retry loop within the solve chain.

**State handed to Phase 4:** A single model (or scenario clone) ready for algorithm dispatch.

### Phase 4: Algorithm Dispatch (cxf_solver_dispatch)

This is the central branching point where the solve flow splits by algorithm type.

**Step 4.1: Parameter backup.** Saves approximately 30 environment parameters to local storage. These span method selection, tolerances, threading, and heuristics. All are restored before return, regardless of outcome.

**Step 4.2: Model type detection and method selection.** Determines the model structure (LP, QP, SOCP) and selects the solving algorithm:

```
                cxf_solver_dispatch
                       |
        +--------------+--------------+
        |              |              |
     [LP/QP]        [SOCP]         [Non-convex QP]
        |              |              |
        v              v              v
  Method selection  Force barrier  Non-convex
  heuristic                        handling
        |
   +----+----+----+----+----+
   |    |    |    |    |    |
   v    v    v    v    v    v
  P.S  D.S  Bar  C3   C4   PDHG
  (0)  (1)  (2)  (3)  (4)   (6)
```

Method selection key:
- P.S = Primal Simplex, D.S = Dual Simplex, Bar = Barrier
- C3 = Concurrent (2 methods), C4 = Deterministic Concurrent
- PDHG = Primal-Dual Hybrid Gradient (first-order, LP only)

For automatic selection on LP models, a heuristic scoring procedure evaluates problem structure (sparsity, dimensions, constraint types, warm-start availability) to choose between simplex and barrier.

**Step 4.3: Solver dispatch.** Routes to the selected algorithm. Each algorithm path is described in the subsequent sections.

**Step 4.4: Solution extraction and result reporting.** After the solver returns, extracts statistics, uncrushed solutions (if presolved), recomputes the objective value for verification, and logs a detailed result summary.

**Step 4.5: Parameter restore.** Restores all backed-up parameters on both success and error paths.

### Phase 5a: Simplex Path

The simplex path is the most detailed algorithm branch, involving the deepest module nesting.

```
cxf_solver_dispatch
    |
    v
cxf_solve_lp (P3.25)
    |
    +--[Phase 1: Parameter extraction]
    |   Read solver parameters, save for later restoration
    |
    +--[Phase 2: Solver state initialization]
    |   Delegate to cxf_simplex_init (P3.22)
    |   -> Allocates SolverState with all working arrays
    |   -> Copies bounds, objective, constraint data from model
    |   -> Selects solve mode (primal/dual/auto)
    |   -> Computes work estimate for tolerance calibration
    |
    +--[Phase 3: Method selection]
    |   Priority chain: user override > simplex mode > concurrent indicator
    |   Default for pure LP: dual simplex (Maros, 2003)
    |
    +--[Phase 4: Crash basis construction]
    |   Delegate to cxf_simplex_crash (P3.21)
    |   -> Evaluates constraint feasibility and sparsity
    |   -> Assigns feasible rows to initial basis
    |   -> Reduces Phase I iteration count
    |
    +--[Phase 5: Crossover setup (if barrier solution present)]
    |   Delegate to cxf_crossover (P3.23)
    |   -> Processes quadratic variables analytically
    |   -> Linearizes binary variable quadratic terms
    |   Delegate to cxf_crossover_bounds (P3.23)
    |   -> Classifies variables by proximity to bounds
    |   -> Snaps near-bound variables to exact bounds
    |   -> Activates unrepresented constraints into basis
    |
    +--[Phase 6: TWO-LEVEL ITERATION LOOP]
    |   (see detailed diagram below)
    |
    +--[Phase 7: Piecewise-linear constraint processing]
    |   Breakpoint transitions, coefficient updates within loop
    |
    +--[Phase 8: Solution extraction]
    |   Copy solution from SolverState to model solution data
    |   Handle presolved model mapping
    |   Issue solution-found callbacks (P3.13)
    |
    +--[Phase 9: Cleanup and status mapping]
        Delegate to cxf_solver_state_cleanup (P3.22)
        Map internal status to public status codes
        Restore environment parameters
```

#### The Two-Level Iteration Loop (Phase 6 Detail)

This is the core of the LP solver. The two-level structure prevents cycling while ensuring convergence.

```
OUTER LOOP (round control, max ~5/10/100 rounds depending on mode)
  |
  +-- Take outer basis snapshot (cxf_basis_snapshot, P3.16)
  |
  +-- INNER LOOP (basis stabilization)
  |     |
  |     +--[1] cxf_basis_snapshot (P3.16)
  |     |       Capture current basis state
  |     |
  |     +--[2] cxf_simplex_iterate (P3.20)
  |     |       Progress logging and callback notification
  |     |       (Naming misnomer: does NOT perform iterations)
  |     |
  |     +--[3] cxf_simplex_phase_end (P3.21)
  |     |       Phase transition checks, constraint cleanup
  |     |
  |     +--[4] cxf_simplex_perturbation (P3.21)
  |     |       Anti-cycling perturbation if stalling detected
  |     |       (EXPAND procedure, Gill et al., 1989)
  |     |
  |     +--[5] cxf_simplex_step (P3.20)
  |     |       PRIMARY SIMPLEX PIVOT:
  |     |       -> cxf_pricing_candidates (P3.17) for entering variable
  |     |       -> Harris two-pass ratio test (P2.4) for leaving variable
  |     |       -> cxf_pivot_with_eta (P3.16) for basis update
  |     |       -> cxf_pricing_cascade_update (P3.18) for pricing refresh
  |     |
  |     +--[6] cxf_simplex_step2 (P3.20)
  |     |       Variable-side bound propagation
  |     |       -> cxf_pivot_update (P3.19) for activity bounds
  |     |       -> cxf_pivot_bound (P3.19) for fixed variables
  |     |
  |     +--[7] cxf_simplex_step3 (P3.20) [LP only, skipped for QP]
  |     |       Constraint-side bound propagation
  |     |       -> Implied bound derivation (Savelsbergh, 1994)
  |     |       -> cxf_pivot_update (P3.19) for activity bounds
  |     |
  |     +--[8] cxf_simplex_phase_end (P3.21) [second call]
  |     |       Post-pivot constraint cleanup
  |     |
  |     +--[9] cxf_basis_diff (P3.16)
  |     |       Compare basis to snapshot
  |     |       If change below threshold -> exit inner loop
  |     |
  |     +--[10] cxf_simplex_post_iterate (P3.20)
  |             Stall detection, termination checks, stagnation
  |             If termination condition -> exit both loops
  |
  +-- Check outer basis diff
  |   If insufficient progress -> exit outer loop
  |
  (repeat outer loop)
```

After the iteration loop terminates, the post-solve pipeline runs:

```
[Iteration loop terminates]
    |
    v
cxf_simplex_refine (P3.21)
    Fix non-basic variables at bounds based on reduced costs
    Recover basic variables near upper bounds
    |
    v
cxf_simplex_final (P3.22)
    Dual-feasibility-based variable fixing
    Complementary slackness analysis
    |
    v
cxf_simplex_cleanup (P3.22)
    Implied bound propagation (FBBT)
    -> Delegates to cxf_propagate_bounds (P3.34)
    Convert tight inequality constraints to equalities
    Free all temporary working arrays
    |
    v
cxf_solution_extract
    Copy solution from SolverState to model
```

### Phase 5b: Barrier Path

```
cxf_solver_dispatch
    |
    v
cxf_solve_barrier (P3.26)
    Validate Q matrix positive-semidefiniteness
    Linearize binary variable quadratic terms
    Apply diagonal adjustments if needed
    |
    v
[Interior-point iterations]
    (Internal to barrier subsystem)
    Produces interior-point solution
    |
    v
[Crossover enabled?]
    |           |
    [Yes]       [No]
    |           |
    v           v
cxf_solve_lp    Return barrier
(with barrier  solution directly
 solution in
 SolverState)
    |
    (Phases 5-9 of simplex path,
     starting with crossover setup)
```

When crossover is enabled (the default), the barrier solution is passed to the simplex pipeline, which performs crossover to convert the interior-point solution to a vertex solution, then runs simplex cleanup iterations.

### Phase 5c: Concurrent Path

```
cxf_solver_dispatch
    |
    +--[Local concurrent (methods 3,4,5)]
    |   cxf_solve_concurrent (P3.26)
    |   -> Clone model for each instance
    |   -> Assign diversified parameters (seed offsets)
    |   -> Spawn worker threads
    |   -> Poll for completion (first-wins)
    |   -> Select winner (objective-based or deterministic)
    |   -> Aggregate all solutions into pool
    |   -> Free clones, join threads
    |
    +--[Distributed concurrent LP]
    |   cxf_solve_concurrent_distributed (P3.26)
    |   -> Create worker environments on remote servers
    |   -> Assign methods: barrier / dual simplex / primal simplex
    |   -> Start async optimization on each
    |   -> Poll for first terminal status
    |   -> Extract basis from winner
    |   -> Apply basis to parent model
    |   -> Run simplex cleanup pass
    |
```

### Phase 6: Solution Processing

After any algorithm path completes, solution processing transforms raw solver output into user-accessible results.

```
[Solver algorithm returns]
    |
    +--[Presolved model exists?]
    |       |
    |      [Yes] -> cxf_uncrush_solution (P3.29)
    |       |       Map solution from reduced to original space
    |       |
    |      [No] -> Use solution directly
    |
    +--[Compute objective value]
    |   cxf_scale_objval (P3.29)
    |   Evaluate linear + quadratic + PWL terms
    |   Account for column scaling and global objective scaling
    |   Add objective constant
    |
    +--[Wire result attributes]
    |       |
    |   +---+---+
    |   |       |
    |  [LP]   [General]
    |   |       |
    |   v       v
    |  cxf_process_lp_solution    cxf_wire_result_attributes
    |  (P3.29)                   (P3.29)
    |  Wire iteration counts,    Wire iteration counts,
    |  node counts, objective    node counts, solution
    |  (status-dependent)        arrays (X, Slack, QCSlack),
    |                            compute optimality gap via
    |                            cxf_compute_gap (P3.29)
```

### Phase 7: Cleanup and Result Delivery

The cleanup phase reverses all setup operations performed in Phase 1, restoring the model and environment to their pre-optimization state.

```
[Solution processing complete]
    |
    +--[Restore environment parameters]
    |   ~30 parameters restored from backup (in cxf_solver_dispatch)
    |   LP-specific parameters restored (in cxf_solve_lp)
    |
    +--[Concurrent parameter restore]
    |   Cached tolerance values restored to concurrent environments
    |   (in cxf_optimize_internal)
    |
    +--[Solver focus and fingerprint restore]
    |   (in cxf_solve_entry and cxf_optimize_internal)
    |
    +--[Log callback statistics]
    |   If callbacks were used: log invocation count and cumulative time
    |   (in cxf_optimize)
    |
    +--[Restore signal handler]
    |   cxf_cleanup_optimization (P3.34)
    |   Restore previous SIGINT handler
    |   Clear module-level model reference
    |
    +--[Write result files]
    |   If result file path configured and status is optimal/suboptimal:
    |   write solution file (or compute IIS first for IIS file formats)
    |   (in cxf_optimize)
    |
    +--[Clear modification-blocked flag]
    |   Re-enable model modifications
    |   (in cxf_optimize)
    |
    +--[Release locale safety]
    |   cxf_release_solve_lock (P3.11)
    |   Restore calling thread's locale
    |   (in cxf_optimize)
    |
    +--[Clear optimization-active flag]
        (in cxf_optimize)
```

## State Transitions

### Model Status Lifecycle

The model's optimization status transitions through a well-defined lifecycle during the pipeline:

```
LOADED (initial)
    |
    v
[cxf_optimize clears status]
    |
    v
SOLVING (internal, not user-visible)
    |
    +---> OPTIMAL          (feasible and optimal within tolerances)
    +---> INFEASIBLE       (no feasible solution exists)
    +---> INF_OR_UNBD      (cannot distinguish after presolve)
    +---> UNBOUNDED        (objective improvable without bound)
    +---> ITERATION_LIMIT  (iteration limit reached, best solution if any)
    +---> TIME_LIMIT       (time limit reached, best solution if any)
    +---> INTERRUPTED      (user abort via signal or callback)
    +---> NUMERIC          (numerical difficulties, solution unreliable)
    +---> SUBOPTIMAL       (feasible but not proven optimal)
    +---> CUTOFF           (objective worse than cutoff value)
```

**Status mapping note:** When solving with a presolved (working) matrix, cxf_solve_lp may swap INFEASIBLE and UNBOUNDED statuses, because presolve transformations can interchange these conditions. For crossover-origin solves, an ambiguous result is conservatively mapped to INF_OR_UNBD.

### Model State Flags During Pipeline

| Flag | Set When | Cleared When | Purpose |
|------|----------|-------------|---------|
| Modification-blocked | Phase 1 entry | Phase 7 cleanup | Prevent concurrent model changes |
| Optimization-active | Phase 1 entry | Phase 7 cleanup | Environment-level activity marker |
| Signal-handler-active | Phase 1 (signal setup) | Phase 7 (signal restore) | Track custom signal handler |
| Solver-execution | Phase 3 (solve entry) | Phase 3 (solve entry cleanup) | Mark active solver mode |

### SolverState Lifecycle

The SolverState is the central mutable structure during simplex solving. Its lifecycle is entirely contained within Phase 5a:

```
[Does not exist]
    |
    v  cxf_simplex_init (P3.22)
[Allocated and populated]
    |
    v  Crash, preprocess, setup, factorize
[Ready for iteration]
    |
    v  Iteration loop (step/step2/step3)
[Solution found or limit reached]
    |
    v  Refine, final, cleanup
[Post-processed]
    |
    v  cxf_solver_state_cleanup (P3.22)
[Deallocated]
```

No module outside the simplex subsystem (P3.20, P3.21, P3.22, P3.25) accesses the SolverState directly. The only external handoff is the solution data, which is copied from the SolverState to the model's solution arrays during solution extraction.

### Parameter Backup/Restore Boundaries

Parameters are backed up and restored at two nested levels:

```
cxf_solver_dispatch:  ~30 parameters (method, tolerances, cuts, branching,
                      threading, heuristics, presolve settings)
    |
    +-- cxf_solve_lp:  Small set of LP-specific parameters (iteration limits,
                       tolerances, mode flags)
```

Both levels restore on all exit paths. This two-level backup is necessary because cxf_solve_lp may be called multiple times within a single cxf_solver_dispatch invocation (e.g., for concurrent solver instances or crossover cleanup passes).

## Error Handling

### Error Propagation Architecture

Errors propagate upward through the call chain in a consistent pattern:

```
cxf_optimize
    |  catches all errors, sets error message, performs cleanup
    v
cxf_optimize_internal
    |  propagates errors from sub-functions
    v
cxf_solve_entry
    |  propagates errors, handles non-convex QP retry
    v
cxf_solver_dispatch
    |  propagates errors, ensures parameter restore on all paths
    v
cxf_solve_lp / cxf_solve_barrier / cxf_solve_concurrent
    |  propagates errors from sub-functions
    v
Simplex iteration functions / Barrier subsystem
    |  return error codes for specific conditions
    v
Leaf functions (pricing, pivot, eta allocation, etc.)
    return specific error codes
```

### Error Propagation Rules

1. **Cascading propagation.** Each layer checks the return code of every sub-function call. On non-zero return, control transfers to a cleanup section that deallocates any allocated state, restores any saved parameters, and propagates the error code upward.

2. **First-error preservation.** The Error Handling module (P3.09) implements a "first error wins" policy. When errors cascade, the root-cause error message is preserved in the environment's error buffer. The error buffer lock (managed by lifecycle callbacks in P3.13) prevents outer functions from overwriting the inner function's more specific error message.

3. **Error vs. status distinction.** Iteration limit, time limit, and numeric difficulty are communicated via the output status parameter, NOT the return code. A zero return code with a non-optimal status indicates successful (but non-optimal) termination. Only genuine errors (out-of-memory, invalid arguments, Q-not-PSD) produce non-zero return codes.

4. **Resource safety.** All cleanup paths are idempotent (null-safe). Partial initialization is handled correctly: any non-null pointer can be safely freed. Parameters are always restored from backup, regardless of success or failure.

### Error Categories and Their Propagation Paths

| Error | Origin | Propagation | Recovery |
|-------|--------|-------------|----------|
| Out-of-memory | Any allocation | Propagates to cxf_optimize, sets "exhausted available memory" message | None; solve aborted |
| Q-not-PSD | cxf_solve_barrier | May trigger non-convex QP retry in cxf_solve_entry | Apply non-convex handling if NonConvex enabled |
| Infeasibility (in iteration) | step/step2/step3 | Sets INFEASIBLE status, zero return | Status communicated to user |
| Unboundedness (in iteration) | step | Sets UNBOUNDED status, zero return | Status communicated to user |
| User interrupt | post_iterate or callback | Sets INTERRUPTED status | Graceful termination with best solution |
| Model validation failure | cxf_optimize entry | Returns immediately with error code | User must fix model |
| Label validation failure | cxf_solve_entry | Propagates to cxf_optimize | User must fix labels |
| Concurrent setup failure | cxf_solve_concurrent | Returns with INTERRUPTED status and error | User must adjust thread/config |

### Callback Error Handling

The callback path (cxf_solve_with_callbacks) implements a specialized error recovery sequence:

1. After the solve completes, the callback result structure is examined.
2. Out-of-memory errors return immediately.
3. User interrupt is reported through the error system with the interrupt code.
4. Other errors trigger a polling-based recovery: wait for the callback thread to complete, send a result request message to retrieve detailed error information, and report the error with the detailed message.

## Configuration

### Parameters Affecting the Pipeline Flow

| Parameter | Effect on Flow | Where Consulted |
|-----------|---------------|-----------------|
| Method | Selects solving algorithm (-1=auto, 0=primal, 1=dual, 2=barrier, 3-5=concurrent, 6=PDHG) | cxf_solver_dispatch Phase 4 |
| SimplexPricing | Selects pricing strategy within simplex | cxf_simplex_init Phase 3 |
| Crossover | Controls barrier-to-simplex crossover (0=off, -1=auto) | cxf_solve_lp Phase 5 |
| NonConvex | Controls non-convex QP handling (0=error, -1=auto, 2=always) | cxf_solve_entry Phase 3 |
| IterationLimit | Maximum simplex iterations | cxf_simplex_post_iterate |
| TimeLimit | Maximum wall-clock time | cxf_simplex_post_iterate |
| ConcurrentMethod | Method selection for concurrent instances | cxf_solver_dispatch Phase 4 |
| ConcurrentJobs | Number of concurrent instances | cxf_solver_dispatch Phase 4 |
| Threads | Thread count for parallel operations | cxf_optimize Phase 1, concurrent |
| Presolve | Controls presolve aggressiveness | cxf_solver_dispatch Phase 8 |
| OutputFlag | Controls logging verbosity | cxf_optimize Phase 1 |
| ResultFile | Triggers post-solve result file writing | cxf_optimize Phase 7 |
| NumericFocus | Controls numerical precision emphasis | cxf_coefficient_stats |
| PoolSolutions | Maximum solution pool size | cxf_copy_solution |
| PoolGap/PoolGapAbs | Solution pool quality threshold | cxf_copy_solution |
| FeasibilityTol | Primal feasibility tolerance | Throughout simplex |
| OptimalityTol | Dual feasibility tolerance | Throughout simplex |
| MarkowitzTol | Basis factorization stability control | cxf_solver_dispatch |

### Parameters Backed Up and Restored

cxf_solver_dispatch backs up approximately 30 parameters spanning:
- Method selection and algorithm control
- Tolerances (feasibility, optimality, Markowitz)
- Heuristic settings
- Thread counts
- Presolve settings

cxf_solve_lp backs up a smaller set of LP-specific parameters:
- Iteration limits
- Tolerance scaling factors
- Mode flags

## Design Decisions

### 1. Layered Call Chain with Clear Separation of Concerns

The optimization pipeline uses a strict layered architecture where each layer adds its specific concerns and delegates downward. No layer skips an intermediate layer. This ensures that all necessary initialization and cleanup occurs in the correct order:

| Layer | Function | Primary Concern |
|-------|----------|----------------|
| Public API | cxf_optimize | Validation, locale, lifecycle |
| Internal Dispatch | cxf_optimize_internal | Path selection, model analysis |
| Solve Chain | cxf_solve_entry | Type detection, scenario routing |
| Algorithm Router | cxf_solver_dispatch | Method selection, presolve cycle |
| Algorithm | cxf_solve_lp / barrier / concurrent | Algorithm execution |

**Rationale:** This layered design follows the standard practice for commercial optimization software (Maros, 2003). Each layer can be reasoned about independently, and the setup/teardown guarantees are local to each layer.

### 2. Three Execution Paths

The optimization system supports three execution paths (normal, callback, no-callback), selected at the cxf_optimize_internal level. This branching occurs early in the pipeline because the threading and communication infrastructure differs fundamentally between paths.

**Rationale:** Separating the paths allows the normal synchronous case to avoid the overhead of callback communication channels and thread synchronization. The callback path provides the full callback experience for remote solver and interactive use. The no-callback fast path provides efficient asynchronous operation without user callback overhead.

### 3. Two-Level Iteration Loop

The simplex solver uses a two-level loop (inner for basis stabilization, outer for round control) rather than a single loop with a simple iteration limit.

**Rationale:** The two-level structure addresses the convergence challenges of the simplex method on degenerate problems. The inner loop detects when the basis has stabilized (no further progress), and the outer loop decides whether restarting is worthwhile. The adaptive convergence threshold (scaled by iteration count) prevents both premature termination and infinite cycling. This approach is related to long-step/short-step pivot strategies (Maros, 2003, Chapter 10).

### 4. Parameter Backup/Restore Pattern

Both cxf_solver_dispatch and cxf_solve_lp save and restore environment parameters around their execution.

**Rationale:** Solver sub-functions may modify environment parameters during the solve (e.g., adjusting tolerances for numerical difficulty, overriding method selection for sub-problems). The backup/restore pattern ensures the user's original parameter settings are never permanently modified by optimization. This is critical for users who call optimize repeatedly on the same model.

### 5. Presolve-Solve-Uncrush Cycle

The optimization pipeline processes presolved models by creating a reduced copy, solving the reduced copy, and then mapping the solution back to the original variable space.

**Rationale:** Presolve reductions (variable fixing, constraint elimination, bound tightening) dramatically reduce solve time for many practical problems. The uncrush step ensures that the user receives a solution in their original variable space, regardless of the internal transformations applied. This three-phase pattern is standard in modern LP solvers (Achterberg et al., 2020).

### 6. Lazy Update Flush at Solve Entry

Pending model modifications are flushed (applied in batch) at the beginning of optimization, not at the point of each individual modification.

**Rationale:** The lazy update pattern amortizes the cost of rebuilding internal data structures. Flushing at solve entry ensures the matrix data is current when the solver needs it, while avoiding unnecessary rebuilds during model construction. The flush occurs at two points (cxf_optimize_internal and cxf_solve_entry) because cxf_solve_entry can be called recursively through the multi-scenario path.

### 7. Attribute Wiring Pattern for Solution Access

Solution data is made available to users through a direct-pointer wiring pattern rather than function-call dispatch.

**Rationale:** After each solve, attribute entries in the model's attribute table are linked directly to the memory locations where result values reside. This enables attribute queries to return values by simple pointer dereference, without the overhead of getter function dispatch. The wiring must be reconfigured after each solve because storage locations may differ depending on the solve outcome.

### 8. Concurrent Solving as Algorithm Portfolio

Concurrent methods create independent model copies and race multiple algorithms or parameter configurations in parallel, selecting the first good result.

**Rationale:** The best algorithm for a given problem instance is often unknown a priori. The portfolio approach (Rice, 1976; Xu et al., 2008) hedges this uncertainty by running multiple strategies simultaneously. Each instance operates on an independent model copy, avoiding fine-grained locking and ensuring thread safety through isolation rather than synchronization.

---

## Verification Checklist

```
[x] No hex addresses or offsets
[x] No Ghidra artifacts (param_, local_, iVar, DAT_, FUN_)
[x] No binary-specific constants or magic numbers
[x] No copied code fragments from analyzed source
[x] All descriptions are behavioral, not implementational
[x] All data structures described semantically
[x] All algorithms cite published sources where applicable
[x] Focus on cross-module integration, not internal module behavior
[x] Passes the Clean Room Test: could be written without seeing the binary
```

## References

- Achterberg, T., Bixby, R.E., Gu, Z., Rothberg, E., and Weninger, D. (2020). "Presolve Reductions in Mixed Integer Programming." *INFORMS Journal on Computing*, 32(2):473-506.
- Bixby, R.E. (2002). "Solving Real-World Linear Programs: A Decade and More of Progress." *Operations Research*, 50(1):3-15.
- Bixby, R.E. and Saltzman, M.J. (1994). "Recovering an Optimal LP Basis from an Interior Point Solution." *Operations Research Letters*, 15(4):169-178.
- Dantzig, G.B. (1963). *Linear Programming and Extensions*. Princeton University Press.
- Gill, P.E., Murray, W., Saunders, M.A., and Wright, M.H. (1989). "A practical anti-cycling procedure for linearly constrained optimization." *Mathematical Programming*, 45(1-3):437-474.
- Gould, N.I.M. and Reid, J.K. (1989). "New crash procedures for large systems of linear constraints." *Mathematical Programming*, 45(1-3):475-501.
- Maros, I. (2003). *Computational Techniques of the Simplex Method*. Springer. International Series in Operations Research and Management Science, Vol. 61.
- Megiddo, N. (1991). "On Finding Primal- and Dual-Optimal Bases." *ORSA Journal on Computing*, 3(1):63-65.
- Nocedal, J. and Wright, S.J. (2006). *Numerical Optimization*, 2nd edition. Springer.
- Rice, J.R. (1976). "The Algorithm Selection Problem." *Advances in Computers*, 15:65-118.
- Savelsbergh, M.W.P. (1994). "Preprocessing and Probing Techniques for Mixed Integer Programming Problems." *ORSA Journal on Computing*, 6(4):445-454.
- Xu, L., Hutter, F., Hoos, H.H., and Leyton-Brown, K. (2008). "SATzilla: Portfolio-based Algorithm Selection for SAT." *Journal of Artificial Intelligence Research*, 32:565-606.
- ConvexFeld Optimization, LLC. *ConvexFeld Optimizer Reference Manual* (public API documentation). cxf_optimize, Method parameter, Crossover parameter, NonConvex parameter, concurrent optimization, callback interface.
