# cxf_vector_norm

**Module:** Matrix Operations
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Computes various vector norms (L1, L2, L-infinity) for numerical vectors. These norm calculations are essential for numerical analysis throughout the optimization process, including steepest edge pricing, solution quality assessment, scaling factor computation, convergence criteria, and numerical stability diagnostics. The function provides a unified interface for all common norm types used in linear programming.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| x | const double* | Input vector | Array of n elements | Yes |
| n | int | Vector length | > 0 | Yes |
| norm_type | int | Norm type selector | 0=L∞, 1=L₁, 2=L₂ | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | double | Computed norm value (non-negative) |

### 2.3 Side Effects

None - pure function with read-only access to input vector.

## 3. Contract

### 3.1 Preconditions

- [ ] x array contains n valid floating-point values
- [ ] n is positive (n > 0)
- [ ] norm_type is 0, 1, or 2
- [ ] Array is properly allocated with at least n elements

### 3.2 Postconditions

- [ ] Returns non-negative norm value (≥ 0)
- [ ] Returns 0.0 if and only if x is the zero vector
- [ ] Input vector x is unchanged
- [ ] Result follows IEEE 754 double-precision arithmetic

### 3.3 Invariants

- [ ] Input vector remains unmodified
- [ ] Norm value is always non-negative
- [ ] For any scalar α: ||αx|| = |α| ||x||

## 4. Algorithm

### 4.1 Overview

**L1 norm (norm_type=1):** Sum of absolute values. Iterates through vector computing |x[i]| using bitwise operations for efficiency, accumulating the sum.

**L2 norm (norm_type=2):** Euclidean norm, square root of sum of squares. Accumulates x[i]² using Kahan summation for accuracy, then applies square root. This is the standard geometric length.

**L∞ norm (norm_type=0):** Maximum absolute value. Tracks running maximum while iterating through vector, using bitwise absolute value for speed.

### 4.2 Detailed Steps

**L1 Norm:**
1. Initialize sum = 0.0
2. For each i from 0 to n-1:
   - Compute abs_xi = |x[i]| via bitwise AND with 0x7FFFFFFFFFFFFFFF
   - Add abs_xi to sum
3. Return sum

**L2 Norm:**
1. Initialize sum = 0.0, compensation = 0.0
2. For each i from 0 to n-1:
   - Compute xi² = x[i] × x[i]
   - Accumulate using Kahan summation:
     - term = xi² - compensation
     - temp = sum + term
     - compensation = (temp - sum) - term
     - sum = temp
3. Return sqrt(sum)

**L∞ Norm:**
1. Initialize max_val = 0.0
2. For each i from 0 to n-1:
   - Compute abs_xi = |x[i]| via bitwise AND
   - If abs_xi > max_val, set max_val = abs_xi
3. Return max_val

### 4.3 Pseudocode (if needed)

```
Function: Vector_Norm(x, n, norm_type)
    if n ≤ 0 or x is NULL then
        return 0.0
    end

    switch norm_type do
        case 1:  // L1 norm
            sum ← 0.0
            for i ← 0 to n-1 do
                bits ← reinterpret x[i] as uint64
                bits ← bits AND 0x7FFFFFFFFFFFFFFF  // Clear sign bit
                abs_xi ← reinterpret bits as double
                sum ← sum + abs_xi
            end
            return sum

        case 2:  // L2 norm (Euclidean)
            sum ← 0.0
            compensation ← 0.0
            for i ← 0 to n-1 do
                xi ← x[i]
                term ← xi × xi - compensation
                temp ← sum + term
                compensation ← (temp - sum) - term
                sum ← temp
            end
            return sqrt(sum)

        case 0:  // L∞ norm (maximum)
        default:
            max_val ← 0.0
            for i ← 0 to n-1 do
                bits ← reinterpret x[i] as uint64
                bits ← bits AND 0x7FFFFFFFFFFFFFFF
                abs_xi ← reinterpret bits as double
                if abs_xi > max_val then
                    max_val ← abs_xi
                end
            end
            return max_val
    end
```

### 4.4 Mathematical Foundation (if applicable)

**L1 norm (Manhattan norm):**
||x||₁ = Σᵢ |x[i]| = |x₁| + |x₂| + ... + |xₙ|

**L2 norm (Euclidean norm):**
||x||₂ = √(Σᵢ x[i]²) = √(x₁² + x₂² + ... + xₙ²)

**L∞ norm (Maximum norm):**
||x||∞ = maxᵢ |x[i]|

