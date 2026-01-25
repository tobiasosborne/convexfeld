# cxf_checkenv

**Module:** Validation
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Validates that a Convexfeld environment pointer is properly initialized. This is a critical guard function called at the entry point of almost every API function to ensure the environment is ready for use before proceeding with operations.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| env | CxfEnv* | Environment pointer to validate | Any pointer value | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | Error code: 0 if valid, nonzero if invalid |

### 2.3 Side Effects

None (read-only validation function).

## 3. Contract

### 3.1 Preconditions

- None (function must handle all possible input states)

### 3.2 Postconditions

- If return is 0, environment is confirmed valid and initialized
- If return is nonzero, environment is not usable
- Environment state is unchanged

### 3.3 Invariants

- Environment structure is not modified
- Function has no global side effects

## 4. Algorithm

### 4.1 Overview

Performs a fast, unlocked check of the environment's validity by examining its active flag. The check validates that the environment pointer is not NULL and that the environment has been successfully initialized.

### 4.2 Detailed Steps

1. Check if the environment pointer is NULL. If NULL, return error code for null argument.
2. Read the active flag from the environment structure (indicates initialization status).
3. If active flag is 0, return error code indicating environment is not initialized.
4. If all checks pass, return success code (0).

### 4.3 Pseudocode

```
FUNCTION validate_environment(env):
    IF env = NULL THEN
        RETURN ERROR_NULL_ARGUMENT
    END IF

    active ← read_active_flag(env)
    IF active = 0 THEN
        RETURN ERROR_NOT_INITIALIZED
    END IF

    RETURN SUCCESS
END FUNCTION
```

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1)
- **Average case:** O(1)
- **Worst case:** O(1)

All operations are constant-time pointer checks and memory reads.

### 5.2 Space Complexity

- **Auxiliary space:** O(1)
- **Total space:** O(1)

No additional memory allocated.

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| env is NULL | 1002 (CXF_ERR_NULL_ARGUMENT) | Null pointer provided |
| active flag is 0 | 1004 (CXF_ERR_NOT_IN_MODEL) | Environment not initialized |

### 6.2 Error Behavior

Function returns immediately on error. No cleanup necessary as no state is modified.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Null pointer | env = NULL | Return ERROR_NULL_ARGUMENT |
| Uninitialized environment | active flag = 0 | Return ERROR_NOT_INITIALIZED |
| Valid environment | active flag = 1 | Return SUCCESS (0) |
| Environment during free | active flag recently set to 0 | Return ERROR_NOT_INITIALIZED |

## 8. Thread Safety

**Thread-safe:** Conditionally

**Conditions:** Safe for concurrent reads. The active flag is read without locking, which is acceptable because:
- The flag transitions are rare (0→1 during initialization, 1→0 during destruction)
- Reading an integer is atomic on modern platforms
- Callers re-check the flag after acquiring locks

**Synchronization required:** None for this function, but callers should re-check after acquiring environment lock.

## 9. Dependencies

### 9.1 Functions Called

None (direct memory access only).

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_newmodel | Model | Before creating new model |
| cxf_addvar | Model | Before adding variables |
| cxf_getintparam | Parameters | Before reading parameters |
| ~All public API | Various | Entry validation |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_loadenv | Creates and initializes environment |
| cxf_freeenv | Destroys environment (sets active=0) |
| cxf_env_acquire_lock | Locks environment for thread-safe operations |

## 11. Design Notes

### 11.1 Design Rationale

This function is designed as an extremely fast guard condition that rejects invalid environments before expensive operations. It's called at the entry of virtually every API function, so performance is critical. The unlocked read is acceptable because false positives (reading active=1 but env freed concurrently) are caught by subsequent locked checks.

### 11.2 Performance Considerations

- Optimized for the common case (valid environment): ~2-3 instructions
- No locking overhead (unlocked read)
- Likely inlined at many call sites
- Branch predictor friendly (usually returns success)

### 11.3 Future Considerations

Could potentially validate additional fields (magic number, pointers) for robustness, but current design prioritizes speed.

## 12. References

- Convexfeld documentation: Environment management
- Standard practice: Guard conditions in API design

## 13. Validation Checklist

- [x] No code copied from implementation
- [x] Algorithm description is implementation-agnostic
- [x] All parameters documented
- [x] All error conditions listed
- [x] Complexity analysis complete
- [x] Edge cases identified
- [x] A competent developer could implement from this spec alone

---

---
*Convexfeld LP Solver Specification*
*Based on published optimization literature*
