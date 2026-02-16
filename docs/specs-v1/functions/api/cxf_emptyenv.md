# cxf_emptyenv

**Module:** API Environment
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Creates an uninitialized Convexfeld environment that allows parameter configuration before activation. This two-stage approach enables setting parameters before the environment is fully initialized. The created environment must be activated with cxf_startenv before it can be used for optimization.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| envP | CxfEnv** | Output pointer where created environment reference is stored | Valid memory address | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | Error code: 0 on success, non-zero error code on failure |
| *envP | CxfEnv* | Created empty environment pointer (always set, even on failure) |

### 2.3 Side Effects

- Allocates heap memory for CxfEnv structure
- Initializes parameter table with default values
- Does NOT read any files (no convexfeld.env)

## 3. Contract

### 3.1 Preconditions

- envP must be a valid pointer to a CxfEnv* variable

### 3.2 Postconditions

On success (return value 0):
- *envP contains a valid empty environment pointer
- Environment is in "unstarted" state
- All parameters are at default values
- No files have been read
- Environment can accept parameter changes via cxf_setintparam, cxf_setdblparam, cxf_setstrparam
- Environment must be activated with cxf_startenv before use

On failure (return value non-zero):
- *envP contains a partial environment pointer (non-NULL)
- Error message is available via cxf_geterrormsg(*envP)
- Partial environment must still be freed with cxf_freeenv

### 3.3 Invariants

- Environment pointer is always set (never leaves *envP unchanged)
- No global state is modified
- No file system or network I/O occurs
- Existing environments are not affected

## 4. Algorithm

### 4.1 Overview

cxf_emptyenv performs minimal initialization: it creates the raw environment structure with default settings and records the creation status. Unlike cxf_loadenv, it deliberately skips file reading and full initialization. This allows applications to configure parameters before the environment is fully activated.

### 4.2 Detailed Steps

1. **Create raw environment structure**
   - Allocate CxfEnv structure on heap
   - Initialize parameter table with default values for all parameters
   - Set up internal memory pools for parameter storage
   - Initialize thread synchronization primitives (critical sections)
   - Pass version/capability flags (0x3003) identifying Convexfeld version and features
   - Pass client type (-1) indicating local client
   - Pass eight reserved extension parameters (currently all zero)

2. **Record initial status**
   - Store creation result code in environment structure
   - Initialize error message buffer to empty state
   - Mark environment state as "unstarted"

3. **Return environment and status**
   - Store environment pointer in *envP (always, even on error)
   - Return error code (0 for success, non-zero for failure)

### 4.3 Mathematical Foundation

Not applicable (administrative function).

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1)
- **Average case:** O(1)
- **Worst case:** O(1)

All cases perform fixed-size allocation and initialization. No file I/O or network operations.

Where:
- Constant time regardless of parameter count (parameter table is fixed-size)

### 5.2 Space Complexity

- **Auxiliary space:** O(1) - Fixed-size environment structure
- **Total space:** O(p) - p = number of parameter slots in parameter table (fixed constant)

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| NULL envP pointer | 1002 | NULL_ARGUMENT - invalid output pointer |
| Memory allocation failure | 1001 | OUT_OF_MEMORY - insufficient heap memory for environment structure |

### 6.2 Error Behavior

On any error:
- Environment pointer is still allocated and returned (if possible)
- Error code is stored in environment structure
- Detailed error message is available via cxf_geterrormsg
- Caller must call cxf_freeenv to clean up even on failure

The function does not throw exceptions or terminate the process. All errors are returned via error code.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Insufficient memory | Heap allocation fails | Return OUT_OF_MEMORY, environment may be NULL or partial |
| NULL output pointer | envP = NULL | Return NULL_ARGUMENT immediately |
| Concurrent creation | Multiple threads calling | Each gets independent environment (thread-safe) |
| After cxf_loadenv | Create empty env after loaded env | Independent environments, no interaction |

## 8. Thread Safety

**Thread-safe:** Yes

This function can be called concurrently from multiple threads. Each call creates an independent environment with its own internal state. No shared global state is modified.

**Synchronization required:** None

- Environment structure allocation is thread-local
- No file system or network I/O
- No process-wide state changes
- Each environment is independent

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_env_create_internal | Environment | Allocate and initialize raw environment structure with default parameters |
| cxf_env_set_status | Environment | Record operation status code in environment structure |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| User application | External | When parameter configuration is needed before environment activation |
| cxf_loadclientenv | Client API | May use empty env pattern for remote connections |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_loadenv | Alternative creation - creates and starts environment in one step |
| cxf_startenv | Complement function - activates empty environment (must be called after this) |
| cxf_freeenv | Inverse operation - deallocates environment |
| cxf_setintparam | Parameter setting - configures empty environment before starting |
| cxf_setdblparam | Parameter setting - configures empty environment before starting |
| cxf_setstrparam | Parameter setting - configures empty environment before starting |

## 11. Design Notes

### 11.1 Design Rationale

The two-stage initialization pattern (cxf_emptyenv + cxf_startenv) provides flexibility for applications that need to configure parameters before the environment is fully activated. This is useful when:

1. Parameters must be set programmatically before activation
2. Different configurations are needed based on runtime conditions
3. Testing or debugging requires specific parameter values

Without cxf_emptyenv, users would need to write parameters to convexfeld.env file, which is less flexible and harder to manage programmatically.

### 11.2 Performance Considerations

cxf_emptyenv is extremely fast (microseconds) because it only allocates memory and initializes data structures. No I/O occurs. This makes it safe to create and destroy environments frequently if needed, though best practice is still to reuse environments.

The function uses the same internal creation function as cxf_loadenv but skips the expensive steps (file I/O and full initialization).

### 11.3 Future Considerations

The eight reserved zero parameters in cxf_env_create_internal provide extensibility for future versions without breaking API compatibility. These could enable:
- Feature capability negotiation
- Version-specific optimizations
- Compatibility flags for behavior changes

## 12. References

- Convexfeld API documentation (environment management)

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
