# Module: Pricing

**Spec Version:** 1.0
**Last Updated:** 2026-01-25
**Functions:** 6

## 1. Overview

### 1.1 Purpose

The Pricing module implements variable selection strategies for the simplex algorithm. In each simplex iteration, pricing determines which non-basic variable should enter the basis. This decision profoundly affects solver performance: good pricing reduces iteration count while bad pricing can lead to cycling or excessive iterations.

The module provides a multi-level partial pricing system with steepest edge selection. Rather than examining all n variables at each iteration (O(n) cost), partial pricing examines progressively smaller candidate sets, achieving O(sqrt(n)) average cost for large problems. Combined with steepest edge pricing (which normalizes reduced costs by edge norms), this typically reduces total iteration count by 20-40% compared to standard Dantzig pricing.

The pricing state maintains cached results and candidate lists across iterations, invalidating and recomputing only when necessary (after pivots or bound changes).

### 1.2 Responsibilities

This module is responsible for:

- [x] Selecting the entering variable for each simplex iteration
- [x] Computing and caching reduced costs for candidate variables
- [x] Maintaining multi-level candidate lists (hierarchical pricing)
- [x] Implementing steepest edge pricing with approximate edge norms
- [x] Updating pricing state after pivots
- [x] Invalidating caches when problem data changes
- [x] Detecting optimality (no improving variable exists)
- [x] Detecting unboundedness (infinite improvement possible)

This module is NOT responsible for:

- [ ] Computing the actual pivot column (FTRAN - handled by Basis)
- [ ] Determining the leaving variable (handled by Ratio Test)
- [ ] Executing the pivot (handled by Pivot module)
- [ ] Managing bound values (handled by Simplex core)

### 1.3 Design Philosophy

The module embodies several optimization principles:

**Hierarchical refinement**: Variables are organized in levels, from all variables (level 0) to increasingly selective subsets. The algorithm operates at refined levels when possible, falling back only when no improving variable is found.

**Lazy evaluation**: Candidate lists and reduced costs are computed on-demand and cached. This avoids redundant computation when the same candidates remain attractive across iterations.

**Quality over speed for selection**: Steepest edge pricing spends more time selecting the best variable, but this investment pays off through fewer total iterations. The criterion balances reduced cost magnitude against the expected step length.

**Cache coherence**: The caching strategy exploits the fact that most pivots affect only a small subset of reduced costs, allowing selective invalidation rather than full recomputation.

## 2. Public Interface

### 2.1 Exported Functions

| Function | Purpose | Typical Caller |
|----------|---------|----------------|
| cxf_pricing_init | Initialize multi-level pricing state | cxf_simplex_setup |
| cxf_pricing_candidates | Get/compute candidate list for current level | cxf_pricing_steepest |
| cxf_pricing_steepest | Select entering variable via steepest edge | cxf_simplex_step |
| cxf_pricing_update | Update state after pivot operation | cxf_pivot_primal |
| cxf_pricing_invalidate | Invalidate caches for variable or all | cxf_basis_refactor |
| cxf_pricing_step2 | Lightweight candidate access for phase 2 | cxf_simplex_step2 |

### 2.2 Exported Types

| Type | Purpose |
|------|---------|
| PricingState | Complete pricing module state (candidate lists, caches) |

### 2.3 Exported Constants

| Constant | Value | Purpose |
|----------|-------|---------|
| PRICING_OPTIMAL | 3 | No improving variable (optimality) |
| PRICING_UNBOUNDED | 5 | Unbounded improvement possible |
| PRICING_CONTINUE | 0 | Variable selected, continue iteration |

## 3. Internal Functions

### 3.1 Private Functions

| Function | Purpose |
|----------|---------|
| compute_reduced_cost | Calculate reduced cost for single variable |
| update_edge_norm | Update steepest edge norm after pivot |
| rebuild_candidates | Regenerate candidate list for level |

### 3.2 Helper Functions

