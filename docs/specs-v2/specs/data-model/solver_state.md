# SolverState

## Purpose

SolverState is the primary runtime data structure for the simplex solver. It holds all working data needed during simplex iterations, including cached problem dimensions, sparse matrix storage for both row-major and column-major access, basis tracking arrays, working bound copies, reduced costs, and control parameters. A single SolverState instance is allocated at the beginning of simplex initialization and is passed to every simplex function as the central state container. It exists for the duration of a single LP solve and is freed during simplex cleanup.

## Fields

### Problem Dimensions

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| model | pointer-to-Model | Back-pointer to the parent optimization model | Non-null during solve | Set once at initialization; never changes |
| numVars | int | Number of decision variables (columns) in the problem | >= 0 | Copied from model matrix at init; read-only thereafter |
| numConstrs | int | Number of constraints (rows) in the problem | >= 0 | Copied from model matrix at init; read-only thereafter |
| numNonzeros | int | Total number of nonzero coefficients in the constraint matrix | >= 0 | Computed as sum of all column lengths at init |
| numSlacks | int | Number of slack variables added for inequality constraints | >= 0 | Determined during initialization based on constraint senses |

### Solve Configuration

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| solveMode | int | Primary simplex algorithm variant to use | PRIMAL=0, DUAL=1, BARRIER=2, AUTO=3 | Set during initialization from environment parameters |
| solveModeAlt | int | Alternative solve mode for fallback or crossover | Same as solveMode | May differ from solveMode when fallback is needed |
| initMode | int | Initialization mode controlling warm-start behavior | 0=fresh start, nonzero=reoptimization | Passed from caller; read-only during solve |
| phase | int | Current simplex phase (Phase I for feasibility, Phase II for optimality) | 1 or 2 | Transitions from 1 to 2 when feasibility is achieved |

### Iteration Control

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| maxIterations | int | Primary iteration limit copied from environment settings | > 0 | Set at init from environment; enforced by iteration loop |
| iterLimit | int | Secondary iteration limit for sub-procedures | > 0 | Set at init from environment |
| tolerance | double | Optimality tolerance for convergence testing | > 0, typically 1e-6 to 1e-8 | Copied from environment at init |

### Basis Tracking

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| numBasic | int | Count of variables currently in the basis | Equals numConstrs when basis is complete | Updated after each pivot operation |
| varStatus | array-of-int [numVars] | Per-variable basis status codes (see Variable Status Codes below) | BASIC (>=0), AT_LOWER (-1), AT_UPPER (-2), SUPERBASIC (-3), FIXED (-4) | Exactly numConstrs entries have value >= 0 |
| basisHeader | array-of-int [numConstrs] | Maps each constraint row to the index of its basic variable | 0 to numVars-1 | Each entry is the column index of the variable basic in that row |

### Row-Major Sparse Matrix (CSR)

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| rowStart | array-of-int64 [numConstrs+1] | Start index in coefficient arrays for each constraint row | >= 0 | rowStart[i+1] >= rowStart[i] |
| rowColCount | array-of-int [numConstrs] | Number of nonzero entries in each row | >= 0 | Sum equals numNonzeros |
| rowColIndices | array-of-int [numNonzeros] | Column indices of nonzero entries, ordered by row | 0 to numVars-1 | Parallel with rowCoefficients |
| rowCoefficients | array-of-double [numNonzeros] | Coefficient values of nonzero entries, ordered by row | Any finite double | Parallel with rowColIndices |

### Column-Major Sparse Matrix (CSC)

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| colStart | array-of-int64 [numVars+1] | Start index in coefficient arrays for each variable column | >= 0 | colStart[j+1] >= colStart[j] |
| colRowCount | array-of-int [numVars] | Number of nonzero entries in each column | >= 0 | Sum equals numNonzeros |
| colRowIndices | array-of-int [numNonzeros] | Row indices of nonzero entries, ordered by column | 0 to numConstrs-1 | Parallel with colCoefficients |
| colCoefficients | array-of-double [numNonzeros] | Coefficient values of nonzero entries, ordered by column | Any finite double | Parallel with colRowIndices |

