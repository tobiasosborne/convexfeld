# cxf_pre_optimize_callback

**Module:** Callbacks
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Invokes the user-registered callback function immediately before optimization begins, providing a pre-optimization hook for users to inspect initial model state, modify parameters, perform validation checks, or conditionally abort optimization. This callback serves as the entry point into solver execution from the user's perspective, enabling setup and decision-making before computational work begins.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| model | CxfModel* | Model being optimized | Valid model pointer | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | 0 to continue optimization, non-zero to abort |

### 2.3 Side Effects

- Increments callback invocation counter in CallbackState
- Updates cumulative callback execution time
- Sets termination flag in AsyncState if callback returns non-zero
- Invokes user-provided callback function (user-defined side effects)

## 3. Contract

### 3.1 Preconditions

- [ ] Model pointer must be valid
- [ ] If callback is registered and enabled, callback function pointer must be valid
- [ ] Called from single-threaded context (optimization thread)
- [ ] Environment lock held by caller

### 3.2 Postconditions

- [ ] Callback invocation counter incremented by 1
- [ ] Cumulative callback time updated with elapsed time
- [ ] If return value is non-zero, termination flag is set
- [ ] If return value is zero, optimization proceeds normally

### 3.3 Invariants

- [ ] Model and environment structures remain valid
- [ ] Callback state structure remains consistent
- [ ] No memory leaks from callback invocation

## 4. Algorithm

### 4.1 Overview

The function acts as a wrapper around the user callback, handling infrastructure concerns (timing, statistics, termination) while delegating domain logic to user code. It follows a guard-check pattern (validating environment, callback state, and enable flag) before invoking the callback, ensuring safety even if components are missing. Timing is tracked using high-resolution timestamps before and after callback execution. If the callback requests termination by returning non-zero, the function sets the appropriate flag in AsyncState to signal the solver.

### 4.2 Detailed Steps

1. Retrieve the environment pointer from the model structure
2. If environment is NULL, return success (no callback infrastructure)
3. Retrieve CallbackState pointer from the environment
4. If CallbackState is NULL, return success (no callback registered)
5. Check the callback enable flag in CallbackState
6. If disabled, return success (callback temporarily disabled)
7. Retrieve the callback function pointer from the environment
8. If function pointer is NULL, return success (no callback function set)
9. Retrieve user data pointer from CallbackState
10. Set the "where" code to indicate pre-optimization phase (e.g., cxf__CB_POLLING)
11. Prepare callback data pointer (typically the CallbackState itself)
12. Increment the callback invocation counter in CallbackState
13. Capture start timestamp using high-resolution timer
14. Invoke the user callback function with (model, callback data, where code, user data)
15. Capture end timestamp
16. Calculate elapsed time and add to cumulative callback time in CallbackState
17. If callback returned non-zero, set the termination flag in AsyncState
18. Return the callback's return value to caller

### 4.3 Pseudocode

```
FUNCTION PreOptimizeCallback(model):
    env ← model.environment
    IF env = NULL THEN
        RETURN 0
    END IF

    callbackState ← env.callbackState
    IF callbackState = NULL OR NOT callbackState.enabled THEN
        RETURN 0
    END IF

    callbackFunc ← env.callbackFunction
    IF callbackFunc = NULL THEN
        RETURN 0
    END IF

    usrdata ← callbackState.userData
    whereCode ← cxf__CB_POLLING
    cbdata ← callbackState

    callbackState.invocationCount ← callbackState.invocationCount + 1
    startTime ← GetHighResolutionTimestamp()

    result ← callbackFunc(model, cbdata, whereCode, usrdata)

    endTime ← GetHighResolutionTimestamp()
    callbackState.cumulativeTime ← callbackState.cumulativeTime + (endTime - startTime)

    IF result ≠ 0 THEN
        env.asyncState.terminationFlag ← 1
    END IF

    RETURN result
END FUNCTION
```

### 4.4 Mathematical Foundation

Not applicable - callback invocation wrapper.

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) - early return if callback not registered
- **Average case:** O(U) - where U is user callback execution time
- **Worst case:** O(U) - dominated by user callback

Where U = execution time of user callback (unbounded, user-defined)

### 5.2 Space Complexity

- **Auxiliary space:** O(1) - fixed local variables
- **Total space:** O(1) + O(U) - user callback may allocate memory

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| User callback returns non-zero | User-defined | Interpreted as termination request |
| User callback throws exception | N/A | Undefined behavior (user responsibility) |

### 6.2 Error Behavior

The function gracefully handles missing components (NULL environment, NULL callback state, disabled callbacks) by returning early with success code. User callback errors (non-zero return) are propagated to caller and trigger termination flag setting. Exception safety depends on user callback implementation.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| No environment | model->env = NULL | Return 0, no callback invoked |
| No callback registered | callbackState = NULL | Return 0, no callback invoked |
| Callback disabled | enableFlag = 0 | Return 0, no callback invoked |
| Callback requests abort | User returns non-zero | Sets termination flag, returns non-zero |
| First callback invocation | invocationCount = 0 | Increments to 1, proceeds normally |

## 8. Thread Safety

**Thread-safe:** Yes (conditionally)

The function is thread-safe when called from a single thread (documented callback behavior). All modifications to CallbackState are non-concurrent since callbacks are always invoked from the optimization thread.

**Synchronization required:** Environment lock must be held by caller (standard for optimization pipeline). User callback should not call thread-unsafe Convexfeld functions unless documented as safe.

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_get_timestamp | Timing | Capture high-resolution timestamps for timing |
| User callback | User Code | Domain-specific pre-optimization logic |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_optimize_internal | Optimization | Called at start of optimization pipeline |
| cxf_solve_dispatch | Solver Dispatch | Called before algorithm selection |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_post_optimize_callback | Complementary function called after optimization |
| cxf_setcallbackfunc | Registers the callback that this function invokes |
| cxf_cbget | Allows callback to query model state during invocation |

## 11. Design Notes

### 11.1 Design Rationale

The wrapper pattern separates infrastructure concerns (timing, statistics, termination) from user logic, ensuring consistent behavior across all callback invocations. Early return guards prevent crashes when components are missing. The "where" code allows the same user callback to handle multiple invocation points (pre, during, post optimization) with different logic.

### 11.2 Performance Considerations

Infrastructure overhead is minimal (10-20 CPU cycles for guard checks, 2 timestamp calls). The function is called only once per optimization run, so overhead is negligible compared to solver time. Timing granularity depends on system timer resolution (typically nanosecond or microsecond precision).

### 11.3 Future Considerations

Could be extended to:
- Support multiple callback priorities (ordered invocation)
- Allow callbacks to be temporarily suspended without unregistering
- Provide callback context stack for nested invocations
- Add profiling hooks for callback performance analysis

## 12. References

- Standard callback patterns in optimization solvers

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
