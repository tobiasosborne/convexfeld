# Module: Utilities

**Spec Version:** 1.0
**Last Updated:** 2026-01-25
**Functions:** 9

## 1. Overview

### 1.1 Purpose

The Utilities module provides a collection of general-purpose helper functions used throughout Convexfeld for mathematical operations, string formatting, model introspection, and data access. This module abstracts platform-specific behavior, provides safe wrappers for standard library functions, and implements specialized query functions for constraint and model metadata.

Functions in this module fall into four categories: mathematical wrappers (log, floor/ceil), safe string formatting, model property checkers (multi-objective detection), and data accessors (constraint names, quadratic constraint data, general constraint counting).

### 1.2 Responsibilities

This module is responsible for:

- [ ] Providing platform-independent mathematical function wrappers
- [ ] Ensuring safe string formatting with guaranteed null termination
- [ ] Performing simplex cleanup bound propagation
- [ ] Checking model properties (multi-objective status)
- [ ] Mapping constraint type indices to human-readable names
- [ ] Retrieving quadratic constraint data without copying
- [ ] Counting general constraints by type and treatment mode

This module is NOT responsible for:

- [ ] Core optimization algorithms
- [ ] Memory allocation/deallocation policies
- [ ] Error message generation (delegates to error module)
- [ ] Model modification (read-only queries only)
- [ ] Thread synchronization (caller's responsibility)

### 1.3 Design Philosophy

Utilities follow a "trust but verify" approach: wrapper functions handle edge cases gracefully (NaN, infinity, NULL pointers) while core functionality assumes validated inputs for performance. The module emphasizes portability (consistent behavior across platforms), safety (guaranteed null termination, special value handling), and zero-copy access (return pointers to internal data rather than copying).

## 2. Public Interface

### 2.1 Exported Functions

| Function | Purpose | Typical Caller |
|----------|---------|----------------|
| cxf_log10_wrapper | Safe base-10 logarithm with special value handling | Statistics, coefficient analysis |
| cxf_floor_ceil_wrapper | Safe floor rounding with special value handling | Display formatting, numerical operations |
| cxf_snprintf_wrapper | Safe string formatting with guaranteed null termination | Error messages, logging, diagnostics |
| cxf_is_multi_obj | Check if model has multiple objectives | Solver selection, algorithm dispatch |
| cxf_get_genconstr_name | Map constraint type index to name string | Logging, error messages, statistics |
| cxf_get_qconstr_data | Retrieve quadratic constraint data pointers | Solver core, query API |
| cxf_count_genconstr_types | Count general constraints by type | Presolve statistics, logging |
| cxf_cleanup_helper | Iterative bound propagation for simplex cleanup | Simplex solver post-processing |

### 2.2 Exported Types

| Type | Purpose |
|------|---------|
| None | Module uses only primitive types and existing Convexfeld types |

### 2.3 Exported Constants

| Constant | Value | Purpose |
|----------|-------|---------|
| NUM_GENCONSTR_TYPES | 19 | Number of general constraint types (0-18) |
| CXF_INFINITY | 1e100 | Convexfeld's infinity representation |
| CXF_UNDEFINED | 1e101 | Undefined value marker |

## 3. Internal Functions

### 3.1 Private Functions

| Function | Purpose |
|----------|---------|

### 3.2 Helper Functions

| Function | Purpose | Used By |
|----------|---------|---------|
| cxf_log10_wrapper | Compute logarithmic scale | Coefficient range analysis |
| cxf_floor_ceil_wrapper | Round to integer | Display formatting |
| cxf_snprintf_wrapper | Format strings safely | Error/log message generation |
| cxf_is_multi_obj | Detect multi-objective models | Solver dispatch logic |
| cxf_get_genconstr_name | Type index to name | Logging, statistics |

## 4. State Management

### 4.1 Module State

| State Element | Type | Lifetime | Thread Safety |
|--------------|------|----------|---------------|
| genconstr_names | const char*[19] | Static | Safe (read-only) |

All other functions are stateless.

### 4.2 State Lifecycle

```
STATIC_INITIALIZATION (program load)
    ↓
genconstr_names array initialized with type names
    ↓
READY (available for entire program lifetime)
```

No state transitions - module is always ready.

### 4.3 State Invariants

At all times, the following must be true:

- [ ] genconstr_names array contains 19 valid null-terminated strings
- [ ] All function return values are well-defined (never uninitialized)
- [ ] Special floating-point values (NaN, Inf) are handled consistently

## 5. Dependencies

### 5.1 Required Modules

| Module | What We Use | Why |
|--------|-------------|-----|
| C Standard Library | isnan, isinf, log10, floor, vsnprintf | Mathematical operations, string formatting |
| Memory | cxf_malloc, cxf_free | Temporary allocations in cleanup helper |

### 5.2 Initialization Order

This module must be initialized:
- **After:** C runtime, memory allocator
- **Before:** Any module using utilities (most modules)

### 5.3 External Dependencies

| Dependency | Type | Purpose |
|------------|------|---------|
| math.h (C99) | System library | Mathematical functions |
| stdio.h (C99) | System library | String formatting |
| stdarg.h (C99) | System library | Variable arguments |

## 6. Dependents

### 6.1 Modules That Depend on Us

| Module | What They Use | Stability |
|--------|---------------|-----------|
| Statistics | cxf_log10_wrapper, cxf_count_genconstr_types | Stable |
| Error/Logging | cxf_snprintf_wrapper | Stable |
| Simplex | cxf_cleanup_helper | Stable |
| Optimization | cxf_is_multi_obj | Stable |
| API | cxf_get_qconstr_data | Stable |

### 6.2 API Stability

The following interfaces are stable and must not change:

- [ ] Function signatures (public API contracts)
- [ ] Special value semantics (NaN, Inf handling)
- [ ] Genconstr name strings (logging compatibility)
- [ ] Null termination guarantees (safety contract)

## 7. Invariants

### 7.1 Module Invariants

What this module guarantees:

- [ ] Mathematical wrappers never crash (handle all special values)
- [ ] String formatting always null-terminates output buffers
- [ ] Returned pointers (genconstr names, qconstr data) are valid
- [ ] Type indices are validated before array access
- [ ] Functions are deterministic (same input → same output)

### 7.2 Required Invariants

What this module requires from others:

- [ ] Buffer sizes provided to snprintf wrapper are accurate
- [ ] Model pointers passed to query functions are valid
- [ ] Constraint indices are within valid ranges
- [ ] Array pointers for count functions are allocated with size 19+

## 8. Error Handling

### 8.1 Error Detection

How errors are detected in this module:

| Error Type | Detection Method |
|------------|------------------|
| Special float values | isnan(), isinf() checks |
| NULL pointers | Defensive NULL checks |
| Out-of-range indices | Bounds validation |
| Invalid models | Magic number validation |

### 8.2 Error Propagation

How errors flow through this module:

```
Mathematical wrappers:
  Invalid input → Special value return (NaN, Inf)

Query functions:
  Invalid input → Error code return (1002, 1006)

Utility functions:
  Invalid input → Safe default return (0, empty string)
```

### 8.3 Recovery Strategies

| Error Type | Recovery |
|------------|----------|
| NaN/Inf input | Return appropriate special value, allow caller to continue |
| NULL pointer | Return safe default (0, empty string, error code) |
| Invalid index | Return "UNKNOWN" string or error code |
| Allocation failure | Return OUT_OF_MEMORY error code |

## 9. Thread Safety

### 9.1 Concurrency Model

Mostly read-only and stateless: Mathematical and string formatting functions are fully thread-safe (no shared state). Query functions are conditionally thread-safe (safe for concurrent reads, unsafe during model modification).

### 9.2 Synchronization Primitives

| Primitive | Protects | Granularity |
|-----------|----------|-------------|
| None | N/A | Stateless or read-only |

### 9.3 Thread Safety Guarantees

- [ ] Mathematical wrappers are unconditionally thread-safe (pure functions)
- [ ] String formatting is thread-safe per invocation (stack-local va_list)
- [ ] Model query functions are thread-safe for concurrent reads
- [ ] Static genconstr_names array is thread-safe (read-only)
- [ ] Cleanup helper is not thread-safe (modifies model data)

### 9.4 Known Race Conditions

- Concurrent calls to cxf_count_genconstr_types with same output arrays create race conditions (non-atomic increments)
- Concurrent model modification while calling query functions is unsafe

## 10. Performance Characteristics

### 10.1 Complexity Summary

| Operation | Time | Space |
|-----------|------|-------|
| log10_wrapper | O(1) | O(1) |
| floor_ceil_wrapper | O(1) | O(1) |
| snprintf_wrapper | O(n) | O(1) |
| is_multi_obj | O(1) | O(1) |
| get_genconstr_name | O(1) | O(1) |
| get_qconstr_data | O(1) | O(1) |
| count_genconstr_types | O(k) | O(1) |
| cleanup_helper | O(n × d × p) | O(n) |

Where:
- n = string length or number of variables
- k = number of general constraints
- d = average variable degree
- p = number of propagation passes (typically 2-3, max 10)

### 10.2 Hot Paths

Performance-critical functions:

- **cxf_log10_wrapper:** ~30-40 CPU cycles (called frequently for coefficient analysis)
- **cxf_floor_ceil_wrapper:** ~15-20 CPU cycles (called for display formatting)
- **cxf_is_multi_obj:** ~5 CPU cycles (simple pointer checks)

Not performance-critical:
- String formatting (I/O bound, logging only)
- Constraint counting (called once during presolve)
- Cleanup helper (called once after simplex completion)

### 10.3 Memory Usage

Minimal memory footprint:
- Static genconstr_names: ~200 bytes (19 string pointers + string data)
- Temporary allocations: Cleanup helper allocates 2×n temporary arrays (freed before return)
- No persistent heap growth

## 11. Function Index

Complete list of functions in this module with links to individual specs:

### Public Utility Functions

1. [cxf_log10_wrapper](../functions/utilities/cxf_log10_wrapper.md) - Safe base-10 logarithm
2. [cxf_floor_ceil_wrapper](../functions/utilities/cxf_floor_ceil_wrapper.md) - Safe floor rounding
3. [cxf_snprintf_wrapper](../functions/utilities/cxf_snprintf_wrapper.md) - Safe string formatting
4. [cxf_is_multi_obj](../functions/utilities/cxf_is_multi_obj.md) - Multi-objective detection
5. [cxf_get_genconstr_name](../functions/utilities/cxf_get_genconstr_name.md) - Constraint type name lookup
6. [cxf_get_qconstr_data](../functions/utilities/cxf_get_qconstr_data.md) - Quadratic constraint data access
7. [cxf_count_genconstr_types](../functions/utilities/cxf_count_genconstr_types.md) - Count constraints by type
8. [cxf_cleanup_helper](../functions/utilities/cxf_cleanup_helper.md) - Simplex bound propagation


9. [cxf_misc_utility](../functions/utilities/cxf_misc_utility.md) - Miscellaneous utility (placeholder)

## 12. Design Decisions

### 12.1 Key Design Choices

| Decision | Rationale | Alternatives Considered |
|----------|-----------|------------------------|
| Explicit special value handling | Ensures portable behavior across platforms | Rely on C99 standard (rejected: pre-C99 varied) |
| Defensive null termination | Prevents buffer overflow vulnerabilities | Trust vsnprintf (rejected: MSVC pre-C99 unsafe) |
| Zero-copy data access | Avoids allocation overhead | Copy data to user buffers (rejected: too slow) |
| Static genconstr names | Fastest lookup, no allocation | Dynamic string table (rejected: overkill) |
| Worklist-based propagation | Avoids redundant work | Naive O(n²) iteration (rejected: too slow) |

### 12.2 Known Limitations

- [ ] Cleanup helper has hardcoded 10-pass limit (may not converge)
- [ ] Caller must zero-initialize count arrays (error-prone API)
- [ ] No validation of array sizes (caller's responsibility)
- [ ] String formatting truncates without warning

### 12.3 Future Improvements

- [ ] Add ceil/round/trunc wrappers for completeness
- [ ] Provide safe snprintf variant with overflow detection
- [ ] Support adaptive pass limit in cleanup helper
- [ ] Add array size validation to count functions

## 13. References

- ISO C99 Standard: Sections 7.12 (math), 7.19 (stdio), 7.15 (stdarg)
- IEEE 754 Floating-Point Arithmetic Standard
- CERT C Coding Standard: STR31-C (Guarantee null-termination)
- Dantzig, G. (1963). Linear Programming and Extensions (bound propagation)
- Kahan summation algorithm for numerical stability

## 14. Validation Checklist

Before finalizing this spec:

- [x] All public functions documented
- [x] All dependencies identified
- [x] Thread safety analyzed
- [x] Error handling complete
- [x] No implementation details leaked
- [x] Module boundaries clear

---

*Reviewed by: Pending*

---
*Convexfeld LP Solver Specification*
*Based on published optimization literature*
