# cxf_dot_product

**Module:** Matrix Operations
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Computes the inner product (dot product) of two vectors, supporting both dense and sparse representations. This fundamental linear algebra operation appears throughout the simplex method in objective value computation, reduced cost calculation, constraint evaluation, and quadratic term evaluation. The function provides variants optimized for different sparsity patterns to maximize computational efficiency.

## 2. Signature

### 2.1 Inputs

**Dense variant:**
| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| x | const double* | First vector | Array of n elements | Yes |
| y | const double* | Second vector | Array of n elements | Yes |
| n | int | Vector length | > 0 | Yes |

**Sparse-dense variant:**
| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| x_indices | const int* | Indices of non-zeros in x | Array of x_nnz elements | Yes |
| x_values | const double* | Values of non-zeros in x | Array of x_nnz elements | Yes |
| x_nnz | int | Number of non-zeros in x | >= 0 | Yes |
| y_dense | const double* | Dense vector y | Array with sufficient size | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | double | Computed dot product (scalar value) |

### 2.3 Side Effects

None - pure function with read-only access to inputs.

## 3. Contract

### 3.1 Preconditions

**Dense variant:**
- [ ] x and y arrays contain n valid floating-point values
- [ ] n is positive
- [ ] Arrays are properly allocated

**Sparse variant:**
- [ ] x_indices contains valid indices in ascending order
- [ ] x_values contains corresponding coefficient values
- [ ] All indices are in valid range for y_dense
- [ ] x_nnz >= 0

### 3.2 Postconditions

- [ ] Returns sum of x[i] * y[i] for all i
- [ ] For sparse variant, returns sum of x_values[k] * y_dense[x_indices[k]]
- [ ] Input arrays unchanged
- [ ] Result follows IEEE 754 double-precision arithmetic rules

### 3.3 Invariants

- [ ] Input vectors remain unmodified
- [ ] Result is commutative: x·y = y·x
- [ ] Result is zero if either vector is all zeros

## 4. Algorithm

### 4.1 Overview

**Dense variant:** Sequentially multiplies corresponding elements and accumulates the sum. May use Kahan summation to reduce floating-point error accumulation for large vectors.

**Sparse-dense variant:** Iterates only over non-zero entries in the sparse vector x, looking up corresponding values in the dense vector y. This is more efficient when x has few non-zeros compared to its full dimension.

**Sparse-sparse variant:** Uses a merge-based algorithm assuming both vectors have sorted indices, iterating simultaneously and multiplying only at matching indices.

### 4.2 Detailed Steps

**Dense dot product:**
1. Initialize sum = 0.0
2. Initialize compensation = 0.0 (for Kahan summation)
3. For each index i from 0 to n-1:
   - Compute term = x[i] * y[i] - compensation
   - Compute temp = sum + term
   - Update compensation = (temp - sum) - term
   - Set sum = temp
4. Return sum

**Sparse-dense dot product:**
1. Validate inputs (return 0.0 for invalid)
2. Initialize sum = 0.0
3. For each k from 0 to x_nnz-1:
   - Extract index idx = x_indices[k]
   - Load x_val = x_values[k]
   - Load y_val = y_dense[idx]
   - Add x_val * y_val to sum
4. Return sum

**Sparse-sparse dot product:**
1. Initialize sum = 0.0, i = 0, j = 0
2. While i < x_nnz and j < y_nnz:
   - Compare x_indices[i] and y_indices[j]
   - If equal: multiply values, add to sum, increment both i and j
   - If x_indices[i] < y_indices[j]: increment i only
   - If x_indices[i] > y_indices[j]: increment j only
3. Return sum

### 4.3 Pseudocode (if needed)

```
Function: Dense_Dot_Product(x, y, n)
    sum ← 0.0
    compensation ← 0.0

    for i ← 0 to n-1 do
        term ← x[i] × y[i] - compensation
        temp ← sum + term
        compensation ← (temp - sum) - term
        sum ← temp
    end

    return sum

Function: Sparse_Dense_Dot_Product(x_indices, x_values, x_nnz, y_dense)
    if x_nnz ≤ 0 or any pointer is NULL then
        return 0.0
    end

    sum ← 0.0

    for k ← 0 to x_nnz-1 do
        idx ← x_indices[k]
        sum ← sum + x_values[k] × y_dense[idx]
    end

    return sum

Function: Sparse_Sparse_Dot_Product(x_indices, x_values, x_nnz,
                                     y_indices, y_values, y_nnz)
    sum ← 0.0
    i ← 0, j ← 0

    while i < x_nnz and j < y_nnz do
        if x_indices[i] = y_indices[j] then
            sum ← sum + x_values[i] × y_values[j]
            i ← i + 1, j ← j + 1
        else if x_indices[i] < y_indices[j] then
            i ← i + 1
        else
            j ← j + 1
        end
    end

    return sum
```

