# Audit Report: Error Propagation, Parameter System & Callback Protocol

**Auditor:** Agent D2
**Date:** 2026-02-16
**Spec Version:** v2 (cleanroom specifications)
**Implementation Scope:** Error handling, parameter system, and callback protocol modules

---

## Executive Summary

This audit examines the implementation of error propagation (P3.09), parameter system (P3.04), and callback protocol (P3.13) against the v2 integration specifications. The audit identified **24 critical violations** spanning missing functions, incorrect implementations, missing error codes, and incomplete protocol support.

**Key Findings:**
- Missing 4 of 4 core error reporting functions (spec requires cxf_error_env, cxf_error_model, cxf_env_set_status, cxf_set_error_message)
- Implemented similar but incompatible functions (cxf_error, cxf_errorlog, cxf_geterrormsg)
- Missing error buffer lock mechanism entirely
- Missing predefined error message table
- Parameter system incomplete (no string parameters, no parameter table structure, no save/restore mechanism)
- Callback protocol fundamentally incomplete (missing where codes, lifecycle hooks are stubs, no mutex)

**Status:** FAIL - Major architectural misalignment with specifications

---

## Section 1: Error Propagation System

### 1.1 Core Error Reporting Functions

**Spec Requirement:** Four error reporting functions organized in 2x2 matrix (error_propagation.md lines 54-69):
- `cxf_error_env(env, error_code, overwrite, format, ...)` - custom message via environment
- `cxf_error_model(model, error_code, overwrite, format, ...)` - custom message via model
- `cxf_env_set_status(env, error_code)` - predefined message via environment
- `cxf_set_error_message(model, error_code)` - predefined message via model

**Implementation Status:**

| Function | Status | Location | Issue |
|----------|--------|----------|-------|
| `cxf_error_env` | MISSING | - | Not implemented |
| `cxf_error_model` | MISSING | - | Not implemented |
| `cxf_env_set_status` | MISSING | - | Not implemented |
| `cxf_set_error_message` | MISSING | - | Not implemented |

**What exists instead:**
- `cxf_error(env, format, ...)` in `src/error/core.c` - Similar to cxf_error_env but:
  - Missing error_code parameter
  - Missing overwrite parameter
  - Cannot propagate error codes through call stack
  - Always overwrites (no first-error-wins support)
- `cxf_errorlog(env, message)` in `src/error/core.c` - Log output function, not error reporting
- `cxf_geterrormsg(env)` in `src/error/core.c` - Correct retrieval function

**Violation Severity:** CRITICAL - Core error propagation architecture missing

### 1.2 Error Code Management

**Spec Requirement:** Error codes must be propagated separately from messages (error_propagation.md lines 86-103):
- Error code ALWAYS updated on env->errorCode field
- Error message conditionally written based on overwrite flag and buffer state
- This separation allows code reclassification while preserving root-cause message

**Implementation Status:**
- CxfEnv structure has `error_buffer[512]` field (cxf_env.h line 23)
- CxfEnv structure is MISSING `errorCode` field entirely
- No way to set or propagate error codes
- cxf_error() only writes messages, never sets codes

**Violation:** CRITICAL - Cannot implement error code propagation pattern

### 1.3 Error Buffer Locking

**Spec Requirement:** Error buffer lock prevents overwrites during optimization (error_propagation.md lines 151-167):
- `errorBufferLocked` field on Environment
- Set by `cxf_pre_optimize_hook`
- Cleared by `cxf_post_optimize_hook`
- Preserves first error in cascading failures

**Implementation Status:**
- CxfEnv has `error_buf_locked` field (cxf_env.h line 53) - GOOD
- Field is NEVER set or checked by any code
- `cxf_pre_optimize_hook` not found in codebase
- `cxf_post_optimize_hook` not found in codebase
- File comments in core.c reference missing lifecycle hooks (lines 31, 76)

**Violation:** CRITICAL - Lock mechanism non-functional

### 1.4 Predefined Error Message Table

