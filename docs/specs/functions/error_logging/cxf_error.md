# cxf_error

**Module:** Error/Logging
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Formats and stores an error message in the environment's error buffer using printf-style formatting. This is the primary internal error reporting mechanism used throughout Convexfeld. The function formats a descriptive error message with variable arguments and stores it for later retrieval via the API, but does not produce any output itself.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| env | CxfEnv* | Environment to store error message in | Valid environment pointer or NULL | No |
| format | const char* | Printf-style format string | Valid format string or NULL | Yes |
| ... | variadic | Variable arguments matching format specifiers | Type-appropriate values | No |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| (none) | void | Function returns no value |
| env error buffer | char[512] | Error message written to buffer |

### 2.3 Side Effects

- Writes formatted error message to environment's error buffer (512 bytes)
- Acquires and releases environment's critical section for thread safety
- Does not produce any log output or invoke callbacks
- Does not modify error buffer if errorBufLocked flag is set (prevents overwriting during nested error handling)

## 3. Contract

### 3.1 Preconditions

- None (function is safe to call with NULL env)
- If env is non-NULL, it should be a valid environment pointer
- Format string should be well-formed if env is valid

### 3.2 Postconditions

On successful completion (env valid and not locked):
- [ ] Error buffer contains formatted message, null-terminated
- [ ] Message is truncated to 511 characters maximum (512th byte is null terminator)
- [ ] Critical section is released (same state as before call)
- [ ] errorBufLocked flag remains unchanged

If env is NULL or errorBufLocked is set:
- [ ] No changes to any state

### 3.3 Invariants

- [ ] Environment structure integrity maintained
- [ ] No memory allocation or deallocation occurs
- [ ] Thread safety locks are properly balanced (acquire/release)
- [ ] Error buffer pointer remains valid

## 4. Algorithm

### 4.1 Overview

The function implements thread-safe error message formatting and storage. It first validates the environment pointer and checks whether the error buffer is locked (indicating nested error handling is in progress). If the buffer is available, the function acquires the environment's critical section lock, formats the message using standard variable argument list processing, ensures null termination, and releases the lock. The locking mechanism prevents message corruption when multiple threads report errors concurrently, while the lock flag prevents overwriting the root cause error message during nested error conditions.

### 4.2 Detailed Steps

1. Validate the environment pointer; if NULL, return immediately without action
2. Check the errorBufLocked flag; if non-zero, return immediately to preserve existing error
3. Retrieve the error buffer pointer from env
4. If error buffer is NULL (uninitialized), return immediately
5. Retrieve the critical section pointer from env
6. If critical section exists, acquire it by entering the critical section
7. Initialize variable argument list processing from the format parameter
8. Format the message into the error buffer using vsnprintf with 512-byte limit
9. Finalize variable argument list processing
10. Ensure defensive null termination at buffer[511]
11. If critical section was acquired, release it by leaving the critical section
12. Return

### 4.3 Pseudocode (if needed)

```
FUNCTION cxf_error(env, format, varargs):
    IF env = NULL THEN
        RETURN
    END IF

    errorBufLocked ← LOAD_INT(env)
    IF errorBufLocked ≠ 0 THEN
        RETURN  // Preserve existing error during nested handling
    END IF

    errorBuffer ← LOAD_POINTER(env)
    IF errorBuffer = NULL THEN
        RETURN
    END IF

    criticalSection ← LOAD_POINTER(env)
    IF criticalSection ≠ NULL THEN
        ENTER_CRITICAL_SECTION(criticalSection)
    END IF

    // Format message using standard library vsnprintf
    argList ← INITIALIZE_VARARGS(format)
    VSNPRINTF(errorBuffer, 512, format, argList)
    FINALIZE_VARARGS(argList)

    // Defensive null termination
    errorBuffer[511] ← '\0'

    IF criticalSection ≠ NULL THEN
        LEAVE_CRITICAL_SECTION(criticalSection)
    END IF
END FUNCTION
```

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) - NULL check returns immediately
- **Average case:** O(n) where n is formatted message length
- **Worst case:** O(n) where n is formatted message length

