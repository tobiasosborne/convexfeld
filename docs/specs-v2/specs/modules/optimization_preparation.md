# Module: Optimization Preparation

## Purpose

The Optimization Preparation module contains the three functions that handle pre-optimization setup, remote remote solver delegation, and asynchronous result delivery. These functions support the Solve Entry & Dispatch module (P3.24) by providing infrastructure that operates at the boundary between the local solver and external execution contexts.


Together, these functions form the "edges" of the optimization lifecycle: preparing the execution context before the solve begins, delegating work to remote infrastructure when configured, and delivering results back through the remote solver communication layer when the solve completes.

## Functions

### cxf_prepare_optimization

**Purpose:** Install an operating system signal handler to enable graceful user-initiated interruption of a running optimization.

**Signature:**
- Input: `model` : pointer-to-Model - The model about to be optimized
- Output: void

**Preconditions:**
- The model must be valid and have a non-null Environment reference
- The model's Environment must be in an active state

**Postconditions:**
- If the solve lock was acquired successfully and the Environment is not in silent mode: the interrupt signal handler has been installed, the model's interrupt-enabled flag is set, the previous signal handler has been saved on the model for later restoration, and the model reference has been stored in a module-level location accessible to the signal handler
- If the solve lock could not be acquired or the Environment is in silent mode: no state has been modified

**Side Effects:**
- Acquires the solve lock (exclusive mode) to ensure only one optimization can install a signal handler at a time
- Stores a reference to the model in a module-level variable (necessary because signal handlers have a fixed signature and cannot receive additional parameters)
- Sets the model's interrupt-enabled flag to indicate that signal-based interruption is active
- Replaces the operating system's interrupt signal handler with a solver-specific handler, saving the previous handler on the model for later restoration by the cleanup counterpart function (cxf_cleanup_optimization, P3.34)

**Error Conditions:**
- Solve lock acquisition failure -> returns silently without installing the handler
- Environment in silent mode -> returns silently without installing the handler

**Behavioral Description:**
This function prepares the signal-based interrupt mechanism that allows the user to press the standard interrupt key (Ctrl+C) during optimization to request a graceful termination.

**Step 1: Solve lock acquisition.** The function attempts to acquire the solve lock in exclusive mode. This ensures mutual exclusion: only one optimization at a time can own the signal handler. If the lock cannot be acquired, the function returns immediately without modifying any state.

**Step 2: Silent mode check.** The function checks whether the Environment is configured in silent mode. Silent mode is used in embedded or library contexts where the solver should not intercept operating system signals. If silent mode is enabled, the function returns without installing the handler.

**Step 3: Model reference storage.** The function stores the model reference in a module-level variable. This is necessary because operating system signal handlers have a fixed calling convention (they receive only the signal number as a parameter) and cannot receive arbitrary context. The module-level reference allows the signal handler to locate the active model and set the termination flag.

**Step 4: Interrupt flag activation.** The model's interrupt-enabled flag is set to indicate that signal-based interruption is active. The solver's iteration loop checks this flag to determine whether to honor interrupt requests.

**Step 5: Signal handler installation.** The function installs a solver-specific handler for the standard interrupt signal (SIGINT) using the standard C library signal registration mechanism. The previous handler is saved on the model so that it can be restored when optimization completes. This save/restore pattern ensures that the solver's signal handler is active only during optimization, and the application's normal signal handling behavior is restored afterward.

**Thread Safety:** Conditional. The solve lock ensures mutual exclusion for signal handler installation. There is a brief window between storing the model reference and installing the signal handler where a signal could theoretically arrive, but this window is extremely small and protected by the solve lock.

**Dependencies:**
- P3.11 (Threading & Synchronization) - Solve lock acquisition
- P1.01 (Environment) - Silent mode configuration flag
- P1.02 (Model) - Interrupt-enabled flag, previous signal handler storage, Environment reference
- P3.34 (Cleanup Utilities) - cxf_cleanup_optimization restores the previous signal handler

---


**Purpose:** Delegate an optimization request to a remote remote solver, handling both standard and callback-aware execution modes through the server's remote procedure call interface.

