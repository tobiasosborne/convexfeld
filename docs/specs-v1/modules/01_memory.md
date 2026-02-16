# Module: Memory Management

**Spec Version:** 1.0
**Last Updated:** 2026-01-25
**Functions:** 11

## 1. Overview

### 1.1 Purpose

The Memory Management module provides the foundational memory allocation infrastructure for the entire Convexfeld optimizer. It implements environment-scoped memory tracking, enabling centralized control over memory usage, leak detection, and bulk cleanup. All dynamic memory allocations in Convexfeld flow through this module, which wraps system allocators with tracking metadata and thread-safety guarantees.

Beyond basic allocation primitives, the module provides specialized allocators optimized for specific usage patterns: arena allocation for temporary eta vectors in simplex iterations, and hierarchical state management for solver working data structures. The module separates lightweight control structures (stack-allocated) from heavyweight working state (heap-allocated) to minimize allocation overhead while maintaining comprehensive resource management.

### 1.2 Responsibilities

This module is responsible for:

- Providing thread-safe memory allocation and deallocation primitives
- Tracking total memory usage per environment for statistics and limit enforcement
- Implementing specialized allocators for performance-critical paths (eta vector arena)
- Managing hierarchical state structures (SolveState, SolverState, BasisState, CallbackState)
- Enabling bulk cleanup of all allocations when environment is destroyed
- Enforcing memory limits configured via MemLimit parameter
- Preventing memory leaks through comprehensive tracking
- Supporting solver state lifecycle (initialization, cleanup, destruction)

This module is NOT responsible for:

- Deciding what to allocate (callers specify requirements)
- Algorithm-specific memory layout (structures defined by other modules)
- Thread scheduling or synchronization policy (only provides primitives)
- Memory defragmentation or compaction
- Cross-environment memory sharing

### 1.3 Design Philosophy

The module follows several key design principles:

1. **Environment-scoped allocation**: Every allocation is associated with a CxfEnv, enabling centralized tracking and bulk cleanup. This prevents memory leaks even in error paths.

2. **Fail-fast validation**: All functions perform defensive NULL checking and return immediately on invalid inputs. Memory operations never crash on NULL pointers.

3. **Separation of concerns**: Lightweight control structures (SolveState) are stack-allocated for speed, while heavyweight working data (SolverState) is heap-allocated for flexibility. This hybrid approach optimizes the common case.

4. **Arena allocation pattern**: Temporary allocations with bulk lifetime (eta vectors) use bump-pointer allocation from pre-allocated chunks. This provides O(1) amortized allocation with minimal overhead.

5. **Hierarchical cleanup**: Free operations proceed from innermost to outermost (arrays before containers). NULL-checking at each level provides robustness against partial initialization.

## 2. Public Interface

### 2.1 Exported Functions

| Function | Purpose | Typical Caller |
|----------|---------|----------------|
| cxf_malloc | Allocate tracked memory block | All modules needing heap memory |
| cxf_calloc | Allocate zero-initialized memory | Structure initialization |
| cxf_realloc | Resize existing allocation | Dynamic array growth |
| cxf_free | Deallocate tracked memory | Cleanup paths throughout codebase |
| cxf_vector_free | Free vector container structure | Matrix operations, sparse vectors |
| cxf_alloc_eta | Allocate from eta vector arena | Simplex basis updates |
| cxf_init_solve_state | Initialize control structure | cxf_optimize, solver entry |
| cxf_cleanup_solve_state | Invalidate control structure | Solver exit, error cleanup |
| cxf_free_solver_state | Free complete solver working state | Model destruction, error recovery |
| cxf_free_basis_state | Free basis snapshot | Warm start cleanup |
| cxf_free_callback_state | Free callback data | Environment destruction |

### 2.2 Exported Types

| Type | Purpose |
|------|---------|
| SolveState | Lightweight solve control (72 bytes, stack-allocated) |
| SolverState | Complete solver working data (MB-GB, heap-allocated) |
| BasisState | Basis snapshot for warm starts |
| CallbackState | Callback registration and statistics |
| VectorContainer | Sparse vector with indices and values |
| EtaBuffer | Arena allocator for eta vectors |

### 2.3 Exported Constants

