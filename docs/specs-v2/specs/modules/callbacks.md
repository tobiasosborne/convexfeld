# Module: Callbacks

## Purpose

The Callbacks module provides the infrastructure for user callback invocation, solver lifecycle hooks, and callback state management during optimization. It is responsible for initializing the synchronization primitives that protect callback invocations from concurrent access, signaling solver termination from within a callback context, managing the error buffer lock across the optimization lifecycle, retrieving constraint data during optimization callbacks (primarily for remote solver deployments), and propagating callback configuration when child environments or model clones are created.

This module contains a mix of function types that share the "callback" naming convention but serve distinct architectural roles:

1. **Callback infrastructure** (cxf_init_callback_struct): Allocates and initializes the mutex used to serialize callback invocations.
2. **Optimization lifecycle hooks** (cxf_pre_optimize_callback, cxf_post_optimize_callback): Internal hooks called before and after optimization to manage error buffer state. Despite their names, these are NOT user callbacks.
3. **User-facing callback operations** (cxf_callback_terminate, cxf_getconstrs_callback): Functions invoked from within a user callback to interact with the solver.
4. **Callback propagation** (cxf_copy_env_callbacks): Copies callback registration and configuration from one environment to another during environment or model cloning.

## Functions

### cxf_init_callback_struct

**Purpose:** Allocate and initialize a mutex for thread-safe callback invocation.

**Signature:**
- Input: `environment` : pointer-to-Environment - The environment context (passed for API consistency but not used by this function)
- Input: `mutex_out` : pointer-to-pointer-to-Mutex - Output parameter to receive the allocated mutex
- Output: int - Zero on success, or an error code on failure

**Preconditions:**
- The output parameter pointer must be non-null and writable

**Postconditions:**
- On success (return value zero): A mutex has been allocated, initialized, and written to the output parameter. The mutex is ready for use in serializing callback invocations.
- On failure (nonzero return): The output parameter is set to null. No mutex has been allocated.

**Side Effects:**
- Allocates memory for a platform-specific mutex structure
- Initializes the mutex to an unlocked state
- Writes the mutex pointer to the output parameter (always writes null first, then overwrites with the allocated pointer on success)

**Error Conditions:**
- Memory allocation failure -> returns the out-of-memory error code; output parameter is null

**Behavioral Description:**
The function first sets the output parameter to null (ensuring a clean state even on failure). It then allocates memory for a platform mutex structure. If the allocation fails, the out-of-memory error code is returned immediately. On successful allocation, the mutex is initialized to an unlocked state using the platform's mutex initialization API, and the pointer is written to the output parameter. The environment parameter is accepted for API consistency with other initialization functions in the solver but is not referenced in the function body.

This function is called during first-time callback registration (either log callback or optimization callback) as part of the lazy allocation pattern for the CallbackState structure. The resulting mutex is stored in the CallbackState and used to serialize all subsequent callback invocations.

**Thread Safety:** Safe. The function performs only allocation and initialization with no shared state access. However, the caller must ensure that only one thread calls this function for a given CallbackState at a time (enforced by the higher-level callback registration logic).

**Dependencies:**
- Memory allocation (cxf_malloc from the Memory Primitives module)
- Platform mutex initialization API

---

### cxf_callback_terminate

**Purpose:** Signal the solver to terminate from within a callback context, handling both local and remote remote solver solves.

**Signature:**
- Input: `model` : pointer-to-Model - The model whose optimization should be terminated
- Output: int - Zero on success, or an error code on failure

**Preconditions:**
- The model must be valid and associated with an active environment
- The function should be called from within a user callback invocation (either directly or via cxf_terminate)

**Postconditions:**
- For local solves: The termination flag in the environment's asynchronous state has been set, causing the solver to exit gracefully at the next iteration boundary with an interrupted status.
- For remote solver solves: A termination request message has been sent to the remote server via the remote solver communication protocol. The remote solver will terminate at its next opportunity.

**Side Effects:**
- Local path: Sets the termination flag in the environment's asynchronous state structure
- Remote path: Acquires and releases the remote solver lock; sends a termination request message over the remote solver communication channel

