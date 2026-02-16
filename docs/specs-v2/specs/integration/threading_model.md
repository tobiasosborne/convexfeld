# Threading Model

## Overview

This document describes the threading and synchronization architecture of the LP solver. The solver follows a **single-threaded API with internal parallelism** design: all public API calls are serialized at the environment level, while internal solver operations may exploit multiple threads for computational work. This architecture simplifies the user-facing concurrency model (users need not worry about thread safety for most operations) while allowing the solver to take advantage of multi-core hardware for performance-critical computations.

Understanding the threading model is essential for reimplementation because it governs how synchronization primitives are used, how thread counts are determined and allocated, how concurrent solver instances coordinate, and what guarantees the solver provides to users about thread safety. Incorrect threading behavior can lead to data corruption, non-deterministic results, or deadlocks.

The threading model addresses five concerns:

1. **API serialization:** Ensuring that concurrent API calls on the same environment do not corrupt shared state.
2. **Locale isolation:** Ensuring that per-thread numeric formatting settings do not interfere across threads.
3. **Thread count management:** Determining how many threads the solver should use, reconciling hardware availability and user preferences.
4. **Internal parallelism:** Distributing computational work across threads within the barrier solver and concurrent solver.
5. **Signal safety:** Ensuring that operating system signal handlers interact correctly with multi-threaded solver operations.

## Components Involved

The threading model spans the following modules and data structures:

| Component | Role in Threading |
|-----------|------------------|
| **P1.01 (Environment)** | Owns the critical section mutex, thread pools, hardware detection state, and thread count parameters |
| **P3.11 (Threading & Synchronization)** | Thread count computation, locale safety, hardware queries, oversubscription warnings |
| **P3.12 (Thread Init & Thunks)** | Per-thread state initialization (RNG), platform mutex abstraction |
| **P3.13 (Callbacks)** | Callback mutex for serializing user callback invocations; callback propagation to child environments |
| **P3.24 (Solve Entry & Dispatch)** | Locale acquisition at API boundary; modification-blocked flag; execution path selection (sync/async/callback) |
| **P3.25 (Solve LP Core)** | Thread-unsafe solver state; concurrent solver dispatch creating independent model copies |
| **P3.26 (Solve Barrier & Concurrent)** | Local concurrent solver with worker threads; distributed concurrent with remote workers; log relay critical section |
| **P3.32 (Optimization Preparation)** | Signal handler installation with solve lock for mutual exclusion |
| **P3.09 (Error Handling)** | Thread-unsafe error buffer; error buffer lock for cascading error preservation |

## Threading Architecture Diagram

```
User Thread(s)
    |
    |  [API calls]
    v
+---------------------------------------------------+
|  PUBLIC API BOUNDARY                              |
|  - Environment critical section serializes access |
|  - Locale saved and switched to "C"              |
|  - Modification-blocked flag set on model        |
+---------------------------------------------------+
    |
    v
+---------------------------------------------------+
|  SOLVE ENTRY (single-threaded per model)          |
|  - cxf_optimize -> cxf_optimize_internal            |
|  - Signal handler installation (with solve lock)  |
|  - Model validation, parameter backup             |
|  - Algorithm selection                            |
+---------------------------------------------------+
    |
    +------------+-------------+
    |            |             |
    v            v             v
 [Simplex]   [Barrier]   [Concurrent]
 single-     internal     N solver
 threaded    thread       instances
             pool for     (independent
             linear       model copies)
             algebra
                |              |
                v              v
           [Worker        [Worker threads
            threads]       or remote
                           workers]
                               |
                               v
                          [First-wins
                           termination,
                           solution
                           aggregation]
    |            |             |
    +------------+-------------+
    |
    v
+---------------------------------------------------+
|  POST-SOLVE (single-threaded)                     |
|  - Solution extraction, status mapping            |
|  - Parameter restore                              |
|  - Locale restored                                |
|  - Modification-blocked flag cleared              |
+---------------------------------------------------+
    |
    v
  Return to User
```

## Lock Hierarchy

