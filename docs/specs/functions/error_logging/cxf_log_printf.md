# cxf_log_printf

**Module:** Error/Logging
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Provides printf-style formatted logging to all configured log destinations in a single convenient function call. This is the primary logging function used throughout the Convexfeld solver for progress messages, warnings, status updates, and informational output. It combines message formatting with multi-destination output, eliminating the need for callers to format messages separately.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| env | CxfEnv* | Environment containing logging configuration | Valid environment pointer or NULL | No |
| format | const char* | Printf-style format string | Valid format string or NULL | Yes |
| ... | variadic | Variable arguments matching format specifiers | Type-appropriate values | No |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| (none) | void | Function returns no value |
| Log file | FILE | Formatted message appended if configured |
| Console | stdout | Formatted message written based on OutputFlag |
| Callback | User function | Formatted message passed to callback if registered |

### 2.3 Side Effects

- Formats message to temporary stack buffer (1024 bytes)
- Writes formatted message to log file if configured
- Writes formatted message to console if OutputFlag > 0
- Invokes log callback with formatted message if registered
- Acquires and releases critical section during all I/O operations
- Flushes file and console buffers for immediate visibility
- Does NOT write to error buffer (unlike cxf_error)

## 3. Contract

### 3.1 Preconditions

- None (function is safe to call with NULL parameters)
- If env is non-NULL, OutputFlag determines whether output occurs
- Format string should be well-formed if env is valid

### 3.2 Postconditions

On successful completion with valid parameters and OutputFlag > 0:
- [ ] Message formatted to temporary buffer with null termination
- [ ] Message truncated to 1023 characters if necessary
- [ ] Message written to all configured log destinations
- [ ] All buffers flushed
- [ ] Critical section released
- [ ] Error buffer unchanged

If env is NULL, format is NULL, or OutputFlag <= 0:
- [ ] No output produced
- [ ] No state changes

### 3.3 Invariants

- [ ] Environment structure integrity maintained
- [ ] Critical section properly balanced
- [ ] No heap memory allocated
- [ ] Error buffer remains unchanged
- [ ] Log file handle remains valid

## 4. Algorithm

### 4.1 Overview

The function implements printf-style formatted logging with thread-safe multi-destination output. It first validates parameters and checks whether logging is enabled via the OutputFlag parameter. If disabled, it returns immediately without formatting. Otherwise, it allocates a temporary buffer on the stack (1024 bytes), acquires the environment's critical section for thread safety, formats the message using standard variable argument processing, and sequentially writes the formatted output to each configured destination (log file, console, callback). The critical section ensures that the entire format-and-output operation is atomic, preventing message interleaving from concurrent threads. After all output is complete, the critical section is released and the function returns.

### 4.2 Detailed Steps

1. Validate environment and format pointers; return immediately if either is NULL
2. Load OutputFlag value from env
3. If OutputFlag <= 0, return immediately (logging disabled)
4. Allocate temporary buffer on stack (1024 bytes)
5. Retrieve critical section pointer from env
6. If critical section exists, acquire it
7. Initialize variable argument list processing from format parameter
8. Format message into temporary buffer using vsnprintf with 1024-byte limit
9. Finalize variable argument list processing
10. Ensure defensive null termination at buffer[1023]
11. Retrieve log file handle from env (approximate offset)
12. If log file is valid, write formatted message with fprintf (no additional newline), flush file
13. If OutputFlag >= 1, write formatted message to stdout with printf, flush stdout
14. Retrieve log callback function pointer and user data from environment (offsets TBD)
15. If callback is registered, invoke it with formatted message
16. If critical section was acquired, release it
17. Return

### 4.3 Pseudocode (if needed)

```
FUNCTION cxf_log_printf(env, format, varargs):
    IF env = NULL OR format = NULL THEN
        RETURN
    END IF

    outputFlag ← LOAD_INT(env)
    IF outputFlag ≤ 0 THEN
        RETURN  // Logging disabled
    END IF

    DECLARE buffer[1024] ON STACK

    criticalSection ← LOAD_POINTER(env)
    IF criticalSection ≠ NULL THEN
        ENTER_CRITICAL_SECTION(criticalSection)
    END IF

    // Format message
    argList ← INITIALIZE_VARARGS(format)
    VSNPRINTF(buffer, 1024, format, argList)
    FINALIZE_VARARGS(argList)
    buffer[1023] ← '\0'

    // Write to log file
    logFile ← LOAD_POINTER(env)
    IF logFile ≠ NULL THEN
        FPRINTF(logFile, "%s", buffer)
        FFLUSH(logFile)
    END IF

    // Write to console
    IF outputFlag ≥ 1 THEN
        PRINTF("%s", buffer)
        FFLUSH(stdout)
    END IF

    // Invoke callback
    logCallback ← LOAD_POINTER(env + callback_offset)
    IF logCallback ≠ NULL THEN
        callbackData ← LOAD_POINTER(env + callback_data_offset)
        INVOKE logCallback(callbackData, buffer)
    END IF

    IF criticalSection ≠ NULL THEN
        LEAVE_CRITICAL_SECTION(criticalSection)
    END IF
END FUNCTION
```

### 4.4 Mathematical Foundation (if applicable)

Not applicable - this is a string formatting and I/O function without mathematical content.

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) - NULL check or OutputFlag check returns immediately
- **Average case:** O(n + f + c) where n = formatted message length, f = file I/O, c = callback time
- **Worst case:** O(n + f + c) - same as average case