**Error Conditions:**
- remote solver communication failure -> returns the error code from the message send operation
- Null asynchronous state (local path) -> silently skips flag setting (no error returned)

**Behavioral Description:**
The function determines whether the current solve is executing locally or on a remote remote solver by attempting a non-blocking lock acquisition on the remote solver synchronization primitive:

1. **Remote path (remote solver):** If the non-blocking lock test succeeds, the solve is operating through a remote solver. The function acquires the full remote solver lock, constructs and sends a termination request message using the remote solver's message protocol, and releases the lock. The remote server processes the termination request and halts the solve.

2. **Local path:** If the non-blocking lock test fails, the solve is running locally. The function accesses the environment's asynchronous state structure (traversing through the model's environment to the root environment) and sets the termination flag. This flag is polled by the solver's main loop at each iteration boundary. When the flag is detected, the solver exits with an interrupted status.

The non-blocking lock test serves as a discriminator between local and remote operation modes. This is an efficient way to determine the execution context without requiring an explicit mode field, since the remote solver lock is only initializable when a remote solver connection is active.

**Thread Safety:** Conditional. The local path performs a single atomic flag write, which is safe against concurrent reads by the solver loop. The remote path acquires the remote solver lock to serialize access to the communication channel. However, the function should not be called concurrently from multiple callbacks on the same model without external synchronization.

**Dependencies:**
- remote solver lock primitives (non-blocking try-lock, full lock, unlock)
- remote solver message protocol (termination message construction and send)
- Model validation and environment traversal

---

### cxf_pre_optimize_callback

**Purpose:** Lock the error buffer before optimization begins to preserve any pre-existing error messages during the solve.

**Signature:**
- Input: `model` : pointer-to-Model - The model about to be optimized
- Output: void

**Preconditions:**
- None (the function handles invalid models gracefully)

**Postconditions:**
- If the model is valid: The environment's error buffer lock flag is set, preventing new error messages from overwriting the buffer contents during optimization. Error codes may still be updated, but the human-readable message text is preserved.
- If the model is invalid: No state change occurs.

**Side Effects:**
- Sets the error buffer locked flag on the model's environment

**Error Conditions:**
- Invalid model (fails structural validation) -> silent return, no action

**Behavioral Description:**
This function is an internal optimization lifecycle hook, NOT a user callback. Despite its name suggesting callback behavior, it is called by the optimization infrastructure at the very beginning of an optimization operation, before the solver loop starts.

The function validates the model using the standard structural validation check (sentinel-based). If validation passes, it sets the error buffer lock flag on the model's environment. This lock causes subsequent error-reporting functions to preserve the existing error message text while still updating the error code. The primary purpose is to ensure that if an error was set before optimization (such as a parameter validation error or license warning), that message is not overwritten by cascading errors that may occur during the solve process.

This function is always paired with cxf_post_optimize_callback, which clears the lock after optimization completes.

**Thread Safety:** Unsafe. The error buffer lock flag is not protected by a mutex. The function is expected to be called from the optimization entry point, which is single-threaded at that stage.

**Dependencies:**
- Model validation (structural validation check)

---

### cxf_post_optimize_callback

**Purpose:** Unlock the error buffer after optimization completes, restoring normal error reporting behavior.

**Signature:**
- Input: `model` : pointer-to-Model - The model that was just optimized
- Output: void

**Preconditions:**
- None (the function handles invalid models gracefully)

**Postconditions:**
- If the model is valid: The environment's error buffer lock flag is cleared, allowing new error messages to be written to the buffer normally.
- If the model is invalid: No state change occurs.

**Side Effects:**
- Clears the error buffer locked flag on the model's environment

**Error Conditions:**
- Invalid model (fails structural validation) -> silent return, no action

**Behavioral Description:**
This function is an internal optimization lifecycle hook, NOT a user callback. It is the complement of cxf_pre_optimize_callback and is called by the optimization infrastructure after the solver loop completes, regardless of the optimization outcome (success, error, user termination, time limit, iteration limit, etc.).

The function validates the model using the standard structural validation check. If validation passes, it clears the error buffer lock flag on the model's environment, restoring the normal error reporting mode where new error messages overwrite the buffer.

