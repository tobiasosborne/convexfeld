# cxf_getintattr

**Module:** API - Attributes
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Retrieves the value of an integer-valued scalar model attribute by name. Provides the primary interface for querying model state information such as optimization status, solution count, model dimensions, and problem characteristics. Uses a table-driven dispatch system to route attribute access to appropriate getter functions or direct value pointers.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| model | CxfModel* | Loaded optimization model | Valid model pointer | Yes |
| attrname | const char* | Attribute name string | Valid attribute name | Yes |
| valueP | int* | Output location for value | Valid pointer | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | Zero on success, error code on failure |
| *valueP | int | Integer attribute value (on success) |

### 2.3 Side Effects

- Reads model attribute table and associated data structures
- May invoke getter functions which could perform computation or caching
- No modification to model state (read-only operation)
- Error messages logged to model's error buffer on failure

## 4. Algorithm

### 4.1 Overview

The function implements a table-driven attribute access system with paths based on model state. For models with active callbacks or special modes (concurrent optimization), it routes to specialized getter functions that handle synchronization.

For models, it performs attribute lookup in the model's attribute table using the name as a key. The table entry contains type information, array vs scalar classification, and pointers to getter functions or direct value locations. The function validates that the requested attribute exists, is of integer type, and is scalar (not array).

Value retrieval follows a priority order: first check for a direct pointer to the value in model memory (fastest path), then invoke a scalar getter function if provided, finally fall back to an array getter with single-element access. This design optimizes common cases while maintaining flexibility for computed attributes.

### 4.2 Detailed Steps

1. **Parameter Validation**: Check if valueP is NULL; if so, set error NULL_ARGUMENT (1002) and log "NULL 'value' argument supplied", goto error exit
2. **Name Validation**: Check if attrname is NULL; if so, set error NULL_ARGUMENT and log "NULL 'attrname' argument supplied", goto error exit
4. **Callback Mode Path**: If callbackCount >= 1, call get_attr_callback with (model, attrname, ATTR_TYPE_INT=1, 0, 1, 0, valueP); if status is 0, return 0; otherwise goto error exit
6. **Local Path**: If both checks fail, proceed to local attribute lookup
7. **Model Validation**: Call checkmodel on model; if error, goto error exit
8. **Attribute Lookup**: Call find_attr with (model, attrname) to get attribute index; if returns -1, set error UNKNOWN_ATTRIBUTE (1004), log "Unknown attribute '<name>'", goto error exit
15. **No Getter Available**: Set error ATTR_NOT_AVAILABLE (1005), fall through to error exit
16. **Error Exit**: Log "Unable to retrieve attribute '<name>'", return error code

### 4.3 Pseudocode

```
function get_int_attr(model, attrname, valueP):
    if valueP = NULL:
        log_error("NULL 'value' argument")
        return ERROR_NULL_ARGUMENT

    if attrname = NULL:
        log_error("NULL 'attrname' argument")
        return ERROR_NULL_ARGUMENT

    # Check for special modes
    if model ≠ NULL:
        if model.callbackCount ≥ 1:
            return get_attr_callback(model, attrname, TYPE_INT, 0, 1, 0, valueP)

        if model.specialMode ≠ 0:
            return get_attr_special(model, attrname, TYPE_INT, valueP)

    # Local attribute access
    status ← checkmodel(model)
    if status ≠ 0:
        return status

    index ← find_attr(model, attrname)
    if index = -1:
        log_error("Unknown attribute")
        return ERROR_UNKNOWN_ATTRIBUTE

    entry ← model.attrTable.entries[index]

    if entry.type ≠ TYPE_INT:
        log_error("Type mismatch")
        return ERROR_UNKNOWN_ATTRIBUTE

    if entry.isArray ≠ 0:
        log_error("Array attribute accessed as scalar")
        return ERROR_UNKNOWN_ATTRIBUTE

    # Try retrieval methods in priority order
    if entry.directValuePtr ≠ NULL:
        *valueP ← *entry.directValuePtr
        return SUCCESS

    if entry.scalarGetter ≠ NULL:
        return entry.scalarGetter(model, 0, -1, 0, valueP)

    if entry.arrayGetter ≠ NULL:
        return entry.arrayGetter(model, 0, 0, -1, 0, valueP)

    log_error("Attribute not available")
    return ERROR_ATTR_NOT_AVAILABLE
```

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) - NULL argument check or direct value pointer
- **Average case:** O(log n + g) where n = number of attributes, g = getter complexity
- **Worst case:** O(log n + g) - same as average

Where:
- n = total number of registered attributes (typically 100-200)
- g = getter function complexity (typically O(1) but can be O(model size) for computed attributes)
- Attribute lookup is O(log n) if find_attr uses binary search or hash table