Where:
- n = length of formatted message (capped at 1023 characters)
- f = file I/O latency (~5-10 microseconds)
- c = callback execution time (user-dependent, potentially unbounded)

### 5.2 Space Complexity

- **Auxiliary space:** O(1) - 1024-byte stack buffer (constant size)
- **Total space:** O(1) - no heap allocation

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| NULL environment | (none) | Returns silently without output |
| NULL format string | (none) | Returns silently without output |
| OutputFlag <= 0 | (none) | Returns silently (logging disabled) |
| Format string too long | (none) | Message truncated to 1023 characters silently |
| File write failure | (none) | Fails silently; continues with other destinations |
| Callback exception | (none) | User callback errors propagate to caller |
| Invalid format specifiers | (none) | Behavior depends on vsnprintf (typically undefined output) |

### 6.2 Error Behavior

The function is designed to be fail-safe and never report errors itself. All error conditions result in silent return or truncation. This prevents errors in logging code from interfering with the primary operation being logged. If any single output destination fails, the function continues with remaining destinations.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| NULL environment | env = NULL | Returns immediately, no output |
| NULL format | format = NULL | Returns immediately, no output |
| Empty format | format = "" | Outputs empty string to destinations |
| No format specifiers | "literal text\n" | Outputs literal text as-is |
| OutputFlag = 0 | Silent mode | No output produced |
| OutputFlag = 1 | Normal mode | Output to console and file |
| Very long message | Result > 1023 chars | Truncates to 1023 chars + null |
| Format with newline | "text\n" | Newline included in output (not added) |
| Multiple newlines | "line1\nline2\n" | All newlines preserved |
| Concurrent calls | Multiple threads | Serialized; no message interleaving |
| Invalid specifier | "%q" | Undefined behavior (vsnprintf dependent) |
| Mismatched args | "%d %d", one_arg | Undefined behavior (vsnprintf dependent) |

## 8. Thread Safety

**Thread-safe:** Yes

The function is fully thread-safe through use of the environment's CRITICAL_SECTION. Multiple threads can safely call cxf_log_printf on the same environment; the entire format-and-output operation is atomic. This prevents message interleaving and ensures complete messages are written to each destination. The stack-allocated buffer is thread-local, avoiding shared state. Threads logging to different environments execute in parallel.

**Synchronization required:** None from caller; function manages internal locking. However, if the user's log callback is not thread-safe, the user is responsible for synchronization within the callback.

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| vsnprintf | C Standard Library | Format variable arguments into buffer |
| fprintf | C Standard Library | Write formatted message to log file |
| printf | C Standard Library | Write formatted message to console |
| fflush | C Standard Library | Flush file and stdout buffers |
| EnterCriticalSection | Windows API | Acquire exclusive lock for I/O |
| LeaveCriticalSection | Windows API | Release exclusive lock |
| (user callback) | User code | Custom log message handling |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| Simplex routines | Solver | Iteration progress logging |
| Presolve | Solver | Reduction statistics logging |
| MIP solver | Solver | Node exploration progress |
| Barrier | Solver | Barrier iteration updates |
| Refactorization | Linear Algebra | Basis refactorization events |
| Parameter validation | Parameters | Warning messages for invalid values |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_errorlog | Lower-level function for pre-formatted messages; cxf_log_printf may delegate to it |
| cxf_error | Error-specific formatting (writes to error buffer); different purpose |
| cxf_geterrormsg | Retrieves error buffer (not affected by this function) |
| cxf_register_log_callback | Registers callback invoked by this function |
| snprintf | Simpler non-outputting formatter |

## 11. Design Notes

### 11.1 Design Rationale

The combination of formatting and output in a single function provides maximum convenience for the common case of logging formatted messages. The 1024-byte buffer size is chosen as a balance between accommodating detailed messages and avoiding excessive stack usage. Stack allocation avoids heap overhead and eliminates failure paths from malloc. The decision to NOT write to the error buffer (unlike cxf_error) reflects this function's role as general logging rather than error reporting. This prevents informational messages from appearing as errors in cxf_geterrormsg.

### 11.2 Performance Considerations

This function may be called extremely frequently during optimization (potentially thousands or millions of times for progress logging in large MIP solves). The OutputFlag early exit is critical for performance in silent mode, avoiding all formatting and I/O overhead. Stack-allocated buffer is much faster than heap allocation (~1 nanosecond vs ~100 nanoseconds). vsnprintf is reasonably efficient (~1-2 microseconds for typical messages). File and console I/O are the slowest operations (~10-20 microseconds each). Total overhead is approximately 20-50 microseconds per call when logging is enabled, which is acceptable for typical logging frequencies (every 100-1000 iterations).

### 11.3 Future Considerations

The 1024-byte buffer may be insufficient for very detailed diagnostic messages; future versions might support dynamic sizing or larger buffers. The synchronous I/O may cause performance issues in extremely verbose logging scenarios; asynchronous logging with a background thread could improve performance. Structured logging (JSON, key-value pairs) would improve log parsing and analysis. Log level support (ERROR, WARN, INFO, DEBUG) would allow finer-grained control than the current OutputFlag.

## 12. References

- C Standard Library: vsnprintf for variable argument formatting
- C Standard Library: fprintf, printf for I/O operations
- Windows API: CRITICAL_SECTION for thread synchronization
- Printf Format Specifiers: Standard printf format string syntax

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
