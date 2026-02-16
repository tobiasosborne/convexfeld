# cxf_set_thread_count

**Module:** Threading
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Configures the actual number of threads that will be used by the solver's thread pool. This function is called after the Threads parameter has been resolved (converting 0=auto to a specific physical core count) and stores the determined thread count for use during optimization. It validates the count against system limits and prepares the environment for parallel execution.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| env | CxfEnv* | Environment to configure | Valid CxfEnv pointer | Yes |
| thread_count | int | Number of threads to use | 1 to logical_processor_count | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | Error code (0 = success, non-zero = error) |

### 2.3 Side Effects

Stores the thread count in the environment structure, making it available for subsequent thread pool creation and parallel algorithm execution.

## 3. Contract

### 3.1 Preconditions

- Environment pointer must be valid
- Thread count must be at least 1
- Thread count should be resolved (not 0=auto)

### 3.2 Postconditions

- Thread count is stored in environment state
- Thread count is capped at logical processor count if exceeded
- Environment is ready for parallel execution with specified thread count

### 3.3 Invariants

- Thread count remains >= 1 after function execution
- Thread count does not exceed system logical processor count

## 4. Algorithm

### 4.1 Overview

This function validates and stores the thread count that will be used during optimization. It implements a lazy initialization strategy where the thread count is recorded but actual thread pool creation is deferred until threads are needed. The function enforces a minimum of 1 thread and caps the maximum at the system's logical processor count to prevent over-subscription.

### 4.2 Detailed Steps

1. Validate environment pointer is not NULL
2. Validate thread count is at least 1 (minimum)
3. Query system logical processor count
4. If requested thread count exceeds logical processor count, cap at that maximum
5. Store the validated thread count in environment state
6. Return success code

### 4.3 Pseudocode

```
FUNCTION set_thread_count(env, count):
    IF env = NULL THEN
        RETURN ERROR_INVALID_ARGUMENT
    END IF

    IF count < 1 THEN
        RETURN ERROR_INVALID_ARGUMENT
    END IF

    max_threads ← get_logical_processor_count()

    IF count > max_threads THEN
        count ← max_threads
    END IF

    env.active_thread_count ← count

    RETURN SUCCESS
END FUNCTION
```

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1)
- **Average case:** O(1)
- **Worst case:** O(1)

All operations are constant time (validation, system query, storage).

### 5.2 Space Complexity

- **Auxiliary space:** O(1)
- **Total space:** O(1)

No dynamic allocation, only stores a single integer value.

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| NULL environment | 1002 (NULL_ARGUMENT) | Invalid environment pointer |
| thread_count < 1 | 1003 (INVALID_ARGUMENT) | Thread count below minimum |

### 6.2 Error Behavior

On error, the function returns immediately without modifying environment state. The thread count remains at its previous value, ensuring the environment stays in a consistent state.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Minimum threads | count = 1 | Accepted, single-threaded mode |
| At system max | count = logical_processors | Accepted as-is |
| Above system max | count > logical_processors | Capped at logical_processors |
| NULL environment | env = NULL | Returns error code |

## 8. Thread Safety

**Thread-safe:** Conditionally

The function should not be called concurrently with active optimization runs. It is safe to call during initialization before optimization begins. If concurrent access is possible, the caller should hold the environment lock.

**Synchronization required:** Environment lock if called concurrently with other operations.

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_get_logical_processors | Threading | Query maximum thread count for validation |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| Solver initialization | Optimization | After resolving Threads parameter |
| cxf_optimize | API | During optimization setup |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_get_threads | Retrieves configured Threads parameter (input to this function) |
| cxf_get_physical_cores | Used to resolve Threads=0 before calling this |
| cxf_get_logical_processors | Used for validation of maximum thread count |
| Thread pool creation | Uses stored thread count to create worker threads |

## 11. Design Notes

### 11.1 Design Rationale

The function uses lazy initialization where the thread count is stored but threads are not created immediately. This avoids the overhead of creating thread pools for single-threaded solves or quick optimizations. Threads are created on-demand when parallel algorithms are invoked.

The capping behavior (reducing excessive thread counts to logical processor count) prevents performance degradation from over-subscription while still allowing the solver to proceed.

### 11.2 Performance Considerations

This function is called once during solver initialization, so performance is not critical. The system query for logical processor count is cached in most implementations, making it very fast (< 1 microsecond).

Lazy thread pool creation means there's no upfront cost if threads aren't needed, improving performance for small or single-threaded problems.

### 11.3 Future Considerations

Could be extended to:
- Support dynamic thread count changes during optimization
- Implement thread pool pre-warming for faster first use
- Add thread affinity configuration
- Support heterogeneous thread counts for different algorithm phases

## 12. References

- Threading patterns: docs/knowledge/patterns/memory_thread.md

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
