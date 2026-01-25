# cxf_check_nan_or_inf

**Module:** Validation
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Detects whether a double-precision floating-point value is either NaN (Not-a-Number) or Infinity (positive or negative). This function serves as a fast pre-filter for validating numeric inputs, allowing subsequent functions to distinguish between NaN and Infinity when both are invalid.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| value | double | Floating-point value to check | Any double value | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | 1 if value is NaN or Infinity, 0 otherwise |

### 2.3 Side Effects

None (pure function).

## 3. Contract

### 3.1 Preconditions

- None (function accepts any double value)

### 3.2 Postconditions

- Return value is 1 if and only if input is NaN or Infinity
- Return value is 0 for all finite values (including zero and denormals)
- Input value is not modified

### 3.3 Invariants

- Function has no side effects
- Result depends only on input value

## 4. Algorithm

### 4.1 Overview

Uses IEEE 754 double-precision bit pattern analysis to detect non-finite values. According to IEEE 754 standard, both NaN and Infinity have exponent fields with all 1s (0x7FF). This function only needs to check the exponent, making it simpler and faster than checking for NaN alone.

### 4.2 Detailed Steps

1. Interpret the double value as a 64-bit unsigned integer (bit-level representation)
2. Extract the 11-bit exponent field (bits 52-62)
3. Check if exponent equals 0x7FF (all 1s)
4. Return 1 if exponent is 0x7FF (non-finite), otherwise return 0

### 4.3 Pseudocode

```
FUNCTION is_nan_or_infinity(value):
    bits ← reinterpret value as 64-bit unsigned integer
    exponent ← (bits >> 52) & 0x7FF

    IF exponent = 0x7FF THEN
        RETURN 1
    ELSE
        RETURN 0
    END IF
END FUNCTION
```

### 4.4 Mathematical Foundation

IEEE 754 double-precision format:
- Exponent = 0x7FF indicates non-finite value
  - If mantissa = 0: Infinity (sign bit determines ±)
  - If mantissa ≠ 0: NaN

This function treats both cases identically without examining the mantissa.

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1)
- **Average case:** O(1)
- **Worst case:** O(1)

Single exponent extraction and comparison.

### 5.2 Space Complexity

- **Auxiliary space:** O(1)
- **Total space:** O(1)

## 6. Error Handling

### 6.1 Error Conditions

None - function always succeeds.

### 6.2 Error Behavior

Function cannot fail. Returns well-defined result for all inputs.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Zero | 0.0 | Return 0 |
| Normal number | 1.23 | Return 0 |
| Large finite | 1.7e308 (near DBL_MAX) | Return 0 |
| Denormal | 2.2e-308 (very small) | Return 0 |
| Positive infinity | +INF | Return 1 |
| Negative infinity | -INF | Return 1 |
| Quiet NaN | qNaN | Return 1 |
| Signaling NaN | sNaN | Return 1 |
| Convexfeld infinity constant | 1e100 | Return 0 (finite) |

## 8. Thread Safety

**Thread-safe:** Yes

Function is pure with no shared state. Safe to call from multiple threads.

**Synchronization required:** None

## 9. Dependencies

### 9.1 Functions Called

None (bit manipulation only).

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_chgcoeffs | Model | First-stage validation of coefficients |
| cxf_addqconstr | Model | Validate quadratic coefficients |
| Validation loops | Various | Pre-filter before detailed NaN check |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_check_nan | Subset check (detects only NaN, not Infinity) |
| cxf_validate_array | May use this as pre-filter |

## 11. Design Notes

### 11.1 Design Rationale

Designed as a fast pre-filter in two-stage validation:
1. Check for non-finite (this function) - O(1) exponent check
2. If non-finite, distinguish NaN from Infinity (cxf_check_nan) - O(1) mantissa check

This is more efficient than always checking both exponent and mantissa, especially when most values are finite (common case).

### 11.2 Performance Considerations

- Faster than cxf_check_nan (no mantissa examination)
- Typically used in pattern: `if (check_nan_or_inf(x)) then check_nan(x)`
- Expected to be inlined by compiler
- No branching beyond the final comparison

### 11.3 Future Considerations

None - function is optimal for its purpose.

## 12. References

- IEEE 754-2008: IEEE Standard for Floating-Point Arithmetic
- isfinite() standard library function (C99) has equivalent semantics

## 13. Validation Checklist

- [x] No code copied from implementation
- [x] Algorithm description is implementation-agnostic
- [x] All parameters documented
- [x] All error conditions listed
- [x] Complexity analysis complete
- [x] Edge cases identified
- [x] A competent developer could implement from this spec alone

---

---
*Convexfeld LP Solver Specification*
*Based on published optimization literature*
