# Parameter System

## Overview

This document describes how solver parameters flow through the system: from initial default values established at environment creation, through layered overrides from configuration files, configuration files, and programmatic settings, to the point where solver modules read parameter values to control algorithmic behavior. The parameter system is the primary mechanism through which users configure every aspect of solver behavior -- algorithm selection, termination criteria, numerical tolerances, logging, threading, and more.

Understanding the parameter system is essential for reimplementation because parameters are not simply stored and retrieved. They participate in a multi-layered inheritance model (environment to model), undergo validation and clamping at multiple points in the solve lifecycle, are temporarily modified and restored by solver internals during optimization, and interact with each other to determine algorithmic behavior. This document captures those integration dynamics.

---

## Components Involved

The parameter system spans the following modules and data structures:

| Component | Spec Reference | Role in Parameter System |
|-----------|---------------|--------------------------|
| Environment (data model) | P1.01 | Owns the parameter table, parameter storage, and per-parameter flags |
| Model (data model) | P1.02 | References an environment (shared or private child) for parameter access |
| Environment Lifecycle | P3.30 | Creates parameter table, sets defaults, applies config overrides during finalization |
| Solve Entry & Dispatch | P3.24 | Reads parameters for execution path selection, concurrent parameter management |
| Solve LP Core | P3.25 | Reads method, tolerances, limits; backs up and restores ~30 parameters around solve |
| Simplex Lifecycle | P3.22 | Copies tolerances, iteration limits, and mode parameters into SolverState at init time |
| Simplex Phases | P3.21 | Reads feasibility tolerance, perturbation parameters |
| Pricing Core | P3.17 | Reads pricing strategy parameter indirectly via SolverState mode |
| Solve Barrier & Concurrent | P3.26 | Reads barrier parameters, thread counts, concurrent method settings |
| Multi-Objective & Scenario | P3.28 | Reads multi-objective settings; may reset solution pool parameters |
| Threading & Synchronization | P3.11 | Reads Threads parameter, hardware limits |
| Parameters & Defaults (reference) | P5.2 | Catalogs all parameters, types, defaults, and valid ranges |
| Tolerances & Constants (reference) | P5.3 | Documents numerical tolerances and their algorithmic roles |

---

## Flow Description

### Phase 1: Parameter Table Construction (Environment Creation)

When an environment is created (P3.30 cxf_env_create_internal), the parameter system is initialized through the following steps:

1. **Static definition scan.** The parameter table is built by scanning a static definition table that contains every parameter's name, data type (int, double, or string), default value, minimum bound, maximum bound, and flags. This table is compiled into the solver library and defines the complete set of recognized parameters.

2. **Entry allocation.** A parameter entry array and a per-parameter flags array are allocated. Each entry stores the parameter's metadata and its current value (initialized to the static default).

3. **Name registration.** Each parameter name (converted to uppercase) is registered in a lookup structure that enables efficient name-based access. This supports the public API pattern where parameters are identified by name strings.

4. **Parent inheritance.** If the environment is created as a child of an existing environment, current parameter values are inherited from the parent's parameter table instead of using the static defaults. This is the mechanism by which model-level environments inherit from the session environment.

**Result:** An environment with all parameters set to their defaults (or inherited values), ready for further configuration.

### Phase 2: Parameter Overrides During Finalization

When the environment is finalized (P3.30 cxf_env_finalize), parameters undergo a layered override process. The order of application determines precedence, with later layers overriding earlier ones:

**Layer 1 -- Built-in defaults.** Already established during creation (Phase 1).

**Layer 2 -- Configuration file parameters.** If the configuration file loading flag is set, the optional solver configuration file (typically in the current working directory) is loaded and its parameter settings are applied. This allows per-project parameter customization without code changes.

**Layer 3 -- Programmatic settings.** The finalization process uses a snapshot/restore mechanism to preserve any parameter values that were set programmatically between environment creation and finalization. Specifically, the environment state is snapshotted at the start of finalization, and after config file parameters are applied, the programmatic settings are restored from the snapshot. This ensures that explicit API calls always take precedence over file-based settings.