The solver uses a small number of synchronization primitives organized in a strict hierarchy to prevent deadlocks. Locks must always be acquired in the order shown (top to bottom); acquiring a lock higher in the hierarchy while holding one lower is prohibited.

```
Level 1: Environment Critical Section (per-environment)
    |
    +-- Protects: reference count, child environment array,
    |   model association, error buffer (when accessed from
    |   multiple threads)
    |
Level 2: Solve Lock (module-level, exclusive)
    |
    +-- Protects: signal handler installation
    |   (only one optimization can own the signal handler)
    |
Level 3: Callback Mutex (per-CallbackState)
    |
    +-- Protects: user callback invocations from concurrent
    |   access; serializes all callback events
    |
Level 4: Thread Pool Mutex (per-environment)
    |
    +-- Protects: thread pool initialization and destruction
    |
Level 5: Log Relay Critical Section (per-concurrent-solve)
        |
        +-- Protects: log message forwarding from distributed
            workers to parent environment
```

**Design note:** The hierarchy is shallow because the solver avoids fine-grained locking in favor of coarse-grained serialization (environment critical section for API access) and isolation (independent model copies for concurrent solving). This reduces the risk of deadlocks and simplifies reasoning about correctness.

## Flow Description

### 1. API Call Serialization

When a user calls a public API function (such as the optimization entry point), the following synchronization sequence occurs:

1. **Model validation.** The model pointer is validated via a sentinel-based structural check. This check is lock-free and operates on immutable data.

2. **Error state clearing.** The error buffer on the environment is cleared to prepare for the new operation. This uses the error buffer lock flag (a lightweight single-thread guard) rather than a mutex. The clearing is skipped if the buffer is locked for nested error preservation.

3. **Locale acquisition.** The calling thread's locale is saved and switched to the standard "C" locale using per-thread locale isolation. This ensures consistent decimal point formatting (period, not comma) throughout the operation without affecting other threads in the application. If the locale is already "C" or the environment is already in an optimization context, this step is a no-op.

4. **State flag setting.** The optimization-active flag on the environment and the modification-blocked flag on the model are set, preventing concurrent API calls from modifying the model during optimization.

5. **Operation execution.** The actual optimization (or other API operation) proceeds.

6. **Cleanup.** The modification-blocked flag is cleared, the locale is restored, and the optimization-active flag is cleared.

The environment's critical section mutex is used when multiple threads may need to access shared environment state (reference counts, child environment arrays, model associations). However, the critical section is NOT acquired for the entire duration of an optimization call. Instead, the modification-blocked flag and the optimization-active flag serve as lightweight guards that prevent conflicting operations without holding a mutex across the potentially long-running solve.

### 2. Thread Count Resolution

Before any parallel operation begins, the solver determines the effective thread count through a resolution chain defined in cxf_get_threads (P3.11). The resolution follows a "most restrictive wins" principle:

**Step 1: Model-level override check.** If a model-level thread override is set (value at least 1), it is used directly, bypassing automatic detection.

**Step 2: Hardware detection (automatic mode).** The solver starts with the logical processor count detected during environment initialization. On systems with many cores, the count is reduced using two heuristics:
- If the logical count exceeds an internal efficiency cap (on the order of 32 threads), the solver prefers the physical core count when it is lower. This accounts for the diminishing returns of simultaneous multithreading (SMT/Hyper-Threading) on compute-intensive workloads.
- The count is clamped to the efficiency cap to prevent excessive thread synchronization overhead.

**Step 3: User parameter application.** The user-configured Threads parameter is read from the environment. If this value is less than the auto-detected count, it takes precedence. A value of zero means "automatic" (no reduction).

**Step 4: Oversubscription warning.** If the resolved thread count exceeds the number of logical processors, a warning is emitted advising the user to reduce the Threads parameter.

The resolution chain is illustrated as follows:

```
Model Override ──(if set)──> base_count
       |
       v (else)
Logical Processors ──(cap for large systems)──> auto_count
       |
       v
min(auto_count, user_Threads_param) ──> effective_threads
```

