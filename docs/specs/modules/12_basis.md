# Module: Basis Operations

**Spec Version:** 1.0
**Last Updated:** 2026-01-25
**Functions:** 10

## 1. Overview

### 1.1 Purpose

The Basis Operations module implements the Product Form of Inverse (PFI) representation and manipulation of the simplex basis matrix. In the simplex method, the basis matrix B consists of m linearly independent columns from the constraint matrix. Rather than storing and updating B^(-1) explicitly (which would require O(m^2) storage and O(m^3) updates), the PFI approach represents B^(-1) as a product of elementary matrices called eta vectors.

This module provides the core numerical engine for the simplex algorithm: forward and backward transformations (FTRAN/BTRAN) for solving linear systems, basis refactorization to reset accumulated numerical errors, and basis state management for warm-starting and cycling detection.

The module enables efficient basis updates with O(d) complexity per pivot (where d is the density of the eta vector) rather than O(m^2) for explicit inverse updates, making large-scale LP solving practical.

### 1.2 Responsibilities

This module is responsible for:

- [x] Forward transformation (FTRAN): Solving Bx = b for x
- [x] Backward transformation (BTRAN): Solving y^T B = c^T for y
- [x] Basis refactorization: Rebuilding B^(-1) from scratch to clear accumulated errors
- [x] Eta vector management: Creating, storing, and applying elementary update matrices
- [x] Basis snapshots: Saving and restoring basis state for warm-starting
- [x] Basis comparison: Detecting cycling through basis equality checks
- [x] Basis validation: Verifying structural integrity of basis state

This module is NOT responsible for:

- [ ] Pivot selection (handled by Pricing module)
- [ ] Ratio test calculations (handled by Ratio Test module)
- [ ] Actual pivot execution (handled by Pivot module)
- [ ] Problem matrix storage (handled by Matrix module)

### 1.3 Design Philosophy

The module follows several key design principles:

**Lazy evaluation**: Rather than maintaining an explicit B^(-1), transformations are computed on-demand by applying eta vectors. This trades computation time for memory and enables efficient updates.

**Periodic reset**: Numerical errors accumulate through repeated eta applications. Periodic refactorization resets these errors, balancing computation cost against numerical stability.

**Minimal storage**: Eta vectors store only non-zero entries, exploiting sparsity in the basis updates.

**Separation of concerns**: The module handles only basis representation and transformation; pivot selection and execution are delegated to other modules.

## 2. Public Interface

### 2.1 Exported Functions

| Function | Purpose | Typical Caller |
|----------|---------|----------------|
| cxf_btran | Backward transformation (y^T B = c^T) | Pricing module (compute dual prices) |
| cxf_ftran | Forward transformation (Bx = b) | Ratio test (compute pivot column) |
| cxf_basis_refactor | Rebuild basis inverse from scratch | Simplex driver (periodic refresh) |
| cxf_basis_snapshot | Save basis state for later restore | Anti-cycling detection |
| cxf_basis_diff | Count differences between two bases | Cycling detection |
| cxf_basis_equal | Check if two bases are identical | Cycling detection |
| cxf_basis_validate | Verify basis structure integrity | Warm start validation |
| cxf_basis_warm | Restore previously saved basis | Warm starting |

### 2.2 Exported Types

| Type | Purpose |
|------|---------|
| EtaVector | Elementary matrix representing single basis update |
| BasisSnapshot | Saved basis state for comparison |
| BasisState | Complete basis representation for warm start |

### 2.3 Exported Constants

| Constant | Value | Purpose |
|----------|-------|---------|
| STATUS_NONBASIC_LOWER | -1 | Variable at lower bound |
| STATUS_NONBASIC_UPPER | -2 | Variable at upper bound |
| STATUS_SUPERBASIC | -3 | Free variable between bounds |
| STATUS_FIXED | -4 | Variable with lb = ub |
| ETA_TYPE_COLUMN | 1 | Eta from column pivot |
| ETA_TYPE_ROW | 2 | Eta from row operation |

## 3. Internal Functions

### 3.1 Private Functions

| Function | Purpose |
|----------|---------|
| cxf_pivot_with_eta | Create Type 2 eta vector for pivot update |
| cxf_timing_refactor | Pricing with work accumulation tracking |

### 3.2 Helper Functions

