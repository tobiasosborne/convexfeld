# Implementation Order Guide

## Purpose

This document provides a practical, staged implementation plan for building an LP solver from the cleanroom specification corpus. It is designed for an engineering team starting from scratch: it tells you what to build first, what to build next, what you can test at each stage, and what external components you will need to supply.

The ordering is driven by two principles:

1. **Dependencies flow upward.** Low-level infrastructure (memory, errors, data structures) must exist before high-level orchestration (solve pipeline, lifecycle) can be built.
2. **Testability at every stage.** Each stage produces something that can be tested in isolation, so defects are caught early before they propagate into higher layers.

The guide references spec IDs throughout. Each ID maps to a specific file in the `specs/` directory tree:

- **P1.xx** -- Data Model layer (`specs/data-model/`)
- **P2.x** -- Algorithm layer (`specs/algorithms/`)
- **P3.xx** -- Module Contracts layer (`specs/modules/`)
- **P4.x** -- Integration layer (`specs/integration/`)
- **P5.x** -- Reference layer (`specs/reference/`)

---

## Recommended Implementation Stages

### Stage 1: Foundation -- Memory and Error Infrastructure

**Specs to implement:**
- P3.01 -- Memory Primitives (`memory_primitives.md`)
- P3.09 -- Error Handling (`error_handling.md`)
- P3.10 -- Logging (`logging.md`)
- P5.1 -- Error & Status Codes (`error_status_codes.md`)

**What you build:**
- `cxf_calloc`, `cxf_realloc`, `cxf_vector_free`, `cxf_model_alloc` -- the memory allocation layer with optional tracking, memory limits, and custom allocator support.
- `cxf_error_env`, `cxf_error_model`, `cxf_set_error_message`, `cxf_env_set_status` -- the error reporting system with first-error preservation and locked-buffer semantics.
- `cxf_log`, `cxf_errorlog`, `cxf_register_log_callback` -- logging output with configurable verbosity.

**What you gain:**
Every subsequent module depends on being able to allocate memory, report errors, and produce log output. These three capabilities are the bedrock of the entire solver.

**What you can test:**
- Allocate and free memory blocks of various sizes; verify zero-initialization.
- Verify memory limit enforcement: allocations that exceed the configured limit return null.
- Verify error message formatting and first-error preservation (set an error, then set another; the first should be retained when the buffer is locked).
- Verify log output appears on stdout or through a registered callback.
- Stress-test allocation and deallocation sequences to verify no memory leaks.

---

### Stage 2: Core Data Structures

**Specs to implement:**
- P1.01 -- Environment (`environment.md`)
- P1.02 -- Model (`model.md`)
- P1.03 -- MatrixData (`matrix_data.md`)
- P1.10 -- Supporting Structures (`supporting_structures.md`)
- P5.2 -- Parameters & Defaults (`parameters_defaults.md`)

**What you build:**
The three fundamental structures of the solver: Environment (global configuration, licensing placeholder, parameter storage, threading state), Model (problem data container with validity sentinels, modification control, attribute system), and MatrixData (CSR/CSC sparse matrix storage for the constraint matrix). Also the supporting structures (IISState, ModificationTracker, WarmStartData) and the parameter system with default values.

**What you gain:**
The ability to create an environment, create a model within it, populate the model with problem data (variables, constraints, bounds, objective coefficients), and store the constraint matrix in both row-major and column-major sparse formats.

**What you can test:**
- Create and destroy Environment and Model instances; verify validity sentinels.
- Set and query parameters on the environment; verify defaults match P5.2.
- Populate a small LP (e.g., 3 variables, 2 constraints) and verify all fields.
- Verify CSR and CSC representations are consistent (same nonzero count, same values accessible by row or by column).
- Test the modification-blocked flag: model modifications should be rejected while the flag is set.
- Verify the lazy-update pending buffer accepts and stores modifications.

---

### Stage 3: Matrix Operations and Validation

**Specs to implement:**
- P3.14 -- Matrix Core (`matrix_core.md`)
- P3.15 -- Matrix Finalization (`matrix_finalization.md`)
- P3.06 -- Model Type Checking (`model_type_checking.md`)
- P3.07 -- Input Validation (`input_validation.md`)
- P3.08 -- Data Validation (`data_validation.md`)

**What you build:**
- `cxf_matrix_setup`, `cxf_prepare_row_data`, `cxf_build_row_major`, `cxf_sort_indices` -- functions to construct and finalize the internal sparse matrix representation from user-provided data.
- `cxf_finalize_row_data` (6-part pipeline) -- the complete matrix finalization sequence.
- Model type detection (`cxf_is_quadratic`, `cxf_is_socp`, etc.) and input/data validation functions.

**What you gain:**
The ability to take raw user input (variable bounds, constraint coefficients, objective) and produce the finalized internal matrix representation that all solver algorithms consume. Plus a comprehensive validation layer that catches malformed inputs before they reach the solver.

