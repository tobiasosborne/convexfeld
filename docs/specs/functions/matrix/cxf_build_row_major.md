# cxf_build_row_major

**Module:** Matrix Operations
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Converts a sparse matrix from Compressed Sparse Column (CSC) format to Compressed Sparse Row (CSR) format. This function performs the core transpose operation in Convexfeld's lazy matrix format conversion pipeline, enabling efficient row-wise access to constraint data when needed by API functions like cxf_getconstrs. The conversion is performed on-demand to minimize memory overhead for applications that primarily work with columns (variables).

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| model | CxfModel* | Model containing matrix data to transpose | Valid pointer to initialized model | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | Status code (0=success, error code otherwise) |
| model->matrix->rowStart | int64_t* | CSR row start pointers (filled) |
| model->matrix->rowEnd | int64_t* | CSR row end pointers (filled) |
| model->matrix->rowColIndices | int32_t* | CSR column indices (filled) |
| model->matrix->rowCoeffValues | double* | CSR coefficient values (filled) |

### 2.3 Side Effects

Populates the CSR arrays in the model's matrix structure with transposed data from the CSC representation. Allocates and frees temporary workspace memory.

## 3. Contract

### 3.1 Preconditions

- [ ] model pointer must be non-NULL and valid
- [ ] model->matrix must be non-NULL
- [ ] model->env must be non-NULL (for memory allocation)
- [ ] CSC arrays must be allocated (colStart, rowIndices, coeffValues)
- [ ] CSR arrays must be allocated (rowStart, rowEnd, rowColIndices, rowCoeffValues)
- [ ] rowStart must be zero-initialized
- [ ] cxf_prepare_row_data must have been called successfully

### 3.2 Postconditions

- [ ] rowStart[i] contains starting position for row i in CSR arrays
- [ ] rowStart[numConstrs] equals numNonzeros (sentinel value)
- [ ] rowEnd[i] equals rowStart[i+1] for all valid i
- [ ] rowColIndices contains column indices sorted in ascending order within each row
- [ ] rowCoeffValues contains corresponding coefficient values
- [ ] Total non-zeros preserved (no data loss)
- [ ] Matrix semantics unchanged (same mathematical matrix)

### 3.3 Invariants

- [ ] Input CSC arrays (colStart, rowIndices, coeffValues) remain unmodified
- [ ] Matrix dimensions (numVars, numConstrs, numNonzeros) remain unchanged
- [ ] Coefficient values are copied bit-exactly (no numerical operations)

## 4. Algorithm

### 4.1 Overview

Implements the classic two-pass sparse matrix transpose algorithm. The first pass counts non-zero entries per row and computes cumulative row start positions. The second pass fills the output arrays by iterating through columns in reverse order, ensuring that column indices within each row appear in ascending order. This algorithm is optimal for CSC-to-CSR conversion with O(nnz) time complexity and minimal auxiliary space.

The reverse iteration in Pass 2 is crucial for maintaining sorted column order within each row. By processing columns from highest to lowest and using pre-decrement pointers, entries naturally appear in ascending column order when the final arrays are read forward.

### 4.2 Detailed Steps

1. Validate input pointers and matrix structure for consistency
2. Extract matrix dimensions (numConstrs, numVars, numNonzeros) and array pointers
3. Allocate temporary working pointer array of size (numConstrs + 1)
4. **Pass 1 - Count Phase:**
   - Iterate through each column j from 0 to numVars-1
   - For each entry k in column j (from colStart[j] to colStart[j+1])
   - Extract row index i from rowIndices[k]
   - Increment rowStart[i] to count entries in row i
5. **Pass 1 - Cumulative Sum:**
   - Convert row counts to cumulative starting positions
   - Set cumsum = 0, then for each row i from 0 to numConstrs
   - Save current rowStart[i], set rowStart[i] = cumsum, add saved value to cumsum
   - Result: rowStart[i] now holds starting index for row i
6. Copy rowStart values to workPtr array for use in Pass 2
7. Set rowEnd[i] = rowStart[i+1] for each row i
8. **Pass 2 - Fill Phase:**
   - Iterate columns j in reverse from numVars-1 down to 0
   - For each entry k in column j (in reverse from colStart[j+1]-1 down to colStart[j])
   - Extract row index i and coefficient value from rowIndices[k] and coeffValues[k]
   - Pre-decrement workPtr[i] to get insertion position
   - Store column j at rowColIndices[position]
   - Store coefficient at rowCoeffValues[position]
9. Free temporary workPtr array
10. Return success status

### 4.3 Pseudocode (if needed)

```
Algorithm: CSC_to_CSR_Transpose
Input: CSC matrix (colStart, rowIndices, coeffValues), dimensions (m, n, nnz)
Output: CSR matrix (rowStart, rowEnd, rowColIndices, rowCoeffValues)

// Pass 1: Count non-zeros per row
rowStart ← array of m+1 zeros
for j ← 0 to n-1 do
    for k ← colStart[j] to colStart[j+1]-1 do
        i ← rowIndices[k]
        rowStart[i] ← rowStart[i] + 1
    end
end

// Convert counts to cumulative positions
cumsum ← 0
for i ← 0 to m do
    count ← rowStart[i]
    rowStart[i] ← cumsum
    cumsum ← cumsum + count
end

// Set up working pointers and row ends
workPtr ← copy of rowStart
for i ← 0 to m-1 do
    rowEnd[i] ← rowStart[i+1]
end

// Pass 2: Fill CSR arrays (reverse iteration)
for j ← n-1 down to 0 do
    for k ← colStart[j+1]-1 down to colStart[j] do
        i ← rowIndices[k]
        val ← coeffValues[k]
        pos ← workPtr[i] - 1
        workPtr[i] ← pos
        rowColIndices[pos] ← j
        rowCoeffValues[pos] ← val
    end
end
```

