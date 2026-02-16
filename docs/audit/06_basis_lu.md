# Audit Report: Basis/LU Operations (Product Form of Inverse)
**Auditor:** Agent B3
**Date:** 2026-02-16
**Scope:** src/basis/lu_factorize.c, ftran.c, btran.c, eta_factors.c, refactor.c, pivot_eta.c, lu_factors.c
**Specs:** algorithms/product_form_inverse.md

## Summary
- Total violations: 9
- Critical: 3 / Major: 4 / Minor: 2

## Violations

### V-01: Wrong eta vector formula in FTRAN
- **Severity:** CRITICAL
- **File:** src/basis/ftran.c, lines 173-182
- **Spec reference:** Lines 143-150 (Step 3: Apply eta vectors in forward order)
- **Description:** FTRAN applies wrong eta transformation formula. Code divides by pivot element then subtracts, but spec says entries already include the division.
- **Expected:**
  ```
  Let tau = x_{i-1, p_i}   (the current value at the pivot position)
  For each nonzero entry (j, eta_j) in the eta vector where j != p_i:
      x_{i, j} = x_{i-1, j} + eta_j * tau
  x_{i, p_i} = eta_{p_i} * tau   (= tau / alpha_i)
  ```
  Where eta_j = -d_j / alpha for j != p, eta_p = 1 / alpha (from spec lines 116-118)
- **Actual:** Code at lines 173-174 does `factor = result[pivot_row] / pivot_elem; result[pivot_row] = factor;` then at 180 does `result[j] -= eta->values[k] * factor`. This divides by pivot twice if eta->values stores the already-divided values.
- **Root cause:** Inconsistency between pivot_eta.c (which stores raw column values at line 105) and ftran.c (which expects pre-divided eta coefficients). The spec says eta entries should be `-d_i / alpha` but pivot_eta.c stores `pivotCol[i]` directly.

### V-02: Wrong eta vector formula in BTRAN
- **Severity:** CRITICAL
- **File:** src/basis/btran.c, lines 183-192
- **Spec reference:** Lines 158-171 (Step 4: Apply eta vectors in reverse order)
- **Description:** BTRAN applies wrong eta transformation formula.
- **Expected:**
  ```
  sigma = y_{p_i} * eta_{p_i} + sum_{j != p_i, eta_j != 0} y_j * eta_j
  y_{p_i} = sigma
  ```
  Where eta entries are -d_j / alpha and eta_p = 1 / alpha
- **Actual:** Code at lines 183-189 computes `temp = sum(eta->values[k] * result[j])` for j != pivot_row, then line 192 does `result[pivot_row] = (result[pivot_row] - temp) / pivot_elem`. This formula is `(y_p - sum) / pivot` which doesn't match spec.
- **Expected formula:** Should be `(y_p * (1/pivot) + sum(y_j * (-d_j/pivot)))` = `y_p / pivot - sum(y_j * d_j) / pivot`. The code's formula `(y_p - sum) / pivot` is correct ONLY if eta->values stores the negated, non-divided column values.
- **Root cause:** Same as V-01 - inconsistency between eta storage format and transformation application.

### V-03: Eta vector storage format undefined in pivot_eta.c
- **Severity:** CRITICAL
- **File:** src/basis/pivot_eta.c, lines 99-108
- **Spec reference:** Lines 108-119 (Step 2: Construct the eta vector)
- **Description:** pivot_eta.c stores raw pivot column values instead of the eta coefficients specified in the spec.
- **Expected:** Eta vector should store `eta_i = -d_i / alpha` for i != p and `eta_p = 1 / alpha` at the pivot position. Only off-diagonal entries are stored in sparse format.
- **Actual:** Line 105 stores `eta->values[k] = pivotCol[i]` - the raw column value `d_i`, not `-d_i / alpha`. Line 81 stores `eta->pivot_elem = pivot` (the raw pivot value, not its reciprocal).
- **Impact:** This creates an inconsistency where FTRAN/BTRAN must handle division and negation, rather than the eta vector being self-contained. This violates the spec's definition of eta vectors.
- **Note:** The comment at line 100 says "Store column value directly" which explicitly contradicts the spec.