**What you can test:**
- Build matrices from hand-crafted test cases and verify CSR/CSC correctness.
- Test index sorting on unsorted input.
- Verify NaN/infinity detection catches invalid coefficients.
- Verify model type detection correctly classifies LP, QP, and SOCP models.
- Test label validation with valid and invalid string inputs.
- Verify that finalization produces row-major data consistent with the column-major input.
- Run on standard small LP test cases (e.g., the Netlib afiro problem has 27 rows and 32 columns).

---

### Stage 4: Basis System and Eta Vectors

**Specs to implement:**
- P1.05 -- BasisState (`basis_state.md`)
- P1.08 -- EtaVector (`eta_vector.md`)
- P2.2 -- Product Form of Inverse (`product_form_inverse.md`)
- P3.16 -- Basis Operations (`basis_operations.md`)
- P3.02 -- Allocation Helpers (`allocation_helpers.md`)

**What you build:**
- The BasisState structure with LU factorization storage, eta vector chain, memory pool, and refactorization control.
- All three EtaVector variants (PIVOT, VARIABLE_FIX, WARM_START) with sparse storage.
- The PFI algorithm for FTRAN and BTRAN operations.
- `cxf_pivot_with_eta` -- the central function that creates an eta vector to record a basis pivot.
- `cxf_basis_refactor` -- variable-fixing based on reduced cost analysis.
- `cxf_basis_snapshot` and `cxf_basis_diff` -- for convergence detection in the iteration loop.
- `cxf_basis_warm` -- warm-start eta creation for reoptimization.
- `cxf_alloc_eta`, `cxf_alloc_work_arrays`, `cxf_setup_resources` -- allocation helpers for the basis system and work arrays.

**External dependency required: Sparse LU factorization library.**
The PFI system requires a periodic full LU factorization of the basis matrix to maintain numerical stability. This factorization is not specified in the cleanroom specs (it is outside the scope of the 158 analyzed functions). You must either:
- Use an existing sparse LU library (LUSOL by Gill/Murray/Saunders, SuiteSparse by Davis, or the LU factorization routines from LAPACK/SuperLU), or
- Implement sparse LU factorization from published literature: Bartels and Golub (1969), Forrest and Tomlin (1972), Reid (1982), Suhl and Suhl (1990), or Maros (2003) Chapter 5.

The LU factorization must support:
- Sparse LU decomposition of the basis matrix B = LU with row and column permutations.
- Forward solve (FTRAN): solve Lx = b, then Ux = y.
- Backward solve (BTRAN): solve U^T x = b, then L^T x = y.
- Threshold pivoting (Markowitz criterion) to balance sparsity and numerical stability.

**What you gain:**
A working basis representation that can track simplex pivots via eta vectors, solve the FTRAN/BTRAN systems needed for pricing and ratio tests, detect convergence via basis snapshots, and maintain numerical accuracy through periodic refactorization.

**What you can test:**
- Create an identity basis for a small LP and verify FTRAN/BTRAN produce correct results.
- Perform a sequence of pivots and verify that the eta chain correctly represents the basis inverse (compare FTRAN results against a dense matrix inverse computed externally).
- Verify that basis snapshots capture the current state and that basis_diff correctly detects changes.
- Verify eta vector memory pool allocation and deallocation.
- Test refactorization: after accumulating many eta vectors, trigger refactorization and verify that FTRAN/BTRAN results are unchanged (within numerical tolerance).

**Milestone: Numerical correctness of the basis system.** This is the single most important verification point before proceeding to the simplex iteration engine. If the basis system has numerical bugs, every subsequent stage will produce wrong answers.

---

### Stage 5: Pricing System

**Specs to implement:**
- P1.07 -- PricingState (`pricing_state.md`)
- P2.3 -- Multi-Level Partial Pricing (`partial_pricing.md`)
- P3.17 -- Pricing Core (`pricing_core.md`)
- P3.18 -- Pricing Support (`pricing_support.md`)

**What you build:**
- The PricingState structure with its mirror-symmetric queue subsystems, flag encoding, and level management.
- The multi-level partial pricing algorithm: a producer-consumer architecture where pricing candidates (variables with attractive reduced costs) are identified, queued, and retrieved for the ratio test.
- `cxf_pricing_candidates` -- the main candidate selection function with adaptive strategy selection (full scan vs. partial expansion, threshold checks).
- `cxf_pricing_update`, `cxf_pricing_update_var`, `cxf_pricing_update_constr` -- pricing data update after pivots.
- `cxf_pricing_invalidate` -- cache invalidation.
- `cxf_pricing_mark_dirty`, `cxf_pricing_mark_constr_dirty` -- stale-marking for incremental updates.
- `cxf_pricing_cascade_update` -- cascading update after pivots that affect multiple candidates.
- `cxf_pricing_end_level`, `cxf_pricing_set_level` -- level management for multi-level pricing.
- `cxf_pricing_get_var_stats`, `cxf_pricing_get_constr_stats`, `cxf_pricing_get_constr_candidates` -- statistics and retrieval.