| Function | Purpose | Used By |
|----------|---------|---------|
| compute_hash | Quick basis comparison hash | cxf_basis_snapshot |
| apply_eta | Apply single eta to vector | cxf_ftran, cxf_btran |

## 4. State Management

### 4.1 Module State

| State Element | Type | Lifetime | Thread Safety |
|--------------|------|----------|---------------|
| Eta list head | EtaVector* | Solve lifetime | Not shared |
| Eta count | int | Solve lifetime | Not shared |
| Variable status array | int* | Solve lifetime | Not shared |
| Basis header | int* | Solve lifetime | Not shared |

### 4.2 State Lifecycle

```
UNINITIALIZED
      |
      v (cxf_simplex_init)
  INITIALIZED -----> Eta list empty, status set from crash
      |
      v (pivots accumulate)
    ACTIVE ---------> Eta vectors accumulate
      |
      v (cxf_basis_refactor)
  REFACTORED -------> Eta list cleared, basis rebuilt
      |
      v (more pivots)
    ACTIVE ---------> Cycle continues
      |
      v (cxf_simplex_final)
  DESTROYED --------> All memory freed
```

### 4.3 State Invariants

At all times, the following must be true:

- [x] Number of basic variables equals number of constraints (m)
- [x] Each constraint row has exactly one basic variable
- [x] Variable status is consistent with basis header
- [x] Eta list represents valid product form of B^(-1)

## 5. Dependencies

### 5.1 Required Modules

| Module | What We Use | Why |
|--------|-------------|-----|
| Memory | cxf_alloc_eta, cxf_free | Eta vector allocation |
| Parameters | cxf_get_feasibility_tol | Numerical tolerances |
| Matrix | Column access functions | Matrix coefficients |

### 5.2 Initialization Order

This module must be initialized:
- **After:** Memory, Matrix, Parameters
- **Before:** Pricing, Ratio Test, Pivot, Simplex

### 5.3 External Dependencies

| Dependency | Type | Purpose |
|------------|------|---------|
| Standard C math | Library | fabs, fmax for numerical operations |
| memcpy/memset | Library | Array copying and zeroing |

## 6. Dependents

### 6.1 Modules That Depend on Us

| Module | What They Use | Stability |
|--------|---------------|-----------|
| Pricing | cxf_btran | Stable interface |
| Ratio Test | cxf_ftran | Stable interface |
| Simplex | cxf_basis_refactor, snapshots | Stable interface |
| Crossover | cxf_basis_validate, cxf_basis_warm | Stable interface |

### 6.2 API Stability

The following interfaces are stable and must not change:

- [x] cxf_ftran, cxf_btran signatures
- [x] Variable status encoding constants
- [x] EtaVector memory layout (for persistence)

## 7. Invariants

### 7.1 Module Invariants

What this module guarantees:

- [x] FTRAN and BTRAN produce mathematically correct solutions to specified tolerance
- [x] Refactorization produces identical results to eta chain (within tolerance)
- [x] Basis snapshots can be restored without side effects
- [x] Eta list memory is properly managed (no leaks)

### 7.2 Required Invariants

What this module requires from others:

- [x] Eta pool provides valid memory blocks
- [x] Matrix coefficients are accessible and unchanged during transformation
- [x] Variable status array is properly allocated

## 8. Error Handling

### 8.1 Error Detection

How errors are detected in this module:

| Error Type | Detection Method |
|------------|------------------|
| Memory allocation failure | NULL return from allocator |
| Invalid basis structure | Consistency check in validate |
| Numerical instability | Small pivot element detection |

### 8.2 Error Propagation

How errors flow through this module:

```
Allocation failure --> Return error code immediately
Validation failure --> Return INVALID_ARGUMENT (1003)
Numerical issue --> May trigger early refactorization
```

### 8.3 Recovery Strategies

| Error Type | Recovery |
|------------|----------|
| Memory failure | Return 1001, caller must handle |
| Invalid basis | Return error, caller should not continue |
| Accumulated error | Trigger refactorization to reset |

## 9. Thread Safety

### 9.1 Concurrency Model

The basis module operates in a single-threaded context within each solver instance. Multiple solver instances can operate concurrently as they maintain completely separate state.

### 9.2 Synchronization Primitives

