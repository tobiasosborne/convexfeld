# cxf_acquire_solve_lock

**Module:** Threading
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Acquires a critical section lock at the solver state level to ensure thread-safe access during optimization operations. This lock protects solver-specific data structures such as basis information, working arrays, pivot operations, and iteration state during concurrent modifications. It provides finer-grained synchronization than the environment-level lock, allowing multiple models to optimize concurrently.

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

Blocks the calling thread until the lock is acquired. Other threads attempting to acquire the same lock will block until this thread releases it.

## 3. Contract

### 3.1 Preconditions

- Solver state pointer should be valid (NULL is handled gracefully)
- Lock should be initialized in the solver state structure
- Should not already hold environment lock if acquiring both (lock ordering)

### 3.2 Postconditions

- Calling thread holds the solve lock
- Lock's recursion count is incremented
- Other threads attempting to acquire will block

### 3.3 Invariants

- Lock can be acquired recursively by the same thread
- Must be released same number of times as acquired
- Cannot be released by a different thread

## 4. Algorithm

### 4.1 Overview

The function implements a two-step process: first validate the solver state pointer is not NULL, then retrieve the critical section pointer from a known offset in the solver state structure and invoke the operating system's critical section acquisition primitive. Windows CRITICAL_SECTION supports recursive acquisition, so the same thread can acquire multiple times safely.

### 4.2 Detailed Steps

1. Check if solver state pointer is NULL; if so, return immediately (no-op)
2. Retrieve the critical section pointer from solver state structure at known offset
3. Check if critical section pointer is NULL; if so, return immediately
4. Call operating system primitive to enter the critical section (blocks until acquired)
5. Return with lock held

### 4.3 Pseudocode

```
FUNCTION acquire_solve_lock(state):
    IF state = NULL THEN
        RETURN
    END IF

    lock ← state.critical_section_pointer

    IF lock = NULL THEN
        RETURN
    END IF

    enter_critical_section(lock)
    # Lock now held by calling thread
END FUNCTION
```

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) - Lock available, acquired immediately (~20-50 nanoseconds)
- **Average case:** O(1) - Typically uncontended
- **Worst case:** O(n) where n = time waiting for lock holder to release (unbounded)

### 5.2 Space Complexity

- **Auxiliary space:** O(1)
- **Total space:** O(1)

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| NULL state | N/A | Handled gracefully, returns without error |
| NULL lock | N/A | Handled gracefully, returns without error |

### 6.2 Error Behavior

The function handles NULL pointers defensively by returning early rather than propagating errors. This design allows calling code to be simpler since lock acquisition never fails visibly (though it may be a no-op for invalid inputs).

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| NULL state | state = NULL | Returns immediately, no lock acquired |
| NULL lock pointer | lock = NULL | Returns immediately, no lock acquired |
| Lock already held | Same thread | Increments recursion count, returns immediately |
| Lock held by other thread | Different thread | Blocks until lock released |
| First acquisition | Uncontended | Acquires immediately |

## 8. Thread Safety

**Thread-safe:** Yes (this IS the thread-safety mechanism)

Multiple threads calling this function will serialize at the critical section entry. Only one thread can hold the lock at a time (excluding recursive acquisition by same thread).

**Synchronization required:** None - this function provides the synchronization.

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| EnterCriticalSection | Windows API | Acquire the critical section lock |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| Pivot operations | Simplex | Before modifying basis during pivots |
| Basis updates | Simplex | Before refactoring or updating basis |
| Iteration updates | Simplex | Before updating iteration counters and objective |
| Eta vector management | Basis | Before allocating or modifying eta vectors |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_release_solve_lock | Paired release function (must be called after this) |
| cxf_env_acquire_lock | Environment-level lock (coarser granularity) |
| cxf_leave_critical_section | Releases environment lock |

## 11. Design Notes

### 11.1 Design Rationale

Convexfeld uses a two-level locking hierarchy:
1. Environment lock: Protects parameters, model list, global state
2. Solve lock: Protects solver state during optimization

This separation allows multiple models to optimize concurrently (each with its own solve lock) while still protecting shared environment resources. The solve lock is held longer (during entire critical operations) but is specific to one optimization run.

Recursive acquisition support simplifies code structure - functions can acquire the lock without checking if the caller already holds it.

### 11.2 Performance Considerations

The lock uses a spin-then-block strategy (typical spin count ~4000 iterations) which is optimal for locks with short hold times. In the uncontended case (common for single-threaded solves), acquisition is extremely fast (~20-50 ns).

For multi-threaded solves, the lock may be contended during critical operations but the hold time should be kept minimal to avoid serializing parallel algorithms.

### 11.3 Future Considerations

Could be enhanced with:
- Deadlock detection (timeout on acquisition)
- Lock hold time tracking for performance analysis
- Priority inheritance to prevent priority inversion
- Read-write lock for operations that only read state

However, the current simple design is appropriate for typical usage patterns.

## 12. References

- Lock-free alternatives: For very short critical sections, atomic operations may be more efficient
- Lock hierarchies: Standard technique for deadlock prevention

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
