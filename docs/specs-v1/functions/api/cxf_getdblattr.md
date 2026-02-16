# cxf_getdblattr

**Module:** API - Attributes
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Retrieves the value of a double-precision floating-point scalar model attribute by name. Provides access to continuous-valued model characteristics such as objective values, optimality gaps, iteration counts, runtime statistics, and mathematical properties like condition numbers. Uses the same table-driven dispatch system as integer attributes but with double-specific type validation and value handling.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| model | CxfModel* | Loaded optimization model | Valid model pointer | Yes |
| attrname | const char* | Attribute name string | Valid attribute name | Yes |
| valueP | double* | Output location for value | Valid pointer | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | Zero on success, error code on failure |
| *valueP | double | Double-precision attribute value (on success) |

### 2.3 Side Effects

- Reads model attribute table and associated data structures
- May invoke getter functions which could perform computation or caching
- No modification to model state (read-only operation)
- Error messages logged to model's error buffer on failure

## 3. Contract

### 3.1 Preconditions

- [ ] Model pointer must be valid (or NULL for error path)
- [ ] attrname must point to null-terminated string
- [ ] valueP must point to writable double storage
- [ ] Attribute name must correspond to a double-typed attribute
- [ ] If requesting solution attributes (ObjVal, etc.), optimization must have completed successfully

### 3.2 Postconditions

- [ ] On success, *valueP contains the attribute value
- [ ] On error, *valueP is not modified
- [ ] Error code and message are logged on failure
- [ ] Function always returns (no blocking or exceptions)

### 3.3 Invariants

- [ ] Model structure remains unmodified
- [ ] Attribute table remains unmodified
- [ ] Environment state remains unchanged

## 4. Algorithm

### 4.1 Overview

The function implements the same three-path dispatch strategy as cxf_getintattr: callback mode, special mode (remote/concurrent), and local mode. The core difference is type validation for ATTR_TYPE_DOUBLE (2) instead of ATTR_TYPE_INT (1), and double-pointer handling throughout.

For local access, attribute lookup by name returns an index into the attribute table. The entry contains type metadata and getter methods. After validating double type and scalar classification, value retrieval follows priority: direct pointer (reading from model memory), scalar getter function (computed value), array getter with single-element access (fallback).


### 4.2 Detailed Steps

1. **Parameter Validation**: Check if valueP is NULL; if so, set error NULL_ARGUMENT (1002), log "NULL 'value' argument supplied", goto error exit
2. **Name Validation**: Check if attrname is NULL; if so, set error NULL_ARGUMENT, log "NULL 'attrname' argument supplied", goto error exit
4. **Callback Mode Path**: If callbackCount >= 1, call get_attr_callback with (model, attrname, ATTR_TYPE_DOUBLE=2, 0, 1, 0, valueP); if status is 0, return 0; otherwise goto error exit
6. **Local Path**: Proceed to local attribute lookup
7. **Model Validation**: Call checkmodel on model; if error, goto error exit
8. **Attribute Lookup**: Call find_attr with (model, attrname); if returns -1, set error UNKNOWN_ATTRIBUTE (1004), log "Unknown attribute '<name>'", goto error exit
9. **Table Entry Access**: Retrieve entry pointer from attrTablePtr->entries[index] (72-byte stride)
15. **No Getter**: Set error ATTR_NOT_AVAILABLE (1005), fall through
16. **Error Exit**: Log "Unable to retrieve attribute '<name>'", return error code

### 4.3 Pseudocode

```
function get_double_attr(model, attrname, valueP):
    if valueP = NULL:
        return error_null_arg("value")

    if attrname = NULL:
        return error_null_arg("attrname")

    # Handle special modes
    if model ≠ NULL:
        if model.callbackCount ≥ 1:
            return get_attr_callback(model, attrname, TYPE_DOUBLE, 0, 1, 0, valueP)

        if model.specialMode ≠ 0:
            return get_attr_special(model, attrname, TYPE_DOUBLE, valueP)

    # Local access path
    status ← checkmodel(model)
    if status ≠ 0:
        return status

    index ← find_attr(model, attrname)
    if index = -1:
        return error_unknown_attr(attrname)

    entry ← model.attrTable.entries[index]

    if entry.type ≠ TYPE_DOUBLE:
        return error_type_mismatch(attrname, "double", entry.type)

    if entry.isArray ≠ 0:
        return error_array_as_scalar(attrname)

    # Priority-based retrieval
    if entry.directValuePtr ≠ NULL:
        *valueP ← *entry.directValuePtr
        return SUCCESS

    if entry.scalarGetter ≠ NULL:
        return entry.scalarGetter(model, 0, -1, 0, valueP)

    if entry.arrayGetter ≠ NULL:
        return entry.arrayGetter(model, 0, 0, -1, 0, valueP)

    return ERROR_ATTR_NOT_AVAILABLE
```

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) - parameter validation fails or direct value pointer
- **Average case:** O(log n + g) where n = attribute count, g = getter complexity
- **Worst case:** O(log n + g)

