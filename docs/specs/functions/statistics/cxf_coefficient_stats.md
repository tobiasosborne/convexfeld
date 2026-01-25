# cxf_coefficient_stats

**Module:** Statistics
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Computes and logs coefficient statistics for all model components, issuing warnings about potential numerical issues that may cause solver instability. Analyzes the numerical properties of matrix coefficients, objective, bounds, RHS, quadratic terms, and piecewise-linear constraints to detect problematic value ranges. Suggests parameter adjustments when numerical issues are detected.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| model | CxfModel* | Model structure containing problem data | Valid pointer | Yes |
| verbose | int | Logging control flag | 0 (silent) or 1 (log) | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | Error code (0 on success) |
| logging | side effect | Statistics and warnings written to log if verbose=1 |

### 2.3 Side Effects

Writes coefficient ranges and numerical warnings to environment log. Computes PWL objective statistics by scanning internal data arrays.

## 3. Contract

### 3.1 Preconditions

- [ ] model pointer must be valid
- [ ] model->matrix must be initialized
- [ ] model->env must be valid for logging
- [ ] model->status must be 0 (not yet solved)

### 3.2 Postconditions

- [ ] If verbose=1, coefficient ranges are logged
- [ ] If verbose=1 and issues detected, warnings are logged
- [ ] If warnings issued and NumericFocus=0, suggestion to set NumericFocus is logged
- [ ] Function returns 0 on success

### 3.3 Invariants

- [ ] Model structure is not modified
- [ ] Coefficient data is not altered

## 4. Algorithm

### 4.1 Overview

The function operates in two phases: computation and reporting. First, it calls a helper function to compute min/max ranges for all standard coefficient types (matrix, objective, bounds, RHS, quadratic terms). Then it manually scans piecewise-linear objective data to compute PWL breakpoint ranges, focusing on "middle points" (interior breakpoints excluding endpoints). If general constraints exist, it calls another helper to compute indicator constraint, PWL constraint, and MAX/MIN constraint ranges.

In the reporting phase (if verbose=1), it logs all ranges in scientific notation. Then it applies numerical threshold checks: coefficient ranges spanning more than 13 orders of magnitude, individual coefficients exceeding 1e13, and PWL breakpoint values exceeding 1e10. For each detected issue, it logs a specific warning message. Finally, if any warnings were issued and the NumericFocus parameter is not already set, it suggests enabling NumericFocus to improve numerical stability.

### 4.2 Detailed Steps

1. Check model status - exit early if model already solved (status != 0)
2. Call cxf_compute_coef_stats to get ranges for: matrix, objective, quadratic objective, RHS, bounds, quadratic matrix, quadratic linear matrix, quadratic RHS
3. Initialize PWL objective ranges to infinity
4. If PWL objective terms exist:
   - For each variable with PWL objective:
     - Skip variables with < 3 breakpoints
     - Extract middle points (excluding first and last breakpoint)
     - Track min/max X coordinates across middle points
     - Track min/max Y (objective) values across middle points
5. If general constraints exist, call cxf_gencon_stats to get ranges for: PWL constraint X/Y, indicator constraint coefficients/RHS, MAX/MIN constraint constants
6. If verbose=0, return 0 (skip logging)
7. Log all coefficient ranges in scientific notation
8. Check matrix coefficient range: if log10(max/min) >= 13, warn about large range; else if max > 1e13, warn about large absolute coefficients
9. Check objective coefficients: if max > 1e13, warn
10. Check RHS values: if max > 1e13, warn
11. Check bounds: if max > 1e13, warn
12. If quadratic matrix exists: check range and absolute values, warn if needed
13. If quadratic linear matrix exists: check range and absolute values, warn if needed
14. If quadratic RHS exists: check if max > 1e13, warn
15. If quadratic objective exists: check range and absolute values, warn if needed
16. If PWL objective exists: check if X or Y middle point values exceed 1e10, warn
17. If general constraints exist: check PWL constraint X/Y, indicator RHS, indicator coefficient range, MAX/MIN constants; warn as needed
18. If any warnings issued and NumericFocus parameter is 0, suggest setting NumericFocus

### 4.3 Pseudocode (if needed)