### 3. Concurrent Optimization Flow

When the solver algorithm dispatch (P3.25) selects a concurrent method, multiple solver instances race in parallel. The threading flow differs between local and distributed concurrent modes.

#### Local Concurrent (shared-memory threads)

1. **Thread allocation.** The total effective thread count is divided evenly among the concurrent instances. For example, with 8 threads and 4 instances, each instance receives 2 threads.

2. **Model cloning.** The source model is cloned once for each concurrent instance, producing fully independent copies with their own matrix data, solver state, and environment. This isolation eliminates the need for fine-grained locking within the solver.

3. **Instance diversification.** Each instance receives a different configuration to explore different portions of the solution space:
   - Per-instance seed offsets for randomized tie-breaking
   - For distributed LP: fixed method assignment (barrier, dual simplex, primal simplex)
4. **Worker thread creation.** Instances beyond the first are dispatched to worker threads. Instance 0 may run on the calling thread's schedule.

5. **Completion polling.** The main thread polls completion flags in a busy-wait loop that transitions from yield-based to sleep-based polling after a threshold number of iterations. The loop also checks for user interrupts.

6. **Winner selection.** The winner is selected based on the determinism mode:
   - Opportunistic: best objective wins (non-deterministic)
   - Deterministic: strict preference ordering ensures reproducible results

7. **Solution aggregation.** All solutions from all instances are collected into the parent model's solution pool. Statistics (iterations, nodes, bounds) are merged.

8. **Cleanup.** All worker threads are joined before any model is freed. Worker models are freed before worker environments. Temporary buffers are freed last.

#### Distributed Concurrent (remote solvers)

The distributed concurrent flow follows the same pattern but replaces worker threads with remote solver workers. Key differences:

- Worker environments are created by connecting to remote solvers
- A critical section protects the log message relay from workers to the parent environment
- Communication with workers uses a structured message protocol (serialization with network byte order)

### 4. Per-Thread State for Parallel Simplex

When the solver uses multiple threads internally (for barrier factorization or concurrent simplex instances), each worker thread needs independent state to avoid data races. The per-thread state model (P3.12) provides this:

- **Worker threads** receive independently allocated random number generator (RNG) state, ensuring deterministic tie-breaking in pivot selection without lock contention. This is essential for reproducibility when the thread count is fixed.

- **The main thread** reuses a process-global shared default RNG state, avoiding unnecessary allocation in single-threaded mode.

The initialization function (cxf_init_thread_local) is called once per thread during thread pool startup, before any concurrent operations begin. The two-mode pattern (independent vs. shared) is a standard design in parallel LP solvers, as described in the literature on parallel simplex methods.

### 5. Signal Handler Thread Safety

The signal-based interrupt mechanism (cxf_prepare_optimization, P3.32) requires special care in a multi-threaded context:

1. **Solve lock for exclusivity.** Only one optimization at a time can own the signal handler. The solve lock (exclusive mode) ensures mutual exclusion for signal handler installation.

2. **Module-level model reference.** Because operating system signal handlers have a fixed calling convention (receiving only the signal number), the active model reference must be stored in a module-level variable accessible to the handler. This creates global state that is inherently not thread-safe for multiple concurrent signal handler installations.

3. **Silent mode bypass.** In embedded or library contexts where the solver should not intercept OS signals, silent mode prevents signal handler installation entirely.

4. **Save/restore pattern.** The previous signal handler is saved on the model during installation and restored by the cleanup function (P3.34) when optimization completes. This ensures the application's normal signal handling is not permanently altered.

The signal handler itself sets a termination flag on the model when an interrupt signal is received. The solver's iteration loop checks this flag at each iteration boundary, providing graceful termination with a valid partial result.

## State Transitions

### Environment Threading State

