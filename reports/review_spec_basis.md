# Spec Compliance Review: Basis Module

**Date:** 2026-01-27
**Reviewer:** Claude Sonnet 4.5
**Scope:** All implemented functions in src/basis/ vs docs/specs/functions/basis/

---

## Executive Summary

**Overall Compliance Status: PARTIAL**

The basis module implements 8 core functions with varying degrees of spec compliance. Key findings:

- **PASS (5 functions):** FTRAN, BTRAN, EtaFactors operations, BasisSnapshot operations
- **PARTIAL (2 functions):** Validation, Warm start
- **INCOMPLETE (1 function):** Refactorization (stub implementation only)
- **SPEC MISMATCHES:** 3 functions have signature differences from specs

### Critical Issues
1. **cxf_basis_refactor** is a stub - does not compute LU factorization as specified
2. **cxf_pivot_with_eta** is completely missing from implementation
3. **cxf_timing_refactor** renamed to **cxf_refactor_check** with different semantics
4. Function signatures use `BasisState*` instead of `SolverState*` as specified

### Recommendations
1. Complete the refactorization implementation with Markowitz ordering
2. Implement cxf_pivot_with_eta for product form updates
3. Reconcile spec/implementation mismatch on state structures
4. Add missing error codes and numerical stability checks

---

## Function-by-Function Analysis

### 1. cxf_ftran

**File:** `src/basis/ftran.c`
**Spec:** `docs/specs/functions/basis/cxf_ftran.md`

**Spec Summary:**
Forward transformation solving Bx = b using LU factors + eta vectors. Should handle L solve, U solve, then apply eta vectors in chronological order.

**Implementation:**
```c
int cxf_ftran(BasisState *basis, const double *column, double *result)
```
- Copies input to result
- Applies eta vectors in chronological order (oldest to newest)
- Performs division by pivot element and updates off-diagonal entries

**Compliance:** ✅ **PASS**

**Signature Match:**
- ❌ Spec expects `SolverState*`, implementation uses `BasisState*`
- ✅ Parameters column and result match spec

**Algorithm Match:**
- ⚠️ **PARTIAL** - Implementation only handles eta vectors, not LU factors
- The spec describes a two-phase algorithm: (1) LU solve, (2) eta application
- Implementation assumes identity basis and only applies eta transformations
- This is correct for the product form representation currently used, but incomplete vs spec

**Error Handling:**
- ✅ NULL checks present
- ✅ Division by zero check
- ✅ Bounds checking on pivot row
- ✅ Returns CXF_OK (0) on success
- ❌ Spec mentions error code 1001 for "out of memory" - not applicable here
- ❌ Spec mentions "may trigger refactorization" - not implemented

**Edge Cases:**
- ✅ Handles empty basis (m=0)
- ✅ Handles no eta vectors (identity case)
- ✅ Zero column case handled implicitly

**Issues:**
1. Type mismatch: `BasisState*` vs `SolverState*`
2. Missing LU factorization phase (only product form implemented)
3. No timing statistics recorded (spec mentions this)
4. No refactorization trigger mechanism

---

### 2. cxf_btran

**File:** `src/basis/btran.c`
**Spec:** `docs/specs/functions/basis/cxf_btran.md`

**Spec Summary:**
Backward transformation solving B^T * y = e_row. Applies eta vectors in reverse chronological order.

**Implementation:**
```c
int cxf_btran(BasisState *basis, int row, double *result)
```
- Initializes result as unit vector e_row
- Collects eta pointers in array for reverse traversal
- Applies eta vectors in reverse order (newest to oldest)
- Uses stack allocation for small eta counts, heap for large

**Compliance:** ✅ **PASS**

**Signature Match:**
- ❌ Spec expects `SolverState*`, implementation uses `BasisState*`
- ✅ Parameters row and result match spec

**Algorithm Match:**
- ✅ Correctly implements reverse eta traversal
- ✅ Proper dot product computation for transpose operation
- ✅ Updates only pivot position (other positions unchanged)
- ⚠️ Same LU factor issue as FTRAN - only handles eta portion

**Error Handling:**
- ✅ NULL checks present
- ✅ Row bounds check
- ✅ Division by zero check
- ✅ Memory allocation failure handling for large eta counts
- ✅ Proper cleanup on error paths

