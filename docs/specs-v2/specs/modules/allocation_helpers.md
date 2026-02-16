# Module: Allocation Helpers

## Purpose

This module provides higher-level allocation functions that build on the memory primitives to allocate and initialize domain-specific solver structures. It includes the arena allocator for eta vectors used in the Product Form of the Inverse, the allocation and initialization of the WorkArrays (solution data container), and the pre-optimization resource validation function that checks licensing and model size limits. These functions bridge the gap between raw memory allocation and the solver's runtime data structures.

## Functions

### cxf_alloc_eta

**Purpose:** Allocate a block of memory from the eta vector memory pool using a bump-allocation strategy within linked chunks.

**Signature:**
- Input: environment : pointer-to-Environment - The environment for underlying memory allocation calls
- Input: pool_state : pointer-to-MemoryPoolState - The arena allocator state managing the pool of chunks
- Input: allocation_size : unsigned integer - Number of bytes to allocate
- Output: pointer (nullable) - Pointer to the allocated memory within the pool, or null on failure

**Preconditions:**
- environment must be a valid, initialized Environment (used only when a new chunk must be allocated)
- pool_state must be a valid, initialized MemoryPoolState with at least one chunk already allocated
- allocation_size should be positive

**Postconditions:**
- On success, the returned pointer references a contiguous block of allocation_size bytes within the current (or a newly allocated) chunk
- The pool state's current offset has been advanced by allocation_size
- If a new chunk was needed, it has been linked into the chunk chain and the pool's minimum chunk size has been increased for the next allocation
- The allocated memory is NOT zero-initialized (the caller is responsible for initialization)

**Side Effects:**
- May allocate a new chunk header and data buffer via cxf_calloc and the system allocator
- Updates the pool state's current chunk pointer, current offset, and minimum chunk size
- Links a new chunk into the singly-linked chunk chain

**Error Conditions:**
- Null pool_state -> returns null
- Failure to allocate a new chunk header -> returns null
- Failure to allocate the data buffer for a new chunk -> returns null

**Behavioral Description:**
cxf_alloc_eta implements a region-based memory allocator (arena allocator) optimized for the rapid allocation of eta vectors during simplex iterations. On the fast path, it checks whether the current chunk has sufficient remaining capacity for the requested allocation; if so, it performs a simple bump allocation by returning the current position in the buffer and advancing the offset. This fast path completes in O(1) with no system calls. On the slow path (insufficient space in the current chunk), it allocates a new chunk whose size is the larger of the requested allocation and the pool's current minimum chunk size. The new chunk is linked after the current chunk, and the pool state is updated to point to it. The minimum chunk size for future allocations is doubled (up to a maximum cap) to reduce the frequency of new chunk allocations over time, following the standard exponential growth strategy for arena allocators. This allocation pattern is described in the context of region-based memory management by Hanson (1990), "Fast allocation and deallocation of memory based on object lifetimes," *Software: Practice and Experience*.

**Thread Safety:** unsafe - The memory pool is owned by a single SolverState, which is used by a single thread. No synchronization is performed.

**Dependencies:** cxf_calloc (for chunk header allocation), system allocator (for chunk data buffer allocation).

---

### cxf_alloc_work_arrays

**Purpose:** Allocate (if necessary) and initialize the WorkArrays solution data container on a Model, preparing it for a new optimization call.

**Signature:**
- Input: model : pointer-to-Model - The model on which to allocate or reinitialize the WorkArrays
- Input: template : pointer-to-WorkArrays (nullable) - Optional template from which to copy scalar field values (e.g., from a scenario model); if null, fields are initialized to defaults
- Output: int - Error code: zero on success, OUT_OF_MEMORY error code on allocation failure

**Preconditions:**
- model must be a valid, initialized Model with a valid environment and matrix data
- If template is non-null, it must point to a valid, initialized WorkArrays structure
- The model's matrix data must be populated (the function reads the variable count from it)