**Spec Requirement:** Predefined message functions map error codes to fixed strings from built-in table (error_propagation.md lines 71-73, error_handling.md line 211):
- Table with ~30 entries
- Standard error codes in contiguous range starting above 10000
- Maps codes to user-facing messages

**Implementation Status:**
- No error message table found in codebase
- No mapping from error codes to messages
- cxf_errorlog() just prints the message passed to it

**Violation:** CRITICAL - Predefined message functions cannot be implemented

### 1.5 Overwrite Parameter Logic

**Spec Requirement:** Custom message functions accept explicit overwrite parameter (error_propagation.md lines 84-103):
```
if overwrite_flag is set AND errorBufferLocked is false:
    should_write = true
else if errorBuffer is currently empty:
    should_write = true
```

**Implementation Status:**
- cxf_error() has no overwrite parameter
- Always writes regardless of buffer state
- No logic to check if buffer is empty
- Cannot implement first-error-wins pattern

**Violation:** MAJOR - Error propagation semantics incorrect

### 1.6 Out-of-Memory Override

**Spec Requirement:** OOM errors always overwrite buffer regardless of lock state (error_propagation.md lines 312-315):
```
if error_code is OUT_OF_MEMORY:
    write predefined OOM message to errorBuffer
    return
```

**Implementation Status:**
- No special handling for OOM errors in any error reporting function
- cxf_error() treats all messages identically

**Violation:** MAJOR - OOM error visibility compromised

### 1.7 Model-Entry Error Functions

**Spec Requirement:** Functions accepting Model pointer resolve to model->env (error_propagation.md lines 64-77):
- cxf_error_model(model, ...) resolves to model->env
- cxf_set_error_message(model, ...) resolves to model->env
- Used during model operations and optimization

**Implementation Status:**
- No model-entry error functions exist
- All error reporting goes through environment pointer directly
- Pattern exists elsewhere: `cxf_pre_optimize_callback(model)` gets env from model (callbacks/invoke.c line 49)

**Violation:** MAJOR - Ergonomics issue, forces callers to manually resolve env

---

## Section 2: Parameter System

### 2.1 Parameter Table Structure

**Spec Requirement:** Environment contains parameter table (parameter_system.md lines 38-47):
- Static definition table with name, type, default, min, max, flags
- Entry allocation for parameter values
- Name registration for lookup
- Support for int, double, and string parameters

**Implementation Status:**
- CxfEnv has individual fields for known parameters (cxf_env.h lines 25-41):
  - feasibility_tol (double)
  - optimality_tol (double)
  - infinity (double)
  - verbosity (int)
  - output_flag (int)
  - refactor_interval (int)
  - max_eta_count (int)
- No parameter table structure
- No dynamic lookup mechanism
- No support for string parameters
- No support for min/max bounds validation beyond hardcoded checks

**Violation:** MAJOR - Extensibility compromised, cannot add parameters without code changes

### 2.2 Parameter Getter Functions

**Spec Requirement:** Generic parameter getter by name (parameter_system.md, P3.04):
- `cxf_getdblparam(env, paramname, valueP)` for double parameters
- `cxf_getintparam(env, paramname, valueP)` for int parameters
- `cxf_getstrparam(env, paramname, valueP)` for string parameters
- Case-insensitive name matching

**Implementation Status:**

| Function | Status | Issues |
|----------|--------|--------|
| cxf_getdblparam | EXISTS | Only supports 3 parameters: FeasibilityTol, OptimalityTol, Infinity. Hardcoded strcmp checks. Case-insensitive (GOOD). (params.c lines 44-70) |
| cxf_getintparam | EXISTS | Only supports 4 parameters: OutputFlag, Verbosity, RefactorInterval, MaxEtaCount. Hardcoded strcmp checks. Case-SENSITIVE (BAD). (params_api.c lines 84-124) |
| cxf_getstrparam | MISSING | Not implemented |

**Violations:**
- MAJOR: No string parameter support
- MINOR: Inconsistent case sensitivity between int and double getters
- MAJOR: Parameters hardcoded, not table-driven

### 2.3 Parameter Setter Functions

