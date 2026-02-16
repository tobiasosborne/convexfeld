# Module: Input Validation

## Purpose

The Input Validation module provides a set of guard functions that verify the correctness and safety of inputs before they are consumed by the solver's core algorithms. These functions protect against null pointers, use-after-free errors, IEEE 754 special floating-point values (NaN, infinity), invalid string labels, multi-objective misconfiguration, and infeasible solution states. They are called at API entry points and internal dispatch boundaries to enforce preconditions and produce meaningful error diagnostics. Most functions in this module are pure or read-only -- they inspect state without modifying it.

## Functions

### cxf_check_env

**Purpose:** Validate that an environment pointer is non-null, live (not freed), and structurally consistent.

**Signature:**
- Input: env : pointer-to-Environment - The environment pointer to validate
- Output: int - Zero on success, or an error code indicating the nature of the invalidity

**Preconditions:**
- None; this function is designed to be called with arbitrary pointer values as a safety gate.

**Postconditions:**
- If the function returns zero, the environment pointer is non-null, its validation sentinel matches the expected constant, and the root environment's sentinel also matches.
- If the function returns a non-zero error code, no state has been modified.

**Side Effects:**
- On detecting an invalid (but non-null) environment, if the environment's output verbosity flag is positive, a warning message is printed to the solver's output indicating that the environment may have been freed prematurely.

**Error Conditions:**
- Null environment pointer -> NULL_ARGUMENT error
- Environment validation sentinel mismatch -> INVALID_ARGUMENT error
- Root environment validation sentinel mismatch -> INVALID_ARGUMENT error (with optional warning message)

**Behavioral Description:**
The function first checks that the environment pointer is non-null. It then reads the validation sentinel from the beginning of the structure and compares it against the expected constant. If the sentinel matches, it follows the root environment pointer and checks that the root environment's sentinel also matches. Both sentinels must match for the function to return success. If the primary sentinel matches but the root sentinel does not, the function optionally prints a diagnostic warning (gated by the output verbosity setting) before returning an error. This two-level sentinel check guards against both direct use-after-free of the environment and indirect corruption where a child environment outlives its root.

**Thread Safety:** safe (read-only access to immutable-after-initialization sentinel fields)

**Dependencies:** Logging subsystem (for warning output).

---

### cxf_check_nan

**Purpose:** Determine whether a double-precision floating-point value is a NaN (Not-a-Number) per the IEEE 754 standard.

**Signature:**
- Input: value : double (passed as its raw bit representation) - The value to test
- Output: bool - True if the value is any NaN (signaling or quiet, positive or negative), false otherwise

**Preconditions:**
- The input must be the bit-level representation of an IEEE 754 double-precision value.

**Postconditions:**
- Returns true if and only if the input represents a NaN value per IEEE 754 (exponent field is all ones and mantissa is non-zero).

**Side Effects:**
- None. This is a pure function.

**Error Conditions:**
- None. The function is total -- it produces a valid result for all possible 64-bit input values.

**Behavioral Description:**
The function tests whether the input value is a NaN by examining its IEEE 754 bit representation. Per the IEEE 754 standard (IEEE, 2008, Section 6.2.1), a NaN is distinguished from infinity by having a non-zero mantissa while the exponent field consists entirely of set bits. The function masks off the sign bit to obtain the absolute bit pattern and compares it against the bit pattern of positive infinity. Any value whose absolute bit pattern exceeds that of positive infinity is a NaN. This test is branchless and requires no memory access.

**Thread Safety:** safe (pure function, no shared state)

**Dependencies:** None.

---

### cxf_check_is_finite

**Purpose:** Determine whether a double-precision floating-point value is finite (neither NaN nor infinity) per the IEEE 754 standard.

**Signature:**
- Input: value : double (passed as its raw bit representation) - The value to test
- Output: bool - True if the value is finite, false if the value is any NaN or any infinity

**Preconditions:**
- The input must be the bit-level representation of an IEEE 754 double-precision value.

**Postconditions:**
- Returns true if and only if the input represents a finite value (neither NaN nor positive/negative infinity).

**Side Effects:**
- None. This is a pure function.