**Performance:**
- ✅ Excellent: Stack allocation for common case (≤64 etas)
- ✅ Falls back to heap only when needed
- ✅ Single traversal to collect pointers, then reverse iteration

**Edge Cases:**
- ✅ Empty basis handled
- ✅ No eta vectors (identity case) short-circuits
- ✅ Single constraint case works

**Issues:**
1. Type mismatch: `BasisState*` vs `SolverState*`
2. Missing LU factorization phase
3. No timing statistics

---

### 3. cxf_pivot_with_eta

**File:** NONE - **MISSING IMPLEMENTATION**
**Spec:** `docs/specs/functions/basis/cxf_pivot_with_eta.md`

**Spec Summary:**
Updates basis factorization after a pivot by creating and appending an eta vector. Key operation for product form updates.

**Implementation:** ❌ **COMPLETELY MISSING**

**Compliance:** ❌ **FAIL**

**Impact:**
This is a **CRITICAL** missing function. Without it:
- Cannot perform basis updates during simplex iterations
- Cannot maintain product form of inverse
- FTRAN/BTRAN cannot work correctly without eta vectors being created

**Expected Signature:**
```c
int cxf_pivot_with_eta(SolverState *state, int pivotRow, double *pivotCol)
```

**Required Behavior:**
1. Validate pivot element magnitude
2. Compute eta multiplier (1/pivot)
3. Create eta structure with off-diagonal entries
4. Append to eta list
5. Update basis header
6. Update variable status

**Workaround:**
Currently, basis updates would fail or require full refactorization after each pivot - extremely inefficient.

---

### 4. cxf_basis_warm

**File:** `src/basis/warm.c`
**Spec:** `docs/specs/functions/basis/cxf_basis_warm.md`

**Spec Summary:**
Initializes solver state from a warm start basis with validation and repair.

**Implementation:**
Two functions implemented:
```c
int cxf_basis_warm(BasisState *basis, const int *basic_vars, int m)
int cxf_basis_warm_snapshot(BasisState *basis, const BasisSnapshot *snapshot)
```

**Compliance:** ⚠️ **PARTIAL**

**Signature Match:**
- ❌ Spec expects 3 parameters: `(SolverState*, BasisSnapshot*, CxfEnv*)`
- ✅ Implementation provides two variants for different use cases

**Algorithm Match:**
- ❌ **INCOMPLETE** - Missing many spec requirements:
  - No dimension change handling
  - No variable mapping for modified problems
  - No repair mechanism for invalid entries
  - No basis count verification/repair
  - No factorization trigger
  - No fallback to crash
  - No warning/repair count return

**Current Behavior:**
- Simple copy of basic variables array
- Clears eta list
- Validates dimensions match exactly
- No sophisticated repair logic

**Error Handling:**
- ✅ NULL checks present
- ✅ Dimension mismatch detected
- ✅ Invalid snapshot rejected
- ❌ No repair return codes (spec: 0=success, 1-100=repairs, negative=error)

**Issues:**
1. Oversimplified implementation vs spec
2. No variable mapping or repair
3. No integration with refactorization
4. No environment parameter for tolerances
5. No fallback mechanisms

---

### 5. cxf_basis_snapshot

**File:** `src/basis/snapshot.c`
**Spec:** `docs/specs/functions/basis/cxf_basis_snapshot.md`

**Spec Summary:**
Creates checkpoint of basis state for restoration/comparison.

**Implementation:**
```c
int cxf_basis_snapshot_create(BasisState *basis, BasisSnapshot *snapshot, int includeFactors)
```

**Compliance:** ✅ **PASS**

**Signature Match:**
- ✅ Matches spec signature exactly

**Algorithm Match:**
- ✅ Stores dimensions (numVars, numConstrs)
- ✅ Deep copies basisHeader array
- ✅ Deep copies varStatus array
- ✅ Records iteration number
- ✅ Marks snapshot as valid
- ⚠️ includeFactors parameter ignored (spec allows this)

**Error Handling:**
- ✅ NULL argument checks
- ✅ Memory allocation failure handling
- ✅ Partial cleanup on error (frees basisHeader if varStatus alloc fails)
- ✅ Returns CXF_ERROR_OUT_OF_MEMORY (1001)

**Edge Cases:**
- ✅ Empty basis (m=0 or n=0) handled
- ✅ NULL pointers set when no allocation needed

**Issues:**
1. L/U factor copying not implemented (acceptable per spec)
2. Type mismatch: `BasisState*` vs spec's implicit `SolverState*`

