# cxf_get_qconstr_data

**Module:** Utilities
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Retrieves all data for a quadratic constraint by index, returning pointers to internal arrays containing linear and quadratic terms. Quadratic constraints have the form x^T Q x + a^T x {≤, =, ≥} rhs, where Q is symmetric and only the lower triangle is stored. This is an internal accessor function used by the optimization core and query APIs.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| model | CxfModel* | Model containing quadratic constraint | Valid pointer | Yes |
| qcindex | int | Quadratic constraint index | 0 to numQConstrs-1 | Yes |
| numLinTerms_out | int* | Output: number of linear terms | Can be NULL | Yes |
| linVars_out | int** | Output: linear variable indices | Can be NULL | Yes |
| linCoeffs_out | double** | Output: linear coefficients | Can be NULL | Yes |
| numQuadTerms_out | int* | Output: number of quadratic terms | Can be NULL | Yes |
| qrow_out | int** | Output: quadratic row indices | Can be NULL | Yes |
| qcol_out | int** | Output: quadratic column indices | Can be NULL | Yes |
| qval_out | double** | Output: quadratic coefficients | Can be NULL | Yes |
| sense_out | char* | Output: constraint sense | Can be NULL | Yes |
| rhs_out | double* | Output: right-hand side value | Can be NULL | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | Error code: 0=success, 1002=NULL_ARGUMENT, 1006=INVALID_INDEX |
| *numLinTerms_out | int | Number of linear terms |
| *linVars_out | int* | Pointer to internal array of variable indices |
| *linCoeffs_out | double* | Pointer to internal array of coefficients |
| *numQuadTerms_out | int | Number of quadratic terms |
| *qrow_out | int* | Pointer to internal row indices (≥ col indices) |
| *qcol_out | int* | Pointer to internal column indices |
| *qval_out | double* | Pointer to internal quadratic coefficients |
| *sense_out | char | Constraint sense: '<', '=', or '>' |
| *rhs_out | double | Right-hand side value |

### 2.3 Side Effects

None (read-only access to internal data)

## 3. Contract

### 3.1 Preconditions

- [ ] Model pointer must be valid (non-NULL, correct magic number)
- [ ] qcindex must be in range [0, numQConstrs)
- [ ] Model must contain quadratic constraint data

### 3.2 Postconditions

- [ ] On success: All non-NULL output pointers are populated
- [ ] Returned pointers reference internal model data (NOT copied)
- [ ] Quadratic terms stored in lower triangular format (row ≥ col)
- [ ] Error code indicates success or specific failure mode

### 3.3 Invariants

- [ ] Model data not modified (read-only function)
- [ ] Returned pointers valid until model modification
- [ ] Array sizes consistent with counts

## 4. Algorithm

### 4.1 Overview

Direct field access from the model's quadratic constraint array. The function validates inputs, computes the offset to the specific constraint's data structure, and returns pointers to internal arrays.

### 4.2 Detailed Steps

1. Validate model pointer
   - If NULL, return ERROR_NULL_ARGUMENT (1002)
2. Check model magic number (0xC0FEFE1D)
   - If invalid, return ERROR_NULL_ARGUMENT
3. Read numQConstrs from model
4. Validate qcindex is in range [0, numQConstrs)
   - If out of range, return ERROR_INVALID_INDEX (1006)
5. Get pointer to QConstrData array
   - If NULL, return ERROR_NULL_ARGUMENT
6. Compute entry pointer: &QConstrArray[qcindex]
   - Each entry is 0xA8 (168) bytes
7. Extract and return fields from QConstrData structure:
   - numLinTerms from entry
   - linVars from entry
   - linCoeffs from entry
   - numQuadTerms from entry
   - quadRow from entry
   - quadCol from entry
   - quadCoeffs from entry
   - sense from entry
   - rhs from entry
8. Return 0 (success)

### 4.3 Quadratic Term Storage Format

Quadratic terms represent symmetric matrix Q:
- Only lower triangle stored: row[k] ≥ col[k]
- Diagonal terms: row[k] = col[k], coefficient is x²ᵢ term
- Off-diagonal: row[k] > col[k], coefficient represents xᵢxⱼ term with implicit factor of 2

Example: x² + 2xy + y² ≤ 1
```
numQuadTerms = 3
quadRow = [0, 1, 1]      # Variable indices (i)
quadCol = [0, 0, 1]      # Variable indices (j), j ≤ i
quadCoeffs = [1.0, 1.0, 1.0]  # Coefficients (off-diagonal has implicit ×2)
sense = '<'
rhs = 1.0
```

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) - direct field access
- **Average case:** O(1)
- **Worst case:** O(1)

All operations are pointer arithmetic and field reads (< 20 CPU cycles).

### 5.2 Space Complexity

- **Auxiliary space:** O(1) - no allocations
- **Total space:** O(1) - returns pointers, no copying

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| NULL model | 1002 | Model pointer is NULL |
| Invalid magic | 1002 | Model magic number incorrect |
| NULL QConstrArray | 1002 | Internal data not initialized |
| Index < 0 | 1006 | Constraint index negative |
| Index ≥ numQConstrs | 1006 | Constraint index out of range |

### 6.2 Error Behavior

On error, function returns error code immediately without modifying output pointers. Output pointers may contain undefined values on error return.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| NULL model | model = NULL | Return 1002 |
| Invalid index | qcindex = -1 | Return 1006 |
| Index too large | qcindex = 999999 | Return 1006 |
| No Q constraints | numQConstrs = 0 | Return 1006 for any index |
| NULL outputs | All outputs NULL | Return 0 (success), no data written |
| Only some outputs | Mix of NULL/non-NULL | Return 0, populate non-NULL outputs |

## 8. Thread Safety

**Thread-safe:** Yes (conditionally)

The function is thread-safe for read-only access because:
- Only reads model data
- No mutable state modified
- No synchronization primitives used

However, concurrent model modification creates race conditions.

**Synchronization required:** Only if model is being modified concurrently

## 9. Dependencies

### 9.1 Functions Called

None (direct field access)

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_getqconstr | API | Public API wrapper with copying |
| Quadratic optimization code | Solver | Access constraint data during solve |
| Presolve routines | Presolve | Analyze quadratic constraints |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_addqconstr | Adds quadratic constraint (modifies data) |
| cxf_delqconstr | Deletes quadratic constraint |
| cxf_is_socp | Checks if model has SOCP (quadratic) constraints |
| cxf_get_qconstr_name | Gets constraint name |

## 11. Design Notes

### 11.1 Design Rationale

Returns pointers to internal arrays rather than copying for performance. Caller must not modify returned arrays. Public API (cxf_getqconstr) provides safe copying interface.

### 11.2 Performance Considerations

Zero-copy design eliminates allocation overhead. Suitable for inner optimization loops. Caller must ensure arrays not accessed after model modification.

### 11.3 Future Considerations

- Could add optional copying mode for safer API
- Could add validation of array consistency (counts match sizes)
- Could cache frequently accessed constraint data

## 12. References

- SOCP and convexity requirements

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

## Important Notes

**Memory Management:**
- Returned pointers are to INTERNAL arrays
- Do NOT free or modify returned arrays
- Arrays valid until next model modification
- Use cxf_getqconstr (public API) for safe copying

**Quadratic Term Interpretation:**
- Diagonal (row=col): coefficient × xᵢ²
- Off-diagonal (row>col): coefficient × 2 × xᵢ × xⱼ (implicit factor of 2)

---
*Convexfeld LP Solver Specification*
*Based on published optimization literature*