### 4.4 Mathematical Foundation (if applicable)

The algorithm performs a matrix transpose operation where matrix A in CSC format is converted to matrix A^T in CSC format (which is equivalent to A in CSR format). The mathematical operation preserves all matrix properties:

- A[i,j] = A^T[j,i] for all i,j
- Non-zero pattern is preserved
- Coefficient values are copied exactly without floating-point operations

The cumulative sum operation in Pass 1 is equivalent to computing prefix sums of the row count vector, which determines the partitioning of the output arrays by row.

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(nnz + m + n) where nnz = numNonzeros, m = numConstrs, n = numVars
- **Average case:** O(nnz + m + n)
- **Worst case:** O(nnz + m + n)

Where:
- nnz = number of non-zero entries in the matrix
- m = number of constraints (rows)
- n = number of variables (columns)

The dominant term is O(nnz) for the two passes over all non-zero entries.

### 5.2 Space Complexity

- **Auxiliary space:** O(m) for temporary working pointer array
- **Total space:** O(nnz + m) including output arrays

The CSR arrays are preallocated by cxf_prepare_row_data, so this function only allocates temporary workspace.

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| model is NULL | 1003 | Invalid argument error |
| matrix is NULL | 1003 | Invalid argument error |
| env is NULL | 1003 | Invalid argument error |
| colStart is NULL | 1003 | CSC arrays not initialized |
| rowStart is NULL | 1003 | CSR arrays not prepared |
| Memory allocation fails | 1001 | Out of memory for temporary workspace |

### 6.2 Error Behavior

On error, the function returns immediately with an error code. If memory allocation fails for the temporary workPtr array, the function returns 1001 without modifying any matrix data. Input validation errors return 1003. The CSR arrays may be partially filled on error but should not be used (caller should check return code).

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Empty matrix | numNonzeros = 0 | All rowStart[i] = 0, rowEnd[i] = 0, no iterations in Pass 2 |
| Empty rows | Some rows have no entries | rowStart[i] = rowStart[i+1], rowEnd[i] = rowStart[i] |
| Empty columns | Some columns have no entries | Skipped in Pass 2 (no iteration), no contribution to CSR |
| Single entry | numNonzeros = 1 | Correctly places single entry in CSR format |
| Dense row | Row i has n entries | rowStart[i+1] - rowStart[i] = n |
| Dense matrix | All m*n entries present | nnz = m*n, all positions filled |

## 8. Thread Safety

**Thread-safe:** No

The function modifies shared matrix data structures (rowStart, rowEnd, rowColIndices, rowCoeffValues) and uses a mutable temporary workspace. Concurrent calls on the same model will corrupt data.

**Synchronization required:** Caller must hold model lock before calling this function. Typically acquired by the public API function (cxf_getconstrs) that triggers the conversion pipeline.

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_malloc | Memory | Allocate temporary working pointer array |
| cxf_free | Memory | Free temporary working pointer array |
| memcpy | Standard C | Copy rowStart to workPtr |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_getconstrs | API | Retrieve constraint data when CSR format needed |
| Internal constraint query | Solver | Access row data during optimization |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_prepare_row_data | Prerequisite - allocates CSR arrays before this function |
| cxf_finalize_row_data | Successor - finalizes state after this function |
| cxf_getconstrs | Caller - triggers 3-stage conversion pipeline |

## 11. Design Notes

### 11.1 Design Rationale

The two-pass algorithm is chosen because:
1. It is the standard, well-understood approach for sparse matrix transpose
2. It requires minimal auxiliary space (only O(m) vs O(nnz) for alternatives)
3. It has optimal time complexity O(nnz)
4. It is cache-friendly with sequential access patterns in Pass 1

Reverse iteration in Pass 2 ensures sorted column order within rows without requiring an explicit sorting step, which would add O(nnz log n) overhead.

### 11.2 Performance Considerations

Memory bandwidth is the primary bottleneck for large matrices. Random writes to rowStart in Pass 1 and to CSR arrays in Pass 2 result in poor cache locality. Parallelization across rows could improve performance but requires atomic operations or thread-local workPtr arrays.

For typical LP problems with 100K-10M non-zeros, conversion time ranges from 0.5ms to 500ms, which is acceptable for a one-time cost.

### 11.3 Future Considerations

Potential optimizations:
- Parallel transpose by partitioning rows across threads
- Cache blocking to improve random write locality
- SIMD for cumulative sum computation
- Avoiding allocation by reusing thread-local buffers

The current implementation prioritizes simplicity and correctness over maximum performance.

## 12. References

- Sparse matrix transpose algorithms: Davis, T. A. (2006). Direct Methods for Sparse Linear Systems. SIAM.
- CSC/CSR formats: Saad, Y. (2003). Iterative Methods for Sparse Linear Systems, 2nd ed.

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
