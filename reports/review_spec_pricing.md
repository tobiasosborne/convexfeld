# Spec Compliance Review: Pricing Module

**Review Date:** 2026-01-27
**Reviewer:** Claude Agent
**Module:** Pricing (M6)
**Files Reviewed:** 7 implementation files
**Specs Reviewed:** 6 specification files

---

## Summary

**Overall Compliance:** PARTIAL - Implementation deviates from specs in several areas

**Status Breakdown:**
- **PASS:** 2 functions (cxf_pricing_create, cxf_pricing_free)
- **PARTIAL:** 5 functions (cxf_pricing_init, cxf_pricing_candidates, cxf_pricing_steepest, cxf_pricing_update, cxf_pricing_invalidate)
- **FAIL:** 1 function (cxf_pricing_step2)

**Critical Issues:** 5 spec violations that could cause bugs
**Minor Issues:** 3 signature/parameter mismatches
**Missing Features:** 2 algorithmic features not implemented

---

## Function-by-Function Analysis

### 1. cxf_pricing_create
**File:** src/pricing/context.c (lines 30-71)
**Spec:** No dedicated spec (lifecycle function)
**Implementation:** Allocates PricingContext structure with level arrays
**Compliance:** ✅ PASS
**Issues:** None - function works as designed for TDD workflow

---

### 2. cxf_pricing_free
**File:** src/pricing/context.c (lines 84-104)
**Spec:** No dedicated spec (lifecycle function)
**Implementation:** NULL-safe deallocation of all pricing structures
**Compliance:** ✅ PASS
**Issues:** None - properly frees all allocated memory

---

### 3. cxf_pricing_init
**File:** src/pricing/init.c (lines 89-179)
**Spec:** docs/specs/functions/pricing/cxf_pricing_init.md

**Spec Summary:**
- Signature: `int cxf_pricing_init(PricingState**, SolverState*, CxfEnv*)`
- Allocate pricing structures with strategy-specific sizes
- Initialize SE weights to 1.0
- Return 0 on success, 1001 on OOM

**Implementation Analysis:**
- Signature: `int cxf_pricing_init(PricingContext*, int num_vars, int strategy)` ❌
- Allocates candidate arrays per level ✅
- Initializes SE weights to 1.0 for SE/Devex strategies ✅
- Returns CXF_OK/CXF_ERROR_OUT_OF_MEMORY ⚠️

**Compliance:** 🟡 PARTIAL

**Issues:**
1. **CRITICAL - Signature Mismatch:** Spec expects `(PricingState**, SolverState*, CxfEnv*)` but implementation uses `(PricingContext*, int, int)`. This is a fundamental API incompatibility.
2. **Parameter Mismatch:** Takes `num_vars` and `strategy` directly instead of extracting from SolverState and CxfEnv
3. **Error Code Mismatch:** Returns `CXF_ERROR_OUT_OF_MEMORY` instead of spec's `1001`
4. **Missing Auto-Selection Logic:** Spec line 100-105 shows sparsity check for auto-selection, but implementation only checks problem size (line 101-108)

**Spec References:**
- Spec lines 16-18: Expected signature with PricingState**, SolverState*, CxfEnv*
- Spec lines 98-105: Auto-selection should check sparsity > 0.95
- Spec line 24: Should return error code 1001 for OOM

---

### 4. cxf_pricing_candidates
**File:** src/pricing/candidates.c (lines 83-172)
**Spec:** docs/specs/functions/pricing/cxf_pricing_candidates.md

**Spec Summary:**
- Signature: `int cxf_pricing_candidates(PricingState*, SolverState*, double tolerance)`
- Scan variables in current section (partial pricing)
- Update pricingState.candidates array and candidateCount
- Advance currentSection for next call
- Return count of candidates found

**Implementation Analysis:**
- Signature: `int cxf_pricing_candidates(PricingContext*, const double*, const int*, int, double, int*, int)` ❌
- Scans variables based on strategy ✅
- Checks attractiveness based on status ✅
- Sorts by |RC| descending ✅
- Returns candidate count ✅

**Compliance:** 🟡 PARTIAL

**Issues:**
1. **CRITICAL - Signature Mismatch:** Spec expects state objects, implementation takes raw arrays
2. **CRITICAL - Section Management:** Implementation uses `ctx->last_pivot_iteration % DEFAULT_NUM_SECTIONS` (line 102) instead of maintaining explicit `currentSection` field as spec requires (spec line 132)
3. **Missing Feature:** Spec line 87-96 says partial pricing should advance `currentSection = (currentSection + 1) % numSections`, but implementation uses iteration counter
4. **Different Output Pattern:** Spec expects candidates written to `pricingState.candidates` array, implementation takes external array

