# cxf_get_logical_processors

**Module:** Threading
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Detects the total number of logical processors available on the system, including hyper-threaded virtual cores. This count represents the maximum number of concurrent hardware threads the CPU can execute. It is used for thread count validation (ensuring the solver doesn't request more threads than available) and as a fallback when physical core detection is unavailable.

## 2. Signature

### 2.1 Inputs

None. This is a system query function.

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | Number of logical processors (minimum 1) |

### 2.3 Side Effects

None. This function only queries system information without modifying any state.

## 3. Contract

### 3.1 Preconditions

None. Function can be called at any time.

### 3.2 Postconditions

- Returns a positive integer representing logical processor count
- Returns at least 1 even if detection fails

### 3.3 Invariants

- System CPU configuration is not modified
- Result is consistent for the lifetime of the process

## 4. Algorithm

### 4.1 Overview

On Windows systems, the function uses the GetSystemInfo API to retrieve system configuration, specifically the dwNumberOfProcessors field which contains the total logical processor count. This is a simple, reliable method that works on all Windows versions. The function ensures a minimum return value of 1 to handle any unexpected failures.

### 4.2 Detailed Steps

1. Declare a system information structure
2. Call system API to populate the structure
3. Extract the logical processor count from the structure
4. Return the count if greater than 0, otherwise return 1

### 4.3 Pseudocode

```
FUNCTION get_logical_processors():
    sys_info ← SYSTEM_INFO_STRUCTURE()

    query_system_info(sys_info)

    processor_count ← sys_info.number_of_processors

    RETURN MAX(processor_count, 1)
END FUNCTION
```

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1)
- **Average case:** O(1)
- **Worst case:** O(1)

Single system API call with constant-time processing.

### 5.2 Space Complexity

- **Auxiliary space:** O(1)
- **Total space:** O(1)

Only allocates a fixed-size system information structure.

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| API returns 0 | N/A | Returns 1 as minimum safe value |

### 6.2 Error Behavior

The function never fails. It returns at least 1 even if the system API returns an unexpected value. This ensures thread pool validation can always complete with a reasonable maximum.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Single-core CPU | System with 1 logical processor | Returns 1 |
| Hyper-threading enabled | 4 core + HT = 8 logical | Returns 8 |
| Hyper-threading disabled | 4 core no HT = 4 logical | Returns 4 |
| Virtual machine | VM with subset of host CPUs | Returns VM processor count |
| Massive server | 128-core system | Returns 128 (or higher) |
| API failure | Unexpected error | Returns 1 |

## 8. Thread Safety

**Thread-safe:** Yes

The function only queries read-only system information via thread-safe OS APIs. Multiple threads can call this function concurrently without synchronization.

**Synchronization required:** None

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| GetSystemInfo | Windows API | Query system configuration including processor count |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_set_thread_count | Threading | Validate thread count doesn't exceed logical processors |
| cxf_get_physical_cores | Threading | Fallback if physical core detection fails |
| Thread pool validation | Threading | Ensure requested threads don't exceed hardware capability |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_get_physical_cores | Returns subset (physical cores only, excluding HT) |
| cxf_get_threads | Returns configured Threads parameter |
| cxf_set_thread_count | Uses this for maximum validation |

## 11. Design Notes

### 11.1 Design Rationale

This function uses the simplest possible detection method (GetSystemInfo) rather than more complex topology enumeration. This is appropriate because:

1. Logical processor count is straightforward to query
2. No need to distinguish between physical and logical
3. Maximum reliability across all Windows versions
4. Sufficient for validation and fallback purposes

The function serves two primary purposes:
- Maximum thread count validation (don't exceed hardware capability)
- Fallback when physical core detection is unavailable

### 11.2 Performance Considerations

The function is extremely fast (< 1 microsecond) and is typically called only once during initialization. Results could be cached statically if called repeatedly, but this is unnecessary given the minimal cost.

### 11.3 Future Considerations

On systems with hot-add CPU support (rare), the logical processor count could change during process lifetime. Current implementation assumes static CPU configuration, which is valid for > 99% of deployments.

## 12. References

- CPU topology: Industry standard distinction between physical and logical processors

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