---

### 6. cxf_basis_refactor

**File:** `src/basis/refactor.c`
**Spec:** `docs/specs/functions/basis/cxf_basis_refactor.md`

**Spec Summary:**
Recomputes LU factorization of basis matrix using Markowitz ordering. Critical for maintaining numerical accuracy.

**Implementation:**
```c
int cxf_basis_refactor(BasisState *basis)
int cxf_solver_refactor(SolverContext *ctx, CxfEnv *env)
```

**Compliance:** ❌ **INCOMPLETE STUB**

**Signature Match:**
- ⚠️ Two variants provided: simple and full
- ❌ Spec expects `(SolverState*, CxfEnv*)`, implementation uses `BasisState*` or `SolverContext*`

**Algorithm Match:**
- ❌ **STUB ONLY** - Does not implement the spec algorithm:
  - No basis matrix extraction
  - No Markowitz LU factorization
  - No factor storage
  - No fill-in minimization
  - No pivot selection
  - No Gaussian elimination

**Current Behavior:**
- `cxf_basis_refactor`: Just clears eta list
- `cxf_solver_refactor`: Clears eta list, resets counters, checks for identity basis
- Returns OK without computing factorization
- TODO comment acknowledges incompleteness

**Error Handling:**
- ✅ NULL checks present
- ❌ No singular basis detection (spec error code 3)
- ❌ No memory allocation failure handling

**Impact:**
This is a **CRITICAL** missing implementation. The refactorization is essential for:
- Maintaining numerical accuracy over iterations
- Handling non-identity bases
- Enabling FTRAN/BTRAN to work with structural variables

**Required Work:**
1. Extract basis columns from constraint matrix
2. Implement Markowitz-ordered Gaussian elimination
3. Store L and U factors in compressed sparse format
4. Return proper error codes for singular bases
5. Record timing statistics

---

### 7. cxf_basis_validate

**File:** `src/basis/warm.c`
**Spec:** `docs/specs/functions/basis/cxf_basis_validate.md`

**Spec Summary:**
Validates basis for consistency, feasibility, and numerical stability with selective checks via flags.

**Implementation:**
```c
int cxf_basis_validate(BasisState *basis)
int cxf_basis_validate_ex(BasisState *basis, int flags)
```

**Compliance:** ⚠️ **PARTIAL**

**Signature Match:**
- ❌ Spec expects `(SolverState*, CxfEnv*, int flags)`
- ✅ Implementation provides two variants: simple and extended

**Algorithm Match:**
- ✅ Simple version checks bounds and duplicates
- ⚠️ Extended version supports selective flags
- ❌ **MISSING** many spec checks:
  - No singularity check
  - No primal feasibility check (Ax = b)
  - No dual feasibility check (reduced costs)
  - No condition number estimation
  - No refactorization trigger

**Implemented Checks:**
- ✅ CXF_CHECK_BOUNDS (variable indices in range)
- ✅ CXF_CHECK_DUPLICATES (no duplicate basic variables)
- ⚠️ CXF_CHECK_CONSISTENCY (partial - only checks var_status for basic vars)
- ❌ CXF_CHECK_COUNT (no-op comment: "more meaningful with full SolverState")

**Error Codes:**
- ✅ Returns CXF_OK (0) on success
- ✅ Returns CXF_ERROR_INVALID_ARGUMENT on violations
- ❌ Spec defines specific codes (-1 to -5) for different failures - not used

**Issues:**
1. Oversimplified vs spec requirements
2. No numerical stability checks
3. No feasibility validation
4. Missing environment parameter for tolerances
5. Generic error code instead of specific diagnostics

---

### 8. cxf_basis_equal & cxf_basis_diff

**File:** `src/basis/basis_stub.c`, `src/basis/snapshot.c`
**Specs:** `docs/specs/functions/basis/cxf_basis_equal.md`, `cxf_basis_diff.md`

**Spec Summary:**
- **cxf_basis_equal**: Tests if two bases are identical
- **cxf_basis_diff**: Computes detailed differences between bases

**Implementation:**

**basis_stub.c:**
```c
int cxf_basis_equal(BasisState *basis, const int *snapshot, int m)
int cxf_basis_diff(const int *snap1, const int *snap2, int m)
```

