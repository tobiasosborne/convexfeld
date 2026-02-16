# Parameters & Defaults

## Overview

This reference document catalogs the solver parameters that control optimization behavior, organized by functional category. Parameters are the primary mechanism through which users configure algorithm selection, termination criteria, numerical tolerances, logging, threading, and all other solver behavior.

Parameters are part of the solver's **public API** and are documented in the ConvexFeld Optimizer Reference Manual. All parameter names, types, default values, valid ranges, and descriptions in this document are drawn from that public documentation.

### Parameter Types

ConvexFeld parameters come in three data types:

| Type | Description |
|------|-------------|
| **int** | Integer-valued parameters. Many use -1 as "automatic" (solver chooses). MAXINT is 2,000,000,000. |
| **double** | Floating-point parameters. Infinity means no limit. |
| **string** | String-valued parameters. Default is typically "" (empty string). |

### Parameter Precedence

Parameters are resolved in a layered precedence order, with later layers overriding earlier ones:

1. **Built-in defaults** -- hardcoded in the solver library during parameter table initialization
2. **Configuration file parameters** -- loaded from the optional `convexfeld.env` file in the current working directory
3. **Programmatic settings** -- set by the user via the API (highest precedence)

Parameters are stored on the environment object. When a model is created from an environment, it inherits the environment's parameter values. Model-level parameter changes do not propagate back to the environment.

### Conventions

- A default of **-1** typically means "automatic" -- the solver selects the best strategy.
- A default of **Infinity** (for double parameters) or **MAXINT** (for int parameters) means "no limit."
- MAXINT = 2,000,000,000 throughout this document.

---

## 1. Termination Parameters

These parameters control when the solver stops. They define resource limits (time, iterations, memory) and objective thresholds.

| Parameter | Type | Default | Min | Max | Description |
|-----------|------|---------|-----|-----|-------------|
| TimeLimit | double | Infinity | 0 | Infinity | Wall-clock time limit in seconds. The solver terminates when this limit is reached. Settable from a callback. |
| IterationLimit | double | Infinity | 0 | Infinity | Maximum number of simplex iterations. |
| BarIterLimit | int | 1000 | 0 | MAXINT | Maximum number of barrier iterations. Settable from a callback. |
| WorkLimit | double | Infinity | 0 | Infinity | Computational work limit in work units (a deterministic measure). Settable from a callback. |
| MemLimit | double | Infinity | 0 | Infinity | Hard memory limit in GB. The solver terminates with an error if this limit is exceeded. |
| SoftMemLimit | double | Infinity | 0 | Infinity | Soft memory limit in GB. The solver takes more conservative memory-saving measures when approaching this limit but does not terminate. |
| NLBarIterLimit | int | 1000 | 0 | MAXINT | Maximum number of barrier iterations for nonlinear models. |
| PDHGIterLimit | double | Infinity | 0 | Infinity | Maximum number of PDHG (Primal-Dual Hybrid Gradient) iterations. |

---

## 2. Tolerance Parameters

These parameters define numerical thresholds that determine when constraints are considered satisfied and when a solution is considered optimal.

| Parameter | Type | Default | Min | Max | Description |
|-----------|------|---------|-----|-----|-------------|
| FeasibilityTol | double | 1e-6 | 1e-9 | 1e-2 | Primal feasibility tolerance. All constraints must be satisfied to within this tolerance. |
| OptimalityTol | double | 1e-6 | 1e-9 | 1e-2 | Dual feasibility tolerance (reduced cost tolerance). Determines when a solution is considered optimal for simplex. |
| BarConvTol | double | 1e-8 | 0.0 | 1.0 | Barrier convergence tolerance. The barrier solver terminates when primal infeasibility, dual infeasibility, and complementarity gap are all below this threshold. |
| BarQCPConvTol | double | 1e-6 | 0.0 | 1.0 | Barrier convergence tolerance for QCP (Quadratically Constrained Program) models. |
| MarkowitzTol | double | 0.0078125 | 1e-4 | 0.999 | Threshold pivoting tolerance for simplex basis factorization (Markowitz criterion). Larger values improve numerical stability at the cost of fill-in and speed. |
| PSDTol | double | 1e-6 | 0 | Infinity | Positive semi-definiteness tolerance for Q matrices. |
| NLBarCFeasTol | double | 1e-8 | 0 | Infinity | Complementarity feasibility tolerance for the nonlinear barrier solver. |
| NLBarDFeasTol | double | 1e-6 | 0 | Infinity | Dual feasibility tolerance for the nonlinear barrier solver. |
| NLBarPFeasTol | double | 1e-6 | 0 | Infinity | Primal feasibility tolerance for the nonlinear barrier solver. |
| PDHGAbsTol | double | 1e-6 | 0 | Infinity | Absolute feasibility tolerance for the PDHG solver. |
| PDHGConvTol | double | 1e-6 | 0 | Infinity | Convergence tolerance for the PDHG solver. |
| PDHGRelTol | double | 1e-6 | 0 | Infinity | Relative feasibility tolerance for the PDHG solver. |

