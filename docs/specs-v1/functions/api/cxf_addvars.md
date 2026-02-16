# cxf_addvars

**Module:** API Variable/Constraint Operations
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Add multiple variables to an optimization model in a single batch operation using Compressed Sparse Column (CSC) format for constraint coefficients. This function provides efficient bulk variable addition and serves as the API-level entry point that wraps the internal 64-bit implementation.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| model | CxfModel* | Target model for variable addition | Valid model pointer | Yes |
| numvars | int | Number of variables to add | >= 0 | Yes |
| numnz | int | Total non-zero coefficients across all variables | >= 0 | Yes |
| vbeg | int* | CSC column start indices for each variable | [0, numnz], NULL if numnz<1 | Conditional |
| vind | int* | Constraint indices for non-zeros | [0, numConstrs) | Conditional |
| vval | double* | Coefficient values for non-zeros | Valid doubles | Conditional |
| obj | double* | Objective coefficients for each variable | Any valid doubles, NULL=all zeros | No |
| lb | double* | Lower bounds for each variable | Valid doubles, NULL=all 0.0 | No |
| ub | double* | Upper bounds for each variable | Valid doubles, NULL=all infinity | No |
| vtype | char* | Variable type for each variable | 'C','B','I','S','N', NULL=all continuous | No |
| varnames | const char** | Names for each variable | Any strings, NULL=default names | No |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | 0 on success, error code otherwise |

### 2.3 Side Effects

- Modifies model structure by adding numvars variables to pending changes buffer
- Allocates and populates temporary 64-bit index array
- Updates pending modification count in environment
- Logs operation status to error tracking system

## 3. Contract

### 3.1 Preconditions

- [ ] model pointer must be valid (passes cxf_checkmodel validation)
- [ ] If numnz > 0, vbeg must be non-NULL
- [ ] If numnz > 0, vind and vval must be non-NULL (unless model accepts NULL)
- [ ] vbeg[i] defines starting position of variable i's coefficients in vind/vval
- [ ] For variable i, coefficients are at indices [vbeg[i], vbeg[i+1]) or [vbeg[i], numnz) for last variable
- [ ] All constraint indices in vind must be valid
- [ ] All coefficient values in vval must be finite (not NaN/Inf)

### 3.2 Postconditions

- [ ] numvars new variables added to model's pending changes
- [ ] Variables appended in order as last columns when model is updated
- [ ] All specified constraint coefficients registered
- [ ] Objective coefficients, bounds, and types stored
- [ ] Error tracking updated with operation status
- [ ] Temporary 64-bit vbeg array allocated, populated, and freed

### 3.3 Invariants

- [ ] Input arrays remain unmodified
- [ ] Existing model structure unchanged until update operation
- [ ] Model remains in valid state regardless of success/failure

## 4. Algorithm

### 4.1 Overview

This function wraps the internal 64-bit variable addition implementation by converting the 32-bit vbeg array to 64-bit format. The conversion is necessary because modern Convexfeld supports models with over 2 billion non-zeros, which exceeds 32-bit index limits. The function validates inputs, allocates a temporary 64-bit array, performs the conversion with loop unrolling optimization, calls the internal implementation, and cleans up the temporary allocation.

### 4.2 Detailed Steps

1. Early exit if numvars < 1 (nothing to add, return success)
2. Validate model pointer and check for modification-blocked state
3. If numnz > 0 and vbeg is NULL, return NULL_ARGUMENT error
4. If numnz < 1 and vbeg is provided, proceed to convert it anyway
5. If vbeg conversion needed:
   a. Allocate 64-bit array of size numvars
   b. Convert 32-bit vbeg values to 64-bit using loop unrolling:
      - Process pairs of elements in main loop
      - Handle remaining odd element separately
6. Call internal 64-bit batch addition with converted vbeg64 array
7. Free temporary vbeg64 array
8. Log operation result to error tracking
9. Return status code from internal operation

### 4.3 Pseudocode

