# cxf_leave_critical_section

**Module:** Threading
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Releases the environment-level critical section lock, allowing other threads to acquire it and access environment resources. This wrapper function provides a consistent interface for releasing the lock acquired by cxf_env_acquire_lock, abstracting the lock location and providing NULL safety. It must be called after acquiring the environment lock to prevent deadlocks.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| env | CxfEnv* | Environment containing lock | Valid CxfEnv pointer | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | void | No return value |

### 2.3 Side Effects

Decrements the lock's recursion count. If the count reaches zero, releases the lock and potentially wakes a waiting thread.

## 3. Contract

### 3.1 Preconditions

- Environment pointer should be valid (NULL is handled gracefully)
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

This wrapper function encapsulates the details of accessing the environment's critical section pointer and invoking the operating system's release primitive. It provides NULL safety checks and abstracts the internal structure offset, making calling code cleaner and more portable.

### 4.2 Detailed Steps

1. Check if environment pointer is NULL; if so, return immediately (no-op)
2. Retrieve the critical section pointer from environment structure at documented offset
3. Check if critical section pointer is NULL; if so, return immediately
4. Call operating system primitive to leave the critical section
5. Return with lock potentially released (depending on recursion count)

### 4.3 Pseudocode

```
FUNCTION leave_critical_section(env):
    IF env = NULL THEN
        RETURN
    END IF

    lock ← env.critical_section_pointer

    IF lock = NULL THEN
        RETURN
    END IF

    os_leave_critical_section(lock)
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
| NULL environment | N/A | Handled gracefully, returns without error |
| NULL lock pointer | N/A | Handled gracefully, returns without error |
| Lock not held | N/A | Undefined behavior (OS may detect and crash) |
| Wrong thread releasing | N/A | Undefined behavior (OS may detect and crash) |

### 6.2 Error Behavior

The function handles NULL pointers defensively by returning early. However, calling this function without holding the lock or from a different thread produces undefined behavior at the OS level.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| NULL environment | env = NULL | Returns immediately, no operation |
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
| cxf_setintparam | API | After modifying integer parameters |
| cxf_setdblparam | API | After modifying double parameters |
| cxf_newmodel | API | After creating new model |
| cxf_freemodel | API | After removing model from list |
| cxf_log_printf | Internal | After writing to log file |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_env_acquire_lock | Paired acquire function (must be called before this) |
| cxf_acquire_solve_lock | Solve-level lock acquisition |
| cxf_release_solve_lock | Solve-level lock release |

## 11. Design Notes

### 11.1 Design Rationale

This wrapper provides several benefits over direct OS API calls:

1. **Encapsulation:** Hides the internal structure offset
2. **NULL Safety:** Handles invalid pointers gracefully
3. **Portability:** Can be reimplemented for different platforms (pthread, etc.)
4. **Consistency:** Paired naming with cxf_env_acquire_lock

The function name uses Windows terminology ("LeaveCriticalSection") to clearly indicate platform-specific implementation, though the interface could be kept platform-independent.

### 11.2 Performance Considerations

Release is extremely fast (~10-20 ns) in the uncontended case. The function is called infrequently (only during environment operations) so performance is not critical. The NULL checks add negligible overhead (<5 ns).

### 11.3 Future Considerations

Best practice patterns for using this function:

**C goto cleanup pattern:**
```c
int error = 0;
cxf_env_acquire_lock(env);
error = operation1(env);
if (error) goto cleanup;
error = operation2(env);
cleanup:
    cxf_leave_critical_section(env);
    return error;
```

**C++ RAII wrapper:**
```cpp
class EnvLockGuard {
    CxfEnv* env_;
public:
    EnvLockGuard(CxfEnv* env) : env_(env) { cxf_env_acquire_lock(env_); }
    ~EnvLockGuard() { cxf_leave_critical_section(env_); }
};
```

## 12. References

- CxfEnv structure documentation: docs/reference/structures/CxfEnv.md
- Lock discipline best practices: Paired acquire/release, RAII pattern

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
