# cxf_env_acquire_lock

**Module:** Threading
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Acquires a critical section lock at the environment level to ensure thread-safe access to global state, parameters, model lists, and other environment-wide resources. This is the coarsest-grained lock in the system, protecting resources shared across all models in the environment. It allows multiple models to optimize concurrently while protecting parameter modifications and model creation operations.

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

Blocks the calling thread until the lock is acquired. Other threads attempting to acquire the same lock will block until this thread releases it.

## 3. Contract

### 3.1 Preconditions

- Environment pointer should be valid (NULL is handled gracefully)
- Lock should be initialized in the environment structure
- Should be acquired before any solve-level locks (lock hierarchy)

### 3.2 Postconditions

- Calling thread holds the environment lock
- Lock's recursion count is incremented
- Other threads attempting to acquire will block

### 3.3 Invariants

- Lock can be acquired recursively by the same thread
- Must be released same number of times as acquired
- Cannot be released by a different thread

## 4. Algorithm

### 4.1 Overview

The function retrieves the critical section pointer from a documented offset in the environment structure and invokes the operating system's critical section acquisition primitive. The lock uses Windows CRITICAL_SECTION which supports recursive acquisition with spin-count optimization for better performance on multi-processor systems.

### 4.2 Detailed Steps

1. Check if environment pointer is NULL; if so, return immediately (no-op)
2. Retrieve the critical section pointer from environment structure at documented offset
3. Check if critical section pointer is NULL; if so, return immediately
4. Call operating system primitive to enter the critical section (blocks until acquired)
5. Return with lock held

### 4.3 Pseudocode

```
FUNCTION env_acquire_lock(env):
    IF env = NULL THEN
        RETURN
    END IF

    lock ← env.critical_section_pointer

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
- **Average case:** O(1) - Typically uncontended (env operations are infrequent)
- **Worst case:** O(n) where n = time waiting for lock holder (unbounded)

### 5.2 Space Complexity

- **Auxiliary space:** O(1)
- **Total space:** O(1)

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| NULL environment | N/A | Handled gracefully, returns without error |
| NULL lock | N/A | Handled gracefully, returns without error |

### 6.2 Error Behavior

The function handles NULL pointers defensively by returning early rather than propagating errors. This design allows calling code to be simpler since lock acquisition never fails visibly.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| NULL environment | env = NULL | Returns immediately, no lock acquired |
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
| cxf_setintparam | API | Before modifying integer parameters |
| cxf_setdblparam | API | Before modifying double parameters |
| cxf_newmodel | API | Before creating new model and adding to list |
| cxf_freemodel | API | Before removing model from environment list |
| cxf_log_printf | Internal | Before writing to log file |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_leave_critical_section | Paired release function (must be called after this) |
| cxf_acquire_solve_lock | Solve-level lock (acquired after environment lock) |
| cxf_release_solve_lock | Releases solve-level lock |

## 11. Design Notes

### 11.1 Design Rationale

The two-level lock hierarchy prevents deadlocks and allows concurrent optimization:

**Level 1 - Environment lock (this function):**
- Scope: All models in environment
- Protects: Parameters, model list, global counters, log files
- Hold time: Very short (microseconds)
- Acquired: During setup/configuration operations

**Level 2 - Solve lock:**
- Scope: Single optimization run
- Protects: Solver state, basis, working arrays
- Hold time: Longer (milliseconds during critical sections)
- Acquired: During optimization operations

This separation allows multiple models to optimize concurrently (each with its own solve lock) while protecting shared environment resources with the environment lock.

### 11.2 Performance Considerations

The lock is typically uncontended because environment operations (parameter changes, model creation) are infrequent compared to solve operations. The spin-count optimization (~4000 iterations) reduces context switches for brief contentions.

Hold time should be minimized - only held during actual parameter/state modifications, not during lengthy computations.

### 11.2 Lock Ordering for Deadlock Prevention

**Correct ordering:** Environment lock → Solve lock
**Incorrect ordering:** Solve lock → Environment lock (DEADLOCK RISK)

Always acquire environment lock first if both are needed. Release in reverse order: Solve lock → Environment lock.

### 11.3 Future Considerations

Could be enhanced with:
- Read-write lock for parameter reads (allow concurrent reads)
- Lock-free parameter access for read-heavy workloads
- Finer-grained locks for different environment subsystems

However, the current design is simple and sufficient for typical usage patterns where environment operations are rare compared to solve operations.

## 12. References

- Lock hierarchies: Standard technique for deadlock prevention in multi-level systems
- CxfEnv structure documentation: docs/reference/structures/CxfEnv.md

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