| Function | Purpose | Used By |
|----------|---------|---------|
| score_variable | Compute steepest edge criterion | cxf_pricing_steepest |
| expand_neighbors | Add neighbor variables to candidates | cxf_pricing_update |

## 4. State Management

### 4.1 Module State

| State Element | Type | Lifetime | Thread Safety |
|--------------|------|----------|---------------|
| PricingState | struct | Solve lifetime | Per-solver |
| Candidate arrays | int*[] | Solve lifetime | Per-solver |
| Cache validity flags | int[] | Per-iteration | Per-solver |
| Current level | int | Per-iteration | Per-solver |

### 4.2 State Lifecycle

```
UNINITIALIZED
      |
      v (cxf_pricing_init)
  INITIALIZED -----> All caches invalid, level 0
      |
      v (cxf_pricing_steepest)
    ACTIVE ---------> Candidates computed, caches populated
      |
      v (cxf_pricing_update)
  PARTIAL_VALID ---> Some caches invalidated
      |
      v (cxf_pricing_invalidate)
  RESET -----------> Caches cleared, return to level 0
      |
      v (solve complete)
  DESTROYED --------> Memory freed
```

### 4.3 State Invariants

At all times, the following must be true:

- [x] Current level is within [0, maxLevel]
- [x] Cached candidate counts are -1 (invalid) or >= 0 (valid)
- [x] Candidate arrays have sufficient capacity for their level
- [x] Edge norms are positive when valid

## 5. Dependencies

### 5.1 Required Modules

| Module | What We Use | Why |
|--------|-------------|-----|
| Memory | cxf_calloc, cxf_free | Array allocation |
| Parameters | cxf_get_optim_tol | Optimality tolerance |
| Basis | cxf_btran | Compute dual prices for reduced costs |

### 5.2 Initialization Order

This module must be initialized:
- **After:** Memory, Parameters, Basis
- **Before:** Simplex iteration loop

### 5.3 External Dependencies

| Dependency | Type | Purpose |
|------------|------|---------|
| sqrt, fabs | C math | Steepest edge norm computation |
| memset | C stdlib | Cache invalidation |

## 6. Dependents

### 6.1 Modules That Depend on Us

| Module | What They Use | Stability |
|--------|---------------|-----------|
| Simplex | cxf_pricing_steepest | Stable |
| Pivot | cxf_pricing_update | Stable |
| Basis | cxf_pricing_invalidate | Stable |

### 6.2 API Stability

The following interfaces are stable and must not change:

- [x] cxf_pricing_steepest return values (entering variable, reduced cost)
- [x] PricingState pointer parameter convention
- [x] Invalidation semantics (-1 for global, >= 0 for specific variable)

## 7. Invariants

### 7.1 Module Invariants

What this module guarantees:

- [x] Selected entering variable has negative reduced cost (for minimization)
- [x] Steepest edge criterion is correctly computed for candidates
- [x] Caches are conservative (may be invalid, never incorrectly valid)
- [x] Level progression is monotonic within iteration

### 7.2 Required Invariants

What this module requires from others:

- [x] Objective coefficients are current
- [x] Variable status array is consistent with bounds
- [x] BTRAN produces correct dual prices

## 8. Error Handling

### 8.1 Error Detection

How errors are detected in this module:

| Error Type | Detection Method |
|------------|------------------|
| Memory allocation failure | NULL return from allocator |
| No improving variable | Empty candidate set after level 0 scan |
| Unbounded direction | Improving variable with no bound in direction |

### 8.2 Error Propagation

How errors flow through this module:

```
Memory failure --> Return 1001 (OUT_OF_MEMORY)
Optimality    --> Return 3 (OPTIMAL status code)
Unbounded     --> Return 5 (UNBOUNDED status code)
```

### 8.3 Recovery Strategies

| Error Type | Recovery |
|------------|----------|
| Memory failure | Caller must abort solve |
| Optimality | Not an error - solution found |
| Unbounded | Not an error - report to user |

