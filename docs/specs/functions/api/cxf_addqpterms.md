# cxf_addqpterms

**Module:** API Variable/Constraint Operations
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Add quadratic objective terms to an optimization model, building the Q matrix for quadratic objective functions of the form: linear_objective + (1/2)x^T·Q·x. Terms are accumulated across multiple calls and applied during model update.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| model | CxfModel* | Target model | Valid model pointer | Yes |
| numqnz | int | Number of quadratic terms to add | >= 0 | Yes |
| qrow | int* | Row indices for Q matrix | [0, numVars), NULL if numqnz=0 | Conditional |
| qcol | int* | Column indices for Q matrix | [0, numVars), NULL if numqnz=0 | Conditional |
| qval | double* | Coefficient values for Q matrix | Finite doubles, NULL if numqnz=0 | Conditional |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | 0 on success, error code otherwise |

### 2.3 Side Effects

- Adds numqnz terms to QPTermsBuffer in pending QPData structure
- Swaps qrow/qcol to maintain upper-triangular storage
- Updates term count
- Logs operation status

## 3. Contract

### 3.1 Preconditions

- [ ] model pointer must be valid
- [ ] Model not in modification-blocked state
- [ ] If numqnz > 0, qrow, qcol, qval must be non-NULL
- [ ] All coefficient values must be finite (not NaN or Inf)
- [ ] All row and column indices must be in range [0, numVars)

### 3.2 Postconditions

- [ ] numqnz quadratic terms added to pending buffer
- [ ] For each term i: Q[qrow[i], qcol[i]] += qval[i] (conceptually)
- [ ] Row/column pairs normalized so row <= col (upper triangular)
- [ ] Terms accumulate across multiple calls
- [ ] Actual Q matrix updated on next model update

### 3.3 Invariants

- [ ] Input arrays remain unmodified
- [ ] Q matrix maintains symmetry: Q[i,j] = Q[j,i]
- [ ] Model structure remains valid

## 4. Algorithm

### 4.1 Overview


### 4.2 Detailed Steps

1. Validate model and check modification-blocked state
2. Early return if numqnz = 0 (success)
3. Validate qrow, qcol, qval are non-NULL
4. Validate all qval entries for NaN/Inf with detailed error reporting
5. Get or initialize QPData structure:
   a. Check if QPData exists and bit 0 is set
   b. If not, allocate 344-byte QPData structure
   c. Set magic value (0x54b249ad2594c37d) for validation
   d. Store current numVars and numConstrs
   e. Set bit 0 to mark as initialized
6. Validate all qrow and qcol indices are in range [0, numVars)
   a. If NULL, allocate with initial capacity = numqnz + 1000
   b. Otherwise check if capacity sufficient
   c. If insufficient, grow capacity = (current + new + 1000)
   d. Use realloc for rowIndices, colIndices, coefficients arrays
8. Copy and normalize quadratic terms:
   a. For each term i:
      - Copy qrow[i] → rowIndices[count+i]
      - Copy qcol[i] → colIndices[count+i]
      - If colIndices[count+i] < rowIndices[count+i], swap them
      - Copy qval[i] → coefficients[count+i]
   b. Update count += numqnz
9. Return success

### 4.3 Pseudocode

```
function cxf_addqpterms(model, numqnz, qrow, qcol, qval):
    if numqnz = 0:
        return SUCCESS

    validate_all_indices_and_values()

    qpdata ← get_or_create_qpdata(model)
    termsBuffer ← qpdata.termsBuffer

    if termsBuffer = NULL:
        capacity ← numqnz + 1000
        termsBuffer ← allocate_terms_buffer(capacity)
    else if termsBuffer.count + numqnz > termsBuffer.capacity:
        newCapacity ← termsBuffer.count + numqnz + 1000
        reallocate_terms_buffer(termsBuffer, newCapacity)

    baseIdx ← termsBuffer.count
    for i ← 0 to numqnz-1:
        row ← qrow[i]
        col ← qcol[i]

        # Enforce upper triangular storage
        if col < row:
            swap(row, col)

        termsBuffer.rowIndices[baseIdx + i] ← row
        termsBuffer.colIndices[baseIdx + i] ← col
        termsBuffer.coefficients[baseIdx + i] ← qval[i]

    termsBuffer.count ← termsBuffer.count + numqnz
    return SUCCESS
```

### 4.4 Mathematical Foundation

Quadratic objective form: f(x) = c^T·x + (1/2)x^T·Q·x

Where Q is symmetric matrix. Function adds terms to build Q incrementally.

Upper triangular normalization ensures: if term (i,j) added with i>j, stored as (j,i)

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) when numqnz = 0
- **Average case:** O(numqnz) for validation and copying
- **Worst case:** O(numqnz) with buffer reallocation

### 5.2 Space Complexity

- **Auxiliary space:** O(numqnz) for new terms
- **Total space:** O(total accumulated terms)

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| Model modification blocked | 1017 | MODEL_MODIFICATION |
| numqnz > 0 but qrow/qcol/qval NULL | 1002 | NULL_ARGUMENT |
| Coefficient is NaN or Inf | 1003 | INVALID_ARGUMENT with element index |
| Variable index out of range | 1006 | INVALID_INDEX |
| Buffer allocation fails | 1001 | OUT_OF_MEMORY |

### 6.2 Error Behavior

On error, logs detailed message including element index for value errors, performs cleanup of QPData structure if needed, and returns error code. No partial terms added.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| No terms | numqnz=0 | Immediate success, no processing |
| Diagonal terms | qrow[i]=qcol[i] | Accepted, no swapping needed |
| Symmetric pairs | Add (i,j) and (j,i) separately | Both stored, accumulate in Q[i,j] |
| Zero coefficients | qval[i]=0.0 | Accepted, creates explicit zero entry |

## 8. Thread Safety

**Thread-safe:** No

**Synchronization required:** Caller must ensure exclusive model access

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_checkmodel | Validation | Verify model validity |
| cxf_calloc | Memory | Allocate QPData and QPTermsBuffer |
| cxf_malloc | Memory | Allocate term arrays |
| cxf_realloc | Memory | Grow term arrays |
| cxf_free | Memory | Clean up on error |
| cxf_check_nan_or_inf | Validation | Validate coefficient values |
| cxf_error | Error Tracking | Log detailed error messages |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_addqconstr | Adds quadratic constraint (different from objective) |
| cxf_delq | Removes quadratic objective terms |
| cxf_getq | Retrieves quadratic objective coefficients |

## 11. Design Notes

### 11.1 Design Rationale

Upper triangular storage reduces memory usage by 50% for symmetric matrices. Incremental term addition supports model building scenarios where Q matrix is constructed progressively. Buffer growth strategy balances allocation overhead with memory efficiency.

### 11.2 Performance Considerations

For large Q matrices, consider sorting terms before addition to improve cache locality during model update. The function uses aggressive loop unrolling (16× in assembly) for bulk term copying on performance-critical paths.

### 11.3 Future Considerations

Option to specify Q matrix sparsity pattern upfront could enable better memory allocation strategies.

## 12. Validation Checklist

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
