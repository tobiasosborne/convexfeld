# cxf_get_threads

**Module:** Threading
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Retrieves the configured thread count from the environment's Threads parameter. This value controls how many parallel threads the solver will use during optimization. A value of 0 indicates auto-detection mode where the solver determines the optimal thread count based on available hardware.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| env | CxfEnv* | Environment to query | Valid CxfEnv pointer | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | Configured thread count (0 = auto, N > 0 = specific count) |

### 2.3 Side Effects

None. This is a read-only query function.

## 3. Contract

### 3.1 Preconditions

- Environment pointer must be valid and initialized

### 3.2 Postconditions

- Return value represents the current Threads parameter setting
- Environment state is unchanged

### 3.3 Invariants

- Environment parameters are not modified
- No global state changes occur

## 4. Algorithm

### 4.1 Overview

This function acts as a convenience wrapper around the parameter retrieval system. It queries the integer parameter named "Threads" from the environment's parameter storage and returns its current value. If the parameter cannot be retrieved due to an error, it returns the default value of 0 (auto-detection mode).

### 4.2 Detailed Steps

1. Query the integer parameter "Threads" from the environment using the parameter retrieval mechanism
2. If retrieval succeeds, return the parameter value
3. If retrieval fails, return 0 (default auto-detection value)

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1)
- **Average case:** O(1)
- **Worst case:** O(1)

Where parameter lookup is constant time via direct offset access or hash table.

### 5.2 Space Complexity

- **Auxiliary space:** O(1)
- **Total space:** O(1)

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| Invalid environment | N/A | Returns 0 (default) |
| Parameter not found | N/A | Returns 0 (default) |

### 6.2 Error Behavior

The function does not propagate errors. Instead, it returns a safe default value (0) if any error occurs during parameter retrieval. This design choice ensures that thread pool initialization can proceed with auto-detection even if the parameter system encounters issues.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| NULL environment | env = NULL | Returns 0 |
| Uninitialized env | Invalid env | Returns 0 |
| Parameter not set | Default state | Returns 0 |
| Threads = 1 | Single-threaded | Returns 1 |
| Threads = 0 | Auto mode | Returns 0 |
| Large thread count | Threads = 1024 | Returns 1024 (no validation) |

## 8. Thread Safety

**Thread-safe:** Yes

Reading parameters is thread-safe as parameters are typically not modified during optimization. If called concurrently with parameter modifications, the environment-level lock should be held by the caller.

**Synchronization required:** None for reads. Environment lock required if called during parameter modifications.

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_getintparam | Parameters | Retrieve integer parameter value |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_set_thread_count | Threading | During solver initialization to determine thread count |
| Thread pool init | Threading | When creating worker threads |
| Solver initialization | Optimization | Before starting optimization |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_getintparam | Underlying mechanism for parameter retrieval |
| cxf_set_thread_count | Sets the actual thread pool size after resolution |
| cxf_get_physical_cores | Used to resolve Threads=0 to specific count |
| cxf_get_logical_processors | Used for validation of thread count |

## 11. Design Notes

### 11.1 Design Rationale

This wrapper function provides a cleaner interface for internal code that needs to query the thread count. Rather than requiring every caller to know the parameter name string ("Threads") and handle errors, this function encapsulates the query and provides sensible defaults.

### 11.2 Performance Considerations

The function is called infrequently (typically once per optimization run) so performance is not critical. It may access parameters via hash table lookup or direct offset access depending on implementation strategy.

### 11.3 Future Considerations

Could be extended to perform validation (e.g., capping at logical processor count) or to cache the value for repeated queries. However, the current simple design is appropriate for its usage pattern.

## 12. References


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