**Spec Requirement:** Generic parameter setter by name with validation (parameter_system.md lines 180-200):
- `cxf_setdblparam(env, paramname, newvalue)` for double parameters
- `cxf_setintparam(env, paramname, newvalue)` for int parameters
- `cxf_setstrparam(env, paramname, newvalue)` for string parameters
- Range validation against parameter table min/max

**Implementation Status:**

| Function | Status | Issues |
|----------|--------|--------|
| cxf_setdblparam | MISSING | Not implemented |
| cxf_setintparam | EXISTS | Validates 4 parameters with hardcoded ranges. Case-sensitive. (params_api.c lines 22-74) |
| cxf_setstrparam | MISSING | Not implemented |

**Violations:**
- MAJOR: No double parameter setter (cannot set tolerances!)
- MAJOR: No string parameter setter
- MINOR: Validation is hardcoded per parameter, not table-driven

### 2.4 Parameter Backup/Restore Mechanism

**Spec Requirement:** Solver backs up ~30 parameters before optimization and restores after (parameter_system.md lines 88-89, 112-122):
- Implemented in cxf_solver_dispatch
- Backs up method, tolerances, thread counts, algorithm tuning params
- Restores on both success and error paths

**Implementation Status:**
- No parameter backup/restore mechanism found in codebase
- No cxf_solver_dispatch function found
- No cxf_solve_lp function found
- Parameter save/restore pattern not implemented anywhere

**Violation:** CRITICAL - User parameter values not protected during optimization

### 2.5 Tolerance Helper Functions

**Spec Requirement:** Fast tolerance getters for inner-loop performance (parameter_system.md line 227):
- Return default on NULL env
- No error checking overhead
- Used in hot paths

**Implementation Status:**
- `cxf_get_feasibility_tol(env)` EXISTS (params.c lines 82-87) - CORRECT
- `cxf_get_optimality_tol(env)` EXISTS (params.c lines 99-104) - CORRECT
- `cxf_get_infinity()` EXISTS (params.c lines 115-117) - CORRECT
- All return defaults on NULL env - GOOD

**Violation:** NONE - This part is correctly implemented

### 2.6 Parameter Inheritance (Child Environments)

**Spec Requirement:** Child environments inherit parameter values at creation (parameter_system.md lines 66-74):
- Model can have private child environment
- One-time copy at creation
- Changes to child don't affect parent
- Changes to parent don't propagate to existing children

**Implementation Status:**
- CxfEnv has `master_env` field pointing to parent (cxf_env.h line 62)
- No code found that creates child environments
- No code found that copies parameters from parent to child
- No parameter table to copy

**Violation:** MAJOR - Per-model parameter isolation not supported

### 2.7 Configuration File Loading

**Spec Requirement:** Parameters loaded from config file during finalization (parameter_system.md lines 51-58):
- Layer 2 override: config file parameters
- Layer 3 override: programmatic settings (highest precedence)
- Snapshot/restore ensures programmatic settings preserved

**Implementation Status:**
- No config file loading found
- No cxf_env_finalize function found
- No layered override mechanism

**Violation:** MINOR - Feature not implemented (but not critical for basic operation)

---

## Section 3: Callback Protocol

### 3.1 Callback Registration Structure

**Spec Requirement:** CallbackContext structure tracks callback state (callback_protocol.md lines 143-184, P1.07):
- Validation sentinels (magic numbers) - GOOD
- Mutex for serialization
- Timing fields
- Enabled flag
- Parent link for inheritance

**Implementation Status:**
- CallbackContext structure exists (cxf_callback.h lines 40-60)
- Has magic numbers (GOOD)
- Has timing fields (GOOD)
- Has enabled flag (GOOD)
- MISSING: mutex field
- MISSING: parent link field
- MISSING: suppress flag field

**Violations:**
- CRITICAL: No mutex - callbacks cannot be serialized
- MAJOR: No parent link - cannot implement callback propagation for clones

### 3.2 Callback WHERE Codes

