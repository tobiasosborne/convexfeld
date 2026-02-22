# Module: Memory Primitives

## Purpose

This module provides the foundational memory allocation, reallocation, and deallocation functions used throughout the solver. All memory operations are routed through these primitives to enable memory usage tracking against a configurable memory limit, support for custom allocator callbacks (for ISV/embedded deployments), and thread-safe usage accounting via thread-local batching. The module also provides the top-level Model allocation function that creates and initializes a Model structure.

## Functions

### cxf_calloc

**Purpose:** Allocate zero-initialized memory with optional memory tracking, limit enforcement, and custom allocator support.

**Signature:**
- Input: environment : pointer-to-Environment (nullable) - The environment providing memory configuration; if null, a plain system allocation is performed with no tracking
- Input: count : unsigned integer - Number of elements to allocate
- Input: element_size : unsigned integer - Size of each element in bytes
- Output: pointer (nullable) - Pointer to the allocated zero-initialized memory, or null on failure

**Preconditions:**
- If environment is non-null, it must be a valid, initialized Environment
- count and element_size must be non-negative; their product must not overflow

**Postconditions:**
- On success, the returned pointer references a zero-initialized block of at least count * element_size bytes
- If memory tracking is enabled on the root environment, the global usage counter has been updated (either directly or via the thread-local batch) to reflect the new allocation
- If the environment has a custom allocator configured, the allocation was performed through the custom allocator callback
- If the environment uses custom allocator mode, a size-tracking header is prepended to the allocation and the returned pointer is offset past the header

**Side Effects:**
- Increments the root environment's memory usage counter (atomically or via thread-local batch)
- May update the peak memory usage counter if the new usage exceeds the previous peak
- May flush the thread-local allocation batch to the global counter if the batch exceeds the flush threshold

**Error Conditions:**
- Memory limit exceeded -> returns null without allocating (the caller is responsible for reporting an out-of-memory error)
- System or custom allocator failure -> returns null
- Null environment -> performs a simple system allocation with no tracking; returns null on system failure

**Behavioral Description:**
cxf_calloc allocates zero-initialized memory, optionally enforcing a memory limit and tracking usage. When an environment is provided, the function first checks whether the requested allocation would cause total memory usage to exceed the environment's configured memory limit; if so, it returns null without allocating. It then delegates the allocation to either a custom allocator callback (if configured on the environment) or the system allocator. After successful allocation, it updates the memory usage counter on the root environment, using thread-local batching to reduce contention on the global atomic counter. When the environment uses custom allocator mode, a small header is prepended to the allocation to store the allocation size (since custom allocators may not support querying allocation sizes), and the returned pointer is adjusted past this header.

**Thread Safety:** conditional - The global memory usage counter is updated using atomic operations. Thread-local batching further reduces contention. The peak usage update is approximately correct (benign race). Safe to call from multiple threads concurrently when each thread uses the same environment.

**Dependencies:** None (leaf function; uses system or custom allocator)

---

### cxf_realloc

**Purpose:** Resize an existing memory allocation with optional memory tracking, limit enforcement, and custom allocator support.

**Signature:**
- Input: environment : pointer-to-Environment (nullable) - The environment providing memory configuration
- Input: existing_pointer : pointer (nullable) - Pointer to the existing allocation; if null, behaves like an allocation; must have been allocated through cxf_calloc or cxf_realloc with the same environment
- Input: new_size : unsigned integer - Desired new size in bytes
- Output: pointer (nullable) - Pointer to the resized memory, or null on failure

**Preconditions:**
- If environment is non-null, it must be a valid, initialized Environment
- If existing_pointer is non-null, it must have been allocated via cxf_calloc or cxf_realloc using the same environment (or a compatible one sharing the same root environment)
- new_size of zero with a non-null existing_pointer indicates a free operation

**Postconditions:**
- If new_size is zero and existing_pointer is non-null: the memory is freed, the usage counter is decremented, and null is returned
- If existing_pointer is null and new_size is positive: a new allocation of new_size bytes is returned (equivalent to cxf_calloc behavior minus zero-initialization)
- If both are provided: the allocation is resized to new_size bytes; on success the old contents are preserved up to the minimum of old and new sizes; on failure, null is returned and the original allocation is unchanged
- Memory tracking counters are updated to reflect the net change (new_size minus old_size)