```
CREATED (no thread resources)
    |
    v  [environment initialization]
INITIALIZED
    - criticalSection: allocated and initialized
    - logicalCoreCount: detected from hardware
    - physicalCoreCount: detected from hardware
    - threadPool1/threadPool2: null (lazy allocation)
    - threadPoolMutex: null (lazy allocation)
    |
    v  [first parallel operation]
THREAD_POOLS_ACTIVE
    - threadPool1: allocated
    - threadPoolMutex: allocated
    - threadPoolInitialized: true
    |
    v  [environment destruction]
DESTROYED
    - Thread pools destroyed
    - threadPoolMutex destroyed
    - criticalSection destroyed
```

### Per-Solve Threading State

```
IDLE (no solve in progress)
    |
    v  [cxf_optimize called]
LOCALE_ACQUIRED
    - Thread locale switched to "C"
    - Modification-blocked flag set
    - Optimization-active flag set
    |
    v  [signal handler installed, if applicable]
SIGNAL_HANDLER_ACTIVE
    - Solve lock held
    - Previous handler saved
    |
    v  [solver dispatch to concurrent method]
CONCURRENT_ACTIVE
    - Model clones created
    - Worker threads running
    - Completion flags being polled
    |
    v  [first solver completes or user interrupts]
COLLECTING_RESULTS
    - All workers terminated
    - All threads joined
    - Solutions being aggregated
    |
    v  [cleanup]
IDLE
    - Worker models freed
    - Signal handler restored
    - Locale restored
    - Flags cleared
```

## Thread Safety Matrix

The following table summarizes the thread safety level of each module and the synchronization required for safe access.

| Module | Thread Safety | Synchronization Required | Notes |
|--------|-------------|-------------------------|-------|
| **P1.01 Environment (core state)** | Conditional | Environment critical section | Reference count, child arrays, model associations require mutex. Error buffer requires caller synchronization. Read-only fields (CPU info, deployment type) are safe after initialization. |
| **P1.01 Environment (parameters)** | Unsafe | Caller serialization | Parameter reads/writes are not internally synchronized. During optimization, parameters are stable (backed up and restored). |
| **P1.02 Model** | Unsafe | Single-owner | Models must not be accessed concurrently. The modification-blocked flag provides a diagnostic guard, not a synchronization mechanism. |
| **P3.09 Error Handling** | Unsafe | Caller must hold environment critical section | Error buffer writes are not synchronized internally. The error buffer lock flag is a single-thread guard for cascading errors, not a multi-thread mechanism. |
| **P3.11 Threading (locale)** | Safe | None (per-thread isolation) | Locale save/restore uses per-thread locale isolation. Each thread operates independently. |
| **P3.11 Threading (hardware queries)** | Safe | None (immutable) | Core counts are set during initialization and never modified. |
| **P3.11 Threading (thread count)** | Safe | None (stable reads) | Reads configuration that is stable during optimization. |
| **P3.11 Threading (oversubscription)** | Conditional | Depends on logging | Emits log messages; thread safety depends on the logging system. |
| **P3.12 Thread Init** | Unsafe | Called once per thread during setup | Per-thread state initialization is not designed for concurrent invocation on the same structure. |
| **P3.12 Mutex Thunk** | Safe | None (platform primitive) | Platform mutex release is inherently thread-safe. |
| **P3.13 Callbacks (mutex init)** | Safe | None (pure allocation) | No shared state; single-caller enforcement at higher level. |
| **P3.13 Callbacks (terminate)** | Conditional | remote solver lock (remote path) | Local: atomic flag write. Remote: acquires CS lock. |
| **P3.13 Callbacks (lifecycle hooks)** | Unsafe | Called from single-threaded entry/cleanup | cxf_pre/post_optimize_callback operate on the error buffer lock flag without synchronization. |
| **P3.13 Callbacks (propagation)** | Unsafe | Called during setup | Modifies callback state before concurrent access begins. |
| **P3.24 Solve Entry** | Conditional | Per-thread locale isolation; single-owner model | Must not be called concurrently on the same model. Locale isolation protects cross-thread formatting. |
| **P3.25 Solve LP Core** | Not thread-safe | Model-level isolation | Each concurrent solve uses an independent model copy. |
| **P3.26 Concurrent (local)** | Internal threading | Independent model clones | Worker threads use independent copies; parent modifications occur only after all workers join. |
| **P3.26 Concurrent (distributed)** | Internal threading | CS for log relay | Workers run on remote servers; CS protects shared log channel. |
| **P3.32 Signal Handler** | Conditional | Solve lock (exclusive) | Only one optimization can install a signal handler at a time. Module-level global state for model reference. |