---

## 3. Simplex Parameters

These parameters control the behavior of the simplex algorithm, including method selection, pricing strategy, perturbation, and related options.

| Parameter | Type | Default | Min | Max | Description |
|-----------|------|---------|-----|-----|-------------|
| Method | int | -1 | -1 | 5 | Algorithm used for solving the root LP. -1 = automatic, 0 = primal simplex, 1 = dual simplex, 2 = barrier, 3 = concurrent (multiple methods in parallel), 4 = deterministic concurrent, 5 = PDHG. |
| SimplexPricing | int | -1 | -1 | 3 | Simplex variable pricing strategy. -1 = automatic, 0 = partial pricing, 1 = steepest edge, 2 = Devex, 3 = quick-start steepest edge. |
| NormAdjust | int | -1 | -1 | 3 | Controls the simplex pricing norm. -1 = automatic, 0-3 = specific norm choices. |
| Quad | int | -1 | -1 | 1 | Controls use of quad precision in simplex computations. -1 = automatic, 0 = off, 1 = on. Improves numerical accuracy at a performance cost. |
| PerturbValue | double | 0.0002 | 0 | Infinity | Magnitude of simplex perturbation. Used to break degeneracy. |
| Sifting | int | -1 | -1 | 2 | Controls sifting within dual simplex. Sifting is efficient for LP models with many more variables than constraints. -1 = automatic, 0 = off, 1 = moderate, 2 = aggressive. |
| SiftMethod | int | -1 | -1 | 2 | LP algorithm used for sifting sub-problems. -1 = automatic, 0 = primal simplex, 1 = dual simplex, 2 = barrier. |
| NetworkAlg | int | -1 | -1 | 2 | Controls detection and exploitation of network structure. -1 = automatic, 0 = off, 1 = detect and use network simplex, 2 = aggressive. |
| DegenMoves | int | -1 | -1 | MAXINT | Limits the number of degenerate simplex moves allowed. -1 = automatic. |
| LPWarmStart | int | -1 | -1 | 2 | Controls warm-start strategy for LP re-optimization. -1 = automatic, 0 = disabled, 1 = use basis, 2 = use basis and solution. |
| InfUnbdInfo | int | 0 | 0 | 1 | When set to 1, computes additional information for infeasible or unbounded models (e.g., an unbounded ray or a Farkas infeasibility proof). |

---

## 4. Barrier Parameters

These parameters control the interior-point (barrier) algorithm and crossover to a basic solution.

| Parameter | Type | Default | Min | Max | Description |
|-----------|------|---------|-----|-----|-------------|
| BarConvTol | double | 1e-8 | 0.0 | 1.0 | Barrier convergence tolerance (also listed under Tolerances). |
| BarCorrectors | int | -1 | -1 | MAXINT | Maximum number of central corrections performed per barrier iteration. -1 = automatic. |
| BarHomogeneous | int | -1 | -1 | 1 | Determines whether to use the homogeneous barrier algorithm. -1 = automatic, 0 = off, 1 = on. The homogeneous algorithm is more robust for detecting infeasibility or unboundedness. |
| BarIterLimit | int | 1000 | 0 | MAXINT | Maximum number of barrier iterations (also listed under Termination). |
| BarOrder | int | -1 | -1 | 1 | Sparse matrix fill-reducing ordering algorithm for the barrier method. -1 = automatic, 0 = Approximate Minimum Degree (AMD), 1 = Nested Dissection. |
| BarQCPConvTol | double | 1e-6 | 0.0 | 1.0 | Barrier convergence tolerance for QCP models (also listed under Tolerances). |
| Crossover | int | -1 | -1 | 4 | Crossover strategy for converting the barrier interior-point solution to a basic (vertex) solution. -1 = automatic, 0 = disabled, 1-4 = specific crossover strategies. |
| CrossoverBasis | int | -1 | -1 | 1 | Controls how the initial basis is constructed for crossover. -1 = automatic. |
| QCPDual | int | 0 | 0 | 1 | When set to 1, computes dual values for QCP models. |