**Additional overrides during finalization:**
- System environment variables override core counts and memory limits (CXF_CORES, CXF_PHYSICALCORES, CXF_MAXCORES, CXF_MEMLIMIT).
**Result:** An active environment with all parameter layers resolved. The final parameter values reflect the precedence: programmatic > config file > defaults.

### Phase 3: Model-Level Parameter Inheritance

When a model is created from an environment, parameter values flow from environment to model through one of two mechanisms:

1. **Shared environment.** The model directly references the creating environment. Parameter reads go to the environment's parameter table. Changes to the environment's parameters affect the model, and vice versa. This is the simpler case but provides no per-model isolation.

2. **Private child environment.** The model receives a private child environment, created by inheriting parameter values from the parent environment (using the parent-inheritance mechanism from Phase 1). The child environment's parameter table is independent: changes to the child do not affect the parent, and changes to the parent do not retroactively propagate to the child. This provides per-model parameter isolation.

The choice between shared and private environments depends on the creation path. Models created for concurrent optimization or multi-objective optimization receive private child environments to allow per-instance parameter customization. The public API also supports explicit creation of child environments for user-controlled parameter isolation.

**Key invariant:** Model-level parameter changes never propagate back to the parent environment. The inheritance is a one-time copy at creation time, not a live binding.

### Phase 4: Parameter Reading at Solve Time

When optimization begins (P3.24 cxf_optimize through cxf_solver_dispatch in P3.25), parameters are read at several points in the call chain:

**4a. Entry validation and logging (P3.24 cxf_optimize).**
- OutputFlag is read to determine whether to log version and hardware information.
- ResultFile is read to determine whether to write result files after optimization.
- The thread count is computed by reconciling the Threads parameter and hardware detection (P3.11 cxf_get_threads).

**4b. Model analysis and path selection (P3.24 cxf_optimize_internal).**
- The callback count and async mode determine the execution path (normal, callback, or no-callback fast path).
- For concurrent optimization: tolerance parameters from all concurrent environments are cached and clamped to safe ranges before optimization begins. The cached values are restored after optimization, ensuring that the user's original settings are preserved.

**4c. Algorithm dispatch (P3.25 cxf_solver_dispatch).**
- Approximately 30 parameters are backed up to local storage at the start of dispatch. These span method selection, tolerances, thread counts, and algorithm tuning. All are restored before the function returns, regardless of success or failure.
- **Method** determines the solving algorithm (simplex, barrier, concurrent, PDHG).
- **NonConvex** controls handling of non-convex quadratic programs.
- **ConcurrentMethod**, **Threads** affect concurrent solver configuration.
- **Presolve** and related parameters control the presolve-solve-uncrush cycle.
- Problem structure and warm-start availability influence automatic method selection when Method is set to AUTO (-1).

**4d. LP solve pipeline (P3.25 cxf_solve_lp).**
- A subset of parameters are saved for restoration after the solve.
- **Method** and **SimplexPricing** determine the simplex variant and pricing strategy.
- **IterationLimit**, **TimeLimit**, and **WorkLimit** control termination.
- **Crossover** parameters control barrier-to-simplex crossover behavior.

**4e. Solver state initialization (P3.22 cxf_simplex_init).**
- Parameters are copied from the environment into the SolverState structure. This is the point where environment-level parameters become solver-internal state:
  - Iteration limits are copied.
  - Tolerances (feasibility, optimality) are copied.
  - The solve mode is selected based on the simplex method parameter and problem characteristics.
  - The pricing strategy parameter influences mode selection.
  - Quad precision, perturbation value, and other algorithmic tuning parameters are transferred.
- Once copied into the SolverState, the solver operates on these local copies. This isolation is critical: it allows the solver to adjust tolerances internally (e.g., tightening near optimality) without modifying the environment.

### Phase 5: Parameter Restoration After Solve

Both cxf_solve_lp and cxf_solver_dispatch implement a parameter backup/restore pattern:

