# cxf_malloc

**Module:** Memory Management
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Primary heap allocation function for Convexfeld's memory management system. Provides environment-scoped memory allocation with centralized tracking, thread-safe access, and integration with memory limits. All dynamic memory allocations in Convexfeld flow through this function to enable comprehensive memory management and error handling.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| env | CxfEnv* | Environment context pointer | Valid CxfEnv pointer | Yes |
| size | size_t | Number of bytes to allocate | > 0 | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | void* | Pointer to allocated memory block, or NULL on failure |

### 2.3 Side Effects

- Updates environment's total memory tracking counter
- Increments environment's allocation count
- May add allocation to environment's tracking list
- Acquires and releases environment's critical section lock

## 3. Contract

### 3.1 Preconditions

- [ ] env must be a valid CxfEnv pointer (not NULL)
- [ ] size must be greater than 0
- [ ] Environment lock must be available (not in deadlock state)

### 3.2 Postconditions

- [ ] If successful: Returns pointer to at least 'size' bytes of writable memory
- [ ] If successful: Returned memory is aligned for any data type
- [ ] If successful: Environment's memory counter is increased by 'size'
- [ ] If successful: Allocation is tracked for cleanup during environment free
- [ ] If failed: Returns NULL and environment state is unchanged

### 3.3 Invariants

- [ ] Environment's total memory count accurately reflects all allocations
- [ ] No other threads can access allocation tracking during this operation
- [ ] Memory limit (if configured) is not exceeded by successful allocations

## 4. Algorithm

### 4.1 Overview

The function implements environment-scoped memory allocation with tracking. It acquires a thread-safety lock, performs the allocation using an underlying allocator (standard malloc or custom pool), updates tracking structures to record the allocation size and location, and releases the lock. The tracking enables memory leak detection, usage reporting, and bulk cleanup when the environment is destroyed.

### 4.2 Detailed Steps

1. Validate input parameters (env != NULL, size > 0)
2. Acquire environment's critical section lock for thread safety
3. Check memory limit if configured (env->memoryLimit)
   - If allocation would exceed limit, release lock and return NULL
4. Perform actual memory allocation via system allocator
5. If allocation successful:
   - Add allocation size to environment's total bytes counter
   - Increment environment's allocation count
   - Record allocation in tracking structure (e.g., linked list or hash table)
6. Release critical section lock
7. Return pointer to allocated memory (or NULL if allocation failed)

### 4.3 Pseudocode (if needed)

```
FUNCTION cxf_malloc(env, size) → pointer or NULL
  IF env = NULL OR size = 0 THEN
    RETURN NULL
  END IF

  ACQUIRE lock(env.criticalSection)

  # Check memory limit
  IF env.memoryLimit > 0 THEN
    IF env.totalBytesAllocated + size > env.memoryLimit THEN
      RELEASE lock(env.criticalSection)
      RETURN NULL
    END IF
  END IF

  # Allocate memory
  ptr ← system_allocate(size)

  IF ptr ≠ NULL THEN
    env.totalBytesAllocated ← env.totalBytesAllocated + size
    env.allocationCount ← env.allocationCount + 1
    add_to_tracking_list(env, ptr, size)
  END IF

  RELEASE lock(env.criticalSection)
  RETURN ptr
END FUNCTION
```

### 4.4 Mathematical Foundation (if applicable)

Not applicable - this is a system-level resource management function.

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1)
- **Average case:** O(1) + lock acquisition overhead
- **Worst case:** O(1) + lock contention delay

Where:
- Allocation, tracking update, and limit check are all constant time
- Lock acquisition may have variable delay under contention

### 5.2 Space Complexity

- **Auxiliary space:** O(1) - fixed tracking overhead per allocation
- **Total space:** O(size) - the requested allocation size

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| env is NULL | N/A (returns NULL) | Invalid environment pointer |
| size is 0 | N/A (returns NULL) | Invalid allocation size |
| Insufficient memory | N/A (returns NULL) | System malloc failed |
| Memory limit exceeded | N/A (returns NULL) | Would exceed configured MemLimit |

### 6.2 Error Behavior

The function returns NULL on any error condition. It does not set error codes directly - callers are responsible for checking the NULL return and propagating an error code (typically CXF_ERR_OUT_OF_MEMORY = 1001). The environment state remains consistent on failure (no partial updates to tracking structures).

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Null environment | env=NULL, size=100 | Return NULL immediately |
| Zero size | env=valid, size=0 | Return NULL immediately |
| Very large allocation | env=valid, size=2^63-1 | Likely return NULL (system malloc fails) |
| Memory limit exactly met | size causes total=limit | Allow allocation (not exceeded) |
| Memory limit would be exceeded | size causes total>limit | Return NULL, no allocation |

## 8. Thread Safety

**Thread-safe:** Yes


## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| malloc (or custom allocator) | System/Memory | Actual memory allocation from heap |
| EnterCriticalSection | System/Threading | Acquire environment lock (Windows) |
| LeaveCriticalSection | System/Threading | Release environment lock (Windows) |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_addvar | API | Allocate variable data arrays |
| cxf_addconstr | API | Allocate constraint data arrays |
| cxf_chgcoeffs | API | Allocate coefficient modification arrays |
| cxf_simplex_init | Solver | Allocate solver working state |
| Hundreds of internal functions | Various | General memory allocation |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_calloc | Zero-initialized variant - calls this then memset |
| cxf_realloc | Resize operation - uses similar tracking mechanism |
| cxf_free | Inverse operation - deallocates and updates tracking |
| malloc | Underlying allocator (wrapped by this function) |

## 11. Design Notes

### 11.1 Design Rationale

Environment-scoped allocation enables several critical features:
1. **Centralized tracking** - Know total memory usage per environment
2. **Bulk cleanup** - Free all allocations when environment destroyed
3. **Memory limits** - Enforce MemLimit parameter to prevent runaway allocation
4. **Thread safety** - Single lock per environment protects all allocations
5. **Error reporting** - Detect and report out-of-memory conditions consistently

The design trades some performance (lock overhead) for robustness and user visibility.

### 11.2 Performance Considerations

- Lock acquisition adds ~20-50 nanoseconds overhead per allocation
- For small allocations (<1KB), this overhead is 10-20% of total time
- Lock contention can be significant in heavily multi-threaded workloads
- Consider using thread-local pools for hot paths with many small allocations
- The function is called extremely frequently - any optimization has system-wide impact

### 11.3 Future Considerations

Potential optimizations:
- Thread-local allocation pools to reduce lock contention
- Size-class segregated allocators for common sizes (8, 16, 32, 64, 128 bytes)
- Per-model memory pools when multiple models share one environment
- Lockless techniques for tracking (e.g., atomic counters for statistics)

## 12. References

- Standard C malloc specification (ISO C standard)
- Windows Critical Sections documentation (for thread safety)
- Convexfeld parameter reference: MemLimit parameter

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