**Spec Requirement:** WHERE codes indicate solver phase (callback_protocol.md lines 99-120):
- CXF_CB_POLLING - periodic heartbeat
- CXF_CB_PRESOLVE - during presolve phase
- CXF_CB_SIMPLEX - simplex iteration progress
- CXF_CB_BARRIER - barrier iteration progress
- CXF_CB_MESSAGE - log message delivery
- CXF_CB_PRE_SOLVE - before optimization (lifecycle hook)
- CXF_CB_POST_SOLVE - after optimization (lifecycle hook)

**Implementation Status:**
- cxf_callback.h defines only 4 codes (lines 19-22):
  - CXF_CB_PRE_SOLVE = 1 (correct)
  - CXF_CB_POLLING = 2 (correct)
  - CXF_CB_MIP_SOL = 3 (WRONG - MIP not supported, should be removed)
  - CXF_CB_POST_SOLVE = 4 (correct)
- MISSING: CXF_CB_PRESOLVE
- MISSING: CXF_CB_SIMPLEX
- MISSING: CXF_CB_BARRIER
- MISSING: CXF_CB_MESSAGE

**Violations:**
- MAJOR: Cannot report simplex progress
- MAJOR: Cannot report barrier progress
- MAJOR: Cannot report presolve progress
- MINOR: MIP callback code should not exist (MIP removed from v2 specs)

### 3.3 Callback Invocation Protocol

**Spec Requirement:** Callback invocation follows standardized protocol (callback_protocol.md lines 84-96):
1. Acquire CallbackContext mutex
2. Check enabled flag
3. Prepare context (where code, cbdata, user_data)
4. Invoke user function
5. Update timing statistics
6. Release mutex

**Implementation Status:**
- `cxf_pre_optimize_callback()` exists (callbacks/invoke.c lines 42-100)
- `cxf_post_optimize_callback()` exists (callbacks/invoke.c lines 124-179)
- Both functions:
  - Guard-check pattern for NULL pointers (GOOD)
  - Get CallbackContext from env->callback_state (GOOD)
  - Check enabled flag (GOOD)
  - Update timing statistics (GOOD)
  - Set termination flag on non-zero return (GOOD for pre, N/A for post)
- Both functions MISSING:
  - Mutex acquisition/release
  - Where code passed is hardcoded (PRE_SOLVE, POST_SOLVE)
  - No other callback invocation points exist (no SIMPLEX, BARRIER, etc.)

**Violations:**
- CRITICAL: No mutex protection - not thread-safe
- MAJOR: Only 2 invocation points implemented (pre/post), missing all solver progress callbacks

### 3.4 Lifecycle Hooks vs User Callbacks

**Spec Requirement:** Lifecycle hooks are NOT user callbacks (callback_protocol.md lines 73-81):
- `cxf_pre_optimize_hook` - sets error buffer lock (internal)
- `cxf_post_optimize_hook` - clears error buffer lock (internal)
- These are separate from user callback invocations

**Implementation Status:**
- Functions named `cxf_pre_optimize_callback` and `cxf_post_optimize_callback` exist
- These ARE user callback invocations (invoke user function)
- Lifecycle hooks (error buffer lock management) NOT FOUND
- Naming convention confuses lifecycle hooks with user callbacks

**Violations:**
- CRITICAL: Error buffer lock lifecycle not implemented
- MAJOR: Naming violates spec conventions (should be cxf_pre_optimize_hook for lifecycle, distinct from callback invocation)

### 3.5 Callback State Initialization

**Spec Requirement:** `cxf_init_callback_struct` initializes 48-byte callback substructure (callback_protocol.md line 35):
- Zeros memory region
- Called during CallbackState allocation
- Mutex initialization happens here

**Implementation Status:**
- `cxf_init_callback_struct()` exists (callbacks/init.c lines 39-48)
- Zeros 48 bytes of memory (GOOD)
- Env parameter unused (per spec) (GOOD)
- MISSING: Mutex initialization (no mutex field exists)

**Violation:** MAJOR - Cannot initialize mutex because structure lacks mutex field

### 3.6 Callback State Reset