1. Before the solve begins, a snapshot of relevant parameters is taken.
2. During the solve, sub-functions may modify parameters (e.g., adjusting tolerances for numerical recovery, changing method selection for sub-problems, modifying cut aggressiveness).
3. After the solve completes (on both success and error paths), all backed-up parameters are restored to their pre-solve values.

This restore pattern is essential for correctness when the user calls cxf_optimize repeatedly on the same model. Without it, internal solver modifications would leak into the user's parameter state.

For concurrent optimization, the restore extends to all concurrent environments: tolerance parameters cached before optimization are restored after optimization completes, ensuring no concurrent environment retains solver-internal modifications.

---

## State Transitions

### Parameter Value Lifecycle

```
[Static Defaults]
      |
      v
[Environment Creation]  -- defaults or inherited values
      |
      v
[Programmatic Settings] -- user API calls before finalization
      |
      v
[Environment Finalization]
      |  Config file overrides (Layer 2)
      |  Programmatic settings restored (Layer 3, highest precedence)
      |  System env var overrides for hardware/memory
      v
[Active Environment]  -- final resolved values
      |
      +--[Shared with Model]  -- model reads directly
      |
      +--[Child Env Created for Model]  -- one-time copy at model creation
            |
            v
      [Model-Level Overrides]  -- user changes per-model parameters
            |
            v
      [Solve-Time Backup]  -- ~30 params saved before dispatch
            |
            v
      [Solver-Internal Modifications]  -- tolerances, methods adjusted
            |
            v
      [Solve-Time Restore]  -- backed-up params restored after solve
```

### Parameter State During Optimization

During optimization, the parameter system exists in a temporarily modified state:

| Phase | Parameter State | Who Modifies | Restored When |
|-------|----------------|-------------|---------------|
| Pre-dispatch backup | ~30 params saved to local storage | cxf_solver_dispatch | After dispatch returns |
| LP-specific backup | LP params saved | cxf_solve_lp | After LP solve returns |
| SolverState copy | Tolerances/limits copied to SolverState | cxf_simplex_init | N/A (SolverState is destroyed) |
| Concurrent clamping | Tolerance params on concurrent envs clamped | cxf_optimize_internal | After optimization returns |

---

## Error Handling

### Parameter Validation

Parameter validation occurs at multiple points:

1. **At set time.** When a parameter is set via the public API, the value is checked against the parameter's minimum and maximum bounds (as defined in the parameter table). Out-of-range values are rejected with an INVALID_ARGUMENT error.

2. **At finalization.** During environment finalization, certain parameters are validated against system capabilities:
   - Hardware-dependent parameters (core counts, memory limits) are reconciled with detected capabilities.

3. **At solve time.** During solver dispatch, parameters are checked for consistency with the model type:
   - PDHG method requested for a QP model generates a warning and method adjustment.
   - Concurrent methods are restricted for SOCP and certain QP models.
   - NonConvex parameter determines whether non-PSD Q matrix errors are treated as terminal.

### Parameter-Related Error Codes

| Situation | Error Code | Description |
|-----------|-----------|-------------|
| Invalid parameter name | UNKNOWN_PARAMETER | Parameter name not found in the lookup structure |
| Value out of range | INVALID_ARGUMENT | Value violates the parameter's min/max bounds |
| Wrong type | INVALID_ARGUMENT | Attempt to set a double value on an int parameter, etc. |
| Unsupported method for model type | Warning + adjustment | Method adjusted to a compatible selection |

### Restore-on-Error Guarantee

All parameter backup/restore mechanisms operate on both success and error paths. This is a critical invariant: even if the solver encounters an out-of-memory error, a numerical failure, or a user interrupt, the environment's parameters are restored to their pre-solve values. The restore code uses idempotent patterns (null-safe deallocation, unconditional assignment) to ensure correctness regardless of the error's origin.

---

## Configuration

### Parameter Categories and Their Solver Impact

Parameters are organized into functional categories, each affecting different aspects of the solve:

#### Method Selection Parameters

