# cxf_floor_ceil_wrapper

**Module:** Utilities
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Provides a safe wrapper for the floor rounding function with explicit handling of special cases (NaN, infinity). Rounds floating-point values down to the nearest integer, ensuring consistent behavior across platforms. Used for display formatting and numerical operations where integer rounding is required.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| value | double | Input value to round | Any double | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | double | floor(value) for finite values, value itself for NaN/Inf |

### 2.3 Side Effects

None (pure function)

## 3. Contract

### 3.1 Preconditions

- [ ] None (handles all input values including special cases)

### 3.2 Postconditions

- [ ] For finite values: returns largest integer ≤ value (as double)
- [ ] For NaN: returns NaN (propagation)
- [ ] For ±Infinity: returns ±Infinity (propagation)

### 3.3 Invariants

- [ ] Function is idempotent: floor(floor(x)) = floor(x)
- [ ] Result is never strictly greater than input
- [ ] No global state modified

## 4. Algorithm

### 4.1 Overview

The function wraps the standard C library floor() function with defensive checks for special floating-point values. It propagates NaN and infinity values unchanged, and rounds all finite values down to the nearest integer.

### 4.2 Detailed Steps

1. Check if input is NaN using isnan()
   - If true, return the NaN value (propagation)
2. Check if input is infinity using isinf()
   - If true, return the infinity value (propagation)
3. For all other cases (finite values), call standard floor(value)

### 4.3 Mathematical Foundation

The floor function is defined as:
```
⌊x⌋ = max{n ∈ ℤ : n ≤ x}
```

Properties:
- ⌊3.7⌋ = 3
- ⌊-3.7⌋ = -4 (rounds towards negative infinity)
- ⌊3.0⌋ = 3 (already integral)
- For all x: x - 1 < ⌊x⌋ ≤ x

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) - 2 comparisons plus floor call
- **Average case:** O(1) - ~15-20 CPU cycles
- **Worst case:** O(1) - same as average

Where:
- Special case checks: 2 comparisons (~2 cycles)
- Standard floor: ~5-10 cycles (hardware/library dependent)

### 5.2 Space Complexity

- **Auxiliary space:** O(1) - no allocations
- **Total space:** O(1) - single return value

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| NaN input | N/A | Returns NaN (not an error, special value propagation) |
| Infinity input | N/A | Returns infinity (mathematical convention) |

### 6.2 Error Behavior

The function never fails. All inputs produce well-defined outputs following standard mathematical conventions.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Already integer | 3.0 | Returns 3.0 (no change) |
| Positive fractional | 3.7 | Returns 3.0 |
| Negative fractional | -3.7 | Returns -4.0 |
| Zero | 0.0 | Returns 0.0 |
| NaN | NaN | Returns NaN |
| Positive infinity | +Inf | Returns +Infinity |
| Negative infinity | -Inf | Returns -Infinity |
| Very large | 1e100 | Returns 1e100 (unchanged) |

## 8. Thread Safety

**Thread-safe:** Yes

The function is fully thread-safe because:
- No shared state
- No mutable globals
- Pure computation
- Standard library floor() is thread-safe

**Synchronization required:** None

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| isnan | C99 math.h | Check for NaN |
| isinf | C99 math.h | Check for infinity |
| floor | C99 math.h | Round down to integer |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_simplex_iterate | Simplex | Display formatting for iteration statistics |
| Display/logging routines | Various | Formatting numeric output |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_ceil_wrapper | Rounds up instead of down |
| cxf_round_wrapper | Rounds to nearest integer |
| cxf_trunc_wrapper | Rounds towards zero |
| cxf_log10_wrapper | Another math wrapper |

## 11. Design Notes

### 11.1 Design Rationale

The explicit special case handling ensures consistent behavior across platforms, particularly for older compilers where floor() behavior on NaN/Inf may vary.

### 11.2 Performance Considerations

Overhead is negligible (~2-3 cycles for checks). Branch prediction typically handles the common case (finite values) efficiently.

### 11.3 Future Considerations

- Could provide variants for ceil, round, trunc
- Could combine into single function with mode parameter
- Could optimize out checks if caller guarantees finite inputs

## 12. References

- ISO C99 Standard: Section 7.12.9 (Rounding and remainder functions)
- IEEE 754 Floating-Point Arithmetic Standard

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