**Spec Requirement:** `cxf_reset_callback_state` resets counters while preserving registration (callback_protocol.md lines 54-80):
- Resets: callback_calls, callback_time, iteration_count, best_obj, start_time, terminate_requested
- Preserves: magic, callback_func, user_data, enabled

**Implementation Status:**
- `cxf_reset_callback_state()` exists (callbacks/init.c lines 81-114)
- Resets correct fields (GOOD)
- Preserves correct fields (GOOD)
- Uses current timestamp for start_time (GOOD)

**Violation:** NONE - Correctly implemented

### 3.7 Termination Signaling

**Spec Requirement:** Termination request mechanism (callback_protocol.md lines 125-134):
- User calls cxf_terminate within callback
- Sets flag on Environment's async state structure
- Separate from CallbackContext to avoid mutex overhead
- Polled by solver main loop at iteration boundaries

**Implementation Status:**
- `cxf_callback_terminate(model)` exists (callbacks/terminate.c lines 59-76)
- Sets env->terminate_flag (GOOD)
- Sets callback_state->terminate_requested (GOOD)
- Sets external flag pointer if configured (GOOD)
- MISSING: Async state structure reference (spec mentions separate async state)
- Flag polling mechanism not audited (solver loop not in scope)

**Violation:** MINOR - Uses fields on Environment/CallbackContext directly, not separate async state (may be acceptable simplification)

### 3.8 Callback Context Lifecycle

**Spec Requirement:** Lazy allocation on first registration (callback_protocol.md lines 188-192):
- Allocated when user first registers callback
- Not allocated on environment creation
- Persists across multiple optimization calls

**Implementation Status:**
- `cxf_callback_create()` exists (callbacks/context.c lines 30-58)
- Sets magic numbers (GOOD)
- Initializes fields to defaults (GOOD)
- Sets enabled=0 initially (GOOD)
- `cxf_callback_free()` exists (callbacks/context.c lines 71-81)
- Clears magic numbers on free (GOOD)
- MISSING: Lazy allocation trigger (env->callback_state allocation on first use)

**Violation:** MINOR - Lifecycle correct but no evidence of lazy allocation pattern in environment code

### 3.9 Callback Propagation for Clones

**Spec Requirement:** Child environments inherit callback configuration (callback_protocol.md lines 285-286):
- `cxf_copy_env_callbacks` creates new CallbackContext for child
- Inherits user_data, suppress flag, timestamps, config from parent
- Child's parentCallbackState links back to source

**Implementation Status:**
- cxf_copy_env_callbacks() NOT FOUND
- No callback propagation mechanism
- No parent link field in CallbackContext

**Violation:** MAJOR - Cannot propagate callbacks to concurrent/multi-scenario solves

### 3.10 Callback Statistics

**Spec Requirement:** Statistics logged after optimization (callback_protocol.md lines 316-324):
- Invocation count
- Cumulative time
- Suppressible via suppressStatisticsLog flag

**Implementation Status:**
- CallbackContext tracks callback_calls and callback_time (GOOD)
- MISSING: suppressStatisticsLog flag
- No logging code found (cxf_optimize not in audit scope)

**Violation:** MINOR - Statistics tracking exists but reporting mechanism not verified

---

## Section 4: Integration Points

### 4.1 Environment Structure Alignment

**Spec Requirement:** Environment (P1.01) must contain:
- errorCode field (int)
- errorBuffer field (string)
- errorBufferLocked field (bool)
- CallbackState pointer
- Parameter table structure

**Implementation Status (cxf_env.h):**
- errorCode: MISSING
- error_buffer[512]: EXISTS (line 23)
- error_buf_locked: EXISTS (line 53) but unused
- callback_state: EXISTS (line 61)
- Parameter table: MISSING (individual fields instead)

**Violation:** CRITICAL - Missing errorCode field breaks error propagation architecture

### 4.2 Function Naming Mismatches

**Spec Functions vs Implementation:**

