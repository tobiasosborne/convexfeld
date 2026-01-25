# Module: Validation

**Spec Version:** 1.0
**Last Updated:** 2026-01-25
**Functions:** 10

## 1. Overview

### 1.1 Purpose

The Validation module provides comprehensive input validation and model feature detection for the Convexfeld optimizer. It serves as the first line of defense against invalid inputs, ensuring data integrity before expensive solver operations begin. The module detects invalid numeric values (NaN, inappropriate infinities), validates model structure and types, and identifies problem characteristics to enable optimal algorithm selection.

This module implements fast guard conditions called at API entry points and strategic validation checkpoints throughout the solver. By catching errors early, the module prevents undefined behavior, improves error messages, and enables the solver to select appropriate algorithms based on problem features (LP vs MIP vs QP vs SOCP).

### 1.2 Responsibilities

This module is responsible for:

- Validating environment pointers and initialization status at API entry points
- Detecting invalid floating-point values (NaN) in user inputs
- Validating numeric arrays for optimization (reject NaN, allow infinity where appropriate)
- Validating and clamping variable types and bounds
- Detecting MIP features (integer variables, SOS constraints, general constraints)
- Detecting quadratic and conic features (QP, QCP, SOCP)
- Classifying problem types for algorithm selection
- Providing fast O(1) feature detection via pre-computed flags and counters
- Enabling early error detection with informative messages

This module is NOT responsible for:

- Fixing invalid data (only detecting and reporting)
- Model structure optimization or transformation
- Detailed constraint validation (handled by specific modules)
- Performance optimization of valid models

### 1.3 Design Philosophy

The module follows key design principles:

1. **Fail-fast validation**: Detect errors at the earliest possible point (API entry) rather than deep in solver algorithms. This improves error messages and prevents wasted computation.

2. **Performance-critical guards**: Validation functions are called extremely frequently and must be optimized for the common case (valid input). Most validation completes in 1-5 nanoseconds.

3. **Defensive NULL handling**: All functions gracefully handle NULL pointers without crashing, returning conservative defaults or error codes. This simplifies calling code and prevents crashes.

4. **IEEE 754 portability**: Numeric validation uses bit manipulation rather than floating-point comparison to ensure consistent behavior across compilers and optimization levels.

5. **Lazy evaluation**: Model feature detection uses cached flags and counters when available, falling back to full scans only when necessary. This amortizes validation cost.

6. **Conservative defaults**: When validation status is ambiguous (NULL model, missing data), functions return conservative answers that prevent incorrect optimizations.

## 2. Public Interface

### 2.1 Exported Functions

| Function | Purpose | Typical Caller |
|----------|---------|----------------|
| cxf_checkenv | Validate environment pointer and initialization | All API entry points |
| cxf_check_nan | Detect NaN in single value | Array validation, coefficient checking |
| cxf_check_nan_or_inf | Detect NaN or Infinity | Objective validation, constraint checking |
| cxf_validate_array | Validate numeric array (reject NaN) | cxf_addvar, cxf_chgcoeffs, model construction |
| cxf_validate_vartypes | Validate variable types, clamp binary bounds | cxf_newmodel, cxf_addvar |
| cxf_check_model_flags1 | Detect MIP features | Algorithm selection, presolve |
| cxf_check_model_flags2 | Detect quadratic/conic features | Algorithm selection (barrier vs simplex) |
| cxf_is_mip_model | Check for integer variables | Solver routing |
| cxf_is_quadratic | Check for quadratic objective | Algorithm selection |
| cxf_is_socp | Check for SOCP/QCP constraints | Algorithm selection |

### 2.2 Exported Types

No types are exported by this module - it operates on standard types (int, double, pointers to CxfEnv and CxfModel structures).

### 2.3 Exported Constants

