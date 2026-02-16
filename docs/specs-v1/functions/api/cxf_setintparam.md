# cxf_setintparam

**Module:** API Parameters
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Sets the value of an integer-valued parameter in a Convexfeld environment. Parameters control optimizer behavior such as threading, logging, solution limits, and algorithmic choices. The function validates parameter bounds, handles special parameters with restricted modification rules, and propagates changes to recording systems if enabled.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| env | CxfEnv* | Environment whose parameter will be modified | Valid environment pointer | Yes |
| paramname | const char* | Name of the parameter to set | Any registered parameter name | Yes |
| newvalue | int | Desired new value for the parameter | Parameter-specific min/max | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | 0 on success, error code on failure |

### 2.3 Side Effects

- Modifies parameter value stored in environment structure
- Updates parameter flags (user-modified, set, clears default flag)
- May log parameter change message to console or log file
- Records parameter change if recording is enabled
- May trigger one-time warnings about environment hierarchy or parameter scope

## 3. Contract

### 3.1 Preconditions

- Environment pointer must be valid and properly initialized
- Function must not be called from within a callback
- Parameter name must not be NULL
- Environment must have a valid parameter table

### 3.2 Postconditions

- If successful, parameter value is updated in environment structure
- Parameter modification flags are set appropriately
- If logging enabled, parameter change is logged
- If recording enabled, change is recorded

### 3.3 Invariants

- Parameter name is not modified
- Environment parameter table structure remains unchanged
- Other parameters are not affected
- Environment remains in consistent state even on error

## 4. Algorithm

### 4.1 Overview

The function implements a multi-stage parameter setting process with extensive validation and special-case handling. First, it validates the environment and ensures the function is not being called from within a callback (which is prohibited). Next, it normalizes the parameter name and looks it up in the environment's parameter table, which is implemented as a hash table for efficient access.

After locating the parameter descriptor, the function validates that the parameter type is indeed integer and that the parameter has a valid storage offset. It then sets internal flags marking the parameter as user-modified and clearing the default flag.

The core update logic only proceeds if the new value differs from the current value. The function checks whether the parameter is fixed (immutable) or requires environment to be unstarted. It clamps the input value to a safe range of negative two billion to positive two billion to prevent integer overflow, then validates against the parameter's specific minimum and maximum bounds.

Several special parameters receive custom handling: "Record" must be set immediately after environment creation. The function manages environment hierarchy warnings, notifying users when changing master environment parameters after models have been created.

Finally, the function logs the parameter change (unless suppressed), formats the change for recording systems if enabled, and updates internal status tracking.

### 4.2 Detailed Steps

1. Validate the environment pointer is valid and active
2. Check that function is not being called from within a callback (error if so)
3. Verify environment has a valid parameter table with entries
4. Normalize parameter name (case-insensitive, remove underscores/whitespace)
5. Look up parameter in hash table by normalized name
6. If not found, return unknown parameter error
7. Retrieve parameter descriptor from table at calculated offset
8. Validate parameter type is integer (type code 1)
9. Validate parameter has non-zero storage offset
10. Get parameter ID and flags from descriptor
11. Set parameter flags: mark as user-modified, set, clear default
12. Compare new value with current value; skip update if identical
13. Check if parameter is fixed (error if so)
14. Check if parameter requires environment to be unstarted (error if started)
15. Clamp new value to range [-2000000000, 2000000000]
16. Convert clamped value to double for comparison
17. Validate value against parameter-specific minimum bound
18. Validate value against parameter-specific maximum bound
19. Check value against parameter limit threshold
20. Handle environment hierarchy (master vs child environments)
21. Issue warning if changing master environment after models exist
22. Issue warning if changing master-only parameter on child environment
23. Handle special "Record" parameter (must set immediately after creation)
24. Update parameter value in environment storage
25. Format log message based on parameter type and hidden flag
26. Log parameter change unless suppressed or silent flag set
27. Propagate change to recording system if active
28. Update environment status and finalize operation

### 4.3 Mathematical Foundation

Parameter value validation:

For integer parameter p with bounds [min_p, max_p] and limit L_p:
- Clamp input: v_clamped = max(-2×10^9, min(2×10^9, v_input))
- Range check: min_p ≤ v_clamped ≤ max_p (error if violated)
- Limit check: v_clamped ≤ L_p (warning if violated, may suppress)

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) - Parameter found in hash table, value unchanged
- **Average case:** O(1) - Hash table lookup + constant operations
- **Worst case:** O(n) - Hash collision resolution, where n is collision chain length

Where:
- n = number of parameters with same hash (typically 1-3)

### 5.2 Space Complexity

