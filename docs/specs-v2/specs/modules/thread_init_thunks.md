# Module: Thread Init & Thunks

## Purpose

The Thread Init & Thunks module provides two low-level infrastructure functions that support the solver's multithreaded operation. The first function initializes a per-thread state structure used to give each worker thread its own independent random number generator (RNG) state, which is essential for deterministic tie-breaking in parallel simplex solvers. The second function is a platform abstraction layer for releasing a mutual exclusion (mutex) lock, providing a level of indirection between the solver and the operating system's synchronization primitives.

These two functions are grouped together as they are both thin infrastructure primitives that serve the solver's threading subsystem without containing any solver-specific algorithmic logic.

## Per-Thread State Model

Parallel simplex solvers require per-thread state to avoid contention on shared resources. The most critical per-thread resource is a random number generator, which is used for:

1. **Tie-breaking during pivot selection:** When multiple candidate pivots have equal or near-equal ratios, a random perturbation is applied to break ties. Using a shared RNG would introduce lock contention and non-determinism from thread scheduling.

2. **Randomized heuristics:** Various LP solver heuristics benefit from random choices (e.g., random variable selection in crash procedures).

The per-thread state structure contains a small number of pointer-sized fields. The key field holds a pointer to the thread's RNG state. The remaining fields are reserved for additional per-thread resources and are initialized to null.

### Two-Mode Initialization

The module supports two initialization modes for per-thread state:

| Mode | When Used | Behavior |
|------|-----------|----------|
| Independent allocation | Worker threads in a thread pool | Allocates a fresh, independent RNG state for the thread, ensuring no contention with other threads |
| Shared default | Main thread or single-threaded mode | Uses a pre-existing shared default RNG state, avoiding unnecessary allocation when thread isolation is not required |

This two-mode pattern is standard in parallel LP solvers. The main thread, which coordinates worker threads and typically does not perform concurrent pivot operations, can safely use the shared default. Worker threads, which operate in parallel on independent subproblems or partitions, each receive their own RNG state to ensure both correctness (no data races) and reproducibility (deterministic results given the same thread count and seed).

## Functions

### cxf_init_thread_local

**Purpose:** Initialize a per-thread state structure, optionally allocating independent RNG state for worker threads.

**Signature:**
- Input: `thread_state` : pointer-to-ThreadLocalState - The per-thread state structure to initialize
- Input: `allocate_independent` : int - If nonzero, allocate independent state for a worker thread; if zero, use the shared default state
- Output: void

**Preconditions:**
- `thread_state` must point to a valid, writable memory region large enough for the per-thread state structure
- If `allocate_independent` is nonzero, the memory allocator must be available (the solver's memory subsystem must be initialized)

**Postconditions:**
- All reserved fields in the per-thread state structure are set to null
- If `allocate_independent` was nonzero: the RNG state field points to a newly allocated, independent RNG state that is owned by this thread
- If `allocate_independent` was zero: the RNG state field points to the shared default RNG state (a process-global default)

**Side Effects:**
- When `allocate_independent` is nonzero, allocates memory for a new RNG state structure via an internal allocation helper

**Error Conditions:**
- If the RNG state allocation fails (when `allocate_independent` is nonzero), the RNG state field may be null or in an indeterminate state. The function does not return an error code; callers relying on allocated state should verify the field is non-null after initialization.

**Behavioral Description:**
The function initializes a per-thread state structure by clearing all fields to their default values and then setting up the RNG state field based on the requested mode. In independent mode (used for worker threads), it calls an internal allocation function that creates and initializes a fresh RNG state. In shared mode (used for the main thread), it assigns the process-global default RNG state to the field. The distinction ensures worker threads operate without contention on the RNG while the main thread avoids unnecessary allocation overhead.

The initialization sequence sets reserved fields to null both before and after assigning the RNG state field, ensuring the entire structure is in a clean, well-defined state regardless of the initialization mode.

**Thread Safety:** Unsafe. This function is intended to be called once per thread during thread startup, before the thread begins concurrent operations. The caller is responsible for ensuring that `thread_state` is not accessed concurrently during initialization.

**Dependencies:**
- Internal RNG state allocator (creates independent RNG instances for worker threads)

---

### LeaveCriticalSection_thunk

**Purpose:** Release a mutex lock by forwarding to the platform's native mutex release function.

**Signature:**
- Input: `mutex` : pointer-to-Mutex - The mutex (critical section) to release
- Output: void

**Preconditions:**
- `mutex` must point to a valid, initialized mutex object
- The calling thread must currently hold ownership of the mutex (i.e., the calling thread must have previously acquired the mutex)

**Postconditions:**
- The calling thread's ownership of the mutex is released
- If other threads are waiting to acquire the mutex, one of them may be unblocked

**Side Effects:**
- Modifies the mutex's internal ownership state
- May unblock a waiting thread

**Error Conditions:**
- Releasing a mutex that the calling thread does not own results in undefined behavior (platform-dependent)
- Passing a null or invalid mutex pointer results in undefined behavior

**Behavioral Description:**
This is a platform abstraction thunk that provides a single level of indirection between the solver code and the operating system's mutex release primitive. The function simply forwards the call to the platform's native critical section release function. This indirection is a standard pattern in cross-platform software that uses dynamic linking: the thunk allows the solver's compiled code to reference a stable internal symbol, while the actual platform function is resolved at load time through the dynamic linker.

In the solver's threading model, mutexes protect shared environment state during concurrent operations such as model creation, parameter modification, and error reporting. This function is the release counterpart to a corresponding mutex acquisition function.

**Thread Safety:** Safe. The underlying platform mutex release primitive is inherently thread-safe. The function is designed to be called from any thread that currently holds the mutex.

**Dependencies:**
- Platform-native mutex release function (provided by the operating system's threading library)

---

## Module-Level Behavioral Notes

### Relationship to the Broader Threading Subsystem

This module contains two of the lowest-level primitives in the solver's threading infrastructure. Neither function contains solver-specific logic; they are pure infrastructure:

- **cxf_init_thread_local** is called during thread pool initialization, once per worker thread, to set up the per-thread state that higher-level solver routines (pricing, pivot selection, heuristics) will consume.
- **LeaveCriticalSection_thunk** is called throughout the solver wherever shared state must be protected. It pairs with a corresponding acquisition function to bracket critical sections.

Other threading-related functions in the solver (thread count computation, thread pool management, locale management for numeric formatting) reside in separate modules. This module's scope is intentionally narrow: initialization of per-thread state and platform mutex abstraction.

### Per-Thread RNG in Parallel Simplex

The use of independent per-thread RNG state is well-established in parallel LP solver design. The key motivation is that the simplex method is inherently sequential (each pivot depends on the previous), but parallel implementations exploit concurrency in pricing (scanning columns for entering variables) and in concurrent simplex (running multiple independent simplex instances with different starting points or perturbations). In both cases, random tie-breaking must be deterministic per thread to ensure reproducibility when the thread count is fixed.

The two-mode initialization pattern (independent vs. shared) is an optimization: single-threaded execution does not need the overhead of an allocated RNG state, so the main thread reuses a shared default.

### Thread Safety Summary

| Function | Thread Safety | Notes |
|----------|---------------|-------|
| cxf_init_thread_local | Unsafe | Called once per thread during setup; not designed for concurrent invocation on the same state |
| LeaveCriticalSection_thunk | Safe | Platform mutex primitives are inherently thread-safe |

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
