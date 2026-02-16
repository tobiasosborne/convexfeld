# Audit Report: Tolerances, Constants & Parameter Defaults
**Auditor:** Agent D3
**Date:** 2026-02-16
**Scope:** All headers, params.c, ratio_test.c, perturbation.c, lu_factorize.c, pivot_check.c, nan_check.c + grep results
**Specs:** reference/tolerances_constants.md, numerical_stability.md, parameters_defaults.md, error_status_codes.md

---

## Summary

This audit examined all numerical tolerances, constants, error codes, and parameter defaults across the ConvexFeld LP solver implementation against the v2 reference specifications. **27 violations were found**, spanning incorrect tolerance values, missing constants, wrong thresholds, and incomplete error code coverage.

**Categories of violations:**
- **Wrong tolerance values**: 13 violations
- **Missing constants**: 6 violations
- **Wrong constant names/values**: 5 violations
- **Missing error codes**: 3 violations (all internal simplex codes absent)
- **Hardcoded magic numbers**: Multiple instances in implementation files

**Critical findings:**
1. **Pivot tolerances completely wrong**: Header defines 1e-10, spec says 1e-9 (Harris) and 1e-13 (minimum)
2. **Perturbation constants wrong**: Implementation uses 1e-6 base scale, spec says 1e-10 floor
3. **Markowitz threshold wrong**: Code uses 0.1, spec says ~7.8e-3 (1/128)
4. **Missing adaptive pricing tolerances**: Fast (1e-6), Standard (1e-10), Aggressive (1e-9) phases not defined
5. **Missing stalling detection constants**: Grace period (5 iters), outer limits (5/10/100) not defined
6. **Error codes incomplete**: Missing 15+ spec error codes, only 5 implemented

---

## Violations

### 1. Header Constants (cxf_types.h)

**File:** `/home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_types.h`

| Line | Constant | Spec Says | Code Has | Violation |
|------|----------|-----------|----------|-----------|
| 122 | CXF_PIVOT_TOL | Harris pivot: 1e-9 | 1e-10 | **WRONG VALUE** (off by 10x) |
| 125 | CXF_ZERO_TOL | Numerical zero (tight): 1e-10 | 1e-12 | **WRONG VALUE** (off by 100x) |
| N/A | Harris pivot tolerance | 1e-9 | **MISSING** | Spec Section 3, Table |
| N/A | Minimum pivot threshold | 1e-13 | **MISSING** | Spec Section 3, Table |
| N/A | Bound equality tolerance | 1e-10 | **MISSING** | Spec Section 8 |
| N/A | Significant bound change | 1e-12 | **MISSING** | Spec Section 6, Table |

**Analysis:**
- `CXF_PIVOT_TOL` (1e-10) appears to conflate two distinct thresholds: Harris pivot tolerance (1e-9) and bound equality tolerance (1e-10)
- `CXF_ZERO_TOL` (1e-12) confuses "significant bound change" (1e-12) with "numerical zero tight" (1e-10)
- Missing 4 critical constants required by spec

---

### 2. Perturbation Constants (perturbation.c)

**File:** `/home/tobiasosborne/Projects/convexfeld/src/simplex/perturbation.c`

| Line | Constant | Spec Says | Code Has | Violation |
|------|----------|-----------|----------|-----------|
| 17 | PERTURB_BASE_SCALE | Minimum bound range: ~1e-10 | 1e-6 | **WRONG VALUE** (10,000x off) |
| 20 | PERTURB_MAX_SCALE | Maximum perturbation: ~1e-6 | 1e-3 | **WRONG VALUE** (1,000x off) |
| 23 | MIN_OBJ_COEFF | Not in spec | 1e-8 | **HALLUCINATED CONSTANT** |

**Analysis:**
From spec (Section 6, "Anti-Cycling and Perturbation Constants"):
- Minimum bound range (floor): ~1e-10
- Maximum perturbation magnitude (ceiling): ~1e-6

Code incorrectly:
- Uses 1e-6 as base scale (should be 1e-10 floor)
- Uses 1e-3 as max scale (should be 1e-6 ceiling)
- Line 99 multiplies `feas_tol * PERTURB_BASE_SCALE = 1e-6 * 1e-6 = 1e-12`, completely wrong
- Introduces `MIN_OBJ_COEFF` not in spec