- **Auxiliary space:** O(1) - Fixed-size buffers for name normalization
- **Total space:** O(1) - No dynamic allocation

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| Environment invalid or NULL | Implementation-specific | Environment validation failure |
| Called from within callback | 0x271b (1011) | Function not allowed in callback context |
| Parameter name not found | 0x2717 (1007) | Unknown parameter name |
| Wrong parameter type | 0x2717 (1007) | Parameter is not integer type |
| Parameter offset is zero | 0x2717 (1007) | Parameter not available/configured |
| Parameter is fixed | 0x2713 (1003) | Parameter cannot be modified |
| Environment already started | 0x2713 (1003) | Parameter requires pre-start modification |
| Value below minimum | 0x2718 (1008) | Value below parameter minimum bound |
| Value above maximum | 0x2718 (1008) | Value above parameter maximum bound |

### 6.2 Error Behavior

On error, the function:
- Records detailed error message in environment error buffer
- Does not modify parameter value or flags
- Propagates error code through finalization function
- Leaves environment in consistent state (no partial updates)
- Returns error code to caller

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Value already set | newvalue == current | No operation, success returned immediately |
| Value at minimum | newvalue == min_value | Accepted if within all bounds |
| Value at maximum | newvalue == max_value | Accepted if within all bounds |
| Value exceeds limit | newvalue > limit_value | Warning logged, value may be accepted (depends on environment) |
| Extreme positive value | newvalue = 2147483647 | Clamped to 2000000000 |
| Extreme negative value | newvalue = -2147483648 | Clamped to -2000000000 |
| NULL parameter name | paramname = NULL | Unknown parameter error |
| Case variant name | "THREADS" vs "threads" | Both accepted (case-insensitive) |
| First parameter set | No prior modifications | Default flag cleared, user-modified flag set |
| Recording parameter | "Record" | Must be set immediately after env creation |

## 8. Thread Safety

**Thread-safe:** Conditionally

Each CxfEnv should only be accessed from a single thread. If multiple threads share an environment, external synchronization is required. The function itself does not acquire locks but may call internal functions that do. Parameter table structure is immutable after initialization, making reads thread-safe, but parameter value modifications are not atomic.

**Synchronization required:** External locking if environment shared across threads

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| ValidateEnvironment | Environment | Verify environment pointer validity |
| CheckIfInCallback | Callback | Detect callback context |
| SetErrorMessage | Error Handling | Record formatted error message |
| LogMessage | Logging | Output parameter change message |
| NormalizeParameterName | Parameters | Convert name to canonical form |
| LookupParameterByName | Parameters | Hash table parameter lookup |
| PropagateToRemote | Distributed | Send parameter to remote environments |
| PropagateToRecording | Recording | Record parameter change |
| FinalizeParameterOperation | Environment | Update environment status |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_setparam | Parameters | Generic parameter setter |
| cxf_readparams | I/O | Loading parameters from file |
| User code | External | Direct API usage |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_getintparam | Inverse operation - retrieves integer parameter |
| cxf_setdblparam | Parallel function for double parameters |
| cxf_setstrparam | Parallel function for string parameters |
| cxf_setparam | Generic wrapper that dispatches by type |
| cxf_getintparaminfo | Retrieves parameter metadata (min/max/default) |
| cxf_resetparams | Resets all parameters to defaults |

## 11. Design Notes

### 11.1 Design Rationale

The extensive validation and special-case handling reflects the critical role parameters play in optimizer behavior. Fixed parameters prevent accidental modification of critical settings. The environment hierarchy system allows child environments (models) to inherit from master while preventing confusion when parameters diverge.

The clamping to ±2 billion prevents integer overflow while allowing near-maximum range. Hash table lookup provides O(1) access to the large parameter set (100+ parameters). The normalization step ensures user convenience (case-insensitive, flexible naming).

### 11.2 Performance Considerations

Hash table lookup is optimized for common parameters. Logging can be suppressed via flags for performance-critical inner loops. Parameter changes during model construction are common, so the function avoids expensive operations. Special parameter handling is minimized through flag-based dispatch rather than string comparisons on every call.

### 11.3 Future Considerations

Parameter deprecation mechanism not yet implemented - old parameters remain valid forever. No parameter value history tracking. No per-thread parameter overrides. The clamping range is hardcoded and may need adjustment for future 64-bit parameter values.

## 12. References

- Convexfeld Optimizer Reference Manual - Parameters section
- Internal parameter table implementation

## 13. Validation Checklist

- [x] No code copied from implementation
- [x] Algorithm description is implementation-agnostic
- [x] All parameters documented
- [x] All error conditions listed
- [x] Complexity analysis complete
- [x] Edge cases identified
- [x] A competent developer could implement from this spec alone

---

*Reviewed by: N/A*

---
*Convexfeld LP Solver Specification*
*Based on published optimization literature*
