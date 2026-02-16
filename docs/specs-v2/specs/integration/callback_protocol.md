# Callback Protocol

## Overview

This document specifies how the user callback system integrates with the solver's optimization pipeline. It describes the complete lifecycle of callback state from registration through invocation to cleanup, maps each callback invocation point to its triggering module and available data, defines the mechanisms by which callbacks influence solver behavior, and documents how the callback protocol adapts to different execution contexts (local, asynchronous, remote solver, concurrent).

A callback is a user-provided function that the solver invokes at defined points during optimization to report progress and allow the user to inspect solver state, request early termination, inject solutions, or add constraints. The callback protocol is the integration layer that connects this user-facing feature to the internal solver modules, ensuring correct synchronization, data availability, and control flow across all solver execution paths.

This specification is intended for reimplementation. It describes behavioral contracts and data flow, not implementation details.

## Components Involved

The callback protocol spans the following modules and data structures:

**Data Structures (Layer 1):**
- P1.01 (Environment) -- holds the log callback function pointer, log callback user data, session reference, and the pointer to the CallbackState
- P1.07 (CallbackState) -- runtime callback management: mutex, timing, validation sentinels, user data, enabled flag, parent link
- P1.02 (Model) -- holds the callback count, callback data pointer, modification-blocked flag, and self-reference for stable callback access

**Module Contracts (Layer 3):**
- P3.13 (Callbacks) -- callback infrastructure: mutex initialization, lifecycle hooks, termination signaling, constraint retrieval, callback propagation
- P3.24 (Solve Entry & Dispatch) -- three execution paths (normal, with-callbacks, no-callbacks) and callback path selection
- P3.25 (Solve LP Core) -- callback invocation points within the simplex iteration loop and solver dispatch
- P3.20 (Simplex Iteration) -- progress logging callback during simplex iterations
- P3.21 (Simplex Phases) -- phase transition points where callbacks may be relevant
- P3.26 (Solve Barrier & Concurrent) -- callback context management for barrier, concurrent, and distributed concurrent solves
- P3.32 (Optimization Preparation) -- remote solver callback channel setup and signal-based interrupt handling

## Flow Description

### 1. Callback Registration

Before any optimization call, the user registers a callback function on the model or environment through the public API. Registration triggers the following sequence:

1. **CallbackState lazy allocation.** If the Environment does not yet have a CallbackState, one is allocated using zero-initialized memory. The structure is initialized with validation sentinels, a mutex (via cxf_init_callback_struct, P3.13), wall-clock timestamps, the enabled flag set to true, and sentinel guard values.

2. **Function pointer and user data storage.** The callback function pointer is stored on the Model (optimization callback) or Environment (log callback). The user data pointer is stored in the CallbackState. The Model's callback count is incremented.

3. **No immediate solver interaction.** Registration is purely a configuration operation. No solver state is modified. The callback takes effect at the next optimization call.

### 2. Optimization Entry and Path Selection

When cxf_optimize (P3.24) is called, the presence of callbacks determines the execution path:

```
cxf_optimize
  |
  v
cxf_optimize_internal
  |
  +-- callback count > 0 AND async mode?  --> cxf_solve_with_callbacks
  |
  +-- callback count == 0 AND async mode? --> cxf_solve_no_callbacks
  |
  +-- otherwise                           --> Normal synchronous path
                                               (callbacks invoked inline)
```

**Path 1: Normal synchronous path.** The solver runs on the calling thread. Callbacks are invoked inline by the solver at each invocation point. The CallbackState mutex serializes invocations. This is the simplest and most common path.

**Path 2: Callback-enabled path (cxf_solve_with_callbacks, P3.24).** Used for remote solver and asynchronous execution with callbacks. This path:
1. Acquires the callback synchronization lock for exclusive channel access.
2. Validates the model state.
3. Locks the callback communication channel.
4. Sets up callback state for the session.
5. Sends an initialization message through the channel.
6. Dispatches the solve (synchronously or asynchronously).
7. Processes callback results, including error propagation.

**Path 3: No-callback fast path (cxf_solve_no_callbacks, P3.24).** Used for asynchronous optimization without user callbacks. Even on this path, the callback data structure is initialized on the environment for internal progress tracking. A state tracker is allocated to cache attribute indices for efficient progress monitoring across the thread boundary.