## Error Handling

### Thread Safety of Error Reporting

Error reporting functions (P3.09) are **not thread-safe**. The error buffer on the environment is a shared resource that is accessed without internal synchronization. This design reflects the solver's single-threaded-API model: under normal usage, only one thread accesses the error buffer at a time.

When multiple threads must access the same environment's error state (e.g., during concurrent solve worker cleanup), the caller must acquire the environment's critical section before calling error-setting functions.

### Error Buffer Lock (Not a Thread Lock)

The error buffer locked flag (P3.13 lifecycle hooks) is a **single-thread cascading-error guard**, not a thread synchronization mechanism. It prevents inner functions from overwriting the root-cause error message during a cascading failure within a single API call. The lock is set by cxf_pre_optimize_hook at optimization start and cleared by cxf_post_optimize_hook at optimization end.

### Errors in Concurrent Solver Instances

Each concurrent solver instance operates on an independent model clone with its own environment. Errors in one instance do not affect other instances' error state. After all instances complete and their threads are joined, errors are aggregated by the winner selection logic:

- If all instances succeed, the best result wins
- If some instances fail (e.g., out-of-memory) and others succeed, the successful results are preferred
- If all instances fail, the first instance's error is propagated
- In deterministic mode, error-vs-success precedence follows a strict ordering to ensure reproducibility

## Configuration

### Parameters Affecting Threading

| Parameter | Purpose | Default | Effect |
|-----------|---------|---------|--------|
| **Threads** | User-requested thread count | 0 (auto) | Zero means auto-detect; positive value caps thread count |
| **ConcurrentMethod** | Selects concurrent solving strategy | -1 (auto) | Controls whether and how concurrent solving is used |
| **ConcurrentJobs** | Number of concurrent solver instances | 0 (auto) | Zero means solver decides based on thread count |
| **Method** | Solver algorithm selection | -1 (auto) | Methods 3, 4, 5 select concurrent LP solving |
| **OutputFlag** | Controls logging verbosity | 1 (enabled) | Affects whether thread count and hardware info are logged |
| **Seed** | Random seed for tie-breaking | 0 | Combined with per-instance offsets for concurrent solver diversification |

### Thread Count Flow: Environment to Solver

```
Environment initialization:
    hardware_detect() -> logicalCoreCount, physicalCoreCount

User configuration:
    cxf_setintparam("Threads", N) -> threadsParameter

At optimization time:
    cxf_get_threads() -> effective_threads
        = min(auto_or_override, threadsParameter)

For concurrent solving:
    effective_threads / concurrent_instance_count -> threads_per_instance
```

### Determinism Control

The solver provides a determinism guarantee: with the same model, parameters, and thread count, the solver produces identical results across runs. This is achieved through:

1. **Deterministic concurrent mode (Method=4).** Thread scheduling is synchronized to ensure reproducible interleaving of concurrent solver instances.

2. **Per-thread RNG state.** Each worker thread has its own random number generator state, initialized from a deterministic seed. Given the same thread count and seed, tie-breaking decisions are identical across runs.

3. **Thread count dependency.** Results may differ when the thread count changes, because different thread counts produce different work partitions and tie-breaking sequences. This is expected behavior documented in the published ConvexFeld API reference.

## Design Decisions

### Single-Threaded API with Internal Parallelism

**Decision:** The public API is single-threaded (one active operation per model at a time), with parallelism handled internally by the solver.