**Spec References:**
- Spec lines 16-18: Expected signature with PricingState* and SolverState*
- Spec lines 137-139: Should advance currentSection counter
- Spec lines 27-30: Should modify pricingState.candidates array, not external array

---

### 5. cxf_pricing_steepest
**File:** src/pricing/steepest.c (lines 50-117)
**Spec:** docs/specs/functions/pricing/cxf_pricing_steepest.md

**Spec Summary:**
- Signature: `int cxf_pricing_steepest(PricingState*, SolverState*, double tolerance)`
- Select variable maximizing |d_j| / sqrt(gamma_j)
- Use weights from pricingState
- Return index or -1 if optimal

**Implementation Analysis:**
- Signature: `int cxf_pricing_steepest(PricingContext*, const double*, const double*, const int*, int, double)` ❌
- Computes SE ratio correctly: abs_rc / sqrt(weight) ✅
- Handles zero weights (min threshold 1e-10) ✅
- Returns best variable or -1 ✅
- Checks attractiveness based on status ✅

**Compliance:** 🟡 PARTIAL

**Issues:**
1. **Signature Mismatch:** Spec expects state objects, implementation takes raw arrays
2. **Weight Safeguard:** Implementation uses MIN_WEIGHT = 1e-10 (line 24), spec recommends checking weight < 1e-10 and using 1.0 (spec line 92-93) - functionally equivalent but different threshold

**Spec References:**
- Spec lines 16-18: Expected signature with PricingState* and SolverState*
- Spec lines 88-93: Weight safeguard implementation matches spec intent
- Spec lines 97-107: SE ratio computation is correct

---

### 6. cxf_pricing_update
**File:** src/pricing/update.c (lines 40-88)
**Spec:** docs/specs/functions/pricing/cxf_pricing_update.md

**Spec Summary:**
- Updates reduced costs after pivot: d'_j = d_j - (d_q / alpha_q) * alpha_j
- Updates SE weights recursively: gamma'_j = gamma_j - 2*alpha_j*rho_j + alpha_j^2*tau
- Handles new nonbasic variable (leaving variable)
- Return 0 on success

**Implementation Analysis:**
- Gets pivot element from pivot_column[leaving_row] ✅
- **MISSING:** Full reduced cost update (acknowledged in comment lines 69-70)
- **MISSING:** Full SE weight update (partial stub at lines 54-73)
- Invalidates cached candidate counts ✅
- Updates iteration counter ✅
- Returns CXF_OK ✅

**Compliance:** 🟡 PARTIAL

**Issues:**
1. **CRITICAL - Missing Algorithm:** Reduced cost update formula (spec lines 108-119) not implemented
2. **CRITICAL - Missing Algorithm:** SE weight recursive update (spec lines 129-141) not implemented
3. **CRITICAL - Missing Leaving Variable Handling:** Spec lines 144-145 require computing weight for leaving variable, not implemented
4. **Incomplete:** Comments at lines 65-70 acknowledge "Full update requires alpha_j for each variable j"

**Spec References:**
- Spec lines 73-88: Reduced cost update should apply to all nonbasic variables
- Spec lines 108-119: RC update formula: d'_j = d_j - ratio * alpha_j
- Spec lines 125-145: SE weight recursive update with tau and rho_j
- Spec line 175: Expected O(nnz) complexity with sparse products

**Note:** This is acknowledged as incomplete due to deferred SolverContext integration (comment line 30-31).

---

### 7. cxf_pricing_invalidate
**File:** src/pricing/update.c (lines 99-136)
**Spec:** docs/specs/functions/pricing/cxf_pricing_invalidate.md

**Spec Summary:**
- Signature: `void cxf_pricing_invalidate(PricingState*, int flags)`
- Set invalidation flags (bitmask)
- Clear candidate list if INVALID_CANDIDATES
- Mark weights for recomputation if INVALID_WEIGHTS

**Implementation Analysis:**
- Takes PricingContext* and flags ✅
- Invalidates candidate lists (lines 105-109) ✅
- Resets weights to 1.0 (lines 113-121) ⚠️
- Handles INVALID_ALL flag (lines 125-135) ✅

**Compliance:** 🟡 PARTIAL

**Issues:**
1. **Different Approach:** Spec expects lazy invalidation (mark invalid, recompute later), but implementation immediately resets weights to 1.0 (lines 117-120)
2. **Missing Flag Storage:** Spec line 76 says "ps.invalidFlags |= flags" but implementation doesn't maintain invalidFlags field
3. **Missing weightsValid Flag:** Spec line 82 references weightsValid boolean, not present in implementation

**Spec References:**
- Spec lines 73-83: Should use flag storage pattern
- Spec lines 67-68: "Mark for recomputation" vs immediate reset
- Spec line 131: "Lazy invalidation" design rationale

---