## 9. Thread Safety

### 9.1 Concurrency Model

Pricing operates in a single-threaded context per solver. Each SolverState has its own PricingState; no cross-thread sharing occurs during optimization.

### 9.2 Synchronization Primitives

| Primitive | Protects | Granularity |
|-----------|----------|-------------|
| None required | N/A | Per-solver isolation |

### 9.3 Thread Safety Guarantees

- [x] Separate solvers have independent pricing state
- [x] No global mutable state
- [x] Concurrent solves are safe

### 9.4 Known Race Conditions

None when each solver operates on its own thread.

## 10. Performance Characteristics

### 10.1 Complexity Summary

| Operation | Time | Space |
|-----------|------|-------|
| cxf_pricing_init | O(n) | O(n) for candidate arrays |
| cxf_pricing_candidates | O(1) to O(n) | O(1) (uses existing arrays) |
| cxf_pricing_steepest | O(k) | O(1) |
| cxf_pricing_update | O(L + k) | O(1) |
| cxf_pricing_invalidate | O(L) or O(L*k) | O(1) |

Where: n = variables, k = candidates at current level, L = number of levels (3-5)

### 10.2 Hot Paths

cxf_pricing_steepest is the critical path, called every iteration:
- Must iterate over candidate set efficiently
- Edge norm lookup must be O(1)
- Score comparison must be cache-friendly

Optimizations:
- Candidate arrays are contiguous for cache efficiency
- Edge norms stored in parallel array
- Early termination when excellent candidate found

### 10.3 Memory Usage

- Candidate arrays: O(n) total across all levels
- Edge norms: O(n) doubles
- Cache validity: O(L) integers
- Typical: ~20 bytes per variable

## 11. Function Index

Complete list of functions in this module with links to individual specs:

### Public Functions

1. [cxf_pricing_init](functions/pricing/cxf_pricing_init.md) - Initialize multi-level pricing state
2. [cxf_pricing_candidates](functions/pricing/cxf_pricing_candidates.md) - Get/compute candidate list for level
3. [cxf_pricing_steepest](functions/pricing/cxf_pricing_steepest.md) - Select entering variable via steepest edge
4. [cxf_pricing_update](functions/pricing/cxf_pricing_update.md) - Update state after pivot
5. [cxf_pricing_invalidate](functions/pricing/cxf_pricing_invalidate.md) - Invalidate caches
6. [cxf_pricing_step2](functions/pricing/cxf_pricing_step2.md) - Lightweight candidate access

## 12. Design Decisions

### 12.1 Key Design Choices

| Decision | Rationale | Alternatives Considered |
|----------|-----------|------------------------|
| Multi-level partial pricing | O(sqrt(n)) vs O(n) per iteration | Full pricing, single-level partial |
| Steepest edge criterion | 20-40% fewer iterations | Dantzig (simple), Devex (approximate) |
| Caching with invalidation | Exploit iteration locality | Always recompute |
| Level sizes as sqrt ratios | Balanced hierarchy | Fixed sizes, linear ratios |

### 12.2 Known Limitations

- [x] Steepest edge norms degrade over many iterations (need periodic reset)
- [x] Partial pricing may miss globally best variable
- [x] Cache memory overhead significant for huge problems

### 12.3 Future Improvements

- [x] Adaptive level sizing based on problem characteristics
- [x] Parallel candidate scoring for large candidate sets
- [x] Hybrid Devex/steepest edge selection

## 13. References

- Dantzig, G. (1963). *Linear Programming and Extensions*.
- Goldfarb, D. and Reid, J. (1977). "A practicable steepest-edge simplex algorithm."
- Harris, P.M.J. (1973). "Pivot selection methods of the Devex LP code."
- Maros, I. (2003). *Computational Techniques of the Simplex Method*.
- Forrest, J.J. and Goldfarb, D. (1992). "Steepest-edge simplex algorithms for linear programming."

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