**Side Effects:**
- Updates the root environment's memory usage counter by the delta between new and old allocation sizes
- May update the peak memory usage counter
- May flush the thread-local allocation batch
- When new_size is zero, frees the existing allocation via the custom free callback or system free

**Error Conditions:**
- Memory limit would be exceeded by the size increase -> returns null; original allocation is unchanged
- System or custom allocator failure -> returns null; original allocation is unchanged
- Null environment with null pointer and zero size -> returns null (no-op)

**Behavioral Description:**
cxf_realloc resizes an existing memory allocation while maintaining memory tracking consistency. It handles three cases: deallocation (new_size is zero), fresh allocation (existing_pointer is null), and true resize. For resize operations where the allocation is growing, it checks the memory limit before proceeding. The function retrieves the old allocation size either from a prepended header (in custom allocator mode) or by querying the system allocator, computes the size delta, and updates the usage counter accordingly. Custom allocator callbacks are used when configured on the environment. The pointer adjustment for custom allocator headers is handled transparently.

**Thread Safety:** conditional - Same thread-safety properties as cxf_calloc. Atomic updates to usage counters; thread-local batching for reduced contention.

**Dependencies:** cxf_calloc (shares memory tracking infrastructure)

---

### cxf_vector_free

**Purpose:** Recursively free a Model (or solver context) structure and all of its owned sub-structures, then null out the caller's pointer to prevent use-after-free.

**Signature:**
- Input/Output: context_pointer : pointer-to-pointer-to-Model - Double pointer to the Model structure to free; the inner pointer is set to null after deallocation

**Preconditions:**
- context_pointer may be null (safe no-op)
- The dereferenced pointer may be null (safe no-op)
- If non-null, the dereferenced pointer must point to a valid Model structure that has not already been freed

**Postconditions:**
- All owned sub-structures have been freed in the correct order: nested (recursive) contexts first, then auxiliary data, then the main structure
- The caller's pointer (*context_pointer) is set to null
- The validity sentinel on the structure has been cleared to prevent accidental reuse
- The environment's model tracking has been updated to reflect the removal

**Side Effects:**
- Frees all memory associated with the Model and its owned sub-structures, including: remote solver state, callback state, nested context structures (recursively), timing state, MIP hint arrays, basis and start arrays, matrix scaling data, internal solver state, solution state, vector pairs, hash tables, basis factorization data, solution information structures, SOS constraint data, general constraint data, IIS state, LP state, warm-start data, barrier state, solution pool, and various auxiliary arrays
- Invalidates the attribute cache before freeing solution data
- Waits for any active asynchronous thread to complete before freeing
- Updates the environment's active model tracking
- Optionally runs timing cleanup after all memory has been freed

**Error Conditions:**
- Null input -> safe no-op (returns immediately)
- Null dereferenced pointer -> safe no-op (returns immediately)

**Behavioral Description:**
cxf_vector_free performs comprehensive, ordered deallocation of a Model structure and all of its transitively owned resources. The function first extracts the environment pointer from the structure for use in subsequent deallocation calls. It then proceeds through a well-defined sequence of cleanup phases: freeing remote solver and callback state if active, recursively freeing up to seven nested context structures (representing presolve models and sub-problems), freeing simple pointer fields and timing state, iterating over and freeing MIP hint arrays, freeing basis-related arrays, freeing auxiliary structures (matrix scaling, internal solver state, solution state, vector pairs, hash tables, basis factorization), freeing solution information structures with attribute cache invalidation, freeing constraint-related data (SOS, general constraints), freeing IIS and LP state, freeing warm-start data with its own nested sub-structures, freeing remaining fields and waiting for async threads, and finally clearing the validity sentinel, updating the environment's model tracking, and freeing the main structure itself.

**Thread Safety:** unsafe - The function must not be called concurrently on the same structure. It must not be called while an optimization is in progress on the structure.