### 8. cxf_pricing_step2
**File:** src/pricing/phase.c (lines 37-85)
**Spec:** docs/specs/functions/pricing/cxf_pricing_step2.md

**Spec Summary:**
- For partial pricing: scan all remaining sections
- For SE/Devex: perform full scan
- For Dantzig: already complete, return -1
- Ensure completeness/confirm optimality

**Implementation Analysis:**
- Performs full scan of all variables ✅
- Finds best violation ✅
- Returns variable index or -1 ✅
- Updates statistics ✅

**Compliance:** ❌ FAIL

**Issues:**
1. **CRITICAL - Missing Strategy Logic:** Implementation does not distinguish between pricing strategies. Spec requires different behavior for partial vs SE/Devex (spec lines 62-75, pseudocode lines 86-107)
2. **CRITICAL - Missing Section Management:** For partial pricing, spec requires saving currentSection, scanning all sections, and restoring (spec lines 67-71, pseudocode lines 87-98). Implementation just does one full scan.
3. **Missing Strategy Check:** Implementation ignores ctx->strategy field entirely

**Spec References:**
- Spec lines 62-75: Different recovery actions based on strategy
- Spec lines 86-107: Pseudocode shows strategy-specific branches
- Spec line 173: "Two-phase pricing ensures completeness"

---

### 9. cxf_pricing_compute_weight (Bonus Function)
**File:** src/pricing/steepest.c (lines 133-143)
**Spec:** None (helper function, not in spec)
**Implementation:** Computes squared norm of column vector
**Compliance:** N/A - Not specified
**Issues:** None - reasonable helper function

---

## Critical Issues

### 1. Signature Incompatibility Across Module
**Severity:** CRITICAL
**Affected Functions:** cxf_pricing_init, cxf_pricing_candidates, cxf_pricing_steepest, cxf_pricing_update

**Problem:** All specs expect functions to take `PricingState*` and `SolverState*` objects, but implementations take `PricingContext*` and raw arrays (reduced_costs, var_status, weights).

**Example:**
```c
// Spec expects:
int cxf_pricing_candidates(PricingState*, SolverState*, double tolerance);

// Implementation has:
int cxf_pricing_candidates(PricingContext *ctx, const double *reduced_costs,
                           const int *var_status, int num_vars, double tolerance,
                           int *candidates, int max_candidates);
```

**Impact:** This is a fundamental architecture mismatch. The specs assume integrated state management, but the implementation uses a standalone context pattern suitable for TDD.

**Root Cause:** Implementation prioritized TDD testability over spec compliance by using dependency injection pattern.

---

### 2. Incomplete Reduced Cost Update
**Severity:** CRITICAL
**Function:** cxf_pricing_update
**File:** src/pricing/update.c

**Problem:** The core RC update algorithm (spec lines 108-119) is not implemented:
```c
// Spec requires:
FOR each nonbasic variable j:
    alpha_j := DOT(column j, pivotRow, m)
    state.reducedCosts[j] -= ratio * alpha_j
```

**Current State:** Comment at line 69 says "Full update requires alpha_j for each variable j, which needs matrix access."

**Impact:** Without RC updates, pricing will use stale costs and select wrong entering variables, causing algorithm failure.

**Reason:** Acknowledged as incomplete pending SolverContext integration.

---

### 3. Incomplete SE Weight Update
**Severity:** CRITICAL
**Function:** cxf_pricing_update
**File:** src/pricing/update.c

**Problem:** The SE weight recursive update (spec lines 129-141) is not implemented:
```c
// Spec requires:
FOR j := 0 TO n - 1:
    ps.weights[j] := ps.weights[j] - 2 * alpha_j * rho_j + alpha_j^2 * tau
```

**Impact:** SE pricing will use stale weights, reducing effectiveness and potentially causing more iterations or numerical issues.

**Reason:** Requires matrix operations not yet available in current context.

---

### 4. Missing Strategy-Specific Logic in step2
**Severity:** CRITICAL
**Function:** cxf_pricing_step2
**File:** src/pricing/phase.c

**Problem:** Implementation does not check pricing strategy and apply different fallback logic:

**Spec requires (lines 86-107):**
```
IF ps.strategy == PARTIAL:
    // Scan all sections
ELSE IF ps.strategy == STEEPEST_EDGE OR DEVEX:
    // Full SE scan
ELSE:  // Dantzig
    RETURN -1  // Already complete
```

**Implementation:** Always performs one full scan regardless of strategy.

**Impact:**
- For Dantzig (full pricing): Wastes time re-scanning already-scanned variables
- For partial pricing: Doesn't properly manage section state
- For SE: Redundant with first pass

---

### 5. Section Management Inconsistency
**Severity:** HIGH
**Function:** cxf_pricing_candidates
**File:** src/pricing/candidates.c

