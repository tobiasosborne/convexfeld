# cxf_post_optimize_callback

**Module:** Callbacks
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Invokes the user-registered callback function immediately after optimization completes, providing a post-optimization hook for users to inspect final solution and statistics, log results, perform post-processing, or trigger follow-up actions. This callback serves as the exit point from solver execution, enabling cleanup and result handling after computational work is complete.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| model | CxfModel* | Model that was optimized | Valid model pointer | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | 0 on success, non-zero if callback encountered error |

### 2.3 Side Effects

- Increments callback invocation counter in CallbackState
- Updates cumulative callback execution time
- Invokes user-provided callback function (user-defined side effects)
- Unlike pre-optimization callback, does NOT set termination flag (optimization already complete)

## 3. Contract

### 3.1 Preconditions

- [ ] Model pointer must be valid
- [ ] Optimization has completed (solution and statistics available if successful)
- [ ] If callback is registered and enabled, callback function pointer must be valid
- [ ] Called from single-threaded context (optimization thread)
- [ ] Environment lock held by caller

### 3.2 Postconditions

- [ ] Callback invocation counter incremented by 1
- [ ] Cumulative callback time updated with elapsed time
- [ ] Return value reflects callback status (0 = success)
- [ ] Model state is unchanged (callback should be read-only)

### 3.3 Invariants

- [ ] Model and environment structures remain valid
- [ ] Callback state structure remains consistent
- [ ] Solution data (if present) remains unchanged
- [ ] No memory leaks from callback invocation

## 4. Algorithm

### 4.1 Overview

The function acts as a wrapper around the user callback, handling infrastructure concerns (timing, statistics) while delegating domain logic to user code. The structure is nearly identical to the pre-optimization callback, but the semantic context differs: the model now contains final solution data and statistics. The function follows the same guard-check pattern, ensuring safety even if components are missing. Unlike the pre-optimization callback, the return value typically does not affect solver behavior since optimization is already complete, but it may be logged or used for error reporting.

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
10. Set the "where" code to indicate post-optimization phase (e.g., cxf__CB_POLLING)
11. Prepare callback data pointer (typically the CallbackState itself)
12. Increment the callback invocation counter in CallbackState
13. Capture start timestamp using high-resolution timer
14. Invoke the user callback function with (model, callback data, where code, user data)
15. Capture end timestamp
16. Calculate elapsed time and add to cumulative callback time in CallbackState
17. Return the callback's return value to caller (typically logged but not enforced)

### 4.3 Pseudocode

```
FUNCTION PostOptimizeCallback(model):
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

    // Note: No termination flag set since optimization is complete
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
| User callback returns non-zero | User-defined | May indicate callback error (logged but not enforced) |
| User callback throws exception | N/A | Undefined behavior (user responsibility) |

### 6.2 Error Behavior

The function gracefully handles missing components (NULL environment, NULL callback state, disabled callbacks) by returning early with success code. User callback errors (non-zero return) are propagated but typically do not affect model state since optimization is complete. Exception safety depends on user callback implementation.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| No environment | model->env = NULL | Return 0, no callback invoked |
| No callback registered | callbackState = NULL | Return 0, no callback invoked |
| Callback disabled | enableFlag = 0 | Return 0, no callback invoked |
| Callback returns error | User returns non-zero | Returns non-zero, logged but no termination |
| Optimization failed | No solution available | Callback invoked, user can check status |

## 8. Thread Safety

**Thread-safe:** Yes (conditionally)

The function is thread-safe when called from a single thread (documented callback behavior). All modifications to CallbackState are non-concurrent since callbacks are always invoked from the optimization thread.

**Synchronization required:** Environment lock must be held by caller (standard for optimization pipeline). User callback should not call thread-unsafe Convexfeld functions unless documented as safe.

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_get_timestamp | Timing | Capture high-resolution timestamps for timing |
| User callback | User Code | Domain-specific post-optimization logic |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_optimize_internal | Optimization | Called at end of optimization pipeline |
| cxf_solve_dispatch | Solver Dispatch | Called after algorithm completes |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_pre_optimize_callback | Complementary function called before optimization |
| cxf_setcallbackfunc | Registers the callback that this function invokes |
| cxf_getintattr / cxf_getdblattr | Used by callback to query solution attributes |

## 11. Design Notes

### 11.1 Design Rationale

The symmetric design with pre-optimization callback simplifies mental model for users: same callback function handles both entry and exit points, distinguished only by "where" code. Post-optimization callbacks are essential for result logging, custom output formats, and triggering dependent processes. The non-enforced return value design acknowledges that optimization is complete and cannot be meaningfully aborted.

### 11.2 Performance Considerations

Infrastructure overhead is minimal (10-20 CPU cycles for guard checks, 2 timestamp calls). The function is called only once per optimization run at the very end, so overhead is negligible. Users can perform expensive post-processing (database writes, file I/O) in this callback without affecting solver performance.

### 11.3 Future Considerations

Could be extended to:
- Support multiple post-processing callbacks with priorities
- Provide summary statistics about callback performance
- Allow callbacks to request re-optimization with modified parameters
- Add hooks for asynchronous post-processing (return before callback completes)

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