**Signature:**
- Input: `model` : pointer-to-Model - The model to optimize remotely
- Output: void

**Preconditions:**
- The model must be configured for remote solver operation (the remote solver mode flag on the model must be set)
- The model's Environment must have an active remote solver connection

**Postconditions:**
- An optimization request has been sent to the remote remote solver through the Environment's server connection
- If the model has registered callbacks, a callback-aware job submission request was sent before the optimization request, enabling the server to relay callback events back to the client
- The function does not wait for the optimization to complete; the result is received asynchronously through the remote solver communication layer

**Side Effects:**
- Queries the model's callback count to determine the execution mode
- For callback-aware mode: checks whether an optimization is already in progress on the server, retrieves job metadata, constructs and sends a job submission request through the server connection
- Constructs and sends an optimization request through the server connection
- All communication with the remote solver occurs through the Environment's server connection handle

**Error Conditions:**
- Server communication failure during job metadata retrieval -> returns without sending the optimization request (callback-aware mode only)
- Server communication failure during job submission -> returns without sending the optimization request (callback-aware mode only)
- Server communication failure during optimization request -> handled by the remote solver communication layer

**Behavioral Description:**
This function implements the client-side logic for delegating an optimization to a remote remote solver. Commercial optimization solvers commonly support a client-server architecture where the client constructs the optimization problem locally, serializes it, and sends it to a remote server for execution. This allows computational resources to be shared across multiple users or applications.

The function operates in two modes, determined by whether the model has user-registered callbacks:

**Callback-aware mode (model has callbacks):**

1. **In-progress check.** The function queries the remote solver to determine whether an optimization is already in progress. If one is, the submission step is skipped (the existing job continues).

2. **Job metadata retrieval.** The function retrieves metadata about the current job context from the server. This metadata enables the server to associate callback events with the correct client session.

3. **Job submission.** A job submission request is constructed from a predefined request template and sent to the server. The submission request registers the callback channel so the server can relay callback events (progress reports, solution events, termination requests) back to the client during optimization.

4. **Optimization request.** After successful submission, a standard optimization request is constructed from a predefined request template and sent to the server to start the actual computation.

**Standard mode (no callbacks):**

1. **Optimization request.** A standard optimization request is constructed from a predefined request template and sent to the server. No callback channel is established.

In both modes, the request messages are built from predefined templates that contain the protocol-specific fields (message type, version information, serialization parameters). The model reference is embedded in the request so that the server can identify which model to optimize.

The function returns immediately after sending the request. It does not wait for the optimization to complete. Result retrieval is handled separately by the remote solver communication layer or by cxf_wait_async.

**Thread Safety:** Conditional. The function does not acquire its own synchronization. The remote solver communication layer provides thread-safe message delivery. The caller is responsible for ensuring that only one optimization request is active per model.

**Dependencies:**
- P1.01 (Environment) - remote solver connection handle
- P1.02 (Model) - Callback count, Environment reference, remote solver mode flag

---

### cxf_wait_async

**Purpose:** Serialize the results of a completed optimization to the remote solver communication channel, transmitting the optimization status, objective value, runtime, and additional solution attributes to the waiting client.

**Signature:**
- Input: `model` : pointer-to-Model - The model with completed optimization results
- Output: int - Zero on success, or an error code if result validation fails