| Constant | Value | Purpose |
|----------|-------|---------|
| SOLVE_STATE_MAGIC | 0x534f4c56 ("SOLV") | Validation magic for SolveState |
| MAX_CHUNK_SIZE | 65536 (64KB) | Maximum eta buffer chunk size |
| ERROR_BUFFER_SIZE | 512 | Size of environment error buffer |

## 3. Internal Functions

All functions listed in section 2.1 are public. No purely internal helper functions exist in this module - all memory operations are exposed for use by other modules.

## 4. State Management

### 4.1 Module State

| State Element | Type | Lifetime | Thread Safety |
|--------------|------|----------|---------------|
| Environment allocation tracking | CxfEnv fields | Environment creation to destruction | Protected by critical section |
| Total bytes allocated counter | uint64_t | Updated on alloc/free | Protected by critical section |
| Allocation count | uint32_t | Updated on alloc/free | Protected by critical section |
| SolveState structure | Stack-allocated | Function scope (optimize call) | Single-threaded per solve |
| SolverState structure | Heap-allocated | Model lifetime or solve duration | Single-threaded access required |
| EtaBuffer chunk chain | Heap-allocated | Between refactorizations | Single-threaded per thread |

### 4.2 State Lifecycle

```
Environment Lifecycle:
  CREATED (loadenv)
    → tracking structures initialized
    → critical section created
  ACTIVE (models and solves)
    → allocations tracked
    → memory limit enforced
  DESTROYED (freeenv)
    → all tracked allocations freed
    → critical section destroyed

SolverState Lifecycle:
  UNINITIALIZED
    → alloc (cxf_simplex_init)
  INITIALIZED
    → populate arrays
  ACTIVE (during solve)
    → algorithm modifies working data
  CLEANUP (after solve)
    → free (cxf_free_solver_state)
  DESTROYED

SolveState Lifecycle:
  UNINITIALIZED (stack memory)
    → init (cxf_init_solve_state)
  VALID (magic number set)
    → used during solve
  INVALIDATED (cleanup)
    → magic cleared, pointers NULLed
  DESTROYED (stack unwind)
```

### 4.3 State Invariants

At all times, the following must be true:

- Environment total memory counter equals sum of all tracked allocation sizes
- Allocation count equals number of tracked allocations
- All tracked allocations are reachable for cleanup
- SolveState magic number is valid when structure is in use
- EtaBuffer chunk chain is fully linked (no orphaned chunks)
- All pointer fields are either NULL or point to valid memory

## 5. Dependencies

### 5.1 Required Modules

| Module | What We Use | Why |
|--------|-------------|-----|
| System (malloc/free) | Underlying heap allocator | Actual memory allocation |
| System (CRITICAL_SECTION) | Thread synchronization | Protect allocation tracking |
| System (memset) | Memory initialization | Zero-initialization in cxf_calloc |
| Timing | cxf_get_timestamp | Capture solve start time in cxf_init_solve_state |

### 5.2 Initialization Order

This module must be initialized:
- **After:** System libraries (malloc, threading primitives)
- **Before:** All other Convexfeld modules (they depend on memory allocation)

### 5.3 External Dependencies

| Dependency | Type | Purpose |
|------------|------|---------|
| malloc/free | C Standard Library | Heap memory allocation |
| memset | C Standard Library | Zero-initialization |
| EnterCriticalSection | Windows API | Acquire lock |
| LeaveCriticalSection | Windows API | Release lock |

## 6. Dependents

### 6.1 Modules That Depend on Us

| Module | What They Use | Stability |
|--------|---------------|-----------|
| All modules | cxf_malloc, cxf_free, cxf_calloc | Must not change |
| Validation | SolveState, SolverState structures | Must not change |
| Solver Core | cxf_alloc_eta, cxf_free_solver_state | Must not change |
| API | All allocation functions | Must not change |
| Matrix | cxf_vector_free | Must not change |

### 6.2 API Stability

The following interfaces are stable and must not change:

- cxf_malloc, cxf_calloc, cxf_realloc, cxf_free signatures
- SolveState and SolverState structure magic numbers
- Arena allocation interface (cxf_alloc_eta)
- State lifecycle functions (init/cleanup/free)

## 7. Invariants

### 7.1 Module Invariants

