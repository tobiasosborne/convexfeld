# cxf_freeenv

**Module:** API Environment
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Deallocates an environment and releases all associated resources including memory and file handles. This function implements reference counting to support shared environments and ensures thread-safe cleanup through critical section synchronization. Must be called for every environment created, regardless of initialization success or failure.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| env | CxfEnv* | Environment to deallocate | Valid CxfEnv pointer or NULL | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | Error code (undocumented): 0 on success, error code on failure |

**Note:** Official API documentation declares return type as `void`, but actual implementation returns `int`. Callers typically ignore the return value per documented API contract.

### 2.3 Side Effects

- Deallocates heap memory for environment structure
- Closes log file if open
- Decrements reference count for shared environments
- Frees parent/master environment if reference count reaches zero

## 3. Contract

### 3.1 Preconditions

- All models created from this environment must be freed first
- Environment pointer may be NULL (returns error code without crashing)
- If environment has active async operations, they will be forcibly terminated

### 3.2 Postconditions

On success:
- Environment memory is deallocated
- All file handles closed
- If reference count reached zero, master environment also freed

On failure (NULL pointer):
- Returns error code 0x2830 (10288)
- No memory operations performed

### 3.3 Invariants

- Function is idempotent: calling on already-freed environment may produce error but won't crash
- Other environments are not affected

## 4. Algorithm

### 4.1 Overview

cxf_freeenv implements a cleanup algorithm that handles reference-counted shared environments and ensures thread-safe resource deallocation. The function uses a critical section to safely decrement reference counts, then conditionally performs either deferred cleanup (if other references exist) or immediate deallocation.

### 4.2 Detailed Steps

1. **Null check**
   - If env is NULL, return error code 0x2830 immediately
   - No further processing occurs

2. **Reference counting setup**
   - Identify whether this is a shared environment (master is non-NULL)
   - Prepare to track both the current environment and master for potential freeing

3. **Thread-safe reference decrement (if shared environment)**
   - Record whether reference count reached zero
   - Exit critical section
   - This ensures atomic reference count updates across threads

4. **Deferred cleanup decision**
   - If current env IS the master AND reference count > 0:
     - Environment is still in use by other references
     - Perform deferred cleanup instead of full deallocation:

       a. **Log warning** (if verbosity >= 1):
          - "Environment still referenced so free is deferred"

       b. **Return** without freeing environment

5. **Full deallocation (if reference count is zero or not master)**
   - Call internal environment free function for current environment
   - If current env is NOT master AND master's reference count hit zero:
     - Also call internal environment free function for master environment
   - This ensures both child and parent are freed when last reference disappears

6. **Return status code**
   - Return 0 on success, error code on failure
   - Note: documented API ignores return value (void contract)

### 4.3 Pseudocode

```
FUNCTION cxf_freeenv(env):
    // Step 1: Null check
    IF env = NULL THEN
        RETURN ERROR_NULL_ENVIRONMENT (0x2830)

    // Step 2: Get master environment
    refCountHitZero ← FALSE

    // Step 3: Handle reference counting
    IF masterEnv ≠ NULL THEN
        ENTER_CRITICAL_SECTION(masterEnv.criticalSection)
        masterEnv.refCount ← masterEnv.refCount - 1
        refCountHitZero ← (masterEnv.refCount = 0)
        LEAVE_CRITICAL_SECTION(masterEnv.criticalSection)

        // Step 4: Deferred cleanup if still referenced
        IF env = masterEnv AND NOT refCountHitZero THEN
            IF env.verbosity ≥ 1 THEN
                LOG("Environment still referenced so free is deferred")
            RETURN status

    // Step 5: Full deallocation
    status ← InternalEnvironmentFree(env)

    IF env ≠ masterEnv AND refCountHitZero THEN
        status ← InternalEnvironmentFree(masterEnv)

    RETURN status
```

### 4.4 Mathematical Foundation

Reference counting invariant:
- Let R(e) = reference count of environment e
- For master environment m with children {c₁, c₂, ..., cₙ}:
  - R(m) = n (number of child environments)
  - When cᵢ is freed: R(m) ← R(m) - 1
  - m is deallocated when R(m) = 0 AND last child is freed

