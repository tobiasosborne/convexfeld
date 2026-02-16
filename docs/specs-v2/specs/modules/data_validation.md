# Module: Data Validation

## Purpose

The Data Validation module provides functions that verify the correctness and consistency of user-supplied and solver-generated data arrays before they are consumed by the solver's computational core. Unlike the Input Validation module (which guards pointers, sentinels, and scalar preconditions), this module focuses on verifying the contents of arrays and composite data structures: coefficient arrays that must not contain NaN values, variable type arrays that must contain recognized type codes, solution vectors that must satisfy constraint and bound feasibility, and variable configurations that must meet structural requirements for specialized pivot algorithms. These functions operate on the model's mathematical data and are called both at API entry points (to reject invalid user input) and internally (to verify solver-produced solutions and to gate specialized algorithm paths).

## Functions

### cxf_validate_array

**Purpose:** Scan a double-precision array for NaN values and report the first occurrence.

**Signature:**
- Input: env : pointer-to-Environment - The environment used for error message reporting (may be null)
- Input: count : int - The number of elements in the array
- Input: array : pointer-to-array-of-double - The array of values to validate
- Output: int - Zero if all values are valid, or INVALID_ARGUMENT error if any NaN is found

**Preconditions:**
- If count is positive, the array pointer must be valid and must point to at least count double-precision values.
- The environment pointer, if non-null, should be a valid initialized environment.

