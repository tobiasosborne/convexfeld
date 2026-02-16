# Module: Logging

## Purpose

The Logging module provides the solver's output infrastructure, supporting multiple simultaneous output destinations for solver progress messages, diagnostic information, and error reports. It implements a dual output model where log messages can be directed to any combination of standard output, a log file, a user-provided callback function, a session-level callback, and a remote solver channel. The module also provides the mechanism for users to register custom callback functions that receive all log output programmatically, enabling GUI integration, custom log filtering, and remote monitoring. The module handles reentrancy protection to prevent infinite recursion when callbacks themselves trigger logging, and processes output line-by-line to support destination-specific formatting requirements.

## Output Destination Model

The solver supports five simultaneous log output destinations, each independently enabled or disabled based on environment configuration:

| Destination | Condition | Description |
|-------------|-----------|-------------|
| Standard output (console) | Output verbosity is enabled, console logging is enabled, and the environment is operating in local mode | Direct console output for interactive use |
| Log file | Output verbosity is enabled and a log file handle is open | Persistent file-based logging |
| Session callback | A session reference is registered (active even when verbosity is disabled) | Session-level notification system for framework integration |
| User callback function | A user callback function pointer is registered | Programmatic log capture for GUI, filtering, or custom routing |
| Remote server | The environment is connected to a remote solver with remote logging enabled | Log forwarding to a remote solver using a line-based message protocol |

### Verbosity Control

The output verbosity flag (an integer parameter on the environment) controls the primary logging behavior:

- **Verbosity >= 1 (normal mode):** Console output, log file output, and remote server output are all potentially active (subject to their individual enable conditions). Session callbacks and user callbacks are active if registered.
- **Verbosity == 0 (quiet mode):** Console and log file output are suppressed. Session callbacks remain active (they are not gated on verbosity). User callbacks remain active if registered.
- **Force-output mode:** A special output mode that forces log file output regardless of other settings, used for internal diagnostic situations.

### Line-by-Line Processing

Log messages are processed line-by-line before being dispatched to destinations. This is necessary because:

1. Callback destinations may need to process individual lines (e.g., for display in a GUI text widget).
2. The remote server protocol uses a line-based message format where each line is prefixed with a protocol tag.
3. The maximum line length is bounded to prevent excessively long single-line messages from overflowing destination buffers.

Messages that span multiple lines are split at newline characters and each line is dispatched separately. Partial lines (messages that do not end with a newline) are buffered internally and prepended to the next log call's output.

## Functions

### cxf_set_error_string

**Purpose:** Set a predefined error message on the environment's error buffer based on a standard error code, accessed through a Model.

**Signature:**
- Input: `model` : pointer-to-Model - The model whose environment will receive the error message
- Input: `error_code` : int - A standard solver error code, or zero to clear the error buffer
- Output: void

**Preconditions:**
- The model must pass structural validation (valid sentinel, non-null)

**Postconditions:**
- If error_code is zero, the environment's error buffer is cleared to an empty string
- If the error code is the out-of-memory error code, the corresponding message is always written to the error buffer regardless of existing content
- For other known error codes, the predefined message is written only if the error buffer is currently empty
- For unknown error codes (when the buffer write conditions are met), a generic message containing the numeric code is written

**Side Effects:**
- May modify the environment's error message buffer contents

**Error Conditions:**
- Invalid model (fails structural validation) -> silent return, no action
- Null environment pointer (extracted from model) -> silent return, no action
- Null error buffer pointer (on the environment) -> silent return, no action

**Behavioral Description:**
This function maps standard solver error codes to predefined human-readable message strings and writes them to the environment's error buffer. It first validates the model structure. Then it extracts the environment and error buffer pointer. If the error code is zero, the buffer is cleared. For nonzero codes, it applies the first-error preservation rule: the out-of-memory error always overwrites (as memory exhaustion is typically the root cause of cascading failures), while all other errors write only to an empty buffer. The error code-to-message mapping covers approximately 30 standard error codes spanning memory errors, argument validation errors, attribute and parameter errors, I/O errors, numerical errors, model state errors, quadratic programming errors, network and server errors, and miscellaneous operational errors. One code in the standard range (10015) is reserved and has no mapping. Unrecognized codes receive a fallback message that includes the numeric value.

**Naming history:** Formerly `cxf_errorlog`; renamed to `cxf_set_error_string` to better reflect that it writes to the error message buffer, not to the log output system.

**Thread Safety:** Unsafe. The caller is responsible for thread-safe access to the environment.

**Dependencies:**
- Model validation (structural validation check, same as used by cxf_set_error_message in the Error Handling module)

---

### cxf_log

**Purpose:** Format and dispatch a log message to all active output destinations based on environment configuration.

**Signature:**
- Input: `environment` : pointer-to-Environment - The environment controlling log destination configuration
- Input: `format` : string - A printf-style format string for the log message
- Input: `...` : variadic arguments - Arguments for the format string
- Output: void