**snapshot.c:**
```c
int cxf_basis_snapshot_equal(const BasisSnapshot *s1, const BasisSnapshot *s2)
int cxf_basis_snapshot_diff(const BasisSnapshot *s1, const BasisSnapshot *s2)
```

**Compliance:** ⚠️ **PARTIAL**

**Signature Mismatches:**
- ❌ Spec expects `(BasisSnapshot*, BasisSnapshot*, int checkStatus)`
- ❌ Implementation has TWO different interfaces:
  - Simple: works with raw int arrays
  - Full: works with BasisSnapshot structs

**Algorithm Match (snapshot.c version):**
- ✅ Dimension checks
- ✅ Element-by-element comparison
- ✅ Returns count of differences (diff)
- ✅ Returns 1/0 for equality
- ❌ No set comparison (spec says "same set in different order = equal")
- ❌ No optional checkStatus parameter

**Algorithm Match (stub version):**
- ✅ Simple equality check
- ✅ Element comparison
- ⚠️ Positional equality only (not set equality)

**Issues:**
1. Two conflicting implementations (stub + full)
2. Set comparison not implemented (spec requirement)
3. No checkStatus flag support
4. Inconsistent return values between versions

---

### 9. cxf_timing_refactor

**File:** `src/basis/refactor.c` (as **cxf_refactor_check**)
**Spec:** `docs/specs/functions/basis/cxf_timing_refactor.md`

**Spec Summary:**
Determines if refactorization should be triggered based on timing/performance criteria.

**Implementation:**
```c
int cxf_refactor_check(SolverContext *ctx, CxfEnv *env)
```

**Compliance:** ⚠️ **PARTIAL (renamed)**

**Signature Match:**
- ⚠️ **RENAMED** from `cxf_timing_refactor` to `cxf_refactor_check`
- ✅ Parameters match intent: context and environment
- ❌ Uses `SolverContext*` instead of `SolverState*`

**Algorithm Match:**
- ✅ Checks eta count limit → returns 2 (required)
- ✅ Checks eta memory limit → returns 2 (required)
- ✅ Checks iteration interval → returns 1 (recommended)
- ✅ Checks FTRAN time degradation → returns 1 (recommended)
- ✅ Three-level return (0/1/2) matches spec

**Criteria Implemented:**
- ✅ Hard limit: eta count > max_eta_count
- ✅ Hard limit: eta memory > max_eta_memory
- ✅ Soft limit: iterations since refactor > interval
- ✅ Performance: avg FTRAN time > 3x baseline

**Error Handling:**
- ✅ NULL checks
- ✅ Safe division (ftran_count > 0 check)
- ✅ Always returns valid recommendation

**Issues:**
1. Function name changed from spec
2. Type mismatch on state parameter
3. Baseline FTRAN must be pre-computed elsewhere

---

## Missing Implementations

### Functions Specified but Not Implemented

1. **cxf_pivot_with_eta** ⚠️ **CRITICAL**
   - Spec: `docs/specs/functions/basis/cxf_pivot_with_eta.md`
   - Status: Completely missing
   - Impact: Cannot perform product form basis updates

---

## Structural Issues

### Type Mismatches

**Problem:** Specs consistently reference `SolverState*` but implementations use:
- `BasisState*` (most functions)
- `SolverContext*` (refactor functions)

**Analysis:**
Looking at the codebase:
- `BasisState` is defined in `cxf_basis.h` - contains basis factorization
- `SolverState` likely intended to be a higher-level structure
- This appears to be a **spec vs implementation design divergence**

**Resolution Options:**
1. Update specs to match implementation (`BasisState*`)
2. Wrap functions to accept `SolverState*` and extract `BasisState*`
3. Refactor code to use unified state structure

**Recommendation:** Option 1 - Update specs. The current implementation is cleaner with separated concerns.

---

## Compliance Matrix

