# Module: Ratio Test

**Spec Version:** 1.0
**Last Updated:** 2026-01-25
**Functions:** 4

## 1. Overview

### 1.1 Purpose

The Ratio Test module implements critical pivot selection logic for the simplex method. Once pricing has selected an entering variable, the ratio test determines which basic variable should leave the basis. This selection must satisfy two key properties: maintaining primal feasibility (keeping all variables within bounds) and numerical stability (avoiding small pivot elements that amplify errors).

The module provides the Harris two-pass ratio test, which first identifies candidates with acceptable ratios and then selects among them the one with the largest pivot element. This approach prevents numerical instability from small pivots while maintaining near-optimal progress.

Beyond basic ratio testing, the module handles bound operations (moving variables to bounds), feasibility validation for bound changes, and special cases including unboundedness detection and row elimination during variable fixing.

### 1.2 Responsibilities

This module is responsible for:

- [x] Selecting the leaving variable via ratio test computation
- [x] Ensuring numerical stability through Harris two-pass approach
- [x] Moving variables to bounds with proper state updates
- [x] Validating that bound operations maintain feasibility
- [x] Detecting unbounded problems (no limiting ratio)
- [x] Handling bound flip operations for dual simplex
- [x] Eliminating redundant rows during variable fixing
- [x] Updating objective value and RHS after bound changes

This module is NOT responsible for:

- [ ] Selecting the entering variable (handled by Pricing)
- [ ] Computing the pivot column (FTRAN - handled by Basis)
- [ ] Executing the actual basis exchange (handled by Pivot)
- [ ] Managing basis status arrays (handled by Basis/Simplex)

### 1.3 Design Philosophy

The module balances several competing concerns:

**Feasibility preservation**: The ratio test must select a leaving variable such that after the pivot, all basic variables remain within their bounds (within tolerance).

**Numerical stability**: The Harris approach prefers larger pivot elements among candidates with similar ratios, preventing the accumulation of numerical errors that would require frequent refactorization.

**Efficiency**: The two-pass approach examines elements twice but saves overall time by reducing refactorization frequency and iteration count.

**Generality**: The module handles standard pivots, bound flips, variable fixing, and row elimination through a unified framework.

## 2. Public Interface

### 2.1 Exported Functions

| Function | Purpose | Typical Caller |
|----------|---------|----------------|
| cxf_ratio_test | Standard Harris ratio test for leaving variable | cxf_simplex_step |
| cxf_pivot_bound | Move variable to bound, update solver state | cxf_simplex_step, dual pivot |
| cxf_pivot_check | Compute feasible bound range from constraints | Presolve, bound tightening |
| cxf_pivot_special | Handle unboundedness, row elimination, bound flips | Special case handling |

### 2.2 Exported Types

| Type | Purpose |
|------|---------|
| RatioTestResult | Contains leaving variable, pivot element, step length |

### 2.3 Exported Constants

| Constant | Value | Purpose |
|----------|-------|---------|
| RATIO_OPTIMAL | 0 | Successful selection (continue) |
| RATIO_UNBOUNDED | 5 | No limiting ratio (unbounded problem) |
| FEASIBILITY_TOL | 1e-6 | Default feasibility tolerance |
| HARRIS_FACTOR | 10.0 | Multiplier for first-pass tolerance |

## 3. Internal Functions

### 3.1 Private Functions

| Function | Purpose |
|----------|---------|
| compute_ratio | Calculate ratio for single basic variable |
| check_unbounded | Verify if problem is truly unbounded |
| update_dual_arrays | Maintain dual pricing arrays after bound change |

### 3.2 Helper Functions

| Function | Purpose | Used By |
|----------|---------|---------|
| find_best_pivot | Select largest pivot among candidates | cxf_ratio_test |
| compute_rhs_impact | Calculate constraint RHS changes | cxf_pivot_bound |

## 4. State Management

### 4.1 Module State

| State Element | Type | Lifetime | Thread Safety |
|--------------|------|----------|---------------|
| Pivot column | double[] | Per-iteration | Per-solver |
| Candidate list | int[] | Per-iteration | Per-solver |
| Dual pricing arrays | double[] | Solve lifetime | Per-solver |

### 4.2 State Lifecycle

```
Called per iteration:
    cxf_ratio_test
        |
        v (compute ratios)
    CANDIDATES_FOUND
        |
        v (second pass)
    LEAVING_SELECTED --> Return to simplex iteration
        |
    or UNBOUNDED --> Problem unbounded

Called for bound operations:
    cxf_pivot_check --> Validation only
        |
    cxf_pivot_bound --> Execute bound change
        |
    or cxf_pivot_special --> Handle edge cases
```

### 4.3 State Invariants

At all times, the following must be true:

- [x] Selected leaving variable has limiting ratio (or none exists)
- [x] Pivot element meets minimum magnitude threshold
- [x] After bound operation, affected RHS values are updated
- [x] Dual pricing arrays remain consistent

## 5. Dependencies

### 5.1 Required Modules

| Module | What We Use | Why |
|--------|-------------|-----|
| Memory | cxf_alloc_eta | Eta vector allocation for bound ops |
| Pricing | cxf_pricing_invalidate, update | Maintain pricing caches |
| Parameters | cxf_get_feasibility_tol | Tolerance values |
| Simplex | cxf_fix_variable | Variable fixing in special cases |

### 5.2 Initialization Order

This module must be initialized:
- **After:** Memory, Parameters, Basis, Pricing
- **Before:** Main simplex iteration loop

### 5.3 External Dependencies

| Dependency | Type | Purpose |
|------------|------|---------|
| fabs | C math | Absolute value for comparisons |

## 6. Dependents