---

## 5. Presolve Parameters

These parameters control the presolve phase, which simplifies and tightens the model before the main optimization.

| Parameter | Type | Default | Min | Max | Description |
|-----------|------|---------|-----|-----|-------------|
| Presolve | int | -1 | -1 | 2 | Presolve aggressiveness. -1 = automatic, 0 = off, 1 = conservative, 2 = aggressive. |
| PrePasses | int | -1 | -1 | MAXINT | Maximum number of presolve passes. -1 = automatic. |
| Aggregate | int | 1 | 0 | 2 | Controls aggregation in presolve. 0 = off, 1 = moderate (default), 2 = aggressive. |
| AggFill | int | -1 | -1 | MAXINT | Controls the amount of fill allowed during presolve aggregation. -1 = automatic. |
| PreDual | int | -1 | -1 | 2 | Controls whether presolve forms and solves the dual of the model. -1 = automatic. |
| PreCrush | int | 0 | 0 | 1 | When set to 1, allows presolve to translate user cuts into the presolved model. Required when adding user cuts in a callback. |
| PreDepRow | int | -1 | -1 | 1 | Controls presolve dependent row reduction. -1 = automatic. |
| DualReductions | int | 1 | 0 | 1 | Controls dual reductions in presolve. When set to 0, dual reductions that could mask infeasibility or unboundedness are disabled. |
| PreQLinearize | int | -1 | -1 | 2 | Controls linearization of quadratic terms during presolve. -1 = automatic. |
| PreSparsify | int | -1 | -1 | 1 | Controls the sparsify reduction in presolve. -1 = automatic. |
| PreSOS1BigM | double | -1 | -1 | Infinity | Controls the Big-M value used in SOS1 reformulation during presolve. -1 = automatic. |
| PreSOS1Encoding | int | -1 | -1 | 3 | Controls the encoding used for SOS1 constraint reformulation. -1 = automatic. |
| PreSOS2BigM | double | -1 | -1 | Infinity | Controls the Big-M value used in SOS2 reformulation during presolve. -1 = automatic. |
| PreSOS2Encoding | int | -1 | -1 | 3 | Controls the encoding used for SOS2 constraint reformulation. -1 = automatic. |

---

## 6. Scaling Parameters

These parameters control the scaling of the model coefficient matrix, which can improve numerical behavior.

| Parameter | Type | Default | Min | Max | Description |
|-----------|------|---------|-----|-----|-------------|
| ScaleFlag | int | -1 | -1 | 3 | Controls model scaling. -1 = automatic, 0 = off, 1 = moderate, 2 = aggressive, 3 = very aggressive. |
| ObjScale | double | 0 | -1 | Infinity | Controls objective function scaling. 0 = automatic scaling off, -1 = use the largest objective coefficient to choose scaling, positive values specify a scaling factor. |
| NumericFocus | int | 0 | 0 | 3 | Controls the degree of extra care taken on numerical issues. 0 = automatic, 1-3 = increasingly careful (and slower) handling. |

---

## 7. Output and Logging Parameters

These parameters control what information the solver writes to the console, log files, and result files.

