# SolutionData (Solution Data Container)

## Purpose

SolutionData is the model-level container for all optimization output data. **Naming history:** Formerly `WorkArrays`; renamed to better reflect its actual role as a solution data container rather than a scratch buffer structure. This structure stores the results of an optimization call: primal variable values, dual values (for constraints, range constraints, and SOS constraints), objective function values and bounds, iteration and node counts, and solution pool entries. It does not hold scratch buffers for simplex iterations -- that role belongs to SolverState. It is allocated at the beginning of an optimization call, populated by the solver during and after the solve, and retained on the Model so that the user can query solution attributes (such as variable values, objective value, iteration count, and solution pool entries) after the optimization returns. It is freed when the solution is cleared or the model is destroyed.

This structure serves as the bridge between the solver's internal state and the public attribute system. After optimization completes, a wiring step connects entries in the model's attribute table to specific fields within this structure, enabling the attribute getter API to return solution data without additional computation.

## Fields

### Solve Mode and Status

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| solveMode | int | Records which optimization algorithm produced the solution (e.g., simplex, barrier) and controls how result attributes are wired | Standard solve mode codes | Set by the solver dispatch logic; read during attribute wiring |

### Primal and Dual Solution Arrays

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| primalValues | pointer-to-array-of-double | Primal solution values for all decision variables; wired to the "X" model attribute | Null before solve; non-null after a feasible solution is found | Length equals numVars from the model's matrix data |
| dualValues | pointer-to-array-of-double | Dual values (shadow prices) for linear constraints; wired to the "Slack" or dual attribute depending on solve mode | Null before solve; non-null after solve with dual information | Length equals numConstrs (plus range and SOS constraints when those sub-arrays are embedded) |
| rangeDuals | pointer-to-array-of-double | Dual values for range constraints; typically points into the dualValues allocation at an offset past the linear constraint duals | Null if no range constraints | Length equals numRangeConstrs; may alias a sub-region of the dualValues allocation |
| sosDuals | pointer-to-array-of-double | Dual values for SOS (Special Ordered Set) constraints; typically points into the dualValues allocation at an offset past the range constraint duals | Null if no SOS constraints | Length equals numSOSConstraints; may alias a sub-region of the dualValues allocation |

### Objective Function Data

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| objectiveValue | double | Best objective function value found during optimization; wired to the "ObjVal" attribute | Any finite double when a feasible solution exists; uninitialized otherwise | Updated by the solver upon finding improving solutions |
| objectiveBound | double | Best proven bound on the optimal objective value; wired to the "ObjBound" attribute | Any finite double | Equals objectiveValue at optimality for LP |
| poolObjectiveBound | double | Objective bound applicable to the solution pool; wired to the "PoolObjBound" attribute | Any finite double | Relevant only when the solution pool is populated |

### Iteration Counters

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| solutionCount | int | Number of feasible solutions found during optimization; wired to the "SolCount" attribute | Non-negative | Incremented each time the solver finds a new feasible solution |
| iterationCount | double | Total simplex iteration count; wired to the "IterCount" attribute | Non-negative | Sum of all simplex iterations across phases and restarts |
| iterationCount0 | double | Iteration count for a specific sub-phase (e.g., Phase I or initial solve); wired to the "IterCount0" attribute | Non-negative | Subset of iterationCount |
| barrierIterationCount | double | Total barrier (interior point) iteration count; wired to the "BarIterCount" attribute | Non-negative | Zero if barrier method was not used |
| pdhgIterationCount | double | Total PDHG (primal-dual hybrid gradient) iteration count; wired to the "PDHGIterCount" attribute | Non-negative | Zero if PDHG was not used |