**Postconditions:**
- On success, the model's WorkArrays pointer references an initialized structure with:
  - The active flag set to indicate the structure is live
  - Scale factors computed from the model's variable count multiplied by standard algorithmic tolerance and scaling constants
  - The base tolerance stored from the solver's tolerance constant
  - Anti-cycling history fields (previous entering variable, previous leaving variable, previous pivot row) initialized to the "not set" sentinel value
  - Adaptive threshold values initialized to the "not yet activated" sentinel value
  - Auxiliary indices initialized to the "not active" sentinel value
  - All solution pool and cut counters cleared to zero
  - All pointer fields for solution arrays (primal values, dual values) set to null
  - If a template was provided, scalar fields (counters, objective values, scale factors, thresholds) have been bulk-copied from the template, with pointer fields subsequently cleared to prevent aliasing
- Any previously allocated solution arrays (primal values, dual values) within the WorkArrays have been freed before reinitialization

**Side Effects:**
- Allocates the WorkArrays structure via cxf_calloc if not already present on the model
- Frees any existing solution arrays owned by the WorkArrays
- Clears borrowed pointer fields to prevent aliasing
- Invokes an internal cleanup routine on the model

**Error Conditions:**
- Out of memory during WorkArrays structure allocation -> returns OUT_OF_MEMORY error code

**Behavioral Description:**
cxf_alloc_work_arrays prepares the model's solution data container for a new optimization run. If the model does not yet have a WorkArrays structure, one is allocated as a zero-initialized block. Any existing solution arrays (primal and dual value arrays) are freed to avoid memory leaks from a previous solve. The function then initializes all fields to their default states: the active flag is set, scale factors are computed as the product of the variable count and standard tolerance/scaling constants, anti-cycling indices are set to sentinel values indicating "not set," and adaptive threshold values are set to sentinel values indicating "not yet activated." If a template WorkArrays is provided (used in multi-scenario optimization where a scenario model's results are transferred to the original model), the scalar fields are bulk-copied from the template and pointer fields are then explicitly cleared to null to prevent double-ownership of the template's arrays. Finally, additional tracking fields (counters, pool data) are zeroed.

**Thread Safety:** unsafe - Must be called from a single thread. The Model structure is not thread-safe.

**Dependencies:** cxf_calloc (for allocation), system deallocator (for freeing old arrays).

---

### cxf_setup_resources

**Purpose:** Validate the license and check model size against license limits before optimization can proceed.

**Signature:**
- Input: model : pointer-to-Model - The model to validate
- Output: int - Error code: zero on success, or a license/resource error code on failure

**Preconditions:**
- model must be a valid, initialized Model with a valid environment
- The model's environment must reference a root environment with license data

**Postconditions:**
- On success (return zero), the license is valid and the model's dimensions (variable count, constraint count, nonzero count, and quadratic term count, including any pending uncommitted modifications) are within the license limits
- On failure, an appropriate error code is returned and an error message may have been logged on the environment

**Side Effects:**
- For cloud-based or web license service deployment types: may perform network communication to validate or refresh a license token, protected by a critical section on the root environment
- May update cached license tokens on the root environment when a fresh token is obtained
- Logs error messages on the environment when initialization validation fails