| Parameter | Read By | Effect |
|-----------|---------|--------|
| Method | cxf_solver_dispatch (P3.25) | Selects the root LP algorithm: simplex, barrier, concurrent, or PDHG |
| SiftMethod | Simplex subsystem | Selects the LP algorithm for sifting sub-problems |
| SimplexPricing | cxf_simplex_init (P3.22) | Selects the variable pricing strategy (partial, steepest edge, Devex) |
| Crossover | cxf_solve_lp (P3.25) | Controls barrier-to-simplex crossover strategy |
| ConcurrentMethod | cxf_solver_dispatch (P3.25) | Controls which algorithms run concurrently |

#### Tolerance Parameters

| Parameter | Read By | Effect |
|-----------|---------|--------|
| FeasibilityTol | cxf_simplex_init (P3.22), crash (P3.21), perturbation (P3.21), cleanup (P3.22) | Primal constraint satisfaction threshold |
| OptimalityTol | cxf_simplex_init (P3.22), pricing (P3.17) | Dual feasibility / reduced cost threshold for optimality declaration |
| MarkowitzTol | Basis factorization (P3.16) | Pivot selection stability/sparsity trade-off |
| BarConvTol | Barrier solver (P3.26) | Interior-point convergence threshold |
| PerturbValue | cxf_simplex_perturbation (P3.21) | Anti-cycling perturbation magnitude |

#### Termination Parameters

| Parameter | Read By | Effect |
|-----------|---------|--------|
| IterationLimit | cxf_simplex_init (P3.22), cxf_solve_lp (P3.25) | Maximum simplex iterations |
| TimeLimit | Solve entry (P3.24), all solver modules | Wall-clock time bound |
| WorkLimit | Solve entry (P3.24), all solver modules | Deterministic work bound |
| BarIterLimit | Barrier solver (P3.26) | Maximum barrier iterations |

#### Output and Logging Parameters

| Parameter | Read By | Effect |
|-----------|---------|--------|
| OutputFlag | cxf_optimize (P3.24), logging subsystem (P3.10) | Master output suppression switch |
| LogToConsole | Logging subsystem (P3.10) | Console output control |
| LogFile | cxf_env_load_logfile (P3.30), logging subsystem | Log file path |
| DisplayInterval | Logging subsystem (P3.10) | Log line frequency |

#### Threading Parameters

| Parameter | Read By | Effect |
|-----------|---------|--------|
| Threads | cxf_get_threads (P3.11), cxf_solver_dispatch (P3.25) | Parallel thread count |
| InheritParams | Concurrent/multi-obj subsystems | Whether sub-environments inherit parent parameters |

#### Algorithmic Tuning Parameters

| Parameter | Read By | Effect |
|-----------|---------|--------|
| Quad | cxf_simplex_init (P3.22) | Quad precision in simplex computations |
| NormAdjust | Simplex pricing subsystem | Pricing norm selection |
| Sifting | Simplex subsystem | Sifting within dual simplex |
| NetworkAlg | Simplex subsystem | Network structure exploitation |
| DegenMoves | Simplex subsystem | Degenerate move limit |
| NumericFocus | cxf_solver_dispatch (P3.25), simplex/barrier | Extra numerical care level |
| ScaleFlag | Matrix scaling subsystem | Model coefficient scaling strategy |
| Seed | Multiple modules | Random number seed for perturbation and tie-breaking |

### Module-Parameter Matrix

The following table maps each major solver module to the parameter categories it reads:

| Module | Method | Tolerances | Limits | Output | Threading | Tuning | Presolve |
|--------|--------|-----------|--------|--------|-----------|--------|----------|
| Solve Entry (P3.24) | | | x | x | x | | |
| Solver Dispatch (P3.25) | x | x | x | x | x | x | x |
| Solve LP (P3.25) | x | x | x | | | x | |
| Simplex Init (P3.22) | x | x | x | | | x | |
| Simplex Phases (P3.21) | | x | | | | x | |
| Pricing Core (P3.17) | | x | | | | | |
| Basis Factorization (P3.16) | | x | | | | | |
| Barrier (P3.26) | x | x | x | | x | | |
| Concurrent (P3.26) | x | x | | | x | | |
| Multi-Objective (P3.28) | | | | | | | |
| Threading (P3.11) | | | | | x | | |
| Env Lifecycle (P3.30) | | | | x | x | | |