**What you gain:**
The ability to efficiently select entering variables for the simplex method. The pricing system is the performance-critical bottleneck of the simplex method on large problems; getting it right determines whether the solver can handle problems with thousands of variables.

**What you can test:**
- Initialize pricing state for a small LP and verify that candidates with violated reduced costs are correctly identified.
- Verify the flag encoding (4 active bits per element for 2 levels) stores and retrieves status correctly.
- Verify that after a pivot, pricing updates correctly mark affected candidates as dirty and re-evaluate them.
- Verify queue management: candidates are correctly split into committed/pending, and retrieval returns them in the expected order.
- Test the adaptive strategy: verify that full scan is triggered when coverage is low and partial expansion is used when the candidate pool is adequate.
- Benchmark pricing on medium-sized problems (hundreds of variables) to verify acceptable performance.

---

### Stage 6: Pivot Operations and Ratio Test

**Specs to implement:**
- P2.4 -- Harris Ratio Test + BFRT (`harris_ratio_test.md`)
- P3.19 -- Pivot Operations (`pivot_operations.md`)

**What you build:**
- The Harris two-pass ratio test: a relaxed ratio test that tolerates small feasibility violations to avoid degenerate pivots, followed by a bound-flipping ratio test (BFRT) that exploits finite bound ranges to make long steps.
- `cxf_pivot_check` -- step length computation with constraint-by-constraint analysis.
- `cxf_pivot_bound` -- the 7-phase variable-fixing pipeline (flag evaluation, linear/quadratic objective processing, Q-neighbor linearization, pricing notification, activity propagation, matrix cleanup).
- `cxf_pivot_primal` -- primal pivot with criterion selection.
- `cxf_pivot_special` -- special-case pivot handling based on direction/bound/cost combinations.
- `cxf_pivot_update` -- post-pivot state update with cancellation detection.

**What you gain:**
The ability to execute a complete simplex pivot: given an entering variable from the pricing system, determine the leaving variable via the ratio test, compute step lengths, update variable values, fix variables at bounds when appropriate, and propagate activity bound changes.

**What you can test:**
- Construct a small LP at a non-optimal vertex and verify that the ratio test correctly identifies the leaving variable.
- Verify that the Harris relaxation accepts near-degenerate pivots that a strict ratio test would reject.
- Verify bound-flipping: when a variable has finite lower and upper bounds, the BFRT should allow a longer step by flipping variables between bounds.
- Test variable fixing: when reduced cost analysis determines a variable is optimally at its bound, verify it is correctly fixed.
- Verify cancellation detection: when subtracting nearly equal quantities during pivot_update, verify the conservative rounding logic activates.
- Execute a sequence of pricing-pivot cycles on a small LP and verify each pivot moves toward the optimum.

---

### Stage 7: Simplex Iteration Engine

**Specs to implement:**
- P1.04 -- SolverState (`solver_state.md`)
- P1.09 -- WorkArrays (`work_arrays.md`)
- P2.1 -- Revised Simplex Method (`revised_simplex.md`)
- P2.8 -- Bound Propagation (`bound_propagation.md`)
- P3.20 -- Simplex Iteration (`simplex_iteration.md`)
- P3.03 -- State Initialization (`state_initialization.md`)
- P3.04 -- State Cleanup: Solver (`state_cleanup_solver.md`)

**What you build:**
- The SolverState structure -- the central mutable state container for the simplex solver, holding problem dimensions, solve configuration, iteration control, basis tracking arrays, CSR/CSC matrix copies, working bounds, reduced costs, and all control parameters.
- The WorkArrays structure for temporary computation buffers.
- The 10-step inner iteration loop described in the optimization pipeline integration spec (P4.1):
  1. `cxf_basis_snapshot` -- capture current basis state
  2. `cxf_simplex_iterate` -- progress logging and callback notification
  3. `cxf_simplex_phase_end` -- phase transition checks (first call)
  4. `cxf_simplex_perturbation` -- anti-cycling if stalling
  5. `cxf_simplex_step` -- primary simplex pivot (pricing + ratio test + basis update)
  6. `cxf_simplex_step2` -- variable-side bound propagation
  7. `cxf_simplex_step3` -- constraint-side bound propagation (LP only)
  8. `cxf_simplex_phase_end` -- post-pivot cleanup (second call)
  9. `cxf_basis_diff` -- convergence detection
  10. `cxf_simplex_post_iterate` -- stall detection, termination checks
- State initialization (`cxf_init_solve_state`, `cxf_setup_basis`, `cxf_setup_work_arrays`) to create the SolverState from model data.
- State cleanup (`cxf_cleanup_solve_state`, `cxf_free_solver_state`, `cxf_free_basis_state`) to tear down the SolverState.

