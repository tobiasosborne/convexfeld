# cxf_matrix_multiply

**Module:** Matrix Operations
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Performs sparse matrix-vector multiplication in the form y = Ax or y += Ax, where A is a sparse constraint matrix in CSC (Compressed Sparse Column) format. This fundamental linear algebra operation is used extensively throughout the simplex method for constraint evaluation, reduced cost computation, and basis operations. The function exploits sparsity for efficiency and supports both overwrite and accumulate modes.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| x | const double* | Input vector | Array of numVars elements | Yes |
| y | double* | Output vector (modified) | Array of numConstrs elements | Yes |
| numVars | int | Number of variables (columns) | > 0 | Yes |
| numConstrs | int | Number of constraints (rows) | > 0 | Yes |
| colStart | const int64_t* | CSC column start indices | Array of (numVars+1) elements | Yes |
| rowIndices | const int32_t* | CSC row indices | Array of nnz elements | Yes |
| coeffValues | const double* | CSC coefficient values | Array of nnz elements | Yes |
| accumulate | int | Mode flag | 0=overwrite y, 1=accumulate to y | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| y | double* | Result vector (y = Ax or y += Ax) |

### 2.3 Side Effects

Modifies the output vector y. If accumulate=0, overwrites y with Ax. If accumulate=1, adds Ax to existing y values.

## 3. Contract

### 3.1 Preconditions

- [ ] x array contains numVars valid floating-point values
- [ ] y array has space for numConstrs elements
- [ ] colStart array has (numVars+1) elements with valid CSC structure
- [ ] rowIndices contains valid row indices in range [0, numConstrs)
- [ ] coeffValues contains valid coefficient values
- [ ] colStart is monotonically non-decreasing
- [ ] colStart[numVars] equals total number of non-zeros

### 3.2 Postconditions

- [ ] If accumulate=0: y[i] = sum over j of A[i,j] * x[j] for all rows i
- [ ] If accumulate=1: y[i] += sum over j of A[i,j] * x[j] for all rows i
- [ ] x array is unchanged
- [ ] Matrix arrays (colStart, rowIndices, coeffValues) are unchanged

### 3.3 Invariants

- [ ] Input matrix structure remains unchanged
- [ ] Input vector x remains unchanged
- [ ] Numerical precision follows IEEE 754 double-precision rules

## 4. Algorithm

### 4.1 Overview

Implements standard sparse matrix-vector multiplication using the CSC format. The algorithm iterates over columns (variables) in order, and for each column, iterates over the non-zero entries, accumulating their contributions to the corresponding rows in the output vector. If x[j] is zero (common in sparse vectors), the entire column is skipped for efficiency. This column-oriented approach is cache-efficient for CSC format.

### 4.2 Detailed Steps

1. If accumulate mode is 0 (overwrite), initialize all elements of y to zero
2. For each variable j from 0 to numVars-1:
   - Load value xj = x[j]
   - If xj equals 0.0, skip to next column (optimization)
   - Extract column range: start = colStart[j], end = colStart[j+1]
   - For each non-zero entry k from start to end-1:
     - Extract row index: row = rowIndices[k]
     - Extract coefficient: coeff = coeffValues[k]
     - Accumulate contribution: y[row] += coeff * xj
3. Return (output is in y array)

### 4.3 Pseudocode (if needed)

```
Algorithm: Sparse_Matrix_Vector_Multiply
Input: Sparse matrix A in CSC format (colStart, rowIndices, coeffValues)
       Dense vector x[numVars], accumulate flag
Output: Dense vector y[numConstrs]

if accumulate = 0 then
    for i ← 0 to numConstrs-1 do
        y[i] ← 0
    end
end

for j ← 0 to numVars-1 do
    xj ← x[j]

    if xj = 0 then
        continue  // Skip zero columns for efficiency
    end

    start ← colStart[j]
    end ← colStart[j+1]

    for k ← start to end-1 do
        row ← rowIndices[k]
        coeff ← coeffValues[k]
        y[row] ← y[row] + coeff × xj
    end
end
```

### 4.4 Mathematical Foundation (if applicable)