```
function cxf_addvars(model, numvars, numnz, vbeg, vind, vval, obj, lb, ub, vtype, varnames):
    if numvars < 1:
        return SUCCESS

    status ← validate_model(model)
    if status ≠ SUCCESS:
        return status

    vbeg64 ← NULL

    if numnz ≥ 1 AND vbeg = NULL:
        return ERROR_NULL_ARGUMENT

    if vbeg ≠ NULL:
        vbeg64 ← allocate_array(numvars × sizeof(int64_t))
        if vbeg64 = NULL:
            return ERROR_OUT_OF_MEMORY

        # Convert with loop unrolling for performance
        for i ← 0 to numvars step 2:
            vbeg64[i] ← sign_extend(vbeg[i])
            if i+1 < numvars:
                vbeg64[i+1] ← sign_extend(vbeg[i+1])

    status ← cxf_x_addvars(model, numvars, numnz, vbeg64, vind, vval, obj, lb, ub, vtype, varnames)

    if vbeg64 ≠ NULL:
        free(vbeg64)

    log_error(model, status)
    return status
```

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) when numvars < 1
- **Average case:** O(numvars + numnz) for vbeg conversion and coefficient processing
- **Worst case:** O(numvars + numnz) when all operations succeed

Where:
- numvars = number of variables to add
- numnz = total non-zero coefficients

### 5.2 Space Complexity

- **Auxiliary space:** O(numvars) for temporary 64-bit vbeg array
- **Total space:** O(numvars + numnz) including pending buffer storage

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| model is NULL or invalid | varies | cxf_checkmodel determines specific code |
| numnz > 0 but vbeg is NULL | 1002 | NULL_ARGUMENT - array required for nonzeros |
| vbeg64 allocation fails | 1001 | OUT_OF_MEMORY - cannot allocate conversion array |
| Internal batch operation fails | varies | Propagated from cxf_x_addvars |

### 6.2 Error Behavior

On error, the function ensures cleanup of the temporary vbeg64 array if it was allocated. The error tracking system is updated with the failure code. The model remains in a consistent state with no partial modifications applied.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| No variables | numvars=0 | Immediate success return, no processing |
| No nonzeros | numnz=0, vbeg=NULL | Variables added with no constraint participation |
| vbeg provided with numnz=0 | numnz=0, vbeg≠NULL | vbeg converted anyway, then passed to batch function |
| NULL optional arrays | obj=NULL, lb=NULL, etc. | Batch function applies defaults (0, 0, infinity, 'C', default names) |
| Single variable | numvars=1 | Works correctly, though cxf_addvar is more efficient |

## 8. Thread Safety

**Thread-safe:** No

Modification of model structures requires external synchronization. Concurrent calls on the same model will result in undefined behavior.

**Synchronization required:** Caller must ensure exclusive access to model during operation

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_checkmodel | Validation | Verify model pointer validity |
| cxf_alloc | Memory | Allocate temporary 64-bit vbeg array |
| cxf_x_addvars | Batch API (internal) | Perform actual addition with 64-bit indices |
| cxf_free | Memory | Deallocate temporary vbeg array |
| cxf_errorlog | Error Tracking | Record operation outcome |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| User code | Application | Standard API entry point for batch variable addition |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_addvar | Single-item variant - wraps this function with numvars=1 |
| cxf_x_addvars | Internal 64-bit implementation - wrapped by this function |
| cxf_delvars | Inverse operation - removes variables from model |

## 11. Design Notes

### 11.1 Design Rationale

The 32-bit to 64-bit conversion maintains API compatibility with older code while supporting modern large-scale models. The wrapper pattern isolates index size conversion from the core logic. Loop unrolling in the conversion reduces overhead for large numvars values.

### 11.2 Performance Considerations

The vbeg conversion adds minimal overhead (simple sign extension operation). For extremely large models with billions of variables, consider using cxf_x_addvars directly to avoid the conversion. The loop unrolling optimization reduces loop overhead by approximately 50% for the conversion step.

### 11.3 Future Considerations

If 32-bit index support is eventually deprecated, this function could become a simple alias to cxf_x_addvars with appropriate type casting.

## 12. References

- Convexfeld Optimizer Reference Manual: cxf_addvars function description
- CSC (Compressed Sparse Column) matrix format documentation

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