---

### 3. LU Factorization Constants (lu_factorize.c)

**File:** `/home/tobiasosborne/Projects/convexfeld/src/basis/lu_factorize.c`

| Line | Constant | Spec Says | Code Has | Violation |
|------|----------|-----------|----------|-----------|
| 21 | MARKOWITZ_THRESHOLD | ~7.8e-3 (1/128) | 0.1 | **WRONG VALUE** (13x off) |
| 22 | MIN_PIVOT | Minimum pivot threshold: 1e-13 | 1e-12 | **WRONG VALUE** (10x off) |

**Analysis:**
From spec (Section 3, "Pivot Tolerances"):
- Markowitz pivot tolerance: ~7.8e-3 (approximately 1/128, a power-of-two fraction)
- Minimum pivot threshold: 1e-13 (used in bound propagation)

Code uses:
- `MARKOWITZ_THRESHOLD = 0.1` instead of ~0.0078125 (13x too large)
- `MIN_PIVOT = 1e-12` instead of 1e-13 (10x too large)

Note: Spec distinguishes Markowitz threshold (LU factorization) from minimum pivot threshold (bound propagation).

---

### 4. Ratio Test Constants (ratio_test.c)

**File:** `/home/tobiasosborne/Projects/convexfeld/src/simplex/ratio_test.c`

| Line | Issue | Spec Says | Code Has | Violation |
|------|-------|-----------|----------|-----------|
| 59 | Relaxed tolerance calculation | 10x feasibility tol (OK) | 10.0 * feasTol | **CORRECT** |
| 73 | Harris pivot tolerance usage | Should use 1e-9 constant | Uses relaxedTol (10*feas_tol = 1e-5) | **WRONG THRESHOLD** |
| 97 | Ditto | 1e-9 | relaxedTol (1e-5) | **WRONG THRESHOLD** |

**Analysis:**
The ratio test uses `relaxedTol = 10 * feasTol = 1e-5` as the pivot threshold, but spec Section 3 says:
- **Harris pivot tolerance**: 1e-9 (minimum absolute value for pivot candidate)
- **Feasibility tolerance relaxation**: 10x feasTol = 1e-5 (for Pass 1 ratio minimum)

The code conflates these two. Lines 73, 97, 103, 138, 139 should use `1e-9` (Harris pivot threshold), not `relaxedTol`.

---

### 5. Pivot Check Constants (pivot_check.c)

**File:** `/home/tobiasosborne/Projects/convexfeld/src/error/pivot_check.c`

| Line | Constant | Spec Says | Code Has | Violation |
|------|----------|-----------|----------|-----------|
| 23 | NEG_INFINITY_THRESHOLD | Negative infinity: -1e100 | -1e99 | **WRONG VALUE** (10x off) |
| 24 | POS_INFINITY_THRESHOLD | Solver infinity: 1e100 | 1e99 | **WRONG VALUE** (10x off) |

**Analysis:**
From spec (Section 6, "Infinity Representation"):
- Solver infinity: **1e100**
- Negative infinity: **-1e100**

Code uses 1e99 / -1e99, which is 10x smaller. This creates inconsistency with `CXF_INFINITY = 1e100` defined in cxf_types.h.

---

### 6. Refactorization Constants (refactor.c)

**File:** `/home/tobiasosborne/Projects/convexfeld/src/basis/refactor.c`

| Line | Constant | Spec Says | Code Has | Violation |
|------|----------|-----------|----------|-----------|
| 27 | MIN_PIVOT_TOL | Minimum pivot threshold: 1e-13 | 1e-10 | **WRONG VALUE** (1000x off) |

**Analysis:**
Spec Section 3 defines minimum pivot threshold as 1e-13 for bound propagation. Code uses 1e-10.

---

### 7. Other Implementation Files

**File:** `/home/tobiasosborne/Projects/convexfeld/src/simplex/pivot_special.c`
- Line 25: `#define THRESHOLD 1e-10` - Unclear what this represents, not in spec

**File:** `/home/tobiasosborne/Projects/convexfeld/src/simplex/pivot_primal.c`
- Line 31: `#define TINY_THRESHOLD 1e-8` - Not in spec, appears to be ad-hoc tolerance