### 3. Lifecycle Hooks (Not User Callbacks)

Before and after optimization, the system invokes lifecycle hooks that share the "callback" name but are NOT user callbacks:

1. **cxf_pre_optimize_callback (P3.13):** Called at the start of cxf_optimize. Sets the error buffer lock on the environment to preserve the first error message throughout the solve. Does not interact with CallbackState or invoke user code.

2. **cxf_post_optimize_callback (P3.13):** Called at the end of cxf_optimize, on all exit paths. Clears the error buffer lock, restoring normal error reporting. Does not interact with CallbackState or invoke user code.

These hooks implement a first-error preservation pattern: in cascading error scenarios, the user sees the root cause rather than a secondary symptom.

### 4. Callback Invocation During Solving

During optimization, the solver invokes the user callback at defined points. Each invocation follows the same protocol:

1. **Mutex acquisition.** The CallbackState mutex is acquired to serialize callback invocations across threads.
2. **Enabled check.** If the CallbackState enabled flag is false, the callback is skipped.
3. **Context preparation.** The "where" code (indicating the solver phase), the CallbackState reference, and the user data pointer are assembled.
4. **User function call.** The registered callback function is invoked with the prepared context.
5. **Timing update.** The elapsed time of the callback execution is added to the CallbackState's cumulative time. The invocation count is incremented.
6. **Mutex release.** The CallbackState mutex is released.

The user callback function receives a "where" code that identifies the solver phase. Within the callback, the user can:
- Query solver state via cxf_cbget (passing a "what" code to retrieve specific data)
- Request termination via cxf_terminate (which calls cxf_callback_terminate, P3.13)

### 5. Callback Invocation Points

The following table maps each callback event type to the module and function that triggers it, and the solver data available to the user callback at that point.

| Event Type | Triggering Module | Triggering Function(s) | Available Data |
|------------|-------------------|----------------------|----------------|
| POLLING | P3.25 (Solve LP Core) | cxf_solver_dispatch, cxf_solve_lp | Elapsed runtime |
| PRESOLVE | P3.25 (Solve LP Core) | cxf_solver_dispatch (during presolve phase) | Rows removed, columns removed, elapsed time |
| SIMPLEX | P3.20 (Simplex Iteration) | cxf_simplex_iterate | Iteration count, objective value, primal/dual infeasibility, elapsed time, simplex phase (primal/dual) |
| BARRIER | P3.26 (Solve Barrier & Concurrent) | Barrier iteration loop (internal) | Iteration count, primal objective, dual objective, primal infeasibility, dual infeasibility, complementarity |
| MESSAGE | P3.10 (Logging) | Log output functions | The log message string |

**Detailed invocation context by solver phase:**

**Simplex callbacks (SIMPLEX):** Invoked by cxf_simplex_iterate (P3.20) once per iteration batch. This function is called within the two-level iteration loop of cxf_solve_lp (P3.25) and reports progress regardless of whether console logging is enabled. The callback receives the current iteration count, objective value, and infeasibility measures. The callback is invoked even when console output is suppressed, ensuring that external monitoring systems (GUI progress bars, distributed managers) receive regular heartbeat notifications.

**Barrier callbacks (BARRIER):** Invoked during each iteration of the interior-point method. The callback receives the barrier iteration count, primal and dual objective values, and convergence measures (primal infeasibility, dual infeasibility, complementarity gap).

**Presolve callbacks (PRESOLVE):** Invoked during the presolve phase when the progress reporting function detects sufficient elapsed time since the last report. Reports the number of rows and columns removed so far.

**Polling callbacks (POLLING):** Invoked periodically during long operations to provide heartbeat-style notifications. These allow the user to check elapsed time and request termination even during phases that do not produce frequent progress events.

**Message callbacks (MESSAGE):** Invoked whenever the solver generates a log message. The callback receives the message string. This is distinct from the optimization callback -- it is driven by the log callback function pointer stored on the Environment.

### 6. Callback Actions and Solver Response

Within a callback, the user can take several actions that influence solver behavior. Each action has a defined mechanism for propagating back into the solver:

#### 6.1 Termination Request

**Mechanism:** The user calls cxf_terminate, which invokes cxf_callback_terminate (P3.13).

