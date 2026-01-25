# cxf_version

**Module:** API Utilities
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Retrieves the version number of the Convexfeld Optimizer library as three integer components: major version, minor version, and technical/patch version. This allows client code to verify library version at runtime for compatibility checking, feature availability determination, or diagnostic logging. The function is designed to be callable before environment initialization.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| majorP | int* | Pointer to store major version | Valid pointer or NULL | No |
| minorP | int* | Pointer to store minor version | Valid pointer or NULL | No |
| technicalP | int* | Pointer to store technical version | Valid pointer or NULL | No |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | void | No return value |
| *majorP | int | Major version number (e.g., 12) if pointer non-NULL |
| *minorP | int | Minor version number (e.g., 0) if pointer non-NULL |
| *technicalP | int | Technical/patch version (e.g., 3) if pointer non-NULL |

### 2.3 Side Effects

None. This is a pure function with no observable side effects beyond writing to the provided output pointers.

## 3. Contract

### 3.1 Preconditions

- None. All parameters are optional (can be NULL).
- No Convexfeld initialization required.
- Function can be called at any time.

### 3.2 Postconditions

- If majorP is non-NULL, *majorP contains the major version number
- If minorP is non-NULL, *minorP contains the minor version number
- If technicalP is non-NULL, *technicalP contains the technical version number
- If all pointers are NULL, function returns without error (no-op)

### 3.3 Invariants

- Version numbers are compile-time constants (never change during execution)
- Function behavior is deterministic and idempotent
- No global state is modified

## 4. Algorithm

### 4.1 Overview

The function implements a trivial version reporting mechanism. It checks each output pointer for NULL and, if non-NULL, writes the corresponding hardcoded version number to the pointed-to location. The version numbers are baked into the binary at compile time and reflect the Convexfeld library build version.

- Major version = 12
- Minor version = 0
- Technical version = 3

The function performs no computation, makes no function calls, and accesses no global state. It is a leaf function in the call graph.

### 4.2 Detailed Steps

1. Check if majorP pointer is NULL
2. If majorP is not NULL, write major version constant to *majorP
3. Check if minorP pointer is NULL
4. If minorP is not NULL, write minor version constant to *minorP
5. Check if technicalP pointer is NULL
6. If technicalP is not NULL, write technical version constant to *technicalP
7. Return

### 4.3 Pseudocode

```
function cxf_version(majorP, minorP, technicalP):
    if majorP ≠ NULL:
        *majorP ← MAJOR_VERSION  // 12 for analyzed binary

    if minorP ≠ NULL:
        *minorP ← MINOR_VERSION  // 0 for analyzed binary

    if technicalP ≠ NULL:
        *technicalP ← TECHNICAL_VERSION  // 3 for analyzed binary
```

### 4.4 Mathematical Foundation

Version number encoding follows semantic versioning principles:
- Version string: "MAJOR.MINOR.TECHNICAL"
- For version 12.0.3: (12, 0, 3)
- Version comparison: V1 > V2 if lexicographic comparison of (major, minor, technical) tuples

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) - Constant time, at most 3 pointer writes
- **Average case:** O(1) - Same as best case
- **Worst case:** O(1) - Same as best case

### 5.2 Space Complexity

- **Auxiliary space:** O(1) - No stack frames, no allocations
- **Total space:** O(1) - Constants only

## 6. Error Handling

### 6.1 Error Conditions

None. The function cannot fail.

### 6.2 Error Behavior

Not applicable. The function has void return type and cannot report errors.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| All pointers NULL | majorP=NULL, minorP=NULL, technicalP=NULL | Returns immediately (no-op) |
| Only major requested | majorP=valid, others=NULL | Sets *majorP, ignores others |
| Only minor requested | minorP=valid, others=NULL | Sets *minorP, ignores others |
| Only technical requested | technicalP=valid, others=NULL | Sets *technicalP, ignores others |
| All pointers valid | All non-NULL | Sets all three version components |
| Uninitialized library | Called before cxf_loadenv | Succeeds (no initialization needed) |
| After library shutdown | Called after cxf_freeenv | Succeeds (stateless function) |
| Concurrent calls | Multiple threads call simultaneously | Thread-safe (read-only constants) |

## 8. Thread Safety

**Thread-safe:** Yes (unconditionally)

The function reads only compile-time constants and writes only to caller-provided memory locations. No shared state is accessed or modified. Multiple threads can call this function concurrently without any synchronization.

**Synchronization required:** None

## 9. Dependencies

### 9.1 Functions Called

None. This is a leaf function.

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| User code | External | Version checking before initialization |
| Diagnostic tools | Logging | Version reporting in logs |
| Compatibility checks | Application | Feature availability determination |
| Compatibility | Version Manager | Version-based compatibility checking |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_platform | Returns platform string (e.g., "win64", "linux64") |
| cxf_versionstring | May exist - returns version as formatted string |

## 11. Design Notes

### 11.1 Design Rationale

The three-component version scheme follows semantic versioning, where:
- Major version changes indicate API-breaking changes
- Minor version changes add features while maintaining compatibility
- Technical version changes are bug fixes and minor improvements

The NULL-pointer tolerance allows callers to request only the version components they need without providing dummy storage for unwanted components. The void return type reflects that the operation cannot fail.

The stateless design allows the function to be called before library initialization, which is essential for version-based compatibility checks in bootstrapping code.

### 11.2 Performance Considerations

The function is extremely fast (typically 1-5 CPU cycles per pointer write). No branches beyond NULL checks. No memory barriers needed. Can be inlined by optimizing compilers. Suitable for use in performance-critical code or tight loops (though this would be unusual).

### 11.3 Future Considerations

No mechanism for extended version information (build date, commit hash, etc.). No support for pre-release version identifiers (alpha, beta, rc). The three-component scheme may be insufficient for complex versioning needs. No way to query version of specific components (LP solver vs MIP solver versions).

## 12. References

- Semantic Versioning 2.0.0 (semver.org)
- Convexfeld Release Notes (version history and compatibility)

## 13. Validation Checklist

- [x] No code copied from implementation
- [x] Algorithm description is implementation-agnostic
- [x] All parameters documented
- [x] All error conditions listed (none in this case)
- [x] Complexity analysis complete
- [x] Edge cases identified
- [x] A competent developer could implement from this spec alone

---

*Reviewed by: N/A*

---
*Convexfeld LP Solver Specification*
*Based on published optimization literature*
