# Code Quality Review Report

**Date:** 2026-01-27
**Issue:** convexfeld-9eg

## Summary

Full codebase review identifying duplicates, inconsistencies, file size violations, test gaps, and dead code.

## 1. File Size Violations (200 LOC Rule)

### Source Files Over Limit (8 files)

| File | Lines | Overage | Priority |
|------|-------|---------|----------|
| simplex/context.c | 307 | 53% | HIGH |
| simplex/solve_lp.c | 283 | 41% | HIGH |
| basis/warm.c | 258 | 29% | MEDIUM |
| api/model.c | 243 | 21% | MEDIUM |
| simplex/iterate.c | 233 | 16% | MEDIUM |
| api/model_stub.c | 213 | 6% | LOW |
| basis/eta_factors.c | 212 | 6% | LOW |
| basis/refactor.c | 208 | 4% | LOW |

### Test Files Over Limit (13 files)

| File | Lines | Notes |
|------|-------|-------|
| test_basis.c | 947 | CRITICAL - split into 5 files |
| test_timing.c | 488 | Split by operation type |
| test_api_model.c | 471 | Split by model operation |
| test_matrix.c | 446 | Split by operation type |
| test_callbacks.c | 439 | Split by callback phase |
| test_pricing.c | 434 | Split by pricing strategy |
| test_error.c | 424 | Split by error category |

## 2. Duplicate Code

### Exact Duplicates

**`clear_eta_list`** - Identical function in two files:
- `src/basis/warm.c` (lines 43-55)
- `src/basis/refactor.c` (lines 34-49)

**Action:** Extract to shared `basis_utils.c` (issue convexfeld-zzm)

### Near-Duplicate Patterns

1. **NULL validation checks** - 40+ occurrences of identical patterns
2. **Memory allocation with cleanup** - Repeated in 6+ files
3. **Magic number validation** - Similar patterns in 4+ files
4. **Array cleanup boilerplate** - Similar patterns in 5+ files

## 3. Error Handling Inconsistencies

### Allocation Function Mix

**Problem:** Mix of `malloc/calloc` (stdlib) and `cxf_malloc/cxf_calloc` (custom)

Files using stdlib directly:
- basis/eta_factors.c
- basis/basis_state.c
- basis/pivot_eta.c
- basis/btran.c
- simplex/iterate.c
- matrix/sparse_matrix.c

**Action:** Standardize on `cxf_*` functions (issue convexfeld-ubl)

### Inconsistent Error Codes

**`pivot_eta.c:58`** returns `-1` instead of defined error code:
```c
if (fabs(pivot) < CXF_PIVOT_TOL) {
    return -1;  /* Should use CXF_ERROR_PIVOT_TOO_SMALL */
}
```

### Missing Error Checks

- `model_stub.c:38-70`: Partial realloc sequence can leak on failure
- `iterate.c:98`: Returns `CXF_ERROR_OUT_OF_MEMORY` for pre-existing NULL (misleading)

## 4. Test Coverage Gaps

### Functions with Zero Tests

| Function | Module | Priority |
|----------|--------|----------|
| cxf_pivot_with_eta() | basis | P1 - CRITICAL |
| cxf_eta_validate() | basis | P1 |
| cxf_sparse_validate() | matrix | P2 |
| cxf_sparse_build_csr() | matrix | P2 |
| cxf_matrix_transpose_multiply() | matrix | P2 |
| cxf_refactor_check() | basis | P2 |

### Edge Cases Not Covered

- NULL inputs on all pricing functions
- NaN/Infinity propagation
- Empty matrices (nnz=0)
- Allocation failure paths
- Degenerate pivots in ratio_test

## 5. Dead Code

### Empty Stub Files (can delete)

- `src/error/error_stub.c` - Only comments
- `src/pricing/pricing_stub.c` - Only includes

### Stub Functions (25+)

**Not implemented (return NOT_SUPPORTED):**
- cxf_addqconstr
- cxf_addgenconstrIndicator

**No-ops (return OK without work):**
- cxf_env_acquire_lock
- cxf_leave_critical_section
- cxf_simplex_perturbation
- cxf_simplex_unperturb
- cxf_pre_optimize_callback
- cxf_post_optimize_callback

**Minimal implementation:**
- cxf_getconstrs - Always returns 0 nonzeros
- cxf_getcoeff - Always returns 0.0
- cxf_delvars - Validates but doesn't delete

## 6. TODO Comments

- `basis/refactor.c:154`: "TODO: Implement full Markowitz-ordered LU factorization"

## Issues Created

| Issue ID | Title | Priority |
|----------|-------|----------|
| convexfeld-1wq | Refactor context.c to under 200 LOC | P2 |
| convexfeld-zzm | Extract duplicate clear_eta_list | P2 |
| convexfeld-ubl | Standardize allocation functions | P2 |
| convexfeld-72e | Add tests for cxf_pivot_with_eta | P1 |
| convexfeld-sb3a | Delete empty stub files | P3 |
| convexfeld-4vl9 | Refactor test_basis.c | P2 |

## Recommendations

1. **Immediate:** Add tests for `cxf_pivot_with_eta` (blocks M7 progress)
2. **Short-term:** Extract duplicate `clear_eta_list`, refactor context.c
3. **Medium-term:** Standardize allocation, delete empty stubs
4. **Long-term:** Refactor large test files, add validation macros