### Working Bounds and Costs

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| lbWorking | array-of-double [numVars] | Working copy of variable lower bounds, modified during solve | Any double; negative infinity for unbounded | lbWorking[j] <= ubWorking[j] for feasible variables |
| ubWorking | array-of-double [numVars] | Working copy of variable upper bounds, modified during solve | Any double; positive infinity for unbounded | ubWorking[j] >= lbWorking[j] for feasible variables |
| reducedCosts | array-of-double [numVars] | Current reduced cost (dj) for each variable | Any finite double | Zero for basic variables at optimality |
| constraintRHS | array-of-double [numConstrs] | Right-hand side values or dual prices for constraints | Any finite double | Updated during pivot operations |

### Constraint Metadata

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| constraintSense | array-of-char [numConstrs] | Sense of each constraint: less-than-or-equal, greater-than-or-equal, or equality | '<', '>', '=' | May be modified during bound flipping |

### Objective Tracking

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| objectiveValue | double | Current value of the objective function | Any finite double | Updated after each pivot with the step contribution |

### Variable Flags

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| varFlags | array-of-int [numVars] | Per-variable type flags indicating special handling requirements (e.g., semi-continuous, SOS, piecewise-linear, quadratic) | Bitmask of type indicators | Set during initialization; read-only during simplex iterations |

### Steepest Edge Pricing Arrays

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| steepestEdgeLB | array-of-double [numVars] | Steepest edge weights for variable lower bound directions | >= 0 | Used by Devex or steepest edge pricing strategies |
| steepestEdgeUB | array-of-double [numVars] | Steepest edge weights for variable upper bound directions | >= 0 | Used by Devex or steepest edge pricing strategies |
| dualSteepestLB | array-of-double [numConstrs] | Dual steepest edge weights for constraint lower directions | >= 0 | Used in dual simplex mode |
| dualSteepestUB | array-of-double [numConstrs] | Dual steepest edge weights for constraint upper directions | >= 0 | Used in dual simplex mode |

### Eta Vector Management (Product Form of Inverse)

> **Ownership note:** The eta vector chain is logically owned by BasisState (P1.05), which holds the authoritative `etaListHead`, `etaTotalCount`, and `etaRowCount` fields alongside the LU factors and memory pool. The fields below are **convenience aliases** that SolverState maintains as direct copies of the BasisState fields, providing fast access from simplex iteration code without an extra pointer dereference through BasisState. Both sets of fields must remain synchronized; any update to one must be reflected in the other.

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| etaPoolMode | int | Controls eta vector storage mode: full tracking or simplified | 0=full eta records, 1=simplified mode | Determines size of allocated eta vectors |
| etaListHead | pointer-to-EtaVector | Convenience alias of BasisState.etaListHead for fast access | Null when no updates recorded | Must be kept in sync with BasisState.etaListHead |
| etaRowCount | int | Convenience alias of BasisState.etaRowCount | >= 0 | Must be kept in sync with BasisState.etaRowCount |
| etaTotalCount | int | Convenience alias of BasisState.etaTotalCount | >= 0 | Must be kept in sync with BasisState.etaTotalCount |

### Pricing Integration

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| pricingState | pointer-to-PricingState | Handle to the pricing subsystem state for variable selection | Non-null during solve | Initialized before first iteration; used by all pricing functions |

### Memory Management

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| memoryAllocator | pointer-to-Allocator | Temporary buffer allocator for eta vectors and scratch space | Non-null during solve | All temporary allocations go through this allocator |

### Working Array Dimensions

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| workSize1 | int64 | Primary working array capacity, typically max(numVars, numConstrs) | >= max(numVars, numConstrs) | Computed at init; used for array sizing |
| workSize2 | int64 | Secondary working array capacity for intermediate computations | >= 0 | Derived from problem dimensions |
| workSize3 | int64 | Tertiary working array capacity for extended storage | >= 0 | Derived from problem dimensions |

### Status and Diagnostics

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| solStatus | int | Solution status after solve completes | Standard LP solution status codes | Set by termination logic |
| matrixStatus | int | Copy of the matrix solution status for cross-referencing | Same domain as solStatus | Copied from matrix at initialization |
| problemVarIndex | int | Index of the variable that caused infeasibility or unboundedness, for diagnostics | 0 to numVars-1, or -1 if unset | Set when INFEASIBLE or UNBOUNDED is detected |

### Timing and Performance

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| timingArea | pointer-to-TimingState | Performance timing accumulator for elapsed time tracking | May be null if timing disabled | Initialized at start of solve |
| workCounter | pointer-to-double | Accumulated computational work metric for performance profiling | Null to disable work counting | Incremented proportionally to operations performed |
| scaleFactor | double | Variable scaling factor for numerical conditioning | > 0 | Used during steepest edge pricing computations |

