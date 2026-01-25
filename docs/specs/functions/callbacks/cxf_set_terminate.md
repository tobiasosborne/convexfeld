# cxf_set_terminate

**Module:** Callbacks
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Sets termination flags in the environment to signal that optimization should exit, used internally by the solver when limits are exceeded or errors occur. Unlike the callback-aware termination function, this simplified version does not handle callback-specific cleanup, making it suitable for high-frequency internal use in limit checking and error handling paths.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| env | CxfEnv* | Environment in which to set termination flag | Valid environment pointer or NULL | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | void | No return value |

### 2.3 Side Effects

- Sets primary environment termination flag to 1
- Sets direct flag pointer (if allocated) to 1
- Sets async termination flag in AsyncState (if present) to 1
- Does NOT set callback-specific flags (handled separately)

## 3. Contract

### 3.1 Preconditions

- [ ] Function may be called with NULL environment (safe no-op)
- [ ] Typically called from solver internal code (limit checkers, error handlers)
- [ ] May be called from any thread
- [ ] Caller is responsible for setting appropriate model status after termination

### 3.2 Postconditions

- [ ] Primary environment flag is set to 1
- [ ] Direct flag pointer (if allocated) is set to 1
- [ ] Async flag (if present) is set to 1
- [ ] Optimization loop will detect termination on next flag check
- [ ] Callback flags are NOT modified (caller's responsibility if needed)

### 3.3 Invariants

- [ ] Environment structure remains valid
- [ ] Flag values are monotonic (0 → 1, never reset during optimization)
- [ ] No memory allocation or deallocation occurs
- [ ] Thread-safe flag setting (atomic operations)

## 4. Algorithm

### 4.1 Overview

The function implements a simplified termination signaling strategy for internal solver use. It sets the minimum set of flags needed for optimization loop termination detection without the overhead of callback-specific handling. This design separates internal termination (limits, errors) from user-requested termination (which requires callback coordination), allowing high-frequency limit checking without unnecessary callback infrastructure access.

### 4.2 Detailed Steps

1. Validate that environment pointer is non-NULL; if NULL, return immediately
2. Set primary environment termination flag to 1 (main flag checked by optimization loops)
3. Check if direct flag pointer is allocated in environment
4. If allocated, dereference and set to 1 (for tight solver loops)
5. Retrieve AsyncState pointer from environment
6. If AsyncState exists, set async termination flag to 1 (for concurrent/remote optimization)
7. Do NOT modify CallbackState (callback termination handled separately by callback-aware function)

### 4.3 Pseudocode

```
FUNCTION SetTerminate(env):
    IF env = NULL THEN
        RETURN
    END IF

    // Always set primary flag
    env.terminateFlag1 ← 1

    // Set direct pointer if allocated
    IF env.terminateFlagPtr ≠ NULL THEN
        *env.terminateFlagPtr ← 1
    END IF

    // Set async flag if present
    IF env.asyncState ≠ NULL THEN
        env.asyncState.terminationFlag ← 1
    END IF

    // Note: CallbackState NOT modified (use cxf_callback_terminate for that)
END FUNCTION
```

### 4.4 Mathematical Foundation

Not applicable - state management operation.

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) - early return if NULL
- **Average case:** O(1) - constant number of flag writes (2-3)
- **Worst case:** O(1) - constant time

### 5.2 Space Complexity

- **Auxiliary space:** O(1) - no additional allocation
- **Total space:** O(1) - operates on existing structures

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| NULL environment | N/A | Safe no-op, returns immediately |

### 6.2 Error Behavior

The function handles NULL environment gracefully by returning early. No error codes are returned (void function). No error messages are logged. The caller is responsible for setting appropriate model status attributes after calling this function.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| NULL environment | env = NULL | Silent return, no operation |
| No direct pointer | terminateFlagPtr = NULL | Sets primary flag only, skips direct pointer |
| No async state | asyncState = NULL | Sets primary and direct flags, skips async |
| Multiple calls | Already terminated | Idempotent, sets flags to 1 again (safe) |
| Concurrent calls | Multiple threads calling | Safe, all write same value (1) |

## 8. Thread Safety

**Thread-safe:** Yes

All flag writes are atomic (int-sized, aligned) on x86-64 architecture. Multiple concurrent calls are safe and idempotent (all set flag to 1). No locks required. No read-modify-write operations. Multiple writers setting to same value (1) have no conflict.

**Synchronization required:** None - function uses only atomic write operations.

## 9. Dependencies

### 9.1 Functions Called

None - this is a leaf function that only sets flags.

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| Time limit handler | Limit Checking | Called when elapsed time exceeds limit |
| Iteration limit handler | Limit Checking | Called when iteration count exceeds limit |
| Node limit handler | MIP | Called when node count exceeds limit |
| Solution limit handler | MIP | Called when solution count exceeds limit |
| Memory limit handler | Memory Management | Called when memory usage exceeds limit |
| Numerical error handler | Error Handling | Called when numerical instability detected |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_callback_terminate | Extended version with callback handling |
| cxf_check_terminate | Checks flags set by this function |
| cxf_terminate | Public API that may call callback-aware version |

## 11. Design Notes

### 11.1 Design Rationale

Separating internal termination (this function) from user termination (callback-aware version) avoids unnecessary overhead in high-frequency limit checking paths. Solver internal code calls this function frequently (every iteration for time checks), so minimal work is critical. The callback-specific flag is intentionally NOT set here because internal termination does not require callback cleanup - callbacks will naturally detect termination via the primary flag.

### 11.2 Performance Considerations

Execution time is approximately 15-30 CPU cycles (2-3 memory writes). This is critical since limit checkers may call this function thousands of times per second in tight loops. No system calls, no locks, and no callback infrastructure access ensures minimal overhead. The function is an excellent candidate for inlining.

### 11.3 Future Considerations

Could be extended to:
- Accept termination reason parameter for diagnostics
- Support conditional termination (only set if not already set)
- Add performance counters for termination frequency
- Provide fast-path macro for inlining in hot loops

## 12. References

- x86-64 memory model: Aligned int writes are atomic
- Standard limit checking patterns in optimization solvers

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
