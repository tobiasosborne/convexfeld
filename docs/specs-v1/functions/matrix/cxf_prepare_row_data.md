# cxf_prepare_row_data

**Module:** Matrix Operations
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Prepares memory and data structures for converting a sparse matrix from CSC (Compressed Sparse Column) to CSR (Compressed Sparse Row) format. This is the first stage of Convexfeld's lazy three-stage conversion pipeline. The function allocates the necessary output arrays and initializes them for use by the subsequent transpose algorithm, enabling efficient row-wise constraint access without maintaining dual representations at all times.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| model | CxfModel* | Model containing matrix to prepare | Valid pointer to initialized model | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | Status code (0=success, error code otherwise) |
| model->matrix->rowStart | int64_t* | Allocated and zero-initialized CSR row start array |
| model->matrix->rowEnd | int64_t* | Allocated CSR row end array |
| model->matrix->rowColIndices | int32_t* | Allocated CSR column indices array |
| model->matrix->rowCoeffValues | double* | Allocated CSR coefficient values array |

### 2.3 Side Effects

Allocates four arrays in the matrix structure. On allocation failure, cleans up any partially allocated memory and sets all pointers to NULL to maintain consistent state.

## 3. Contract

### 3.1 Preconditions

- [ ] model pointer must be non-NULL and valid
- [ ] model->matrix must be non-NULL
- [ ] model->env must be non-NULL (required for memory allocation)
- [ ] Matrix dimensions (numConstrs, numNonzeros) must be set correctly

### 3.2 Postconditions

- [ ] rowStart is allocated with (numConstrs + 1) elements, zero-initialized
- [ ] rowEnd is allocated with numConstrs elements
- [ ] rowColIndices is allocated with numNonzeros elements
- [ ] rowCoeffValues is allocated with numNonzeros elements
- [ ] All arrays are uninitialized except rowStart (zero-filled)
- [ ] On error, all CSR pointers are NULL (consistent partial state not left behind)

### 3.3 Invariants

- [ ] CSC arrays (colStart, rowIndices, coeffValues) remain unmodified
- [ ] Matrix dimensions remain unchanged
- [ ] If rowStart is already non-NULL, function returns success immediately (idempotent)

## 4. Algorithm

### 4.1 Overview

Performs sequential memory allocation for the four CSR arrays required by the transpose algorithm. The rowStart array is allocated with an extra sentinel element and zero-initialized because the first pass of the transpose uses it as a counter array. Error handling uses a cleanup chain to ensure no memory leaks occur if allocation fails partway through. The function is idempotent - if CSR data is already prepared, it returns success without re-allocating.

### 4.2 Detailed Steps

1. Validate model pointer is non-NULL
2. Extract matrix and environment pointers from model
3. Validate matrix and environment pointers are non-NULL
4. Check if rowStart is already allocated (idempotency check)
5. If already allocated, return success immediately
6. Extract matrix dimensions: numConstrs and numNonzeros
7. Allocate rowStart array with (numConstrs + 1) elements (8 bytes each)
8. If allocation fails, jump to error cleanup and return out-of-memory error
9. Allocate rowEnd array with numConstrs elements (8 bytes each)
10. If allocation fails, free rowStart, set to NULL, return error
11. Allocate rowColIndices array with numNonzeros elements (4 bytes each)
12. If allocation fails, free rowEnd and rowStart, set to NULL, return error
13. Allocate rowCoeffValues array with numNonzeros elements (8 bytes each)
14. If allocation fails, free rowColIndices, rowEnd, rowStart, set all to NULL, return error
15. Zero-initialize rowStart array using memset
16. Return success code

### 4.3 Pseudocode (if needed)

```
Algorithm: Prepare_CSR_Arrays
Input: Model with matrix dimensions (m constraints, nnz nonzeros)
Output: Allocated CSR arrays or error code

if model is NULL or matrix is NULL or env is NULL then
    return ERROR_INVALID_ARGUMENT
end

if matrix.rowStart ≠ NULL then
    return SUCCESS  // Already prepared
end

m ← matrix.numConstrs
nnz ← matrix.numNonzeros

// Allocate arrays with error handling chain
matrix.rowStart ← allocate (m+1) × 8 bytes
if allocation failed then
    return ERROR_OUT_OF_MEMORY
end

matrix.rowEnd ← allocate m × 8 bytes
if allocation failed then
    free(rowStart)
    rowStart ← NULL
    return ERROR_OUT_OF_MEMORY
end

matrix.rowColIndices ← allocate nnz × 4 bytes
if allocation failed then
    free(rowEnd), free(rowStart)
    rowEnd, rowStart ← NULL
    return ERROR_OUT_OF_MEMORY
end

matrix.rowCoeffValues ← allocate nnz × 8 bytes
if allocation failed then
    free(rowColIndices), free(rowEnd), free(rowStart)
    all ← NULL
    return ERROR_OUT_OF_MEMORY
end

// Initialize rowStart to zeros
fill rowStart[0..m] with zeros

return SUCCESS
```

