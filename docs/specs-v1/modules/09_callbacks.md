# Module: Callbacks

**Spec Version:** 1.0
**Last Updated:** 2026-01-25
**Functions:** 7

## 1. Overview

### 1.1 Purpose

The Callbacks module provides infrastructure for user-defined monitoring and control of the optimization process through callback functions. It manages the lifecycle of callback state, invokes user-provided functions at strategic points during optimization, and implements a flexible termination signaling mechanism that coordinates graceful shutdown across all solver components.

This module serves as the extensibility layer for Convexfeld's optimization engine, allowing users to inject custom logic for progress monitoring, early termination, heuristic generation, and result logging without modifying core solver code.

### 1.2 Responsibilities

This module is responsible for:

- [ ] Allocating and initializing callback state structures (848 bytes)
- [ ] Managing callback function registration and deregistration
- [ ] Invoking user callbacks at pre-optimization and post-optimization points
- [ ] Tracking callback invocation statistics (call count, cumulative time)
- [ ] Implementing multi-level termination flag signaling
- [ ] Checking termination status during optimization loops
- [ ] Resetting callback state between optimization runs

This module is NOT responsible for:

- [ ] Determining which optimization algorithm to use
- [ ] Managing solver-specific state (basis, pricing, simplex iterations)
- [ ] Enforcing parameter constraints
- [ ] Handling memory allocation for user callback data

### 1.3 Design Philosophy

The callback system follows a non-intrusive monitoring pattern: callbacks observe and influence optimization without directly accessing solver internals. The design prioritizes performance in the common case (callbacks disabled or rarely invoked) while providing rich extensibility when needed. State management uses persistent allocation with reset-on-reuse rather than allocate-on-demand to avoid overhead in repeated solve scenarios.

Termination signaling uses a layered flag approach to ensure detection regardless of execution context (single-threaded, multi-threaded, concurrent, or remote optimization).

## 2. Public Interface

### 2.1 Exported Functions

| Function | Purpose | Typical Caller |
|----------|---------|----------------|
| cxf_setcallbackfunc | Register user callback | User code (public API) |
| cxf_init_callback_struct | Initialize 48-byte substructure | Internal callback setup |
| cxf_pre_optimize_callback | Invoke pre-optimization callback | Optimization dispatcher |
| cxf_post_optimize_callback | Invoke post-optimization callback | Optimization dispatcher |
| cxf_set_terminate | Set basic termination flags | Internal limit checkers, error handlers |
| cxf_callback_terminate | Set callback-aware termination flags | cxf_terminate (public API) |
| cxf_check_terminate | Check if termination requested | Optimization loops (simplex, barrier, MIP) |

### 2.2 Exported Types

| Type | Purpose |
|------|---------|
| CallbackState | 848-byte structure tracking callback configuration and statistics |
| CallbackFunction | User-provided callback signature: `int (*)(CxfModel*, void*, int, void*)` |

### 2.3 Exported Constants

| Constant | Value | Purpose |
|----------|-------|---------|
| CALLBACK_MAGIC1 | 0xCA11BAC7 | Validation marker for CallbackState |
| CALLBACK_MAGIC2 | 0xF1E1D5AFE7E57A7E | Secondary validation marker |
| CALLBACK_STATE_SIZE | 848 bytes | Size of CallbackState structure |

## 3. Internal Functions

### 3.1 Private Functions

| Function | Purpose |
|----------|---------|
| cxf_init_callback_struct | Zero-initialize 48-byte callback substructure |
| cxf_reset_callback_state | Reset statistics without freeing state |

### 3.2 Helper Functions

| Function | Purpose | Used By |
|----------|---------|---------|
| cxf_get_timestamp | High-resolution timing | Pre/post callback wrappers |

## 4. State Management

### 4.1 Module State

| State Element | Type | Lifetime | Thread Safety |
|--------------|------|----------|---------------|
| CallbackState* | Heap | Allocated on first callback registration, never freed | Unsafe - single threaded |
| enableFlag | int32 | Set/cleared when callback registered/unregistered | Unsafe - write serialized |
| invocationCount | double | Incremented per callback invocation | Unsafe - optimization thread only |
| cumulativeTime | double | Accumulated across invocations | Unsafe - optimization thread only |
| terminationFlag | int | Set by termination request | Safe - atomic writes |

### 4.2 State Lifecycle

```
NOT_INITIALIZED (env->callbackState = NULL)
    ↓
    cxf_setcallbackfunc(non-NULL callback)
    ↓
ALLOCATED (848 bytes, magic numbers set)
    ↓
    cxf_setcallbackfunc(NULL) OR enable flag = 0
    ↓
DISABLED (structure persists, enable flag = 0)
    ↓
    cxf_setcallbackfunc(non-NULL callback)
    ↓
ENABLED (enable flag = 1, ready for invocation)
    ↓
    cxf_optimize / cxf_reset_callback_state
    ↓
RESET (statistics cleared, configuration preserved)
```

