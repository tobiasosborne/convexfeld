# cxf_check_nan

**Module:** Validation
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Detects whether a double-precision floating-point value is NaN (Not-a-Number). This function is used to validate user-provided numeric inputs before they are used in optimization models, ensuring data quality and preventing undefined behavior.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| value | double | Floating-point value to check | Any double value | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | 1 if value is NaN, 0 otherwise |

### 2.3 Side Effects

None (pure function).

## 3. Contract

### 3.1 Preconditions

- None (function accepts any double value)

### 3.2 Postconditions

- Return value is 1 if and only if input is NaN
- Return value is 0 for all finite values and infinities
- Input value is not modified

### 3.3 Invariants

- Function has no side effects
- Result depends only on input value

## 4. Algorithm

### 4.1 Overview

Uses IEEE 754 double-precision bit pattern analysis to detect NaN values. According to IEEE 754 standard, a double is NaN if and only if its exponent field contains all 1s (0x7FF) AND its mantissa field is non-zero.

### 4.2 Detailed Steps

1. Interpret the double value as a 64-bit unsigned integer (bit-level representation)
2. Extract the 11-bit exponent field (bits 52-62)
3. Extract the 52-bit mantissa field (bits 0-51)
4. Check if exponent equals 0x7FF AND mantissa is non-zero
5. Return 1 if both conditions met (NaN), otherwise return 0

### 4.3 Pseudocode

```
FUNCTION is_nan(value):
    bits ← reinterpret value as 64-bit unsigned integer
    exponent ← (bits >> 52) & 0x7FF
    mantissa ← bits & 0x000FFFFFFFFFFFFF

    IF exponent = 0x7FF AND mantissa ≠ 0 THEN
        RETURN 1
    ELSE
        RETURN 0
    END IF
END FUNCTION
```

### 4.4 Mathematical Foundation

IEEE 754 double-precision format (64 bits):
- Bit 63: Sign
- Bits 62-52: Exponent (11 bits, biased by 1023)
- Bits 51-0: Mantissa (52 bits, implicit leading 1)

Special values with exponent = 0x7FF:
- Infinity: mantissa = 0
- NaN: mantissa ≠ 0

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1)
- **Average case:** O(1)
- **Worst case:** O(1)

All operations are bit-level manipulations (shift, mask, compare).

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
| Positive zero | +0.0 | Return 0 |
| Negative zero | -0.0 | Return 0 |
| Normal number | 3.14159 | Return 0 |
| Large finite | 1e100 | Return 0 |
| Positive infinity | +INF | Return 0 |
| Negative infinity | -INF | Return 0 |
| Quiet NaN | qNaN | Return 1 |
| Signaling NaN | sNaN | Return 1 |
| Negative NaN | -NaN | Return 1 |

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
| cxf_validate_array | Validation | Array validation |
| cxf_chgcoeffs | Model | Coefficient validation |
| cxf_addqconstr | Model | Quadratic coefficient validation |
| cxf_check_nan_or_inf | Validation | As refinement check |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_check_nan_or_inf | Superset check (detects NaN OR Infinity) |
| cxf_validate_array | Calls this function in a loop |

## 11. Design Notes

### 11.1 Design Rationale

Bit manipulation approach chosen over floating-point comparison (value != value) for:
- Portability across compilers and optimization levels
- Consistency (not affected by -ffast-math flags)
- Predictable performance

### 11.2 Performance Considerations

- Extremely fast: 2-3 CPU cycles typical
- Should be inlined by compiler
- No branching in critical path
- Called in tight loops, so must be minimal overhead

### 11.3 Future Considerations

None - function is complete and optimal for its purpose.

## 12. References

- IEEE 754-2008: IEEE Standard for Floating-Point Arithmetic
- Bit patterns for special values

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