| Constant | Value | Purpose |
|----------|-------|---------|
| CXF_OK | 0 | Validation success |
| CXF_ERR_NULL_ARGUMENT | 1002 | NULL pointer provided |
| CXF_ERR_INVALID_ARGUMENT | 1003 | Invalid value detected |
| CXF_ERR_NOT_IN_MODEL | 1004 | Environment not initialized |
| NaN detection (bit patterns) | Various | IEEE 754 special value patterns |

## 3. Internal Functions

### 3.1 Private Functions

| Function | Purpose |
|----------|---------|
| (None in this module) | All validation functions are public |

All functions are exposed as module exports to maximize reusability across the codebase.

## 4. State Management

### 4.1 Module State

| State Element | Type | Lifetime | Thread Safety |
|--------------|------|----------|---------------|
| Model integer variable count | int | Model lifetime | Updated on model changes |
| Model quadratic flag | bool | Model lifetime | Updated on model changes |
| Model SOCP flag | bool | Model lifetime | Updated on model changes |

### 4.2 State Lifecycle

```
Environment Validation State:
  UNINITIALIZED (active=0)
    → fails cxf_checkenv
  INITIALIZED (active=1)
    → passes cxf_checkenv
  DESTROYED (active=0)
    → fails cxf_checkenv

Model Feature State:
  EMPTY
    → all flags false, all counts zero
  POPULATED (variables/constraints added)
    → flags and counters updated incrementally
  MODIFIED (coefficients changed)
    → flags may need recomputation
  OPTIMIZED
    → flags stable (read-only during solve)
```

### 4.3 State Invariants

At all times, the following must be true:

- Environment active flag accurately reflects initialization status
- Model feature flags are consistent with actual model structure
- Integer variable count equals number of integer/binary variables
- Quadratic flag is true if and only if Q matrix is non-empty
- SOCP flag is true if and only if quadratic constraints exist
- Numeric validation never modifies input data
- Validation functions are idempotent (multiple calls produce same result)

## 5. Dependencies

### 5.1 Required Modules

| Module | What We Use | Why |
|--------|-------------|-----|
| (None) | Direct memory access only | Validation is leaf module |
| IEEE 754 standard | Bit patterns for special values | Numeric validation |
| System (stdint.h) | Fixed-width integer types | Bit manipulation |

### 5.2 Initialization Order

This module must be initialized:
- **After:** (None - leaf module)
- **Before:** All other modules (they use validation)

### 5.3 External Dependencies

| Dependency | Type | Purpose |
|------------|------|---------|
| IEEE 754 | Standard | Floating-point representation |
| stdint.h | C Standard Library | uint64_t for bit manipulation |

## 6. Dependents

### 6.1 Modules That Depend on Us

| Module | What They Use | Stability |
|--------|---------------|-----------|
| All API functions | cxf_checkenv | Must not change |
| Model construction | cxf_validate_array, cxf_validate_vartypes | Must not change |
| Solver routing | cxf_is_mip_model, cxf_is_quadratic, cxf_is_socp | Must not change |
| Presolve | cxf_check_model_flags1, cxf_check_model_flags2 | Must not change |
| Matrix operations | cxf_check_nan, cxf_validate_array | Must not change |

### 6.2 API Stability

The following interfaces are stable and must not change:

- cxf_checkenv signature and return codes
- Numeric validation function signatures
- Model feature detection return values (boolean semantics)
- Error codes for validation failures

## 7. Invariants

### 7.1 Module Invariants

What this module guarantees:

- Validation functions never modify input data
- NULL inputs are handled safely without crashes
- Numeric validation is consistent across platforms (bit manipulation)
- Fast path (O(1)) for common feature detection queries
- Error codes accurately reflect validation failure reasons
- No false positives in feature detection (conservative)
- Thread-safe for concurrent reads

### 7.2 Required Invariants

What this module requires from others:

- Environment structure layout must be stable
- Model structure offsets must not change
- Feature flags must be maintained by model construction
- Numeric arrays must be contiguous in memory
- Variable type codes must use standard values ('C', 'B', 'I', 'S', 'N')

