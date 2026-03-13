# Spec V2 Audit: Error Handling, Callbacks, Logging, Statistics/Diagnostics

Audited: 2026-03-13
Scope: P3.09 (Error Handling), P3.13 (Callbacks), P3.10 (Logging), P3.12 (Statistics & Diagnostics)
Specs: docs/specs-v2/specs/modules/{error_handling,callbacks,logging,statistics_diagnostics}.md
Integration: docs/specs-v2/specs/integration/{error_propagation,callback_protocol}.md
Reference: docs/specs-v2/specs/reference/error_status_codes.md

---

## Files Reviewed

### Spec Files
- docs/specs-v2/specs/modules/error_handling.md (4 functions)
- docs/specs-v2/specs/modules/callbacks.md (6 functions)
- docs/specs-v2/specs/modules/logging.md (3 functions)
- docs/specs-v2/specs/modules/statistics_diagnostics.md (7 functions)
- docs/specs-v2/specs/integration/error_propagation.md
- docs/specs-v2/specs/integration/callback_protocol.md
- docs/specs-v2/specs/reference/error_status_codes.md

### Implementation Files
- src/error/core.c
- src/error/error_reporting.c
- src/error/pivot_check.c
- src/error/model_flags.c
- src/error/env_check.c
- src/error/terminate.c
- src/error/nan_check.c
- src/callbacks/init.c
- src/callbacks/terminate.c
- src/logging/output.c
- src/logging/system.c
- src/logging/format.c
- src/timing/timestamp.c
- include/convexfeld/cxf_error.h
- include/convexfeld/cxf_callback.h

---

## Compliant Functions

### cxf_error_env (P3.09) -- PARTIALLY COMPLIANT
- Signature matches spec (env, error_code, overwrite, format, ...)
- Null env check: returns silently -- COMPLIANT
- Error code always written -- COMPLIANT
- Overwrite + buffer-lock logic -- COMPLIANT
- Empty-buffer-only fallback -- COMPLIANT
- See violations below for deviations.

### cxf_error_model (P3.09) -- PARTIALLY COMPLIANT
- Signature matches spec (model, error_code, overwrite, format, ...)
- Null model/env check -- COMPLIANT
- Delegates to same write_error logic -- COMPLIANT
- See violations below for deviations.

### cxf_set_error_message (P3.09) -- PARTIALLY COMPLIANT
- Signature matches spec (model, error_code)
- Null model/env check -- COMPLIANT
- Uses predefined message table -- COMPLIANT
- See violations below for deviations.

### cxf_env_set_status (P3.09) -- PARTIALLY COMPLIANT
- Signature matches spec (env, error_code)
- Null env check -- COMPLIANT
- Uses same predefined message table -- COMPLIANT
- See violations below for deviations.

### cxf_error_message (helper) -- COMPLIANT
- Maps all 32 standard error codes (10001-10032 + 20001-20003)
- Includes 10015 (IIS_NOT_INFEASIBLE) -- spec says "reserved and has no
  predefined message" but reference/error_status_codes.md assigns it to
  IIS_NOT_INFEASIBLE. See V7 for details.
