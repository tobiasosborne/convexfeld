# cxf_getcoeff

**Module:** API - Matrix Queries
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Retrieves a single coefficient from the constraint matrix by row and column index. Provides random access to individual A[i,j] values without requiring retrieval of entire rows or columns. Performs sparse lookup in the column-major internal representation, applying transformations for constraint senses and variable negations.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| model | CxfModel* | Optimization model | Valid pointer | Yes |
| constr | int | Constraint index (row) | 0 to numConstrs-1 | Yes |
| var | int | Variable index (column) | 0 to numVars-1 | Yes |
| valP | double* | Output: coefficient value | Valid pointer | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | Zero on success, error code on failure |
| *valP | double | Coefficient A[constr, var] (zero if not present) |

### 2.3 Side Effects

- Reads from matrix data structures (CSC format arrays)
- May read scaling factors and sense arrays
- No modification to model state (read-only)
- Error messages logged on invalid indices

## 3. Contract

### 3.1 Preconditions

- [ ] Model pointer must be valid
- [ ] valP must be non-NULL
- [ ] constr must be in range [0, numConstrs)
- [ ] var must be in range [0, numVars)
- [ ] Model matrix must exist (not NULL)

### 3.2 Postconditions

- [ ] *valP contains coefficient value (zero if entry not present in sparse matrix)
- [ ] Value is adjusted for '>=' constraints (negated from internal form)
- [ ] Value is unscaled if scaling factors exist
- [ ] Value accounts for variable negation in maximization problems
- [ ] On error, *valP is not modified

### 3.3 Invariants

- [ ] Matrix data structures remain unchanged
- [ ] Model dimensions unchanged
- [ ] Sparse matrix structure unchanged

## 4. Algorithm

### 4.1 Overview

The function performs sparse lookup in a Compressed Sparse Column (CSC) matrix. It first validates model and index ranges. For local access, it retrieves the column's data (start index, length, row indices, coefficient values).

Linear search through the column's row indices finds the requested row. If found, the function reads the corresponding coefficient and applies three transformations: unscaling (if scaling factors exist), negation for '>=' constraints (internal form is '<=' with negated coefficients), and negation for negated variables in maximization problems. If the row is not found in the column, the coefficient is zero (sparse matrix convention).

The algorithm exploits column-major storage for efficiency - columns are stored contiguously, allowing fast traversal. The search could be optimized to binary search if row indices are sorted, but the implementation uses linear search for simplicity.

### 4.2 Detailed Steps

1. **Model Validation**: Call checkmodel; if error, return error code
2. **Output Validation**: Check if valP is NULL; if so, set error INVALID_ARGUMENT (1002) and goto error exit
4. **Row Validation**: Check if constr < 0 OR constr >= matrix->numConstrs; if violated, log "Row index %d out of range in coefficient query" with constr value, set error INVALID_INDEX (1006), goto error exit
5. **Column Validation**: Check if var < 0 OR var >= matrix->numVars; if violated, log "Column index %d out of range in coefficient query" with var value, set error INVALID_INDEX (1006), goto error exit
6. **CSC Access**: Read colLen[var] to get number of nonzeros in column, read colStart[var] to get starting index in sparse arrays
8. **Row Pointer**: Calculate pointer to row indices: rowPtr = &rowIndices[colStart[var]]
9. **Linear Search**: For i from 0 to colLen[var]-1, check if rowPtr[i] == constr; if match found, goto found label; if no match, goto not_found
10. **Not Found**: Set *valP = 0.0 and return SUCCESS
11. **Found Label**: Read raw coefficient: coeff = coeffValues[colStart[var] + i]
12. **Unscaling**: If matrix->colScale is non-NULL, compute unscaled value: coeff = coeff / (rowScale[constr] × colScale[var])
13. **Constraint Sense**: If sense[constr] == '>', negate coefficient: coeff = -coeff
14. **Variable Negation**: If matrix->isMaximize == 1 AND varNegated[var] == 1, negate coefficient: coeff = -coeff
15. **Output**: Set *valP = coeff and return SUCCESS
16. **Error Exit**: Log "Failed to retrieve coefficient" and return error code

### 4.3 Pseudocode

```
function get_coeff(model, constr, var, valP):
    status ← checkmodel(model)
    if status ≠ 0: return status

    if valP = NULL:
        return INVALID_ARGUMENT

    matrix ← model.matrix
    if matrix = NULL:
        return INVALID_DATA

    # Validate indices
    if constr < 0 ∨ constr ≥ matrix.numConstrs:
        log_error("Row index out of range")
        return INVALID_INDEX

    if var < 0 ∨ var ≥ matrix.numVars:
        log_error("Column index out of range")
        return INVALID_INDEX

    # CSC lookup
    colStartIdx ← matrix.colStart[var]
    colLen ← matrix.colLen[var]
    rowPtr ← &matrix.rowIndices[colStartIdx]

    # Search for row in column
    for i ← 0 to colLen-1:
        if rowPtr[i] = constr:
            coeff ← matrix.coeffValues[colStartIdx + i]
            goto found

    # Not found - sparse zero
    *valP ← 0.0
    return SUCCESS

found:
    # Apply transformations
    if matrix.colScale ≠ NULL:
        coeff ← coeff / (matrix.rowScale[constr] × matrix.colScale[var])

    if matrix.sense[constr] = '>':
        coeff ← -coeff

    if matrix.isMaximize = 1 ∧ matrix.varNegated[var] = 1:
        coeff ← -coeff

    *valP ← coeff
    return SUCCESS
```

