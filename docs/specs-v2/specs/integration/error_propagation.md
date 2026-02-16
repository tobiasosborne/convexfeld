# Integration: Error Propagation

## Overview

This specification describes how errors propagate through the LP solver system, from initial detection deep within internal algorithms through cascading error handling layers to the point where a user retrieves a meaningful error message via the public API. Error propagation is a cross-cutting concern that touches nearly every module in the solver: the error handling primitives (P3.09), the logging subsystem (P3.10), the input and data validation modules (P3.07, P3.08), the solve entry chain (P3.24), the solver dispatch and LP core (P3.25), the barrier and concurrent solvers (P3.26), and the environment and model lifecycle modules (P3.30, P3.31).

The solver's error propagation design follows three governing principles:

1. **First-error-wins semantics.** The error message closest to the root cause is preserved. Subsequent error messages generated as errors cascade up the call stack are suppressed unless they are out-of-memory errors.

2. **Error code propagation via return values.** All internal functions return zero for success and a nonzero error code for failure. Error codes propagate upward through the call stack via return values until they reach the API boundary, where they become the return value visible to the user.

3. **Separation of error code and error message.** The numeric error code is always updated (reflecting the most recent error in the cascade), while the human-readable error message is conditionally written (preserving the first message). This separation allows callers to override the error code for reclassification while keeping the original diagnostic message.

## Components Involved

### Primary Components

| Component | Module | Role in Error Propagation |
|-----------|--------|---------------------------|
| **Error Handling** | P3.09 | Core error reporting primitives: the four functions that write error codes and messages to the Environment |
| **Environment** | P1.01 | Hosts the error state: error code, error buffer, error buffer lock flag |
| **Solve Entry & Dispatch** | P3.24 | API boundary where errors are captured, buffer is cleared at entry, and buffer lock is managed via lifecycle hooks |
| **Callbacks** | P3.13 | Manages the error buffer lock through pre/post-optimize lifecycle hooks |

### Secondary Components

| Component | Module | Role in Error Propagation |
|-----------|--------|---------------------------|
| **Input Validation** | P3.07 | Generates validation errors at API entry points (null pointer, invalid sentinel, NaN detection) |
| **Data Validation** | P3.08 | Generates data content errors (NaN in arrays, invalid variable types, infeasible solutions) |
| **Logging** | P3.10 | Contains cxf_errorlog, which sets predefined error messages; also provides log output for error diagnostics |
| **Solve LP Core** | P3.25 | Generates solver errors (numeric, out-of-memory) and propagates them through the solve chain |
| **Solve Barrier & Concurrent** | P3.26 | Generates Q-not-PSD errors and propagates solver errors |
| **Model Lifecycle** | P3.31 | Generates modification errors (a model-update error message) during lazy update flush |
| **Environment Lifecycle** | P3.30 | Generates initialization errors during environment finalization |

## Flow Description

### Error State Model

The solver uses a single-level error state model anchored on the Environment structure. The error state consists of three fields:

```
Environment
  +-- errorCode       : int     (numeric error code; 0 = success)
  +-- errorBuffer     : string  (human-readable error message; fixed capacity)
  +-- errorBufferLocked : bool  (when true, prevents message overwrites)
```

The Model does not maintain its own error state. All error reporting through a Model resolves to the Model's associated Environment. This means that in a system with multiple Models sharing one Environment, the error state reflects the most recent error from any Model operation on that Environment.

### The Four Error Reporting Functions

Error messages reach the error buffer through a 2x2 matrix of functions organized along two dimensions:

```
                        Custom Message           Predefined Message
                        (format string)          (code lookup table)
                    +------------------------+------------------------+
  Via Environment   |   cxf_error_env         |   cxf_env_set_status    |
                    |   (P3.09)              |   (P3.09)              |
                    +------------------------+------------------------+
  Via Model         |   cxf_error_model       |   cxf_set_error_message |
                    |   (P3.09)              |   (P3.09)              |
                    +------------------------+------------------------+
```