**Local path:** The function accesses the environment's asynchronous state structure and sets the termination flag. This flag is polled by the solver's main iteration loop at each iteration boundary (checked in cxf_simplex_post_iterate, P3.20, and at equivalent checkpoints in the barrier solver). When the flag is detected, the solver exits gracefully with an INTERRUPTED status.

**Remote path (remote solver):** The function acquires the remote solver lock and sends a termination request message through the communication channel. The remote solver terminates at its next iteration boundary.

The termination flag is deliberately kept separate from the CallbackState to avoid requiring the solver's main loop to acquire the callback mutex at every iteration. The flag write is atomic with respect to the solver's read, providing efficient synchronization without locking overhead.

#### 6.2 Data Query (All Callback Points)

**Mechanism:** The user calls cxf_cbget with a "what" code to retrieve specific solver state.

**Data availability:** The available data depends on the "where" code (callback event type). Requesting data that is not available for the current event type returns an error. The data is read-only from the user's perspective; the callback cannot modify solver state through cxf_cbget.

### 7. Callback State Lifecycle

The CallbackState (P1.07) follows a defined lifecycle that spans the optimization session:

```
[Environment created]
        |
        v
   (no CallbackState)  ----[user registers callback]----> [CallbackState allocated]
        |                                                         |
        |                                     +-------------------+
        |                                     |
        v                                     v
[cxf_optimize called]                  [CallbackState initialized]
        |                              - validation sentinels set
        |                              - mutex allocated
        |                              - timestamps recorded
        |                              - enabled = true
        |                              - counters zeroed
        |                                     |
        v                                     v
[solve dispatched]                     [CallbackState active]
        |                              - mutex protects invocations
        |                              - counters accumulate
        |                              - timing tracks overhead
        |                                     |
        v                                     v
[solve completes]                      [CallbackState persists]
        |                              - statistics available
        |                              - ready for next solve
        |                                     |
        v                                     v
[cxf_optimize returns]                 [Statistics logged]
  - logs callback count               - invocation count
  - logs cumulative time               - cumulative time in callbacks
        |
        v
[Environment freed]
        |
        v
[CallbackState destroyed]
  - mutex destroyed
  - validation tags cleared
  - memory freed
```

**Key lifecycle properties:**

- **Lazy allocation:** The CallbackState is allocated on first callback registration, not on Environment creation. Many solver sessions never use callbacks, so this avoids unnecessary overhead.
- **Persistence across solves:** The CallbackState persists between optimization calls. Timing statistics accumulate across multiple cxf_optimize invocations on the same model.
- **Shared across callback types:** A single CallbackState instance serves both log callbacks and optimization callbacks. They share synchronization and timing infrastructure.
- **Inheritance for child environments:** When a child environment or model clone is created (via cxf_copy_env_callbacks, P3.13), the child receives a new CallbackState that inherits the user data, suppress flag, timestamps, and configuration from the parent. The child's parentCallbackState pointer links back to the source.

## State Transitions

### Model State During Callback-Aware Optimization

| State | Entry Condition | Properties |
|-------|----------------|------------|
| **Pre-optimization** | cxf_optimize called, before dispatch | Modification-blocked flag set. Status code cleared. Error buffer locked. |
| **Path selection** | cxf_optimize_internal evaluates callback count | If callback count > 0 and async: enters callback path. Otherwise: normal path. |
| **Callback channel setup** | cxf_solve_with_callbacks acquires lock | Callback synchronization lock held. Channel locked. Init message sent. |
| **Solving with callbacks** | Solver dispatched | Callbacks invoked at defined points. Mutex serializes invocations. Termination flag polled. |
| **Callback result processing** | Solver returns to cxf_solve_with_callbacks | Channel unlocked. Result structure examined for errors. |
| **Post-optimization** | cxf_optimize cleanup | Error buffer unlocked. Modification-blocked flag cleared. Callback statistics logged. Locale restored. |

### CallbackState Transitions During a Single Solve

| Phase | CallbackState Activity |
|-------|----------------------|
| Registration | User data and suppress flag stored. Primary model reference set. |
| Solve start | Mutex begins serializing invocations. Counters resume accumulating. |
| Callback invocation | Mutex acquired -> user function called -> timing updated -> mutex released. |
| Termination request | Termination flag set on async state (separate from CallbackState). |
| Solve end | Statistics available. CallbackState remains valid for next solve. |

## Error Handling

### Error Propagation from Callbacks