### Presolve Statistics

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| removedRows | int | Number of rows eliminated during presolve | >= 0 | Set by presolve; used for progress logging |
| removedCols | int | Number of columns eliminated during presolve | >= 0 | Set by presolve; used for progress logging |
| lastReportTime | double | Elapsed time at last progress log message | >= 0.0 | Updated each time a progress message is printed |
| timingMode | int | Selects wall-clock or CPU time for progress reporting | 0=CPU time, 1=wall-clock time | Set at initialization |

## Variable Status Codes

The variable status encoding follows the standard revised simplex convention as described in Maros, *Computational Techniques of the Simplex Method* (Springer, 2003, Chapter 3) and originally formalized by Dantzig in *Linear Programming and Extensions* (Princeton University Press, 1963).

| Code | Name | Meaning |
|------|------|---------|
| >= 0 | BASIC | Variable is in the basis; the non-negative value indicates the constraint row in which this variable is basic |
| -1 | AT_LOWER | Non-basic variable held at its lower bound |
| -2 | AT_UPPER | Non-basic variable held at its upper bound |
| -3 | SUPERBASIC | Non-basic variable between its bounds (occurs with free variables or in degenerate situations) |
| -4 | FIXED | Non-basic variable whose lower bound equals its upper bound |

This encoding compactly represents both the basis membership and the bound status of each variable in a single integer array. The non-negative values for basic variables serve double duty as indices into the basis header, eliminating the need for a separate lookup structure.

## Relationships

- **Owns** all working arrays (bounds, status, matrix storage, steepest edge weights, reduced costs, constraint metadata). These are allocated during initialization and freed during cleanup.
- **Borrows** the parent Model (back-pointer). The Model owns the SolverState, not the reverse.
- **Owns** the PricingState, which is allocated and managed as part of simplex initialization.
- **Owns** the eta vector linked list. Eta vectors are allocated through the memory allocator and linked via etaListHead.
- **Borrows** the TimingState, which may be shared with the caller.
- **References** BasisState (external structure for LU factorization of the basis matrix). The BasisState is used during basis refactorization but is not stored directly within SolverState.
- The constraint matrix is stored in **dual representation** (both CSR and CSC) to support efficient row and column access during different phases of the simplex algorithm.

## Lifecycle

### Creation
1. The simplex initialization function allocates the SolverState via zero-initialized memory allocation.
2. Problem dimensions (numVars, numConstrs, numNonzeros) are copied from the model's matrix data.
3. A back-pointer to the parent model is stored.
4. Working array sizes are computed based on problem dimensions using a formula that ensures adequate scratch space: approximately max(10000, max(numVars, numConstrs)) plus a fraction of the nonzero count.
5. All working arrays are allocated: basis tracking, sparse matrix storage, bounds, reduced costs, scaling weights, and variable flags.
6. Bounds and objective coefficients are copied from the model matrix into working arrays.
7. Iteration limits and tolerances are copied from the environment.
8. The solve mode is selected based on environment parameters.

### Mutation
- **Every pivot operation** updates: varStatus, basisHeader, reducedCosts, objectiveValue, and the eta vector list.
- **Bound flipping** modifies constraintSense and row coefficients (negation of a matrix row to maintain consistency after a bound flip).
- **Basis refactorization** resets the eta vector list (etaListHead, etaTotalCount) and recomputes the LU factorization of the current basis.
- **Phase transitions** update the phase field when moving from Phase I (feasibility) to Phase II (optimality).
- **Steepest edge updates** modify the pricing weight arrays after each pivot.
- **Presolve statistics** (removedRows, removedCols, lastReportTime) are updated during progress reporting.

### Destruction
1. All eta vectors in the linked list are freed through the memory allocator.
2. All working arrays (bounds, status, matrix storage, scaling, flags) are freed.
3. The PricingState is freed.
4. The SolverState structure itself is freed.
5. The parent model's pointer to the SolverState is set to null.

Cleanup must proceed in reverse allocation order to avoid dangling references.

## Invariants