What this module guarantees:

- All allocated memory is tracked in environment structures
- Total memory counter accurately reflects sum of allocations
- Memory limits are enforced before allocation completes
- NULL pointers are safe to pass to free functions
- Cleanup functions never fail (void return)
- Thread-safe allocation through critical sections
- No memory leaks when environment is destroyed

### 7.2 Required Invariants

What this module requires from others:

- Callers must not access memory after freeing
- SolverState access must be single-threaded
- Environment pointer must remain valid during allocations
- No pointer arithmetic beyond allocated size
- Proper pairing of init/cleanup for state structures

## 8. Error Handling

### 8.1 Error Detection

How errors are detected in this module:

| Error Type | Detection Method |
|------------|------------------|
| NULL environment | Direct pointer check |
| NULL pointer | Direct pointer check |
| Allocation failure | NULL return from malloc |
| Memory limit exceeded | Compare total + size against limit |
| Invalid magic number | Compare against expected constant |

### 8.2 Error Propagation

How errors flow through this module:

```
Allocation Functions (malloc/calloc/realloc):
  NULL env → return NULL immediately
  Size validation failed → return NULL
  Memory limit exceeded → return NULL
  System allocation failed → return NULL
  (Caller checks NULL and propagates CXF_ERR_OUT_OF_MEMORY)

Free Functions (free/cleanup):
  NULL pointer → no-op, silent return
  Double-free → undefined behavior (caller error)

Init Functions:
  NULL pointer → return error code
  Valid inputs → always succeed
```

### 8.3 Recovery Strategies

| Error Type | Recovery |
|------------|----------|
| Allocation failure | Return NULL, caller decides (retry, fail, reduce capacity) |
| NULL input to free | Silent no-op, safe to call |
| Memory limit exceeded | Return NULL, caller may reduce requirements |
| Init failure | Return error code, caller must not use structure |

## 9. Thread Safety

### 9.1 Concurrency Model

**Model:** Multiple-readers, multiple-writers with lock protection.

Allocation and deallocation functions use environment-scoped critical sections to protect tracking structures. Multiple threads can allocate concurrently on the same environment without corruption, but throughput may be limited by lock contention.

State management functions (init/cleanup/free) have no internal locking - callers must ensure exclusive access to the state structures being manipulated.

### 9.2 Synchronization Primitives

| Primitive | Protects | Granularity |
|-----------|----------|-------------|
| Environment CRITICAL_SECTION | Allocation tracking, memory counters | Per-environment (coarse) |
| (None for state functions) | SolverState, SolveState | Caller responsibility |

### 9.3 Thread Safety Guarantees

- Concurrent allocations on same environment are serialized and safe
- Concurrent allocations on different environments proceed in parallel
- Free operations are thread-safe for different allocations
- State management requires caller synchronization
- Arena allocation (eta buffers) is thread-local by design

### 9.4 Known Race Conditions

No race conditions within this module when used correctly. Potential issues from misuse:
- Concurrent free of same pointer (caller error, double-free)
- Concurrent access to SolverState without locking (caller error)
- Reading memory during concurrent free (caller synchronization issue)

## 10. Performance Characteristics

### 10.1 Complexity Summary

| Operation | Time | Space |
|-----------|------|-------|
| cxf_malloc | O(1) amortized | O(size) |
| cxf_calloc | O(size) | O(size) |
| cxf_realloc | O(min(old, new)) avg | O(new_size) |
| cxf_free | O(1) | O(-size) |
| cxf_alloc_eta (fast path) | O(1) | O(size) |
| cxf_alloc_eta (slow path) | O(1) | O(max(size, chunk_size)) |
| cxf_init_solve_state | O(1) | O(1) |
| cxf_free_solver_state | O(n) eta chains | O(-total) |

### 10.2 Hot Paths

**Critical performance paths:**
- cxf_malloc: Called extremely frequently throughout solver (millions of times)
- cxf_alloc_eta: Called in simplex iteration inner loop (thousands of times per solve)
- cxf_init_solve_state: Called once per optimize (must be fast for repeated solves)

**Optimizations:**
- Lock acquisition overhead dominates for small allocations (~20-50 ns)
- Arena allocation provides 10x speedup for eta vectors vs individual malloc
- Stack-allocated SolveState avoids allocation overhead (~100 ns saved per solve)

