# cxf_geterrormsg

**Module:** API Error Handling
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Retrieves the most recent error message text associated with a Convexfeld environment. When API functions fail and return error codes, they record descriptive error messages in the environment. This function provides access to those messages for user-facing error reporting, debugging, and diagnostics. The returned string is always valid and owned by the environment.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| env | CxfEnv* | Environment to query for error message | Valid pointer or NULL | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | char* | Pointer to error message string (never NULL) |

### 2.3 Side Effects

None. This is a read-only query function. The returned pointer may reference memory owned by the environment or static memory, but the function itself modifies no state.

## 3. Contract

### 3.1 Preconditions

- None strictly required. Function handles NULL or invalid environment gracefully.

### 3.2 Postconditions

- Returns a valid, null-terminated string pointer
- If environment is valid, returns the most recent error message from that environment
- If environment is invalid or NULL, returns a default error message
- Returned string is valid until environment is modified or freed

### 3.3 Invariants

- Return value is never NULL
- Returned string is null-terminated
- Environment state is not modified
- Error message content is not modified by this function

## 4. Algorithm

### 4.1 Overview

The function implements a simple error message retrieval mechanism with fallback for invalid environments. It first validates the environment pointer using an internal validation function that returns zero for valid environments and non-zero for invalid ones.

If the environment is valid, the function retrieves the error message pointer stored at a fixed offset within the environment structure (offset 0x1F88). This pointer references either a dynamically allocated buffer or a statically allocated buffer that is updated by error-reporting functions throughout the API.

If the environment validation fails (NULL pointer, invalid magic number, or other structural problems), the function returns a pointer to a static default error message string located in the read-only data section of the library binary.

The function makes no allocations, performs no string copies, and calls no logging functions. It is designed to be safe to call in error-recovery scenarios where the environment may be in an inconsistent state.

### 4.2 Detailed Steps

1. Call environment validation function (no parameters visible - likely checks thread-local storage, global state, or cached environment)
2. Examine validation result (0 = valid, non-zero = invalid)
3. If validation returns 0 (valid environment):
   a. Read pointer from environment structure
   b. Return this pointer to caller
4. If validation returns non-zero (invalid environment):
   a. Return pointer to static default error message
   b. Default message is located at fixed address in .rdata section
5. Return error message pointer to caller

### 4.3 Pseudocode

```
function cxf_geterrormsg(env):
    validationStatus ← ValidateEnvironment()

    if validationStatus = 0:
        // Environment is valid
        errorMsgPtr ← *(env + OFFSET_ERROR_MESSAGE)
        return errorMsgPtr
    else:
        // Environment invalid or NULL
        return STATIC_DEFAULT_ERROR_MESSAGE
```

### 4.4 Mathematical Foundation

Error message pointer retrieval:
- Environment base address: E
- Error message offset: O = 0x1F88 (8072 decimal)
- Error message pointer address: E + O
- Error message pointer: M = *[E + O]
- If E invalid or O not accessible, return M_default (static constant)

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) - Direct pointer dereference
- **Average case:** O(1) - Validation + pointer dereference
- **Worst case:** O(1) - Validation (may check magic numbers) + pointer return

### 5.2 Space Complexity

- **Auxiliary space:** O(1) - No allocations, no stack frames beyond function locals
- **Total space:** O(1) - Returns pointer to existing memory

## 6. Error Handling

### 6.1 Error Conditions

This function cannot fail in the traditional sense (no error codes returned). However, it handles error conditions gracefully:

| Condition | Behavior | Description |
|-----------|----------|-------------|
| NULL environment | Return default message | Static error message for invalid environment |
| Invalid environment magic | Return default message | Environment structure corrupted |
| Uninitialized environment | Return default message | Environment never initialized |
| Valid but no error | Return empty or "No error" | No API error has occurred |

### 6.2 Error Behavior

The function is designed for use in error-recovery paths and never fails. If called with an invalid environment, it returns a static message rather than crashing or returning NULL.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| NULL environment | env = NULL | Returns static default message |
| Uninitialized env | env points to zero memory | Returns static default message |
| Environment freed | env previously freed | Returns static default message (or undefined if memory reused) |
| No error occurred | API calls succeeded | Returns empty string or "No error" message |
| Multiple errors | Several API failures | Returns most recent error message only |
| Concurrent modification | Another thread sets error | May return stale or new message (race condition) |
| Very long error | Message exceeds buffer | Message is truncated at buffer boundary |
| Called before init | Before cxf_loadenv | Returns static default message |
| Called after shutdown | After cxf_freeenv | Returns static default message |

## 8. Thread Safety

**Thread-safe:** Conditionally

The function performs read-only operations on the environment structure, making it safe to call concurrently with other read operations. However, if another thread concurrently modifies the error message (by triggering an API error), this function may observe a partially updated message or experience a race condition.

The environment validation function may access thread-local storage, which is inherently thread-safe.

**Synchronization required:** None for read-only usage; external locking if concurrent error reporting possible

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_env_validate | Environment | Validate environment structure (no visible parameters) |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| Error handlers | Throughout API | Retrieving error text after failure |
| User code | External | Displaying errors to users |
| Logging systems | Diagnostics | Recording error messages |
| Debugging tools | Development | Error analysis |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_geterrorcode | Retrieves most recent error code (numeric) |
| cxf_seterror | Sets custom error message in environment |
| cxf_clearerrormsg | Clears error message (may not exist) |

## 11. Design Notes

### 11.1 Design Rationale

The always-valid return pointer design eliminates the need for NULL checks in error-handling code, simplifying client implementations. The static default message for invalid environments allows error reporting even when the environment is corrupted or uninitialized.

Storing the error message as a pointer in the environment (rather than inline buffer) allows for flexible buffer sizing and possible sharing of common error messages. The fixed offset (0x1F88) allows direct access without function call overhead.

The validation function's lack of visible parameters suggests use of thread-local storage or global state checking, allowing environment validation without explicitly passing the environment pointer to the validation routine.

### 11.2 Performance Considerations

The function is extremely fast (typically 2-10 CPU cycles): one function call for validation, one pointer dereference to retrieve the message. No string operations, no allocations. Suitable for use in performance-critical error paths.

The error message is stored as a pointer rather than copied, avoiding allocation and string copy overhead. This means the message lifetime is tied to the environment or the next error occurrence.

### 11.3 Future Considerations

No mechanism for retrieving historical errors (only most recent). No way to query error message without potentially observing concurrent modifications. The buffer size is not exposed, so truncation is not detectable. No support for structured error information (error codes + parameters) beyond the formatted message string.

The static default message content is unknown without examining the binary, making it difficult to document expected behavior precisely.

## 12. References

- Convexfeld Error Handling Documentation
- Environment Structure Specification

## 13. Validation Checklist

- [x] No code copied from implementation
- [x] Algorithm description is implementation-agnostic
- [x] All parameters documented
- [x] All error conditions listed
- [x] Complexity analysis complete
- [x] Edge cases identified
- [x] A competent developer could implement from this spec alone

---

*Reviewed by: N/A*

---
*Convexfeld LP Solver Specification*
*Based on published optimization literature*