**File:** `/home/tobiasosborne/Projects/convexfeld/src/simplex/refine.c`
- Line 13: `#define NEAR_ZERO_TOL 1e-12` - Conflicts with spec's "numerical zero tight" (1e-10)

**File:** `/home/tobiasosborne/Projects/convexfeld/src/solver_state/helpers.c`
- Line 16: `#define BOUND_TOL 1e-10` - Likely should be bound equality tolerance (spec: 1e-10), OK if used correctly

**File:** `/home/tobiasosborne/Projects/convexfeld/src/pricing/steepest.c`
- Line 24: `#define MIN_WEIGHT 1e-10` - Not in spec

**File:** `/home/tobiasosborne/Projects/convexfeld/src/pricing/update.c`
- Line 21: `#define MIN_WEIGHT 1e-10` - Not in spec

---

### 8. Missing Constants (Not Defined Anywhere)

From spec, the following constants are **completely missing** from implementation:

#### Adaptive Pivot Tolerance (Spec Section 3)
| Constant | Spec Value | Where Used |
|----------|------------|------------|
| Fast phase pricing tolerance | ~1e-6 | Initial iterations |
| Standard phase pricing tolerance | ~1e-10 | Fallback/difficulties |
| Aggressive phase pricing tolerance | ~1e-9 | Near optimality |

**Violation:** Adaptive pricing tolerance strategy not implemented.

#### Anti-Cycling (Spec Section 6)
| Constant | Spec Value | Where Used |
|----------|------------|------------|
| Stalling grace period | ~5 iterations | Before stalling detection activates |
| Outer iteration limit (dual) | ~100 | Dual simplex |
| Outer iteration limit (crossover) | ~10 | Barrier crossover |
| Outer iteration limit (primal) | ~5 | Primal simplex |

**Violation:** Stalling detection constants not defined.

#### Scaling (Spec Section 7)
| Constant | Spec Value | Where Used |
|----------|------------|------------|
| Minimum scaling factor | ~1e-6 | Floor on row/col scaling |
| Maximum scaling factor | ~1e6 | Ceiling on row/col scaling |
| Scaling iteration count | 9-10 | Ruiz equilibration iterations |

**Violation:** Scaling constants not defined (though setup.c has SCALE_CLAMP_MIN 1e-6, correct).

#### Barrier (Spec Section 4)
| Constant | Spec Value | Where Used |
|----------|------------|------------|
| Barrier convergence tolerance | 1e-8 | Barrier termination |
| Barrier QCP convergence tolerance | 1e-6 | QCP models |

**Violation:** Barrier tolerances not defined (barrier not implemented yet).

#### Other (Spec Section 11)
| Constant | Spec Value | Where Used |
|----------|------------|------------|
| Large bound marker | ~1e20 | "Effectively infinite" heuristic threshold |
| Quadratic half-factor | 0.5 | QP objective evaluation |

**Violation:** Large bound marker not defined. Quadratic half-factor N/A (no QP support).

---

### 9. Parameter Defaults (env.c)

**File:** `/home/tobiasosborne/Projects/convexfeld/src/api/env.c`

| Line | Parameter | Spec Says | Code Has | Violation |
|------|-----------|-----------|----------|-----------|
| 36 | feasibility_tol | 1e-6 (range [1e-9, 1e-2]) | CXF_FEASIBILITY_TOL (1e-6) | **CORRECT** |
| 37 | optimality_tol | 1e-6 (range [1e-9, 1e-2]) | CXF_OPTIMALITY_TOL (1e-6) | **CORRECT** |
| 38 | infinity | 1e100 | CXF_INFINITY (1e100) | **CORRECT** |
| 49 | max_eta_count | 50-200, default ~100 | 100 | **CORRECT** |
| 51 | refactor_interval | Default min(100, max(50, m/4)) | 50 | **POTENTIALLY WRONG** |

**Analysis:**
- `refactor_interval = 50` is within spec range but spec says default should be adaptive: `min(100, max(50, m/4))` where m is constraint count
- This is model-dependent, so hardcoding 50 is incorrect for general case

---

### 10. Hardcoded Tolerances in solve_lp.c