The function is idempotent: clearing an already-cleared lock flag has no ill effects. This property ensures correctness even if the function is called multiple times during cleanup or in exception-handling paths.

**Thread Safety:** Unsafe. The error buffer lock flag is not protected by a mutex. The function is expected to be called from the optimization cleanup path, which is single-threaded at that stage.

**Dependencies:**
- Model validation (structural validation check)

---

### cxf_getconstrs_callback

**Purpose:** Retrieve constraint matrix data during an optimization callback, primarily for remote solver deployments supporting lazy constraint and user cut generation.

**Signature:**
- Input: `model` : pointer-to-Model - The model being optimized
- Input: `numnz_out` : pointer-to-pointer - Output parameter to receive the nonzero count or pointer
- Input: `cbeg` : pointer-to-array - Output array for constraint start indices (may be null for count-only query)
- Input: `cind` : pointer-to-array - Output array for variable indices (may be null for count-only query)
- Input: `cval` : pointer-to-array - Output array for coefficient values (may be null for count-only query)
- Output: int - Zero on success, or an error code on failure

**Preconditions:**
- The model must be valid and associated with an environment connected to a remote solver
- The function must be called from within an active optimization callback context
- If data arrays (cbeg, cind, cval) are non-null, they must be pre-allocated with sufficient capacity

**Postconditions:**
- On success with all output arrays non-null: The output arrays are populated with the constraint matrix data in compressed sparse row format (start indices, variable indices, coefficient values), and the nonzero count is set.
- On success with any output array null: Only the nonzero count is returned (count-only query mode).
- On failure: An appropriate error code is returned and error information is reported to the environment.

**Side Effects:**
- Acquires and releases the remote solver communication lock
- Sends remote procedure call messages to the remote solver
- May wait (with polling and sleep) if the remote optimization is still in progress during error recovery
- Copies data from the remote solver's response buffers to the user-provided output arrays
- May set error messages on the environment through the error reporting system

**Error Conditions:**
- Not in a callback context (no active optimization) -> returns callback error code
- License error from remote server -> returns license error code; reports error message from server
- Memory allocation failure on remote server -> returns out-of-memory error code
- Remote communication failure -> waits for optimization to complete, retrieves and reports error details from the server, returns the original error code

**Behavioral Description:**
The function retrieves constraint matrix data from the solver during an active optimization callback. It operates primarily in remote solver scenarios where the optimization is executing on a remote server and the callback is handling communication between client and server.

1. **Context validation:** The function first checks whether an optimization is currently in progress. If not, it returns a callback error code, as constraint retrieval is only valid during an active callback.

2. **Lock acquisition:** The remote solver communication lock is acquired to serialize access to the remote communication channel.

3. **Query mode determination:** If any of the output data arrays (start indices, variable indices, or coefficient values) is null, the function operates in count-only mode, requesting only the number of nonzeros without fetching the actual data.

4. **Remote data request:** A remote procedure call is made to the remote solver to retrieve the constraint data. The request is constructed from a protocol template and includes the model reference.

5. **Data copy:** If the request succeeds and the function is not in count-only mode, the response data is copied from the server's response buffers to the user-provided output arrays. The copy handles three data components separately: start indices (as integer arrays), variable indices (as integer arrays), and coefficient values (as floating-point arrays).

6. **Error recovery:** If the remote request fails (for reasons other than out-of-memory or license errors), the function enters an error recovery path: it waits for the remote optimization to complete (polling with sleep intervals), then makes a secondary request to retrieve detailed error information from the server, and reports the error through the environment's error reporting system.

7. **Lock release:** The remote solver communication lock is released.

**Thread Safety:** Conditional. The function acquires the remote solver lock for all remote communication, serializing access to the communication channel. The function should only be called from within a callback context, which is itself serialized by the CallbackState mutex.

**Dependencies:**
- remote solver lock primitives (acquire, release)
- remote solver RPC protocol (constraint data request, error info request, status check)
- Error reporting (environment error setting from the Error Handling module)
- Logging (progress messages during wait loops from the Logging module)

---

### cxf_copy_env_callbacks