**Postconditions:**
- If the function returns zero, no element in the first count entries of the array is NaN.
- If the function returns an error, at least one element is NaN, and the error message (if the environment's error buffer is available and unlocked) identifies the index of the first NaN element.

**Side Effects:**
- On detecting a NaN, writes a diagnostic message to the environment's error buffer if all of the following conditions hold: the environment pointer is non-null, the error buffer pointer within the environment is non-null, the error buffer lock flag is zero (unlocked), and the error buffer is currently empty (first character is the null terminator). This protocol respects the cascading error pattern described in the Environment specification, preserving any previously-set root-cause error message.

**Error Conditions:**
- Null array pointer -> returns zero (success; null arrays are accepted as vacuously valid)
- Count is zero or negative -> returns zero (success; empty arrays are vacuously valid)
- Array element is NaN -> INVALID_ARGUMENT error, with error message identifying the element index

**Behavioral Description:**
The function iterates through each element of the array from index 0 to count-1. For each element, it calls the NaN detection function (cxf_check_nan from the Input Validation module) on the element's bit-level representation. On the first NaN detected, the function writes a formatted error message identifying the offending element index and returns an error code immediately. If no NaN is found across all elements, it returns success. The validation is early-exit: only the index of the first NaN is reported, and elements after it are not examined. Note that this function checks only for NaN, not for infinity. Infinite values are permitted by this function. Callers requiring finiteness checks should use cxf_check_is_finite on individual values separately.

**Thread Safety:** conditional (safe if no other thread is writing to the same environment's error buffer concurrently)

**Dependencies:** cxf_check_nan (Input Validation module), string formatting utility.

---

### cxf_validate_vartypes

**Purpose:** Validate that every element of a variable type array contains a recognized LP variable type code.

**Signature:**
- Input: env : pointer-to-Environment - The environment used for error logging
- Input: count : int - The number of type codes in the array
- Input: vartypes : pointer-to-array-of-char - The array of variable type characters to validate
- Output: int - Zero if all type codes are valid, or INVALID_ARGUMENT error if an invalid code is found

**Preconditions:**
- If count is positive, the vartypes pointer must be valid and must point to at least count characters.
- The environment pointer should be a valid initialized environment (used for error logging).

**Postconditions:**
- If the function returns zero, every element in the first count entries of the array is one of the recognized variable type codes (case-insensitive).
- If the function returns an error, at least one element contains an unrecognized type code.

**Side Effects:**
- On detecting an invalid type code, logs an error message through the environment's error logging subsystem, identifying the offending character.

**Error Conditions:**
- Null vartypes pointer -> returns zero (success; null is accepted as vacuously valid)
- Count is zero or negative -> returns zero (success; empty arrays are vacuously valid)
- Unrecognized variable type character -> INVALID_ARGUMENT error, with logged error message

**Behavioral Description:**
The function iterates through each character in the type array. For each character, it performs case normalization: lowercase ASCII letters are converted to uppercase before comparison. The normalized character is then checked against the recognized LP variable type code:

- 'C' : Continuous variable

If the character does not match the recognized code, the function logs an error through the environment's error logging facility (reporting the original, non-normalized character for diagnostic clarity) and returns an error code immediately. Validation is early-exit: only the first invalid character causes an error return.

**Thread Safety:** conditional (safe if no other thread is concurrently modifying the error state of the same environment)

**Dependencies:** Error logging subsystem (cxf_log_error from the Error Handling module).

---

### cxf_validate_solution

**Purpose:** Comprehensively validate a solution vector against all model constraints, variable bounds, and special constraint types, producing detailed violation metrics and optional diagnostic output.

**Signature:**
- Input: model : pointer-to-Model - The model against which the solution is validated
- Input: solution : pointer-to-array-of-double - The solution vector (one value per variable)
- Input: violation_info : pointer-to-ViolationInfo (optional, may be null) - Output structure for detailed violation metrics
- Input: verbose : int - Verbosity level: 0 for silent summary, nonzero for detailed warnings
- Output: int - Zero on success (validation completed), or OUT_OF_MEMORY if workspace allocation fails

**Preconditions:**
- The model pointer must be valid and the model must have matrix data populated (non-null matrix with valid dimensions).
- The solution array must contain at least numVars double-precision values.
- If violation_info is non-null, it must point to a valid, writable ViolationInfo structure.

**Postconditions:**
- On success, the function has computed violation metrics for all constraint categories. If violation_info is non-null, it is populated with the maximum violation and worst-violating index for each category (constraint, bound). Diagnostic messages may have been printed to the solver log.
- The model's modification control state is restored to its pre-call value.
- Any internally allocated workspace has been freed.

**Side Effects:**
- Temporarily modifies and restores the model's modification-blocked flag to allow attribute queries during validation.
- Allocates temporary workspace for constraint activity computation; this workspace is freed before the function returns.
- In verbose mode, prints warning messages to the solver log for violations exceeding their respective tolerances (feasibility tolerance for constraint and bound violations). Also prints diagnostic suggestions about possible numerical causes (large coefficients, wide coefficient ranges, large bounds, large right-hand sides) when overall violations are significantly above tolerance.
- In silent mode, prints a one-line summary of maximum violations per category.

**Error Conditions:**
- Workspace allocation failure -> OUT_OF_MEMORY error (returned immediately; no partial results)

**Behavioral Description:**
The function performs a comprehensive, multi-category solution validation. It proceeds through the following validation phases:

1. **Linear Constraint Feasibility:** Computes the constraint activity vector (A * x) using the model's sparse column representation. If the model has scaling factors, the solution is unscaled before multiplication and the activity is unscaled afterward. The residual (rhs - activity) is computed, and for greater-than-or-equal constraints the residual is negated (following the internal less-than-or-equal normal form convention). For each constraint, the violation is computed based on the constraint sense: absolute residual for equality constraints, or the positive part of the signed residual for inequality constraints. The maximum constraint violation and the sum of all constraint violations are tracked, along with the index of the worst-violating constraint.

2. **Quadratic Constraint Feasibility:** For each quadratic constraint, the function evaluates the quadratic form (x^T Q x + c^T x) minus the right-hand side, computing the violation based on constraint sense (equality, less-than-or-equal, or greater-than-or-equal). The quadratic and linear terms are evaluated by iterating through the sparse coefficient storage.

3. **Indicator Constraint Feasibility:** For models with indicator constraints, the function validates the implied linear constraints when the indicator variable is active.

4. **Variable Bound Feasibility:** For each variable, the function computes the bound violation as the maximum of (lower bound - x) and (x - upper bound), clamped to zero when feasible.

5. **SOS Constraint Feasibility:** For models with SOS (Special Ordered Set) constraints, the function delegates to an internal SOS validation routine that checks SOS1/SOS2 conditions.

6. **General Constraint Feasibility:** For models with general constraints (piecewise-linear, function constraints, etc.), the function delegates to an internal general constraint validation routine.

After all validation phases, the function produces output. In verbose mode, it prints warnings for each category whose maximum violation exceeds the relevant tolerance, and suggests possible numerical causes when the overall violation is large. In silent mode, it prints a compact one-line summary. If a violation_info output structure was provided, it is populated with the per-category maximum violations, violation sums, and worst-violating indices.

The ViolationInfo output structure contains fields for: maximum overall violation, maximum bound violation, maximum constraint violation, accumulated bound violation sum, accumulated constraint violation sum, and the indices of the worst-violating bound and constraint entries.

**Thread Safety:** unsafe (temporarily modifies model state; allocates and frees workspace; writes to solver log)

**Dependencies:** Input Validation module (cxf_check_nan, cxf_check_is_finite), Error Handling module (logging), model attribute query functions, internal SOS validation routine, internal general constraint validation routine, memory allocation subsystem.

---

### cxf_special_check

**Purpose:** Determine whether a variable qualifies for special-case pivot handling in the simplex algorithm by validating its bound constraints, variable flags, and quadratic objective structure.

**Signature:**
- Input: state : pointer-to-SolverState - The simplex solver's runtime state
- Input: varIdx : int - The index of the variable to validate
- Output: int - 1 if the variable qualifies for special pivot handling, 0 if it does not

**Preconditions:**
- The solver state must be fully initialized with valid working bound arrays, variable flag arrays, and (if the variable has quadratic terms) valid quadratic objective storage.
- varIdx must be in the range [0, numVars).

**Postconditions:**
- If the function returns 1, all structural requirements for special pivot handling are satisfied for this variable: its lower bound is not unbounded below, its flag configuration is recognized, its bound configuration is consistent, and (if it has quadratic objective terms) all quadratic entries and their neighbor variables meet the non-negativity and boundedness requirements.
- If the function returns 0, at least one structural requirement is not met and the variable must be handled by the standard pivot path.
- The solver state is not modified except that the optional work counter may be incremented.

**Side Effects:**
- If the solver state has an active work counter (a non-null work tracking pointer), the function increments it proportionally to the number of quadratic matrix entries examined. The increment is the product of a work scaling constant, the solver state's scale factor, and the number of off-diagonal quadratic entries processed. This work tracking is used for performance profiling and iteration limit enforcement. If the work counter pointer is null, no work tracking occurs.

**Error Conditions:**
- None (returns 0 for any disqualifying condition rather than raising an error).

**Behavioral Description:**
The function implements a multi-stage qualification check for the special pivot path. The special pivot is an optimized algorithm variant used when a variable's structure permits simplified pivot operations. The stages are evaluated sequentially with early rejection:

**Stage 1 - Lower Bound Check:** The variable's working lower bound is compared against the solver's negative infinity threshold (the negation of the solver's infinity constant, as defined by the Environment's infinityThreshold parameter). If the lower bound is below this threshold, the variable is effectively unbounded below, which is incompatible with the special pivot because it may introduce unboundedness during bound-flip operations. The variable is rejected.

**Stage 2 - Variable Flag Check:** The variable's flag word is examined against a reserved-bits mask. The flag word uses specific bits to encode special structural properties (such as finite upper bound and participation in quadratic objectives). If any bit outside the recognized set is active, the variable has an unsupported configuration and is rejected. The recognized bits include indicators for finite upper bounds, quadratic participation, and a small number of other structural properties.

**Stage 3 - Bound Consistency Check:** If the flag indicates the variable has a finite upper bound, the function verifies that the lower bound does not exceed the solver's positive infinity threshold. A lower bound above positive infinity combined with a finite upper bound represents an impossible bound configuration, and the variable is rejected.

**Stage 4 - Quadratic Structure Validation:** If the flag indicates the variable participates in the quadratic objective, the function validates the Q-matrix structure for this variable. This involves three sub-checks:

(a) The diagonal entry Q[i,i] must be non-negative (a necessary condition for positive semidefiniteness of the Q matrix in convex QP; see Nocedal and Wright, *Numerical Optimization*, 2006, Section 16.4).

(b) For each off-diagonal entry in the variable's row of the sparse Q matrix, the coefficient must be non-negative.

(c) For each neighbor variable referenced by an off-diagonal Q entry, the neighbor's lower bound must be at or above the negative infinity threshold (i.e., the neighbor must not be unbounded below).

These conditions ensure that fixing the variable at a bound during a special pivot will not introduce unboundedness through the quadratic coupling. The iteration through off-diagonal entries uses the sparse row storage of the Q matrix: for each variable, a start index and count define the range of off-diagonal entries to examine. If any sub-check fails, the loop exits early and the variable is rejected. After the loop (whether completed or exited early), the work counter is updated if present.

If all stages pass, the function returns 1, indicating the variable qualifies for the special pivot path. If any stage fails, it returns 0, and the caller should use the standard pivot operations (bound-flip pivot or primal pivot) instead.

**Thread Safety:** unsafe (may write to work counter; reads mutable solver state arrays)

**Dependencies:** SolverState data model (Layer 1), specifically the working bounds arrays, variable flags array, and quadratic objective sparse storage. Called by the pivot subsystem (cxf_pivot_special).

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