Thread safety:
- Reference count operations are atomic via critical section
- Mutex invariant: ∀ threads t₁, t₂ accessing R(m), operations are serialized

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) - NULL pointer or simple environment with no resources
- **Average case:** O(1) - Typical environment cleanup with file/memory deallocation
- **Worst case:** O(1) - All operations are constant time

### 5.2 Space Complexity

- **Auxiliary space:** O(1) - Fixed stack allocation for cleanup variables
- **Total space:** O(1) - Deallocates memory rather than allocating


## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| NULL environment pointer | 0x2830 (10288) | Special error code for NULL env (not standard CXF_ERR_NULL_ARGUMENT) |
| Models not freed | Undefined | Behavior undefined if models still exist |

### 6.2 Error Behavior

On NULL pointer:
- Returns error code immediately without accessing memory
- No cleanup operations performed

On other errors:
- Best-effort cleanup continues
- Errors during cleanup are logged but may not halt deallocation
- Memory is freed even if network disconnect fails
- Return value indicates success/failure (though typically ignored)

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| NULL pointer | env = NULL | Return error 0x2830, no crash |
| Already freed | Pointer to freed memory | Undefined behavior (may crash or return error) |
| Active models exist | Environment with unfreed models | Undefined behavior, may cause corruption |
| Reference count > 1 | Shared environment | Decrement count, defer actual free |
| Last reference freed | Reference count becomes 0 | Free both child and master environments |
| Concurrent free | Multiple threads freeing same env | Critical section prevents corruption, one succeeds |

## 8. Thread Safety

**Thread-safe:** Conditionally

- Reference counting operations are protected by critical section
- Safe to call from multiple threads on different environments
- NOT safe to call from multiple threads on same environment simultaneously (though critical section provides some protection)

**Synchronization required:**

Internal synchronization:
- EnterCriticalSection/LeaveCriticalSection ensure atomic operations

Caller responsibilities:
- Do not free environment concurrently from multiple threads
- Ensure all models using environment are freed first
- No synchronization needed for freeing different environments

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| EnterCriticalSection | Windows API | Thread synchronization for reference count |
| LeaveCriticalSection | Windows API | Release critical section lock |
| cxf_log | Logging | Log deferred free warnings |
| cxf_env_free_internal | Environment | Actual memory deallocation |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| User application | External | Cleanup at program termination or after optimization |
| cxf_freemodel | Model | May call if model owns private environment |
| Error cleanup paths | Various | Cleanup after initialization failures |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_loadenv | Inverse operation - creates initialized environment |
| cxf_emptyenv | Inverse operation - creates empty environment |
| cxf_freemodel | Model deallocation - must be called before this |
| cxf_geterrormsg | Error retrieval - cannot be used after environment is freed |

## 11. Design Notes

### 11.1 Design Rationale

**Reference Counting:** Supports pattern where environments can be "copied" or shared between models. This reduces initialization overhead.

**Return Value Discrepancy:** Official API declares `void` return type, but implementation returns `int`. This allows internal error tracking while maintaining simple API contract for users who don't need error details.

**Deferred Free with Active References:** When reference count > 0, environment is not freed. This ensures memory still in use is not deallocated.

**NULL Pointer Tolerance:** Returns error code rather than crashing on NULL, making defensive programming easier.

### 11.2 Performance Considerations

- Normal case is very fast: O(1) memory deallocation
- Reference counting adds critical section overhead (microseconds)

Best practices:
- Free models before freeing environment to avoid undefined behavior
- Reuse environments when possible to avoid repeated initialization

### 11.3 Future Considerations

The eight reserved parameters in creation functions suggest extensibility for new resource types that may require cleanup.

## 12. References

- Windows CRITICAL_SECTION documentation (thread synchronization)

## 13. Validation Checklist

- [X] No code copied from implementation
- [X] Algorithm description is implementation-agnostic
- [X] All parameters documented
- [X] All error conditions listed
- [X] Complexity analysis complete
- [X] Edge cases identified
- [X] A competent developer could implement from this spec alone

---

*Reviewed by: Pending*

---
*Convexfeld LP Solver Specification*
*Based on published optimization literature*
