# cxf_getdblparam

**Module:** Parameters
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Retrieves the current value of a double-precision floating-point parameter from a Convexfeld environment. Parameters control solver behavior such as tolerance settings, time limits, and convergence criteria. Each environment maintains its own copy of parameter values that can be queried independently.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| env | CxfEnv* | Environment containing the parameter | Valid environment pointer | Yes |
| paramname | const char* | Name of the parameter to query | Non-null, valid parameter name | Yes |
| valueP | double* | Pointer to store retrieved value | Non-null pointer | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | Error code (0 on success, non-zero on failure) |
| *valueP | double | Retrieved parameter value (only valid on success) |

### 2.3 Side Effects

Updates internal environment status field to reflect the success or failure of the operation. Does not modify parameter values or other environment state.

## 3. Contract

### 3.1 Preconditions

- [ ] Environment pointer must be valid and initialized
- [ ] Environment must have an initialized parameter table
- [ ] Parameter name pointer must be non-null
- [ ] Output pointer must be non-null and point to valid memory
- [ ] Parameter name must refer to a double-typed parameter

### 3.2 Postconditions

- [ ] On success: output location contains the current parameter value
- [ ] On success: return value is 0
- [ ] On failure: appropriate error code is returned
- [ ] On failure: error message is recorded in environment
- [ ] Environment status field reflects the operation result

### 3.3 Invariants

- [ ] Environment parameter table is not modified
- [ ] Parameter values in environment are not modified
- [ ] Input parameter name string is not modified
- [ ] Environment structure integrity is maintained

## 4. Algorithm

### 4.1 Overview

The function performs a table-driven parameter lookup using a parameter registry maintained by the environment. Each parameter entry in the table contains metadata including the parameter's name, type, and storage location offset. The function validates the environment, normalizes the parameter name for lookup, searches the parameter table, validates type compatibility, and reads the value from the calculated storage location.

The parameter storage uses a base-plus-offset addressing scheme where all parameter values are stored in a contiguous region of the environment structure starting at a fixed base offset, with each parameter at its specific offset from that base.

### 4.2 Detailed Steps

1. Validate that the environment pointer is valid and properly initialized
2. Verify that the environment has a non-null parameter table pointer
3. Verify that the parameter table has been initialized (non-null table reference)
4. Copy the input parameter name to a local buffer for normalization
5. Normalize the parameter name (case handling, whitespace removal, etc.)
6. Search the parameter table for an entry matching the normalized name
7. If no matching entry is found, record an error and return failure code
8. Retrieve the parameter metadata entry from the table
9. Validate that the parameter's type field indicates it is a double (type code 2)
10. If type mismatch, record a type error and return failure code
11. Check that the parameter's offset field is non-zero (zero indicates unavailable parameter)
12. If offset is zero, record an error and return failure code
13. Calculate the memory address: environment base + parameter storage base offset + parameter-specific offset
14. Read the double value from the calculated address
15. Store the value in the output location
16. Update environment status to success
17. Return success code (0)

### 4.3 Mathematical Foundation

The addressing calculation follows the formula:

```
address = env_base + PARAM_STORAGE_BASE + param_entry.offset
```

Where:
- env_base = base address of the CxfEnv structure
- PARAM_STORAGE_BASE = fixed offset to parameter storage region (0x1fc0)
- param_entry.offset = parameter-specific offset from table entry

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) - if parameter hash table provides direct access
- **Average case:** O(log n) - if parameter table uses binary search
- **Worst case:** O(n) - if parameter table uses linear search

Where:
- n = number of parameters in the parameter table (typically ~100-200)

### 5.2 Space Complexity

- **Auxiliary space:** O(1) - fixed-size local buffer for parameter name
- **Total space:** O(1) - no dynamic allocation

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| Invalid environment pointer | Environment-specific | Environment validation function returns error |
| Null parameter table | 1007 | Environment has no parameter table initialized |
| Null parameter name | 1007 | Parameter name pointer is null |
| Parameter not found | 1007 | No entry in table matches the parameter name |
| Type mismatch | 1007 | Parameter exists but is not of type double |
| Zero offset | 1007 | Parameter entry has invalid offset (unavailable) |

### 6.2 Error Behavior

On any error condition:
1. An error message is recorded in the environment's error buffer
2. The environment status field is updated with the error code
3. The function returns the error code
4. The output value pointer is not modified (contents undefined)

The function maintains environment consistency on error - no partial state changes occur.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Case-insensitive name | "feasibilitytol" vs "FeasibilityTol" | Both should work (after normalization) |
| Unknown parameter | "NonExistentParam" | Return error 1007 |
| Integer parameter name | "Method" (an int param) | Return error 1007 (type mismatch) |
| String parameter name | "LogFile" (a string param) | Return error 1007 (type mismatch) |
| Empty environment | env with no param table | Return error 1007 |
| Read-only parameter | Any valid double param | Succeeds (all params readable) |

## 8. Thread Safety

**Thread-safe:** Conditionally

The function is thread-safe for concurrent reads of the same environment, as it only reads from the environment structure and parameter table without modifying them. However:

- Concurrent reads while another thread modifies parameters: undefined behavior
- Concurrent reads are safe only if no thread is modifying parameter values
- Parameter table structure is immutable after environment creation (safe)
- Parameter value storage may be modified by set operations (requires synchronization)

**Synchronization required:** Caller must ensure no concurrent parameter modifications occur during the read operation if thread safety is required.

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_check_env | Validation | Validates environment pointer and magic number |
| cxf_normalize_param_name | Parameters | Normalizes parameter name for lookup |
| cxf_find_param | Parameters | Searches parameter table by name |
| cxf_error | Error Handling | Records error message in environment |
| cxf_env_set_status | Error Handling | Updates environment status field |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_get_feasibility_tol | Parameters | Wrapper to get FeasibilityTol |
| cxf_get_optimality_tol | Parameters | Wrapper to get OptimalityTol |
| cxf_getenv + parameter query | API | User code querying parameters |
| Solver initialization | LP Core | Reading solver control parameters |
| Barrier solver setup | Barrier | Reading convergence tolerances |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_setdblparam | Inverse operation - sets double parameter value |
| cxf_getintparam | Parallel operation for integer parameters |
| cxf_getstrparam | Parallel operation for string parameters |
| cxf_getdblparaminfo | Metadata query - min/max/default for parameter |
| cxf_resetparams | Resets all parameters to defaults |

## 11. Design Notes

### 11.1 Design Rationale

The table-driven design allows for:
- Centralized parameter metadata management
- Type safety through runtime type checking
- Efficient storage using struct offsets rather than hash maps
- Easy addition of new parameters without code changes
- Consistent error handling across all parameter operations

The base-plus-offset addressing scheme minimizes memory overhead while providing O(1) access once the parameter entry is found.

### 11.2 Performance Considerations

- Local buffer copy prevents potential security issues with untrusted parameter names
- Parameter lookups may be called frequently during solver initialization
- Consider caching frequently-used parameters in solver state structures
- Type validation prevents crashes from C type system bypass attempts

### 11.3 Future Considerations

- Parameter table search could be optimized with hash table instead of linear/binary search
- Parameter name normalization could be skipped if caller guarantees canonical names
- Thread-safe version could use read-write locks for concurrent access

## 12. References

- Standard optimization parameter conventions (tolerances, limits)
- IEEE 754 double precision format for understanding value representation

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