**What you gain:**
A complete simplex iteration engine that can solve LP problems. This is the first stage where you have an end-to-end solver: given a prepared SolverState with an initial basis, the iteration loop will execute simplex pivots until optimality, infeasibility, unboundedness, or an iteration limit is reached.

**What you can test:**
- **Solve small LP test problems.** The Netlib test set (publicly available) provides standard benchmarks:
  - `afiro`: 27 rows, 32 columns -- should solve in under 50 iterations.
  - `adlittle`: 56 rows, 97 columns -- should solve in under 200 iterations.
  - `blend`: 74 rows, 83 columns -- a good test for degeneracy handling.
  - `sc50a`: 50 rows, 48 columns -- a simple problem that tests basic correctness.
- Verify that optimal objective values match known solutions (published in Netlib documentation).
- Verify that infeasible problems are correctly detected (construct an LP with contradictory constraints).
- Verify that unbounded problems are correctly detected (construct an LP with no lower bound on the objective).
- Verify iteration counts are reasonable (within a factor of 2-3 of published benchmarks).
- Verify that the iteration limit terminates the solver correctly and reports the appropriate status.
- Test warm-starting: solve an LP, modify a bound or objective coefficient, and resolve from the previous basis.

**Milestone: First LP solves.** This is the most significant milestone in the entire project. Once you can solve Netlib afiro correctly, the core algorithm is working.

---

### Stage 8: Simplex Lifecycle and Phases

**Specs to implement:**
- P2.5 -- Crash Basis Construction (`crash_basis.md`)
- P2.6 -- Perturbation / Anti-Cycling (`perturbation.md`)
- P3.21 -- Simplex Phases (`simplex_phases.md`)
- P3.22 -- Simplex Lifecycle (`simplex_lifecycle.md`)
- P3.34 -- Cleanup Utilities (`cleanup_utilities.md`)

**What you build:**
- `cxf_simplex_init` (4-part pipeline) -- the full simplex initialization sequence: allocate SolverState, copy model data, select solve mode, compute work estimates, size and allocate all working arrays.
- `cxf_simplex_crash` -- crash basis construction using the algorithm from P2.5 (evaluation of constraint feasibility and sparsity to assign feasible rows to the initial basis, reducing Phase I iterations).
- `cxf_simplex_perturbation` -- the EXPAND anti-cycling procedure (Gill et al., 1989) that applies controlled perturbations to bounds when the solver detects stalling due to degeneracy.
- `cxf_simplex_preprocess` -- pre-solve preprocessing within the simplex context.
- `cxf_simplex_setup` -- simplex method configuration.
- `cxf_simplex_phase_end` -- phase transition handling (Phase I to Phase II transition when feasibility is achieved).
- `cxf_simplex_refine` -- post-solve refinement (fix non-basic variables at bounds based on reduced costs, recover basic variables near upper bounds).
- `cxf_simplex_final` -- final result processing (dual-feasibility-based variable fixing, complementary slackness analysis).
- `cxf_simplex_cleanup` -- implied bound propagation (FBBT) via `cxf_propagate_bounds`, constraint tightening, working array deallocation.
- `cxf_cleanup_helper`, `cxf_cleanup_coeff_change`, `cxf_cleanup_optimization`, `cxf_propagate_bounds` -- cleanup utilities.

**What you gain:**
The complete simplex lifecycle from initialization through crash basis, iteration, post-processing, and cleanup. The crash basis dramatically reduces Phase I iteration counts on practical problems. The anti-cycling perturbation ensures convergence on degenerate problems. The post-solve refinement and cleanup produce polished solutions.