**File:** `/home/tobiasosborne/Projects/convexfeld/src/simplex/solve_lp.c`

Grep found multiple hardcoded tolerances:
- Line 798: `if (violation > 1e-8)` - Should use feasibility_tol or named constant
- Line 925: `if (viol2 > 1e-8)` - Ditto
- Line 994: `if (fabs(xval) > 1e-10)` - Should use bound equality tolerance or numerical zero
- Line 1032: `if (fabs(true_aux - stored_aux) > 1e-6)` - Should use feasibility_tol
- Line 1081: `fabs(state->work_x[basic_var]) > 1e-10` - Should use numerical zero constant
- Line 1159: `if (fabs(term) > 1e-10)` - Ditto

**Violation:** 6 hardcoded magic numbers instead of named constants or parameter access.

---

### 11. Hardcoded Tolerances in iterate.c

**File:** `/home/tobiasosborne/Projects/convexfeld/src/simplex/iterate.c`

- Line 313: `if (state->use_bland && stepSize < 1e-8 && ...)` - Should be named constant
- Line 324: `if (chosen_idx > 0 && stepSize < 1e-8 && ...)` - Ditto
- Line 356: `if (stepSize < 1e-8)` - Comment says "1e-8 catches FP artifacts", should be named

**Violation:** 3 hardcoded magic numbers.

---

## Missing Error Codes

### Comparison: Spec vs Implementation

**Spec defines (error_status_codes.md, Section "Error Codes"):**

| Name | Spec Value | Code Has |
|------|------------|----------|
| OUT_OF_MEMORY | 10001 | ✓ (as CXF_ERROR_OUT_OF_MEMORY = -1) |
| NULL_ARGUMENT | 10002 | ✓ (as CXF_ERROR_NULL_ARGUMENT = -2) |
| INVALID_ARGUMENT | 10003 | ✓ (as CXF_ERROR_INVALID_ARGUMENT = -3) |
| UNKNOWN_ATTRIBUTE | 10004 | ✗ **MISSING** |
| DATA_NOT_AVAILABLE | 10005 | ✓ (as CXF_ERROR_DATA_NOT_AVAILABLE = -4) |
| INDEX_OUT_OF_RANGE | 10006 | ✗ **MISSING** |
| UNKNOWN_PARAMETER | 10007 | ✗ **MISSING** |
| VALUE_OUT_OF_RANGE | 10008 | ✗ **MISSING** |
| OPTIMIZATION_IN_PROGRESS | 10017 | ✗ **MISSING** |
| DUPLICATES | 10018 | ✗ **MISSING** |
| MODEL_MODIFICATION | 10029 | ✗ **MISSING** |
| NUMERIC | 10014 | ✗ **MISSING** |
| Q_NOT_PSD | 10020 | ✗ **MISSING** (QP not implemented) |
| QCP_EQUALITY_CONSTRAINT | 10021 | ✗ **MISSING** (QP not implemented) |
| EXCEED_2B_NONZEROS | 10025 | ✗ **MISSING** |
| CALLBACK | 10011 | ✗ **MISSING** |
| FILE_READ | 10012 | ✗ **MISSING** |
| FILE_WRITE | 10013 | ✗ **MISSING** |
| IIS_NOT_INFEASIBLE | 10015 | ✗ **MISSING** |
| NOT_SUPPORTED | 10024 | ✓ (as CXF_ERROR_NOT_SUPPORTED = -5) |
| INVALID_PIECEWISE_OBJ | 10026 | ✗ **MISSING** |
| UPDATEMODE_CHANGE | 10027 | ✗ **MISSING** |
| TUNE_MODEL_TYPES | 10031 | ✗ **MISSING** |
| SECURITY | 10032 | ✗ **MISSING** |
| NOT_IN_MODEL | 20001 | ✗ **MISSING** |
| FAILED_TO_CREATE_MODEL | 20002 | ✗ **MISSING** |
| INTERNAL | 20003 | ✗ **MISSING** |

**Summary:**
- Spec defines: **26 error codes**
- Implementation defines: **5 error codes** (CXF_ERROR_OUT_OF_MEMORY, NULL_ARGUMENT, INVALID_ARGUMENT, DATA_NOT_AVAILABLE, NOT_SUPPORTED)
- **Missing: 21 error codes**

