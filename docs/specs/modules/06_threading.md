# Module: Threading

**Spec Version:** 1.0
**Last Updated:** 2026-01-25
**Functions:** 9

## 1. Overview

### 1.1 Purpose

The Threading module provides thread management, synchronization, and CPU topology detection for the Convexfeld optimizer. This module abstracts platform-specific threading primitives, enforces lock hierarchies to prevent deadlocks, manages thread pool configuration based on hardware capabilities, and generates entropy for random number generators. The module enables safe concurrent execution of optimization algorithms while protecting shared data structures from race conditions.

The module serves as the foundation for all parallel operations in Convexfeld, from simple concurrent model manipulation to complex parallel search algorithms in the MIP solver.

### 1.2 Responsibilities

This module is responsible for:

- Detecting physical CPU cores and logical processors for optimal thread pool sizing
- Resolving the Threads parameter (converting 0=auto to actual core count)
- Validating and storing the configured thread count for solver use
- Providing two-level lock hierarchy (environment and solve locks) for deadlock prevention
- Acquiring and releasing critical section locks with recursive support
- Generating pseudo-random seeds combining timestamp, process ID, and thread ID
- Abstracting platform-specific threading APIs (Windows CRITICAL_SECTION)

This module is NOT responsible for:

- Creating or managing thread pools (handled by parallel algorithm modules)
- Work distribution or task scheduling across threads
- Thread affinity or NUMA awareness
- Inter-thread communication beyond mutual exclusion
- Random number generation (only seed generation)
- Parameter storage or retrieval (delegates to Parameter module)

### 1.3 Design Philosophy

The module follows a defensive programming philosophy: locks never fail visibly (NULL pointers handled gracefully), CPU detection uses fallback chains to always return valid values, and thread count validation prevents over-subscription. The two-level lock hierarchy (environment → solve) is strictly enforced to prevent deadlocks while allowing maximum concurrency.

Configuration is lazy: thread count is stored but thread pool creation is deferred until threads are actually needed, avoiding overhead for single-threaded problems.

## 2. Public Interface

### 2.1 Exported Functions

| Function | Purpose | Typical Caller |
|----------|---------|----------------|
| cxf_get_threads | Query configured Threads parameter | Solver initialization |
| cxf_set_thread_count | Set actual thread pool size after resolution | Solver initialization |
| cxf_get_physical_cores | Detect physical CPU cores (auto-detection) | Thread resolution (Threads=0) |
| cxf_get_logical_processors | Detect total logical processors (validation) | Thread count validation |
| cxf_acquire_solve_lock | Acquire solver-state level lock | Simplex pivot, basis update |
| cxf_release_solve_lock | Release solver-state level lock | After pivot completion |
| cxf_env_acquire_lock | Acquire environment level lock | Parameter modification, model creation |
| cxf_leave_critical_section | Release environment level lock | After parameter change |
| cxf_generate_seed | Generate pseudo-random seed | RNG initialization, perturbations |

### 2.2 Exported Types

| Type | Purpose |
|------|---------|
| CRITICAL_SECTION | Platform-specific lock structure (Windows) |
| (No exported types - uses standard platform types) | - |

### 2.3 Exported Constants

| Constant | Value | Purpose |
|----------|-------|---------|
| (No module-level constants) | - | - |

## 3. Internal Functions

### 3.1 Private Functions

All nine functions are public-facing; no purely internal helper functions exist in this module.

### 3.2 Helper Functions

| Function | Purpose | Used By |
|----------|---------|---------|
| GetSystemInfo | Query system processor count | cxf_get_logical_processors |
| GetLogicalProcessorInformation | Query processor topology | cxf_get_physical_cores |
| EnterCriticalSection | Acquire Windows critical section | cxf_acquire_solve_lock, cxf_env_acquire_lock |
| LeaveCriticalSection | Release Windows critical section | cxf_release_solve_lock, cxf_leave_critical_section |
| QueryPerformanceCounter | Get timestamp for seed generation | cxf_generate_seed |

## 4. State Management

### 4.1 Module State

