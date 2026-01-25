# cxf_getconstrs

**Module:** API - Matrix Queries
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Retrieves constraint matrix data for a range of constraints in Compressed Sparse Row (CSR) format. Provides efficient access to constraint coefficients by extracting row-wise data (which constraints contain which variables at what coefficients) from the internally column-oriented matrix storage. Supports two-call pattern for size querying followed by data retrieval.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| model | CxfModel* | Optimization model | Valid pointer | Yes |
| numnzP | int* | Output: total nonzero count | Valid pointer | Yes |
| cbeg | int* | Output: CSR row start indices | Valid or NULL | No |
| cind | int* | Output: variable indices | Valid or NULL | No |
| cval | double* | Output: coefficient values | Valid or NULL | No |
| start | int | First constraint index | 0 to numConstrs-1 | Yes |
| len | int | Number of constraints | 1 to numConstrs-start | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | Zero on success, error code on failure |
| *numnzP | int | Total nonzero count in requested constraints |
| cbeg[i] | int | Starting index in cind/cval for constraint start+i |
| cind[] | int | Variable indices for nonzeros |
| cval[] | double | Coefficient values for nonzeros |

### 2.3 Side Effects

- May build row-major (CSR) representation if not already cached
- Caches row-major data in matrix structure for future queries
- Allocates and frees temporary int64 buffer for cbeg values
- No modification to constraint data itself (read-only)

## 3. Contract

### 3.1 Preconditions

- [ ] Model pointer must be valid
- [ ] numnzP must be non-NULL
- [ ] start must be in range [0, numConstrs)
- [ ] start + len must be <= numConstrs
- [ ] len must be non-negative
- [ ] If cbeg/cind/cval are non-NULL, they must have sufficient space
- [ ] If any of cbeg/cind/cval is NULL, all three should be NULL (query-only mode)

### 3.2 Postconditions

- [ ] *numnzP contains accurate nonzero count
- [ ] If output arrays provided, cbeg[0..len-1] contains start indices
- [ ] If output arrays provided, cind[0..numnzP-1] contains variable indices
- [ ] If output arrays provided, cval[0..numnzP-1] contains coefficients
- [ ] Coefficients are adjusted for '>=' constraints (negated from internal form)
- [ ] Row-major representation is cached if built during call
- [ ] Temporary buffer is freed before return

### 3.3 Invariants

- [ ] Internal matrix data (CSC format) remains unchanged
- [ ] Model dimensions unchanged
- [ ] Constraint senses unchanged

## 4. Algorithm

### 4.1 Overview

The function operates in two modes: query-only (null output arrays) and retrieval (non-null arrays). It begins by validating the model and index range. For models with active callbacks, it uses a specialized callback-aware path.

The core algorithm builds a row-major representation if not already cached. Convexfeld stores matrices in column-major (CSC) format for efficient variable operations, but constraint queries need row-major (CSR) format. Building CSR involves transposing the sparse matrix structure, which is triggered lazily on first constraint query.

After ensuring CSR data exists, the function scans the requested constraint range, accumulating nonzero counts from row start/end pointers. If output arrays are provided, it copies data while applying transformations: for greater-than-or-equal constraints, coefficients are negated (Convexfeld stores all constraints in less-than-or-equal form internally). The function uses an int64 temporary buffer internally and converts to int32 for the public API.

### 4.2 Detailed Steps