```
IF model.status != 0:
  RETURN 0  // Already solved

COMPUTE ranges for matrix, obj, qobj, rhs, bounds, qmatrix, qlmatrix, qrhs

pwlObjXMin ← ∞, pwlObjXMax ← 0
pwlObjYMin ← ∞, pwlObjYMax ← 0

FOR each variable with PWL objective:
  IF numBreakpoints < 3:
    SKIP
  FOR i in [startIndex+1 .. endIndex-1]:  // Middle points only
    pwlObjXMin ← min(pwlObjXMin, xValues[i])
    pwlObjXMax ← max(pwlObjXMax, xValues[i])
    pwlObjYMin ← min(pwlObjYMin, yValues[i])
    pwlObjYMax ← max(pwlObjYMax, yValues[i])

IF general constraints exist:
  COMPUTE ranges for PWL con, indicator con, MAX/MIN con

IF verbose == 0:
  RETURN 0

issueWarning ← false

LOG "Coefficient statistics:"
LOG matrix, qmatrix, qlmatrix, objective, qobjective, bounds, rhs, qrhs ranges
LOG pwlObj ranges if valid
LOG general constraint ranges if valid

// Threshold checks
IF log10(matrixMax/matrixMin) >= 13:
  WARN "large matrix coefficient range"
  issueWarning ← true
ELSE IF matrixMax > 1e13:
  WARN "large matrix coefficients"
  issueWarning ← true

IF objMax > 1e13:
  WARN "large objective coefficients"
  issueWarning ← true

// Similar checks for rhs, bounds, qmatrix, qlmatrix, qrhs, qobj, pwl, gencon...

IF issueWarning AND env.numericFocus == 0:
  SUGGEST "Consider reformulating model or setting NumericFocus parameter"

RETURN 0
```

### 4.4 Mathematical Foundation (if applicable)

Numerical conditioning is assessed using the base-10 logarithm of the ratio of maximum to minimum coefficient magnitudes. A range of 13 orders of magnitude (log10(max/min) >= 13) indicates potential ill-conditioning in linear algebra operations. This threshold is based on IEEE 754 double precision representing approximately 15-16 decimal digits, leaving a safety margin of 2-3 digits for roundoff error accumulation.

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) - Model already solved (early exit)
- **Average case:** O(n + m + nnz + q) - Scan all coefficients
- **Worst case:** O(n + m + nnz + q)

Where:
- n = number of variables
- m = number of constraints
- nnz = number of nonzero matrix coefficients
- q = number of quadratic terms

### 5.2 Space Complexity

- **Auxiliary space:** O(1) - Fixed-size local variables for ranges
- **Total space:** O(1)

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| Helper function failure | Propagated | Error from cxf_compute_coef_stats or cxf_gencon_stats |
| Memory allocation failure | 1001 | Out of memory error (from helper) |

### 6.2 Error Behavior

Function returns immediately on error from helper functions. Does not perform cleanup as no resources are allocated. Model state remains unchanged.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Model already solved | status != 0 | Return 0 immediately (no processing) |
| Verbose = 0 | verbose flag off | Compute stats but do not log |
| No quadratic terms | All Q counts = 0 | Skip Q-related logging and warnings |
| No PWL objectives | pwlObjCount = 0 | Skip PWL objective logging |
| No general constraints | genConstrCount = 0 | Skip general constraint logging |
| Well-conditioned model | All ranges < thresholds | Log statistics, no warnings |
| NumericFocus already set | env.numericFocus > 0 | Issue warnings but skip suggestion |

## 8. Thread Safety

**Thread-safe:** No

**Synchronization required:** Model must not be modified during execution. Logging system should be thread-safe externally.

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_compute_coef_stats | Statistics | Compute ranges for standard coefficient types |
| cxf_gencon_stats | Statistics | Compute ranges for general constraints |
| cxf_log_printf | Logging | Format and write log messages |
| log10 | Math library | Compute logarithm for range calculations |
| snprintf | C library | Format warning message strings |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_optimize | API | Called during pre-solve phase to check numerical properties |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_presolve_stats | Logs model structure statistics (constraint types) |
| cxf_compute_coef_stats | Helper that computes coefficient ranges |
| cxf_gencon_stats | Helper that computes general constraint ranges |

## 11. Design Notes

### 11.1 Design Rationale

Numerical issues are a common source of solver failures and incorrect results. By detecting problematic coefficient ranges before optimization begins, users can reformulate their models or adjust parameters to improve stability. Separating PWL middle point values from endpoints reflects the fact that endpoint values are typically bounded by variable bounds, while interior breakpoints indicate the structure of the approximation. The 1e10 threshold for PWL is lower than the 1e13 threshold for general coefficients because piecewise-linear approximations lose accuracy faster with large values.

### 11.2 Performance Considerations

The function scans all model coefficients once, which is necessary for accurate diagnostics. The overhead is O(model size) but acceptable as it runs only once per optimization. PWL objective scanning iterates through middle points only (not all breakpoints) to focus on the approximation quality metric.

### 11.3 Future Considerations

Could be extended to check for extremely small coefficients (underflow risk), detect coefficients with very different scales in the same constraint (row scaling issues), or recommend specific reformulation techniques based on detected patterns.

## 12. References

- Gill, Murray, Saunders, Wright (2005). "Numerical Linear Algebra and Optimization"
- Convexfeld Optimizer Reference Manual: NumericFocus parameter
- IEEE 754 Standard for Floating-Point Arithmetic

## 13. Validation Checklist

Before finalizing this spec, verify:

- [x] No code copied from implementation
- [x] Algorithm description is implementation-agnostic
- [x] All parameters documented
- [x] All error conditions listed
- [x] Complexity analysis complete
- [x] Edge cases identified
- [x] A competent developer could implement from this spec alone

---

*Reviewed by: Pending*

---
*Convexfeld LP Solver Specification*
*Based on published optimization literature*