Additionally, cxf_errorlog (P3.10) is behaviorally identical to cxf_set_error_message, despite its placement in the Logging module.

**Custom message functions** accept a printf-style format string and variadic arguments, producing context-specific messages that include runtime values (e.g., "Variable index out of range: 5000"). These are used by internal functions that have detailed knowledge of the error context.

**Predefined message functions** map a standard error code to a fixed message string from a built-in table of approximately 30 entries. These are used at API boundaries and for standard error conditions where a consistent user-facing message is preferred.

**Environment-entry functions** operate directly on an Environment pointer. These are used during environment initialization, licensing, and other operations that occur before any Model exists.

**Model-entry functions** accept a Model pointer and resolve to the Model's associated Environment. These are used during model manipulation, optimization, and all API functions that operate on Models.

### Error Write Decision Logic

Every error reporting function applies the same conditional write logic before modifying the error buffer:

```
function report_error(environment, error_code, message, overwrite_flag):
    if environment is null: return
    if error_code is zero: return

    // Error code is ALWAYS updated
    environment.errorCode = error_code

    if environment.errorBuffer is null: return

    // Message write decision
    should_write = false

    if overwrite_flag is set AND errorBufferLocked is false:
        should_write = true
    else if errorBuffer is currently empty:
        should_write = true

    if should_write:
        write message to errorBuffer (bounded by buffer capacity)
```

For predefined message functions, the logic is slightly different:

```
function set_predefined_error(environment, error_code):
    if error_code is zero:
        clear errorBuffer to empty
        return

    // Out-of-memory ALWAYS overwrites
    if error_code is OUT_OF_MEMORY:
        write predefined OOM message to errorBuffer
        return

    // Other errors: only write to empty buffer
    if errorBuffer is not empty:
        return

    write predefined message for error_code to errorBuffer
```

The key distinction: custom message functions have an explicit `overwrite` parameter, while predefined message functions always use the empty-buffer-only rule (except for out-of-memory).

## State Transitions

### Error Buffer Lifecycle

The error buffer transitions through a well-defined lifecycle during each API call:

```
                                 API Entry
                                    |
                                    v
                         +-------------------+
                         |   CLEARED (empty)  |<-------- cxf_set_error_message(0)
                         +-------------------+           or cxf_env_set_status(0)
                                    |
                           First error occurs
                                    |
                                    v
                         +-------------------+
                         | POPULATED (has msg)|
                         +-------------------+
                                    |
               +--------------------+--------------------+
               |                                         |
    Error cascades upward                     Optimization begins
    (inner error preserved)                   (cxf_pre_optimize_callback)
               |                                         |
               v                                         v
    +-------------------+                     +-------------------+
    | POPULATED (first  |                     |     LOCKED        |
    |  msg preserved)   |                     | (msg frozen)      |
    +-------------------+                     +-------------------+
               |                                         |
               |                                Optimization ends
               |                                (cxf_post_optimize_callback)
               |                                         |
               |                                         v
               |                              +-------------------+
               |                              |    UNLOCKED       |
               |                              | (still populated) |
               +------->------->-------->-----+-------------------+
                                                         |
                                                  API returns
                                                         |
                                                         v
                                              +-------------------+
                                              | USER RETRIEVAL    |
                                              | (cxf_geterrormsg)  |
                                              +-------------------+
                                                         |
                                                  Next API call
                                                         |
                                                         v
                                              +-------------------+
                                              |   CLEARED (empty) |
                                              +-------------------+
```

### Error Buffer States

| State | errorBuffer | errorBufferLocked | Behavior |
|-------|-------------|-------------------|----------|
| **Cleared** | Empty string | false | Ready for new error messages. Any error write succeeds. |
| **Populated** | Contains message | false | First-error-wins: only empty-buffer writes or overwrite-flagged writes succeed. |
| **Locked** | Contains message | true | Frozen: no writes succeed (except OOM override on predefined functions). Error codes still update. |
| **Unlocked** | Contains message | false | Returns to Populated behavior. Existing message preserved unless explicitly overwritten. |