**Error Conditions:**
- None. The function is total -- it produces a valid result for all possible 64-bit input values.

**Behavioral Description:**
The function tests for finiteness using a branchless arithmetic technique on the IEEE 754 bit representation. It masks off the sign bit to get the absolute bit pattern, then adds a carefully chosen constant such that the unsigned addition overflows (wraps around) if and only if the absolute bit pattern is at or above the infinity threshold. The high bit of the result is extracted by a right shift: it is set (yielding 1) for finite values and clear (yielding 0) for non-finite values. This is equivalent to the C99 `isfinite()` predicate but implemented without branches or library calls for maximum performance. The function complements cxf_check_nan: cxf_check_is_finite rejects both NaN and infinity, while cxf_check_nan rejects only NaN.

**Thread Safety:** safe (pure function, no shared state)

**Dependencies:** None.

---

### cxf_is_finite

**Purpose:** Determine whether a double-precision floating-point value is finite (neither NaN nor infinity) per the IEEE 754 standard.

**Signature:**
- Input: value : double (passed as its raw bit representation) - The value to test
- Output: bool - True if the value is finite, false if the value is any NaN or any infinity

**Preconditions:**
- The input must be the bit-level representation of an IEEE 754 double-precision value.

**Postconditions:**
- Returns true if and only if the input represents a finite value (neither NaN nor positive/negative infinity).

**Side Effects:**
- None. This is a pure function.

**Error Conditions:**
- None. The function is total.

**Behavioral Description:**
This function is identical in behavior to cxf_check_is_finite. Both names refer to the same underlying operation. The implementation uses the same branchless IEEE 754 bit-manipulation technique described in that function's specification.

**Naming history:** Formerly `cxf_check_nan_or_inf`; renamed to better reflect that it returns true for finite values and false for non-finite values (NaN or infinity).

**Thread Safety:** safe (pure function, no shared state)

**Dependencies:** None.

---

### cxf_check_label

**Purpose:** Validate that a model's label-related attributes (variable names, constraint names, and related metadata) are consistent and accessible.

**Signature:**
- Input: model : pointer-to-Model - The model whose label attributes are to be checked
- Input: base_name : string - The base name used to construct label attribute identifiers
- Output: int - Zero on success, or an error code if validation fails