### Scaling and Tolerance Parameters

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| scaledTolerance | double | A tolerance value scaled by the number of decision variables, used for convergence and quality checks during the solve | Positive double | Computed as numVars multiplied by a base tolerance (a standard numerical tolerance in the range typical for LP solvers, e.g., around 1e-9) |
| scaleFactor1 | double | A dimension-dependent scale factor used for numerical conditioning during the solve | Positive double | Computed as numVars multiplied by an algorithmic scaling constant |
| scaleFactor2 | double | A second dimension-dependent scale factor, initialized identically to scaleFactor1 but may diverge during the solve | Positive double | Computed as numVars multiplied by an algorithmic scaling constant |
| baseTolerance | double | The unscaled base tolerance constant from which scaledTolerance is derived | Positive double | Set at initialization; read-only during the solve |

### Iteration History Tracking

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| activeFlag | int | Indicates that this structure has been initialized and contains valid data | 0 (uninitialized) or 1 (active) | Set to 1 during allocation; checked before accessing other fields |
| cycleDetectionFlag | int | Flag used by anti-cycling logic to detect and break degenerate pivot cycles | 0 or 1 | Set to 1 at initialization; toggled by anti-cycling procedures |
| previousEnteringVar | int | Index of the entering variable from the most recent pivot, used for anti-cycling heuristics | -1 (unset) or a valid variable index | Set to -1 at initialization; updated after each pivot |
| previousLeavingVar | int | Index of the leaving variable from the most recent pivot, used for anti-cycling heuristics | -1 (unset) or a valid variable index | Set to -1 at initialization; updated after each pivot |
| previousPivotRow | int | Row index of the most recent pivot operation | -1 (unset) or a valid row index | Set to -1 at initialization; updated after each pivot |

### Solution Pool

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| poolSolutionCount | int | Number of solutions currently stored in the solution pool | Non-negative | Must not exceed any configured pool size limit |
| poolSolutionCount2 | int | Secondary count tracking solutions added during the current solve | Non-negative | May lag poolSolutionCount during incremental updates |
| poolVariableValues | pointer-to-array-of-pointers-to-double | Array of pointers, where each entry points to a complete primal solution vector for one solution in the pool | Null if no pool, non-null if pool is active | Length equals poolSolutionCount; each entry has length numVars |
| poolObjectiveValues | pointer-to-array-of-double | Objective function value for each solution in the pool | Null if no pool | Length equals poolSolutionCount; entries may use a sentinel value to indicate uninitialized slots |
| poolObjectiveBounds | pointer-to-array-of-double | Objective bound at the time each pool solution was found | Null if no pool | Length equals poolSolutionCount |

### Cut Data

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| cutCount | int | Number of user cuts or lazy constraints stored | Non-negative | Updated during callback processing |
| cutVariableValues | pointer-to-array-of-pointers-to-double | Array of pointers, where each entry points to the variable values associated with one cut | Null if no cuts | Length equals cutCount |
| cutObjectiveValues | pointer-to-array-of-double | Objective values associated with each stored cut | Null if no cuts | Length equals cutCount |

### Threshold Values

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| thresholds | array-of-double [6] | Adaptive threshold values used by the solver for dynamic tolerance adjustment during the solve (e.g., pricing tolerances, ratio test bounds, perturbation limits) | Initialized to -1.0 (indicating "not yet set"); set to positive values during the solve as adaptive strategies engage | A value of -1.0 signals that the threshold has not been activated |

### Auxiliary Indices

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| auxiliaryIndices | array-of-int [3] | Auxiliary variable or constraint indices used for tracking special solver states (e.g., the variable causing infeasibility, the constraint involved in a bound violation, or the last refactorization point) | Initialized to -1 (unset); set to valid indices during the solve | A value of -1 indicates the index is not active |

## Relationships

- **Owned by** Model. The Model holds a pointer to the SolutionData structure. The Model is responsible for allocating and freeing SolutionData.

- **Owns** primalValues and dualValues arrays. These are separately allocated arrays whose lifetime is managed by the SolutionData allocation and deallocation functions.

- **Borrows** rangeDuals and sosDuals. These typically point into the interior of the dualValues allocation rather than being independently allocated. They must not be freed separately.