Errors during callback processing propagate through several layers:

1. **Within the user callback:** If the user callback returns a non-zero value, the solver treats this as a termination request with an error condition. The solve terminates at the next iteration boundary.

2. **cxf_callback_terminate errors (P3.13):** On the local path, termination flag setting cannot fail. On the remote path, remote solver communication failures return an error code from the message send operation.

3. **cxf_getconstrs_callback errors (P3.13):** Out-of-memory on the remote server and communication failures are detected and reported through the environment's error system. On communication failure, the function enters a polling recovery loop, waiting for the remote optimization to complete before retrieving detailed error information.

4. **cxf_solve_with_callbacks error processing (P3.24):** After the solve completes, the callback result structure is examined:
   - Out-of-memory errors propagate immediately.
   - User interrupt errors are reported through the error system.
   - Other errors trigger a recovery sequence: wait for the callback thread, request detailed error info from the server, report the error with the detailed message.
   - Secondary errors (present in the callback result but not the primary code) are also propagated.

### Error Buffer Locking

The error buffer locking mechanism (cxf_pre_optimize_callback / cxf_post_optimize_callback, P3.13) ensures that during optimization, the first error message is preserved even when cascading errors occur. This is critical for callback-intensive solves where multiple callback invocations might generate error messages. The lock prevents secondary error messages from overwriting the root-cause message while still allowing error codes to be updated.

## Configuration

### Parameters Affecting Callback Behavior

| Parameter | Effect on Callbacks |
|-----------|-------------------|
| OutputFlag | Controls whether console logging occurs, but does NOT affect callback invocation. Callbacks are always invoked regardless of OutputFlag. (P3.20: "the external logging callback is always invoked, regardless of whether a message was printed") |
| Threads | Affects the frequency of progress callbacks in simplex. Time-based throttling normalizes by thread count (P3.20). |
| Method | Determines which solver runs and therefore which callback event types are generated (simplex -> SIMPLEX; barrier -> BARRIER). |
| TimeLimit / IterationLimit | Termination conditions checked at cxf_simplex_post_iterate (P3.20). These interact with callbacks because the termination check occurs at the same iteration boundaries where callbacks are invoked. |

### Callback Configuration on the CallbackState

| Field | Role |
|-------|------|
| userData | Opaque pointer passed to every callback invocation. Set during registration. |
| enabled | Master switch. When false, all callback invocations are skipped without unregistering. |
| suppressStatisticsLog | When true, the callback statistics (invocation count, cumulative time) are not logged at solve completion. Useful for benchmarking. |
| mutex | Serializes all callback invocations. Allocated during first registration. |

## Design Decisions

### D1: Separated Termination Mechanism

Termination signaling is intentionally decoupled from the CallbackState. The solver's main iteration loop checks a simple flag in the environment's asynchronous state at each iteration boundary. If termination were gated on the callback mutex, the solver would need to acquire the mutex at every iteration just to check the flag, serializing the solver's main loop with callback invocations. The separated design makes the termination check effectively free from a synchronization perspective (Butenhof, 1997, on lightweight signaling patterns).

### D2: Mutex-Based Serialization

Callback invocations are serialized through a mutex rather than lock-free techniques. This is appropriate because:
- Callback invocations are relatively infrequent (typically once per iteration batch or less).
- The critical section encompasses the entire user callback execution, which may be arbitrarily long.
- A mutex simplifies correctness reasoning in the presence of multiple worker threads during concurrent or barrier solves.

### D3: Lazy CallbackState Allocation

The CallbackState is allocated on-demand when the first callback is registered, not when the Environment is created. This follows the zero-overhead principle: environments that never use callbacks incur no memory or initialization cost for the callback infrastructure. This pattern is appropriate because a substantial fraction of solver usage never involves callbacks.

### D4: Shared State for Log and Optimization Callbacks

A single CallbackState instance serves both log callbacks and optimization callbacks. The function pointers for each callback type reside in the Environment (log callback) and Model (optimization callback), but they share the CallbackState's synchronization and timing infrastructure. This avoids duplicating the mutex and timing overhead and ensures that cumulative timing statistics reflect total callback overhead.

### D5: Callback Invocation Regardless of Output Suppression