## 8. Error Handling

### 8.1 Error Detection

How errors are detected in this module:

| Error Type | Detection Method |
|------------|------------------|
| NULL pointer | Direct pointer comparison |
| NaN value | IEEE 754 bit pattern (exponent=0x7FF, mantissa≠0) |
| Infinity where invalid | IEEE 754 bit pattern (exponent=0x7FF, mantissa=0) |
| Invalid variable type | Compare against valid set ('C', 'B', 'I', 'S', 'N') |
| Binary bounds violation | Check lb > ub or bounds outside [0,1] |

### 8.2 Error Propagation

How errors flow through this module:

```
Environment Validation:
  NULL env → return CXF_ERR_NULL_ARGUMENT
  active=0 → return CXF_ERR_NOT_IN_MODEL
  active=1 → return CXF_OK

Numeric Validation:
  NaN detected → return 1 or error code
  Valid value → return 0 or CXF_OK
  Infinity → depends on context (allowed for bounds, rejected for coefficients)

Array Validation:
  NULL array → return error
  NaN in array → return error with index
  All valid → return CXF_OK

Feature Detection:
  NULL model → return 0 (assume LP/continuous)
  Valid model → return accurate boolean
```

### 8.3 Recovery Strategies

| Error Type | Recovery |
|------------|----------|
| Invalid environment | Return error, caller must not proceed |
| NaN in data | Return error, caller must reject input |
| Invalid variable type | Return error with message, caller fixes type |
| Binary bounds violation | Clamp to [0,1], validate lb≤ub, proceed or error |
| Feature detection on NULL | Return conservative default (assume LP) |

## 9. Thread Safety

### 9.1 Concurrency Model

**Model:** Read-only validation, fully concurrent.

All validation functions perform read-only operations on input data and model structures. Multiple threads can validate concurrently without synchronization. The only exception is cxf_validate_vartypes which modifies binary bounds (must have exclusive access to model).

### 9.2 Synchronization Primitives

| Primitive | Protects | Granularity |
|-----------|----------|-------------|
| (None) | All validation functions | No locking required |

Exception: cxf_validate_vartypes requires external synchronization by caller.

### 9.3 Thread Safety Guarantees

- All validation functions safe for concurrent reads
- Multiple threads can validate different data in parallel
- Multiple threads can validate same data in parallel
- No shared state modified (except cxf_validate_vartypes)
- Numeric validation uses thread-local stack variables
- Feature detection reads model flags without locking

### 9.4 Known Race Conditions

No race conditions within validation operations. Potential issues:
- Concurrent modification during validation (caller must synchronize)
- cxf_validate_vartypes concurrent with other model access (not thread-safe)
- Reading feature flags during model modification (caller must synchronize)

## 10. Performance Characteristics

### 10.1 Complexity Summary

| Operation | Time | Space |
|-----------|------|-------|
| cxf_checkenv | O(1) | O(1) |
| cxf_check_nan | O(1) | O(1) |
| cxf_check_nan_or_inf | O(1) | O(1) |
| cxf_validate_array | O(n) | O(1) |
| cxf_validate_vartypes | O(n) | O(1) |
| cxf_check_model_flags1 | O(1) avg, O(n) worst | O(1) |
| cxf_check_model_flags2 | O(1) | O(1) |
| cxf_is_mip_model | O(1) avg, O(n) worst | O(1) |
| cxf_is_quadratic | O(1) | O(1) |
| cxf_is_socp | O(1) | O(1) |

Where n = number of elements to validate.

### 10.2 Hot Paths

**Critical performance paths:**
- cxf_checkenv: Called at entry of every API function (millions of times)
- cxf_check_nan: Called in tight validation loops (hundreds of thousands of times)
- Feature detection: Called during algorithm selection (thousands of times)

