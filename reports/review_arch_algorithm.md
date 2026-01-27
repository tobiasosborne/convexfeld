# Architecture Review: Algorithm Flow

**Reviewer:** Claude Opus 4.5
**Date:** 2026-01-27
**Scope:** Algorithm correctness for revised simplex method implementation

## Executive Summary

**Will this algorithm produce correct results?** PARTIALLY - with significant gaps.

The specifications describe a mathematically correct revised simplex algorithm based on industry-standard techniques. However, the current implementation is at an **early stage** - many critical components exist only as stubs or are entirely missing. The implemented pieces (FTRAN, BTRAN, pricing) appear algorithmically correct, but the system cannot solve real LPs yet.

**Confidence Level:** LOW for production use, HIGH for foundation quality.

---

## Algorithm Trace

### Specified Flow (from docs/specs/architecture/data_flow.md)

```
cxf_optimize()
    |
    +-> Validation Phase (check model)
    +-> Preparation Phase (update model, build matrix)
    +-> Algorithm Selection (default: dual simplex)
    |
    +-> cxf_solve_lp()
        |
        +-> cxf_simplex_init()      [NOT IMPLEMENTED - stub only]
        +-> cxf_simplex_crash()     [NOT IMPLEMENTED]
        +-> cxf_simplex_preprocess() [NOT IMPLEMENTED]
        +-> cxf_simplex_setup()     [NOT IMPLEMENTED]
        +-> cxf_basis_refactor()    [PARTIAL - identity basis only]
        |
        +-> MAIN ITERATION LOOP:    [NOT IMPLEMENTED]
            |
            +-> cxf_pricing_candidates() or cxf_pricing_steepest()  [IMPLEMENTED]
            +-> cxf_btran()          [IMPLEMENTED - for dual prices]
            +-> cxf_ftran()          [IMPLEMENTED - for pivot column]
            +-> cxf_ratio_test()     [NOT IMPLEMENTED]
            +-> cxf_pivot_primal()   [NOT IMPLEMENTED]
            +-> Update objective, check termination
            |
        +-> cxf_simplex_refine()    [NOT IMPLEMENTED]
        +-> cxf_simplex_cleanup()   [NOT IMPLEMENTED]
        +-> cxf_simplex_final()     [NOT IMPLEMENTED]
```

### Current Implementation Reality

The actual `cxf_solve_lp()` in `src/simplex/solve_lp_stub.c` is a **trivial stub** that:
1. Sets each variable to its optimal bound based on objective coefficient sign
2. Returns OPTIMAL for any unconstrained LP
3. Completely ignores constraints

This stub is useful for tracer bullet testing but cannot solve any constrained LP.

---

## Correctness Analysis

### Pricing

**Status:** IMPLEMENTED and algorithmically correct.

**Files:**
- `/home/tobias/Projects/convexfeld/src/pricing/candidates.c`
- `/home/tobias/Projects/convexfeld/src/pricing/steepest.c`

**Analysis:**

The pricing implementation correctly identifies attractive variables:
- Variables at lower bound with negative reduced cost (rc < -tolerance)
- Variables at upper bound with positive reduced cost (rc > tolerance)
- Free variables with any nonzero reduced cost

The steepest edge implementation uses the correct formula:
```
SE_ratio = |reduced_cost| / sqrt(weight)
```

Partial pricing (section cycling) is correctly implemented to reduce O(n) scans.

**Issues Found:**
1. Global static variable `g_reduced_costs` in candidates.c is NOT thread-safe (documented as known limitation)
2. Weight updates in `cxf_pricing_update()` are incomplete - full SE weight update requires alpha_j which needs matrix access

**Verdict:** Correct for single-threaded use.

---

### FTRAN (Forward Transformation)

**Status:** IMPLEMENTED and algorithmically correct.

**File:** `/home/tobias/Projects/convexfeld/src/basis/ftran.c`

**Analysis:**

FTRAN solves Bx = b by computing x = B^(-1) * b using the Product Form of Inverse (PFI).

The implementation correctly:
1. Copies input to result (handles identity basis case)
2. Applies eta vectors in chronological order (oldest to newest)
3. For each eta: `result[r] = temp / pivot_elem`, and `result[j] -= eta_value[j] * temp`

**Mathematical correctness verified:**
- E^(-1) * x at pivot row: x[r] = x[r] / pivot_elem
- E^(-1) * x at other rows: x[j] -= eta[j] * temp