| State Element | Type | Lifetime | Thread Safety |
|--------------|------|----------|---------------|
| env.active_thread_count | int | Environment lifetime | Protected by env lock |
| env.critical_section | CRITICAL_SECTION* | Environment lifetime | Self-protecting |
| state.critical_section | CRITICAL_SECTION* | Solve lifetime | Self-protecting |
| (No module-global state) | - | - | - |

### 4.2 State Lifecycle

```
SYSTEM_BOOT
    ↓
[CPU detection - stateless, query on demand]
    ↓
ENVIRONMENT_CREATED
    ↓
env.critical_section allocated and initialized
    ↓
THREADS_CONFIGURED
    ↓
cxf_get_threads → cxf_set_thread_count
    ↓
env.active_thread_count stored
    ↓
SOLVER_STATE_CREATED
    ↓
state.critical_section allocated and initialized
    ↓
OPTIMIZATION_RUNNING
    ↓
Locks acquired/released as needed
    ↓
OPTIMIZATION_COMPLETE
    ↓
CLEANUP
    ↓
Critical sections destroyed
```

### 4.3 State Invariants

At all times, the following must be true:

- env.active_thread_count >= 1 (minimum single-threaded)
- env.active_thread_count <= cxf_get_logical_processors() (no over-subscription)
- If env.critical_section is non-NULL, it is initialized and valid
- If state.critical_section is non-NULL, it is initialized and valid
- Lock acquisition count must match release count (balanced)
- Environment lock must be acquired before solve lock (hierarchy)

## 5. Dependencies

### 5.1 Required Modules

| Module | What We Use | Why |
|--------|-------------|-----|
| Parameters | cxf_getintparam | Retrieve Threads parameter value |
| Error | Error code definitions | Return standardized errors (1002, 1003) |

### 5.2 Initialization Order

This module must be initialized:
- **After:** Parameters module (provides parameter retrieval)
- **Before:** Solver core, parallel algorithms (consumers of threading services)

### 5.3 External Dependencies

| Dependency | Type | Purpose |
|------------|------|---------|
| GetSystemInfo | Windows API | Query logical processor count |
| GetLogicalProcessorInformation | Windows API | Query physical core count |
| EnterCriticalSection | Windows API | Acquire critical section lock |
| LeaveCriticalSection | Windows API | Release critical section lock |
| QueryPerformanceCounter | Windows API | Get high-resolution timestamp |
| GetCurrentProcessId | Windows API | Get process ID for seed |
| GetCurrentThreadId | Windows API | Get thread ID for seed |

## 6. Dependents

### 6.1 Modules That Depend on Us

| Module | What They Use | Stability |
|--------|---------------|-----------|
| Simplex | Solve lock acquisition/release | Critical |
| Basis | Solve lock for refactorization | Critical |
| Parameters | Environment lock for modifications | Critical |
| API | Environment lock for model creation | Public API - stable |
| MIP | Seed generation for randomization | Stable |
| Concurrent | Thread count for parallel search | Stable |

### 6.2 API Stability

The following interfaces are stable and must not change:

- Lock acquisition/release function signatures
- Two-level lock hierarchy (environment → solve ordering)
- Thread count resolution logic (Threads=0 → physical cores)
- Seed generation non-negative range (0 to 2^31-1)

## 7. Invariants

### 7.1 Module Invariants

What this module guarantees:

- CPU detection always returns at least 1 (never fails)
- Thread count is always validated and capped at logical processor count
- Lock acquisition is recursive-safe (same thread can acquire multiple times)
- Lock release decrements recursion count correctly
- Seed generation produces non-negative values in range [0, 2^31-1]
- NULL pointers in lock functions handled gracefully (no-op)

### 7.2 Required Invariants

What this module requires from others:

- Locks must be initialized before first acquisition attempt
- Lock hierarchy must be respected: env lock before solve lock
- Locks must be released by the same thread that acquired them
- Parameter system must provide valid Threads parameter value

## 8. Error Handling

### 8.1 Error Detection

How errors are detected in this module:

| Error Type | Detection Method |
|------------|------------------|
| NULL environment/state | Explicit NULL checks |
| Invalid thread count | Range validation (< 1) |
| CPU detection failure | Fallback chain prevents failure |
| Lock not held | Undefined behavior (OS may detect) |