| Spec Name | Impl Name | Status |
|-----------|-----------|--------|
| cxf_error_env | cxf_error | Renamed & signature changed |
| cxf_errorlog | cxf_errorlog | Same name but wrong semantics (logging not error reporting) |
| cxf_set_error_string | Not found | Missing |
| cxf_pre_optimize_hook | Not found | Missing (confused with callback invocation) |
| cxf_post_optimize_hook | Not found | Missing (confused with callback invocation) |
| cxf_copy_env_callbacks | Not found | Missing |

**Violation:** MAJOR - Misnamed functions suggest different implementation approach than spec

---

## Section 5: Positive Findings

Despite numerous violations, some components are correctly implemented:

### 5.1 Well-Implemented Features

1. **Tolerance helper functions** (params.c)
   - Fast, no-error-checking getters
   - Correct default return on NULL
   - Used for hot-path performance

2. **Basic callback structure** (cxf_callback.h)
   - Magic numbers for validation
   - Timing fields
   - Enabled flag

3. **Callback reset logic** (callbacks/init.c)
   - Correctly resets statistics
   - Preserves registration
   - Uses proper timestamp

4. **Guard-check pattern in callbacks** (callbacks/invoke.c)
   - NULL pointer safety
   - Progressive validation
   - Clean failure path

5. **Termination signaling basics** (callbacks/terminate.c)
   - Sets multiple flags
   - Handles external pointer
   - NULL-safe

### 5.2 Code Quality

- File organization is clean (separate files for core, model_flags, terminate)
- Comments explain deferred features clearly
- NULL safety patterns used consistently
- No memory leaks in audited code

---

## Section 6: Recommended Actions

### Priority 1: Critical Fixes (Blocks Spec Compliance)

1. **Add errorCode field to CxfEnv**
   - Type: int
   - Initialize to 0
   - Update on every error

2. **Implement 4 core error reporting functions**
   - cxf_error_env(env, code, overwrite, format, ...)
   - cxf_error_model(model, code, overwrite, format, ...)
   - cxf_env_set_status(env, code)
   - cxf_set_error_message(model, code)
   - Replace cxf_error() with these

3. **Implement error buffer lock lifecycle**
   - cxf_pre_optimize_hook() sets env->error_buf_locked
   - cxf_post_optimize_hook() clears env->error_buf_locked
   - All error reporting functions check lock before write

4. **Create predefined error message table**
   - Array mapping error codes to fixed strings
   - ~30 entries
   - Used by cxf_env_set_status / cxf_set_error_message

5. **Add mutex to CallbackContext**
   - pthread_mutex_t or equivalent
   - Initialize in cxf_init_callback_struct
   - Acquire/release around callback invocations

6. **Implement parameter backup/restore**
   - Save ~30 parameters before optimization
   - Restore on all exit paths (success/error)
   - Store in local struct on stack

### Priority 2: Major Fixes (Limits Functionality)

7. **Add missing callback WHERE codes**
   - CXF_CB_SIMPLEX
   - CXF_CB_BARRIER
   - CXF_CB_PRESOLVE
   - CXF_CB_MESSAGE
   - Remove CXF_CB_MIP_SOL (not in LP-only solver)

8. **Implement parameter table structure**
   - Static definition table
   - Dynamic entry array
   - Name lookup mechanism
   - Min/max validation

9. **Add double/string parameter setters**
   - cxf_setdblparam()
   - cxf_setstrparam()

10. **Add parent link to CallbackContext**
    - parentCallbackState pointer
    - Set during callback propagation
    - Used for shared timing baselines

11. **Implement cxf_copy_env_callbacks**
    - Create child CallbackContext
    - Inherit configuration from parent
    - Set parent link

### Priority 3: Minor Fixes (Polish)

12. **Standardize case sensitivity**
    - Make all parameter getters/setters case-insensitive

13. **Add suppressStatisticsLog flag**
    - Field on CallbackContext
    - Control callback statistics logging

14. **Remove MIP callback code**
    - CXF_CB_MIP_SOL constant
    - Any MIP-related callback logic

15. **Implement config file loading**
    - Read parameters from file during finalization
    - Apply layered override precedence

---

## Section 7: Impact Assessment

### Code Rewrite Scope