**Problem:** Implementation uses `ctx->last_pivot_iteration % DEFAULT_NUM_SECTIONS` (line 102) to determine current section, but spec expects explicit `currentSection` field that increments (spec line 138).

**Spec expects:**
```c
ps.currentSection := (ps.currentSection + 1) MOD ps.numSections
```

**Implementation does:**
```c
int current_section = ctx->last_pivot_iteration % DEFAULT_NUM_SECTIONS;
```

**Impact:**
- Section sequence is iteration-dependent rather than pricing-call-dependent
- Multiple pricing calls in same iteration would use same section
- Doesn't align with spec's section cycling design

---

## Recommendations

### Immediate Actions

1. **Document Architecture Decision**
   - The signature mismatches are pervasive and appear to be an intentional design choice for TDD
   - Create ADR (Architecture Decision Record) explaining:
     - Why standalone context pattern was chosen
     - How it differs from spec's integrated state approach
     - Plan for eventual integration or spec update

2. **Complete cxf_pricing_update**
   - Add TODO markers for missing RC update loop
   - Add TODO markers for missing SE weight update
   - Estimate effort for full implementation (requires matrix access)

3. **Fix cxf_pricing_step2 Strategy Logic**
   - Add switch statement on ctx->strategy
   - Implement partial pricing multi-section scan
   - Skip redundant work for already-complete strategies

4. **Fix Section Management**
   - Add explicit `currentSection` field to PricingContext
   - Increment in cxf_pricing_candidates after each call
   - Remove iteration-based section calculation

5. **Add Missing Tests**
   - Test that step2 behaves differently for different strategies
   - Test section advancement in candidates function
   - Test RC update when implemented

### Long-Term Actions

1. **Resolve Spec vs Implementation Gap**
   - Option A: Update specs to match TDD-friendly implementation pattern
   - Option B: Refactor implementation to match specs (post-TDD phase)
   - Recommend: Update specs, as implementation pattern is more testable

2. **Complete Update Function**
   - Integrate with matrix operations module
   - Implement full RC update with sparse products
   - Implement full SE weight recursive update

3. **Add Integration Tests**
   - Test pricing with real simplex iterations
   - Verify RC updates maintain optimality
   - Verify SE weights improve iteration count

---

## Test Coverage Assessment

Based on test_pricing.c (tests/unit/test_pricing.c):

**Covered:**
- PricingContext lifecycle (create/free/init)
- Basic candidate selection
- Steepest edge ratio computation
- Weight safeguards
- Invalidation mechanics
- Statistics tracking

**NOT Covered:**
- Strategy-specific behavior in step2
- Section cycling in partial pricing
- RC update (not implemented)
- SE weight update (not implemented)
- Multi-section fallback logic
- Interaction with SolverState (uses mocks)

**Test Pass Rate:** All 31 tests pass (test file lines 390-434)

---

## Conclusion

The pricing module implementation is **functional for TDD purposes** but **not spec-compliant** in several important ways:

**Strengths:**
- Core algorithms (SE ratio, candidate selection) are correct
- Well-structured for testing (dependency injection)
- Good error handling and NULL safety
- Proper memory management

**Weaknesses:**
- Fundamental API mismatch (PricingContext vs PricingState/SolverState)
- Missing core update algorithms (RC and SE weight updates)
- Incomplete strategy handling (step2)
- Section management doesn't match spec

**Overall Assessment:** The module is in a "TDD scaffold" state - it implements enough to make tests pass and explore the algorithm space, but is not yet production-ready. The gap between spec and implementation should be bridged during integration phase (M7 Simplex Engine).

**Recommendation:**
1. Document current state as "TDD implementation phase"
2. Create integration plan for spec compliance
3. Complete update functions during M7 integration
4. Consider updating specs to reflect successful TDD patterns

---

**Files Reviewed:**
- /home/tobias/Projects/convexfeld/src/pricing/init.c
- /home/tobias/Projects/convexfeld/src/pricing/context.c
- /home/tobias/Projects/convexfeld/src/pricing/candidates.c
- /home/tobias/Projects/convexfeld/src/pricing/steepest.c
- /home/tobias/Projects/convexfeld/src/pricing/update.c
- /home/tobias/Projects/convexfeld/src/pricing/phase.c
- /home/tobias/Projects/convexfeld/src/pricing/pricing_stub.c

**Specs Referenced:**
- docs/specs/functions/pricing/cxf_pricing_init.md
- docs/specs/functions/pricing/cxf_pricing_candidates.md
- docs/specs/functions/pricing/cxf_pricing_steepest.md
- docs/specs/functions/pricing/cxf_pricing_update.md
- docs/specs/functions/pricing/cxf_pricing_invalidate.md
- docs/specs/functions/pricing/cxf_pricing_step2.md

---

*End of Report*