### Error Clearing at API Entry

At the start of each public API call, the error state is reset to provide a clean slate:

1. **cxf_optimize** clears the environment's message buffers and resets message state during Step 4 (State initialization).
2. **cxf_set_error_message** and **cxf_env_set_status** clear the buffer when called with error code zero.
3. Standard API entry patterns call the predefined message setter with code zero to clear prior error state.

This ensures that the error message retrieved after an API call always relates to that specific call, not to a prior operation.

## Error Handling

### Error Cascading Through the Call Stack

Errors propagate upward through the call stack using the return-code convention. Each function in the call chain checks its callees' return values and propagates nonzero error codes to its own caller:

```
cxf_optimize (public API)
    |
    +-- clears error buffer
    +-- calls cxf_optimize_internal
    |       |
    |       +-- calls cxf_solve_entry
    |       |       |
    |       |       +-- calls cxf_solver_dispatch
    |       |       |       |
    |       |       |       +-- calls cxf_solve_lp
    |       |       |       |       |
    |       |       |       |       +-- calls cxf_simplex_init
    |       |       |       |       |       |
    |       |       |       |       |       +-- ALLOCATION FAILS
    |       |       |       |       |       +-- cxf_error_env(env, OOM, overwrite=1,
    |       |       |       |       |       |     "Failed to allocate solver state")
    |       |       |       |       |       +-- returns OOM code  <-- root cause msg written
    |       |       |       |       |
    |       |       |       |       +-- receives OOM code
    |       |       |       |       +-- performs cleanup (dealloc partial state)
    |       |       |       |       +-- returns OOM code  <-- propagates upward
    |       |       |       |
    |       |       |       +-- receives OOM code
    |       |       |       +-- restores ~30 saved parameters
    |       |       |       +-- returns OOM code
    |       |       |
    |       |       +-- receives OOM code
    |       |       +-- restores solver focus / fingerprint flags
    |       |       +-- returns OOM code
    |       |
    |       +-- receives OOM code
    |       +-- cleans up thread-local state
    |       +-- returns OOM code
    |
    +-- receives OOM code
    +-- cxf_pre_optimize_callback (locks buffer -- but too late, msg already set)
    +-- an out-of-memory error message may be set with overwrite=0
    +-- cxf_post_optimize_callback (unlocks buffer)
    +-- clears modification-blocked flag
    +-- releases locale safety state
    +-- returns OOM code to user
```

At each level, the function may optionally report its own error context using `overwrite=0`, which only writes if the buffer is empty. Since the innermost function already wrote the root-cause message, all subsequent writes are suppressed, preserving the most specific diagnostic.

### First-Error Preservation Mechanisms

Three mechanisms work together to preserve the root-cause error message:

1. **Empty-buffer check.** The custom message functions (cxf_error_env, cxf_error_model) check whether the error buffer is empty before writing. When called with `overwrite=0`, they only write to an empty buffer. Since the innermost error reporter writes first, its message persists.

2. **Buffer lock flag.** The error buffer lock (managed by cxf_pre_optimize_callback and cxf_post_optimize_callback) provides an explicit lock that prevents overwrites even when `overwrite=1` is specified. This is used during optimization to protect error messages set before the solve loop from being overwritten by cascading errors during the solve.

3. **Out-of-memory override.** The predefined message functions always write the out-of-memory message regardless of buffer state. This override exists because memory exhaustion is frequently the root cause of cascading failures -- an allocation failure deep in the solver may trigger a chain of secondary failures (cleanup failures, logging failures), and the original OOM message is the most important diagnostic.

### Error Code vs. Error Message Divergence

Because the error code is always updated while the error message is conditionally written, the error code and error message may refer to different error events in a cascade:

- The **error code** reflects the most recent error in the cascade (typically a high-level, less specific error like "model modification failure").
- The **error message** reflects the first error (typically the most specific, root-cause message like "out of memory during matrix allocation").

This divergence is intentional: users typically want the error code for programmatic handling (switch/case on the code) and the error message for diagnostic purposes (log the message for debugging).

## Configuration

### Verbosity and Error Diagnostics

The output verbosity parameter on the Environment affects error diagnostic output:

- **Verbosity >= 1:** Validation failures may produce warning messages printed to the solver output (e.g., "environment may have been freed prematurely" from cxf_check_env in P3.07).
- **Verbosity == 0:** Warning messages are suppressed, but the error code and error buffer are still set.

The verbosity setting does not affect the error buffer or error code -- these are always set regardless of verbosity. Verbosity only affects supplementary diagnostic output via the log system.

### Thread Safety of Error Reporting

The error reporting functions are not internally synchronized. Thread safety considerations:

- The error buffer lock flag is a lightweight single-thread mechanism, not a mutex-based lock. It prevents nested overwrites within a single API call chain, not concurrent access from multiple threads.
- Callers accessing the same Environment from multiple threads must acquire the Environment's critical section before calling error reporting functions.
- In practice, during optimization the solve is single-threaded (at the error reporting level) even when internal parallelism is used, because the error state is only written from the main thread's call chain.

## Design Decisions

### D1: Single Error State on Environment (Not on Model)

The solver maintains error state exclusively on the Environment, not on the Model. This means:

- All Models sharing an Environment share a single error buffer.
- Error messages from one Model's operations may be overwritten by a subsequent operation on a different Model within the same Environment.
- The user must retrieve error messages immediately after the failing API call.

This design simplifies the error handling architecture (one state to manage) and aligns with the public API convention where `cxf_geterrormsg` takes an Environment, not a Model.

### D2: First-Error-Wins Rather Than Last-Error-Wins

The first-error-wins strategy prioritizes root-cause diagnostics over recency. In error cascades (which are common in numerical software), the innermost error message typically provides the most actionable diagnostic information. A last-error-wins strategy would often surface a generic cleanup failure message instead of the specific allocation or numerical failure that started the cascade.

### D3: Out-of-Memory as a Critical Override

Out-of-memory is the only error that unconditionally overwrites the error buffer. This special treatment is justified because:

- OOM errors cause widespread secondary failures (cleanup code that tries to allocate also fails).
- The OOM message is almost always the true root cause when it appears.
- Without the override, OOM errors would be masked by the first-error-wins rule when a less critical error happens to be reported first.

### D4: Separate Overwrite Control for Custom vs. Predefined Messages

Custom message functions (cxf_error_env, cxf_error_model) accept an explicit `overwrite` parameter, giving callers fine-grained control. Predefined message functions (cxf_set_error_message, cxf_env_set_status) do not accept an overwrite parameter and always use the empty-buffer-only rule (except for OOM).

This distinction reflects usage patterns:
- Custom messages are used internally where the caller knows whether it is the first reporter (overwrite=1) or a cascading reporter (overwrite=0).
- Predefined messages are used at API boundaries where the standard message should only appear if no more specific message was already set.

### D5: Buffer Lock via Lifecycle Hooks (Not via Error Functions)

The error buffer lock is managed by the optimization lifecycle hooks (cxf_pre_optimize_callback / cxf_post_optimize_callback from P3.13), not by the error reporting functions themselves. This separates concerns:

- Error reporting functions are simple, stateless operations that check the lock but never set it.
- The lock lifecycle is managed by the optimization entry point, which has the context to decide when locking is appropriate.
- The lock/unlock operations are idempotent, ensuring safety even on multiple cleanup paths.

---

## Error Flow Patterns

### Pattern 1: Validation Error Flow

Validation errors are the simplest flow. They occur at API entry points when user input is invalid.

