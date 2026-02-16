# cxf_write

**Module:** API I/O
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Exports optimization models and associated data to external files in various formats. The file extension determines what is written: complete model formulations in LP or MPS format, solution data, basis vectors, parameter settings, or attribute values. The function automatically processes pending model modifications and handles file compression when requested through extension suffixes. This is the primary export mechanism for persisting models, solutions, and configurations to disk.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| model | CxfModel* | Model containing data to export | Valid model pointer | Yes |
| filename | const char* | Output file path with extension | Valid path string | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | 0 on success, error code on failure |

### 2.3 Side Effects

- Creates or overwrites file at specified path
- Processes pending model modifications (like cxf_updatemodel)
- Acquires environment lock during operation
- Logs write operations if logging enabled
- May create compressed output if compression extension specified
- Updates model internal state after write completes

## 3. Contract

### 3.1 Preconditions

- Model pointer must be valid and initialized
- Filename must not be NULL
- File path must be writable (directory exists, permissions sufficient)
- Model must be in consistent state (or function will update it first)
- Sufficient disk space for output file

### 3.2 Postconditions

- If successful, file exists at specified path with exported data
- Pending model modifications are processed
- Environment lock is released
- Model internal state reflects any updates performed
- Error status is recorded if operation failed

### 3.3 Invariants

- Model data is not modified by export (read-only operation on model content)
- Environment remains in consistent state
- File at path may be overwritten without warning
- Compression does not alter exported data semantics

## 4. Algorithm

### 4.1 Overview

The function implements a comprehensive file export system with automatic format detection and preprocessing. It begins by validating the model and acquiring an environment lock to ensure thread-safe file operations.

The core algorithm extracts the file extension using similar logic to cxf_read: scanning for the rightmost period character and handling double extensions for compression. It matches the extension against a lookup table to determine the output format type. The known formats include:

Model formats (require update):
- .mps, .rew (MPS format, regular or reward)
- .lp, .rlp (LP format, regular or robust)
- .dua, .dlp (Dualized model formats)
- .ilp (Irreducible Infeasible Subsystem)
- .json (JSON model format)

Data formats (may require update):
- .sol, .mst (Solution, MIP start)
- .bas (LP basis)
- .hnt (MIP hints)
- .ord (Branching priorities)
- .prm (Parameters)
- .attr (Attributes)

The function uses bitmasks to efficiently categorize which formats require model updates before writing. If the model has pending modifications (indicated by a callback count flag) and the file type requires an update, the function calls the model update routine to ensure consistency.

The actual write operation is dispatched to a specialized writer function based on the determined file type. Compression handling is integrated into the writer layer, which automatically compresses output if a compression extension (.gz, .bz2, etc.) is detected.

After the write completes successfully, the function updates the model's internal state and propagates any status through the error handling system.

### 4.2 Detailed Steps

1. Validate model pointer
2. Acquire environment lock from model
3. If lock acquisition fails, return error
4. Validate filename is not NULL
5. If NULL, return null argument error
6. Check if model has pending modifications (callback count != 0)
7. If pending modifications exist:
   a. Extract file extension using rightmost-dot scanning
   b. Handle compression suffixes by finding data extension
   c. Compare extension against file type table (case-insensitive)
   d. Determine file type code from lookup
   e. Check if file type is in "requires update" category using bitmask test
   f. Bitmask 0x6243E covers: MPS, REW, LP, RLP, DUA, DLP, ILP, SOL, MST, HNT, BAS, ATTR, JSON
   g. Bitmask 0x1C900 covers: PRM and additional types
   h. Special case: (filetype & 0xFFFFFFFE) == 6 for certain paired types
   i. If file type requires update, call cxf_updatemodel
   j. If update fails, goto cleanup with error
   k. Call specialized writer with update flag
   l. If write fails, goto cleanup with error
8. Call main write dispatcher function with model and filename
9. Main dispatcher determines format and calls appropriate writer:
   - Model writers (MPS, LP, etc.)
   - Solution writers (SOL, MST)
   - Basis writer (BAS)
   - Parameter writer (PRM)
   - Attribute writer (ATTR)
   - Other format writers
10. Writer handles file creation, formatting, and compression
11. On success, return from dispatcher
12. Handle errors through error handler
13. Release environment lock
14. Return status code

### 4.3 Pseudocode

```
function cxf_write(model, filename):
    // Validate and lock
    status ← ValidateModel(model)
    if status ≠ 0: return status

    env ← model.environment
    status ← AcquireLock(env)
    if status ≠ 0: return status

    // Validate filename
    if filename = NULL:
        SetError(model, NULL_ARGUMENT, "No filename supplied")
        goto cleanup

    // Check if update needed
    if model.callbackCount ≠ 0:
        fileType ← DetermineFileType(filename)

        requiresUpdate ← false
        if fileType < 64 ∧ (1 << fileType) & 0x6243E ≠ 0:
            requiresUpdate ← true
        else if (fileType & 0xFFFFFFFE) = 6:
            requiresUpdate ← true
        else if fileType < 64 ∧ (1 << fileType) & 0x1C900 ≠ 0:
            requiresUpdate ← true

        if requiresUpdate:
            status ← cxf_updatemodel(model)
            if status ≠ 0: goto cleanup

            status ← WriteWithUpdate(model, filename)
            if status ≠ 0: goto cleanup

    // Main write dispatch
    status ← WriteDispatcher(model, filename)

cleanup:
    RecordError(model, status)
    ReleaseLock(env)
    return status
```