| Component | Lines to Change | Effort |
|-----------|----------------|--------|
| Error reporting functions | ~200 new + ~100 refactor | 2-3 days |
| Error message table | ~50 new | 0.5 days |
| Error buffer lock lifecycle | ~30 new | 0.5 days |
| Parameter table infrastructure | ~300 new | 3-4 days |
| Parameter setters | ~100 new | 1 day |
| Callback mutex integration | ~50 changes | 1 day |
| Callback WHERE codes | ~100 new | 1 day |
| Callback propagation | ~80 new | 1 day |
| **TOTAL** | ~910 lines | ~10-12 days |

### Breaking Changes

The following changes will break existing code:

1. **Function signature changes**
   - cxf_error() → cxf_error_env() with new parameters
   - All callers must be updated

2. **Error reporting pattern changes**
   - Must provide error code, not just message
   - Must decide overwrite flag at each call site

3. **Parameter access changes**
   - If parameter table implemented, storage location changes
   - Tolerance fields may become accessors

4. **Callback structure changes**
   - Adding mutex requires initialization
   - Adding parent link changes memory layout

### Compatibility Strategy

Two options:

**Option A: Clean Break**
- Implement spec-compliant functions
- Delete old functions
- Update all call sites (force compile errors)
- Pros: Clean codebase
- Cons: All code must be updated at once

**Option B: Gradual Migration**
- Implement new functions alongside old
- Mark old functions deprecated
- Migrate call sites incrementally
- Remove old functions in later phase
- Pros: Can test incrementally
- Cons: Temporary code duplication

**Recommendation:** Option A - code base is small enough for clean break

---

## Section 8: Test Impact

### Test Files to Update

Based on file naming patterns, these test files likely need updates:

```
tests/unit/error_*_test.c
tests/unit/parameters_*_test.c
tests/unit/callbacks_*_test.c
tests/integration/*_test.c (any that check error codes/messages)
```

### New Test Cases Needed

1. Error propagation cascade (10 tests)
   - First-error-wins semantics
   - Overwrite flag behavior
   - OOM override behavior
   - Error code vs message divergence
   - Buffer lock prevents overwrites
   - Multiple cascading errors
   - Model-entry vs env-entry paths
   - Predefined vs custom messages
   - Empty buffer checks
   - Lock lifecycle

2. Parameter save/restore (8 tests)
   - Backup before optimization
   - Restore on success
   - Restore on error
   - Child environment inheritance
   - Config file override
   - Programmatic override precedence
   - Parameter table lookup
   - Range validation

3. Callback protocol (12 tests)
   - Mutex serialization
   - WHERE code dispatch
   - Timing statistics
   - Termination signaling
   - Callback propagation to clones
   - Parent link chain
   - Error buffer lock during callbacks
   - Lazy allocation
   - Statistics suppression
   - Pre/post lifecycle hooks
   - Progress callback invocations
   - Thread safety

**Total new tests:** ~30 tests, est. 1000-1500 lines

---

## Section 9: Documentation Gaps

The following aspects are undocumented or poorly documented:

1. **Error reporting patterns**
   - When to use overwrite=1 vs overwrite=0
   - How error codes map to messages
   - Error cascade examples

2. **Parameter system**
   - Parameter table structure
   - Override precedence rules
   - Save/restore mechanism

3. **Callback protocol**
   - WHERE code meanings
   - Available data per callback type
   - Thread safety guarantees
   - Mutex acquisition rules

4. **Integration points**
   - How error system interacts with callbacks
   - How parameters flow to solver state
   - Lifecycle hook sequencing

**Recommendation:** Create integration guide document showing error/param/callback interactions

---

## Conclusion

The current implementation provides basic functionality but deviates significantly from the v2 specifications in critical areas:

1. **Error propagation** is incomplete (missing 4 core functions, no error codes, no lock mechanism)
2. **Parameter system** is hardcoded rather than table-driven
3. **Callback protocol** lacks thread safety and progress reporting

These are not minor deviations - they represent fundamental architectural misalignments that will accumulate technical debt and block future features.