### 10.3 Memory Usage

**Typical memory consumption:**
- Tracking overhead: ~24-32 bytes per allocation (header or hash table entry)
- Small problem (1K vars/constraints): ~10-50 MB total
- Medium problem (100K vars): ~500 MB - 2 GB total
- Large problem (1M+ vars): ~5-50 GB total
- Arena overhead: ~5-10% waste in last chunk of each eta buffer

## 11. Function Index

Complete list of functions in this module with links to individual specs:

### Core Allocation

1. [cxf_malloc](functions/memory/cxf_malloc.md) - Primary tracked allocation
2. [cxf_calloc](functions/memory/cxf_calloc.md) - Zero-initialized allocation
3. [cxf_realloc](functions/memory/cxf_realloc.md) - Resize existing allocation
4. [cxf_free](functions/memory/cxf_free.md) - Deallocate tracked memory

### Specialized Allocators

5. [cxf_vector_free](functions/memory/cxf_vector_free.md) - Free sparse vector container
6. [cxf_alloc_eta](functions/memory/cxf_alloc_eta.md) - Arena allocation for eta vectors

### State Management

7. [cxf_init_solve_state](functions/memory/cxf_init_solve_state.md) - Initialize solve control
8. [cxf_cleanup_solve_state](functions/memory/cxf_cleanup_solve_state.md) - Invalidate solve control
9. [cxf_free_solver_state](functions/memory/cxf_free_solver_state.md) - Free complete solver state
10. [cxf_free_basis_state](functions/memory/cxf_free_basis_state.md) - Free basis snapshot
11. [cxf_free_callback_state](functions/memory/cxf_free_callback_state.md) - Free callback state

## 12. Design Decisions

### 12.1 Key Design Choices

| Decision | Rationale | Alternatives Considered |
|----------|-----------|------------------------|
| Environment-scoped tracking | Enables bulk cleanup, leak detection, memory limits | Per-allocation metadata (higher overhead) |
| Arena allocation for eta vectors | 10x performance improvement in hot path | Individual malloc (simpler but slower) |
| Stack vs heap for SolveState | Minimize allocation overhead for frequent operation | Always heap-allocate (slower, simpler) |
| NULL-safe free functions | Simplifies cleanup code, prevents crashes | Require NULL checks by callers (error-prone) |
| Fixed 512-byte error buffer | Avoid allocation in error paths | Dynamic allocation (can fail during errors) |
| Coarse-grained locking | Simpler implementation, adequate performance | Fine-grained per-allocation locks (complex) |

### 12.2 Known Limitations

- Lock contention can limit allocation throughput under heavy concurrency
- Coarse environment lock prevents parallel allocations on same environment
- Fixed chunk sizes in arena allocator may waste memory for unusual patterns
- No memory compaction or defragmentation support
- Error buffer size (512 bytes) limits error message detail

### 12.3 Future Improvements

- Thread-local allocation pools to reduce lock contention
- Lockless techniques for tracking statistics (atomic counters)
- Size-class segregated allocators for common sizes (8, 16, 32, 64 bytes)
- Bulk allocation API to allocate multiple blocks with single lock
- Memory usage profiling and optimization recommendations
- Support for custom allocators (GPU memory, huge pages)

## 13. References

- Standard C memory allocation (malloc, calloc, realloc, free)
- Windows CRITICAL_SECTION documentation
- "The Art of Computer Programming Vol 1" - Dynamic storage allocation
- "Efficient Implementation of the Revised Simplex Method" - Eta vector management
- Linux kernel slab allocator - Arena allocation inspiration
- Lea allocator documentation - System malloc implementation

## 14. Validation Checklist

Before finalizing this spec:

- [x] All public functions documented
- [x] All dependencies identified
- [x] Thread safety analyzed
- [x] Error handling complete
- [x] No implementation details leaked
- [x] Module boundaries clear
- [x] State lifecycle documented
- [x] Performance characteristics specified
- [x] Design rationale provided

---

*Based on: 11 function specifications in cleanroom/specs/functions/memory/*

---
*Convexfeld LP Solver Specification*
*Based on published optimization literature*