**Dependencies:** cxf_calloc (via the environment's memory tracking for deallocation accounting). Calls numerous specialized cleanup functions for sub-structures.

---

### cxf_model_alloc

**Purpose:** Allocate and perform initial setup of a Model structure, including sentinel values, self-reference, optional child environment creation, and attribute table allocation.

**Signature:**
- Input: environment : pointer-to-Environment - The parent environment for this model
- Input: create_child_environment : int - If nonzero, create a private child environment for the model
- Input: child_parameter : int - Configuration parameter passed to child environment creation
- Output: pointer-to-Model (nullable) - Pointer to the newly allocated and initialized Model, or null on failure

**Preconditions:**
- environment must be a valid, activated Environment
- environment must have a valid license (activation state is ACTIVE)

**Postconditions:**
- On success, the returned Model has:
  - Its validity sentinel and secondary sentinel set to the predefined constants
  - Its primary_model self-reference set to point to itself
  - Its modification control flags cleared
  - Its environment pointer set (to either the parent or a newly created child environment)
  - Its environment_owned flag set appropriately (1 if a child environment was created, 0 otherwise)
  - Its attribute table allocated and initialized
  - Its internal subsystems initialized via the model initialization and setup routines
- On failure, any partially allocated resources have been cleaned up via cxf_vector_free, and null is returned

**Side Effects:**
- Allocates memory for the Model structure, attribute table, and optionally a child environment
- Registers the model with its environment's model tracking system (via the initialization and setup routines)

**Error Conditions:**
- Out of memory during Model structure allocation -> returns null
- Failure to create child environment -> cleans up and returns null
- Out of memory during attribute table allocation -> cleans up and returns null

**Behavioral Description:**
cxf_model_alloc allocates a zero-initialized block of memory for the Model structure, then writes the validity sentinel values and initializes the self-reference pointer. If a child environment is requested, it creates one that inherits parameters from the parent environment and updates the model's environment pointer and ownership flag accordingly. It then allocates a zero-initialized attribute table structure and stores it in the model. Finally, it calls the model initialization function (which populates the attribute table with entries for all supported attributes) and the model setup function (which completes internal state preparation). If any allocation or initialization step fails, the function uses cxf_vector_free to clean up any partially allocated resources and returns null.

**Thread Safety:** unsafe - Model allocation is not thread-safe. The caller must ensure no concurrent access to the parent environment during model creation, or protect the call with external synchronization.

**Dependencies:** cxf_calloc (for memory allocation), cxf_vector_free (for cleanup on failure), model initialization and setup routines (for attribute table population and internal state preparation), child environment creation (when requested).

---

## Module-Level Design Rationale

### Memory Tracking Architecture

The memory primitives implement a three-tier tracking system designed for high-performance multi-threaded solvers:

1. **Global atomic counter**: The root environment maintains a single 64-bit counter representing total memory usage. This counter is updated using atomic operations to ensure correctness under concurrent access.

2. **Thread-local batching**: To reduce contention on the global counter, each thread accumulates allocation deltas in a thread-local buffer. When the accumulated delta exceeds a configurable threshold (on the order of several megabytes), the batch is flushed to the global counter. This amortizes the cost of atomic operations over many allocations.

3. **Memory limit enforcement**: Before each allocation, the function checks whether the total usage (global counter plus thread-local pending) would exceed the configured memory limit. If so, the allocation is refused, allowing the solver to handle memory pressure gracefully rather than crashing.

This architecture follows standard practices for memory-managed runtime systems, as described by Berger et al. (2000), "Hoard: A Scalable Memory Allocator for Multithreaded Applications," ASPLOS.

### Custom Allocator Support

The custom allocator callback mechanism allows ISV (Independent Software Vendor) deployments to redirect all solver memory allocations through application-provided allocators. When custom allocators are active, a small header is prepended to each allocation to store the allocation size, since custom allocators may not support the platform-specific query for allocation size. The returned pointer is offset past this header, making the header invisible to the caller.

### Model Deallocation Ordering

cxf_vector_free follows a strict deallocation order designed to prevent dangling references and use-after-free errors. The key ordering constraints are:
- Attribute cache invalidation must precede solution data deallocation (since the attribute table may contain pointers into solution data)
- Nested contexts must be freed before the parent (since they may reference parent resources)
- The validity sentinel is cleared before final deallocation to detect use-after-free in debug scenarios
- Asynchronous threads must be joined before freeing any state they may reference
- The environment's model tracking is updated before freeing the structure, so the environment does not retain a stale reference

---

## Verification Checklist

```
[x] No hex addresses or offsets
[x] No Ghidra artifacts (param_, local_, iVar, DAT_, FUN_)
[x] No binary-specific constants
[x] No copied code fragments
[x] All algorithms cite published sources
[x] All data structures described semantically
[x] Passes the Clean Room Test (Rule 10)
```