### 4.4 Mathematical Foundation (if applicable)

Not applicable - this is a memory management function without mathematical operations.

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) if already prepared (idempotent path)
- **Average case:** O(m) for zero-initialization of rowStart
- **Worst case:** O(m) for initialization

Where:
- m = numConstrs (number of constraints/rows)

The memset operation dominates, with time linear in the rowStart array size.

### 5.2 Space Complexity

- **Auxiliary space:** O(1) - no temporary allocations
- **Total space:** O(nnz + m) for allocated CSR arrays
  - rowStart: (m+1) × 8 bytes
  - rowEnd: m × 8 bytes
  - rowColIndices: nnz × 4 bytes
  - rowCoeffValues: nnz × 8 bytes
  - Total: 16m + 8 + 12nnz bytes

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| model is NULL | 1003 | Invalid argument |
| matrix is NULL | 1003 | Invalid argument |
| env is NULL | 1003 | Invalid argument |
| rowStart allocation fails | 1001 | Out of memory |
| rowEnd allocation fails | 1001 | Out of memory |
| rowColIndices allocation fails | 1001 | Out of memory |
| rowCoeffValues allocation fails | 1001 | Out of memory |

### 6.2 Error Behavior

On allocation failure, the function immediately frees all previously allocated arrays in reverse order and sets their pointers to NULL. This ensures no partial state is left behind - either all arrays are allocated successfully or none are. The function maintains exception safety by using a cleanup chain pattern.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Already prepared | rowStart ≠ NULL | Return success immediately, no re-allocation |
| Empty matrix | numNonzeros = 0 | Allocate zero-sized rowColIndices and rowCoeffValues arrays |
| No constraints | numConstrs = 0 | Allocate 1-element rowStart array (sentinel only) |
| Large matrix | numNonzeros > 10^9 | May fail allocation due to memory limits |
| Repeated calls | Called multiple times | First call allocates, subsequent calls return success |

## 8. Thread Safety

**Thread-safe:** No

The function modifies shared matrix structure pointers. Concurrent calls could result in race conditions where both threads allocate memory, leading to memory leaks.

**Synchronization required:** Caller must hold model lock. Typically acquired by cxf_getconstrs before invoking the preparation pipeline.

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_malloc | Memory | Allocate each of the four CSR arrays |
| cxf_free | Memory | Free arrays on allocation failure |
| memset | Standard C | Zero-initialize rowStart array |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_getconstrs | API | First stage of CSR conversion pipeline |
| Internal constraint access | Solver | When row-major data is needed |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_build_row_major | Successor - performs transpose using allocated arrays |
| cxf_finalize_row_data | Finalizer - sets flags after conversion complete |
| cxf_getconstrs | Caller - triggers conversion when CSR data needed |

## 11. Design Notes

### 11.1 Design Rationale

The lazy conversion approach minimizes memory usage for models that never query constraint data by row. Allocating CSR arrays only when needed avoids doubling memory consumption for the matrix representation. The idempotency check allows safe repeated calls without memory leaks.

The extra sentinel element in rowStart (size m+1 instead of m) is standard for CSR format - rowStart[m] stores the total number of non-zeros, simplifying bounds checking.

### 11.2 Performance Considerations

Memory allocation dominates performance. Four separate allocations incur overhead from repeated lock acquisition on the memory pool. An alternative design could allocate a single contiguous block and partition it, reducing lock contention and improving cache locality.

The memset zero-initialization of rowStart is fast on modern hardware (~10 GB/s) and negligible for typical problem sizes (m < 1M rows = 8 MB).

### 11.3 Future Considerations

Potential optimizations:
- Single contiguous allocation for all four arrays
- Reuse CSR arrays across multiple models (pooling)
- Huge page support for very large models
- Aligned allocation for SIMD operations in transpose

The error handling chain could be simplified using RAII patterns in a C++ implementation.

## 12. References

- Sparse matrix storage formats: Saad, Y. (2003). Iterative Methods for Sparse Linear Systems, 2nd ed.
- Memory management patterns: Alexandrescu, A. (2001). Modern C++ Design.

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