State never transitions back to NOT_INITIALIZED (no deallocation).

### 4.3 State Invariants

At all times, the following must be true:

- [ ] If CallbackState exists, both magic numbers are valid (0xCA11BAC7 and 0xF1E1D5AFE7E57A7E)
- [ ] If enableFlag is 1, callback function pointer is non-NULL
- [ ] invocationCount and cumulativeTime are non-negative
- [ ] Timestamps are monotonically increasing within a single optimization run
- [ ] Termination flags are never decremented during optimization (monotonic 0→1)

## 5. Dependencies

### 5.1 Required Modules

| Module | What We Use | Why |
|--------|-------------|-----|
| Memory | cxf_calloc, cxf_free | Allocate CallbackState |
| Timing | cxf_get_timestamp | Track callback execution time |
| Validation | Model/environment validation | Ensure safe callback invocation |
| Error/Logging | Error code reporting | Signal callback errors |

### 5.2 Initialization Order

This module must be initialized:
- **After:** Memory, Timing, Validation modules (dependencies)
- **Before:** Optimization module (uses callbacks during solve)

### 5.3 External Dependencies

| Dependency | Type | Purpose |
|------------|------|---------|
| User callback function | User code | Domain-specific logic injected at runtime |
| High-resolution timer | System | Nanosecond/microsecond precision timing |

## 6. Dependents

### 6.1 Modules That Depend on Us

| Module | What They Use | Stability |
|--------|---------------|-----------|
| Optimization | Pre/post callbacks, termination checking | Stable - core integration points |
| API | cxf_setcallbackfunc | Stable - public API contract |
| Simplex | cxf_check_terminate | Stable - termination polling |
| Barrier | cxf_check_terminate | Stable - termination polling |
| MIP | cxf_check_terminate | Stable - termination polling |

### 6.2 API Stability

The following interfaces are stable and must not change:

- [ ] CallbackState structure layout (offsets used by optimization core)
- [ ] Callback function signature (public API contract)
- [ ] Termination flag locations (performance-critical hot paths)
- [ ] cxf_check_terminate return semantics (0=continue, 1=stop)

## 7. Invariants

### 7.1 Module Invariants

What this module guarantees:

- [ ] Callback infrastructure never crashes (defensive NULL checks, safe defaults)
- [ ] Statistics are accurate (timing, invocation counts)
- [ ] Termination requests are eventually detected (within one check interval)
- [ ] User data pointers are preserved exactly as registered
- [ ] No memory leaks from callback operations
- [ ] Callback invocations are strictly serialized (no concurrent user code execution)

### 7.2 Required Invariants

What this module requires from others:

- [ ] User callback functions must not throw exceptions (C ABI assumption)
- [ ] Environment lock held during callback registration/modification
- [ ] Optimization thread does not change during active optimization
- [ ] Termination flags are never reset to 0 during optimization

## 8. Error Handling

### 8.1 Error Detection

How errors are detected in this module:

| Error Type | Detection Method |
|------------|------------------|
| Allocation failure | NULL return from cxf_calloc |
| Invalid magic number | Comparison with expected values |
| NULL callback function | Pointer check before invocation |
| NULL environment/model | Defensive checks at entry points |

### 8.2 Error Propagation

How errors flow through this module:

```
Error Source
    ↓
Error Code (1001=OOM, 1002=NULL, etc.)
    ↓
Early Return (no partial state modification)
    ↓
Caller handles error (logs, cleans up)
```

Callbacks use special handling:
- User callback errors (non-zero return) set termination flag
- Infrastructure errors return error codes to caller

### 8.3 Recovery Strategies

| Error Type | Recovery |
|------------|----------|
| Allocation failure | Return error code, callback remains disabled |
| NULL callback | Silent no-op (callbacks skipped) |
| User callback error | Set termination flag, stop optimization gracefully |
| Termination during callback | Propagate to solver, clean shutdown |

## 9. Thread Safety

### 9.1 Concurrency Model

Single-threaded with respect to callbacks: Only the optimization thread invokes callbacks. Multiple threads may call cxf_check_terminate concurrently (read-only, atomic), and cxf_set_terminate / cxf_callback_terminate may be called from any thread (atomic writes).

### 9.2 Synchronization Primitives

| Primitive | Protects | Granularity |
|-----------|----------|-------------|
| Environment lock | Callback registration/modification | Coarse (entire operation) |
| Atomic int writes | Termination flags | Fine (single word) |

### 9.3 Thread Safety Guarantees