**Verdict:** Correct.

---

### BTRAN (Backward Transformation)

**Status:** IMPLEMENTED and algorithmically correct.

**File:** `/home/tobias/Projects/convexfeld/src/basis/btran.c`

**Analysis:**

BTRAN solves y^T B = e_row^T, needed for computing simplex tableau rows (dual prices).

The implementation correctly:
1. Initializes result as unit vector e_row
2. Collects eta pointers for reverse traversal
3. Applies eta vectors in REVERSE order (newest to oldest)
4. For each eta: computes dot product then updates pivot position

**Memory management:** Uses stack allocation for small eta counts (< 64), heap for larger.

**Verdict:** Correct.

---

### Basis Update (Eta Vector Creation)

**Status:** IMPLEMENTED for EtaFactors structure, BUT cxf_pivot_primal NOT IMPLEMENTED.

**Files:**
- `/home/tobias/Projects/convexfeld/src/basis/eta_factors.c` - structure management
- `cxf_pivot_primal` - NOT FOUND

**Analysis:**

The EtaFactors structure and lifecycle are correctly implemented:
- `cxf_eta_create()` - properly allocates sparse storage
- `cxf_eta_validate()` - checks invariants including finite values
- Linked list management for PFI

However, the **actual pivot execution** (`cxf_pivot_primal`) which creates the eta vector during a simplex pivot is **NOT IMPLEMENTED**.

**Critical Missing Logic:**
```c
// This function should:
// 1. Swap entering variable into basis
// 2. Create eta vector: eta[p] = 1/pivot_element, eta[k] = -pivot_col[k]/pivot_element
// 3. Update basisHeader and variableStatus
// 4. Update objective value
```

**Verdict:** Foundation correct, critical function missing.

---

### Ratio Test

**Status:** NOT IMPLEMENTED

**Expected Location:** `src/simplex/` or `src/ratio_test/`

**Analysis:**

The ratio test is **not implemented anywhere** in the codebase. This is a critical gap.

The specification describes Harris two-pass ratio test which should:
1. First pass: Find candidates with ratio within tolerance
2. Second pass: Among candidates, select largest pivot element

**Mathematical correctness (from spec):**
- For variable at lower bound with positive pivot: ratio = (value - lb) / pivot
- For variable at upper bound with negative pivot: ratio = (ub - value) / (-pivot)
- Select minimum positive ratio

**Impact:** Without ratio test, cannot select leaving variable. Simplex cannot iterate.

---

### Refactorization

**Status:** PARTIAL - handles identity basis only.

**File:** `/home/tobias/Projects/convexfeld/src/basis/refactor.c`

**Analysis:**

`cxf_basis_refactor()` and `cxf_solver_refactor()` correctly:
1. Clear existing eta list
2. Reset counters
3. Check if basis is all slacks (identity basis)

**MAJOR GAP:** For non-identity basis (structural columns in basis), the implementation is incomplete:
```c
/* TODO: Implement full Markowitz-ordered LU factorization
 * when matrix access is available. */
```

Without full refactorization:
- Cannot handle structural variables in basis
- Numerical accuracy will degrade without LU refresh
- System may fail after first non-trivial pivot

**Verdict:** Insufficient for production.

---

## Numerical Stability Assessment

**Status:** MIXED - good practices specified, implementation incomplete.

### Specified Safeguards (from specs)

1. **Pivot tolerance checking:** Specified in ratio test module
2. **Refactorization triggers:**
   - Eta count > threshold (typically 100-200)
   - Memory usage > limit
   - FTRAN performance degradation
3. **Perturbation for degeneracy:** Specified in simplex module
4. **Iterative refinement:** Specified post-solve

### Implemented Safeguards

1. **Division by zero checks:** Present in FTRAN, BTRAN
2. **Bounds checks:** Present in eta operations
3. **Finite value validation:** `cxf_eta_validate()` uses `isfinite()`
4. **Refactor check function:** `cxf_refactor_check()` implemented

### Missing Safeguards

1. **Harris pivot selection:** Not implemented (needs ratio test)
2. **Perturbation:** Not implemented
3. **Full refactorization:** Not implemented
4. **Iterative refinement:** Not implemented

**Verdict:** Will have numerical issues on non-trivial problems.

---

## Missing Pieces

### Critical (Cannot solve LPs without these)