```
User calls cxf_optimize(model)
    |
    v
cxf_optimize validates model (cxf_checkmodel, P3.07)
    |
    +-- Model pointer is null
    |       |
    |       v
    |   cxf_checkmodel returns NULL_ARGUMENT
    |       |
    |       v
    |   cxf_optimize returns NULL_ARGUMENT to user
    |   (no error buffer write -- there is no valid Environment to write to)
    |
    +-- Model sentinel invalid (freed or corrupt model)
    |       |
    |       v
    |   cxf_checkmodel returns INVALID_ARGUMENT
    |   (optional warning printed if verbosity > 0)
    |       |
    |       v
    |   cxf_optimize returns INVALID_ARGUMENT to user
    |
    +-- Model is valid
            |
            v
        Proceed to optimization...
```

For validation within parameter-setting API calls, the flow includes error message setting:

```
User calls cxf_setintparam(env, "UnknownParam", 5)
    |
    v
Parameter lookup fails (name not in table)
    |
    v
cxf_error_env(env, UNKNOWN_PARAMETER, overwrite=1,
    "Unknown parameter '%s'", "UnknownParam")
    |
    v
Error code set on env, message written to buffer
    |
    v
Returns UNKNOWN_PARAMETER to user
```

### Pattern 2: Solver Error Flow

Solver errors arise during optimization and must propagate through multiple layers.

```
User calls cxf_optimize(model)
    |
    v
cxf_optimize
    +-- clears error buffer
    +-- acquires locale safety
    +-- calls cxf_optimize_internal
            |
            v
        cxf_optimize_internal
            +-- applies pending modifications (cxf_model_apply_modifications)
            |       |
            |       +-- modification succeeds
            |
            +-- dispatches to cxf_solver_dispatch
                    |
                    v
                cxf_solver_dispatch
                    +-- selects barrier method
                    +-- calls cxf_solve_barrier (P3.26)
                    |       |
                    |       +-- Q matrix is not PSD
                    |       +-- cxf_error_model(model, Q_NOT_PSD, overwrite=1,
                    |       |     "Q matrix is not positive semi-definite (PSD).
                    |       |      Set NonConvex parameter to allow...")
                    |       +-- returns Q_NOT_PSD
                    |
                    +-- receives Q_NOT_PSD
                    +-- logs result summary
                    +-- restores ~30 parameters
                    +-- returns Q_NOT_PSD
            |
            v
        cxf_optimize_internal receives Q_NOT_PSD
            +-- non-convex QP handling may retry (if NonConvex param allows)
            +-- if retry fails or not attempted, returns Q_NOT_PSD
    |
    v
cxf_optimize receives Q_NOT_PSD
    +-- cxf_post_optimize_callback (unlocks error buffer)
    +-- clears modification-blocked flag
    +-- releases locale safety
    +-- returns Q_NOT_PSD to user

User calls cxf_geterrormsg(env) -> "Q matrix is not positive..."
```

### Pattern 3: Memory Error Flow (OOM Override)

Out-of-memory errors receive special treatment because they override the error buffer.

```
cxf_solver_dispatch
    +-- calls cxf_solve_lp
            |
            v
        cxf_solve_lp
            +-- calls cxf_simplex_init (P3.22) to allocate SolverState
            |       |
            |       +-- during allocation of working arrays:
            |       +-- cxf_malloc returns null (allocation failure)
            |       +-- cxf_error_env(env, OUT_OF_MEMORY, overwrite=1,
            |       |     "Out of memory allocating pricing arrays")
            |       +-- returns OUT_OF_MEMORY
            |
            +-- receives OUT_OF_MEMORY
            +-- cleanup: deallocates partial solver state
            +-- returns OUT_OF_MEMORY
    |
    v
cxf_solver_dispatch receives OUT_OF_MEMORY
    +-- cxf_set_error_message(model, OUT_OF_MEMORY)
    |     (predefined OOM message OVERRIDES the custom message
    |      because OOM always overwrites)
    +-- restores parameters
    +-- returns OUT_OF_MEMORY
    |
    v
cxf_optimize receives OUT_OF_MEMORY
    +-- cxf_error_model(model, OUT_OF_MEMORY, overwrite=0,
    |     an out-of-memory error message)
    |     (overwrite=0, but buffer already has OOM message, so this is suppressed)
    +-- returns OUT_OF_MEMORY to user
```

