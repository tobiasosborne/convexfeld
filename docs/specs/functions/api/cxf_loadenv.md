# cxf_loadenv

**Module:** API Environment
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Creates and initializes a complete Convexfeld environment ready for immediate use. This is the primary entry point for creating an environment that reads configuration files and optionally sets up logging in a single operation. The environment serves as the foundation for all optimization models and must be created before any model operations.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| envP | CxfEnv** | Output pointer where created environment reference is stored | Valid memory address | Yes |
| logfilename | const char* | Path to log file for environment output | NULL, empty string, or valid file path | No |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | Error code: 0 on success, non-zero error code on failure |
| *envP | CxfEnv* | Created environment pointer (always set, even on failure) |

### 2.3 Side Effects

- Allocates heap memory for CxfEnv structure
- Searches current directory for convexfeld.env and loads parameter settings
- Opens log file if specified (creates or truncates file)

## 3. Contract

### 3.1 Preconditions

- envP must be a valid pointer to a CxfEnv* variable
- If logfilename is non-NULL and non-empty, it must be a valid path with write permissions

### 3.2 Postconditions

On success (return value 0):
- *envP contains a valid, fully initialized environment pointer
- Log file is open if requested
- Environment is ready for model creation
- Configuration from convexfeld.env has been applied

On failure (return value non-zero):
- *envP contains a partial environment pointer (non-NULL)
- Error message is available via cxf_geterrormsg(*envP)
- Partial environment must still be freed with cxf_freeenv

### 3.3 Invariants

- Environment pointer is always set (never leaves *envP unchanged)
- Existing environments are not affected

## 4. Algorithm

### 4.1 Overview

cxf_loadenv performs a three-stage initialization sequence. First, it creates the raw environment structure with default parameter values and allocates internal data structures. Second, it records the creation status in the environment. Third, it configures logging and loads user-specified parameter overrides from the convexfeld.env configuration file.

The function is designed to always return an environment pointer, even on failure, to enable error message retrieval. This means cleanup is required regardless of success.

### 4.2 Detailed Steps

1. **Create raw environment structure**
   - Allocate CxfEnv structure on heap
   - Initialize parameter table with default values
   - Set up internal memory pools
   - Initialize thread synchronization primitives
   - Pass version/capability flags (0x3003) and client type (-1 for local)
   - Pass reserved extension parameters (currently all zero)

2. **Record initial status**
   - Store creation result code in environment structure
   - Initialize error message buffer
   - Set environment state marker

3. **Configure logging and parameters (only if creation succeeded)**
   - If logfilename is non-NULL and non-empty:
     - Open log file for writing (create or truncate)
     - Set up log buffering and formatting
   - Search current directory for convexfeld.env file
   - If found, parse PRM-format parameter settings
   - Apply parameter overrides to environment
   - Mark environment as "started" and ready for use

4. **Update final status**
   - Record final initialization result in environment
   - Set environment state to active or error state

5. **Return environment and status**
   - Store environment pointer in *envP (always, even on error)
   - Return error code (0 for success)

### 4.3 Mathematical Foundation

Not applicable (administrative function).

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) - Pure memory allocation, no files exist
- **Average case:** O(n) - n = number of parameters in convexfeld.env
- **Worst case:** O(n) - n parameters

Where:
- n = number of parameter settings in convexfeld.env file

### 5.2 Space Complexity

- **Auxiliary space:** O(1) - Fixed-size environment structure
- **Total space:** O(p) - p = number of parameter entries in parameter table

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| NULL envP pointer | 1002 | NULL_ARGUMENT - invalid output pointer |
| Memory allocation failure | 1001 | OUT_OF_MEMORY - insufficient heap memory |
| Log file cannot be created | I/O error | File system permission or path invalid |
| Invalid convexfeld.env syntax | Parameter error | Configuration file parsing failure |

### 6.2 Error Behavior

On any error:
- Environment pointer is still allocated and returned
- Error code is stored in environment structure
- Detailed error message is available via cxf_geterrormsg
- Partial initialization state is preserved for debugging
- Caller must call cxf_freeenv to clean up even on failure

The function does not throw exceptions or terminate the process. All errors are returned via error code.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| NULL logfilename | logfilename = NULL | Create environment without log file, load convexfeld.env if present |
| Empty logfilename | logfilename = "" | Same as NULL - no logging |
| No convexfeld.env file | File not in current directory | Proceed with default parameters |
| Log file already open | Log path points to locked file | Return I/O error |
| Concurrent creation | Multiple threads calling simultaneously | Each thread gets independent environment (no shared state) |

## 8. Thread Safety

**Thread-safe:** Yes

This function can be called concurrently from multiple threads. Each call creates an independent environment with its own internal state.

**Synchronization required:** None from caller perspective

Internal synchronization:
- Environment structure allocation is thread-local
- No environment-level locking needed during creation

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_env_create_internal | Environment | Allocate and initialize raw environment structure |
| cxf_env_set_status | Environment | Record operation status in environment |
| cxf_env_load_logfile | Environment | Configure logging and load convexfeld.env parameters |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| User application | External | Primary API entry point for creating environments |
| cxf_loadmodel | Model I/O | May create environment automatically when loading model files |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_emptyenv | Alternative creation - creates uninitialized environment for parameter configuration |
| cxf_startenv | Complement to cxf_emptyenv - finalizes empty environment |
| cxf_freeenv | Inverse operation - deallocates environment |
| cxf_geterrormsg | Error retrieval - gets detailed error message from environment |

## 11. Design Notes

### 11.1 Design Rationale

The all-in-one approach of cxf_loadenv simplifies the most common use case: creating an environment with default behavior. The function combines three operations (create, status update, configuration) that could be separate API calls.

The decision to always return an environment pointer, even on failure, enables error message retrieval. This is safer than returning NULL, which would make error diagnosis difficult.

The flags parameter (0x3003) encodes version and capability information, allowing binary compatibility checking and feature negotiation.

### 11.2 Performance Considerations

- File I/O for convexfeld.env can be slow on network file systems
- Best practice: create environment once per program and reuse for multiple models
- Multi-threaded programs should create one environment per thread for safety

### 11.3 Future Considerations

The eight reserved zero parameters in cxf_env_create_internal suggest planned extensibility for future versions. These could be used for feature flags, version negotiation, or extension points.

## 12. References

- PRM file format documentation (for convexfeld.env parsing)

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