The operation computes the matrix-vector product:

y = Ax where A ∈ ℝ^(m×n), x ∈ ℝ^n, y ∈ ℝ^m

Or in accumulate mode: y ← y + Ax

For each output element: y_i = Σⱼ A[i,j] × x[j]

The CSC format stores column j as a list of (row_index, value) pairs:
- Indices: rowIndices[colStart[j] : colStart[j+1]]
- Values: coeffValues[colStart[j] : colStart[j+1]]

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(numVars) if all columns are empty or x is all zeros
- **Average case:** O(nnz) where nnz = number of non-zero entries
- **Worst case:** O(nnz)

Where:
- nnz = colStart[numVars] = total number of non-zero matrix entries
- The zero-check optimization reduces work for sparse x vectors

### 5.2 Space Complexity

- **Auxiliary space:** O(1) - no temporary allocations
- **Total space:** O(numConstrs) for output vector y

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| NULL pointers | N/A | Undefined behavior (precondition violation) |
| Invalid dimensions | N/A | Undefined behavior (precondition violation) |
| Out-of-bounds row index | N/A | May cause segmentation fault |

### 6.2 Error Behavior

This is a low-level computational kernel with no explicit error checking for performance. Callers must ensure preconditions are satisfied. Invalid inputs result in undefined behavior.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Zero vector x | All x[j] = 0 | y = 0 (all columns skipped) |
| Empty columns | colStart[j] = colStart[j+1] | Column j contributes nothing |
| Empty matrix | All columns empty | y = 0 |
| Single entry | nnz = 1 | y[row] += coeff * x[col] |
| Dense column | Column has numConstrs entries | All y elements updated |
| Accumulate mode | accumulate = 1 | Existing y values preserved and added to |

## 8. Thread Safety

**Thread-safe:** Conditionally

Read-only access to matrix data (colStart, rowIndices, coeffValues) and input vector x. Writes only to output vector y. Safe for concurrent execution if:
- Different threads use different output vectors y
- Input data (A, x) is not being modified concurrently

**Synchronization required:** None if preconditions met. Not safe to write to y from multiple threads simultaneously.

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| memset | Standard C | Zero-initialize y (if accumulate=0) |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_simplex_step | Simplex | Constraint evaluation (Ax = b check) |
| cxf_pricing_candidates | Pricing | Reduced cost computation (c - A^T y) |
| cxf_ftran | Basis | Forward transformation with matrix |
| cxf_presolve | Presolve | Bound tightening calculations |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_matrix_transpose_multiply | Computes y = A^T x (transpose multiply) |
| cxf_dot_product | Special case for single row (sparse-dense dot product) |
| cxf_build_row_major | Provides CSR format for row-wise multiplication |

## 11. Design Notes

### 11.1 Design Rationale

CSC format is chosen because:
1. Convexfeld stores matrices in column-major format for variable operations
2. Column-wise iteration is cache-friendly for CSC
3. Zero-skipping is efficient when x is sparse (common in simplex)
4. No temporary storage required

The accumulate mode supports iterative algorithms that build up results incrementally without requiring separate y = Ax then z = y + ... operations.

### 11.2 Performance Considerations

Memory bandwidth is the bottleneck for large sparse matrices. The algorithm performs:
- Sequential reads of colStart (good cache locality)
- Random reads of x via column index j (acceptable if x fits in cache)
- Random writes to y via rowIndices (poor cache locality)

For very dense columns (>16 entries), SIMD vectorization (AVX) could improve performance. The zero-check optimization is critical for sparse x vectors common in simplex phase 1.

### 11.3 Future Considerations

Potential optimizations:
- AVX vectorization for dense columns
- Memory prefetching for rowIndices and y arrays
- Blocked algorithms for better cache utilization
- Parallel execution by partitioning columns across threads
- Kahan summation for improved numerical stability

## 12. References

- Sparse matrix-vector multiplication: Saad, Y. (2003). Iterative Methods for Sparse Linear Systems, 2nd ed.
- CSC format specification: Davis, T. A. (2006). Direct Methods for Sparse Linear Systems. SIAM.

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
