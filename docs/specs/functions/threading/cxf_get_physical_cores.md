# cxf_get_physical_cores

**Module:** Threading
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Detects the number of physical CPU cores available on the system, excluding hyper-threaded virtual cores. This count is used to determine optimal thread pool sizing when the Threads parameter is set to auto-detect mode (0). Using physical core count rather than logical processor count avoids performance degradation from hyper-threading over-subscription in compute-intensive optimization algorithms.

## 2. Signature

### 2.1 Inputs

None. This is a system query function.

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | Number of physical CPU cores (minimum 1) |

### 2.3 Side Effects

None. This function only queries system information without modifying any state.

## 3. Contract

### 3.1 Preconditions

None. Function can be called at any time.

### 3.2 Postconditions

- Returns a positive integer representing physical core count
- Returns at least 1 even if detection fails

### 3.3 Invariants

- System CPU topology is not modified
- Result is consistent for the lifetime of the process (unless running on VM with hot-add CPUs)

## 4. Algorithm

### 4.1 Overview

On Windows systems, the function uses the GetLogicalProcessorInformation API to enumerate processor topology information. It iterates through the returned structures and counts entries with relationship type "ProcessorCore" which represents physical cores. On detection failure, it falls back to returning the logical processor count or a minimum of 1.

### 4.2 Detailed Steps

1. Allocate a buffer for processor information structures
2. Call system API to retrieve processor topology information
3. If API call fails, fall back to logical processor count query
4. Iterate through returned processor information structures
5. Count entries where relationship type indicates a physical processor core
6. Return the physical core count if greater than 0, otherwise return 1

### 4.3 Pseudocode

```
FUNCTION get_physical_cores():
    buffer ← ALLOCATE_ARRAY(processor_info, 256)
    buffer_size ← SIZE_OF(buffer)

    success ← query_processor_topology(buffer, buffer_size)

    IF NOT success THEN
        RETURN get_logical_processors()
    END IF

    physical_count ← 0
    offset ← 0

    WHILE offset < buffer_size DO
        info ← buffer[offset]
        IF info.relationship = PROCESSOR_CORE THEN
            physical_count ← physical_count + 1
        END IF
        offset ← offset + SIZE_OF(processor_info)
    END WHILE

    RETURN MAX(physical_count, 1)
END FUNCTION
```

### 4.4 Mathematical Foundation

The distinction between physical and logical processors:

- Physical cores (P): Independent execution units with dedicated L1/L2 cache
- Logical processors (L): Total hardware threads including hyper-threading
- Relationship: L = P × HT_factor, where HT_factor = 2 for hyper-threading enabled, 1 otherwise

For compute-bound optimization algorithms, performance scaling follows physical cores more closely than logical processors due to cache contention between hyper-threaded siblings.

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) - Single system API call
- **Average case:** O(n) where n = number of processor topology entries (typically < 100)
- **Worst case:** O(n)

Where:
- n = number of processor topology entries returned by OS (proportional to CPU count)

### 5.2 Space Complexity

- **Auxiliary space:** O(n) for processor information buffer
- **Total space:** O(n)

Typically allocates buffer for 256 entries which is sufficient for systems with up to 128 physical cores.

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| API not available | N/A | Falls back to logical processor count |
| API call fails | N/A | Falls back to logical processor count |
| Zero cores detected | N/A | Returns 1 as minimum |

### 6.2 Error Behavior

The function never fails. It uses a fallback chain: physical core detection → logical processor count → return 1. This ensures thread pool initialization can always proceed with a reasonable value.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Single-core CPU | System with 1 core | Returns 1 |
| Hyper-threading enabled | 4 core + HT | Returns 4 (not 8) |
| Hyper-threading disabled | 4 core no HT | Returns 4 |
| Virtual machine | Subset of host cores | Returns VM core count |
| API unavailable | Old Windows version | Falls back to logical count |
| NUMA system | Multi-socket server | Returns total across all nodes |

## 8. Thread Safety

**Thread-safe:** Yes

The function only queries read-only system information via thread-safe OS APIs. Multiple threads can call this function concurrently without synchronization.

**Synchronization required:** None

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| GetLogicalProcessorInformation | Windows API | Query processor topology |
| cxf_get_logical_processors | Threading | Fallback if physical detection fails |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| Thread pool initialization | Threading | When Threads=0 (auto-detect) |
| cxf_set_thread_count | Threading | To determine optimal thread count |
| Solver initialization | Optimization | During setup phase |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_get_logical_processors | Returns total including hyper-threading (typically 2x) |
| cxf_get_threads | Returns configured Threads parameter |
| cxf_set_thread_count | Uses this function's result when Threads=0 |

## 11. Design Notes

### 11.1 Design Rationale

Using physical core count for thread pool sizing is based on the observation that optimization algorithms are compute-bound rather than memory-bound. Hyper-threading provides limited benefit for such workloads because:

1. Both threads compete for the same execution units
2. Cache contention between hyper-threaded siblings
3. Limited improvement when floating-point units are saturated

Empirical testing shows simplex and barrier algorithms scale better to physical cores than to logical processors.

### 11.2 Performance Considerations

This function is called infrequently (typically once per process lifetime) so performance is not critical. The OS API call takes less than 1 millisecond. Results could be cached if the function were called repeatedly, but this is unnecessary for typical usage patterns.

### 11.3 Future Considerations

On heterogeneous CPU architectures (e.g., ARM big.LITTLE or Intel hybrid), may need to distinguish between performance and efficiency cores. Current implementation treats all physical cores equally.

## 12. References

- CPU topology detection: Standard OS facility for querying processor configuration
- Hyper-threading performance analysis: Industry research on optimization algorithm scaling

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