Where:
- Attribute lookup is O(log n) with binary search or O(1) with hash table
- Getter complexity g is typically O(1) for cached values, can be O(model size) for computed attributes like condition numbers

### 5.2 Space Complexity

- **Auxiliary space:** O(1) - stack-local variables only
- **Total space:** O(1) - no heap allocations

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| NULL valueP | 1002 | Output pointer not provided |
| NULL attrname | 1002 | Attribute name not provided |
| Invalid model | Various | Model validation fails |
| Unknown attribute | 1004 | Attribute name not in table |
| Type mismatch | 1004 | Attribute is not double type (e.g., integer) |
| Array attribute | 1004 | Tried to access array as scalar |
| Getter unavailable | 1005 | No retrieval method configured |
| Getter fails | Various | Getter function returns error |

### 6.2 Error Behavior

The function logs detailed error messages identifying the specific problem: NULL pointer, unknown name, type mismatch showing expected vs actual type, array vs scalar conflict. A second generic error message "Unable to retrieve attribute" provides a consistent wrapper. The output valueP is never modified on error, leaving caller's memory unchanged.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Integer attribute | attrname = "Status" (int) | Type mismatch error |
| Array attribute | attrname = "X" (double array) | Array-as-scalar error |
| Undefined value | Attribute set to cxf__UNDEFINED | Returns special value (1e101) |
| NaN value | Computed attribute yields NaN | NaN propagates to caller |
| Infinity | Unbounded objective | Returns +inf or -inf |
| No solution | ObjVal before optimization | May return undefined or error |
| Callback mode | Active callbacks | Routes through callback-aware path |

## 8. Thread Safety

**Thread-safe:** Yes (conditionally)

Multiple threads may safely read attributes concurrently as long as no thread is modifying the model. The attribute table is read-only. Direct value pointers read from model memory that may change during optimization - use modificationBlocked to prevent changes. Getter functions may implement internal locking if needed.

**Synchronization required:** Model must not be modified concurrently with reads

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_error | Error | Log error messages |
| cxf_checkmodel | Validation | Validate model structure |
| cxf_find_attr | Attributes | Name-to-index lookup |
| cxf_get_attr_callback | Attributes | Callback-aware access |
| cxf_get_attr_special | Attributes | Special mode access |
| Attribute getter functions | Various | Compute attribute values |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| User application | External | Query solution quality |
| Post-optimization analysis | External | Extract runtime statistics |
| cxf_optimize | Optimization | Internal checks |
| Callbacks | User code | Monitor progress |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_getintattr | Sibling - same design for integers |
| cxf_getstrattr | Sibling - same design for strings |
| cxf_getdblattrelement | Related - array element access |
| cxf_getdblattrarray | Related - array range access |
| cxf_setdblattr | Inverse - set double attribute |
| cxf_getdblattrlist | Related - batch access |

## 11. Design Notes

### 11.1 Design Rationale

The function design is identical to cxf_getintattr except for type validation and pointer types. This consistency makes the API predictable and maintainable. The table-driven approach allows adding attributes without code changes to getters. Type safety is enforced at runtime through table metadata.

Double attributes often represent solution quality (objective value, gap), runtime statistics (elapsed time, iterations), or mathematical properties (condition number). These may be undefined before optimization completes or for infeasible models. The function correctly propagates special values like cxf__UNDEFINED (1e101) and IEEE special values (NaN, infinity) to the caller.

### 11.2 Performance Considerations

Direct value pointers provide O(1) access for frequently-used attributes. Computed attributes like MIPGap use cached values recalculated only when solution changes. The attribute lookup overhead is amortized for batch queries - users querying many attributes should consider cxf_getdblattrlist.

For attributes requiring expensive computation (exact condition number via SVD), the getter may cache results and return cached value on subsequent calls. The function itself doesn't implement caching to maintain simplicity.

### 11.3 Future Considerations

Potential enhancements: support for custom user-defined double attributes, attribute dependencies (query triggers automatic computation of related attributes), and precision control for high-accuracy requirements.

## 12. References

- Convexfeld Optimizer Reference Manual - Attributes section
- Convexfeld documentation on solution attributes
- IEEE 754 floating-point standard for special value handling

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
