# cxf_log10_wrapper

**Module:** Utilities
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Provides a safe wrapper for the base-10 logarithm function with explicit handling of special cases (NaN, negative values, zero, infinity). Used throughout Convexfeld for computing logarithmic scales, coefficient ranges, and numerical diagnostics. Ensures consistent behavior across platforms and provides predictable results for edge cases.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| value | double | Input value | Any double | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | double | Base-10 logarithm of input, or special value for edge cases |

### 2.3 Side Effects

None (pure function)

## 3. Contract

### 3.1 Preconditions

- [ ] None (handles all input values including special cases)

### 3.2 Postconditions

- [ ] For positive finite values: returns log₁₀(value)
- [ ] For zero: returns -Infinity
- [ ] For negative values: returns NaN
- [ ] For +Infinity: returns +Infinity
- [ ] For NaN: returns NaN (propagation)

### 3.3 Invariants

- [ ] Function is deterministic (same input always produces same output)
- [ ] No global state modified

## 4. Algorithm

### 4.1 Overview

The function wraps the standard C library log10() function with defensive checks for edge cases. It explicitly tests for special floating-point values before delegating to the standard library, ensuring portable and predictable behavior across all platforms.

### 4.2 Detailed Steps

1. Check if input is NaN using isnan()
   - If true, return the NaN value (propagation)
2. Check if input is negative (value < 0.0)
   - If true, return NaN (logarithm undefined for negatives)
3. Check if input is zero (value == 0.0)
   - If true, return -Infinity (mathematical limit)
4. Check if input is positive infinity using isinf()
   - If true, return +Infinity (mathematical limit)
5. For all other cases (positive finite values), call standard log10(value)

### 4.3 Mathematical Foundation

The base-10 logarithm is defined as:
```
log₁₀(x) = y  ⟺  10^y = x
```

Special values follow mathematical limits:
- lim(x→0⁺) log₁₀(x) = -∞
- lim(x→+∞) log₁₀(x) = +∞
- log₁₀(x) undefined for x ≤ 0 (returns NaN)

Properties:
- log₁₀(1) = 0
- log₁₀(10) = 1
- log₁₀(100) = 2
- log₁₀(ab) = log₁₀(a) + log₁₀(b)
- log₁₀(a/b) = log₁₀(a) - log₁₀(b)

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) - 4 comparisons plus log10 call
- **Average case:** O(1) - ~30-40 CPU cycles
- **Worst case:** O(1) - same as average

Where:
- Special case checks: ~4 comparisons (~4 cycles)
- Standard log10: ~20-30 cycles (hardware/library dependent)

### 5.2 Space Complexity

- **Auxiliary space:** O(1) - no allocations
- **Total space:** O(1) - single return value

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| Negative input | NaN | Logarithm undefined for negative values |
| NaN input | NaN | Special value propagated |
| Zero input | -Infinity | Mathematical limit |

### 6.2 Error Behavior

The function never fails or throws exceptions. All invalid inputs return well-defined special values (NaN or -Infinity) that signal the nature of the issue to the caller.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Zero | 0.0 | Returns -Infinity |
| Negative | -1.0 | Returns NaN |
| NaN | NaN | Returns NaN (propagation) |
| Positive infinity | +Inf | Returns +Infinity |
| One | 1.0 | Returns 0.0 |
| Ten | 10.0 | Returns 1.0 |
| Very small | 1e-300 | Returns ~-300.0 |
| Very large | 1e300 | Returns ~300.0 |

## 8. Thread Safety

**Thread-safe:** Yes

The function is fully thread-safe because:
- No shared state accessed
- No mutable global variables
- Pure mathematical computation
- Standard library log10() is thread-safe

**Synchronization required:** None

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| isnan | C99 math.h | Check for NaN |
| isinf | C99 math.h | Check for infinity |
| log10 | C99 math.h | Compute base-10 logarithm |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_coefficient_stats | Statistics | Coefficient range analysis |
| Numerical analysis routines | Various | Order-of-magnitude computations |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_log_wrapper | Natural logarithm (ln) variant |
| cxf_floor_ceil_wrapper | Another math function wrapper |

## 11. Design Notes

### 11.1 Design Rationale

The explicit edge case handling ensures:
1. Consistent behavior across platforms (MSVC, GCC, Clang)
2. Predictable results for domain errors
3. No errno side effects
4. Clear documentation of special value semantics

Pre-C99 implementations of log10() varied in their handling of special cases. This wrapper abstracts those differences.

### 11.2 Performance Considerations

The overhead is minimal (~5-10 nanoseconds) compared to the log10 computation itself (~20-30 ns). The checks are well-predicted by branch predictors since most calls use normal positive values.

### 11.3 Future Considerations

For performance-critical code paths, a version without checks could be provided if the caller guarantees positive finite inputs.

## 12. References

- ISO C99 Standard: Section 7.12.6 (Logarithmic functions)
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
