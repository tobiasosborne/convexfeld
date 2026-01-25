# cxf_read

**Module:** API I/O
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Imports auxiliary data into an existing optimization model from external files. The file extension determines the data type: MIP start solutions, hints, LP basis vectors, variable branching priorities, parameter settings, or attribute values. Unlike cxf_readmodel which creates a new model from a problem file, this function adds supplementary information to an already-constructed model to guide or configure the optimization process.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| model | CxfModel* | Model to receive imported data | Valid model pointer | Yes |
| filename | const char* | Path to data file | Valid file path string | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | 0 on success, error code on failure |

### 2.3 Side Effects

- Sets reading flag in environment during operation
- Loads data into model (MIP start, hints, basis, priorities, parameters, attributes)
- Updates model internal state after successful import
- Logs success message to console if logging enabled
- May decompress file to temporary location if compressed format detected

## 3. Contract

### 3.1 Preconditions

- Model pointer must be valid and initialized
- Filename must not be NULL
- File must exist and be readable
- File extension must correspond to a supported data type
- Model must be in a state compatible with the data being imported

### 3.2 Postconditions

- If successful, model contains imported data
- Reading flag is cleared in environment
- Model internal state is updated
- Success is logged if logging enabled
- Temporary decompressed files are cleaned up

### 3.3 Invariants

- Model structure integrity is maintained
- Environment state remains consistent
- File is not modified
- Original model data (constraints, variables, objective) is not modified

## 4. Algorithm

### 4.1 Overview

The function implements a file-type-dispatched data import system. It begins by validating the model and acquiring an environment lock to ensure thread-safe file operations. A reading flag is set in the environment to prevent concurrent read operations.

The core algorithm extracts the file extension by scanning the filename for the last period character. It handles double extensions for compressed files (e.g., ".bas.gz") by detecting compression suffixes (.zip, .gz, .bz2, .7z, .xz) and extracting the true data extension. Extension matching is performed case-insensitively to accept variations like ".MST" or ".mst".

The extension is matched against a lookup table mapping extensions to data type codes:
- .mst, .sol → MIP start/solution
- .bas → LP basis
- .hnt → MIP hints
- .ord → Branching priority order
- .prm → Parameter settings
- .attr → Attribute values

For compressed files, the function decompresses to a temporary location. It then attempts to open the file, trying multiple compression suffix combinations if needed. Based on the determined file type, control is dispatched to specialized reader functions that parse and load the specific data format.

After successful import, the function logs the operation, propagates the success to distributed systems if applicable, and updates the model's internal state to reflect the newly imported data.

### 4.2 Detailed Steps

1. Validate model pointer
2. Get environment from model structure
3. Acquire environment lock for thread safety
4. Set reading flag in environment to prevent concurrent reads
5. Validate filename is not NULL (error if NULL)
6. Scan filename from left to right, recording position of each '.' character
7. Determine last extension by finding rightmost '.'
8. Calculate extension length
9. Check if extension matches known compression formats (.zip, .gz, .bz2, .7z, .xz)
10. If compression format, scan backwards to find previous '.' for actual data extension
11. Compare extension against known data types using case-insensitive matching
12. Build priority-ordered list of extensions found (compression + data)
13. Loop through known file type table (up to 18 entries)
14. For each entry, compare suffix length and characters (case-insensitive)
15. On match, record file type code and break loop
16. If compressed file detected, call decompression function
17. Build file path with potential compression suffix combinations
18. Attempt to open file with each combination (up to 6 attempts)
19. On successful open, close file and proceed with that path
20. If no combination opens successfully, return file open error
21. Dispatch to appropriate reader based on file type code:
    - Type 6/7: cxf_readMIPStart
    - Type 8: cxf_readBasis
    - Type 9: cxf_readParams
    - Type 11: cxf_readBranchingOrder
    - Type 14: cxf_readHints
    - Type 15: cxf_readAttributes
    - Unknown: Return import error
22. If reader returns success, log operation with data type and filename
23. Update model internal state
24. Free decompressed temporary file if created
25. Clear reading flag in environment
26. Release environment lock
27. Return status code

### 4.3 Pseudocode

```
function cxf_read(model, filename):
    status ← ValidateModel(model)
    if status ≠ 0: return status

    env ← model.environment
    AcquireLock(env)
    env.readingFlag ← 1

    if filename = NULL:
        return Error(NULL_ARGUMENT, "No filename supplied")

    // Extract extension
    lastDot ← FindLastOccurrence(filename, '.')
    if lastDot = NULL:
        extension ← EndOfString(filename)
    else:
        extension ← lastDot

    // Handle compression
    compressionType ← DetectCompression(extension)
    if compressionType ≠ NONE:
        extension ← FindPreviousDot(filename, lastDot)

    // Match file type
    fileType ← UNKNOWN
    for each entry in FileTypeTable:
        if CaseInsensitiveMatch(extension, entry.extension):
            fileType ← entry.typeCode
            break

    // Handle decompression
    tempPath ← NULL
    if NeedsDecompression(filename):
        tempPath ← DecompressFile(env, filename)
        if tempPath = NULL:
            return Error(FILE_OPEN, "Unable to open compressed file")

    // Try to open file
    actualPath ← tempPath ? tempPath : filename
    file ← TryOpenWithSuffixes(actualPath)
    if file = NULL:
        return Error(FILE_OPEN, "Unable to open file for input")

    Close(file)

    // Dispatch to reader
    status ← DispatchReader(model, fileType, actualPath)

    if status = SUCCESS:
        LogImport(env, fileType, filename)
        UpdateModelInternal(model)

    if tempPath ≠ NULL:
        DeleteTemporaryFile(tempPath)

    env.readingFlag ← 0
    ReleaseLock(env)
    return status
```