| Parameter | Type | Default | Min | Max | Description |
|-----------|------|---------|-----|-----|-------------|
| OutputFlag | int | 1 | 0 | 1 | When set to 0, suppresses all solver output to the console. |
| LogToConsole | int | 1 | 0 | 1 | When set to 0, disables logging to the console. Log file output (if configured) is unaffected. |
| LogFile | string | "" | -- | -- | Path to a log file. If non-empty, all solver output is also written to this file. |
| DisplayInterval | int | 5 | 1 | MAXINT | Frequency of log output lines, in seconds. Controls how often progress lines are printed. |
| Record | string | "" | -- | -- | File for recording API calls (for reproducibility). |
| ResultFile | string | "" | -- | -- | File to which the solution is written after optimization. File format is determined by the extension. |
| JSONSolDetail | int | 0 | 0 | 1 | Level of detail included in JSON solution output. 0 = basic, 1 = detailed. |
| IgnoreNames | int | 0 | 0 | 1 | When set to 1, the solver ignores user-provided variable and constraint names. |
| InputFile | string | "" | -- | -- | Input file for the command-line tool. Used only by convexfeld_cl. |

---

## 8. Threading and Concurrency Parameters

These parameters control parallel execution and distributed computing.

| Parameter | Type | Default | Min | Max | Description |
|-----------|------|---------|-----|-----|-------------|
| Threads | int | 0 | 0 | MAXINT | Number of parallel threads to use. 0 = automatic (use all available cores, up to a solver-determined limit). |
| ThreadLimit | int | 0 | 0 | MAXINT | Hard limit on total threads across all concurrent solves. 0 = no limit. |
| ConcurrentMethod | int | -1 | -1 | 3 | Controls which LP algorithms run concurrently. -1 = automatic. |
| ConcurrentJobs | int | 0 | 0 | MAXINT | Number of distributed concurrent optimization jobs. 0 = no distributed concurrency. |
| InheritParams | int | -1 | -1 | 1 | Controls whether concurrent and multi-objective sub-environments inherit parameters from the parent. -1 = automatic. |

---

## 9. Tuning Parameters

These parameters control the automatic parameter tuning feature, which systematically searches for parameter settings that improve solve performance on a given model.

| Parameter | Type | Default | Min | Max | Description |
|-----------|------|---------|-----|-----|-------------|
| TuneOutput | int | 2 | 0 | 3 | Controls the amount of output produced during tuning. 0 = no output, 3 = verbose. |
| TuneResults | int | -1 | -1 | MAXINT | Number of improved parameter sets to report. -1 = automatic. |
| TuneTrials | int | 0 | 0 | MAXINT | Number of trial runs per parameter set during tuning. More trials increase reliability. |
| TuneTimeLimit | double | 86400 | 0 | Infinity | Time limit for the entire tuning session, in seconds. Default is 24 hours. |
| TuneTargetTime | double | 0.005 | 0.001 | Infinity | Target solve time for tuning. |
| TuneCriterion | int | -1 | -1 | 2 | Criterion used to evaluate parameter sets during tuning. -1 = automatic. |
| TuneJobs | int | 0 | 0 | MAXINT | Number of distributed tuning jobs (static workers). |
| TuneDynamicJobs | int | 0 | 0 | MAXINT | Number of distributed tuning jobs (dynamic workers). |
| TuneMetric | int | -1 | -1 | 3 | Metric used to aggregate results across tuning trials. -1 = automatic. |
| TuneCleanup | int | 0 | 0 | 1 | When set to 1, enables cleanup of intermediate tuning data. |
| TuneBaseSettings | string | "" | -- | -- | Path to a parameter file specifying baseline settings for tuning. |
| TuneIgnoreSettings | string | "" | -- | -- | Path to a parameter file listing parameters to exclude from tuning. |
| TuneParams | string | "" | -- | -- | Path to a parameter file listing parameters to include in tuning. |
| TuneUseFilename | int | 0 | 0 | 1 | When set to 1, includes the model filename in tuning output. |

---

## 10. Multi-Objective Parameters

These parameters control behavior when solving models with multiple objective functions.

| Parameter | Type | Default | Min | Max | Description |
|-----------|------|---------|-----|-----|-------------|
| ObjNumber | int | 0 | 0 | MAXINT | Selects which objective function to query or modify in a multi-objective model. |
| MultiObjMethod | int | -1 | -1 | 2 | Warm-start method for solving subsequent objectives. -1 = automatic. |
| MultiObjPre | int | -1 | -1 | 2 | Controls initial presolve on multi-objective models. -1 = automatic. |
| MultiObjSettings | string | "" | -- | -- | Comma-separated list of parameter files for individual objectives. |

