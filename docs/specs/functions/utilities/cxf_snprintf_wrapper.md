# cxf_snprintf_wrapper

**Module:** Utilities
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Provides a safe wrapper for the snprintf string formatting function with guaranteed null-termination and input validation. Ensures consistent behavior across platforms (particularly for pre-C99 implementations that didn't guarantee null-termination on truncation). Used throughout Convexfeld for formatting error messages, log output, and general string generation.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| buffer | char* | Destination buffer for formatted string | Non-NULL | Yes |
| size | size_t | Size of buffer in bytes | > 0 | Yes |
| format | const char* | Printf-style format string | Non-NULL | Yes |
| ... | variadic | Arguments matching format specifiers | Type-matched | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | Number of characters that would be written (excluding null), or -1 on error |
| buffer | char* | Modified to contain formatted string with guaranteed null termination |

### 2.3 Side Effects

- Modifies buffer contents
- Ensures buffer[size-1] = '\0' for defensive null termination

## 3. Contract

### 3.1 Preconditions

- [ ] Buffer pointer must not be NULL
- [ ] Buffer size must be greater than 0
- [ ] Format string must not be NULL
- [ ] Variable arguments must match format specifiers in type and count

### 3.2 Postconditions

- [ ] Buffer is null-terminated (buffer[size-1] = '\0' at minimum)
- [ ] At most size-1 characters written (excluding null terminator)
- [ ] Return value indicates total length that would be written if size were unlimited
- [ ] On error (NULL buffer or size=0), returns -1

### 3.3 Invariants

- [ ] Buffer always null-terminated on successful execution
- [ ] Buffer contents valid up to min(return_value, size-1) characters

## 4. Algorithm

### 4.1 Overview

The function wraps vsnprintf with defensive validation and explicit null-termination to ensure portable behavior across pre-C99 and C99 implementations. It validates inputs before formatting and force-terminates the buffer afterwards for safety.

### 4.2 Detailed Steps

1. Validate buffer pointer
   - If NULL, return -1 (error)
2. Validate buffer size
   - If 0, return -1 (error)
3. Initialize variable argument list (va_start)
4. Call vsnprintf to format string into buffer
   - Writes at most size-1 characters plus null terminator
   - Returns number of characters that would be written (excluding null)
5. End variable argument list (va_end)
6. Defensively null-terminate buffer at position size-1
   - Ensures termination even if pre-C99 implementation doesn't guarantee it
7. Return result from vsnprintf

### 4.3 Pseudocode

```
FUNCTION cxf_snprintf_wrapper(buffer, size, format, ...):
    IF buffer = NULL THEN
        RETURN -1
    IF size = 0 THEN
        RETURN -1

    va_list args
    va_start(args, format)
    result ← vsnprintf(buffer, size, format, args)
    va_end(args)

    # Defensive null termination
    buffer[size - 1] ← '\0'

    RETURN result
END FUNCTION
```

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(n) where n is output string length (simple format string)
- **Average case:** O(n + k) where k is format complexity
- **Worst case:** O(n × k) for complex format strings with many specifiers

Where:
- n = length of output string
- k = number and complexity of format specifiers

Typical range: 100-1000 CPU cycles depending on format complexity

### 5.2 Space Complexity

- **Auxiliary space:** O(1) - va_list structure on stack
- **Total space:** O(1) - output written to provided buffer

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| NULL buffer | -1 | Buffer pointer is NULL |
| Zero size | -1 | Buffer size is 0 |
| Format encoding error | -1 | Invalid format string (from vsnprintf) |

### 6.2 Error Behavior

On error, function returns -1. Buffer contents are undefined for NULL buffer or size=0 cases. For format errors, buffer contains partial output up to the error point and is null-terminated.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Exact fit | Output length = size-1 | Full string written, null-terminated |
| Truncation | Output length > size-1 | Truncated at size-1, null-terminated, return > size |
| NULL buffer | buffer = NULL | Return -1 immediately |
| Zero size | size = 0 | Return -1 immediately |
| Empty format | format = "" | Write empty string (just null terminator) |
| No format specifiers | format = "text" | Copy literal text |

## 8. Thread Safety

**Thread-safe:** Yes (conditionally)

The function is thread-safe per invocation because:
- Each call uses its own va_list on the stack
- No shared global state modified
- vsnprintf is thread-safe

However, concurrent writes to the same buffer by multiple threads would create a race condition (caller's responsibility).

**Synchronization required:** None for function itself, but caller must synchronize access to shared buffers

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| vsnprintf | C99 stdio.h | Format string with variable argument list |
| va_start | C99 stdarg.h | Initialize variable argument list |
| va_end | C99 stdarg.h | Clean up variable argument list |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_error | Error | Format error messages |
| cxf_log_printf | Logging | Format log messages |
| Various | Throughout | String formatting for diagnostics |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| sprintf | Unsafe version without size limit |
| snprintf | Standard library function wrapped by this |
| vsnprintf | Underlying formatting function |

## 11. Design Notes

### 11.1 Design Rationale

The wrapper exists primarily for portability:
1. Pre-C99 MSVC _snprintf didn't null-terminate on truncation
2. Provides consistent error handling across platforms
3. Explicit size validation prevents buffer overflows
4. Defensive null-termination adds safety layer

### 11.2 Performance Considerations

Overhead is minimal (~2-3 cycles for validation, ~1 cycle for defensive termination). Total time dominated by vsnprintf formatting (~100-1000 cycles). This is acceptable for error/logging paths which are not performance-critical.

### 11.3 Future Considerations

- Could add format string validation to detect errors earlier
- Could provide formatted length calculation without writing (pass NULL buffer)
- Could add overflow detection callback for diagnostics

## 12. References

- ISO C99 Standard: Section 7.19.6.5 (snprintf function)
- CERT C Coding Standard: STR31-C (Guarantee null-termination)

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