**Error Conditions:**
- Environment relationship invalid (model's initialized environment does not match root environment) -> returns LICENSE_SIZE_EXCEEDED error
- License suspended -> returns LICENSE_SIZE_EXCEEDED error
- Cloud/WLS token validation failure (all fallback attempts exhausted) -> returns LICENSE_SIZE_EXCEEDED error
- Batch mode active on environment -> returns CANNOT_OPTIMIZE_BATCH error (batch-mode models cannot be optimized locally)
- Model variable count exceeds license limit -> returns LICENSE_LIMIT error
- Model constraint count (sum of all constraint types plus pending) exceeds license limit -> returns LICENSE_LIMIT error
- Model nonzero count exceeds license limit -> returns LICENSE_LIMIT error
- Model quadratic term count exceeds license limit -> returns LICENSE_LIMIT error

**Behavioral Description:**
cxf_setup_resources performs a multi-tier validation before allowing optimization to proceed. First, it validates the environment chain by confirming that the model's license reference matches the root environment's license reference; if not, or if a basic environment validation fails, it returns an error immediately. If the model has no matrix data (empty model), it returns success since there is nothing to validate. It then checks whether the license has been externally suspended.

For cloud-based and web license service (WLS) deployment types, the function enters a critical section and performs a cascading token validation strategy: it first tries a cached token, then an alternate cached token, then attempts a token refresh from the configuration server, and finally makes a full license service API call. Each successful validation updates the token cache for future calls. This multi-tier caching minimizes network latency during repeated optimization calls.

After license-type-specific validation, the function checks whether the environment is in batch mode (which prohibits local optimization). Finally, it validates the model's dimensions against the license limits. The total constraint count is computed as the sum of all constraint types (linear, quadratic, SOS, general, piecewise-linear, and others), including any pending uncommitted modifications from the lazy update buffer.

**Thread Safety:** conditional - Token validation for cloud and WLS licenses is protected by a critical section on the root environment, serializing concurrent token operations. Model size checks are read-only and safe for concurrent access to different models sharing the same environment.

**Dependencies:** initialization validation functions (for basic environment validations and token validation), system identification functions (for machine/user identification in cloud licensing), error logging (for reporting license failures).

---

## Module-Level Design Rationale

### Arena Allocation for Eta Vectors

The eta vector memory pool uses a region-based (arena) allocator rather than individual heap allocations for several reasons:

1. **Allocation speed**: Most eta vector allocations complete in O(1) via bump allocation (pointer increment), with no system calls or free-list management. This is critical because eta vectors are allocated on every simplex pivot, which may occur millions of times in a single solve.

2. **Zero fragmentation**: Sequential allocation within chunks eliminates internal fragmentation. There is no per-object overhead for free-list pointers or size headers.

3. **Bulk deallocation**: Eta vectors are never individually freed. The entire pool is released at once during basis refactorization (when the eta chain is reset) or at simplex cleanup. This matches the lifecycle pattern perfectly: create many small objects, use them, discard them all.

4. **Exponential chunk growth**: Starting with a small initial chunk and doubling the chunk size (up to a maximum cap) reduces the number of system allocations while avoiding excessive upfront memory commitment. This growth strategy is standard for arena allocators, as described by Hanson (1990).

The Product Form of the Inverse (PFI) approach that generates these eta vectors is described in Dantzig and Orchard-Hays (1954), "The Product Form for the Inverse in the Simplex Method," *Mathematical Tables and Other Aids to Computation*.

### WorkArrays Initialization Pattern

The WorkArrays allocation function follows a "create-or-reinitialize" pattern common in solver implementations:

- If the structure does not exist, it is allocated.
- If it already exists from a previous solve, its arrays are freed but the structure itself is reused.
- Sentinel values (-1 for indices, -1.0 for thresholds) distinguish "not yet set" from valid values, enabling lazy initialization of adaptive parameters during the solve.
- Template copying supports multi-scenario optimization where results from one solve are used to initialize the next.

The adaptive threshold pattern (initialized to -1.0 meaning "compute on first use") is a standard technique for dynamic tolerance adjustment in LP solvers, as discussed in Maros (2003), *Computational Techniques of the Simplex Method*, Chapter 8.

### License Validation Strategy

The initialization validation function implements a defense-in-depth approach:

1. **Environment chain validation** catches corrupted or detached model-environment relationships.
2. **License suspension check** supports external administrative control.
3. **Token caching with cascading fallback** minimizes network latency while ensuring validity.
4. **Comprehensive size checking** covers all constraint types and pending modifications to prevent license circumvention via lazy updates.

The critical section protecting token operations ensures that concurrent optimizations do not corrupt shared token state, while the model size checks are inherently read-only and safe for concurrent access.

## References

- Dantzig, G.B. and Orchard-Hays, W. (1954). "The Product Form for the Inverse in the Simplex Method." *Mathematical Tables and Other Aids to Computation*, 8(46):64-67.
- Hanson, D.R. (1990). "Fast allocation and deallocation of memory based on object lifetimes." *Software: Practice and Experience*, 20(1):5-12.
- Maros, I. (2003). *Computational Techniques of the Simplex Method*. Springer. Chapters 2 and 8.
- Berger, E.D., McKinley, K.S., Blumofe, R.D., and Wilson, P.R. (2000). "Hoard: A Scalable Memory Allocator for Multithreaded Applications." ASPLOS.

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