The callback system invokes user callbacks even when console output is suppressed (OutputFlag = 0). This ensures that external monitoring systems (GUI progress bars, distributed computing coordinators) receive progress updates regardless of the console logging configuration. The separation between logging output and callback invocation is a deliberate design choice (P3.20).

### D6: Callback Propagation for Model Clones

When models are cloned (for concurrent solving, multi-scenario, or multi-objective), callback configuration must be propagated to the clone so that the user continues to receive progress events. cxf_copy_env_callbacks (P3.13) handles this propagation, creating a new CallbackState for the child environment that inherits configuration from the parent. The child's parentCallbackState link enables shared timing baselines while maintaining independent invocation statistics.

### D7: Three Execution Paths Based on Callback Presence

The optimization system provides three distinct execution paths (P3.24), and the presence of callbacks is a primary factor in path selection:
- **Normal path:** Callbacks invoked inline during synchronous execution. Simplest path, lowest overhead.
- **Callback path:** Full callback communication infrastructure with channel management. Required for remote solver and for asynchronous execution with callbacks.
- **No-callback fast path:** Lightweight progress tracking via a state tracker. Avoids callback synchronization overhead when no user callbacks are registered.

This three-way split reflects the engineering principle that the common case (no callbacks) should be fast, while the less common cases (callbacks, remote execution) accept additional overhead for their functionality.

## Thread Safety Across Execution Contexts

### Local Synchronous Solving

In the simplest case (single-threaded simplex or barrier on the local machine), the solver and callbacks execute on the same thread. The CallbackState mutex is still acquired to maintain API contract consistency, but there is no contention.

### Concurrent LP Solving

When the concurrent solver (P3.26) creates multiple solver instances on shared-memory threads, each instance operates on an independent model clone with its own environment. Callback configuration is propagated to each clone via cxf_copy_env_callbacks (P3.13). The callback invocations from different solver instances are serialized by the CallbackState mutex (each clone has its own CallbackState and thus its own mutex). The parent model is not modified during concurrent execution; all writes to the parent occur after all workers have been joined.

### Distributed Concurrent Solving

For distributed concurrent LP (cxf_solve_concurrent_distributed, P3.26), worker models execute on remote solvers. A log callback relay is registered on each worker that uses a critical section (mutex) to safely relay worker log messages to the parent environment. User optimization callbacks are not relayed across the distributed boundary; only log callbacks are forwarded.

### Barrier Solver Callbacks

The barrier (interior-point) solver invokes callbacks from its iteration loop. Since barrier iterations are typically faster than simplex iterations and the barrier method may use internal parallelism, the callback invocation frequency is tuned to avoid excessive overhead. The CallbackState mutex ensures that if multiple barrier threads attempt to invoke callbacks simultaneously, they are serialized.

## Callback Statistics and Diagnostics

At the end of each optimization call, cxf_optimize (P3.24) logs callback performance statistics if callbacks were used:

- **Invocation count:** Total number of times the user callback was invoked during this solve.
- **Cumulative time:** Total wall-clock time spent executing user callback code.

These statistics help users diagnose performance issues where excessive callback overhead slows the optimization. The statistics are accumulated in the CallbackState and are available programmatically as well as through the log.

The suppressStatisticsLog flag on the CallbackState can be used to suppress this logging, which is useful when callbacks are used for monitoring but the overhead logging is unwanted.

---

## Verification Checklist

```
[x] No hex addresses or offsets
[x] No Ghidra artifacts (param_, local_, iVar, DAT_, FUN_)
[x] No binary-specific constants or magic numbers
[x] No copied code fragments
[x] All descriptions are behavioral, not implementational
[x] All data structures described semantically using Layer 1 types
[x] Explicit cross-references to P1.01, P1.02, P1.07, P3.10, P3.13, P3.20, P3.24, P3.25, P3.26, P3.32
[x] Passes the Clean Room Test: could be written without seeing the binary
```

## References

- Butenhof, D.R. (1997). *Programming with POSIX Threads*. Addison-Wesley. (Mutex design patterns for callback synchronization.)
- ConvexFeld Optimization, LLC. *ConvexFeld Optimizer Reference Manual* (public API documentation). Callback types, callback codes, cxf_setcallbackfunc, cxf_cbget, cxf_terminate.
- McConnell, S. (2004). *Code Complete*, 2nd edition. Microsoft Press. Chapter 24: Defensive Programming. (Sentinel-based validation patterns.)