**Purpose:** Copy callback registration, configuration, and state from a source environment to a destination environment, used when creating child environments or model clones.

**Signature:**
- Input: `source_environment` : pointer-to-Environment - The environment to copy callback state from
- Input: `destination_environment` : pointer-to-Environment - The environment to receive callback state
- Input: `model` : pointer-to-Model - Optional model for inheriting timing and configuration from a primary model's callback state (may be null)
- Output: int - Zero on success, or an error code on failure

**Preconditions:**
- The source environment must be valid
- The destination environment must be valid (unless performing a link-only operation)

**Postconditions:**
- On success (return value zero):
  - If the source has a registered log callback: The log callback function pointer and user data have been copied to the destination. The destination's log callback system has been initialized.
  - If the source has an active session with a CallbackState: A new CallbackState has been allocated on the destination (if one did not already exist), initialized with validation sentinels, a mutex, timestamps, sentinel guards, and the enabled flag. User data and the suppress-statistics flag have been copied from the source. The destination's CallbackState is linked to the source's CallbackState via the parent reference. If a model was provided, the timing fields and configuration parameters are inherited from the model's primary environment's CallbackState.
  - If the source has neither an active session nor a CallbackState: Simple callback-related configuration fields have been copied from source to destination.
- On failure (nonzero return): Either the log callback initialization failed or the CallbackState allocation failed due to insufficient memory.

**Side Effects:**
- May initialize the destination environment's log callback system
- Stores the log callback function pointer and user data on the destination environment
- May allocate a new CallbackState structure on the destination environment
- Initializes the CallbackState with validation sentinels, a mutex, timestamps, configuration fields, sentinel guards, and the enabled flag
- Copies the session reference from source to destination
- Links the destination's CallbackState to the source's CallbackState via the parent pointer
- If a model is provided, copies the primary model reference and inherits timing and configuration from the primary model's CallbackState

**Error Conditions:**
- Log callback initialization failure -> returns the error code from initialization; sets an error message on the destination environment indicating failure to set the log callback
- Memory allocation failure during CallbackState creation -> returns the out-of-memory error code

**Behavioral Description:**
The function propagates callback configuration from a source environment to a destination environment in several stages:

1. **Log callback propagation:** If the source environment has a registered log callback (non-null log callback function pointer), the function first initializes the log callback infrastructure on the destination environment, then copies the callback function pointer and user data. If the log callback initialization fails, the function reports an error and returns immediately.

2. **Session and CallbackState check:** The function checks whether the source environment has an active session (non-zero session reference). If there is no active session, or if the source has no CallbackState, only simple callback-related configuration fields are copied from source to destination, and the function returns success.

3. **CallbackState allocation and initialization:** If the source has an active session with a CallbackState, the function prepares the destination's CallbackState. The session reference is copied from source to destination. If the destination does not already have a CallbackState, one is allocated using zero-initialized memory allocation. The newly allocated structure is initialized as follows:
   - The primary validation sentinel is set
   - A mutex is allocated and initialized for the CallbackState (via cxf_init_callback_struct)
   - The environment back-pointer is set to the destination environment
   - Current wall-clock timestamps are recorded in both timestamp fields
   - Configuration fields are zeroed
   - Internal tracking fields are zeroed
   - The enabled flag is set to true
   - The secondary validation sentinel is set
   - Both sentinel guard values are set to their expected constants

4. **Configuration inheritance:** The user data pointer and suppress-statistics flag are copied from the source CallbackState to the destination CallbackState.

5. **Model-aware timestamp inheritance:** If a model is provided, the function retrieves the primary model (the original model from which this model was derived), sets the primary model reference in the destination CallbackState, then copies the registration timestamp, baseline timestamp, and configuration fields from the primary model's environment's CallbackState. This ensures that cloned models and child environments share the same timing baseline for consistent elapsed-time reporting.

6. **Parent linking:** The destination CallbackState's parent pointer is set to the source's CallbackState, establishing the inheritance relationship.

**Thread Safety:** Unsafe. This function modifies callback state on the destination environment without acquiring synchronization primitives. It is expected to be called during environment or model setup, before the destination environment is exposed to concurrent access.