### 4.4 Mathematical Foundation (if applicable)

The dot product (inner product) of two vectors x, y ∈ ℝ^n is defined as:

x · y = Σᵢ x[i] × y[i] = x₁y₁ + x₂y₂ + ... + xₙyₙ

Properties:
- Commutative: x · y = y · x
- Distributive: (x + y) · z = x · z + y · z
- Scalar multiplication: (αx) · y = α(x · y)
- Cauchy-Schwarz inequality: |x · y| ≤ ||x||₂ × ||y||₂

For sparse vectors with index sets I_x and I_y:
x · y = Σᵢ∈(I_x ∩ I_y) x[i] × y[i]

## 5. Complexity

### 5.1 Time Complexity

**Dense variant:**
- **Best case:** O(n)
- **Average case:** O(n)
- **Worst case:** O(n)

**Sparse-dense variant:**
- **Best case:** O(1) if x_nnz = 0
- **Average case:** O(nnz_x)
- **Worst case:** O(nnz_x)

**Sparse-sparse variant:**
- **Best case:** O(min(nnz_x, nnz_y))
- **Average case:** O(nnz_x + nnz_y)
- **Worst case:** O(nnz_x + nnz_y)

Where:
- n = vector dimension
- nnz_x = number of non-zeros in x
- nnz_y = number of non-zeros in y

### 5.2 Space Complexity

- **Auxiliary space:** O(1) - no temporary allocations
- **Total space:** O(1) - only scalar accumulator

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| n <= 0 | N/A | Returns 0.0 |
| NULL pointers | N/A | Returns 0.0 or undefined behavior |
| Invalid indices | N/A | Undefined behavior (array access violation) |

### 6.2 Error Behavior

This is a performance-critical kernel. Dense variant may skip validation. Sparse variants may check for NULL and zero length, returning 0.0 as a safe default. Invalid indices cause undefined behavior - caller must ensure validity.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Zero vectors | x or y all zeros | Return 0.0 |
| Empty sparse | x_nnz = 0 | Return 0.0 immediately |
| Single element | n = 1 | Return x[0] * y[0] |
| No common indices | Sparse-sparse, disjoint | Return 0.0 (no matching indices) |
| Identical vectors | x = y | Return ||x||²₂ (sum of squares) |
| Orthogonal vectors | x · y = 0 | Return 0.0 or near-zero with rounding |

## 8. Thread Safety

**Thread-safe:** Yes

Pure function with read-only access to inputs. Safe for concurrent execution from multiple threads with different or identical inputs.

**Synchronization required:** None

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| None | - | May be fully inlined |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_simplex_step | Simplex | Objective value = c^T x |
| cxf_pricing_candidates | Pricing | Reduced cost = c[j] - (A_j)^T y |
| cxf_matrix_multiply | Matrix | Column contribution in Ax |
| cxf_quadratic_adjust | Quadratic | Quadratic term evaluation |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_vector_norm | Special case: ||x||₂ = √(x · x) |
| cxf_matrix_multiply | Generalizes to matrix-vector: y = Ax |
| cxf_matrix_transpose_multiply | Uses dot products for y = A^T x |

## 11. Design Notes

### 11.1 Design Rationale

Multiple variants optimize for different sparsity patterns:
- Dense-dense: Simple when both vectors are dense
- Sparse-dense: Efficient when one vector is sparse (common in simplex)
- Sparse-sparse: Optimal when both are sparse (rare but useful)

Kahan summation in dense variant reduces floating-point error accumulation, critical for numerical stability in long vectors. The overhead is minimal (~20%) compared to accuracy improvement.

### 11.2 Performance Considerations

**Dense variant:**
- Memory bandwidth limited for large n
- SIMD vectorization (AVX/FMA) can achieve 4-8x speedup
- Kahan summation adds ~20% overhead but significantly improves accuracy

**Sparse-dense variant:**
- Random memory access to y_dense (poor cache locality)
- Prefetching y_dense[idx] can help
- Efficient when nnz_x << n

**Sparse-sparse variant:**
- Merge requires sorted indices
- Linear scan is optimal for sorted inputs
- Rarely used in practice (most operations are sparse-dense)

### 11.3 Future Considerations

Optimizations:
- FMA instructions for multiply-add fusion
- AVX-512 vectorization for dense paths
- Cache prefetching for sparse lookups
- Pairwise summation as alternative to Kahan
- Mixed precision (accumulate in higher precision)

## 12. References

- Kahan summation: Kahan, W. (1965). "Pracniques: Further Remarks on Reducing Truncation Errors". Communications of the ACM 8 (1): 40.
- Dot product algorithms: Golub, G. H., & Van Loan, C. F. (2013). Matrix Computations, 4th ed. Johns Hopkins University Press.

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