Where:
- n = length of formatted error message (capped at 511 characters)

### 5.2 Space Complexity

- **Auxiliary space:** O(1) - uses only stack variables for pointers and va_list
- **Total space:** O(1) - writes to pre-allocated 512-byte buffer

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| NULL environment | (none) | Function returns silently; no error reported |
| Error buffer locked | (none) | Function returns silently; preserves existing error |
| NULL error buffer | (none) | Function returns silently; uninitialized environment |
| Format string too long | (none) | Message truncated to 511 characters silently |

### 6.2 Error Behavior

The function is designed to be fail-safe and never produce errors itself (to avoid infinite recursion in error handling). All error conditions result in silent return with no state changes. Message truncation occurs automatically via vsnprintf's built-in length limiting. The errorBufLocked mechanism prevents overwriting error messages during nested error handling, ensuring the root cause is preserved.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| NULL environment | env = NULL | Returns immediately, no crash |
| Empty format string | format = "" | Writes empty string to buffer |
| Format string with no specifiers | format = "error text" | Copies literal text to buffer |
| Very long message | format + args → 600 chars | Truncates to 511 chars + null |
| Nested error handling | errorBufLocked = 1 | Returns without overwriting |
| Concurrent calls | Multiple threads | Serialized by critical section |
| NULL format string | format = NULL | Likely crashes (caller responsibility) |

## 8. Thread Safety

**Thread-safe:** Yes

The function is fully thread-safe through the use of the environment's CRITICAL_SECTION lock. Multiple threads can safely call cxf_error on the same environment; access to the error buffer is serialized. Threads calling cxf_error on different environments execute in parallel without contention since each environment has its own lock.

**Synchronization required:** None from caller; function manages its own locking internally via EnterCriticalSection/LeaveCriticalSection on env

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| vsnprintf | C Standard Library | Format variable arguments into buffer |
| EnterCriticalSection | Windows API | Acquire exclusive access to error buffer |
| LeaveCriticalSection | Windows API | Release exclusive access to error buffer |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_addvar | API | Reports invalid variable type, index errors |
| cxf_getintattr | API | Reports attribute not found errors |
| cxf_setdblparam | API | Reports parameter validation errors |
| cxf_checkenv | Internal | Reports NULL or invalid environment |
| (hundreds more) | Various | Error reporting throughout API |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_geterrormsg | Retrieves the message written by this function |
| cxf_errorlog | Outputs error message to log destinations |
| cxf_log_printf | Printf-style logging (different buffer, produces output) |

## 11. Design Notes

### 11.1 Design Rationale

The separation of error message formatting (cxf_error) from error logging (cxf_errorlog) allows flexible error handling patterns. Callers can format an error message, decide whether to log it based on context, and return an error code independently. The fixed 512-byte buffer avoids dynamic allocation in error paths, preventing allocation failures during error reporting. The errorBufLocked flag solves a critical problem in nested error handling where a secondary error (e.g., in error reporting code itself) could overwrite the original root cause error message.

### 11.2 Performance Considerations

Error paths are not performance-critical since they only execute on failure. The fixed-size buffer avoids malloc overhead. vsnprintf is reasonably fast (<1 microsecond for typical messages). The critical section overhead is minimal (~50 nanoseconds on modern CPUs). No I/O operations occur in this function, keeping it extremely fast.

### 11.3 Future Considerations

The 512-byte buffer size is sufficient for current error messages but may be limiting for very detailed diagnostic messages in future versions. The Windows-specific CRITICAL_SECTION could be abstracted for cross-platform support. The errorBufLocked mechanism could be enhanced with reference counting for deeply nested error scenarios.

## 12. References

- C Standard Library: vsnprintf documentation for format string processing
- Windows API: CRITICAL_SECTION synchronization primitive
- Error Handling Patterns: Nested error handling in system software

## 13. Validation Checklist

Before finalizing this spec, verify:

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