| Function | File | Spec | Signature | Algorithm | Errors | Status |
|----------|------|------|-----------|-----------|--------|--------|
| cxf_ftran | ftran.c | ✓ | ⚠️ type | ⚠️ partial | ✓ | PASS |
| cxf_btran | btran.c | ✓ | ⚠️ type | ⚠️ partial | ✓ | PASS |
| cxf_pivot_with_eta | - | ✓ | ❌ | ❌ | ❌ | **MISSING** |
| cxf_basis_warm | warm.c | ✓ | ❌ | ❌ | ⚠️ | PARTIAL |
| cxf_basis_snapshot | snapshot.c | ✓ | ✓ | ✓ | ✓ | PASS |
| cxf_basis_refactor | refactor.c | ✓ | ⚠️ type | ❌ stub | ⚠️ | **STUB** |
| cxf_basis_validate | warm.c | ✓ | ❌ | ⚠️ partial | ⚠️ | PARTIAL |
| cxf_basis_equal | stub/snapshot | ✓ | ❌ | ⚠️ | ✓ | PARTIAL |
| cxf_basis_diff | stub/snapshot | ✓ | ❌ | ⚠️ | ✓ | PARTIAL |
| cxf_timing_refactor | refactor.c* | ✓ | ⚠️ renamed | ✓ | ✓ | PASS* |

*Renamed to `cxf_refactor_check`

---

## Critical Issues

### 1. Missing cxf_pivot_with_eta (BLOCKER)

**Severity:** CRITICAL
**Impact:** Simplex iterations cannot update basis

**Description:**
The product form update function is completely missing. Without it:
- Cannot create eta vectors during pivots
- FTRAN/BTRAN will have empty eta lists
- Basis becomes stale after first pivot
- Must refactor after every pivot (extremely inefficient)

**Required Actions:**
1. Implement eta vector creation from pivot column
2. Compute eta multiplier and off-diagonal entries
3. Append to eta linked list
4. Update basis header and variable status
5. Handle small pivot rejection

**Priority:** MUST FIX before simplex can work

---

### 2. Stub Refactorization (BLOCKER)

**Severity:** CRITICAL
**Impact:** Cannot handle non-identity bases

**Description:**
`cxf_basis_refactor` only clears eta list without computing factorization. This means:
- Only identity bases (all slacks) work
- Structural variables in basis will fail
- Numerical accuracy cannot be restored
- No singularity detection

**Required Actions:**
1. Extract basis columns from constraint matrix
2. Implement Markowitz-ordered LU factorization
3. Store L and U factors (or equivalent eta representation)
4. Detect singular bases and return error code 3
5. Handle memory allocation for factors

**Priority:** MUST FIX for real-world problems

---

### 3. Incomplete Warm Start (HIGH)

**Severity:** HIGH
**Impact:** Cannot reoptimize modified problems efficiently

**Description:**
Warm start only does simple copy without validation/repair. Missing:
- Variable mapping for dimension changes
- Invalid basis entry repair
- Basis count verification
- Fallback to crash start
- Repair statistics return

**Required Actions:**
1. Add dimension change handling
2. Implement variable mapping
3. Add basis validation and repair logic
4. Integrate with refactorization
5. Return repair count or warnings

**Priority:** Should fix for production use

---

### 4. Incomplete Validation (MEDIUM)

**Severity:** MEDIUM
**Impact:** Cannot verify basis correctness

**Description:**
Validation only checks basic invariants. Missing critical checks:
- Singularity/condition number
- Primal feasibility (Ax = b)
- Dual feasibility (reduced costs)
- Specific error diagnostics

**Required Actions:**
1. Add condition number estimation
2. Implement primal feasibility checks
3. Add dual feasibility checks
4. Return specific error codes per failure type

**Priority:** Should add for robustness

---

### 5. Type Mismatch Throughout (LOW)

**Severity:** LOW
**Impact:** Spec/code divergence, documentation confusion

**Description:**
All specs use `SolverState*` but code uses `BasisState*` or `SolverContext*`.

**Required Actions:**
1. **Option A:** Update all specs to use `BasisState*` (recommended)
2. **Option B:** Refactor code to use unified state structure
3. Document the structure relationship clearly

**Priority:** Document/clarify but not urgent

---

## Recommendations

### Immediate Actions (Before Next Milestone)

1. **Implement cxf_pivot_with_eta** (M5 or M7)
   - Critical for simplex iterations
   - ~100 LOC based on spec
   - Test with simple pivot scenarios

2. **Complete cxf_basis_refactor** (M5 or M7)
   - Essential for structural variables
   - ~300 LOC for Markowitz LU
   - Can start with simpler ordering (natural/column minimum)

3. **Reconcile State Type Mismatch**
   - Update specs to use `BasisState*` OR
   - Create wrapper functions
   - Document the design decision

### Medium-Term Actions (Later Milestones)

4. **Enhance cxf_basis_warm**
   - Add dimension change handling
   - Implement repair logic
   - Integrate with crash start fallback