### 4.4 Mathematical Foundation

File type determination uses suffix matching:
- Extension E extracted from filename F
- For compression suffix C ∈ {.zip, .gz, .bz2, .7z, .xz}:
  - If Suffix(F) = C, then E = Suffix(Prefix(F, |F| - |C|))
- File type T = Lookup(E, TypeTable)
- If T ∉ TypeTable, then T = UNKNOWN

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(m) - Uncompressed file, direct type match, where m is filename length
- **Average case:** O(m + n) - Extension detection + file open, where n is file size for header check
- **Worst case:** O(m × k + n) - Multiple suffix attempts, where k is compression suffix count

Where:
- m = length of filename string
- n = file size (for decompression overhead)
- k = number of compression suffix attempts (typically 6)

### 5.2 Space Complexity

- **Auxiliary space:** O(n) - Temporary decompressed file for compressed inputs
- **Total space:** O(n + s) - Plus data structure size s for imported data

Where:
- n = decompressed file size
- s = size of imported data structures (basis, solution vectors, etc.)

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| Model invalid | Implementation-specific | Model validation failed |
| Filename is NULL | 0x2712 (1002) | CXF_ERR_NULL_ARGUMENT |
| File cannot be opened | 0x2713 (1003) | Unable to open file for input |
| Unknown file type | 0x271C (1012) | Unknown file extension |
| Cannot import type | 0x271C (1012) | File type cannot be imported to model |
| Decompression failed | 0x2713 (1003) | Unable to decompress file |
| Reader function error | Varies | Specific to reader implementation |

### 6.2 Error Behavior

On error:
- Reading flag is cleared in environment
- Environment lock is released
- Temporary files are cleaned up
- Error message is recorded in environment
- Error code is propagated through error handling system
- Model state is unchanged (no partial imports)

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Compressed file | "basis.bas.gz" | Decompress, detect .bas extension |
| Double compression | "data.mst.gz.bz2" | May fail or handle outer compression only |
| No extension | "myfile" | Unknown file type error |
| Extension only | ".sol" | Attempts to open as solution file |
| Case variant | "START.MST" vs "start.mst" | Both accepted (case-insensitive) |
| Multiple dots | "model.v2.final.bas" | Uses rightmost extension (.bas) |
| Empty file | 0-byte file | Reader may fail or return empty data |
| Large compressed file | Multi-GB archive | May exhaust memory during decompression |
| Read-only file | File without write permission | Succeeds (read-only access) |
| Network path | "\\server\share\file.mst" | Handled by file system |
| Relative path | "../data/solution.sol" | Resolved relative to working directory |

## 8. Thread Safety

**Thread-safe:** Yes (with environment locking)

The function acquires an environment lock at the start and releases it on completion. The reading flag prevents concurrent read operations on the same environment. However, multiple threads can safely read different models or use different environments concurrently.

**Synchronization required:** Internal (automatic environment locking)

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_checkmodel | Model Validation | Validate model pointer |
| AcquireEnvironmentLock | Threading | Thread-safe file access |
| SetErrorMessage | Error Handling | Record error with context |
| DecompressFile | Compression | Handle compressed inputs |
| TryOpenFile | File I/O | Attempt file access |
| cxf_readMIPStart | MIP | Load MIP start solution |
| cxf_readBasis | LP | Load basis vectors |
| cxf_readParams | Parameters | Load parameter settings |
| cxf_readBranchingOrder | MIP | Load variable priorities |
| cxf_readHints | MIP | Load solution hints |
| cxf_readAttributes | Attributes | Load attribute values |
| LogMessage | Logging | Record success message |
| UpdateModelInternal | Model | Refresh model state |
| ReleaseEnvironmentLock | Threading | Release lock |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| User code | External | Direct API usage |
| Batch scripts | Automation | Loading solution data |
| Warm start systems | Optimization | Provide initial solutions |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_readmodel | Reads complete model (not supplementary data) |
| cxf_write | Exports data to files (inverse operation) |
| cxf_loadmodel | Programmatic model creation (alternative to file reading) |
| cxf_readmodeldata | Lower-level model reading with more options |

## 11. Design Notes

### 11.1 Design Rationale

The extension-based dispatch system provides intuitive file type detection without requiring format identification headers. Support for compressed files reduces storage requirements and network transfer time. The reading flag prevents race conditions when multiple operations attempt concurrent file access on the same environment.

The separation of reader functions by data type allows specialized parsing logic while maintaining a unified entry point. The temporary decompression approach isolates compression handling from the data parsing logic.

### 11.2 Performance Considerations

Compressed file handling incurs decompression overhead but reduces I/O time for large files. Multiple file open attempts with suffix combinations add latency but improve user convenience. The reading flag is a simple mutex alternative to full environment locking.

### 11.3 Future Considerations

No support for streaming decompression (entire file decompressed before reading). No format version detection (assumes current version). No validation that imported data is compatible with model structure. Limited error recovery (no partial import on reader failure).

## 12. References

- Convexfeld File Format Documentation
- Compression library specifications (zlib, bzip2, 7-zip, xz)

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