---

## 11. Function Constraint Parameters (Deprecated)

These parameters control piecewise-linear (PWL) approximation of nonlinear function constraints. They are deprecated in favor of the native nonlinear support.

| Parameter | Type | Default | Min | Max | Description |
|-----------|------|---------|-----|-----|-------------|
| FuncNonlinear | int | 1 | 0 | 1 | Approximation approach for function constraints. 0 = PWL outer approximation, 1 = use native nonlinear support. |
| FuncPieces | int | 0 | -2 | 200000000 | Strategy for determining the number of PWL pieces. Negative values use error-based refinement. |
| FuncPieceError | double | 1e-3 | 1e-6 | 1e+6 | Maximum allowed error in PWL translation of function constraints. |
| FuncPieceLength | double | 1e-2 | 1e-5 | 1e+6 | Maximum piece length for PWL approximation. |
| FuncPieceRatio | double | -1 | -1 | 1 | Controls whether PWL approximation under- or over-estimates. -1 = automatic. |
| FuncMaxVal | double | 1e+6 | 1e-2 | Infinity | Maximum allowed value for variables appearing in function constraints. |

---

## 12. Miscellaneous Parameters

| Parameter | Type | Default | Min | Max | Description |
|-----------|------|---------|-----|-----|-------------|
| Seed | int | 0 | 0 | MAXINT | Random number seed. Controls random perturbations that may influence the solution path. Different seeds can lead to different solution paths. |
| SolutionTarget | int | -1 | -1 | 2 | Selects the target solution type for LP. -1 = automatic, 0 = optimal basic solution, 1 = optimal non-basic solution (if available), 2 = any primal feasible solution. |
| UpdateMode | int | 1 | 0 | 1 | Controls lazy update behavior. 0 = pending modifications are applied explicitly via update(), 1 = modifications are flushed automatically when needed. |
| FeasRelaxBigM | double | 1e6 | 0 | Infinity | Big-M value used when constructing feasibility relaxation models. |
| IISMethod | int | -1 | -1 | 3 | Selects the method for computing an Irreducible Inconsistent Subsystem (IIS). -1 = automatic. |
| ScenarioNumber | int | 0 | 0 | MAXINT | Selects the active scenario in a multi-scenario model. |
| FixVarsInIndicators | int | 0 | 0 | 1 | Controls conversion of fixed variables in indicator constraints. |

---

## Environment Variables

In addition to the parameters above, several system environment variables influence solver behavior. These are read during environment initialization and cannot be changed programmatically.

| Environment Variable | Type | Description |
|---------------------|------|-------------|
| CXF_CONFIG_FILE | string | Path to the ConvexFeld configuration file, overriding the default search. |
| CXF_CORES | int (1-1024) | Overrides the auto-detected logical core count. |
| CXF_PHYSICALCORES | int (1-1024) | Overrides the auto-detected physical core count. |
| CXF_MAXCORES | int (>0) | Limits the maximum number of cores the solver may use. |
| CXF_MEMLIMIT | double (>=0) | Memory limit in GB applied during environment initialization. |

---

## Sources

- [ConvexFeld Optimizer Reference Manual -- Parameter Reference](https://docs.convexfeld.com/projects/optimizer/en/current/reference/parameters.html)
- [ConvexFeld Optimizer Reference Manual -- Parameter Groups](https://docs.convexfeld.com/projects/optimizer/en/current/concepts/parameters/groups.html)
- [ConvexFeld Optimizer Reference Manual -- Parameter Descriptions](https://docs.convexfeld.com/projects/optimizer/en/current/reference/parameters/descriptions.html)
- [ConvexFeld Optimizer Reference Manual -- Parameter Guidelines](https://docs.convexfeld.com/projects/optimizer/en/current/concepts/parameters/guidelines.html)
- [GAMS/ConvexFeld Solver Documentation](https://www.gams.com/latest/docs/S_CONVEXFELD.html)