**Preconditions:**
- The environment must be in the active state (activation completed successfully)

**Postconditions:**
- The formatted message has been dispatched to all currently active output destinations
- If the message contains complete lines (terminated by newline characters), those lines have been flushed to all destinations
- If the message ends with a partial line (no trailing newline), the partial content is retained in an internal buffer and will be prepended to the next cxf_log call's output

**Side Effects:**
- Writes to standard output (if the console destination is active)
- Writes to the log file (if the file destination is active)
- Invokes the session callback (if a session reference is registered)
- Invokes the user callback function (if registered)
- Sends data to the remote solver (if the remote destination is active)
- Modifies the environment's internal log buffer and log state fields
- Sets and clears the environment's reentrancy guard flag

**Error Conditions:**
- Inactive environment (activation state is not active) -> silent return, no output produced
- Reentrancy detected (logging already in progress on this environment) -> silent return to prevent infinite recursion
- No active destinations (all destinations disabled by configuration) -> silent return after destination evaluation

**Behavioral Description:**
The function performs the following behavioral steps:

1. **Reentrancy check:** The function checks whether logging is already in progress on this environment. If reentrancy is detected (e.g., a callback invoked during logging itself calls cxf_log), the function returns immediately to prevent infinite recursion. If not reentrant, it sets a reentrancy guard flag on the environment.

2. **Activation check:** The function verifies the environment is in the active state. If the environment is not active, no logging occurs.

3. **Destination resolution:** Based on the environment's current configuration (output verbosity, console flag, log file handle, session reference, user callback pointer, remote solver state, and server mode), the function computes a bitmask of active destinations. The destination evaluation logic follows the rules described in the Output Destination Model section above.

4. **Destination change detection:** If the set of active destinations has changed since the last cxf_log call, the internal log buffer is cleared to avoid sending stale partial content to newly activated destinations.

5. **Message formatting:** The format string and variadic arguments are formatted into the environment's internal log buffer, appended after any previously buffered partial line content. The buffer has a fixed capacity (on the order of 1 KB).

6. **Line-by-line dispatch:** The buffer contents are scanned for complete lines. For each complete line (delimited by a newline character or reaching the maximum line length):
   - **Console destination:** The line is written to standard output and the output is flushed.
   - **Log file destination:** The line is written to the log file handle and the file is flushed.
   - **Session callback destination:** The line is copied to a callback communication buffer (with bounded length) and the session callback is invoked with a message event type.
   - **User callback destination:** The line is copied to a temporary buffer and passed to the registered user callback function along with the user's data pointer.
   - **Remote server destination:** The line is formatted with a protocol prefix tag and sent to the remote solver connection.

7. **Partial line retention:** After processing all complete lines, any remaining partial line (content after the last newline) is shifted to the beginning of the internal log buffer for inclusion in the next cxf_log call.

8. **Reentrancy cleanup:** The reentrancy guard flag is cleared.

**Thread Safety:** Conditional. The function uses a per-environment reentrancy guard flag that prevents recursive calls within the same thread. However, concurrent calls to cxf_log from different threads on the same environment are not internally synchronized. The caller must acquire the environment's critical section if cxf_log may be called from multiple threads simultaneously. In practice, logging during optimization is typically serialized by the solver's own control flow, and the reentrancy guard prevents the most common source of concurrent logging (callbacks that log).

**Dependencies:**
- Reentrancy check utility (thread-local or environment-local reentrancy detection)
- Session callback invocation mechanism (from the Callbacks module)

---

### cxf_register_log_callback

**Purpose:** Register or unregister a user-provided callback function that receives all log output from the solver.

**Signature:**
- Input: `environment` : pointer-to-Environment - The environment on which to register the callback
- Input: `model` : pointer-to-Model - Optional model reference for inheriting callback configuration (may be null)
- Input: `callback` : pointer-to-function - The log callback function, or null to unregister
- Input: `user_data_primary` : pointer - Opaque user data pointer passed to the callback on each invocation
- Input: `user_data_secondary` : pointer - Secondary user data stored in the callback state structure
- Input: `suppress_statistics` : int - If nonzero, suppress logging of callback performance statistics at solve completion
- Output: int - Zero on success, or an error code on failure

**Preconditions:**
- No strict preconditions; the function handles a null environment gracefully

**Postconditions:**
- On success (return value zero):
  - If callback is non-null: The log callback function pointer and primary user data are stored on the environment. A CallbackState structure exists on the environment (lazily allocated if not already present). The CallbackState is initialized with timestamps, validation sentinels, a mutex for synchronization, and the provided configuration. Subsequent cxf_log calls on this environment will invoke the registered callback for each log line.
  - If callback is null: The log callback function pointer is cleared. No further callback invocations will occur for log output. The CallbackState is not deallocated (it may still be needed for optimization callbacks).
- On failure (nonzero return): The callback was not registered due to a memory allocation failure. The environment's callback state may be partially initialized.

