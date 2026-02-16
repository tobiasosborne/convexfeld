# cxf_terminate

**Module:** API - Optimization Control
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Requests early termination of a currently running optimization process. This function provides a thread-safe mechanism to signal the solver to stop, typically invoked from callbacks, separate monitoring threads, or signal handlers. The termination is cooperative - the solver checks flags periodically and stops at a safe point, preserving partial solution data.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| model | CxfModel* | Model whose optimization should terminate | Valid or NULL pointer | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | void | No return value |

### 2.3 Side Effects

- Sets termination flag in environment structure (one of three possible locations)
- May trigger callback termination logic if callbacks are active
- No immediate effect on solver - flag is polled during optimization
- After solver recognizes termination, Status attribute will be set to INTERRUPTED

## 3. Contract

### 3.1 Preconditions

- [ ] Model pointer may be NULL (safe to call with NULL)
- [ ] If model is non-NULL, must have valid magic number
- [ ] Model environment pointer must be valid if model is valid
- [ ] Function can be called whether or not optimization is running

### 3.2 Postconditions

- [ ] If optimization is active, termination flag is set
- [ ] If optimization is not active, no change occurs
- [ ] If model is invalid, function returns immediately with no effect
- [ ] Function always returns (never blocks or throws)

### 3.3 Invariants

- [ ] Model structure validity unchanged
- [ ] Environment structure pointers unchanged
- [ ] Only termination flags are modified
- [ ] Function is reentrant - multiple calls safe

## 4. Algorithm

### 4.1 Overview

The function implements a lightweight, lock-free termination signaling mechanism. It first performs minimal validation - checking for NULL model and verifying the model's magic number for basic validity. Then it checks if optimization is currently active by querying the environment's optimization state.

If optimization is not running, the function returns immediately without modifying any state. If optimization is active, the function determines which termination path to use based on model state: callback-aware termination if callbacks are registered, direct flag pointer if the solver has provided one, or the default environment flag otherwise. This three-path approach accommodates different solver phases and optimization modes.

The implementation avoids locks and complex operations to maintain thread safety and low latency. All flag writes are to single integer/pointer-sized memory locations, which are atomic on x86-64 architectures. This design allows safe invocation from signal handlers and tight timing-critical loops.

### 4.2 Detailed Steps

1. **NULL Check**: If model pointer is NULL, return immediately
2. **Magic Validation**: Read first integer of model structure; if not equal to MODEL_MAGIC (0xC0FEFE1D), return immediately
4. **Optimization Check**: Call check_optimizing function on environment; if return value is non-zero (optimization not active), return immediately

### 4.3 Pseudocode

```
procedure terminate(model):
    # Fast-path validation
    if model = NULL:
        return

    if model.magic ≠ MODEL_MAGIC:
        return

    # Check if optimization is running
    env ← model.env
    if check_optimizing(env) ≠ 0:
        return  # Not optimizing

    # Determine termination path
    if model.callbackCount > 0:
        callback_terminate(model)
        return

    if env.terminateFlagPtr ≠ NULL:
        *env.terminateFlagPtr ← 1
        return

    env.terminateFlag1 ← 1
    return
```

### 4.4 Mathematical Foundation

Not applicable - this is a control flow function without mathematical computation.

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) - NULL or invalid magic check
- **Average case:** O(1) - flag assignment
- **Worst case:** O(1) - callback termination path

All paths execute constant-time operations.

### 5.2 Space Complexity

- **Auxiliary space:** O(1) - only uses registers and stack-local pointers
- **Total space:** O(1) - no allocations

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| NULL model | None | Function returns silently |
| Invalid magic | None | Function returns silently |
| Optimization not active | None | Function returns silently (no-op) |

Note: This function never returns an error code - it uses void return type and fails silently.

### 6.2 Error Behavior

