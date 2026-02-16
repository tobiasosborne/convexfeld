# Module: Pivot Execution

**Spec Version:** 1.0
**Last Updated:** 2026-01-25
**Functions:** 2

## 1. Overview

### 1.1 Purpose

The Pivot Execution module implements the core variable manipulation operations in the simplex solver. It provides the mechanical foundation for both iterative simplex optimization and problem reduction through variable elimination.

The module handles two distinct but related operations:
1. **Primal pivot execution**: Performing basis-preserving variable moves that maintain the Product Form of Inverse (PFI) representation through eta vectors
2. **Variable fixing**: Permanently eliminating variables at constant values during presolve or branch-and-bound

While the Ratio Test module determines which variable should pivot and by how much, and the Pricing module determines which variable is the most attractive candidate, this module performs the actual mechanical work of updating data structures, propagating value changes, and maintaining solver invariants.

### 1.2 Responsibilities

This module is responsible for:

- [x] Executing primal simplex pivots with eta vector creation
- [x] Propagating variable value changes to objective and constraint RHS
- [x] Permanently fixing variables at constant values
- [x] Maintaining matrix sparsity through entry marking
- [x] Handling piecewise-linear objective segments
- [x] Managing quadratic objective linearization
- [x] Updating neighbor lists for combinatorial relationships
- [x] Invalidating and updating pricing state

This module is NOT responsible for:

- [ ] Selecting which variable to pivot (handled by Pricing)
- [ ] Determining step size (handled by Ratio Test)
- [ ] Basis inverse transformations (handled by Basis module)
- [ ] Overall solve control flow (handled by Simplex)

### 1.3 Design Philosophy

The module follows several key design principles:

**Separation of concerns**: Pivot execution is separate from pivot selection (Pricing) and pivot legality (Ratio Test). This modularity allows each component to be optimized independently.

**Lazy evaluation**: Eta vectors defer basis factorization work until FTRAN/BTRAN operations actually need it. This amortizes expensive linear algebra across iterations.

**Graceful degradation**: Multiple operating modes (standard, fast, special) allow the solver to trade accuracy for speed when appropriate.

**Numerical robustness**: Multiple stability checks (bound range, coefficient magnitude, threshold comparisons) prevent ill-conditioned operations.

**Feature integration**: PWL, quadratic, and combinatorial features are handled directly rather than through separate preprocessing, allowing exploitation of problem structure.

## 2. Public Interface

### 2.1 Exported Functions

| Function | Purpose | Typical Caller |
|----------|---------|----------------|
| cxf_pivot_primal | Execute primal simplex pivot with eta creation | cxf_simplex_step |
| cxf_fix_variable | Permanently fix variable at constant value | Presolve, branching |

### 2.2 Exported Types

| Type | Purpose |
|------|---------|
| EtaVector (Type 1) | Elementary matrix for column update |
| PivotMode | Operating mode flags (standard, fast, special) |

### 2.3 Exported Constants

| Constant | Value | Purpose |
|----------|-------|---------|
| PIVOT_MODE_STANDARD | 0 | Full eta creation and updates |
| PIVOT_MODE_FAST | 1 | Reduced overhead for degenerate pivots |
| PIVOT_MODE_SPECIAL | 2 | Special constraint handling |
| STATUS_FIXED | -4 | Fixed variable status code |

## 3. Internal Functions

### 3.1 Private Functions

| Function | Purpose |
|----------|---------|
| compute_pivot_value | Determine optimal value for pivot |
| handle_pwl_segment | Update piecewise-linear objective segment |
| process_neighbors | Update neighbor lists after pivot |

### 3.2 Helper Functions

| Function | Purpose | Used By |
|----------|---------|---------|
| mark_column_removed | Set column entries to -1 | Both functions |
| update_rhs | Subtract coefficient * value from RHS | Both functions |

## 4. State Management

### 4.1 Module State