**Rationale:** This design dramatically simplifies the user-facing concurrency model. Users do not need to worry about locking or synchronization for most operations. The solver can still exploit multi-core hardware through internal thread pools for barrier factorization and through the concurrent solver pattern that creates independent model copies. The alternative -- a fully thread-safe API with fine-grained locking -- would add significant complexity and overhead for a use case (concurrent access to the same model) that is rarely needed in practice.

### Model-Level Isolation for Concurrent Solving

**Decision:** Concurrent solver instances operate on fully independent model clones, not on shared data structures with fine-grained locking.

**Rationale:** The simplex method is inherently sequential (each pivot depends on the previous), making it unsuitable for fine-grained parallelization at the operation level. By cloning the entire model for each concurrent instance, the solver avoids all intra-solve locking overhead. The cost is additional memory for the cloned models, but this is typically modest compared to the working memory required by the solver state itself. This approach is standard in portfolio-based algorithm selection systems (Rice, 1976; Xu, Hutter, Hoos, and Leyton-Brown, 2008).

### Locale Isolation via Per-Thread Locale

**Decision:** Locale safety is achieved through per-thread locale switching rather than a global locale lock.

**Rationale:** A global locale lock would serialize all optimization calls across all threads in the process, even for unrelated models on separate environments. Per-thread locale isolation allows multiple threads to use different locales simultaneously, which is essential for applications that embed the solver alongside GUI threads or other components that need their own locale settings. The save/restore pattern ensures that the application's locale is not permanently altered by the solver.

### Lazy Thread Pool Allocation

**Decision:** Thread pools are allocated on first use, not during environment creation.

**Rationale:** Many solver use cases are single-threaded (e.g., small LP models, API calls that do not require optimization). Allocating thread pools eagerly would waste resources for these use cases. Lazy allocation (protected by the thread pool mutex) ensures that thread pool overhead is only incurred when parallel operations are actually needed.

### Module-Level Global State for Signal Handling

**Decision:** The active model reference for the signal handler is stored in a module-level variable.

**Rationale:** Operating system signal handlers have a fixed calling convention that does not allow passing arbitrary context. The module-level variable is the only mechanism for the signal handler to locate the active model. The solve lock ensures mutual exclusion so that at most one model reference is stored at a time. This is a standard pattern for signal handling in C libraries (IEEE POSIX signal handling conventions).

### Physical Core Count Preference for Auto-Detection

**Decision:** On large systems, the auto-detected thread count prefers the physical core count over the logical core count.

**Rationale:** LP solving is a compute-intensive workload that benefits primarily from independent execution units (physical cores), not from simultaneous multithreading (SMT/Hyper-Threading). SMT provides diminishing returns for compute-bound tasks because the two logical cores on the same physical core share execution resources. Using the physical core count as the baseline on large systems avoids the overhead of excessive thread synchronization while still utilizing all available compute capacity. An internal cap further limits the thread count to prevent diminishing returns from thread coordination overhead.

---

## Verification Checklist

```
[x] No hex addresses or offsets
[x] No Ghidra artifacts (param_, local_, iVar, DAT_, FUN_)
[x] No binary-specific constants or magic numbers
[x] No copied code fragments
[x] All descriptions are behavioral, not implementational
[x] All data structures described semantically using Layer 1 types
[x] Explicit cross-references to P1.01, P3.09, P3.11, P3.12, P3.13, P3.24, P3.25, P3.26, P3.32
[x] Passes the Clean Room Test: could be written without seeing the binary
```

## References

- ConvexFeld Optimization, LLC. *ConvexFeld Optimizer Reference Manual* (public API documentation). Thread safety guarantees, Threads parameter, ConcurrentMethod parameter, Method parameter, deterministic mode.
- IEEE Std 1003.1 (POSIX). Signal handling: signal(), SIGINT. Per-thread locale: uselocale(), newlocale().
- Rice, J.R. (1976). "The Algorithm Selection Problem." *Advances in Computers*, 15:65-118.
- Xu, L., Hutter, F., Hoos, H.H., and Leyton-Brown, K. (2008). "SATzilla: Portfolio-based Algorithm Selection for SAT." *Journal of Artificial Intelligence Research*, 32:565-606.