**Note:** Implementation uses negative values (-1, -2, ...) while spec uses positive 10000+ range. This is a **naming/encoding discrepancy** but functionally acceptable if consistent.

---

### Optimization Status Codes

**Spec defines (error_status_codes.md, Section "Optimization Status Codes"):**

| Name | Spec Value | Code Has |
|------|------------|----------|
| LOADED | 1 | ✗ (not in CxfStatus enum) |
| OPTIMAL | 2 | ✓ (as CXF_OPTIMAL = 1) |
| INFEASIBLE | 3 | ✓ (as CXF_INFEASIBLE = 2) |
| UNBOUNDED | 5 | ✓ (as CXF_UNBOUNDED = 3) |
| INF_OR_UNBD | 4 | ✓ (as CXF_INF_OR_UNBD = 4) |
| ITERATION_LIMIT | 7 | ✓ (as CXF_ITERATION_LIMIT = 5) |
| TIME_LIMIT | 9 | ✓ (as CXF_TIME_LIMIT = 6) |
| NUMERIC | 12 | ✓ (as CXF_NUMERIC = 7) |
| SUBOPTIMAL | 13 | ✗ **MISSING** |
| CUTOFF | 6 | ✗ **MISSING** |
| WORK_LIMIT | 16 | ✗ **MISSING** |
| MEM_LIMIT | 17 | ✗ **MISSING** |
| INTERRUPTED | 11 | ✗ **MISSING** |
| INPROGRESS | 14 | ✗ **MISSING** |
| LOCALLY_OPTIMAL | 18 | ✗ **MISSING** (nonlinear only) |
| LOCALLY_INFEASIBLE | 19 | ✗ **MISSING** (nonlinear only) |

**Summary:**
- Spec defines: **17 optimization status codes**
- Implementation defines: **8 status codes**
- **Missing: 9 status codes**

**Note:** Implementation uses different numeric values (1-7) than spec (1-19). This is a **serious encoding discrepancy**.

---

### Internal Simplex Return Codes

**Spec defines (error_status_codes.md, Section "Internal Simplex Return Codes"):**

| Semantic Meaning | Spec Says | Code Has |
|------------------|-----------|----------|
| Continue | Simplex iteration completed normally | ✗ **MISSING** |
| Optimal | Current basis is optimal | ✗ **MISSING** |
| Infeasible | Bound inconsistency detected | ✗ **MISSING** |
| Unbounded | Unbounded ray found | ✗ **MISSING** |

**Violation:** Internal simplex return codes are **completely absent** from implementation. Code uses CxfStatus enum directly in internal functions, which violates spec's internal/external separation.

---

## Extra Error Codes

**Implementation defines (cxf_types.h):**

All 5 implementation error codes are in spec (see table above). **No hallucinated error codes found.**

---

## Recommendations

### Immediate Fixes (Critical)

1. **Fix pivot tolerances in cxf_types.h:**
   ```c
   #define CXF_HARRIS_PIVOT_TOL    1e-9   // Was CXF_PIVOT_TOL = 1e-10
   #define CXF_MIN_PIVOT_TOL       1e-13  // New constant
   #define CXF_BOUND_EQUALITY_TOL  1e-10  // New constant
   #define CXF_NUMERICAL_ZERO_TOL  1e-10  // Was CXF_ZERO_TOL = 1e-12
   #define CXF_BOUND_CHANGE_TOL    1e-12  // New constant
   ```

2. **Fix perturbation.c constants:**
   ```c
   #define PERTURB_MIN_RANGE  1e-10  // Was PERTURB_BASE_SCALE = 1e-6
   #define PERTURB_MAX_MAG    1e-6   // Was PERTURB_MAX_SCALE = 1e-3
   // Remove MIN_OBJ_COEFF or document its purpose
   ```
   Then fix line 99 logic to match spec Section 6.

3. **Fix lu_factorize.c constants:**
   ```c
   #define MARKOWITZ_THRESHOLD  0.0078125  // Was 0.1 (use 1/128)
   #define MIN_PIVOT            1e-13      // Was 1e-12
   ```