| State Element | Type | Lifetime | Thread Safety |
|--------------|------|----------|---------------|
| Eta list | EtaVector* | Solve lifetime | Per-solver |
| Column data | CSC arrays | Solve lifetime | Per-solver |
| Objective arrays | double[] | Solve lifetime | Per-solver |
| Neighbor lists | int[][], double[][] | Solve lifetime | Per-solver |

### 4.2 State Lifecycle

```
For cxf_pivot_primal:
    FEASIBILITY_CHECK --> Check bound range vs tolerance
           |
           v
    STABILITY_CHECK --> Verify numeric stability
           |
           v
    VALUE_DETERMINATION --> Compute optimal pivot value
           |
           v
    PWL_HANDLING --> Update piecewise-linear segment (if needed)
           |
           v
    QUADRATIC_HANDLING --> Adjust neighbors (if needed)
           |
           v
    ETA_CREATION --> Allocate and populate eta vector
           |
           v
    OBJECTIVE_UPDATE --> Add c*v to constant, zero coefficient
           |
           v
    RHS_UPDATE --> Subtract A[:,j]*v from RHS
           |
           v
    PRICING_UPDATE --> Invalidate and update pricing state

For cxf_fix_variable:
    QUADRATIC_LINEARIZE --> Update neighbor coefficients
           |
           v
    OBJECTIVE_UPDATE --> Add c*v + 0.5*v^2*Q[j,j] to constant
           |
           v
    RHS_UPDATE --> Subtract A[:,j]*v from RHS
           |
           v
    COLUMN_REMOVAL --> Mark all entries as -1
           |
           v
    STATUS_UPDATE --> Set bounds to fixed value, status = -4
```

### 4.3 State Invariants

At all times, the following must be true:

- [x] Objective constant reflects contributions from fixed/pivoted variables
- [x] RHS values reflect variable substitutions
- [x] Column counts are consistent with marked entries
- [x] Neighbor lists maintain symmetry for quadratic terms

## 5. Dependencies

### 5.1 Required Modules

| Module | What We Use | Why |
|--------|-------------|-----|
| Memory | cxf_alloc_eta | Eta vector allocation |
| Pricing | cxf_pricing_invalidate, update | Maintain pricing caches |
| Parameters | Tolerances | Numerical thresholds |

### 5.2 Initialization Order

This module must be initialized:
- **After:** Memory, Matrix, Parameters, Pricing
- **Before:** Simplex iteration can begin

### 5.3 External Dependencies

| Dependency | Type | Purpose |
|------------|------|---------|
| fabs | C math | Absolute value comparisons |
| memcpy | C stdlib | Data copying in eta creation |

## 6. Dependents

### 6.1 Modules That Depend on Us

| Module | What They Use | Stability |
|--------|---------------|-----------|
| Simplex | cxf_pivot_primal | Stable |
| Presolve | cxf_fix_variable | Stable |
| Branch & Bound | cxf_fix_variable | Stable |
| Basis | Eta vectors created by pivot | Stable |

### 6.2 API Stability

The following interfaces are stable and must not change:

- [x] cxf_pivot_primal side effects on state
- [x] cxf_fix_variable guarantees (lb=ub=value, status=-4)
- [x] Eta vector memory layout

## 7. Invariants

### 7.1 Module Invariants

What this module guarantees:

- [x] Pivot maintains mathematical correctness of LP relaxation
- [x] Objective value = old_objective + c_j * value (for pivot)
- [x] For each constraint: rhs'[i] = rhs[i] - A[i,j] * value
- [x] Fixed variables have lb = ub = value and status = -4

### 7.2 Required Invariants

What this module requires from others:

- [x] Eta pool provides valid memory blocks
- [x] Bounds arrays are properly allocated
- [x] Matrix CSC structure is consistent
- [x] Variable indices are within valid range

## 8. Error Handling

### 8.1 Error Detection

How errors are detected in this module:

| Error Type | Detection Method |
|------------|------------------|
| Memory allocation failure | NULL return from eta allocator |
| Infeasible bounds | lb > ub + tolerance |
| Numerical instability | Max coefficient vs bound range check |

### 8.2 Error Propagation

How errors flow through this module:

```
Memory failure --> Return 1001 immediately
Infeasible --> Return early, no state changes
Numerical issue --> May proceed with warning
```

### 8.3 Recovery Strategies

| Error Type | Recovery |
|------------|----------|
| Memory failure | Caller must handle (typically abort) |
| Infeasible | Skip pivot, caller decides |
| Numerical | Trigger refactorization |

## 9. Thread Safety

### 9.1 Concurrency Model

Both functions operate on per-solver state only. No global mutable state exists. Concurrent solves on different instances are safe.

### 9.2 Synchronization Primitives

| Primitive | Protects | Granularity |
|-----------|----------|-------------|
| None | N/A | Per-solver isolation |

### 9.3 Thread Safety Guarantees

- [x] Separate solver instances are independent
- [x] No global state modified
- [x] Concurrent solves are safe

### 9.4 Known Race Conditions

None when properly used (one thread per solver).

## 10. Performance Characteristics

### 10.1 Complexity Summary

| Operation | Time | Space |
|-----------|------|-------|
| cxf_pivot_primal | O(nnz_col + neighbors) | O(nnz_col) for eta |
| cxf_fix_variable | O(nnz_col + qrow) | O(1) |

Where: nnz_col = column nonzeros, neighbors = quadratic neighbor count

### 10.2 Hot Paths

cxf_pivot_primal is the performance-critical path, called every simplex iteration:
- Eta vector allocation and population
- RHS update loop
- Neighbor list surgery (for quadratic)

Performance varies by case:
- Fast path (no PWL, no Q, few entries): 10-50 microseconds
- Typical case: 50-200 microseconds
- Expensive case (dense column, many neighbors): 200-1000 microseconds

### 10.3 Memory Usage

- Eta vectors: ~200 bytes to ~50 KB each, accumulate until refactorization
- No additional allocation for cxf_fix_variable
- Typical eta chain: 100-1000 etas (1-50 MB total)

## 11. Function Index

Complete list of functions in this module with links to individual specs:

### Public Functions

1. [cxf_pivot_primal](functions/pivot/cxf_pivot_primal.md) - Execute primal simplex pivot
2. [cxf_fix_variable](functions/pivot/cxf_fix_variable.md) - Permanently fix variable at value

## 12. Design Decisions

### 12.1 Key Design Choices

| Decision | Rationale | Alternatives Considered |
|----------|-----------|------------------------|
| PFI via eta vectors | O(d) updates vs O(m^2) explicit | LU with updates, explicit inverse |
| Mode flags for variants | Avoid separate functions | Separate pivot_fast, pivot_special |
| Integrated PWL/quadratic | Exploit structure in iteration | Separate preprocessing |
| Lazy entry deletion | O(1) mark vs O(n) compact | Immediate compaction |

### 12.2 Known Limitations

- [x] Eta chain length bounded by memory
- [x] Very dense columns are expensive
- [x] Quadratic neighbor updates can dominate for dense Q

### 12.3 Future Improvements

- [x] Batch pivot mode for multiple degenerate pivots
- [x] Parallel neighbor updates for large Q
- [x] Adaptive mode selection based on column structure

## 13. References

- Dantzig, G. B. (1963). *Linear Programming and Extensions*. Princeton University Press.
- Forrest, J. J., & Tomlin, J. A. (1972). "Updated triangular factors of the basis."
- Maros, I. (2003). *Computational Techniques of the Simplex Method*. Kluwer Academic Publishers.
- Nocedal, J., & Wright, S. J. (2006). *Numerical Optimization* (2nd ed.). Springer.
- Achterberg, T. (2007). *Constraint Integer Programming*. PhD Thesis, TU Berlin.

## 14. Validation Checklist

Before finalizing this spec:

- [x] All public functions documented
- [x] All dependencies identified
- [x] Thread safety analyzed
- [x] Error handling complete
- [x] No implementation details leaked
- [x] Module boundaries clear

---

*Reviewed by: Pending*

---
*Convexfeld LP Solver Specification*
*Based on published optimization literature*
