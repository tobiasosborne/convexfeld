# Module: Crossover

**Spec Version:** 1.0
**Last Updated:** 2026-01-25
**Functions:** 2

## 1. Overview

### 1.1 Purpose

The Crossover module implements barrier-to-simplex conversion algorithms that transform interior-point solutions into basic feasible solutions. This transformation is essential in hybrid optimization strategies where a barrier (interior-point) method quickly produces a near-optimal interior solution, which is then "crossed over" to a vertex solution that simplex methods can efficiently refine to exact optimality.

Interior-point methods produce solutions where variables typically lie strictly within their bounds, satisfying the Karush-Kuhn-Tucker (KKT) conditions approximately. In contrast, simplex methods operate on vertex solutions where at most m variables (for m constraints) are non-basic, and all others are at bounds. The crossover module bridges this gap efficiently.

The result is a warm-start basis for primal or dual simplex algorithms that can complete optimization in relatively few iterations (typically 10-1000 vs thousands from scratch).

### 1.2 Responsibilities

This module is responsible for:

- [x] Bound snapping: Moving variables close to bounds to exact bound values
- [x] Quadratic optimization: Positioning variables with quadratic objective terms optimally
- [x] Basis identification: Classifying variables as basic, nonbasic, or superbasic
- [x] Binary quadratic simplification: Converting x^2 = x for binary variables
- [x] Problem simplification: Reducing superbasic count before simplex cleanup
- [x] Feasibility preservation: Maintaining constraint satisfaction during conversion

This module is NOT responsible for:

- [ ] Barrier (interior-point) method solution (handled by Barrier module)
- [ ] Simplex cleanup iterations (handled by Simplex module)
- [ ] Basis factorization (handled by Basis module)
- [ ] Final optimality verification (handled by Simplex module)

### 1.3 Design Philosophy

The module follows several key design principles:

**Objective-aware conversion**: Rather than blindly snapping to nearest bounds, the crossover considers objective improvement when deciding variable positions. Variables with positive reduced costs go to lower bounds; negative reduced costs to upper bounds.

**Quadratic optimization**: For QP problems, variables with diagonal quadratic terms are positioned at their 1D optimal point within bounds, directly improving objective during crossover rather than leaving it to simplex.

**Exploiting problem structure**: Binary variables have the special property that x^2 = x, allowing quadratic-to-linear conversion that can dramatically simplify subsequent optimization.

**Conservative transformation**: All transformations maintain feasibility. The crossover may not achieve exact optimality, but it guarantees the simplex warm-start begins from a valid point.

## 2. Public Interface

### 2.1 Exported Functions

| Function | Purpose | Typical Caller |
|----------|---------|----------------|
| cxf_crossover | Main crossover with quadratic optimization | cxf_barrier_solve, cxf_solve_lp |
| cxf_crossover_bounds | Bound snapping for linear problems | cxf_crossover (as first pass) |

### 2.2 Exported Types

| Type | Purpose |
|------|---------|
| CrossoverResult | Status and statistics from crossover |

### 2.3 Exported Constants

| Constant | Value | Purpose |
|----------|-------|---------|
| CROSSOVER_SUCCESS | 0 | Crossover completed successfully |
| CROSSOVER_UNBOUNDED | 5 | Variable value exceeds infinity/2 |
| SNAP_TOLERANCE | 1e-6 to 1e-8 | Distance threshold for bound snapping |

## 3. Internal Functions

### 3.1 Private Functions

| Function | Purpose |
|----------|---------|
| optimize_1d_quadratic | Find optimal x in [lb, ub] for c*x + (Q/2)*x^2 |
| convert_binary_quadratic | Transform x^2 to x for binary variables |
| classify_variable | Determine basic/nonbasic/superbasic status |

### 3.2 Helper Functions

| Function | Purpose | Used By |
|----------|---------|---------|
| distance_to_bound | Compute min distance to either bound | cxf_crossover_bounds |
| compute_reduced_cost | Get reduced cost for classification | cxf_crossover |

## 4. State Management

### 4.1 Module State

| State Element | Type | Lifetime | Thread Safety |
|--------------|------|----------|---------------|
| Variable values | double[] | Modified in place | Per-solver |
| Variable status | int[] | Modified in place | Per-solver |
| Basis header | int[] | May be modified | Per-solver |
| Binary conversion count | int | Solve duration | Per-solver |

### 4.2 State Lifecycle

```
BARRIER_COMPLETE
      |
      v (interior-point solution available)
  INTERIOR_SOLUTION --> All variables strictly within bounds
      |
      v (cxf_crossover_bounds - optional first pass)
  PARTIAL_SNAP --> Near-bound variables snapped
      |
      v (cxf_crossover - main pass)
QUADRATIC_OPTIMIZED --> Variables at optimal 1D positions
      |
      v (binary conversion if applicable)
BINARY_CONVERTED --> x^2 = x simplification applied
      |
      v
CROSSOVER_COMPLETE --> Basic feasible solution for simplex
      |
      v (simplex cleanup)
    OPTIMAL --> Final vertex solution
```

### 4.3 State Invariants

At all times during crossover:

- [x] Constraint feasibility is maintained (Ax = b within tolerance)
- [x] Variable bounds are respected (lb <= x <= ub)
- [x] At most m variables can remain superbasic
- [x] Objective value improves or stays same (never worsens significantly)

## 5. Dependencies

### 5.1 Required Modules

| Module | What We Use | Why |
|--------|-------------|-----|
| Simplex | cxf_simplex_chgvar_primal/dual | Variable state updates |
| Basis | cxf_basis_refactor | Refactorization after conversion |
| Pivot | cxf_pivot_bound | Move variables to bounds |
| Pricing | cxf_pricing_invalidate_var | Clear pricing caches |
| Parameters | FeasibilityTol, CrossoverBinary | Configuration |

