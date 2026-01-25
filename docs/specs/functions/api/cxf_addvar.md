# cxf_addvar

**Module:** API Variable/Constraint Operations
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Add a single variable to an optimization model with optional constraint coefficients. This convenience function wraps the bulk variable addition interface to simplify the common case of adding one variable at a time.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| model | CxfModel* | Target model for variable addition | Valid model pointer | Yes |
| numnz | int | Count of non-zero constraint coefficients | >= 0 | Yes |
| vind | int* | Constraint indices with non-zero coefficients | [0, numConstrs), NULL if numnz=0 | No |
| vval | double* | Coefficient values for specified constraints | Valid doubles, NULL if numnz=0 | No |
| obj | double | Objective function coefficient | Any valid double | Yes |
| lb | double | Lower bound on variable value | Any valid double, -CXF_INFINITY for unbounded | Yes |
| ub | double | Upper bound on variable value | Any valid double, CXF_INFINITY for unbounded | Yes |
| vtype | char | Variable domain type | 'C', 'B', 'I', 'S', 'N' | Yes |
| varname | const char* | Human-readable variable identifier | Any string, NULL for default | No |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | 0 on success, error code otherwise |

### 2.3 Side Effects

- Modifies model structure by adding one variable to pending changes buffer
- Updates pending modification count in environment
- Logs operation status to error tracking system

## 3. Contract

### 3.1 Preconditions

- [ ] model pointer must be valid (passes cxf_checkmodel validation)
- [ ] If numnz > 0, vind and vval arrays must be non-NULL
- [ ] vind array must contain numnz valid constraint indices
- [ ] vval array must contain numnz valid coefficient values
- [ ] Variable type character must be one of: 'C', 'B', 'I', 'S', 'N'

### 3.2 Postconditions

- [ ] One new variable added to model's pending changes
- [ ] Variable will be appended as the last column when model is updated
- [ ] Specified constraint coefficients registered for the new variable
- [ ] Objective coefficient and bounds stored for the new variable
- [ ] Error tracking updated with operation status

### 3.3 Invariants

- [ ] Input arrays (vind, vval, varname) remain unmodified
- [ ] Existing model structure unchanged until update operation
- [ ] Model remains in valid state regardless of success/failure

## 4. Algorithm

### 4.1 Overview

The function acts as a thin wrapper around the batch variable addition interface. It performs these key steps: validates the model, allocates a temporary single-element array to represent the column start position, forwards all parameters to the bulk addition function (converting scalars to single-element arrays via stack references), cleans up the temporary allocation, and logs the result.

The design follows a common pattern in the API where single-item operations wrap their batch counterparts. The wrapper handles the complexity of converting scalar parameters (obj, lb, ub, vtype, varname) into the array format expected by the batch interface.

### 4.2 Detailed Steps

1. Validate model pointer and check for modification-blocked state
2. Allocate temporary int64_t array of size 1 for column begin index
3. Initialize the begin index array with value 0 (coefficients start at position 0)
4. Call batch variable addition function with numvars=1, passing:
   - The temporary begin index array
   - The provided constraint index/value arrays
   - References to scalar parameters (obj, lb, ub, vtype, varname) as single-element arrays
5. Deallocate the temporary begin index array
6. Log the operation result to error tracking
7. Return the status code from the batch operation

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) for validation + allocation + call overhead
- **Average case:** O(numnz) for copying numnz coefficients in batch function
- **Worst case:** O(numnz) when all operations succeed

Where:
- numnz = number of non-zero constraint coefficients for this variable

### 5.2 Space Complexity

- **Auxiliary space:** O(1) for temporary begin index array
- **Total space:** O(numnz) including coefficient storage in pending buffer

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| model is NULL or invalid | varies | cxf_checkmodel determines specific code |
| Memory allocation fails for begin array | 1001 | Cannot allocate 8-byte temporary array |
| Batch operation fails | varies | Propagated from cxf_x_addvars |
| numnz > 0 but vind/vval NULL | 1002 | NULL pointer when data expected |
| Invalid constraint index | 1006 | vind[i] outside valid range |

### 6.2 Error Behavior

On error, the function ensures cleanup of any allocated resources before returning. The temporary begin index array is freed if allocated. The error tracking system is updated with the failure code. The model remains in a consistent state with no partial modifications applied.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| No coefficients | numnz=0, vind=NULL, vval=NULL | Success, variable added with no constraint participation |
| Binary variable | vtype='B', lb=0.0, ub=1.0 | Binary domain enforced, bounds may be adjusted |
| Unbounded above | ub=CXF_INFINITY | No upper bound constraint |
| Unbounded below | lb=-CXF_INFINITY | No lower bound constraint |
| NULL variable name | varname=NULL | Default name generated (e.g., "C0", "C1") |

## 8. Thread Safety

**Thread-safe:** No

Modification of model structures requires external synchronization. Concurrent calls on the same model will result in undefined behavior.

**Synchronization required:** Caller must ensure exclusive access to model during operation

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_checkmodel | Validation | Verify model pointer validity and structure |
| cxf_alloc | Memory | Allocate temporary begin index array |
| cxf_x_addvars | Batch API | Perform actual variable addition with 64-bit indices |
| cxf_free | Memory | Deallocate temporary begin index array |
| cxf_errorlog | Error Tracking | Record operation outcome |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| User code | Application | Standard API entry point for single variable addition |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_addvars | Batch variant - adds multiple variables efficiently |
| cxf_x_addvars | Internal 64-bit implementation - wrapped by this function |
| cxf_delvars | Inverse operation - removes variables from model |
| cxf_chgcoeff | Modifies existing matrix coefficients after variable exists |

## 11. Design Notes

### 11.1 Design Rationale

The wrapper pattern simplifies the API for common single-variable additions while reusing the thoroughly tested batch implementation. This avoids code duplication and ensures consistent behavior between single and batch operations. The temporary allocation overhead is negligible for typical use cases.

### 11.2 Performance Considerations

For adding multiple variables, prefer cxf_addvars to avoid repeated function call overhead and allocations. The single-variable wrapper is optimized for code clarity rather than bulk performance. Each call allocates and frees an 8-byte temporary array, which is acceptable for individual additions but wasteful for bulk operations.

### 11.3 Future Considerations

None identified. The wrapper pattern is stable and unlikely to change.

## 12. References

- Convexfeld Optimizer Reference Manual: cxf_addvar function description
- Internal documentation on 64-bit index support for large models

## 13. Validation Checklist

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