1. **Basis completeness**: Exactly numConstrs entries in varStatus have value >= 0 (are basic). The basisHeader array contains a permutation of these basic variable indices.
2. **Basis consistency**: For every row i, varStatus[basisHeader[i]] == i. That is, the variable recorded as basic in row i reports row i as its basis position.
3. **Bound ordering**: For every variable j, lbWorking[j] <= ubWorking[j] unless the problem is infeasible.
4. **Fixed variable consistency**: If varStatus[j] == FIXED (-4), then lbWorking[j] == ubWorking[j].
5. **Matrix duality**: The CSR and CSC representations encode the same constraint matrix. Any modification to one must be reflected in the other.
6. **Eta list integrity**: The eta vector linked list starting from etaListHead contains exactly etaTotalCount entries and is null-terminated.
7. **Dimension immutability**: numVars, numConstrs, and numNonzeros are never modified after initialization. All array sizes depend on these dimensions.
8. **Reduced cost complementarity**: At optimality, for each non-basic variable j: if varStatus[j] == AT_LOWER, then reducedCosts[j] >= -tolerance; if varStatus[j] == AT_UPPER, then reducedCosts[j] <= tolerance. This is the standard optimality condition of the simplex method.

## Thread Safety

SolverState is **not thread-safe**. It is designed as a single-threaded working structure for one simplex solve.

- All fields are read and written without synchronization.
- Each thread performing a concurrent solve must have its own independent SolverState.
- The parent Model may be shared across threads, but SolverState must not be.
- The memory allocator referenced by SolverState should be thread-local or externally synchronized.

## Design Rationale

**Centralized state container**: SolverState consolidates all working data for the simplex solver into a single structure, avoiding the need to pass many individual arrays between functions. This "god object" pattern is common in high-performance numerical solvers where function call overhead matters and the state is inherently tightly coupled.

**Dual sparse matrix storage (CSR + CSC)**: The constraint matrix is stored in both row-major (CSR) and column-major (CSC) formats. Row access is needed for ratio tests (scanning a constraint row to find the minimum ratio), while column access is needed for pricing (scanning a variable column to compute reduced costs) and for FTRAN/BTRAN operations. Storing both avoids expensive format conversions during iterations at the cost of doubled matrix memory. This is a standard trade-off in LP solver implementations (see Maros, 2003, Section 2.4).

**Working bound copies**: Bounds are copied from the model into working arrays so that the simplex algorithm can modify them (e.g., tightening via bound propagation or adjusting during presolve) without affecting the original model data. This preserves the ability to restart or re-solve from the original problem.

**Product Form of Inverse (PFI)**: The basis inverse is maintained as a sequence of eta vectors (elementary matrices) rather than an explicit dense inverse. This is the classical approach described by Dantzig (1963) and refined by subsequent authors. Each pivot appends one eta vector to the linked list. Periodic refactorization resets the list when the accumulated eta vectors degrade numerical accuracy or when the list becomes too long for efficient FTRAN/BTRAN operations.

**Variable status encoding**: Using the basis row index as the status code for basic variables (non-negative values) and reserved negative values for non-basic states is a compact encoding that eliminates the need for a separate "is basic" flag array and a separate basis-row lookup. This pattern is widely used in simplex implementations (Maros, 2003, Section 3.2).

**Steepest edge / Devex weight arrays**: Maintaining per-variable and per-constraint pricing weights enables the steepest edge pricing strategy (Goldfarb and Reid, 1977) and its approximate variant Devex (Harris, 1973). These weights are updated incrementally after each pivot rather than recomputed from scratch, which is essential for performance on large problems.

**Memory allocation pattern**: The tiered allocation strategy (main structure first, then arrays sized by dimensions, then arrays sized by nonzero count) allows early failure if memory is insufficient for the core structure, before committing to large array allocations. Zero-initialization of the main structure ensures that all pointer fields start as null, simplifying cleanup if a later allocation fails partway through initialization.

## References

- Dantzig, G.B. (1963). *Linear Programming and Extensions*. Princeton University Press.
- Maros, I. (2003). *Computational Techniques of the Simplex Method*. Springer.
- Goldfarb, D. and Reid, J.K. (1977). "A practicable steepest-edge simplex algorithm." *Mathematical Programming*, 12(1):361-371.
- Harris, P.M.J. (1973). "Pivot selection methods of the Devex LP code." *Mathematical Programming*, 5(1):1-28.
- Forrest, J.J.H. and Goldfarb, D. (1992). "Steepest-edge simplex algorithms for linear programming." *Mathematical Programming*, 57(1):341-374.

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