- **Owns** poolVariableValues, poolObjectiveValues, poolObjectiveBounds arrays. Each entry in poolVariableValues is itself an independently allocated array that must be freed individually.

- **Owns** cutVariableValues and cutObjectiveValues arrays. Similar ownership semantics as the solution pool arrays.

- **Referenced by** the attribute table. After optimization, the attribute wiring step stores pointers into SolutionData fields within the model's attribute table entries. These pointers become invalid when SolutionData is freed, so attribute cache invalidation must precede SolutionData deallocation.

- **Populated by** the solver (SolverState or barrier state). The solver writes solution data into SolutionData during and after optimization.

- **Read by** the presolve uncrushing step. After solving a presolved model, the uncrush operation reads primal values from SolutionData to map them back to the original variable space.

## Lifecycle

### Creation

1. At the start of an optimization call, the allocation function checks whether the Model already has a SolutionData instance.
2. If not, a zero-initialized block is allocated, large enough to hold all fixed-size fields.
3. The activeFlag is set to 1, indicating the structure is live.
4. Scale factors are computed from the model's variable count multiplied by standard algorithmic tolerance and scaling constants.
5. The baseTolerance is stored from the solver's tolerance constant.
6. History tracking fields (previousEnteringVar, previousLeavingVar, previousPivotRow) are set to -1 (unset).
7. Threshold values are set to -1.0 (not yet activated).
8. Auxiliary indices are set to -1 (not active).
9. If a template SolutionData is provided (e.g., from a scenario model), the non-pointer fields are bulk-copied from the template. After copying, all pointer fields (primalValues, dualValues, rangeDuals, sosDuals) are explicitly set to null to prevent aliasing of the template's owned arrays.
10. Solution pool and cut counters are cleared to zero, and their associated pointer arrays are set to null.

### Mutation

- **During simplex iterations**: the cycle detection fields, threshold values, and auxiliary indices are updated as the solver progresses. Scale factors may be adjusted. Iteration counters are incremented.
- **On finding a feasible solution**: primalValues and dualValues arrays are allocated (if not already) and populated. The objectiveValue is set. The solutionCount is incremented.
- **On solve completion**: the attribute wiring function connects attribute table entries to SolutionData fields. The solveMode is finalized.

### Destruction

1. The attribute cache on the Model is invalidated, breaking any wired pointers from the attribute table into SolutionData.
2. The primalValues array is freed if non-null.
3. The dualValues array is freed if non-null. Since rangeDuals and sosDuals alias into this allocation, they are not freed separately; they are simply set to null.
4. Each entry in poolVariableValues (if the pool is active) is freed individually, then the poolVariableValues array itself is freed. poolObjectiveValues and poolObjectiveBounds are freed.
5. Each entry in cutVariableValues is freed, then the array itself. cutObjectiveValues is freed.
6. The SolutionData structure itself is freed.
7. The Model's pointer to SolutionData is set to null.

Deallocation must occur in reverse allocation order to avoid dangling references. In particular, attribute cache invalidation must happen before any field deallocation.

## Invariants