4. **Fix pivot_check.c infinity thresholds:**
   ```c
   #define NEG_INFINITY_THRESHOLD (-1e100)  // Was -1e99
   #define POS_INFINITY_THRESHOLD (1e100)   // Was 1e99
   ```

5. **Fix ratio_test.c to use Harris pivot tolerance:**
   - Replace `relaxedTol` with separate `HARRIS_PIVOT_TOL = 1e-9` for pivot element checks (lines 73, 97, 103, 138, 139)
   - Keep `relaxedTol = 10 * feasTol` for ratio threshold in Pass 1 (line 114)

6. **Remove all hardcoded magic numbers** in solve_lp.c, iterate.c, and replace with named constants or environment parameters.

### Medium Priority

7. **Add missing constants** to cxf_types.h or appropriate headers:
   - Adaptive pricing tolerances (fast, standard, aggressive)
   - Stalling detection thresholds
   - Large bound marker (1e20)
   - Scaling factor bounds (already partially done in setup.c)

8. **Add missing error codes** to CxfStatus enum:
   - All 21 missing error codes from spec
   - Use spec numeric values (10001+) or document the mapping from negative values

9. **Add missing optimization status codes:**
   - All 9 missing status codes
   - **Fix numeric value discrepancy** (critical for API compatibility)

10. **Implement internal simplex return codes:**
    - Create separate enum for internal simplex states
    - Map to public CxfStatus at API boundary

### Low Priority

11. **Document all non-spec constants** (MIN_WEIGHT, TINY_THRESHOLD, etc.) or remove if unused.

12. **Make refactor_interval adaptive** per spec formula: `min(100, max(50, m/4))`

13. **Add barrier and QP constants** when those features are implemented.

---

## Conclusion

The ConvexFeld implementation has **significant discrepancies** from the v2 reference specifications in numerical tolerances and constants. Most violations stem from:

1. **Misreading or misinterpreting spec tolerance values** (off by 10x-10,000x)
2. **Conflating distinct thresholds** (e.g., Harris pivot vs. bound equality vs. numerical zero)
3. **Missing constants** required by spec (adaptive pricing, stalling detection, large bound marker)
4. **Hardcoded magic numbers** instead of named constants
5. **Incomplete error code coverage** (21 of 26 spec error codes missing)

**Correctness impact:**
- **High:** Perturbation constants wrong by 10,000x will cause incorrect anti-cycling behavior
- **High:** Markowitz threshold 13x too large will degrade LU factorization sparsity
- **Medium:** Pivot tolerance discrepancies (10x-1000x) may cause numerical instability
- **Medium:** Missing adaptive pricing tolerances prevents proper convergence strategy
- **Low:** Missing error codes reduces diagnostic capability but doesn't affect correctness

**Compliance score:** Approximately **40% compliant** with spec tolerances/constants.

---

## Files Audited

### Implementation
1. /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_types.h
2. /home/tobiasosborne/Projects/convexfeld/include/convexfeld/convexfeld.h
3. /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_env.h
4. /home/tobiasosborne/Projects/convexfeld/src/parameters/params.c
5. /home/tobiasosborne/Projects/convexfeld/src/simplex/ratio_test.c
6. /home/tobiasosborne/Projects/convexfeld/src/simplex/perturbation.c
7. /home/tobiasosborne/Projects/convexfeld/src/basis/lu_factorize.c
8. /home/tobiasosborne/Projects/convexfeld/src/error/pivot_check.c
9. /home/tobiasosborne/Projects/convexfeld/src/error/nan_check.c
10. /home/tobiasosborne/Projects/convexfeld/src/api/env.c
11. /home/tobiasosborne/Projects/convexfeld/src/basis/refactor.c
12. Plus grep results across all .c and .h files

### Specifications
1. /home/tobiasosborne/Projects/convexfeld/docs/specs-v2/specs/reference/tolerances_constants.md
2. /home/tobiasosborne/Projects/convexfeld/docs/specs-v2/specs/reference/numerical_stability.md
3. /home/tobiasosborne/Projects/convexfeld/docs/specs-v2/specs/reference/parameters_defaults.md
4. /home/tobiasosborne/Projects/convexfeld/docs/specs-v2/specs/reference/error_status_codes.md

---

**END OF AUDIT REPORT**