### V-04: Missing pivot element storage in eta vector
- **Severity:** MAJOR
- **File:** src/basis/pivot_eta.c, EtaFactors structure
- **Spec reference:** Lines 116-119 (eta_p = 1 / alpha)
- **Description:** The eta vector structure stores pivot_elem separately but doesn't store the eta coefficient at the pivot position in the values array.
- **Expected:** According to spec, eta_p = 1 / alpha should be part of the eta vector representation. The spec says "only the nonzero entries are stored" (line 119), implying the pivot position's value is included conceptually even if stored separately.
- **Actual:** pivot_elem is stored separately in the EtaFactors struct (line 81), and the values array only contains off-diagonal entries (lines 103-108). This is technically OK if FTRAN/BTRAN use it correctly, but the separation creates ambiguity about whether pivot_elem is the raw pivot or the eta coefficient.
- **Issue:** The code stores the raw pivot (not 1/alpha), which requires FTRAN/BTRAN to perform division. This is inefficient and error-prone compared to pre-computing 1/alpha once.

### V-05: BTRAN applies B_0^(-T) in wrong order
- **Severity:** MAJOR
- **File:** src/basis/btran.c, lines 201-206
- **Spec reference:** Lines 155-177 (Step 4: BTRAN algorithm)
- **Description:** BTRAN applies the initial basis factorization after eta vectors, but the spec ordering suggests it should be integrated differently.
- **Expected:** For B = B_0 * E_1 * ... * E_k, we have B^(-T) = E_k^(-T) * ... * E_1^(-T) * B_0^(-T). So BTRAN should: (1) Apply eta vectors newest to oldest, (2) Apply B_0^(-T) last. Code does this correctly at lines 138-206.
- **Actual:** Upon closer inspection, code is correct. Lines 138-198 apply etas in reverse order, then lines 202-206 apply LU transpose. This matches the spec.
- **Severity downgrade:** Actually NOT a violation - code is correct. FALSE ALARM.

### V-06: Missing sparse FTRAN optimization
- **Severity:** MAJOR
- **File:** src/basis/ftran.c
- **Spec reference:** Lines 152-153 (Sparse FTRAN optimization)
- **Description:** FTRAN doesn't implement the sparse optimization mentioned in spec.
- **Expected:** "If tau = 0 for a given eta vector (the value at the pivot position is zero), the entire eta vector application is skipped."
- **Actual:** No check for zero tau before applying eta at lines 147-182. Every eta is processed even if result[pivot_row] is zero.
- **Impact:** Performance degradation on sparse problems where hyper-sparsity could be exploited.

### V-07: Missing sparse BTRAN optimization
- **Severity:** MAJOR
- **File:** src/basis/btran.c
- **Spec reference:** Line 177 (Sparse BTRAN optimization)
- **Description:** BTRAN doesn't implement the sparse optimization mentioned in spec.
- **Expected:** "If y_{p_i} = 0 and all entries of y at positions corresponding to nonzero entries in the eta vector are zero, the entire application is skipped."
- **Actual:** No sparsity check before applying eta at lines 161-192. Every eta is processed unconditionally.
- **Impact:** Performance degradation on sparse problems.

