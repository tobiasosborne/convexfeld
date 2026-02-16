# cxf_validate_array

**Module:** Validation
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Validates that a double-precision array does not contain NaN values. This function performs bulk validation of user-provided numeric arrays (such as objective coefficients, bounds, or other model data) to ensure data quality before copying values into the model structure.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| env | CxfEnv* | Environment pointer (for context) | Any pointer | Yes |
| count | int | Number of elements in array | Any integer | Yes |
| array | const double* | Array of values to validate | Any pointer including NULL | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | 0 if valid, error code if array contains NaN |

### 2.3 Side Effects

None (read-only validation function).

## 3. Contract

### 3.1 Preconditions

- If array is not NULL, it must point to valid memory of at least `count` elements
- `count` should reasonably represent the array size (negative/zero are handled gracefully)

### 3.2 Postconditions

- If return is 0, array contains no NaN values (or is NULL)
- If return is nonzero, array contains at least one NaN value
- Array contents are not modified
- Environment state is not modified

### 3.3 Invariants

- Input array is not modified
- Function has no global side effects

## 4. Algorithm

### 4.1 Overview

Performs a linear scan of the array, checking each element for NaN using IEEE 754 bit pattern analysis. Infinity values are explicitly allowed (important for bound arrays where ±∞ are valid). Returns immediately upon finding the first NaN.

### 4.2 Detailed Steps

1. Check if array pointer is NULL. If NULL, return success (NULL indicates defaults should be used).
2. Check if count is ≤ 0. If so, return success (empty or invalid count treated as valid).
3. For each element i from 0 to count-1:
   a. Check if array[i] is NaN (using IEEE 754 bit pattern check)
   b. If NaN found, return error code for invalid argument
4. If loop completes without finding NaN, return success.

### 4.3 Pseudocode

```
FUNCTION validate_array(env, count, array):
    IF array = NULL THEN
        RETURN SUCCESS
    END IF

    IF count ≤ 0 THEN
        RETURN SUCCESS
    END IF

    FOR i FROM 0 TO count-1 DO
        IF is_nan(array[i]) THEN
            RETURN ERROR_INVALID_ARGUMENT
        END IF
    END FOR

    RETURN SUCCESS
END FUNCTION
```

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) - NULL array or count ≤ 0
- **Average case:** O(n) - NaN found midway, where n = count
- **Worst case:** O(n) - no NaN found, full scan required

Where n = count (number of elements).

### 5.2 Space Complexity

- **Auxiliary space:** O(1)
- **Total space:** O(1)

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| Array contains NaN | 1003 (CXF_ERR_INVALID_ARGUMENT) | Invalid numeric input |

### 6.2 Error Behavior

Returns immediately upon finding first NaN. Does not report which element index contains NaN (caller handles error messaging).

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| NULL array | array = NULL | Return SUCCESS |
| Zero count | count = 0 | Return SUCCESS |
| Negative count | count = -5 | Return SUCCESS (defensive) |
| All finite | [1.0, 2.5, 3.7] | Return SUCCESS |
| Contains +∞ | [1.0, +INF, 2.0] | Return SUCCESS (infinity allowed) |
| Contains -∞ | [-INF, 0.0] | Return SUCCESS (infinity allowed) |
| Contains NaN | [1.0, NAN, 2.0] | Return ERROR at index 1 |
| All NaN | [NAN, NAN, NAN] | Return ERROR at index 0 |
| Single element NaN | [NAN] | Return ERROR |

## 8. Thread Safety

**Thread-safe:** Yes

Function only reads from input array. Safe for concurrent calls with different arrays or same array (read-only).

**Synchronization required:** None

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_check_nan | Validation | Check individual element for NaN |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_newmodel | Model | Validate obj, lb, ub arrays |
| cxf_addvars | Model | Validate arrays when adding variables |
| cxf_chgcoeffs | Model | Validate coefficient changes |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_check_nan | Per-element check (called in loop) |
| cxf_validate_vartypes | Validates variable type array |

## 11. Design Notes

### 11.1 Design Rationale

**Why allow Infinity:** Bound arrays (lb/ub) legitimately use ±∞ to represent unbounded variables. This function is used for both coefficient arrays (where infinity is unusual) and bound arrays (where infinity is common), so it must accept infinity.

**Why check only NaN:** NaN is always invalid input and indicates corrupted or uninitialized data. Infinity, while potentially indicating user error, has well-defined semantics in optimization.

**Why accept NULL:** NULL array indicates that default values should be used (e.g., all zeros for objective, all ±∞ for bounds). This is a valid API pattern.

### 11.2 Performance Considerations

- O(n) scan cannot be avoided without SIMD
- Early exit on first NaN minimizes cost for invalid input
- Function is not inlined (called with varying array sizes)
- Typical overhead: ~5-10 nanoseconds per element

### 11.3 Future Considerations

Could be optimized with SIMD to check multiple doubles simultaneously, but current implementation is adequate given that validation is not a bottleneck (dwarfed by solve time).

## 12. References

- IEEE 754 standard for floating-point special values
- Convexfeld API conventions for array handling

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