Note: In this flow, the predefined OOM message from cxf_set_error_message may override the more specific custom message from cxf_error_env. This is a trade-off in the design: the OOM override ensures the OOM condition is always visible, but it may replace a more specific allocation failure message with a generic one. The numeric error code (OUT_OF_MEMORY) remains the same either way.

### Pattern 4: Model Modification Error Flow

Errors during the lazy update flush (cxf_model_apply_modifications, P3.31) follow a specific pattern.

```
cxf_optimize_internal
    +-- calls cxf_model_apply_modifications (Phase 3)
            |
            v
        cxf_model_apply_modifications
            +-- Phase 7: Name uniqueness validation
            |       |
            |       +-- Duplicate variable name detected
            |       +-- cxf_error_model(model, INVALID_ARGUMENT, overwrite=1,
            |       |     a tag-uniqueness error message)
            |       +-- error_code = INVALID_ARGUMENT
            |
            +-- Phase 8 Error Path:
            +-- cxf_set_error_message(model, error_code)
            |     (buffer not empty, so predefined message is suppressed;
            |      a tag-uniqueness error message preserved)
            +-- logs error code
            +-- clears pending buffer
            +-- returns INVALID_ARGUMENT
    |
    v
cxf_optimize_internal receives INVALID_ARGUMENT
    +-- returns INVALID_ARGUMENT through solve chain
    |
    v
cxf_optimize returns INVALID_ARGUMENT to user

User calls cxf_geterrormsg(env) -> a tag-uniqueness error message
```

### Pattern 5: Environment Initialization Error Flow

Errors during environment finalization (P3.30) occur before any Model exists, so they use the Environment-entry error functions.

```
User calls cxf_loadenv(&env, "logfile.log")
    |
    v
cxf_env_create_internal creates Environment
    |
    v
cxf_env_finalize
    +-- Stage 1: Takes state snapshot for rollback
    +-- Stage 2: Hardware check
    |       |
    |       +-- CPU does not support required SIMD instructions
    |       +-- cxf_env_set_status(env, NOT_SUPPORTED)
    |       |     (predefined message: "Hardware not supported")
    |       +-- cxf_error_env(env, NOT_SUPPORTED, overwrite=1,
    |       |     "This processor does not support...")
    |       |     (overwrite=1 succeeds, replaces generic message)
    |       +-- jumps to Stage 8 (cleanup)
    |
    +-- Stage 8: Error Cleanup
    +-- Restores environment from snapshot (atomic rollback)
    +-- Environment remains in INACTIVE state
    +-- Returns NOT_SUPPORTED

User receives NOT_SUPPORTED return code
User calls cxf_geterrormsg(env) -> "This processor does not support..."
```

### Pattern 6: Callback Error Flow

Errors from user callbacks or remote solver callbacks follow a specialized path through cxf_solve_with_callbacks (P3.24).

```
cxf_solve_with_callbacks
    +-- acquires callback lock
    +-- sets up callback channel
    +-- dispatches solve (async)
    |
    [worker thread encounters error]
    |
    +-- callback result structure populated:
    |     primary_error = CALLBACK
    |     secondary_error = 0
    |     error_message = a callback error message
    |
    +-- unlocks callback channel
    +-- examines callback result:
    |
    +-- Case: OUT_OF_MEMORY
    |       +-- returns OUT_OF_MEMORY immediately
    |
    +-- Case: USER_INTERRUPT
    |       +-- cxf_error_model(model, INTERRUPTED, ...)
    |       +-- returns INTERRUPTED
    |
    +-- Case: Other error (e.g., CALLBACK)
    |       +-- waits for callback thread to complete (polling with sleep)
    |       +-- sends result request to retrieve detailed error message
    |       +-- waits for response
    |       +-- cxf_error_model(model, CALLBACK, overwrite=1,
    |       |     detailed_message_from_callback_thread)
    |       +-- returns CALLBACK
    |
    +-- Case: Secondary error only
            +-- returns secondary error code
```