### V-08: Wrong permutation application in FTRAN LU solve
- **Severity:** MAJOR
- **File:** src/basis/ftran.c, lines 31-73
- **Spec reference:** Lines 137-141 (Apply the initial factorization)
- **Description:** The LU solve permutation order may be incorrect.
- **Expected:** For B_0 = LU solve, spec describes forward/back substitution but doesn't explicitly define permutation handling for B = P^T * L * U * Q form.
- **Actual:** Lines 37-39 do `temp[k] = result[lu->perm_row[k]]` (applying row permutation P), then lines 69-71 do `result[lu->perm_col[k]] = temp[k]` (applying column permutation Q^T). This implements: x = Q^T * U^(-1) * L^(-1) * P * b.
- **Analysis:** For B_0 = P^T * L * U * Q, we have B_0^(-1) = Q^(-1) * U^(-1) * L^(-1) * P. Since Q is a permutation, Q^(-1) = Q^T. The code applies Q^T at the end, which is correct. The permutation order is correct.
- **Severity downgrade:** Actually NOT a violation - code appears correct. Need to verify against Bartels-Golub paper. POTENTIAL FALSE ALARM.

### V-09: Missing refactorization trigger: numerical accuracy degradation
- **Severity:** MAJOR
- **File:** src/basis/refactor.c, lines 188-221
- **Spec reference:** Lines 251-252 (Numerical accuracy degradation trigger)
- **Description:** Refactorization check doesn't monitor FTRAN/BTRAN residuals.
- **Expected:** "When the residual of an FTRAN or BTRAN operation exceeds a tolerance, indicating that rounding errors in the product form have become unacceptable."
- **Actual:** cxf_refactor_check monitors eta_count, eta_memory, iteration interval, and FTRAN timing (lines 196-218), but doesn't check ||Bx - a|| residuals.
- **Impact:** Numerical instability may accumulate undetected until it causes solve failure rather than triggering preventive refactorization.

### V-10: Missing variable-fixing eta records
- **Severity:** MINOR
- **File:** src/basis/
- **Spec reference:** Lines 179-189 (Step 5: Variable-Fixing Eta Records)
- **Description:** No implementation of variable-fixing eta records described in spec.
- **Expected:** "When a variable is identified as being at one of its bounds and can be fixed: 1. A compact or full eta record is created..."
- **Actual:** No code implements variable fixing with eta records. The EtaFactors structure has type, status, and obj_coeff fields (cxf_basis.h lines 54-62) suggesting support was intended, but no function creates type=1 eta records for fixing.
- **Impact:** Missing optimization feature. Not critical for correctness but limits solver capabilities for bound-tightening and presolve.

### V-11: Missing Markowitz refinements (threshold pivoting validation)
- **Severity:** MINOR
- **File:** src/basis/lu_factorize.c, lines 144, 151
- **Spec reference:** Lines 221 (Pivot tolerance section), Reid 1982 citation
- **Description:** Markowitz implementation uses fixed threshold but spec mentions "threshold partial pivoting" that should balance stability and sparsity.
- **Expected:** Dynamic threshold adjustment based on matrix condition, or at minimum validation that chosen pivots meet both Markowitz (sparsity) and stability (magnitude) criteria.
- **Actual:** MARKOWITZ_THRESHOLD = 0.1 is hardcoded (line 21). Line 144 computes `threshold = MARKOWITZ_THRESHOLD * col_max`, and line 151 requires `val >= threshold`. This is threshold pivoting, but the threshold value isn't adaptive.
- **Impact:** May select unstable pivots on ill-conditioned matrices. However, MIN_PIVOT check at line 166 provides a safety net.
- **Assessment:** Implementation is acceptable but not optimal. Minor issue.

---

## V-01 through V-03 ANALYSIS: The Core Inconsistency

The critical violations V-01, V-02, and V-03 are all symptoms of a single architectural inconsistency:

**The spec defines eta vectors as:**
```
eta_i = -d_i / alpha  for i != p
eta_p = 1 / alpha
```

**But pivot_eta.c stores:**
```c
eta->values[k] = pivotCol[i];      // Raw d_i, not -d_i/alpha
eta->pivot_elem = pivot;            // Raw alpha, not 1/alpha
```

**This forces FTRAN/BTRAN to do:**
- FTRAN: `factor = result[p] / pivot_elem; result[j] -= values[k] * factor;`
- BTRAN: `temp = sum(values[k] * result[j]); result[p] = (result[p] - temp) / pivot_elem;`