1. **Model Validation**: Call checkmodel; if error, return error code
2. **Empty Query Check**: If len < 1, set *numnzP = 0 and return success
3. **Environment Access**: Retrieve environment pointer from model
4. **Null Array Check**: Determine if cbeg/cind/cval are all NULL (query mode) OR if any is non-NULL (retrieval mode)
5. **Temp Buffer Allocation**: If in retrieval mode, allocate int64 array of size len for temporary cbeg storage; if allocation fails, return OUT_OF_MEMORY (1001)
6. **Model Revalidation**: Call checkmodel again (allocation may have triggered collection)
7. **Matrix Check**: Verify model->matrix pointer is non-NULL; if NULL, set error INVALID_ARGUMENT (1003)
8. **Range Validation**: Check start >= 0 AND (start + len) <= matrix->numConstrs; if violated, log "Invalid index range for constraint query: [start, start+len]" and return INVALID_INDEX (1006)
9. **Callback Mode Check**: If model->callbackCount > 0, invoke getconstrs_callback with (model, &numnz, tempCbeg, cind, cval, start, len); goto result checking
10. **Row Data Check**: If matrix->rowStart is NULL, need to build row-major representation
11. **Build Trigger**: Determine preparation mode based on matrix->rowDataTrigger and matrix->matrixType fields
12. **Preparation**: If mode determined, call prepare_row_data
13. **CSR Construction**: Call build_row_major; if error, goto result checking
14. **Finalization**: If mode was set, call finalize_row_data; if error, goto error exit
15. **Matrix Refresh**: Re-read matrix pointer (may have changed during build)
16. **CSR Array Access**: Read rowStart, rowEnd, rowColIndices, rowCoeffValues, sense arrays from matrix structure
17. **Count Loop**: Initialize numnz = 0; for i from start to start+len-1, add (rowEnd[i] - rowStart[i]) to numnz
18. **Data Copy Check**: If tempCbeg AND cind AND cval are all non-NULL, proceed to copy
19. **Copy Loop**: Initialize outIdx = 0; for i from 0 to len-1: set tempCbeg[i] = outIdx; get row = start+i; get rowIdx = rowStart[row], rowEndIdx = rowEnd[row]; if sense[row] == '>', copy with negation loop (cind[outIdx] = rowColIndices[rowIdx], cval[outIdx] = -rowCoeffValues[rowIdx] via sign bit flip); else copy normally; increment outIdx
20. **Result Checking**: If status == 0, check if numnz > 2,000,000,000; if so, set error SIZE_LIMIT_EXCEEDED (10025); else set *numnzP = (int)numnz
21. **cbeg Conversion**: If tempCbeg exists AND cbeg is non-NULL, convert int64 array to int32: for i from 0 to len-1, cbeg[i] = (int)tempCbeg[i]
22. **Error Logging**: On error, log "Failed to retrieve constraints"
23. **Cleanup**: If tempCbeg was allocated, free it
24. **Return**: Return status code

### 4.3 Pseudocode

```
function get_constrs(model, numnzP, cbeg, cind, cval, start, len):
    status ← checkmodel(model)
    if status ≠ 0: return status

    if len < 1:
        *numnzP ← 0
        return SUCCESS

    # Allocate temp buffer if retrieving data
    if cbeg ≠ NULL ∧ cind ≠ NULL ∧ cval ≠ NULL:
        tempCbeg ← allocate(len × sizeof(int64))
        if tempCbeg = NULL:
            return OUT_OF_MEMORY

    # Validate range
    if start < 0 ∨ start + len > model.matrix.numConstrs:
        return INVALID_INDEX

    # Handle callback mode
    if model.callbackCount > 0:
        status ← get_constrs_callback(model, &numnz, tempCbeg, cind, cval, start, len)
        goto check_result

    # Build row-major if needed
    if model.matrix.rowStart = NULL:
        determine_prep_mode()
        if prep_mode ≠ -1:
            prepare_row_data(model)
        build_row_major(model)
        if prep_mode ≠ -1:
            finalize_row_data(model)

    # Count nonzeros
    numnz ← 0
    for i ← 0 to len-1:
        row ← start + i
        numnz += rowEnd[row] - rowStart[row]

    # Copy data if requested
    if tempCbeg ∧ cind ∧ cval:
        outIdx ← 0
        for i ← 0 to len-1:
            row ← start + i
            tempCbeg[i] ← outIdx
            for j ← rowStart[row] to rowEnd[row]-1:
                cind[outIdx] ← rowColIndices[j]
                if sense[row] = '>':
                    cval[outIdx] ← -rowCoeffValues[j]  # Negate for >= constraints
                else:
                    cval[outIdx] ← rowCoeffValues[j]
                outIdx++

check_result:
    if status = 0:
        if numnz > 2,000,000,000:
            return SIZE_LIMIT_EXCEEDED
        *numnzP ← numnz
        if tempCbeg ∧ cbeg:
            convert_int64_to_int32(tempCbeg, cbeg, len)

    free(tempCbeg)
    return status
```

### 4.4 Mathematical Foundation

Internally, Convexfeld stores constraints in the form:
```
Ax ≤ b  (for '≤' constraints)
-Ax ≤ -b  (for '≥' constraints, stored as negated '≤')
Ax = b  (for '=' constraints)
```

When retrieving a '>=' constraint, the function negates coefficients to present them in the user's expected form:
```
Retrieved: Ax ≥ b
```

This transformation is applied coefficient-wise: `cval[k] = -internal_value[k]`

The CSR format uses row start pointers:
```
cbeg[i] = starting index for constraint i
nonzeros for constraint i: cind[cbeg[i]..cbeg[i+1]-1], cval[cbeg[i]..cbeg[i+1]-1]
```

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) - len = 0 early return
- **Average case:** O(nnz_range + nnz_total) if building CSR, O(nnz_range) if cached
- **Worst case:** O(nnz_total + nnz_range) for first call after model modification

Where:
- nnz_total = total nonzeros in entire matrix
- nnz_range = nonzeros in requested constraint range
- Building CSR from CSC requires full matrix transpose: O(nnz_total)
- Counting and copying requested range: O(nnz_range)

### 5.2 Space Complexity