- Default "Unknown error" fallback -- COMPLIANT (spec says "generic fallback
  message that includes the numeric error code value" -- see V3)

---

## VIOLATIONS

### [V1] cxf_error_env / cxf_error_model -- zero error code not guarded
- **Spec says:** "If the error code is zero, no state is modified" (silent return).
- **Code does:** The write_error() helper unconditionally writes `env->error_code = error_code` even when error_code is 0. There is no early-return for zero error_code in cxf_error_env or cxf_error_model.
- **File:** src/error/error_reporting.c:70-91 (write_error), :97-106 (cxf_error_env), :108-117 (cxf_error_model)
- **Impact:** Calling cxf_error_env(env, 0, ...) will set env->error_code to 0, which may silently clear a previously-set error code. The spec requires no state modification when error_code is zero.

### [V2] cxf_set_error_message / cxf_env_set_status -- zero error code does not clear buffer
- **Spec says:** "If error_code is zero, the error buffer is cleared (set to empty string)."
- **Code does:** When error_code is 0, write_error() is called with msg="Unknown error" (the default case of cxf_error_message(0)). Since error_code 0 means error_code != CXF_ERROR_OUT_OF_MEMORY, overwrite is 0, and the buffer may or may not be empty, the behavior is undefined. The buffer is NOT explicitly cleared to empty string.
- **File:** src/error/error_reporting.c:119-127
- **Impact:** The clearing-to-empty-string behavior required by the spec is completely missing. API entry points that call `cxf_set_error_message(model, 0)` to clear prior error state will not work.

### [V3] cxf_error_message -- fallback does not include numeric code
- **Spec says:** "Error codes that do not appear in the table receive a generic fallback message that includes the numeric error code value."
- **Code does:** Returns static string "Unknown error" with no numeric code included.
- **File:** src/error/error_reporting.c:63
- **Impact:** Users cannot determine which unrecognized error code was returned when the fallback message is displayed.

### [V4] cxf_set_error_message -- missing model structural validation
- **Spec says:** "The model must pass structural validation (valid sentinel, non-null)."
- **Code does:** Only checks `!model || !model->env`. No sentinel/magic number validation is performed on the model.
- **File:** src/error/error_reporting.c:119-120
- **Impact:** Corrupt or freed model pointers may not be caught.

### [V5] cxf_env_set_status -- missing null error_buffer check
- **Spec says:** "Null error buffer pointer (on the environment) -> silent return, no action."
- **Code does:** Does not check whether env->error_buffer is a valid buffer before writing (though since error_buffer is an inline array in the struct this is less critical -- but the spec explicitly describes the buffer as a pointer that could be null).
- **File:** src/error/error_reporting.c:124-127
- **Impact:** If the environment structure changes to use a pointer-based error buffer, this will crash.

### [V6] write_error -- OOM override writes without checking error_buffer pointer
- **Spec says:** For predefined message functions, OOM always overwrites. For custom message functions, the buffer pointer must be non-null.
- **Code does:** write_error() does not check for null error_buffer pointer before snprintf. Since error_buffer is an inline array this works, but the spec explicitly requires a null-buffer-pointer guard.
- **File:** src/error/error_reporting.c:70-91

### [V7] Error code 10015 -- spec inconsistency, code maps it
- **Spec (error_handling.md):** "One error code in the standard range (10015) is reserved and has no predefined message."
- **Spec (error_status_codes.md):** Assigns 10015 to IIS_NOT_INFEASIBLE with a defined description.
- **Code does:** Maps 10015 to "IIS: model not infeasible" -- consistent with error_status_codes.md but inconsistent with error_handling.md.
- **File:** src/error/error_reporting.c:42
- **Impact:** Minor -- the two spec documents disagree. Code follows the reference spec (correct choice).

### [V8] cxf_init_callback_struct -- wrong implementation (memset vs mutex allocation)
- **Spec says:** "Allocate and initialize a mutex for thread-safe callback invocation." The function should: (1) set output parameter to null, (2) allocate memory for a platform mutex, (3) initialize the mutex, (4) write pointer to output parameter. Signature: `(environment, mutex_out) -> int`.
- **Code does:** Takes `(env, void *callbackSubStruct)` and does `memset(callbackSubStruct, 0, 48)`. This zeros a 48-byte region instead of allocating and initializing a mutex. The output-parameter-to-pointer-to-pointer signature is missing entirely.
- **File:** src/callbacks/init.c:39-48
- **Impact:** CRITICAL. No mutex is allocated. The entire callback serialization mechanism specified by the spec is absent. This means concurrent callback invocations are unprotected.

### [V9] cxf_callback_terminate -- wrong signature and behavior
- **Spec says:** Signature: `(model) -> int`. Must detect local vs. remote execution via non-blocking lock test on remote solver lock. For local path: set termination flag on environment's asynchronous state (traversing to root environment). Returns 0 on success, error code on remote failure.
- **Code does:** Signature: `(model) -> void` (returns void, not int). Sets env->terminate_flag directly (no async state traversal to root environment). No remote/local path discrimination. No return value.
- **File:** src/callbacks/terminate.c:59-76
- **Impact:** No return value for callers to check. No remote solver support. No root-environment traversal.

### [V10] cxf_pre_optimize_callback -- NOT IMPLEMENTED
- **Spec says:** Function that validates model and sets error buffer lock flag before optimization.
- **Code does:** No implementation found anywhere in the codebase. Not in src/callbacks/ or anywhere else.
- **Impact:** CRITICAL. Error buffer locking before optimization is missing. First-error preservation during optimization will not work as specified.

### [V11] cxf_post_optimize_callback -- NOT IMPLEMENTED
- **Spec says:** Function that validates model and clears error buffer lock flag after optimization.
- **Code does:** No implementation found anywhere in the codebase.
- **Impact:** CRITICAL. Paired with V10 -- the error buffer lock lifecycle is entirely absent.

### [V12] cxf_getconstrs_callback -- NOT IMPLEMENTED
- **Spec says:** Retrieve constraint matrix data during optimization callback, primarily for remote solver.
- **Code does:** No implementation found.
- **Impact:** Remote solver callback constraint retrieval is absent. Acceptable for LP-only solver without remote solver support.

### [V13] cxf_copy_env_callbacks -- NOT IMPLEMENTED
- **Spec says:** Copy callback registration, configuration, and state from source to destination environment.
- **Code does:** No implementation found.
- **Impact:** Model cloning and child environment callback propagation will not work. Affects concurrent solving and multi-scenario optimization.

### [V14] cxf_errorlog -- NOT IMPLEMENTED
- **Spec says:** (Logging module P3.10) Set predefined error message on environment's error buffer via Model. Behaviorally identical to cxf_set_error_message.
- **Code does:** No implementation found. The function name does not appear in any source file.
- **Impact:** Any caller expecting cxf_errorlog will get a link error.

### [V15] cxf_log -- NOT IMPLEMENTED (replaced by cxf_log_printf)
- **Spec says:** `cxf_log(environment, format, ...)` -- Format and dispatch log message to all active destinations (console, log file, session callback, user callback, remote server). Must implement: reentrancy guard, activation check, destination bitmask, destination change detection, line-by-line dispatch, partial line retention.
- **Code does:** `cxf_log_printf(env, level, format, ...)` exists but has a different name AND different signature (adds `level` parameter not in spec). Behavior deviates massively: no reentrancy guard, no activation check, no destination bitmask, no line-by-line processing, no partial line buffering, no log file support, no session callback, no remote server destination. Only outputs to console + user callback.
- **File:** src/logging/output.c:29-67
- **Impact:** CRITICAL. The logging system is a minimal stub. Five of five output destinations are partially or fully missing. Reentrancy protection is absent.

### [V16] cxf_register_log_callback -- missing CallbackState management
- **Spec says:** Must lazily allocate CallbackState if not present, initialize with sentinels, mutex, timestamps, enabled flag, sentinel guards. Must store secondary user data and suppress-statistics flag. Must support model-based timing inheritance. Signature: `(env, model, callback, user_data_primary, user_data_secondary, suppress_statistics) -> int`.
- **Code does:** Signature is `(env, callback, data) -> int`. Only stores callback pointer and data on environment. No CallbackState allocation, no sentinels, no mutex, no timestamps, no model inheritance, no suppress-statistics flag, no secondary user data.
- **File:** src/logging/output.c:82-93
- **Impact:** CallbackState lifecycle is not managed through log callback registration. Timing infrastructure is absent.

### [V17] cxf_log_printf -- suppresses output when output_flag <= 0 even for callbacks
- **Spec says:** (for cxf_log) "Verbosity == 0 (quiet mode): Console and log file output are suppressed. Session callbacks remain active. User callbacks remain active if registered."
- **Code does:** Returns immediately when `env->output_flag <= 0`, which suppresses ALL output including user callbacks.
- **File:** src/logging/output.c:44-46
- **Impact:** User callbacks registered for log monitoring will not receive messages when output is suppressed. This violates the spec's explicit requirement that callbacks remain active regardless of verbosity.

### [V18] cxf_get_timestamp -- returns elapsed seconds, spec says 64-bit hashed session ID
- **Spec says:** (Statistics & Diagnostics P3.12) Returns a 64-bit integer. Three-step process: (1) get UTC system time, (2) convert to epoch-relative integer, (3) multiply by large odd constant for hash mixing. Result is NOT usable as a timestamp for timing. Used for session IDs and correlation.
- **Code does:** Returns `double` (not int64_t). Uses CLOCK_MONOTONIC (not UTC system time). Returns raw seconds with no hash mixing. IS used as a timestamp for timing measurements.
- **File:** src/timing/timestamp.c:34-45
- **Impact:** The implementation serves a completely different purpose than the spec describes. The spec's cxf_get_timestamp is a session-ID generator; the implementation is an elapsed-time clock. The codebase uses it for timing (correct for their purpose), but the spec function is unimplemented.

---

## Missing Functions

### From P3.09 (Error Handling)
All 4 functions (cxf_error_env, cxf_error_model, cxf_set_error_message, cxf_env_set_status) are PRESENT but have violations as noted above.

### From P3.13 (Callbacks)
| Function | Status |
|----------|--------|
| cxf_init_callback_struct | PRESENT but wrong implementation (V8) |
| cxf_callback_terminate | PRESENT but wrong signature/behavior (V9) |
| cxf_pre_optimize_callback | MISSING (V10) |
| cxf_post_optimize_callback | MISSING (V11) |
| cxf_getconstrs_callback | MISSING (V12) |
| cxf_copy_env_callbacks | MISSING (V13) |

### From P3.10 (Logging)
| Function | Status |
|----------|--------|
| cxf_errorlog | MISSING (V14) |
| cxf_log | MISSING -- replaced by non-compliant cxf_log_printf (V15) |
| cxf_register_log_callback | PRESENT but wrong signature/behavior (V16) |

### From P3.12 (Statistics & Diagnostics)
| Function | Status |
|----------|--------|
| cxf_presolve_stats | MISSING |
| cxf_coefficient_stats | MISSING |
| cxf_compute_coef_stats | MISSING |
| cxf_gencon_stats | MISSING |
| cxf_compute_violations | MISSING |
| cxf_compute_fingerprint | MISSING |
| cxf_get_timestamp | PRESENT but wrong semantics (V18) |

**Total missing: 13 of 20 spec functions entirely absent.**

---

## Extra Functions (not in spec)

These functions exist in the implementation but are not specified in the V2 spec modules under audit:

| Function | File | Notes |
|----------|------|-------|
| cxf_error | src/error/core.c:25 | V1-era error function. Not in V2 spec. Takes (env, format, ...) with no error_code or overwrite. |
| cxf_geterrormsg | src/error/core.c:50 | Public API helper -- may be specified elsewhere. |
| cxf_set_error_string | src/error/core.c:66 | Logging function with V1 semantics. Clears error buffer after logging, which is not spec behavior. |
| cxf_validate_pivot_element | src/error/pivot_check.c:37 | Not in error handling spec. May belong to ratio test module. |
| cxf_special_check | src/error/pivot_check.c:69 | Not in error handling spec. May belong to statistics module. |
| cxf_check_model_flags1 | src/error/model_flags.c:26 | Not in error handling spec. May belong to solver dispatch. |
| cxf_check_model_flags2 | src/error/model_flags.c:73 | Not in error handling spec. May belong to solver dispatch. |
| cxf_checkenv | src/error/env_check.c:24 | Input validation (P3.07), not error handling (P3.09). |
| cxf_check_terminate | src/error/terminate.c:25 | Termination polling. Not in P3.09 or P3.13 spec. |
| cxf_terminate | src/error/terminate.c:54 | Simplified version -- may overlap with cxf_callback_terminate spec intent. |
| cxf_reset_terminate | src/error/terminate.c:70 | Not in spec. |
| cxf_check_nan | src/error/nan_check.c:24 | Data validation (P3.08), not error handling (P3.09). |
| cxf_is_finite | src/error/nan_check.c:46 | Data validation (P3.08), not error handling (P3.09). |
| cxf_set_terminate | src/callbacks/terminate.c:28 | Not in spec. Overlaps with cxf_callback_terminate. |
| cxf_reset_callback_state | src/callbacks/init.c:81 | Not in V2 callbacks spec. May have been in V1. |
| cxf_log_printf | src/logging/output.c:29 | Not in spec. Replaces cxf_log with incompatible signature. |
| cxf_snprintf_wrapper | src/logging/format.c:26 | Utility function, not in spec. |
| cxf_get_logical_processors | src/logging/system.c:29 | Utility function, not in spec. |

---

## Notes

### Severity Summary
- **CRITICAL violations (3):** V8 (no mutex in callback init), V10+V11 (no error buffer lock/unlock lifecycle), V15 (logging system is a stub)
- **HIGH violations (4):** V1 (zero error code not guarded), V2 (zero error code does not clear buffer), V9 (callback_terminate wrong signature), V16 (register_log_callback missing CallbackState)
- **MEDIUM violations (4):** V3 (fallback message missing code), V4 (missing model validation), V14 (cxf_errorlog missing), V17 (callbacks suppressed by output_flag)
- **LOW violations (3):** V5 (null buffer pointer), V6 (OOM no buffer check), V7 (spec inconsistency on 10015)
- **INFORMATIONAL (1):** V18 (cxf_get_timestamp semantic mismatch -- implementation purpose differs from spec)

### Statistics & Diagnostics Module
All 7 functions in the Statistics & Diagnostics module (P3.12) are MISSING from the implementation:
- cxf_presolve_stats
- cxf_coefficient_stats
- cxf_compute_coef_stats
- cxf_gencon_stats
- cxf_compute_violations
- cxf_compute_fingerprint
- cxf_get_timestamp (present but serves wrong purpose)

This entire module is unimplemented. This is expected for an LP-only solver focused on the revised simplex method -- these functions primarily serve QP/MIP/advanced feature diagnostics. However, cxf_compute_violations would be valuable for LP solution quality validation.

### Architecture Note: error_buf_locked Field
The error_buf_locked field IS present on CxfEnv (used in write_error at error_reporting.c:82), but no code ever sets or clears it. The spec requires cxf_pre_optimize_callback and cxf_post_optimize_callback to manage this field. Since those functions are missing (V10, V11), the lock is always false, making the overwrite-protection mechanism inert.

### Legacy Code in src/error/core.c
The file src/error/core.c contains V1-era functions (cxf_error, cxf_geterrormsg, cxf_set_error_string) that predate the V2 spec's 4-function error reporting model. These coexist with the V2-compliant functions in error_reporting.c. The old cxf_error function lacks the error_code and overwrite parameters required by V2. The old cxf_set_error_string function clears the error buffer after logging, which contradicts the V2 error propagation model's requirement that the error buffer persist for user retrieval.

### CallbackContext vs. CallbackState
The implementation uses `CallbackContext` (defined in cxf_callback.h) while the spec describes `CallbackState`. The implementation structure is simpler: it lacks the mutex field, parent link, suppress-statistics flag, sentinel guards, and secondary validation sentinel specified for CallbackState. The implementation stores the callback function pointer directly in CallbackContext, while the spec stores the optimization callback on the Model and the log callback on the Environment, with CallbackState providing shared infrastructure.