The function is designed for graceful degradation. Invalid inputs or inappropriate calling contexts simply cause early return with no side effects. This makes it safe to call speculatively without checking preconditions. The function never logs errors, throws exceptions, or modifies global state beyond the termination flags.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| NULL pointer | model = NULL | Returns immediately, no error |
| Invalid model | Corrupted magic number | Returns immediately, no effect |
| Not optimizing | Called when solver idle | check_optimizing returns non-zero, early return |
| Already terminated | Called multiple times | Flags set multiple times (idempotent) |
| Concurrent calls | Multiple threads call simultaneously | All threads set same flag, safe due to atomic writes |
| Signal handler context | Called from SIGINT handler | Safe - no locks, minimal stack usage |
| Callback context | Called from user callback | Uses callback-aware path |
| Optimization just finished | Race between completion and terminate | If flag set before poll, may abort post-processing |

## 8. Thread Safety

**Thread-safe:** Yes (unconditionally)

The function is fully thread-safe without external synchronization. All reads are of immutable or synchronized data. All writes are to single-word memory locations (atomic on x86-64). No locks are acquired, preventing deadlock. The function can be safely called from:
- Multiple concurrent threads
- Signal handlers (async-signal-safe)
- Timer callbacks
- User optimization callbacks
- Any context where model pointer remains valid

**Synchronization required:** None

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| check_optimizing | Environment | Query if optimization is currently active |
| callback_terminate | Callbacks | Callback-aware termination logic |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| User application | External | Explicit termination request |
| Signal handlers | User code | Ctrl+C or timeout handling |
| Monitoring threads | User code | Time/resource limit enforcement |
| Callbacks | User code | Early stopping criteria |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_optimize | Counterpart - initiates optimization |
| cxf_update | Unrelated - processes model modifications |
| cxf_setintparam | Complementary - TimeLimit parameter for automatic termination |
| Callback functions | Coordination - callbacks can call cxf_terminate |

## 11. Design Notes

### 11.1 Design Rationale

The void return type and silent failure mode are intentional design choices. Since termination is a best-effort operation (solver may complete before flag is checked), there's no meaningful error state to report. The function is designed to be callable from restrictive contexts like signal handlers, which prohibits complex error handling.

The three-path termination mechanism (callback/direct/default) accommodates different solver phases. During callback execution, the callback system needs coordination to ensure clean shutdown. When the solver provides a direct flag pointer, that's the fastest path. The default environment flag is the fallback when neither special case applies.

The magic number check is a lightweight validity test without full model validation. This allows termination even if the model is partially corrupted, as long as the magic number and environment pointer are intact. Full validation (via cxf_checkmodel) would be too expensive and unnecessary for this hot path.

### 11.2 Performance Considerations

The function is designed for minimal latency - typically 10-50 CPU cycles on x86-64 hardware. All operations are register-based or single cache-line reads/writes. The check_optimizing call may access multiple fields but they're typically cache-resident during active optimization.

The callback_terminate path may be more expensive (hundreds of cycles) due to callback state management, but this is acceptable since callbacks themselves have higher overhead. The function never allocates memory, acquires locks, or performs I/O, making it suitable for real-time contexts.

### 11.3 Future Considerations

Potential enhancements: support for gradual termination with timeout (hard vs soft termination), priority levels for termination (immediate vs finish-current-operation), and termination reason codes to distinguish user termination from timeout/resource exhaustion.

## 12. References

- POSIX signal-safety requirements (async-signal-safe functions)
- C11 memory model - atomic operations on naturally-aligned types
- Convexfeld documentation on callbacks and termination

## 13. Validation Checklist

- [x] No code copied from implementation
- [x] Algorithm description is implementation-agnostic
- [x] All parameters documented
- [x] All error conditions listed (none in this case)
- [x] Complexity analysis complete
- [x] Edge cases identified
- [x] A competent developer could implement from this spec alone

---

*Reviewed by: Pending*

---
*Convexfeld LP Solver Specification*
*Based on published optimization literature*