1. **Active flag consistency**: If activeFlag is 0, no other field should be read or written. Callers must check activeFlag (or the null-ness of the Model's pointer) before accessing SolutionData.

2. **Array length consistency**: The length of primalValues equals numVars from the model's matrix data. The length of dualValues equals numConstrs plus numRangeConstrs plus numSOSConstraints. Each poolVariableValues entry has length numVars.

3. **Dual aliasing**: rangeDuals and sosDuals point into the dualValues allocation. Specifically, rangeDuals points to dualValues offset by numConstrs elements, and sosDuals points to dualValues offset by numConstrs plus numRangeConstrs elements. These must not be freed independently.

4. **Pool count consistency**: poolSolutionCount equals the number of non-null entries in poolVariableValues. poolObjectiveValues and poolObjectiveBounds have the same length as poolVariableValues.

5. **Threshold sentinel**: Any threshold value equal to -1.0 has not been activated and should be treated as "use default" by any algorithm consulting it.

6. **Index sentinel**: Any auxiliary index or previous-pivot index equal to -1 indicates "not set" and must not be used as an array index.

7. **Attribute wiring validity**: If attribute table entries have been wired to SolutionData fields, the SolutionData structure must remain allocated and at the same memory address until the attribute cache is invalidated. Freeing or reallocating SolutionData without invalidating the cache produces dangling pointers.

## Thread Safety

SolutionData is **not thread-safe**. It is designed to be accessed by a single thread during and after a single optimization call.

- All fields are read and written without synchronization.
- Each concurrent optimization (e.g., concurrent LP solves or scenario processing) must operate on its own Model with its own SolutionData instance.
- After optimization returns, the user's thread may read SolutionData fields (via the attribute API) without locking, since no other thread should be modifying the structure post-solve.

## Design Rationale

**Separation from SolverState**: While SolverState holds the internal working data needed *during* simplex iterations (basis arrays, reduced costs, sparse matrix copies, eta vectors), SolutionData holds the *results* that persist after the solver finishes. This separation means the solver can free all its temporary working data (SolverState) immediately after completing, while the solution data (SolutionData) remains available for the user to query. This is a standard pattern in commercial LP solvers where the solver's internal memory footprint should be reclaimed promptly, but solution attributes must remain accessible.

**Dual value aliasing**: Rather than allocating three separate arrays for linear constraint duals, range constraint duals, and SOS constraint duals, a single contiguous allocation is made and the three pointers are set to offsets within it. This reduces the number of allocations and improves cache locality when iterating over all dual values. The trade-off is slightly more complex deallocation logic (only the base array is freed). This is a common memory management optimization in numerical software (Maros, 2003, Section 2.2 on efficient memory management).

**Template copying**: When solving multi-scenario optimization problems, the solver clones the model and solves each scenario independently. After a scenario solve, the scenario model's SolutionData serves as a template for populating the original model's SolutionData. The bulk copy transfers scalar fields (counters, objective values, scale factors, thresholds) efficiently, while pointer fields are cleared afterward to prevent double ownership. This is a standard "copy-then-fixup" pattern for structures containing a mix of value and pointer fields.

**Adaptive thresholds initialized to -1.0**: The threshold array uses -1.0 as a sentinel value meaning "not yet set." Many simplex implementations use adaptive tolerances that are computed lazily on first need, based on problem characteristics observed during the solve (e.g., the magnitude of matrix coefficients or the degree of degeneracy). Initializing to -1.0 allows each consumer to detect "first use" and compute an appropriate initial threshold. This pattern is described in the context of dynamic tolerance adjustment by Maros (2003, Chapter 8).

**Anti-cycling history**: The previousEnteringVar, previousLeavingVar, and previousPivotRow fields support anti-cycling heuristics that detect when the simplex method is revisiting the same basis. Simplex cycling -- where the algorithm visits the same sequence of degenerate bases indefinitely -- is a well-known pathology (Beale, 1955). Practical solvers detect potential cycles by tracking recent pivot history and applying perturbation or variable selection adjustments when a cycle is suspected.

## References

- Maros, I. (2003). *Computational Techniques of the Simplex Method*. Springer. (Chapters 2, 8)
- Dantzig, G.B. (1963). *Linear Programming and Extensions*. Princeton University Press.
- Beale, E.M.L. (1955). "Cycling in the dual simplex algorithm." *Naval Research Logistics Quarterly*, 2(4):269-275.
- Chvatal, V. (1983). *Linear Programming*. W.H. Freeman. (Chapter 3: solution representation)

---

## Verification Checklist

```
[x] No hex addresses or offsets
[x] No Ghidra artifacts (param_, local_, iVar, DAT_, FUN_)
[x] No binary-specific constants
[x] No copied code fragments
[x] All algorithms cite published sources
[x] All data structures described semantically
[x] Passes the Clean Room Test (Rule 10)
```