**Optimizations:**
- cxf_checkenv: 2-3 instructions (~1 nanosecond)
- Numeric checks: Bit manipulation (1-2 nanoseconds)
- Feature detection: Counter checks with early exit (~5-30 nanoseconds)
- Array validation: SIMD potential for parallel checking (~5 ns per element)

### 10.3 Memory Usage

Minimal memory usage:
- No heap allocation in any validation function
- Stack usage: typically 16-64 bytes per function call
- No caching or memoization (rely on model flags)

## 11. Function Index

Complete list of functions in this module with links to individual specs:

### Environment Validation

1. [cxf_checkenv](functions/validation/cxf_checkenv.md) - Validate environment pointer and initialization

### Numeric Validation

2. [cxf_check_nan](functions/validation/cxf_check_nan.md) - Detect NaN in double value
3. [cxf_check_nan_or_inf](functions/validation/cxf_check_nan_or_inf.md) - Detect NaN or Infinity
4. [cxf_validate_array](functions/validation/cxf_validate_array.md) - Validate numeric array

### Model Data Validation

5. [cxf_validate_vartypes](functions/validation/cxf_validate_vartypes.md) - Validate types, clamp binary bounds

### Feature Detection

6. [cxf_check_model_flags1](functions/validation/cxf_check_model_flags1.md) - Detect MIP features
7. [cxf_check_model_flags2](functions/validation/cxf_check_model_flags2.md) - Detect quadratic/conic features
8. [cxf_is_mip_model](functions/validation/cxf_is_mip_model.md) - Check for integer variables
9. [cxf_is_quadratic](functions/validation/cxf_is_quadratic.md) - Check for quadratic objective
10. [cxf_is_socp](functions/validation/cxf_is_socp.md) - Check for SOCP/QCP constraints

## 12. Design Decisions

### 12.1 Key Design Choices

| Decision | Rationale | Alternatives Considered |
|----------|-----------|------------------------|
| Bit manipulation for NaN | Portable, predictable, fast | Floating-point comparison (compiler-dependent) |
| Separate flags1/flags2 | Partition MIP vs continuous features | Single function (less clear semantics) |
| Unlocked environment check | Maximum performance for hot path | Locked check (unnecessary overhead) |
| Conservative NULL handling | Prevent crashes, fail-safe | Strict validation (more crashes) |
| Cached feature flags | O(1) common case, amortized cost | Always scan model (too slow) |
| NULL model → assume LP | Safe default for algorithm selection | NULL model → error (less convenient) |

### 12.2 Known Limitations

- cxf_validate_array has O(n) cost, may be slow for large arrays
- Feature detection may be O(n) worst case if flags are stale
- No validation of constraint structure (handled elsewhere)
- Binary bound clamping is implicit (may surprise users)
- NaN/Inf detection doesn't distinguish signaling vs quiet NaN

### 12.3 Future Improvements

- SIMD-accelerated array validation for large arrays
- More granular feature flags (specific constraint types)
- Validation result caching for repeated queries
- Lazy flag updates with versioning for staleness detection
- Support for validation callbacks (user-defined checks)
- Validation profiles (strict/lenient modes)

## 13. References

- IEEE 754-2008: IEEE Standard for Floating-Point Arithmetic
- "What Every Computer Scientist Should Know About Floating-Point Arithmetic"
- Mixed-Integer Programming: Wolsey (1998)
- Convex Optimization: Boyd & Vandenberghe (2004)
- Convexfeld Optimization Reference Manual

## 14. Validation Checklist

Before finalizing this spec:

- [x] All public functions documented
- [x] All dependencies identified
- [x] Thread safety analyzed
- [x] Error handling complete
- [x] No implementation details leaked
- [x] Module boundaries clear
- [x] Performance characteristics specified
- [x] Design rationale provided

---

*Based on: 10 function specifications in cleanroom/specs/functions/validation/*

---
*Convexfeld LP Solver Specification*
*Based on published optimization literature*