**What you can test:**
- Verify crash basis reduces iteration count vs. a slack-variable starting basis (test on Netlib problems; crash should reduce iterations by 20-50% on many problems).
- Verify anti-cycling: construct a known cycling LP (Beale's cycling example) and verify the perturbation procedure prevents infinite looping.
- Verify Phase I to Phase II transition: construct an LP where the initial basis is infeasible; verify Phase I finds feasibility and Phase II optimizes.
- Verify solution refinement improves objective value (should be within feasibility tolerance of the true optimum).
- Verify cleanup correctly frees all allocated memory (run under a memory leak detector).
- Test the full init-crash-iterate-refine-cleanup pipeline on medium Netlib problems:
  - `bandm`: 305 rows, 472 columns
  - `e226`: 223 rows, 282 columns
  - `israel`: 174 rows, 142 columns

---

### Stage 9: LP Solve Pipeline

**Specs to implement:**
- P3.25 -- Solve LP Core (`solve_lp_core.md`)
- P3.29 -- Solution Processing (`solution_processing.md`)
- P3.05 -- State Cleanup: Buffers (`state_cleanup_buffers.md`)
- P3.33 -- Statistics & Diagnostics (`statistics_diagnostics.md`)
- P5.3 -- Tolerances & Constants (`tolerances_constants.md`)

**What you build:**
- `cxf_solve_lp` (6-part pipeline) -- the complete LP solve orchestration: parameter extraction, solver state initialization, method selection, crash basis, the two-level iteration loop (inner for basis stabilization, outer for round control), piecewise-linear constraint processing, solution extraction, and status mapping.
- `cxf_solver_dispatch` (6-part pipeline) -- algorithm routing: LP/QP/SOCP detection, method selection heuristic, the presolve-solve-uncrush cycle, parameter backup/restore, and result reporting.
- Solution processing: `cxf_process_lp_solution`, `cxf_uncrush_solution`, `cxf_wire_result_attributes`, `cxf_compute_gap`, `cxf_scale_objval`, `cxf_copy_solution`.
- Statistics and diagnostics: `cxf_presolve_stats`, `cxf_coefficient_stats`, `cxf_compute_coef_stats`, `cxf_compute_violations`, `cxf_compute_fingerprint`, `cxf_get_timestamp`.
- Buffer cleanup: `cxf_free_callback_state`, `cxf_free_solution_pool`, `cxf_clear_solution`, `cxf_clear_pending_buffer`, `cxf_reset_pending_buffer`.

**External dependency required: Presolve system.**
The presolve-solve-uncrush cycle in `cxf_solver_dispatch` relies on a presolve subsystem that is not fully specified in this corpus. The presolve system performs variable elimination, constraint aggregation, bound tightening, and redundancy removal to create a reduced problem. You must either:
- Implement presolve from published literature: Andersen and Andersen (1995) "Presolving in Linear Programming."
- Use an existing open-source presolve implementation (e.g., from HiGHS or GLPK).
- Initially stub the presolve system to pass through the original problem unchanged, and add presolve reductions incrementally.

**What you gain:**
A complete LP solver pipeline that handles the full flow: parameter management, algorithm selection, initialization, solving, solution processing, attribute wiring, and cleanup. The solution processing stage transforms raw solver output into user-accessible result attributes.

**What you can test:**
- Solve the full Netlib LP test set (about 100 problems) through the complete pipeline and verify objective values match published solutions.
- Verify parameter backup/restore: solve an LP, verify all environment parameters are unchanged after the solve completes.
- Verify solution attribute wiring: after a solve, query primal values, dual values, reduced costs, slack values, and objective value through the attribute system.
- Verify coefficient statistics and warnings: construct a poorly scaled LP and verify appropriate warnings are logged.
- Test iteration limit and time limit handling through the pipeline.
- Verify the two-level iteration loop: the inner loop should stabilize the basis, and the outer loop should control round-to-round progress.

**Milestone: Complete LP solver.** After this stage, you have a solver capable of handling production LP problems through a proper API-like interface. Validate on Netlib benchmarks; all should solve correctly.

---

### Stage 10: Public API Entry Chain

**Specs to implement:**
- P3.24 -- Solve Entry & Dispatch (`solve_entry.md`)
- P3.31 -- Model Lifecycle (`model_lifecycle.md`)
- P3.32 -- Optimization Preparation (`optimization_preparation.md`)
- P3.30 -- Environment Lifecycle (`environment_lifecycle.md`)
- P3.13 -- Callbacks (`callbacks.md`)
- P3.11 -- Threading & Synchronization (`threading_sync.md`)
- P3.12 -- Thread Init & Thunks (`thread_init_thunks.md`)
- P1.06 -- CallbackState (`callback_state.md`)
- P4.1 -- Optimization Pipeline (`optimization_pipeline.md`)
- P4.2 -- Threading Model (`threading_model.md`)
- P4.3 -- Error Propagation (`error_propagation.md`)
- P4.4 -- Callback Protocol (`callback_protocol.md`)
- P4.5 -- Parameter System (`parameter_system.md`)

**What you build:**
- The full public API entry chain: `cxf_optimize` -> `cxf_optimize_internal` -> `cxf_solve_entry` -> `cxf_solver_dispatch`.
- Three execution paths: normal synchronous, callback (with callback communication channel), and no-callback asynchronous (with worker thread).
- Model lifecycle: `cxf_model_create_internal`, `cxf_env_model_cleanup`, `cxf_update_model_manager`, `cxf_model_apply_modifications` (lazy update flush).
- Environment lifecycle: `cxf_env_create_internal`, `cxf_env_free_internal`, `cxf_env_finalize` (8-part licensing pipeline), `cxf_env_load_logfile`, `cxf_env_update_active_model`.
- Optimization preparation: signal handler installation, remote solver delegation.
- Callbacks: `cxf_init_callback_struct`, `cxf_callback_terminate`, `cxf_pre_optimize_callback`, `cxf_post_optimize_callback`, `cxf_getconstrs_callback`, `cxf_copy_env_callbacks`.
- Threading: locale safety, solve lock acquisition/release, CPU detection, thread count management.

**What you gain:**
A fully featured solver with a public API surface: environment creation, model creation with lazy updates, parameter configuration, callback registration, and the complete optimize call with validation, locale safety, signal handling, lifecycle hooks, and cleanup. This is the full user-facing solver.

**What you can test:**
- Test the complete public API flow: create environment, create model, add variables and constraints, call optimize, query results.
- Verify model validation: invalid model pointers, null arguments, and concurrent modification attempts should all be caught and reported.
- Verify locale safety: the solver should produce consistent numeric output regardless of the calling thread's locale.
- Verify signal handling: send SIGINT during a long solve and verify graceful interruption.
- Verify lazy update flush: add variables, add constraints, and verify that modifications are applied before the solve begins.
- Verify callback invocation: register a callback and verify it is called with correct progress information during the solve.
- Test the no-callback asynchronous path: start a solve, verify the calling thread is not blocked, and verify results are available when the solve completes.
- Verify error propagation: errors from deep within the solve chain should propagate to the cxf_optimize return code with an appropriate error message.

---

### Stage 11: Crossover (Barrier-to-Simplex Conversion)

**Specs to implement:**
- P2.7 -- Crossover Algorithm (`crossover.md`)
- P3.23 -- Crossover Module (`crossover.md`)

**What you build:**
- `cxf_crossover` -- the driver function for barrier-to-simplex crossover, including quadratic variable processing and binary variable linearization.
- `cxf_crossover_bounds` (4-part pipeline) -- the main crossover function that classifies variables by proximity to bounds, snaps near-bound variables to exact bounds, activates unrepresented constraints into the basis, and produces a basic feasible solution from an interior-point solution.

**What you gain:**
The ability to convert an interior-point (barrier) solution to a vertex (simplex) solution. Crossover is essential for post-barrier cleanup when users need an exact vertex solution, which is the default behavior when barrier is used as the algorithm.

**What you can test:**
- Start with a known interior-point solution (e.g., computed by an external barrier solver or constructed analytically for a small LP) and verify that crossover produces a vertex solution with the same or better objective value.
- Verify bound snapping: variables within tolerance of a bound should be exactly at the bound after crossover.
- Verify that the resulting basis is valid (correct number of basic variables, basis matrix is nonsingular).
- Test on problems where crossover is known to be challenging: highly degenerate LPs where many variables are near bounds.

**References:** Megiddo (1991), Bixby and Saltzman (1994).

---

### Stage 12: Barrier Entry and Concurrent Solving

**Specs to implement:**
- P3.26 -- Solve Barrier & Concurrent (`solve_barrier_concurrent.md`)

**External dependency required: Interior-point method (IPM) implementation.**
The core barrier/interior-point algorithm (predictor-corrector Mehrotra steps, Cholesky factorization of the normal equations, centering parameter selection) is not specified in this corpus. You must either:
- Implement an IPM from published literature: Mehrotra (1992) "On the Implementation of a Primal-Dual Interior Point Method," Gondzio (1996) "Multiple centrality corrections in a primal-dual method for linear programming," Wright (1997) *Primal-Dual Interior-Point Methods*.
- Use an existing open-source IPM library.
- Skip barrier entirely and rely on simplex only (a fully functional LP solver does not require barrier).

**What you build:**
- `cxf_solve_barrier` -- barrier entry point: Q-matrix PSD validation, binary variable linearization, delegation to the IPM implementation, and routing to crossover (Stage 11) when crossover is enabled.
- `cxf_solve_concurrent` (6-part pipeline) -- concurrent optimization: model cloning, parameter diversification, worker thread spawning, first-wins polling, winner selection, and solution aggregation.
- `cxf_solve_concurrent_distributed` -- distributed concurrent solving across remote solvers.

**What you gain:**
Two additional algorithm paths beyond simplex: barrier (interior-point) for large sparse problems, and concurrent (algorithm portfolio) for hedging between methods on problems where the best algorithm is unknown.

**What you can test:**
- If an IPM is available: solve an LP via barrier, verify the objective matches the simplex solution, verify crossover produces a valid vertex solution.
- Concurrent solving: configure method=3 (concurrent) and verify the solver runs multiple methods and returns a correct solution.
- Verify that concurrent model clones are fully independent (no shared mutable state).
- Verify parameter diversification: each concurrent instance should use a different random seed.

---

## Dependency Notes

### Hard Dependencies (MUST Be Implemented First)

The following dependency chains are strict. Each item requires all items above it to be functional.

```
Memory Primitives (P3.01)
    |
    v
Error Handling (P3.09) + Logging (P3.10)
    |
    v
Core Data Structures (P1.01-P1.03)
    |
    v
Matrix Operations (P3.14-P3.15)
    |
    v
Basis System (P1.05, P1.08, P3.16)
    |
    v
Pricing System (P1.07, P3.17-P3.18)
    |
    v
Pivot Operations (P3.19)
    |
    v
Simplex Iteration (P3.20) + SolverState (P1.04)
    |
    v
Simplex Lifecycle (P3.21-P3.22)
    |
    v
Solve LP Core (P3.25) + Solution Processing (P3.29)
    |
    v
Solve Entry Chain (P3.24)
```

### Modules That Can Be Stubbed Initially

The following modules can be deferred or stubbed without blocking the core LP solver:

| Module | Stub Behavior | When to Implement |
|--------|---------------|-------------------|
| P3.13 -- Callbacks | No-op callback functions; skip callback invocation | Stage 10, when the public API is built |
| P3.11 -- Threading | Single-threaded stub; no-op lock functions | Stage 10, when concurrent solving is needed |
| P3.23 -- Crossover | Skip crossover; simplex-only operation | Stage 11, when barrier support is added |
| P3.26 -- Barrier & Concurrent | Return "method not supported" | Stage 12, when barrier/concurrent are needed |
| P3.32 -- Optimization Preparation | Skip signal handler and remote solver delegation | Stage 10 |
| P3.33 -- Statistics | No-op diagnostic functions; skip fingerprinting | Stage 9, when the pipeline is complete |
| P3.34 -- Cleanup Utilities (partially) | Stub signal handler restoration; implement bound propagation for Stage 8 | Stage 10 for full cleanup |

### External Components Required

| Component | When Needed | Published References | Open-Source Options |
|-----------|-------------|---------------------|---------------------|
| Sparse LU factorization | Stage 4 (Basis System) | Bartels & Golub (1969), Forrest & Tomlin (1972), Reid (1982), Maros (2003) Ch. 5 | LUSOL, SuiteSparse, SuperLU |
| Presolve system | Stage 9 (LP Pipeline) | Andersen & Andersen (1995), Achterberg et al. (2020) | HiGHS presolve, GLPK presolve |
| Interior-point method | Stage 12 (Barrier) | Mehrotra (1992), Gondzio (1996), Wright (1997) | OOQP, HiGHS IPM |

**Note on presolve stubbing:** The LP solver can operate without presolve by passing the problem through unchanged. Presolve improves performance (often dramatically on large problems) but is not required for correctness. It is recommended to stub presolve initially and add it when the core solver is validated.

---

## Milestone Testing Points

### After Stage 4 -- Basis System Verification

**Goal:** Confirm that the basis representation is numerically correct.

**Tests:**
- For a known LP with a known optimal basis, construct the BasisState and verify that FTRAN(e_i) produces the correct column of B^{-1}.
- Perform 50+ sequential pivots and verify that the accumulated eta chain still produces correct FTRAN/BTRAN results (compare against dense inverse).
- Trigger a refactorization and verify results are unchanged within tolerance (1e-10 or tighter).
- Verify that numerical accuracy degrades gracefully as the eta chain grows, and that refactorization restores full accuracy.

**Failure at this point means:** Stop and fix the basis system before proceeding. All subsequent stages depend on it.

### After Stage 7 -- First LP Solves

**Goal:** Solve small LP problems correctly.

**Tests:**
- Solve Netlib `afiro` (optimal objective: approximately -464.7531428571) and verify the objective matches to 6 significant digits.
- Solve Netlib `adlittle` (optimal objective: approximately 225494.96316) and verify.
- Solve at least 5 additional small Netlib problems and verify all optimal objectives.
- Verify that infeasible problems return INFEASIBLE status.
- Verify that unbounded problems return UNBOUNDED status.

**Failure at this point means:** Debug the iteration engine. Check pricing (is the correct entering variable selected?), ratio test (is the correct leaving variable selected?), and pivot update (are variable values correctly updated?). Use a dense simplex implementation as a reference oracle.

**Recommended validation tool:** Compare results against GLPK or HiGHS on the same problems. Both are open-source LP solvers with Netlib test results available.

### After Stage 9 -- Full LP Solver

**Goal:** Solve the complete Netlib LP test suite through the full pipeline.

**Tests:**
- Solve all ~100 Netlib LP problems. All should return OPTIMAL status with objectives matching published values to at least 6 significant digits.
- Verify that the solver handles edge cases: empty problems, single-variable problems, fixed-variable problems, problems with free variables (infinite bounds in both directions).
- Performance benchmark: solve times should be within an order of magnitude of GLPK for the same problems (the solver may be slower due to lack of presolve and other optimizations, but should not be pathologically slow).
- Memory test: solve the largest Netlib problems (e.g., `pilot87`, `maros-r7`) and verify no memory leaks and reasonable memory usage.

**Failure at this point means:** The pipeline orchestration has a bug. Check parameter backup/restore, solution extraction, attribute wiring, and status mapping. The iteration engine (Stage 7) should already be correct at this point, so the issue is likely in the wrapping logic.

### After Stage 10 -- Complete Solver

**Goal:** A fully featured LP solver with a public API.

**Tests:**
- Run the full Netlib suite through the public API (environment creation, model construction, optimize, result query).
- Verify callback functionality: register a callback that logs iteration counts and verify the logged values are monotonically increasing.
- Verify concurrent solving: configure 2 concurrent methods and verify both produce the same optimal objective.
- Stress test: solve 1000 LPs in sequence from the same environment, verifying no resource leaks.
- Thread safety test: solve independent LPs from multiple threads simultaneously, each with its own model and environment.

---

## Summary: What to Build and When

| Stage | Name | Key Deliverable | Spec Count | Cumulative Capability |
|-------|------|----------------|------------|----------------------|
| 1 | Foundation | Memory + Error + Logging | 4 | Basic infrastructure |
| 2 | Data Structures | Environment + Model + Matrix | 5 | Problem representation |
| 3 | Matrix & Validation | Matrix ops + Validation | 5 | Finalized matrix, input checking |
| 4 | Basis System | PFI + Eta + Basis ops | 5 | Basis representation and updates |
| 5 | Pricing | Pricing state + Partial pricing | 4 | Entering variable selection |
| 6 | Pivots | Ratio test + Pivot ops | 2 | Complete simplex pivot |
| 7 | Iteration Engine | SolverState + Iteration loop | 7 | **Solve small LPs** |
| 8 | Lifecycle & Phases | Crash + Perturbation + Lifecycle | 5 | Solve full LPs efficiently |
| 9 | LP Pipeline | Solve LP core + Solution processing | 5 | **Complete LP solver** |
| 10 | Public API | Entry chain + Callbacks + Threading | 13 | Full user-facing solver |
| 11 | Crossover | Barrier-to-simplex conversion | 2 | Post-barrier cleanup |
| 12 | Barrier & Concurrent | IPM entry + Concurrent racing | 1 | Alternative algorithms |

**Total:** 58 spec files across 12 implementation stages.

---

## Verification Checklist

```
[x] No hex addresses or offsets
[x] No binary-specific constants or magic numbers
[x] No Ghidra artifacts (param_, local_, iVar, DAT_, FUN_)
[x] No copied code fragments from analyzed source
[x] All references are to published literature or public documentation
[x] Passes the Clean Room Test
```

## References

- Andersen, E.D. and Andersen, K.D. (1995). "Presolving in Linear Programming." *Mathematical Programming*, 71(2):221-245.
- Bartels, R.H. and Golub, G.H. (1969). "The Simplex Method of Linear Programming Using LU Decomposition." *Communications of the ACM*, 12(5):266-268.
- Bixby, R.E. and Saltzman, M.J. (1994). "Recovering an Optimal LP Basis from an Interior Point Solution." *Operations Research Letters*, 15(4):169-178.
- Dantzig, G.B. (1963). *Linear Programming and Extensions*. Princeton University Press.
- Forrest, J.J.H. and Goldfarb, D. (1992). "Steepest-edge simplex algorithms for linear programming." *Mathematical Programming*, 57(1):341-374.
- Forrest, J.J.H. and Tomlin, J.A. (1972). "Updated triangular factors of the basis to maintain sparsity in the product form simplex method." *Mathematical Programming*, 2(1):263-278.
- Gill, P.E., Murray, W., Saunders, M.A., and Wright, M.H. (1989). "A practical anti-cycling procedure for linearly constrained optimization." *Mathematical Programming*, 45(1-3):437-474.
- Goldfarb, D. and Reid, J.K. (1977). "A practicable steepest-edge simplex algorithm." *Mathematical Programming*, 12(1):361-371.
- Gondzio, J. (1996). "Multiple centrality corrections in a primal-dual method for linear programming." *Computational Optimization and Applications*, 6(2):137-156.
- Harris, P.M.J. (1973). "Pivot selection methods of the Devex LP code." *Mathematical Programming*, 5(1):1-28.
- Maros, I. (2003). *Computational Techniques of the Simplex Method*. Springer.
- Megiddo, N. (1991). "On Finding Primal- and Dual-Optimal Bases." *ORSA Journal on Computing*, 3(1):63-65.
- Mehrotra, S. (1992). "On the Implementation of a Primal-Dual Interior Point Method." *SIAM Journal on Optimization*, 2(4):575-601.
- Reid, J.K. (1982). "A sparsity-exploiting variant of the Bartels-Golub decomposition for linear programming bases." *Mathematical Programming*, 24(1):55-69.
- Suhl, U.H. and Suhl, L.M. (1990). "Computing sparse LU factorizations for large-scale linear programming bases." *ORSA Journal on Computing*, 2(4):325-335.
- Wright, S.J. (1997). *Primal-Dual Interior-Point Methods*. SIAM.