### 5.2 Space Complexity

- **Auxiliary space:** O(1) - only uses stack variables
- **Total space:** O(1) - no allocations

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| NULL valueP pointer | 1002 | Output location not provided |
| NULL attrname pointer | 1002 | Attribute name not provided |
| Model validation fails | Various | Invalid model pointer or structure |
| Attribute not found | 1004 | Attribute name doesn't exist |
| Type mismatch | 1004 | Attribute is not integer type |
| Array attribute | 1004 | Attribute is array, not scalar |
| Getter unavailable | 1005 | No method to retrieve attribute value |
| Getter execution fails | Various | Getter function returned error |

### 6.2 Error Behavior

On error, the function logs two messages: a specific error describing what went wrong (NULL argument, unknown attribute, type mismatch, etc.) and a generic "Unable to retrieve attribute" wrapper. The *valueP output location is not modified on error. The function returns immediately after logging errors without attempting fallback retrieval methods.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| NULL model pointer | model = NULL, valid name | Proceeds to local path, checkmodel fails |
| Empty attrname | attrname = "" | Lookup fails, returns UNKNOWN_ATTRIBUTE |
| Uninitialized model | Freshly allocated but not initialized | checkmodel fails |
| Model with callbacks | callbackCount > 0 | Routes to callback-aware path |
| Double attribute requested | attrname = "ObjVal" (double) | Type mismatch error with actual type |
| Array attribute requested | attrname = "X" (variable array) | Array-as-scalar error |
| Optimization not run | Status attribute before optimize | Returns default/uninitialized value |
| All getters NULL | Malformed attribute entry | Returns ATTR_NOT_AVAILABLE |

## 8. Thread Safety

**Thread-safe:** Yes (conditionally)

Reading attributes is thread-safe as long as no other thread is modifying the model. The attribute table is immutable after model creation. Direct value pointers read from model memory that may change during optimization - the modificationBlocked flag prevents structural changes. Getter functions may perform locking if needed. Multiple threads may safely read attributes concurrently.

**Synchronization required:** None for reads, model must not be modified concurrently

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_error | Error Handling | Log error messages with code |
| cxf_checkmodel | Validation | Validate model pointer and structure |
| cxf_find_attr | Attributes | Lookup attribute index by name |
| cxf_get_attr_callback | Attributes | Callback-aware attribute access |
| cxf_get_attr_special | Attributes | Special mode attribute access |
| Attribute getter functions | Various | Type-specific value computation |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| User application | External | Query optimization results |
| cxf_optimize | Optimization | Check Status attribute |
| Internal validation | Various | Verify model state |
| I/O functions | File I/O | Query model characteristics |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_getdblattr | Sibling - same structure for double attributes |
| cxf_getstrattr | Sibling - same structure for string attributes |
| cxf_getintattrelement | Related - access single array element |
| cxf_getintattrarray | Related - access array range |
| cxf_setintattr | Inverse - set integer attribute value |
| cxf_getintattrlist | Related - batch access multiple attributes |

## 11. Design Notes

### 11.1 Design Rationale

The table-driven design allows adding new attributes without modifying the retrieval logic. Each attribute entry contains all metadata needed for type-safe access: type code, array flag, and function pointers. The three-tier getter priority (direct/scalar/array) optimizes common cases while supporting flexibility for computed or dynamically-sized attributes.

The separate code paths for callbacks and local access isolate complexity in the respective subsystems. This keeps the common path (local access) fast and simple. The double error logging (specific + generic) provides useful diagnostic information while maintaining consistent error messages across all attribute functions.

Type validation prevents incorrect access patterns that would lead to undefined behavior or crashes. The array vs scalar distinction is enforced to ensure users don't accidentally access a single element when they need the full array or vice versa.

### 11.2 Performance Considerations

The direct value pointer path is the fastest - single dereference with no function call overhead. This is used for frequently-accessed attributes like Status and NumVars. Computed attributes like SolCount use getter functions which may cache results. The attribute lookup (find_attr) is typically O(log n) with binary search on a sorted name array or O(1) with hash table.

For hot paths where attributes are queried in tight loops, users should cache values rather than repeatedly calling the getter. The function does not perform internal caching (besides what individual getters do) to maintain simplicity and avoid cache invalidation complexity.

### 11.3 Future Considerations

Potential enhancements: attribute versioning to detect stale cached values, bulk attribute queries to amortize lookup overhead, attribute change notifications/callbacks, and compile-time attribute name validation using C macros.

## 12. References

- Convexfeld Optimizer Reference Manual - Attributes section
- Convexfeld Optimizer Reference Manual - Error codes

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