### 6.1 Modules That Depend on Us

| Module | What They Use | Stability |
|--------|---------------|-----------|
| Simplex | cxf_ratio_test, cxf_pivot_bound | Stable |
| Pivot | cxf_pivot_check, cxf_pivot_special | Stable |
| Presolve | cxf_pivot_check, cxf_pivot_bound | Stable |
| Dual Simplex | cxf_pivot_bound, bound flip support | Stable |

### 6.2 API Stability

The following interfaces are stable and must not change:

- [x] cxf_ratio_test output semantics (leaving var, pivot, step)
- [x] Error code meanings (0 = success, 5 = unbounded)
- [x] Bound operation side effects on RHS and objective

## 7. Invariants

### 7.1 Module Invariants

What this module guarantees:

- [x] Selected leaving variable yields largest pivot among near-optimal candidates
- [x] Unboundedness is correctly detected when no ratio limits step
- [x] Bound operations correctly update objective and constraints
- [x] Dual pricing arrays remain consistent after updates

### 7.2 Required Invariants

What this module requires from others:

- [x] Pivot column (FTRAN result) is correctly computed
- [x] Basis header accurately identifies basic variables
- [x] Variable bounds are consistent (lb <= ub)
- [x] Variable status reflects actual position (at bound or basic)

## 8. Error Handling

### 8.1 Error Detection

How errors are detected in this module:

| Error Type | Detection Method |
|------------|------------------|
| Unbounded problem | No limiting ratio exists |
| Memory allocation failure | NULL return from allocator |
| Invalid pivot column | All coefficients below threshold |

### 8.2 Error Propagation

How errors flow through this module:

```
Unbounded --> Return 5 (UNBOUNDED)
Memory failure --> Return 1001 (OUT_OF_MEMORY)
No valid pivot --> May indicate numerical issues
```

### 8.3 Recovery Strategies

| Error Type | Recovery |
|------------|----------|
| Unbounded | Report to user, terminate solve |
| Memory failure | Abort current operation |
| Numerical issues | Trigger basis refactorization |

## 9. Thread Safety

### 9.1 Concurrency Model

The ratio test operates in a single-threaded context per solver. Each solver maintains its own working arrays; no cross-thread sharing occurs.

### 9.2 Synchronization Primitives

| Primitive | Protects | Granularity |
|-----------|----------|-------------|
| None | N/A | Per-solver isolation |

### 9.3 Thread Safety Guarantees

- [x] cxf_ratio_test is read-only on shared state
- [x] cxf_pivot_bound modifies only per-solver state
- [x] Concurrent solves are safe

### 9.4 Known Race Conditions

None when each solver operates on its own thread.

## 10. Performance Characteristics

### 10.1 Complexity Summary

| Operation | Time | Space |
|-----------|------|-------|
| cxf_ratio_test | O(m) dense, O(nnz) sparse | O(m) for candidates |
| cxf_pivot_bound | O(nnz_j + neighbors) | O(1) if no eta |
| cxf_pivot_check | O(nnz_j) | O(1) |
| cxf_pivot_special | O(nnz_j + rows) | O(1) |

Where: m = constraints, nnz = nonzeros in pivot column, nnz_j = column density

### 10.2 Hot Paths

cxf_ratio_test is called every iteration and is performance-critical:
- Two passes over pivot column (Harris approach)
- Must iterate efficiently over sparse columns
- Candidate list management overhead

Optimizations:
- Sparse iteration using column index arrays
- Combined first/second pass when pivot column is short
- Early termination when excellent pivot found

### 10.3 Memory Usage

- Pivot column: O(m) doubles (temporary)
- Candidate list: O(m) integers (reused)
- Dual pricing: O(m) doubles per array (2-4 arrays)
- Typical: ~40 bytes per constraint

## 11. Function Index

Complete list of functions in this module with links to individual specs:

### Public Functions

1. [cxf_ratio_test](functions/ratio_test/cxf_ratio_test.md) - Harris two-pass ratio test
2. [cxf_pivot_bound](functions/ratio_test/cxf_pivot_bound.md) - Move variable to bound
3. [cxf_pivot_check](functions/ratio_test/cxf_pivot_check.md) - Validate bound change feasibility
4. [cxf_pivot_special](functions/ratio_test/cxf_pivot_special.md) - Handle special cases

## 12. Design Decisions

### 12.1 Key Design Choices

| Decision | Rationale | Alternatives Considered |
|----------|-----------|------------------------|
| Harris two-pass | Balance stability and optimality | Standard ratio test (less stable) |
| 10x tolerance for first pass | Empirically effective relaxation | 2x, 100x (less/more relaxed) |
| Sparse iteration | Efficient for typical LP sparsity | Dense iteration (simpler) |
| Integrated bound operations | Unified handling of related ops | Separate modules |

### 12.2 Known Limitations

- [x] Dense pivot columns may be slow
- [x] Harris overhead adds ~100% to ratio test time
- [x] Dual pricing arrays consume O(m) memory

### 12.3 Future Improvements

- [x] Vectorized ratio computation for dense columns
- [x] Adaptive Harris factor based on problem conditioning
- [x] Lazy dual pricing array updates

## 13. References

- Dantzig, G.B. (1963). *Linear Programming and Extensions*. Princeton University Press.
- Harris, P.M.J. (1973). "Pivot selection methods of the Devex LP code." *Mathematical Programming Study*, 4, 30-57.
- Tomlin, J.A. (1972). "Pivoting for size and sparsity in linear programming inversion routines."
- Koberstein, A. (2005). *The dual simplex method, techniques for a fast and stable implementation*.
- Huangfu, Q. and Hall, J.A.J. (2018). "Parallelizing the dual revised simplex method."

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