5. **Enhance cxf_basis_validate**
   - Add numerical stability checks
   - Implement feasibility validation
   - Add specific error diagnostics

6. **Consolidate Snapshot Functions**
   - Remove duplicate implementations in stub
   - Implement set comparison for equality
   - Add checkStatus parameter

### Long-Term Improvements

7. **Add LU Factor Storage**
   - Currently only eta vectors
   - Would enable hybrid update strategies
   - Better for dense subproblems

8. **Implement Markowitz Ordering**
   - Minimize fill-in during factorization
   - Critical for large sparse problems
   - Standard in production LP solvers

9. **Add Comprehensive Tests**
   - No test files found for basis module
   - Need unit tests for each function
   - Need integration tests for FTRAN/BTRAN chains

---

## Testing Gaps

**No test files found for basis module.**

Required test coverage:
- ✅ FTRAN with various eta patterns
- ✅ BTRAN with reverse traversal
- ✅ Pivot update creating eta vectors
- ✅ Refactorization on identity and structural bases
- ✅ Snapshot create/restore/compare
- ✅ Warm start with dimension changes
- ✅ Validation with invalid bases
- ✅ Numerical stability edge cases

**Recommendation:** Create `tests/test_basis.c` with comprehensive coverage.

---

## Conclusion

The basis module has a **solid foundation** but **critical gaps** prevent full functionality:

**Strengths:**
- ✅ Clean FTRAN/BTRAN implementations
- ✅ Good error handling in completed functions
- ✅ Efficient stack allocation strategy in BTRAN
- ✅ Complete snapshot functionality

**Weaknesses:**
- ❌ Missing pivot update function (blocker)
- ❌ Refactorization is stub only (blocker)
- ❌ Warm start oversimplified
- ❌ Validation incomplete

**Compliance Score: 5/10 functions complete**

**Blocker Count: 2 critical functions missing/incomplete**

The module **cannot support simplex iterations** without completing `cxf_pivot_with_eta` and `cxf_basis_refactor`. These should be the **top priority** for next implementation phase.

---

## Appendix: Function Location Map

| Function | File | LOC | Status |
|----------|------|-----|--------|
| cxf_ftran | src/basis/ftran.c | 96 | Done |
| cxf_btran | src/basis/btran.c | 138 | Done |
| cxf_eta_create | src/basis/eta_factors.c | 60 | Done |
| cxf_eta_free | src/basis/eta_factors.c | 8 | Done |
| cxf_eta_init | src/basis/eta_factors.c | 41 | Done |
| cxf_eta_validate | src/basis/eta_factors.c | 45 | Done |
| cxf_eta_set | src/basis/eta_factors.c | 18 | Done |
| cxf_basis_snapshot_create | src/basis/snapshot.c | 51 | Done |
| cxf_basis_snapshot_diff | src/basis/snapshot.c | 29 | Done |
| cxf_basis_snapshot_equal | src/basis/snapshot.c | 3 | Done |
| cxf_basis_snapshot_free | src/basis/snapshot.c | 18 | Done |
| cxf_basis_refactor | src/basis/refactor.c | 16 | Stub |
| cxf_solver_refactor | src/basis/refactor.c | 68 | Stub |
| cxf_refactor_check | src/basis/refactor.c | 34 | Done |
| cxf_basis_validate | src/basis/warm.c | 29 | Partial |
| cxf_basis_validate_ex | src/basis/warm.c | 57 | Partial |
| cxf_basis_warm | src/basis/warm.c | 16 | Partial |
| cxf_basis_warm_snapshot | src/basis/warm.c | 28 | Partial |
| cxf_basis_create | src/basis/basis_state.c | 43 | Done |
| cxf_basis_free | src/basis/basis_state.c | 20 | Done |
| cxf_basis_init | src/basis/basis_state.c | 30 | Done |
| cxf_basis_equal | src/basis/basis_stub.c | 15 | Stub |
| cxf_basis_diff | src/basis/basis_stub.c | 13 | Stub |
| cxf_basis_snapshot | src/basis/basis_stub.c | 13 | Stub |
| **cxf_pivot_with_eta** | **MISSING** | **0** | **None** |

**Total LOC:** ~857 (excluding missing pivot function)
**Completion:** ~70% (by LOC, ~50% by critical functionality)

---

*End of Review*