### Pattern 7: Concurrent / Multi-Scenario Error Flow

In concurrent solving (P3.26) or multi-scenario optimization (P3.24), errors from sub-solves propagate through the dispatch layer.

```
cxf_solve_dispatch (multi-scenario)
    +-- validates multi-objective / multi-scenario compatibility
    |       |
    |       +-- both are set -> INVALID_ARGUMENT
    |       +-- cxf_error_model(model, error_code, overwrite=1,
    |       |     "Multi-objective and multi-scenario cannot be combined")
    |       +-- returns error_code
    |
    +-- creates scenario model clone
    +-- delegates to cxf_solve_entry with clone
    |       |
    |       +-- clone solve fails (e.g., infeasibility detected as error)
    |       +-- error code propagated
    |
    +-- receives error from cxf_solve_entry
    +-- returns error (solution copy-back skipped)
```

## Error Recovery

### State Cleanup on Error

When errors occur, the solver ensures consistency through several cleanup mechanisms:

**Parameter Restore Pattern.** cxf_solver_dispatch (P3.25) saves approximately 30 environment parameters before dispatching to a solver. On both success and error paths, all parameters are restored. This ensures the Environment is never left in a solver-modified state after an error.

**Solver State Deallocation.** cxf_solve_lp (P3.25) deallocates the SolverState structure on all exit paths -- normal completion, error, and early termination. This prevents memory leaks from abandoned solve attempts.

**Modification-Blocked Flag Clearing.** cxf_optimize (P3.24) clears the Model's modification-blocked flag on all exit paths, re-enabling model modifications even after a failed optimization.

**Locale Restoration.** cxf_optimize acquires locale safety state at entry and releases it on all exit paths, ensuring the calling thread's locale is restored even after errors.

**Atomic Initialization Rollback.** cxf_env_finalize (P3.30) takes a full snapshot of the Environment state before attempting finalization. On failure, the snapshot is restored, returning the Environment to its pre-finalization INACTIVE state. This allows the user to retry finalization with different parameters.

**Pending Buffer Clearing.** cxf_model_apply_modifications (P3.31) clears the pending modifications buffer on failure to prevent reprocessing of modifications that caused the error.

### What Is NOT Recovered

Certain state changes are not rolled back on error:

- **The error code and message** are intentionally left set (they ARE the error report).
- **Memory freed during cleanup** is not re-allocated. If a partial solve allocated and then freed working arrays, that memory is released.
- **Log output** produced before the error is not retracted (log writes are irreversible).
- **Timing accumulators** may have been updated with partial measurements.

### Solving After Errors

After an error from cxf_optimize, the user can typically:

1. Retrieve the error message via the public API.
2. Modify the model or parameters to address the error (e.g., adjust tolerances for numeric errors, add the NonConvex parameter for Q-not-PSD errors).
3. Call cxf_optimize again. The next call will clear the error buffer and start fresh.

The parameter restore pattern and modification-blocked clearing ensure the Model and Environment are in a valid state for retry after any error.

---

## Verification Checklist

```
[x] No hex addresses or offsets
[x] No Ghidra artifacts (param_, local_, iVar, DAT_, FUN_)
[x] No binary-specific constants or magic numbers
[x] No copied code fragments
[x] All descriptions are behavioral, not implementational
[x] All data structures described semantically using Layer 1 types
[x] All module references use cleanroom spec identifiers (P1.01, P3.09, etc.)
[x] Error code values described by name, not by numeric literal (where names are used)
[x] Passes the Clean Room Test: could be written by an LP solver expert who has never seen the binary
```