---

## Design Decisions

### Two-Level Inheritance Model

The parameter system uses a two-level inheritance model: environment-level defaults and model-level overrides. This design was chosen over alternatives (such as per-solve parameter sets or a deep inheritance hierarchy) for several reasons:

1. **Simplicity.** Two levels are sufficient for the vast majority of use cases: set organization-wide defaults on the environment, and override per-model as needed.

2. **Isolation.** Private child environments provide clean per-model isolation without the complexity of multi-level scoping. Each model sees a flat parameter namespace, not a chain of overrides.

3. **Consistency with public API.** The public API exposes parameters as simple name-value pairs on either the environment or the model. The two-level model maps directly to this API surface.

### Parameter Backup and Restore Pattern

The solver internally modifies parameters during optimization (adjusting tolerances, overriding methods, changing cut aggressiveness). Rather than threading these modifications through function parameters, the solver modifies the environment's parameter table directly and relies on a backup/restore pattern to ensure the user's values are preserved.

This design was chosen because:

1. **Deep call chains.** The solve chain is many layers deep (cxf_optimize through cxf_simplex_step). Threading parameter overrides through every layer would require adding parameters to dozens of function signatures.

2. **Dynamic adjustment.** The solver may decide to adjust parameters based on runtime observations (e.g., tightening tolerances after detecting numerical instability). These decisions occur at arbitrary points in the call chain and cannot be predicted at dispatch time.

3. **Simplicity for sub-functions.** Every sub-function can simply read parameters from the environment without knowing whether they reflect user values or solver-internal overrides. The restore at the top level ensures correctness.

The trade-off is that parameter state is not purely functional: there is a window during optimization where the environment holds modified values. This is acceptable because the modification-blocked flag on the model prevents user code from observing the modified state during optimization.

### Tolerance Copying into SolverState

Tolerances are copied from the environment into the SolverState during simplex initialization (P3.22), rather than being read from the environment on each use. This design provides:

1. **Performance.** Reading a field from a local structure is faster than a parameter table lookup by name.

2. **Isolation.** The solver can adjust tolerances internally (e.g., using adaptive pricing tolerance phases) without affecting the environment.

3. **Snapshot semantics.** The SolverState captures the tolerances as they were at solve start. Even if the environment's parameters are restored mid-solve by a nested backup/restore cycle, the solver sees consistent values.

### Concurrent Environment Parameter Clamping

Before concurrent optimization, tolerance parameters on all concurrent environments are cached and clamped to safe ranges. This ensures that:

1. **No concurrent instance operates with extreme tolerances** that could produce unreliable results.

2. **The user's original values are preserved** through the cache/restore mechanism.

3. **Result quality is comparable across instances**, avoiding the situation where one instance declares "optimal" with loose tolerances while another is still iterating with tight tolerances.

The clamping is applied to tolerance parameters specifically because they directly affect convergence criteria and solution quality. Other parameter categories (method selection, limits) are not clamped because they do not pose numerical safety concerns.

### The AUTO Default Convention

Many parameters use -1 as a default value meaning "automatic" -- the solver selects the best strategy based on model characteristics. This convention:

1. **Reduces user burden.** Users need not understand algorithmic internals to get good default behavior.

2. **Enables adaptive selection.** The solver can tailor its strategy to the specific problem's structure, dimensions, and warm-start availability, which generally outperforms any fixed user choice.

3. **Centralizes decision logic.** Algorithm selection heuristics are concentrated in the dispatch layer (cxf_solver_dispatch) rather than distributed across the parameter system.

When the solver resolves an AUTO parameter, the resolved value may or may not be written back to the parameter table. For method selection, the resolved method is stored for reporting purposes. For cut parameters, the resolution happens within the solver and is not visible to the parameter table. This distinction is a design choice: method selection is informational (the user may want to know which method was chosen), while cut aggressiveness is an internal detail.