| Component | Status | Impact |
|-----------|--------|--------|
| `cxf_simplex_init()` | Stub only | No solver state initialization |
| `cxf_simplex_crash()` | Not implemented | No initial basis selection |
| `cxf_ratio_test()` | Not implemented | Cannot select leaving variable |
| `cxf_pivot_primal()` | Not implemented | Cannot execute pivots |
| Main iteration loop | Not implemented | No optimization happens |
| Full LU refactorization | Not implemented | Cannot handle structural basis |

### Important (Needed for robust production solver)

| Component | Status | Impact |
|-----------|--------|--------|
| `cxf_simplex_preprocess()` | Not implemented | No scaling/tightening |
| `cxf_simplex_refine()` | Not implemented | Reduced accuracy |
| Phase I method | Not implemented | Cannot handle infeasible starts |
| Perturbation | Not implemented | May cycle on degenerate problems |
| Unboundedness detection | Not implemented | Will hang on unbounded problems |

### Nice to Have (For performance)

| Component | Status | Impact |
|-----------|--------|--------|
| Partial pricing persistence | Stub | Reduced efficiency |
| Steepest edge weight updates | Incomplete | More iterations |
| Sparse FTRAN/BTRAN | Basic | Slower on sparse problems |

---

## Critical Bugs

### Bug 1: Stub solve_lp ignores constraints

**File:** `/home/tobias/Projects/convexfeld/src/simplex/solve_lp_stub.c`

The current implementation:
```c
for (i = 0; i < model->num_vars; i++) {
    if (coeff >= 0.0) val = lb;
    else val = ub;
    model->solution[i] = val;
}
model->status = CXF_OPTIMAL;
```

This will return incorrect "optimal" solutions for any constrained LP.

### Bug 2: No constraint matrix integration

The CxfModel structure has a `SparseMatrix *matrix` field but:
- It's never populated by the API
- Solver never reads from it
- FTRAN/BTRAN don't access constraint columns

### Bug 3: Refactorization cannot handle real basis

`cxf_solver_refactor()` contains:
```c
if (all_slacks) {
    return REFACTOR_OK;  // Identity basis - OK
}
// For non-identity basis: TODO comment, returns OK without doing anything
```

This will cause numerical drift after first pivot.

---

## Edge Cases Analysis

### Unbounded Detection

**Status:** NOT IMPLEMENTED

The specification correctly describes detection:
> If ratio test finds no leaving variable (all pivot column entries non-positive), problem is unbounded.

No code implements this check.

### Infeasibility Detection

**Status:** NOT IMPLEMENTED

The specification correctly describes Phase I:
> Phase I minimizes sum of artificial variables. If optimal > tolerance, problem is infeasible.

Phase I is not implemented.

### Degeneracy Handling

**Status:** NOT IMPLEMENTED

The specification describes perturbation approach. Not implemented.

### Free Variables

**Status:** PARTIAL

Pricing correctly handles free variables (status -3). But pivot execution is missing.

### Fixed Variables

**Status:** NOT IMPLEMENTED

The specification describes `cxf_fix_variable()`. Not found in implementation.

---

## Verdict

### Current State

The project has:
- **Excellent specifications** based on industry-standard algorithms
- **Solid foundation** in FTRAN, BTRAN, pricing, eta management
- **Critical gaps** in simplex core, ratio test, pivot, refactorization

### Will It Work?

| Scenario | Result |
|----------|--------|
| Empty LP (0 variables) | WORKS - trivial case handled |
| Unconstrained LP | WRONG - ignores constraints but returns OPTIMAL |
| Simple constrained LP | WILL NOT WORK - no iteration loop |
| Degenerate LP | WILL NOT WORK - no perturbation |
| Infeasible LP | WILL NOT WORK - no Phase I |
| Unbounded LP | WILL NOT WORK - no detection |

### Recommendations

1. **Priority 1:** Implement main simplex iteration loop
2. **Priority 2:** Implement ratio test (Harris two-pass)
3. **Priority 3:** Implement `cxf_pivot_primal()` with eta creation
4. **Priority 4:** Implement full LU refactorization
5. **Priority 5:** Implement Phase I for infeasibility handling

### Confidence Assessment

| Aspect | Confidence |
|--------|------------|
| Spec correctness | HIGH - based on standard algorithms |
| Foundation code quality | HIGH - clean, documented |
| Current ability to solve LPs | NONE - cannot solve constrained problems |
| Path to completion | HIGH - clear roadmap in M7 milestone |

---

*Report generated by Claude Opus 4.5 for Convexfeld LP Solver project.*