**Preconditions:**
- The model pointer must be valid (sentinel checks are the caller's responsibility).
- The base_name string must be non-null and null-terminated.

**Postconditions:**
- If the function returns zero, all required label attributes are present and accessible on the model.
- The model's modification control state is preserved exactly as it was before the call.

**Side Effects:**
- Temporarily clears the model's modification-blocked flag to permit attribute reads during optimization, then restores it to its original value before returning. This ensures that label validation can be performed even while an optimization is in progress without permanently altering the model's modification lock state.

**Error Conditions:**
- Primary label attribute check fails -> propagated error code from the attribute subsystem
- Secondary label attribute check fails -> propagated error code
- Tertiary label attribute check fails -> propagated error code, except that a DATA_NOT_AVAILABLE error on the third (optional) attribute is suppressed and treated as success

**Behavioral Description:**
The function checks up to three label-related attribute categories on the model, specified by appending different suffixes to the provided base name. It constructs each attribute identifier by formatting the base name with a suffix into a buffer, then invokes the internal attribute checker for that identifier. The checks are sequential with early exit: if the first check fails, the second is not attempted. The third check is conditional on the model having an active attribute table and is lenient -- a data-not-available error on the third attribute is tolerated. Before performing any checks, the function saves and temporarily clears the model's modification-blocked flag to allow attribute reads; this flag is unconditionally restored before returning, regardless of success or failure.

**Thread Safety:** unsafe (modifies model modification-blocked flag temporarily; not safe for concurrent access to the same model)

**Dependencies:** String formatting utility (cxf_snprintf), internal attribute file checker.

---

### cxf_check_multiobj_scenario

**Purpose:** Query the number of multi-objective scenarios configured on a model.

**Signature:**
- Input: model : pointer-to-Model - The model to query
- Output: int - The number of multi-objective scenarios (zero if none configured)

**Preconditions:**
- The model pointer must be valid (non-null, valid sentinel).
- The model's matrix data must be non-null (i.e., the model must have been populated).

**Postconditions:**
- Returns the count of multi-objective scenarios from the model's matrix data.
- No state is modified.

**Side Effects:**
- None. This is a read-only query.

**Error Conditions:**
- None explicitly handled. The function performs no null checks; if the model or its matrix data is null, the behavior is undefined. Callers must validate the model before calling this function.

**Behavioral Description:**
The function reads the multi-objective scenario count from the model's matrix data structure and returns it directly. This count determines whether the multi-objective solve path should be activated during optimization dispatch. A return value of zero indicates standard single-objective optimization. This is a trivial accessor -- a single field read with no computation or validation. The function name uses "check" loosely; it is more accurately a query than a validation function. The caller is responsible for ensuring the model is valid before invoking this function, as it performs no safety checks of its own.

**Thread Safety:** safe (read-only access to immutable-during-solve field)

**Dependencies:** None.

---

### cxf_check_feasibility

**Purpose:** Perform a quick feasibility check on the current LP solution, verifying that variable bounds, general constraint relationships, and piecewise-linear/SOS contributions are satisfied within a given tolerance.

**Signature:**
- Input: model : pointer-to-Model - The model whose solution is to be checked
- Input: tolerance : double - The feasibility tolerance for bound comparisons (typically the environment's FeasibilityTol parameter)
- Output: int - 1 if the solution is feasible, 0 if infeasible or the model is invalid

**Preconditions:**
- If model is non-null, it should have valid matrix data with a populated solution.
- The tolerance should be a small positive value.

**Postconditions:**
- If the function returns 1, the solution satisfies all checked feasibility conditions within the given tolerance, and the matrix's solution status is updated to indicate verified feasibility.
- If the function returns 0, at least one feasibility condition was violated, or the model/matrix was null, or the solution had not yet been computed.

**Side Effects:**
- Reads and writes the solution status field in the model's matrix data. On entry, if the status indicates "needs verification," the function resets it to unknown while checking, then sets it to feasible on success or leaves it as unknown on failure. This caching mechanism allows subsequent calls to return immediately without re-checking.

**Error Conditions:**
- Null model pointer -> returns 0
- Null matrix data -> returns 0
- Solution status is zero (not yet computed) -> returns 0 immediately

**Behavioral Description:**
The function implements a three-phase feasibility check with result caching and early exit on first violation.

First, it checks the cached solution status: if the status indicates the solution has already been verified as feasible, it returns 1 immediately. If the status indicates the solution has not been computed, it returns 0. Only when the status indicates "needs verification" does the full check proceed.

Phase 1 (Variable Bounds): For each variable, the function examines the reduced cost to determine which bound is relevant. Variables with negative reduced costs are checked against their upper bound; variables with positive reduced costs are checked against their lower bound. A violation occurs when a bound exceeds the tolerance in the relevant direction. The tolerance is negated using IEEE 754 sign-bit manipulation for exact arithmetic.

Phase 2 (General Constraint Pairs): For each general constraint pair, the function checks that both linked variables have their lower bounds above negative tolerance and their upper bounds below positive tolerance, verifying that the constraint relationship is satisfied within tolerance.

Phase 3 (Piecewise-Linear/SOS Contributions): For variables that participate in piecewise-linear or SOS constraints, the function checks that the contribution values at the boundaries of each variable's contribution range are consistent with the variable's bounds. A variable passes if it has no SOS data, or if both its lower-bound condition and upper-bound condition are met.

The check uses early exit: the first violation detected in any phase causes an immediate return of 0 without examining remaining variables or constraints. On success across all phases, the solution status is set to feasible and 1 is returned.

**Thread Safety:** unsafe (reads and writes matrix solution status field)

**Dependencies:** None (self-contained numerical checks).

---

## Verification Checklist

```
[x] No hex addresses or offsets
[x] No Ghidra artifacts (param_, local_, iVar, DAT_, FUN_)
[x] No binary-specific constants or magic numbers
[x] No copied code fragments
[x] All algorithms cite published sources where applicable
[x] All data structures described using semantic types from Layer 1 specs
[x] Passes the Clean Room Test
```