### 4.4 Mathematical Foundation

The constraint matrix coefficient represents the contribution of variable `var` to constraint `constr`:

```
A[constr, var] × x[var]  (in constraint equation)
```

Transformations applied:

1. **Unscaling**: Internal scaling for numerical stability
   ```
   A_orig[i,j] = A_scaled[i,j] / (rowScale[i] × colScale[j])
   ```

2. **Constraint sense**: Internally stored as '<='
   ```
   If sense = '>=': presented_coeff = -internal_coeff
   If sense = '<=': presented_coeff = internal_coeff
   If sense = '=': presented_coeff = internal_coeff
   ```

3. **Variable negation**: Maximization converted to minimization
   ```
   If max problem and var is negated: presented_coeff = -internal_coeff
   ```

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) - validation fails or column is empty
- **Average case:** O(nnz_col) - linear search through column
- **Worst case:** O(nnz_col) - row at end of column or not found

Where:
- nnz_col = number of nonzeros in the queried column
- Could be O(log nnz_col) with binary search if rows are sorted

### 5.2 Space Complexity

- **Auxiliary space:** O(1) - only local variables
- **Total space:** O(1) - no allocations

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| Model validation fails | Various | Invalid model pointer |
| NULL valP | 1002 | Output pointer not provided |
| Matrix is NULL | 1003 | Model has no matrix data |
| Row index invalid | 1006 | constr out of bounds |
| Column index invalid | 1006 | var out of bounds |

### 6.2 Error Behavior

On error, the function logs a descriptive error message identifying the problem: which index is invalid and what the valid range is. The *valP output is never modified on error. The function returns immediately after validation errors without accessing matrix data.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Sparse zero | (i,j) not in sparse structure | Returns 0.0 |
| Explicit zero | (i,j) stored as 0.0 | Returns 0.0 |
| First nonzero | Row matches first entry in column | Fast path - first comparison |
| Last nonzero | Row matches last entry in column | Worst case - full scan |
| Empty column | Variable unused in all constraints | Returns 0.0 immediately |
| Dense column | Variable in all constraints | O(m) search where m = numConstrs |
| Scaled model | Scaling factors present | Returns unscaled value |
| '>=' constraint | Sense is '>' | Returns negated coefficient |
| Negated variable | Max problem with var negated | Returns negated coefficient |
| Combined transforms | Scaled '>' with negated var | Returns appropriately transformed value |

## 8. Thread Safety

**Thread-safe:** Yes (conditionally)

Multiple threads may safely read coefficients concurrently as long as no thread is modifying the model. All accessed data structures (CSC arrays, scaling, sense) are read-only during queries. No internal state is modified. The function is reentrant.

**Synchronization required:** Model must not be modified concurrently with reads

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_checkmodel | Validation | Validate model pointer |
| cxf_logError | Error | Log error messages |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| User application | External | Query individual coefficients |
| Matrix inspection | External | Analyze problem structure |
| Model transformation | External | Modify or validate constraints |
| Debugging tools | External | Examine specific coefficients |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_getconstrs | Related - retrieves full constraint rows (CSR) |
| cxf_getcols | Related - retrieves full variable columns (CSC) |
| cxf_chgcoeffs | Inverse - modifies coefficients |
| cxf_getdblattrarray | Related - retrieves constraint RHS |
| cxf_getcharattrarray | Related - retrieves constraint senses |
| cxf_x_getcoeff | Extended - same function with 64-bit indices |

## 11. Design Notes

### 11.1 Design Rationale

Single-coefficient access is designed for sparse random queries where full row/column retrieval would be wasteful. The function exploits column-major storage (CSC) for direct array access without intermediate buffering or copying.

Returning zero for absent entries follows sparse matrix conventions - simplifies user code by avoiding special "not found" handling. The transformation pipeline (unscale, sense adjust, variable negate) presents a natural mathematical view while maintaining internal efficiency.

Linear search is adequate for typical column sparsity (10-100 nonzeros per column). Binary search could optimize dense columns but adds complexity and conditional branching overhead that may not pay off for typical cases.

### 11.2 Performance Considerations

For querying many coefficients, use cxf_getconstrs or cxf_getcols for batch access - amortizes overhead. For sparse random queries, this function is efficient: O(nnz_col) search dominates. The transformation overhead (scaling, negation) is negligible - single FP operations.

Access patterns matter: querying many coefficients in the same column is cache-friendly (column data is contiguous). Querying many coefficients in the same row is cache-unfriendly (scattered across columns). For row-wise access patterns, use cxf_getconstrs instead.

### 11.3 Future Considerations

Potential enhancements: binary search for sorted row indices within columns, caching recently-accessed columns for repeated queries, batch coefficient query interface to reduce per-call overhead, and hints for expected access patterns.

## 12. References

- Convexfeld Optimizer Reference Manual - cxf_getcoeff documentation
- Compressed Sparse Column format specification
- Saad, Y. (2003). Iterative Methods for Sparse Linear Systems (CSC format)

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