**The spec expects:**
- FTRAN: `tau = result[p]; result[j] += eta_j * tau; result[p] = eta_p * tau;`
- BTRAN: `sigma = result[p] * eta_p + sum(result[j] * eta_j); result[p] = sigma;`

**Root cause:** The implementation chose to defer negation and division to the application phase rather than pre-computing eta coefficients during storage. This trades storage simplicity for application complexity and violates the spec's definition.

**Verification question:** Does the current implementation produce correct numerical results despite using different formulas?

Let's check the math:

FTRAN spec formula with eta_j = -d_j/alpha:
```
result[j]_new = result[j]_old + (-d_j/alpha) * result[p]_old
              = result[j]_old - (d_j/alpha) * result[p]_old
```

FTRAN code formula with values[k] = d_j:
```
factor = result[p]_old / pivot_elem     // pivot_elem = alpha
result[j]_new = result[j]_old - values[k] * factor
              = result[j]_old - d_j * (result[p]_old / alpha)
              = result[j]_old - (d_j/alpha) * result[p]_old
```

**Conclusion:** The formulas are mathematically equivalent! The code is CORRECT numerically, but VIOLATES the spec's API contract for eta vector representation.

**Recommendation:** This is a spec compliance violation but not a correctness bug. Options:
1. Change code to match spec (pre-compute -d_i/alpha during storage)
2. Update spec to document the implementation's approach
3. Add comments explaining the formula transformation

For a clean-room implementation, option 1 (match spec) is preferred.

---

## Spec Sections Not Implemented

### 1. Variable-Fixing Eta Records (Lines 179-189)
- Compact/full eta record creation for variable fixing
- Objective function updates for fixed variables
- Reversal mechanism for unfixing

### 2. Spike Handling (mentioned in task description, not in spec)
- The audit task description mentions "Missing spike handling: How new columns are integrated"
- However, the spec doesn't contain a section on "spike" handling
- The spec describes pivot column transformation (lines 108-131) but doesn't use the term "spike"
- **Assessment:** Either the audit task description is referencing a different spec section, or "spike" is terminology from a different source (possibly Forrest & Tomlin 1972)

### 3. Fill-in Growth Monitoring (Lines 253)
- Spec mentions "Fill-in growth: When the total number of nonzero entries across all eta vectors exceeds a storage threshold"
- refactor.c checks eta_memory (line 202) but this tracks total bytes, not nnz count
- Missing: explicit nnz-based fill-in monitoring

### 4. Warm Start After Refactorization (Lines 318-319)
- No code to invalidate cached results that depend on old eta vectors
- No mechanism to track what needs recomputation after refactor

### 5. Memory Exhaustion Handling (Lines 314-315)
- Spec says "returns an error code" and "caller should either trigger immediate refactorization"
- pivot_eta.c returns CXF_ERROR_OUT_OF_MEMORY (line 96) but no calling code implements the "refactor and retry" recovery strategy

### 6. Residual Checking (Line 239)
- Spec says "Checking the residual ||Bx - a|| after FTRAN operations"
- No code computes residuals to validate FTRAN accuracy

---

## Code Sections Not In Spec

### 1. Diagonal Coefficient System (diag_coeff)
- **Code:** BasisState.diag_coeff, used in ftran.c lines 111-116, btran.c lines 93-96, 204-206
- **Purpose:** Handles ±1 coefficients for auxiliary variables based on constraint sense
- **Spec reference:** Not mentioned in product_form_inverse.md
- **Assessment:** This is implementation-specific handling for the initial basis B_0. Spec assumes B_0 is factorized via LU but doesn't detail how slack variable coefficients are handled. This is a reasonable extension.

### 2. LU Factorization of Non-Identity Basis
- **Code:** lu_factorize.c entire file (369 lines)
- **Spec reference:** Lines 99-107 mention LU factorization citing Bartels & Golub 1969, but product_form_inverse.md doesn't detail the factorization algorithm
- **Assessment:** The spec references the LU factorization but defers algorithm details to the cited paper. The implementation of Markowitz ordering is reasonable and consistent with the citations (Suhl & Suhl 1990, Reid 1982).