- [ ] cxf_check_terminate is safe for concurrent reads from multiple threads
- [ ] cxf_set_terminate and cxf_callback_terminate are safe from any thread (atomic writes)
- [ ] Callback invocation is strictly single-threaded (optimization thread only)
- [ ] Callback registration must be externally synchronized (environment lock required)
- [ ] Statistics (counts, times) are only modified by optimization thread

### 9.4 Known Race Conditions

None when used correctly. Potential issues if:
- User modifies callback state from callback function (unsupported)
- Multiple threads call cxf_setcallbackfunc without external locking
- Callback function performs non-thread-safe operations

## 10. Performance Characteristics

### 10.1 Complexity Summary

| Operation | Time | Space |
|-----------|------|-------|
| Callback registration (first) | O(1) + allocation | O(1) - 848 bytes |
| Callback registration (subsequent) | O(1) | O(1) - reuse existing |
| Pre/post callback invocation | O(U) | O(1) |
| Termination check | O(1) | O(1) |
| Termination set | O(1) | O(1) |
| State reset | O(1) | O(1) |

Where U = user callback execution time (unbounded, user-defined)

### 10.2 Hot Paths

Performance-critical functions called frequently:

- **cxf_check_terminate:** Called every 10-100 solver iterations (simplex), every iteration (barrier), every node (MIP). Optimized to ~10-20 CPU cycles with favorable branch prediction.
- **cxf_set_terminate:** Called on limit detection (time, iterations, nodes). Optimized to ~20-50 CPU cycles.

Not performance-critical:
- Callback registration (once per model lifecycle)
- Pre/post callback invocations (once per optimization run)

### 10.3 Memory Usage

- CallbackState: 848 bytes (allocated once, never freed)
- Timestamps: 2 × 8 bytes per callback invocation (transient)
- No heap growth during optimization

## 11. Function Index

Complete list of functions in this module with links to individual specs:

### Public Functions

1. [cxf_setcallbackfunc](../functions/callbacks/cxf_setcallbackfunc.md) - Register user callback function

### Internal Functions

1. [cxf_init_callback_struct](../functions/callbacks/cxf_init_callback_struct.md) - Initialize 48-byte callback substructure
2. [cxf_reset_callback_state](../functions/callbacks/cxf_reset_callback_state.md) - Reset callback statistics for new run
3. [cxf_pre_optimize_callback](../functions/callbacks/cxf_pre_optimize_callback.md) - Invoke callback before optimization
4. [cxf_post_optimize_callback](../functions/callbacks/cxf_post_optimize_callback.md) - Invoke callback after optimization
5. [cxf_set_terminate](../functions/callbacks/cxf_set_terminate.md) - Set basic termination flags
6. [cxf_callback_terminate](../functions/callbacks/cxf_callback_terminate.md) - Set callback-aware termination flags
7. [cxf_check_terminate](../functions/callbacks/cxf_check_terminate.md) - Check if termination requested

## 12. Design Decisions

### 12.1 Key Design Choices

| Decision | Rationale | Alternatives Considered |
|----------|-----------|------------------------|
| Persistent allocation (never free) | Avoids allocation overhead in repeated solve scenarios | Allocate-on-enable, free-on-disable (rejected: too much overhead) |
| Multi-level termination flags | Ensures detection in all execution contexts | Single flag (rejected: doesn't cover remote/concurrent cases) |
| Reset instead of reinitialize | Preserves configuration, clears statistics efficiently | Full deallocation/reallocation (rejected: slower) |
| Separate pre/post callbacks | Clear semantic separation, simpler user code | Single callback with mode parameter (rejected: less clear) |
| Void return for internal functions | Simplifies error handling for auxiliary operations | Return error codes (rejected: overkill for simple operations) |

### 12.2 Known Limitations

- [ ] Callbacks are strictly serialized (no parallel user code execution)
- [ ] Callback state is never freed (small permanent memory overhead)
- [ ] User callback exceptions are undefined behavior (C ABI limitation)
- [ ] Termination detection is eventually consistent (not immediate)

### 12.3 Future Improvements

- [ ] Support multiple callback priorities for ordered execution
- [ ] Add callback context stack for nested invocations
- [ ] Provide callback profiling hooks
- [ ] Support asynchronous callbacks (return before completion)
- [ ] Add termination reason codes for diagnostics

## 13. References

- Standard callback patterns in optimization solvers
- x86-64 memory model: Aligned int writes/reads are atomic

## 14. Validation Checklist

Before finalizing this spec:

- [x] All public functions documented
- [x] All dependencies identified
- [x] Thread safety analyzed
- [x] Error handling complete
- [x] No implementation details leaked
- [x] Module boundaries clear

---

*Reviewed by: Pending*

---
*Convexfeld LP Solver Specification*
*Based on published optimization literature*
