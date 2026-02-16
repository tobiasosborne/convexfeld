# cxf_release_solve_lock

**Module:** Threading
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Releases a critical section lock at the solver state level, allowing other threads to acquire it and access solver data structures. This function must be called after cxf_acquire_solve_lock to properly release the lock and prevent deadlocks. It decrements the lock's recursion count and releases the lock when the count reaches zero.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| state | SolverState* | Solver state containing lock | Valid SolverState pointer | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | void | No return value |

### 2.3 Side Effects

Decrements the lock's recursion count. If the count reaches zero, releases the lock and potentially wakes a waiting thread.

## 3. Contract

### 3.1 Preconditions

- Solver state pointer should be valid (NULL is handled gracefully)
- Lock must be currently held by the calling thread
- Must be called from the same thread that acquired the lock

### 3.2 Postconditions

- Lock's recursion count is decremented
- If count reaches zero, lock is fully released
- If other threads are waiting, one is awakened to acquire the lock

### 3.3 Invariants

- Must be called exactly once for each acquire call
- Cannot release a lock held by a different thread
- Lock count never goes negative (undefined behavior if violated)

## 4. Algorithm

### 4.1 Overview

The function validates the solver state pointer, retrieves the critical section pointer from the solver state structure, and invokes the operating system's critical section release primitive. This decrements the recursion count and releases the lock if the count reaches zero. The operation is very fast in the uncontended case.

### 4.2 Detailed Steps

1. Check if solver state pointer is NULL; if so, return immediately (no-op)
2. Retrieve the critical section pointer from solver state structure at known offset
3. Check if critical section pointer is NULL; if so, return immediately
4. Call operating system primitive to leave the critical section
5. Return with lock potentially released (depending on recursion count)

### 4.3 Pseudocode

```
FUNCTION release_solve_lock(state):
    IF state = NULL THEN
        RETURN
    END IF

    lock ← state.critical_section_pointer

    IF lock = NULL THEN
        RETURN
    END IF

    leave_critical_section(lock)
    # Lock recursion count decremented
    # If count = 0, lock released and waiter awakened
END FUNCTION
```

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) - No waiters, simple count decrement (~10-20 nanoseconds)
- **Average case:** O(1) - Typically no waiters
- **Worst case:** O(1) - Even with waiters, wake operation is constant time (~2-10 microseconds)

### 5.2 Space Complexity

- **Auxiliary space:** O(1)
- **Total space:** O(1)

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| NULL state | N/A | Handled gracefully, returns without error |
| NULL lock pointer | N/A | Handled gracefully, returns without error |
| Lock not held | N/A | Undefined behavior (OS may detect and crash) |
| Wrong thread releasing | N/A | Undefined behavior (OS may detect and crash) |

### 6.2 Error Behavior

The function handles NULL pointers defensively by returning early. However, calling this function without holding the lock or from a different thread produces undefined behavior at the OS level, which may result in crashes or silent corruption.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| NULL state | state = NULL | Returns immediately, no operation |
| NULL lock pointer | lock = NULL | Returns immediately, no operation |
| Recursive held (count > 1) | Same thread, multiple acquires | Decrements count, lock still held |
| Final release (count = 1) | Last release | Decrements to 0, releases lock |
| Waiters present | Other threads blocked | Wakes one waiting thread |

## 8. Thread Safety

**Thread-safe:** Yes (when used correctly)

This function is thread-safe when called by the thread that acquired the lock. It is undefined behavior to call from a different thread.

**Synchronization required:** None - this function is part of the synchronization mechanism.

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| LeaveCriticalSection | Windows API | Release the critical section lock |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| Pivot operations | Simplex | After completing basis modifications |
| Basis updates | Simplex | After refactoring completes |
| Iteration updates | Simplex | After updating counters |
| Eta vector management | Basis | After allocating/modifying eta vectors |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_acquire_solve_lock | Paired acquire function (must be called before this) |
| cxf_env_acquire_lock | Environment-level lock acquisition |
| cxf_leave_critical_section | Releases environment lock |

## 11. Design Notes

### 11.1 Design Rationale

This function must be paired with cxf_acquire_solve_lock to maintain lock discipline. The recursive support means that nested function calls can safely acquire and release without tracking whether an outer function already holds the lock.

The defensive NULL checking ensures that calling code doesn't need to check validity before releasing, simplifying error handling paths.

### 11.2 Performance Considerations

Release is extremely fast (~10-20 ns) when no other threads are waiting. Even with waiters, the wake operation is handled efficiently by the OS kernel. The function should be called as soon as the critical section work is complete to minimize lock contention.

### 11.3 Future Considerations

Best practice patterns:
- Use goto cleanup pattern in C to ensure release on all paths
- Use RAII wrappers in C++ for automatic release
- Keep critical sections as short as possible
- Never call blocking I/O while holding the lock

## 12. References

- Lock discipline: Industry best practices for paired acquire/release
- RAII pattern: Automatic resource management for exception safety

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