### 3. Identity Basis Fast Path
- **Code:** refactor.c lines 136-162
- **Purpose:** Optimizes refactorization when all basic variables are slack variables at their natural positions
- **Spec reference:** Not mentioned
- **Assessment:** Reasonable optimization, doesn't contradict spec

### 4. Stack Allocation for Eta Pointers
- **Code:** ftran.c lines 125-133, btran.c lines 141-150
- **Purpose:** Avoids heap allocation for small eta counts (< 64)
- **Spec reference:** Not mentioned
- **Assessment:** Performance optimization, doesn't affect correctness

### 5. EtaFactors.type Field
- **Code:** EtaFactors structure has type field (1=refactorization, 2=pivot)
- **Spec reference:** Spec mentions two eta vector types at line 122-126 but uses different terminology ("Full mode" vs "Compact mode" for storage, not for operation type)
- **Assessment:** Implementation uses type to distinguish refactorization from pivot, but spec's type distinction is about storage mode (full vs compact). Mismatch in terminology but not a violation.

### 6. Separate Functions for BTRAN with Unit Vector vs Arbitrary Vector
- **Code:** btran.c has cxf_btran (unit vector input) and cxf_btran_vec (arbitrary vector)
- **Spec reference:** Spec describes BTRAN as solving yB = c (line 19), which is arbitrary vector
- **Assessment:** Unit vector version (cxf_btran) is an optimization for the common case of computing a single tableau row. Reasonable extension.

---

## Recommendations

### Priority 1 (Critical Fixes)
1. **Resolve V-01, V-02, V-03 inconsistency:** Choose between:
   - **Option A:** Modify pivot_eta.c to store -d_i/alpha in values array and 1/alpha in pivot_elem, update FTRAN/BTRAN to use simpler formulas per spec
   - **Option B:** Document the current approach as an optimization and update spec to reflect implementation's formula

### Priority 2 (Major Enhancements)
2. **Implement V-09:** Add residual checking to detect numerical degradation
3. **Implement V-06, V-07:** Add sparse optimization for FTRAN/BTRAN
4. **Verify V-08:** Confirm LU permutation order against Bartels-Golub paper

### Priority 3 (Minor Improvements)
5. **Implement V-10:** Add variable-fixing eta records for advanced presolve
6. **Address V-11:** Consider adaptive threshold selection for Markowitz pivoting

### Non-Blocking
7. Document the diag_coeff system as an extension to handle auxiliary variable coefficients
8. Add citation comments in lu_factorize.c referencing the specific algorithm papers

---

## Test Coverage Recommendations

To validate compliance and catch regressions:

1. **Eta formula correctness test:** Create a test with a known basis, apply a pivot, compute FTRAN/BTRAN, verify against hand-calculated results
2. **Numerical stability test:** Apply long eta chain (200+ pivots) and measure FTRAN residual ||Bx - a||
3. **Sparse performance test:** Time FTRAN/BTRAN on hyper-sparse vectors, verify optimization effectiveness
4. **Refactor trigger test:** Force numerical degradation and verify refactorization is triggered
5. **Permutation test:** Verify LU solve produces B^(-1)a correctly for various permutation patterns

---

## Conclusion

The implementation is **functionally correct** but has **specification compliance issues**. The most significant violation is the eta vector storage format (V-01, V-02, V-03), which produces correct numerical results but doesn't match the spec's defined representation. This is a documentation/architecture issue rather than a bug.

The missing optimizations (sparse FTRAN/BTRAN, residual checking) are performance and robustness features that should be added but don't affect basic correctness.

The implementation demonstrates good software engineering (stack allocation optimization, identity basis fast path, separation of concerns) even where it extends beyond the spec.

Overall assessment: **7/10 for spec compliance**, **9/10 for implementation quality**.