**Preconditions:**
- The model must have a valid Environment with an active remote solver communication channel (a writable stream handle and a synchronization primitive for thread-safe channel access)
- The optimization must have completed (results are available through the model's attribute system)

**Postconditions:**
- On success (return value zero): All result entries have been serialized and flushed to the communication channel. The model's temporary state fields have been restored to their pre-call values.
- On failure (nonzero return): The communication channel may contain a partial message. The model's temporary state fields have been restored regardless.

**Side Effects:**
- Temporarily clears a model status field at the start and restores it on exit (ensures clean attribute retrieval during serialization)
- Queries model attributes through the public attribute API: optimization status, objective value, and runtime
- Acquires and releases the communication channel's synchronization primitive to ensure atomic message delivery
- Writes a structured result message to the communication channel, consisting of a message header and a sequence of typed result entries
- Flushes the communication channel to ensure data delivery

**Error Conditions:**
- Attribute retrieval failure for optimization status -> uses a default status value (model loaded, no solution)
- Attribute retrieval failure for objective value -> uses a sentinel value indicating no result
- Attribute retrieval failure for runtime -> uses zero
- Result entry validation failure for any entry -> returns the error code, releases the synchronization primitive, and restores model state

**Behavioral Description:**
This function is the server-side counterpart to the remote solver optimization request. After the solver completes an optimization on behalf of a remote client, this function packages the results into a structured message and delivers them through the communication channel.

**Phase 1: Model state preparation.** The function saves and temporarily clears a model status field to ensure that attribute retrieval during serialization operates cleanly. This field is always restored during cleanup, regardless of whether the function succeeds or fails.

**Phase 2: Result attribute retrieval.** The function queries three primary result attributes through the model's attribute API:

- **Optimization status** (integer): The outcome of the optimization (optimal, infeasible, unbounded, time limit, etc.). If the attribute query fails, a default status indicating "model loaded, no solution available" is used.
- **Objective value** (double): The best objective value found. If the attribute query fails, a sentinel value indicating "no result available" is used.
- **Runtime** (double): The wall-clock time spent in optimization. If the attribute query fails, zero is used.

These defaults ensure that the function always produces a valid result message, even when the optimization did not produce a complete solution.

**Phase 3: Result descriptor table construction.** The function constructs a table of result descriptors from a predefined template. Each descriptor specifies a result type, an element count, and a reference to the data to be serialized. The primary entries in the table include the model reference (for client-side identification), the optimization status, the objective value, and the runtime. Additional entries may describe supplementary solution attributes.

**Phase 4: Communication channel acquisition.** The function retrieves the communication stream and synchronization primitive from the Environment. All subsequent write operations occur under the synchronization primitive to prevent message interleaving when multiple results are delivered concurrently.

**Phase 5: Message header serialization.** Under the synchronization primitive, the function writes the message header to the communication channel. The header consists of a message type identifier (indicating that this is an optimization result message) and the number of result entries that follow. All multi-byte integers are converted to network byte order before transmission, following standard network protocol conventions.

**Phase 6: Result entry serialization.** For each entry in the result descriptor table:

1. **Validation.** The entry's type and count are validated. If validation fails, the function breaks out of the serialization loop and proceeds to cleanup.

2. **Entry header.** The type code and element count are written in network byte order.

3. **Entry data.** The data is serialized according to its type:
   - **Array types** (integer arrays, floating-point arrays): Each element is individually converted to network byte order and written.
   - **Scalar and special types**: The data is written as a contiguous block whose size is determined by the type and count.

**Phase 7: Flush and cleanup.** The communication channel is flushed to ensure all data is delivered. The synchronization primitive is released. The model status field that was temporarily cleared in Phase 1 is restored to its saved value. The function returns zero on success or the validation error code on failure.

**Thread Safety:** Conditional. The function acquires the communication channel's synchronization primitive (a critical section) to ensure atomic message delivery. Multiple threads may call this function concurrently on different models sharing the same communication channel; the synchronization primitive prevents message interleaving.

**Dependencies:**
- P1.01 (Environment) - Communication stream handle, communication channel synchronization primitive
- P1.02 (Model) - Environment reference, status field, attribute system
- P3.09 (Error Handling) - Error code propagation
- Public attribute API (cxf_getintattr, cxf_getdblattr) for result retrieval

---

## Module-Level Behavioral Notes

### Signal Handler Lifecycle

The signal-based interrupt mechanism spans two functions across two modules:

| Phase | Function | Module | Action |
|-------|----------|--------|--------|
| **Setup** | cxf_prepare_optimization | P3.32 (this module) | Installs the interrupt signal handler and saves the previous handler |
| **Active** | (during optimization) | - | Signal handler is active; receiving the interrupt signal sets a termination flag on the model |
| **Teardown** | cxf_cleanup_optimization | P3.34 (Cleanup Utilities) | Restores the previously saved signal handler |

This lifecycle ensures that the solver intercepts the interrupt signal only during active optimization. Outside of optimization, the application's normal signal handling behavior is preserved. The pattern is standard practice in library code that needs temporary control of signals without permanently altering the application's signal disposition.

### Silent Mode and Embedded Usage

The silent mode check in cxf_prepare_optimization supports embedded usage scenarios. When a solver library is embedded in a larger application (e.g., a web server, a GUI application, or an automated pipeline), the application may already have its own signal handlers or may need signals for other purposes. Silent mode prevents the solver from interfering with the host application's signal management.

### Compute Server Communication Model


```
Client Side                         Server Side
-----------                         -----------
  (sends optimization request)       |
                                     v
                                  [run solver]
                                     |
                                     v
                                  cxf_wait_async
                                    (sends results)
[receive results]  <--------------
```


This client-server delegation pattern is well-established in commercial optimization software, enabling scenarios such as:

- **Shared compute resources:** Multiple users or applications submit optimization requests to a central server pool.
- **Cloud computing:** Optimization problems are sent to cloud instances with more computational resources.
- **Distributed solving:** Large problems are delegated to dedicated high-performance servers.

### Callback-Aware Remote Execution


### Result Serialization Protocol

cxf_wait_async uses a structured, self-describing message format for result delivery:

1. **Message header:** A type identifier and entry count allow the receiver to identify and parse the message.
2. **Typed entries:** Each result entry includes a type code, element count, and data payload. This self-describing format allows the protocol to be extended with new result types without breaking backward compatibility.
3. **Network byte order:** All multi-byte integer values are converted to network byte order (big-endian) before transmission, following standard network protocol conventions (see Stevens, *UNIX Network Programming*, 1998).

The function's error handling follows a robust pattern: default values are used for any attribute that cannot be retrieved, ensuring that a valid result message is always produced. The only error that terminates the function early is a validation failure on a result entry, which indicates a protocol-level inconsistency.

### Relationship to Solve Entry & Dispatch (P3.24)

The three functions in this module are called at different points in the optimization lifecycle managed by P3.24:

| Function | Called By | When |
|----------|-----------|------|
| cxf_prepare_optimization | cxf_optimize (P3.24) | Early in the optimization pipeline, after model validation, to set up interrupt handling |
| cxf_wait_async | cxf_optimize_internal (P3.24) | On the server side, after optimization completes, to send results back to the client |

### Error Handling Patterns

The three functions employ different error handling strategies appropriate to their roles:

- **cxf_prepare_optimization:** Silent failure. If the solve lock cannot be acquired or silent mode is enabled, the function returns without error. The optimization proceeds without signal handler support. This is appropriate because signal handling is a convenience feature, not a correctness requirement.


- **cxf_wait_async:** Defensive defaults with cleanup guarantee. Attribute retrieval failures use safe default values to produce a valid result message. The only hard failure is a result validation error. In all cases, the synchronization primitive is released and the model state is restored, preventing resource leaks and state corruption.

---

## Verification Checklist

```
[x] No hex addresses or offsets
[x] No Ghidra artifacts (param_, local_, iVar, DAT_, FUN_)
[x] No binary-specific constants or magic numbers
[x] No copied code fragments
[x] All descriptions are behavioral, not implementational
[x] All data structures described semantically using Layer 1 types
[x] Explicit cross-references to P1.01, P1.02, P3.09, P3.11, P3.24, P3.34
[x] Passes the Clean Room Test
```

## References

- Stevens, W.R. (1998). *UNIX Network Programming, Volume 1: Networking APIs*. Prentice Hall. (Network byte order conventions.)
- ConvexFeld Optimization, LLC. *ConvexFeld Optimizer Reference Manual* (public API documentation). cxf_optimize, remote solver deployment, asynchronous optimization, callback interface.
- IEEE Std 1003.1 (POSIX). Signal handling: signal(), SIGINT, signal handler registration and restoration patterns.