**Side Effects:**
- Stores the callback function pointer and user data on the environment
- May allocate a CallbackState structure (if one does not already exist)
- Initializes the CallbackState with validation sentinels, a synchronization mutex, timestamps, and configuration fields
- If a model is provided, inherits callback timing and configuration from the model's environment's existing CallbackState

**Error Conditions:**
- Null environment pointer -> returns zero (silent no-op)
- Memory allocation failure during CallbackState creation -> returns the out-of-memory error code

**Behavioral Description:**
The function first stores the callback function pointer and primary user data directly on the environment (these are stored in the environment, not in the CallbackState, as documented in the CallbackState Layer 1 specification). If the callback pointer is null, this effectively unregisters the log callback, and the function returns success immediately.

If the callback is non-null, the function ensures a CallbackState structure exists on the environment. If no CallbackState has been allocated yet (lazy allocation pattern), one is allocated using zero-initialized memory allocation. The newly allocated CallbackState is initialized as follows:
- Primary and secondary validation sentinels are set to their implementation-defined constants
- A mutex is allocated and initialized for thread-safe callback invocation
- The environment back-pointer is set
- Registration and baseline timestamps are recorded from the current wall-clock time
- The enabled flag is set to true
- Sentinel guard values are set to their expected constants
- Internal tracking fields are zeroed

The secondary user data pointer and the suppress-statistics flag are stored in the CallbackState.

If a model reference is provided, the function inherits callback configuration from the model's primary model's environment. Specifically, it copies timing fields and configuration parameters from the existing CallbackState on the model's environment into the newly configured CallbackState. This inheritance mechanism ensures that child environments or model copies share timing baselines with their parent, enabling consistent elapsed-time reporting across related solver sessions.

**Thread Safety:** Conditional. The function should not be called concurrently with active callback invocations on the same environment without external synchronization. Once registered, the CallbackState's mutex protects subsequent callback invocations from concurrent access during parallel optimization.

**Dependencies:**
- Memory allocation (cxf_calloc from the Memory Primitives module)
- Callback structure initialization (from the Callbacks module)
- Timestamp retrieval (from the Statistics & Diagnostics module)

---

## Module-Level Behavioral Notes

### Naming Clarification

The function named cxf_set_error_string is somewhat confusingly placed in this Logging module. It sets a predefined error message on the environment's error buffer, which is the same operation performed by cxf_set_error_message and cxf_env_set_status in the Error Handling module. It is included in the Logging module per the project's function-to-module mapping.

### Relationship to Error Handling Module

The Error Handling module (P3.09) and this Logging module share responsibility for the error message buffer:

- **Error Handling** provides cxf_error_env and cxf_error_model (custom formatted messages) and cxf_set_error_message / cxf_env_set_status (predefined messages from codes).
- **Logging** provides cxf_set_error_string (predefined messages from codes, identical behavior to cxf_set_error_message) and cxf_log / cxf_register_log_callback (log output system, unrelated to the error buffer).

The error message buffer and the log output system are entirely separate subsystems: the error buffer holds the most recent error for user retrieval via the API, while the log system produces ongoing progress and diagnostic output.

### Reentrancy and Callback Safety

The reentrancy guard in cxf_log is critical for preventing infinite recursion. The common scenario is:

1. The solver calls cxf_log to output a progress message.
2. cxf_log invokes the user's registered log callback.
3. The user's callback calls a solver API function that itself calls cxf_log.
4. Without the reentrancy guard, step 3 would recurse back to step 2 indefinitely.

The guard ensures that if cxf_log detects it is already executing on the current environment, the nested call returns immediately without producing output.

### Log Buffer and Partial Line Handling

The environment maintains an internal log buffer that accumulates formatted text between cxf_log calls. This buffer serves two purposes:

1. **Partial line continuity:** If a cxf_log call formats a message that does not end with a newline, the partial content is retained. The next cxf_log call's output is appended to this retained content, so that complete lines eventually form and are dispatched.

2. **Destination change detection:** If the set of active destinations changes between cxf_log calls (e.g., a log file is opened or a callback is registered), the buffer is cleared to prevent sending context-less partial content to newly added destinations.

### Thread Safety Summary

| Function | Thread Safety | Notes |
|----------|---------------|-------|
| cxf_set_error_string | Unsafe | Caller must synchronize |
| cxf_log | Conditional | Reentrancy-protected within one thread; caller must synchronize cross-thread |
| cxf_register_log_callback | Conditional | Must not race with active callback invocations |

---

## Verification Checklist

```
[x] No hex addresses or offsets
[x] No Ghidra artifacts (param_, local_, iVar, DAT_, FUN_)
[x] No binary-specific constants or magic numbers
[x] No copied code fragments
[x] All descriptions are behavioral, not implementational
[x] All data structures described semantically using Layer 1 types
[x] Passes the Clean Room Test
```