### Parameter Save/Restore Scope

cxf_solver_dispatch backs up approximately 30 parameters -- a broad set spanning method, tolerances, threading, and presolve. This broad scope reflects the reality that the solver may modify any of these parameters during a solve:

- Tolerances may be adjusted for numerical recovery.
- Method may be overridden for sub-problems or crossover.
- Presolve may be disabled for retry after a presolve-related failure.

The cost of this broad backup (saving ~30 typed values to local storage) is negligible compared to the solve itself, making the inclusive approach preferable to a minimal backup that risks missing a parameter the solver modifies in an unexpected code path.

---

## Key Parameter Interactions

### Method and Problem Type

The Method parameter interacts with the detected problem type to determine the actual algorithm used:

| Problem Type | Method=AUTO Resolution | Restrictions |
|-------------|----------------------|--------------|
| LP (small/medium) | Typically dual simplex | All methods available |
| LP (large sparse) | Typically barrier | All methods available |
| LP (with warm start) | Typically simplex | Warm start not useful for barrier |
| QP | Barrier (default) | PDHG not available for QP |
| SOCP | Barrier (forced) | Only barrier supports conic constraints |

### Thread Count and Concurrent Methods

The effective thread count (computed by cxf_get_threads, P3.11) constrains which concurrent methods are available:

1. The Threads parameter is reconciled with hardware detection.
2. Concurrent methods require sufficient threads to run multiple solvers in parallel.
3. The thread count is divided among concurrent instances, with each instance receiving a share.
4. If the effective thread count is too low for meaningful concurrency, the dispatch falls back to a non-concurrent method.

### Tolerances and Convergence

Tolerance parameters interact with each other and with termination parameters:

- **FeasibilityTol** and **OptimalityTol** together determine when a simplex solution is declared optimal. Both must be satisfied simultaneously.
- **BarConvTol** determines barrier convergence independently. After barrier convergence, crossover to a basic solution uses simplex tolerances.
- Tighter tolerances generally require more iterations, interacting with **IterationLimit** and **TimeLimit**.
- **NumericFocus** influences how aggressively the solver tightens internal tolerances and applies numerical safeguards.

### Presolve and Method Selection

The **Presolve** parameter affects the solve chain before method selection occurs. Disabling presolve can change which method is optimal (presolve may change the problem structure in ways that favor a different algorithm). The **PreDual** parameter can cause the solver to solve the dual formulation instead of the primal, which changes the effective problem dimensions and may trigger different method selection heuristics.

---

## Verification Checklist

```
[x] No hex addresses or offsets
[x] No Ghidra artifacts (param_, local_, iVar, DAT_, FUN_)
[x] No binary-specific constants
[x] No copied code fragments
[x] All descriptions are behavioral, not implementational
[x] All data structures described semantically
[x] All cross-references use spec IDs (P1.01, P3.24, etc.)
[x] Passes the Clean Room Test (Rule 10)
```

## References

- ConvexFeld Optimization, LLC. *ConvexFeld Optimizer Reference Manual* (public API documentation). Parameter Reference, Parameter Groups, Parameter Guidelines.
- Maros, I. (2003). *Computational Techniques of the Simplex Method*. Springer. International Series in Operations Research and Management Science, Vol. 61. (Parameter impact on simplex algorithm behavior.)
- Bixby, R.E. (2002). "Solving Real-World Linear Programs: A Decade and More of Progress." *Operations Research*, 50(1):3-15. (Algorithm selection heuristics and method parameter design.)
- Gill, P.E., Murray, W., Saunders, M.A., and Wright, M.H. (1989). "A practical anti-cycling procedure for linearly constrained optimization." *Mathematical Programming*, 45(1-3):437-474. (Perturbation parameter usage.)
- Nocedal, J. and Wright, S.J. (2006). *Numerical Optimization*. 2nd ed. Springer. (Barrier method convergence and tolerance parameter design.)