**Dependencies:**
- Log callback initialization (from the Logging module)
- Memory allocation (cxf_calloc from the Memory Primitives module)
- Callback structure initialization (cxf_init_callback_struct from this module)
- Timestamp retrieval (from the Statistics and Diagnostics module)
- Error reporting (from the Error Handling module)

---

## Module-Level Behavioral Notes

### Naming Confusion: "Callback" Overloading

The term "callback" is overloaded in this module's function names, referring to three distinct concepts:

1. **User callbacks** (optimization callbacks, log callbacks): Functions registered by the user that the solver invokes during optimization to report progress or allow intervention. cxf_callback_terminate and cxf_getconstrs_callback operate within this context -- they are called from inside a user callback to interact with the solver.

2. **Lifecycle hooks** (cxf_pre_optimize_callback, cxf_post_optimize_callback): Internal functions called by the optimization infrastructure at the start and end of optimization. Despite their "callback" suffix, these have nothing to do with user callbacks. They manage the error buffer lock, ensuring that the first error message recorded before optimization is preserved throughout the solve.

3. **Callback infrastructure** (cxf_init_callback_struct, cxf_copy_env_callbacks): Functions that set up, initialize, and propagate the callback system itself, including mutex allocation and CallbackState configuration.

Users of this specification should be aware that cxf_pre_optimize_callback and cxf_post_optimize_callback are purely internal lifecycle hooks. They do not invoke user callbacks, do not interact with the CallbackState, and do not involve the callback mutex. Their only relationship to "callbacks" is their name and their position in the optimization lifecycle.

### Error Buffer Locking Pattern

The cxf_pre_optimize_callback / cxf_post_optimize_callback pair implements a first-error preservation pattern for optimization. The typical control flow is:

1. User calls the optimization entry point.
2. cxf_pre_optimize_callback sets the error buffer lock.
3. The optimization loop executes, potentially encountering multiple errors.
4. Because the lock is set, only error codes are updated but the original error message text is preserved.
5. cxf_post_optimize_callback clears the lock.
6. The original (root cause) error message is available to the user.

This pattern ensures that in cascading error scenarios (common during optimization, where one failure triggers multiple downstream failures), the user sees the original error rather than a secondary symptom.

### Callback State Lifecycle

The CallbackState structure (specified in the CallbackState data model) is lazily allocated and shared across both log callbacks and optimization callbacks. This module provides the infrastructure for that lifecycle:

- **cxf_init_callback_struct** allocates the mutex stored within the CallbackState
- **cxf_copy_env_callbacks** handles CallbackState allocation, initialization, and inheritance for child environments
- **cxf_callback_terminate** uses the termination mechanism (via the environment's async state, not the CallbackState) to signal solver termination

The log callback function pointer and its user data reside in the Environment, not in the CallbackState. The CallbackState provides shared synchronization and timing infrastructure for all callback types.

### Compute Server Considerations

Two functions in this module (cxf_callback_terminate, cxf_getconstrs_callback) have dual code paths for local and remote (remote solver) operation:

- **cxf_callback_terminate** uses a non-blocking lock test to discriminate between modes: if the remote solver lock can be acquired, the solve is remote and a termination message is sent; otherwise, a local termination flag is set.
- **cxf_getconstrs_callback** is primarily oriented toward remote solver deployments, using RPC protocol messages to request constraint data from the remote server. For local solves, the function's context validation would typically prevent invocation outside the expected callback context.

### Thread Safety Summary

| Function | Thread Safety | Notes |
|----------|---------------|-------|
| cxf_init_callback_struct | Safe | Pure allocation and initialization; no shared state |
| cxf_callback_terminate | Conditional | Local path uses atomic flag write; remote path acquires remote solver lock |
| cxf_pre_optimize_callback | Unsafe | Called from single-threaded optimization entry point |
| cxf_post_optimize_callback | Unsafe | Called from single-threaded optimization cleanup path |
| cxf_getconstrs_callback | Conditional | Acquires remote solver lock; must be called from within a callback context |
| cxf_copy_env_callbacks | Unsafe | Called during environment/model setup before concurrent access |

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