### 5.2 Initialization Order

This module must be initialized:
- **After:** Barrier solve complete, state structures allocated
- **Before:** Simplex cleanup iterations

### 5.3 External Dependencies

| Dependency | Type | Purpose |
|------------|------|---------|
| fabs, sqrt | C math | Distance and magnitude computation |

## 6. Dependents

### 6.1 Modules That Depend on Us

| Module | What They Use | Stability |
|--------|---------------|-----------|
| Barrier | cxf_crossover | Stable |
| Hybrid Optimizer | cxf_crossover | Stable |
| Reoptimization | cxf_crossover | Stable |

### 6.2 API Stability

The following interfaces are stable and must not change:

- [x] cxf_crossover preserves feasibility
- [x] Variable status encoding after crossover
- [x] Error code meanings (0 = success, 5 = unbounded)

## 7. Invariants

### 7.1 Module Invariants

What this module guarantees:

- [x] Crossover produces valid warm-start basis for simplex
- [x] Constraint feasibility is preserved throughout
- [x] Objective value does not worsen significantly (|delta| < tolerance)
- [x] Superbasic count is minimized (typically < 0.1 * initial)
- [x] Binary variables with Q terms have them converted to linear

### 7.2 Required Invariants

What this module requires from others:

- [x] Interior-point solution is feasible and near-optimal
- [x] Bounds arrays are valid (lb <= ub)
- [x] Variable type information is accurate (especially binary flags)
- [x] Quadratic diagonal information is available

## 8. Error Handling

### 8.1 Error Detection

How errors are detected in this module:

| Error Type | Detection Method |
|------------|------------------|
| Unbounded variable | Value exceeds infinity/2 |
| Memory allocation | NULL return from allocator |
| Invalid basis update | Error from chgvar functions |

### 8.2 Error Propagation

How errors flow through this module:

```
Sub-operation error --> Return immediately
                   --> Partial conversion state remains
                   --> Caller must decide on recovery
```

### 8.3 Recovery Strategies

| Error Type | Recovery |
|------------|----------|
| Unbounded | Return code 5, abort crossover |
| Memory | Return code 1001, abort |
| Partial conversion | Can continue with simplex from current state |

## 9. Thread Safety

### 9.1 Concurrency Model

Crossover operates on per-solver state only. No global mutable state exists. The module reads environment parameters and writes solver state.

### 9.2 Synchronization Primitives

| Primitive | Protects | Granularity |
|-----------|----------|-------------|
| None | N/A | Per-solver isolation |

### 9.3 Thread Safety Guarantees

- [x] Concurrent crossovers on different solvers are safe
- [x] No global state modified
- [x] Environment must not change during crossover

### 9.4 Known Race Conditions

None when properly used.

## 10. Performance Characteristics

### 10.1 Complexity Summary

| Operation | Time | Space |
|-----------|------|-------|
| cxf_crossover_bounds | O(n + k*m) | O(1) |
| cxf_crossover | O(n*m) | O(1) |

Where: n = variables, m = constraints, k = variables snapped

### 10.2 Hot Paths

Crossover is called once per barrier solve, not in a hot loop. Performance is dominated by:
- Variable change operations (O(m) each in worst case)
- Pricing cache invalidation
- Optional refactorization

Typical crossover: 5-20% of total barrier+simplex time

### 10.3 Memory Usage

Crossover uses minimal additional memory:
- O(1) for temporary variables
- Existing solver state arrays are modified in place
- No new allocations in typical case

## 11. Function Index

Complete list of functions in this module with links to individual specs:

### Public Functions

1. [cxf_crossover](functions/crossover/cxf_crossover.md) - Main crossover with quadratic optimization
2. [cxf_crossover_bounds](functions/crossover/cxf_crossover_bounds.md) - Bound snapping for linear problems

## 12. Design Decisions

### 12.1 Key Design Choices

| Decision | Rationale | Alternatives Considered |
|----------|-----------|------------------------|
| Two-pass approach | Bounds first, then quadratic | Single pass, quadratic first |
| 1D quadratic optimization | Directly improve objective | Leave to simplex |
| Binary conversion | Major simplification for MIQP | Skip, handle in simplex |
| Selective snapping | Only near-bound variables | Snap all to nearest |

### 12.2 Known Limitations

- [x] Integer variables other than binary not specially handled
- [x] Off-diagonal quadratic terms not optimized (only diagonal)
- [x] Highly degenerate problems may have poor crossover quality

### 12.3 Future Improvements

- [x] Multi-variable quadratic optimization for dense Q
- [x] Improved basis selection heuristics
- [x] Parallel variable processing for large problems

## 13. References

- Bixby, R.E. (2002). "Solving Real-World Linear Programs: A Decade and More of Progress." *Operations Research*, 50(1), 3-15.
- Mehrotra, S. (1992). "On the Implementation of a Primal-Dual Interior Point Method." *SIAM Journal on Optimization*, 2(4), 575-601.
- Andersen, E.D., Gondzio, J., Meszaros, C., & Xu, X. (1996). "Implementation of Interior Point Methods for Large Scale Linear Programming."
- Benson, H.Y., & Shanno, D.F. (2008). "Interior-Point Methods for Nonconvex Nonlinear Programming."
- Wright, S.J. (1997). *Primal-Dual Interior-Point Methods*. SIAM.
- Nocedal, J., & Wright, S.J. (2006). *Numerical Optimization*, 2nd edition. Springer.
- Vanderbei, R.J. (2014). *Linear Programming: Foundations and Extensions*, 4th edition. Springer.

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