Properties common to all norms:
- Non-negativity: ||x|| ≥ 0, with equality iff x = 0
- Homogeneity: ||αx|| = |α| ||x|| for scalar α
- Triangle inequality: ||x + y|| ≤ ||x|| + ||y||

Relationships:
- ||x||∞ ≤ ||x||₂ ≤ √n ||x||∞
- ||x||₂ ≤ ||x||₁ ≤ √n ||x||₂
- ||x||∞ ≤ ||x||₁ ≤ n ||x||∞

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(n) for all norm types
- **Average case:** O(n) for all norm types
- **Worst case:** O(n) for all norm types

Where:
- n = vector dimension

The sqrt operation in L2 norm adds negligible constant overhead.

### 5.2 Space Complexity

- **Auxiliary space:** O(1) - only scalar accumulators
- **Total space:** O(1)

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| n <= 0 | N/A | Returns 0.0 |
| x is NULL | N/A | Returns 0.0 or undefined behavior |
| Invalid norm_type | N/A | Defaults to L∞ norm |
| Overflow in L2 | N/A | May return Infinity |

### 6.2 Error Behavior

Minimal error checking for performance. Invalid inputs may return 0.0 or cause undefined behavior. L2 norm may overflow if sum of squares exceeds DBL_MAX, returning Infinity.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Zero vector | All x[i] = 0 | Return 0.0 for all norm types |
| Single element | n = 1 | Return |x[0]| for all norms |
| All equal values | x[i] = c for all i | L1: n|c|, L2: √n |c|, L∞: |c| |
| One large element | Most zeros, one large | L∞ ≈ L2 << L1 |
| All unit values | x[i] = ±1 | L1: n, L2: √n, L∞: 1 |
| Very large values | May overflow L2 | L2 returns Inf, others OK |

## 8. Thread Safety

**Thread-safe:** Yes

Pure function with read-only access to input. Safe for concurrent execution from multiple threads.

**Synchronization required:** None

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| sqrt | Standard C (math.h) | Square root for L2 norm |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_pricing_steepest | Pricing | Compute ||reduced_costs||₂ for edge weights |
| cxf_coefficient_stats | Statistics | Find max|A_ij| using L∞ norm |
| cxf_simplex_refine | Simplex | Compute ||Ax - b||₂ for residual |
| cxf_simplex_preprocess | Simplex | Row/column scaling factors |
| cxf_simplex_step3 | Simplex | ||dual_violations||∞ for optimality |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_dot_product | L2 norm is sqrt of self-dot-product: ||x||₂ = √(x·x) |
| cxf_sparse_vector_norm_l2 | Sparse variant computing same L2 norm |
| cxf_matrix_multiply | Uses norm for numerical checks |

## 11. Design Notes

### 11.1 Design Rationale

**Unified interface:** Single function for all norm types simplifies API and reduces code duplication.

**Bitwise absolute value:** Faster than fabs() library call, correctly handles NaN and Infinity, works by clearing IEEE 754 sign bit.

**Kahan summation for L2:** Reduces accumulated floating-point error in sum of squares, critical for large vectors where many small squares are summed.

**Default to L∞:** Most robust norm (no overflow), reasonable fallback for unknown norm types.

### 11.2 Performance Considerations

**Memory bandwidth limited:** For large vectors, sequential access is optimal. All variants iterate linearly.

**SIMD opportunities:**
- L1 and L∞: Parallel absolute value and reduction
- L2: Parallel multiply and accumulation (4-8x speedup with AVX)
- Modern CPUs can process 4 doubles per cycle

**Bitwise absolute value:** Avoids function call overhead, ~2x faster than fabs().

**Kahan overhead:** ~20% slower than naive summation, but much better accuracy.

### 11.3 Future Considerations

**Overflow handling for L2:**
Could implement scaling similar to BLAS dnrm2:
- Find max|x[i]|
- Scale by 1/max
- Compute norm of scaled vector
- Multiply result by max

**Mixed precision:** Accumulate in higher precision (e.g., long double) for better accuracy.

**Sparse variant:** For sparse vectors, only iterate over non-zeros.

**Pairwise summation:** Alternative to Kahan with similar accuracy, better parallelization.

## 12. References

- Kahan summation: Kahan, W. (1965). "Pracniques: Further Remarks on Reducing Truncation Errors". Communications of the ACM 8 (1): 40.
- Norm computations: Higham, N. J. (2002). Accuracy and Stability of Numerical Algorithms, 2nd ed. SIAM.
- BLAS dnrm2: Lawson, C. L., et al. (1979). Basic Linear Algebra Subprograms for Fortran Usage. ACM Trans. Math. Softw. 5 (3): 308–323.

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