- **Auxiliary space:** O(len) for temporary cbeg buffer
- **Total space:** O(nnz_total) for cached CSR representation (one-time allocation)

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| Model validation fails | Various | Invalid model pointer |
| NULL numnzP | Likely crash | Required output parameter |
| Memory allocation fails | 1001 | Cannot allocate temp buffer |
| Matrix is NULL | 1003 | Model has no matrix data |
| Invalid index range | 1006 | start or len out of bounds |
| CSR build fails | Various | Error during matrix transpose |
| Size exceeds limit | 10025 | More than 2 billion nonzeros |

### 6.2 Error Behavior

On error, the function guarantees temp buffer cleanup (freed if allocated). The numnzP output may contain a partial count if error occurs late in execution. Output arrays (cbeg/cind/cval) are not modified if arrays are NULL or error occurs before copy phase. CSR cache may be partially built on error - subsequent calls may succeed using the cached data.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Empty range | len = 0 | Returns numnz = 0, success |
| Single constraint | len = 1 | Returns nonzeros for one row |
| All constraints | start = 0, len = numConstrs | Returns entire matrix in CSR |
| Empty constraints | Constraints with no nonzeros | Returns 0 counts, empty arrays |
| Dense row | Constraint with all variables | Returns n nonzeros |
| Only query call | cbeg/cind/cval all NULL | Returns count without allocating output |
| Mixed senses | Some '<', some '>', some '=' | Negates only '>' coefficients |
| Very large model | >2B nonzeros | Returns SIZE_LIMIT_EXCEEDED |
| Callback mode | Active callbacks | Uses callback-safe path |
| First call | CSR not cached | Triggers matrix transpose |
| Subsequent calls | CSR cached | Fast path using cached data |

## 8. Thread Safety

**Thread-safe:** Conditionally

The function modifies shared model state (CSR cache) during first call. If multiple threads call simultaneously on an uncached model, race conditions may occur. After CSR is cached, concurrent reads are safe. Use model-level locking to serialize first calls. For models with active callbacks, uses callback-safe implementation.

**Synchronization required:** Serialize first call per model, or use after optimization when cache is populated

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_checkmodel | Validation | Validate model pointer |
| cxf_malloc | Memory | Allocate temp cbeg buffer |
| cxf_free | Memory | Free temp buffer |
| cxf_error | Error | Log error messages |
| cxf_getconstrs_callback | Callbacks | Callback-aware retrieval |
| cxf_prepare_row_data | Matrix | Prepare for CSR build |
| cxf_build_row_major | Matrix | Transpose CSC to CSR |
| cxf_finalize_row_data | Matrix | Finalize CSR build |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| User application | External | Extract constraint structure |
| I/O functions | File I/O | Write model to file formats |
| Analysis tools | External | Inspect problem structure |
| Model transformation | External | Modify or reformulate model |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_getcols | Inverse - retrieves column data (variables) |
| cxf_getcoeff | Related - retrieves single coefficient |
| cxf_getdblattrarray | Related - retrieves constraint RHS values |
| cxf_getcharattrarray | Related - retrieves constraint senses |
| cxf_addconstr | Inverse - adds a constraint |
| cxf_delconstrs | Related - removes constraints |
| cxf_x_getconstrs | Extended - handles >2B nonzeros with int64 |

## 11. Design Notes

### 11.1 Design Rationale

The two-call pattern (query size, allocate, retrieve data) is standard for variable-size output in C APIs. The CSR caching strategy amortizes transpose cost across multiple queries. The int64 intermediate representation supports large models internally while maintaining int32 public API for compatibility.

Negating coefficients for '>=' constraints presents a natural interface to users while maintaining internal efficiency (storing all constraints as '<=' simplifies solver algorithms). The XOR-based negation (sign bit flip) is numerically exact and handles special values correctly.

The callback-mode path isolates concurrency concerns from the main implementation. Lazy CSR building avoids work if users never query constraints.

### 11.2 Performance Considerations

First call after model modification incurs full matrix transpose: O(nnz) time and space. Subsequent calls are O(nnz_range) - fast for small ranges. The cache remains valid until model modification invalidates it.

For incremental constraint queries, make a single call with full range rather than multiple small-range calls - this amortizes transpose cost. For very large models where transpose is expensive, consider querying individual coefficients with cxf_getcoeff if only sparse access is needed.

### 11.3 Future Considerations

Potential enhancements: incremental CSR updates on constraint add/remove (avoid full rebuild), compressed output formats for very large queries, streaming interface for memory-limited environments, and parallel CSR construction for multi-threaded transpose.

## 12. References

- Convexfeld Optimizer Reference Manual - cxf_getconstrs documentation
- Compressed Sparse Row format specification
- Saad, Y. (2003). Iterative Methods for Sparse Linear Systems (CSR/CSC formats)

## 13. Validation Checklist

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