**Recommendation:** Invest 10-12 days to bring these systems into spec compliance before proceeding with higher-level features. The longer these issues persist, the more expensive they become to fix.

---

## Appendix A: Spec-to-Implementation Mapping

| Spec Module | Spec Functions | Implemented | Missing | Status |
|-------------|---------------|-------------|---------|--------|
| P3.09 Error Handling | cxf_error_env<br>cxf_error_model<br>cxf_env_set_status<br>cxf_set_error_message<br>cxf_geterrormsg<br>cxf_checkenv | cxf_geterrormsg<br>cxf_checkenv | 4 core error reporters | 33% |
| P3.04 Parameters | cxf_getintparam<br>cxf_setintparam<br>cxf_getdblparam<br>cxf_setdblparam<br>cxf_getstrparam<br>cxf_setstrparam<br>cxf_get_infinity<br>cxf_get_feasibility_tol<br>cxf_get_optimality_tol | cxf_getintparam<br>cxf_setintparam<br>cxf_getdblparam<br>cxf_get_infinity<br>cxf_get_feasibility_tol<br>cxf_get_optimality_tol | cxf_setdblparam<br>cxf_getstrparam<br>cxf_setstrparam | 67% |
| P3.13 Callbacks | cxf_init_callback_struct<br>cxf_reset_callback_state<br>cxf_pre_optimize_hook<br>cxf_post_optimize_hook<br>cxf_callback_terminate<br>cxf_copy_env_callbacks | cxf_init_callback_struct<br>cxf_reset_callback_state<br>cxf_callback_terminate | cxf_pre_optimize_hook<br>cxf_post_optimize_hook<br>cxf_copy_env_callbacks | 50% |

**Overall Completion:** ~50% of spec-defined functions exist, but many have incorrect signatures or semantics

---

## Appendix B: Error Code Reference

From cxf_types.h, current error codes:

```c
CXF_OK                      = 0   // Success
CXF_OPTIMAL                 = 1   // Optimal solution
CXF_INFEASIBLE              = 2   // Infeasible
CXF_UNBOUNDED               = 3   // Unbounded
CXF_INF_OR_UNBD             = 4   // Infeasible or unbounded
CXF_ITERATION_LIMIT         = 5   // Iteration limit
CXF_TIME_LIMIT              = 6   // Time limit
CXF_NUMERIC                 = 7   // Numerical difficulties
CXF_ERROR_OUT_OF_MEMORY     = -1  // Memory allocation failed
CXF_ERROR_NULL_ARGUMENT     = -2  // NULL pointer
CXF_ERROR_INVALID_ARGUMENT  = -3  // Invalid argument
CXF_ERROR_DATA_NOT_AVAILABLE = -4 // Data not available
CXF_ERROR_NOT_SUPPORTED     = -5  // Feature not supported
```

**Missing from spec (error_handling.md line 211):**
- Standard error codes should be in contiguous range starting above 10000
- Should have ~30 predefined codes with fixed messages
- Code 10015 reserved (unmapped)

**Action:** Audit full spec list of error codes and add missing entries

---

## Appendix C: Parameter List

Current parameters in implementation:

**Double Parameters (3):**
- FeasibilityTol (getter only)
- OptimalityTol (getter only)
- Infinity (constant, getter only)

**Int Parameters (4):**
- OutputFlag (getter + setter)
- Verbosity (getter + setter)
- RefactorInterval (getter + setter)
- MaxEtaCount (getter + setter)

**From spec (parameter_system.md lines 213-270), missing parameters:**

Method Selection: Method, SiftMethod, SimplexPricing, Crossover, ConcurrentMethod
Tolerances: MarkowitzTol, BarConvTol, PerturbValue
Limits: IterationLimit, TimeLimit, WorkLimit, BarIterLimit
Output: LogToConsole, LogFile, DisplayInterval
Threading: Threads, InheritParams
Tuning: Quad, NormAdjust, Sifting, NetworkAlg, DegenMoves, NumericFocus, ScaleFlag, Seed
Presolve: Presolve, PreDual

**Total missing: ~25 parameters**

---

**End of Audit Report**