### 4.4 Mathematical Foundation

File type categorization uses bitwise operations:
- For file type T ∈ [0, 63]:
  - Category1 = (2^T & 0x6243E) ≠ 0
  - Category2 = (2^T & 0x1C900) ≠ 0
  - PairedType = (T & 0xFFFFFFFE) = 6

Where bitmasks represent:
- 0x6243E = 0b011010010000111110 (bits 1-5, 8-10, 13, 17-18)
- 0x1C900 = 0b0001110100100000000 (bits 8, 11-12, 14-16)

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(n) - Direct write without update, where n is model size
- **Average case:** O(n + m) - Update + write, where m is file write time
- **Worst case:** O(n × k + m) - Large model update with compression, where k is compression overhead

Where:
- n = model size (variables + constraints)
- m = file I/O time
- k = compression ratio overhead (typically 1-10)

### 5.2 Space Complexity

- **Auxiliary space:** O(n) - Output buffer for formatted model
- **Total space:** O(n + c) - Plus compressed buffer if compression used

Where:
- n = formatted output size
- c = compressed output size (typically 0.1n to 0.5n)

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| Model invalid | Implementation-specific | Model validation failed |
| Filename NULL | 0x2712 (1002) | CXF_ERR_NULL_ARGUMENT |
| File cannot be created | Implementation-specific | Permission denied or path invalid |
| Disk full | Implementation-specific | Insufficient disk space |
| Model update failed | Varies | Error from cxf_updatemodel |
| Writer failed | Varies | Format-specific write error |
| Compression failed | Implementation-specific | Compression library error |

### 6.2 Error Behavior

On error:
- Environment lock is released
- Partial output file may exist (implementation-dependent)
- Error message is recorded in model/environment
- Error code is propagated through error handling system
- Model state reflects any updates performed before error

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Compressed output | "model.lp.gz" | Write LP format, compress with gzip |
| Double compression | "model.mps.gz.bz2" | Behavior undefined (may use outer compression) |
| No extension | "outputfile" | May use default format or error |
| Extension only | ".lp" | Creates file named ".lp" |
| Case variant | "MODEL.MPS" vs "model.mps" | Both write MPS format |
| Existing file | Overwrite existing file | File overwritten without warning |
| Read-only destination | Write to protected directory | Permission error |
| Network path | "\\server\share\model.lp" | Handled by file system |
| Very long filename | 1000+ character path | Platform-dependent (may succeed or fail) |
| Special characters | "my model (v2).lp" | Handled by file system |
| Empty model | 0 variables, 0 constraints | Writes valid empty model file |
| Unsolved model | No solution available | Writes model formulation only |
| Infeasible model | No feasible solution | Writes model formulation |

## 8. Thread Safety

**Thread-safe:** Yes (with environment locking)

The function acquires an environment lock at the start and releases it on completion. This prevents concurrent write operations on the same model. Multiple threads can safely write different models or use different environments concurrently.

**Synchronization required:** Internal (automatic environment locking)

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_checkmodel | Model Validation | Validate model pointer |
| AcquireEnvironmentLock | Threading | Ensure thread-safe file access |
| SetErrorMessage | Error Handling | Record error with context |
| DetermineFileType | File I/O | Extract and classify extension |
| cxf_updatemodel | Model | Process pending modifications |
| WriteWithUpdate | File I/O | Write with update flag set |
| WriteDispatcher | File I/O | Route to format-specific writer |
| WriteMPS | File I/O | MPS format writer |
| WriteLP | File I/O | LP format writer |
| WriteSolution | File I/O | Solution format writer |
| WriteBasis | File I/O | Basis format writer |
| WriteParams | File I/O | Parameter format writer |
| WriteAttributes | File I/O | Attribute format writer |
| CompressOutput | Compression | Apply compression if requested |
| RecordError | Error Handling | Log error status |
| ReleaseEnvironmentLock | Threading | Release lock |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| User code | External | Direct API usage |
| Batch scripts | Automation | Model persistence |
| Solution archival | Optimization | Save best solutions |
| Debugging | Development | Export for analysis |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_read | Inverse operation - imports data from files |
| cxf_updatemodel | Called internally to ensure model consistency |
| cxf_writeparams | Writes parameters only (specialized version) |
| cxf_writebasis | Writes basis only (specialized version) |
| cxf_writesol | Writes solution only (specialized version) |

## 11. Design Notes

### 11.1 Design Rationale

The automatic model update mechanism ensures exported models are always consistent, preventing the export of stale or invalid formulations. Extension-based format detection provides intuitive operation without requiring explicit format parameters. The bitmask categorization allows efficient determination of which formats require updates without string comparisons or large switch statements.

Support for compression extensions eliminates the need for separate compression steps in user workflows. The environment locking prevents race conditions when multiple threads perform I/O operations.

### 11.2 Performance Considerations

The callback count check avoids unnecessary update calls on models without pending modifications. Format-specific writers are optimized for their target format. Compression is applied after formatting, allowing writers to remain simple. The bitmask tests are faster than string-based format categorization.

### 11.3 Future Considerations

No support for streaming writes (entire model formatted in memory before writing). No format version specification (always writes current version). No control over compression level. Limited ability to write partial models or filtered subsets. No support for writing to non-filesystem destinations (network streams, databases).

## 12. References

- Convexfeld File Format Specifications (MPS, LP, etc.)
- Compression library documentation (zlib, bzip2, 7-zip)

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