### 8.2 Error Propagation

How errors flow through this module:

```
Thread count validation failure
    → cxf_set_thread_count returns error code 1003
    → Caller handles (typically abort initialization)

NULL pointer in lock functions
    → Return immediately (no-op)
    → No visible error (defensive design)

CPU detection API failure
    → Fall back to next detection method
    → Final fallback: return 1
    → Never fails visibly
```

### 8.3 Recovery Strategies

| Error Type | Recovery |
|------------|----------|
| Invalid thread count | Return error code, do not modify state |
| NULL lock pointer | No-op return, continue execution |
| CPU detection failure | Use fallback chain to always succeed |
| Lock hierarchy violation | Undefined behavior (deadlock possible) |

## 9. Thread Safety

### 9.1 Concurrency Model

**Lock functions:** These ARE the thread-safety mechanism - they provide mutual exclusion.
**CPU detection:** Thread-safe (read-only OS queries).
**Thread configuration:** Conditionally safe - must not be called during active optimization.
**Seed generation:** Thread-safe (different thread IDs produce different seeds).

### 9.2 Synchronization Primitives

| Primitive | Protects | Granularity |
|-----------|----------|-------------|
| Environment CRITICAL_SECTION | Parameters, model list, global state | Environment-wide |
| Solve CRITICAL_SECTION | Solver state, basis, working arrays | Per-optimization |

### 9.3 Thread Safety Guarantees

- cxf_get_threads: Thread-safe for reads
- cxf_set_thread_count: NOT thread-safe (must be called before optimization)
- cxf_get_physical_cores: Thread-safe (stateless query)
- cxf_get_logical_processors: Thread-safe (stateless query)
- cxf_acquire/release_solve_lock: Thread-safe (provides mutual exclusion)
- cxf_env_acquire_lock/leave: Thread-safe (provides mutual exclusion)
- cxf_generate_seed: Thread-safe (no shared state)

### 9.4 Known Race Conditions

**Lock hierarchy violation:**
- Thread A: acquire solve lock → acquire env lock (wrong order)
- Thread B: acquire env lock → acquire solve lock (correct order)
- Result: DEADLOCK

Mitigation: Always acquire environment lock first if both are needed.

**Concurrent thread count modification:**
- Thread A: calls cxf_set_thread_count
- Thread B: starts optimization using thread count
- Thread A: modifies thread count mid-optimization
- Result: Undefined behavior (thread pool size mismatch)

Mitigation: Only call cxf_set_thread_count during initialization, not during optimization.

## 10. Performance Characteristics

### 10.1 Complexity Summary

| Operation | Time | Space |
|-----------|------|-------|
| cxf_get_threads | O(1) | O(1) |
| cxf_set_thread_count | O(1) | O(1) |
| cxf_get_physical_cores | O(n) where n = CPU count | O(n) temporary |
| cxf_get_logical_processors | O(1) | O(1) |
| cxf_acquire_solve_lock | O(1) uncontended | O(1) |
| cxf_release_solve_lock | O(1) | O(1) |
| cxf_env_acquire_lock | O(1) uncontended | O(1) |
| cxf_leave_critical_section | O(1) | O(1) |
| cxf_generate_seed | O(1) | O(1) |

### 10.2 Hot Paths

Performance-critical functions called frequently:

1. **cxf_acquire/release_solve_lock** - Called every simplex pivot
   - Uncontended case: 20-50 nanoseconds (very fast)
   - Contended case: Microseconds to milliseconds (blocks until available)
   - Optimization: Spin-then-block strategy (~4000 spin iterations)

2. **cxf_generate_seed** - Called once per optimization or parallel thread
   - Total time: 50-100 nanoseconds
   - Breakdown: 3 OS queries (~30-70ns) + hash mixing (~20-30ns)

Cold paths (called infrequently):
- CPU detection: Once per process lifetime, acceptable ~1ms overhead
- Thread configuration: Once per optimization initialization
- Environment lock: Only for parameter changes and model creation

### 10.3 Memory Usage

Minimal memory footprint:

**Per environment:**
- critical_section: ~40 bytes (Windows CRITICAL_SECTION structure)
- active_thread_count: 4 bytes

**Per solver state:**
- critical_section: ~40 bytes

**CPU detection temporary:**
- Physical core detection: Up to 256 × 48 bytes = ~12 KB temporary buffer
- Logical processor detection: ~48 bytes (SYSTEM_INFO structure)

**Total persistent:** <100 bytes per environment + state
**Peak temporary:** ~12 KB during CPU detection

## 11. Function Index

Complete list of functions in this module with links to individual specs:

### Public Functions

1. [cxf_get_threads](../functions/threading/cxf_get_threads.md) - Query configured Threads parameter
2. [cxf_set_thread_count](../functions/threading/cxf_set_thread_count.md) - Set actual thread pool size
3. [cxf_get_physical_cores](../functions/threading/cxf_get_physical_cores.md) - Detect physical CPU cores
4. [cxf_get_logical_processors](../functions/threading/cxf_get_logical_processors.md) - Detect logical processors
5. [cxf_acquire_solve_lock](../functions/threading/cxf_acquire_solve_lock.md) - Acquire solver-state lock
6. [cxf_release_solve_lock](../functions/threading/cxf_release_solve_lock.md) - Release solver-state lock
7. [cxf_env_acquire_lock](../functions/threading/cxf_env_acquire_lock.md) - Acquire environment lock
8. [cxf_leave_critical_section](../functions/threading/cxf_leave_critical_section.md) - Release environment lock
9. [cxf_generate_seed](../functions/threading/cxf_generate_seed.md) - Generate pseudo-random seed

## 12. Design Decisions

### 12.1 Key Design Choices

| Decision | Rationale | Alternatives Considered |
|----------|-----------|------------------------|
| Two-level lock hierarchy | Allows concurrent optimization of different models | Single global lock (serializes everything) |
| Physical cores for auto-detection | Better performance for compute-intensive algorithms | Logical processors (over-subscription from HT) |
| Recursive critical sections | Simplifies code (nested calls don't deadlock) | Non-recursive (requires tracking) |
| NULL-safe lock functions | Defensive programming (graceful degradation) | Crash on NULL (harsh but detects bugs) |
| Lazy thread pool creation | Avoid overhead for single-threaded problems | Eager creation (simpler but wasteful) |
| Seed combining 3 sources | Uniqueness across time, processes, and threads | Single source (collision prone) |

### 12.2 Known Limitations

- Windows-specific implementation (requires porting for Linux/macOS)
- No lock timeout or deadlock detection (can hang forever)
- No thread affinity or NUMA awareness
- No priority inheritance for locks
- Seed generation assumes non-adversarial environment (not cryptographically secure)
- Physical core detection may be inaccurate on virtual machines

### 12.3 Future Improvements

- Cross-platform lock abstraction (pthread, C++11 std::mutex)
- Lock timeout with deadlock detection and recovery
- Thread affinity hints for NUMA systems
- Read-write locks for parameter access (allow concurrent reads)
- Hardware RNG for seed generation (RDRAND instruction)
- Dynamic thread pool resizing during optimization
- Lock contention profiling and reporting

## 13. References

- Windows API Documentation: Critical Sections, System Information
- POSIX Threads (pthread) specification
- Coffman, E. G., et al. (1971). "System Deadlocks". *ACM Computing Surveys* 3(2): 67–78.
- Herlihy, M., & Shavit, N. (2008). *The Art of Multiprocessor Programming*. Morgan Kaufmann.
- Intel Software Developer's Manual: CPU topology detection
- MurmurHash3: Hash function for seed generation

## 14. Validation Checklist

Before finalizing this spec:

- [x] All public functions documented
- [x] All dependencies identified
- [x] Thread safety analyzed
- [x] Error handling complete
- [x] No implementation details leaked
- [x] Module boundaries clear
- [x] Lock hierarchy enforced
- [x] Performance characteristics documented
- [x] State lifecycle defined
- [x] Invariants specified

---

*Date: 2026-01-25*
*Reviewed by: Pending*

---
*Convexfeld LP Solver Specification*
*Based on published optimization literature*
