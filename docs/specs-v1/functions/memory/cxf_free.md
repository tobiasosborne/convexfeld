# cxf_free

**Module:** Memory Management
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Deallocate memory previously allocated by cxf_malloc, cxf_calloc, or cxf_realloc. Returns memory to the environment's memory pool and updates internal tracking structures. This function must be used to free all memory allocated through Convexfeld's allocation functions - using standard library free() on Convexfeld-allocated memory produces undefined behavior.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| env | CxfEnv* | Environment context pointer | Valid CxfEnv pointer or NULL | Yes |
| ptr | void* | Pointer to memory to free | NULL or pointer from cxf_malloc/cxf_calloc/cxf_realloc | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | void | No return value |

### 2.3 Side Effects

- Deallocates memory block
- Decrements environment's total memory tracking counter
- Decrements environment's allocation count
- Removes allocation from environment's tracking list
- Acquires and releases environment's critical section lock
- Pointer becomes invalid (dangling) after call

## 3. Contract

### 3.1 Preconditions

- [ ] If ptr is not NULL: env must be valid CxfEnv pointer
- [ ] If ptr is not NULL: ptr must be from cxf_malloc/cxf_calloc/cxf_realloc
- [ ] Pointer must not have been freed previously (no double-free)

### 3.2 Postconditions

- [ ] Memory at ptr is deallocated and cannot be accessed
- [ ] Environment's memory counter decreased by allocation size
- [ ] Environment's allocation count decremented
- [ ] Allocation removed from tracking list
- [ ] Function completes without error (void return)

### 3.3 Invariants

- [ ] Environment's total memory count accurately reflects all allocations
- [ ] NULL pointer and NULL environment are safe (no-op)

## 4. Algorithm

### 4.1 Overview

The function validates inputs (safe handling of NULL), acquires a thread-safety lock, retrieves the allocation size from tracking structures, updates the environment's memory counters by subtracting the freed size, removes the allocation from the tracking list, releases the lock, and finally calls the system free function to return memory to the heap.

### 4.2 Detailed Steps

1. Check if env is NULL or ptr is NULL - if so, return immediately (no-op)
2. Acquire environment's critical section lock
3. Retrieve allocation size from tracking structure (header or hash table)
4. Update environment's memory statistics:
   - Subtract size from totalBytesAllocated
   - Decrement allocationCount
5. Remove allocation from tracking list (unlink from linked list or remove from hash table)
6. Release critical section lock
7. Call system free() to deallocate memory
8. Return (void)

### 4.3 Pseudocode (if needed)

```
PROCEDURE cxf_free(env, ptr)
  # Safe handling of NULL inputs
  IF env = NULL OR ptr = NULL THEN
    RETURN  # No-op
  END IF

  ACQUIRE lock(env.criticalSection)

  # Retrieve allocation size from tracking
  size ← get_allocation_size(ptr)  # From header or tracking table

  # Update statistics
  env.totalBytesAllocated ← env.totalBytesAllocated - size
  env.allocationCount ← env.allocationCount - 1

  # Remove from tracking list
  remove_from_tracking_list(env, ptr)

  RELEASE lock(env.criticalSection)

  # Free the actual memory
  system_free(ptr)

  RETURN
END PROCEDURE
```

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1)
- **Average case:** O(1)
- **Worst case:** O(1)

Where:
- All operations (tracking lookup, list removal, free) are constant time
- Lock acquisition may have variable delay under contention

### 5.2 Space Complexity

- **Auxiliary space:** O(1) - no additional allocation
- **Total space:** O(-size) - releases size bytes back to system

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| env is NULL | None (silent no-op) | Invalid environment, safe to call |
| ptr is NULL | None (silent no-op) | NULL pointer, safe to call |
| Double-free | Undefined behavior | Same pointer freed twice (may crash) |
| Wrong allocator | Undefined behavior | Freeing memory not allocated by cxf_ functions |

### 6.2 Error Behavior

The function has no error return value. NULL inputs are handled safely (no-op). Double-free and wrong-allocator errors result in undefined behavior - may crash, corrupt heap, or appear to succeed. Debug builds may detect and abort on these errors.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| NULL environment | env=NULL, ptr=valid | No-op, return immediately |
| NULL pointer | env=valid, ptr=NULL | No-op, return immediately |
| Both NULL | env=NULL, ptr=NULL | No-op, return immediately |
| Very large allocation | ptr to 1GB block | Free normally, update counters |
| Allocated in different env | env=envA, ptr from envB | Undefined behavior |

## 8. Thread Safety

**Thread-safe:** Yes

**Synchronization required:** Acquires environment->criticalSection during tracking updates. Multiple threads can call cxf_free concurrently on different allocations without corruption.

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| free | System | Actual memory deallocation |
| EnterCriticalSection | System | Acquire environment lock |
| LeaveCriticalSection | System | Release environment lock |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_freemodel | API | Free model structures and arrays |
| cxf_freeenv | API | Free all environment allocations |
| cxf_free_solver_state | Internal | Free solver working state |
| cxf_free_basis_state | Internal | Free basis snapshot |
| All API and internal functions | Various | Error cleanup and normal deallocation |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_malloc | Inverse operation - allocates memory |
| cxf_calloc | Inverse operation - allocates zero-initialized memory |
| cxf_realloc | Inverse operation - allocates/resizes memory |
| free | Underlying system function (wrapped by this) |

## 11. Design Notes

### 11.1 Design Rationale

Requirements for free function:
1. **NULL-safe** - Simplifies cleanup code (no need to check before calling)
2. **Thread-safe** - Multiple threads can free different allocations safely
3. **Tracking update** - Maintain accurate memory usage statistics
4. **Environment-scoped** - Enables bulk cleanup when environment destroyed

The void return (no error code) matches standard library convention and reflects that memory freeing "cannot fail" in normal operation.

### 11.2 Performance Considerations

- Free operation is very fast: ~50-100 nanoseconds typical
- Lock acquisition adds ~20-50 nanoseconds overhead
- Total overhead: ~100-150 nanoseconds per free
- Negligible for most use cases
- May matter in tight loops with millions of allocations (consider bulk operations)

### 11.3 Future Considerations

- Provide bulk free operation for array of pointers (single lock acquisition)
- Debug mode: Detect double-free by setting magic marker in freed memory
- Debug mode: Fill freed memory with pattern (0xDD) to catch use-after-free
- Statistics: Track peak memory usage, allocation size distribution

## 12. References

- Standard C free specification (ISO C standard)
- "Undefined Behavior in C" - double-free consequences
- Memory allocator security (heap corruption from invalid free)

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