| Primitive | Protects | Granularity |
|-----------|----------|-------------|
| None | N/A | Per-solver isolation |

### 9.3 Thread Safety Guarantees

- [x] Separate solver instances are fully independent
- [x] No global mutable state exists
- [x] Eta pool allocation may require synchronization if shared

### 9.4 Known Race Conditions

None when used correctly (one thread per solver instance).

## 10. Performance Characteristics

### 10.1 Complexity Summary

| Operation | Time | Space |
|-----------|------|-------|
| cxf_btran | O(k * d) | O(m) work array |
| cxf_ftran | O(k * d) | O(m) work array |
| cxf_basis_refactor | O(n * d) | O(nnz) for LU factors |
| cxf_basis_snapshot | O(n + m) | O(n + m) for copy |
| cxf_basis_diff | O(n + m) | O(1) |

Where: k = eta count, d = average eta density, n = variables, m = constraints

### 10.2 Hot Paths

FTRAN and BTRAN are the performance-critical paths, called on every simplex iteration:
- BTRAN for pricing (computing dual prices)
- FTRAN for ratio test (computing pivot column)

Optimizations:
- Sparse eta representation reduces work
- Cache-friendly traversal order
- Early termination when result becomes sparse

### 10.3 Memory Usage

- Eta vectors: ~50-500 bytes each, accumulate until refactorization
- Typical eta chain: 100-1000 etas between refactorizations
- Working arrays: O(m) doubles for transformation scratch space
- Snapshot: O(n + m) integers plus metadata

## 11. Function Index

Complete list of functions in this module with links to individual specs:

### Public Functions

1. [cxf_btran](functions/basis/cxf_btran.md) - Backward transformation solving y^T B = c^T
2. [cxf_ftran](functions/basis/cxf_ftran.md) - Forward transformation solving Bx = b
3. [cxf_basis_refactor](functions/basis/cxf_basis_refactor.md) - Rebuild basis inverse from current basis
4. [cxf_basis_snapshot](functions/basis/cxf_basis_snapshot.md) - Save current basis state
5. [cxf_basis_diff](functions/basis/cxf_basis_diff.md) - Count differences between bases
6. [cxf_basis_equal](functions/basis/cxf_basis_equal.md) - Check if two bases are identical
7. [cxf_basis_validate](functions/basis/cxf_basis_validate.md) - Verify basis structure integrity
8. [cxf_basis_warm](functions/basis/cxf_basis_warm.md) - Restore previously saved basis

### Internal Functions

1. [cxf_pivot_with_eta](functions/basis/cxf_pivot_with_eta.md) - Create Type 2 eta vector for pivot
2. [cxf_timing_refactor](functions/basis/cxf_timing_refactor.md) - Pricing with work tracking

## 12. Design Decisions

### 12.1 Key Design Choices

| Decision | Rationale | Alternatives Considered |
|----------|-----------|------------------------|
| Product Form of Inverse | O(d) updates vs O(m^2) for explicit | Explicit inverse, LU with updates |
| Linked list for etas | O(1) insertion, natural traversal | Array (reallocation issues) |
| Periodic refactorization | Bounded error accumulation | Continuous error correction |
| Sparse eta storage | Exploits problem structure | Dense storage |

### 12.2 Known Limitations

- [x] Eta chain length bounded by memory (typically 2-5 * m)
- [x] Very dense problems reduce PFI effectiveness
- [x] No parallel FTRAN/BTRAN (inherently sequential)

### 12.3 Future Improvements

- [x] LU factorization with Forrest-Tomlin updates (more stable)
- [x] Hyper-sparse techniques for very sparse problems
- [x] Adaptive refactorization threshold based on fill-in

## 13. References

- Dantzig, G. B. (1963). *Linear Programming and Extensions*. Princeton University Press.
- Bartels, R. H. and Golub, G. H. (1969). "The simplex method of linear programming using LU decomposition."
- Forrest, J. J. and Tomlin, J. A. (1972). "Updated triangular factors of the basis."
- Maros, I. (2003). *Computational Techniques of the Simplex Method*. Springer.
- Hall, J.A.J. and McKinnon, K.I.M. (2005). "Hyper-sparsity in the revised simplex method."
- Koberstein, A. (2005). "The dual simplex method, techniques for a fast and stable implementation."

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
