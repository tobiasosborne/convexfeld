# Spec V2 Audit: Matrix & Memory

## Files Reviewed

### Spec Files
- `docs/specs-v2/specs/modules/matrix_core.md`
- `docs/specs-v2/specs/modules/matrix_finalization.md`
- `docs/specs-v2/specs/modules/memory_primitives.md`
- `docs/specs-v2/specs/modules/allocation_helpers.md`

### Implementation Files
- `src/matrix/multiply.c`
- `src/matrix/vectors.c`
- `src/matrix/sort.c`
- `src/matrix/sparse_matrix.c`
- `src/matrix/row_major.c`
- `src/matrix/sparse_stub.c`
- `src/matrix/matrix_internal.h`
- `src/memory/alloc.c`
- `src/memory/state_cleanup.c`
- `src/memory/memory_internal.h`
- `src/memory/vectors.c`
- `src/memory/internal_types.h`

---

## Compliant Functions

### cxf_alloc_eta (Allocation Helpers spec)
- **Status:** PARTIALLY COMPLIANT (see violations below for deviations)
- Implementation exists in `src/memory/vectors.c:119`
- Signature matches spec: takes environment, pool_state (EtaBuffer), allocation_size
- Fast-path bump allocation is correct
- Slow-path new chunk allocation is correct
- Exponential growth strategy is implemented
- Chunk linking is correct
- Returns NULL on failure as spec requires

---

## VIOLATIONS

### [V1] cxf_sort_indices -- Wrong function name and wrong sort key
- **Spec says:** Function is named `cxf_sort_indices`. Signature takes `(int64 count, double* values, int* indices)`. Primary sort key is the `values` array (doubles sorted ascending); the `indices` array is permuted as a satellite.
- **Code does:** No function named `cxf_sort_indices` exists. Instead, two functions exist: `cxf_sort_by_values(int *indices, int n)` sorts integer indices by their own values (ascending), and `cxf_sort_by_values_paired(int *indices, double *values, int n)` sorts integer indices ascending with values as satellites. Both sort by the **indices** array, not by the **values** array. The spec explicitly says the primary sort key is the values (doubles), with indices permuted to match.
- **File:** `src/matrix/sort.c:150-167`
- **Severity:** HIGH -- semantics are inverted from spec

### [V2] cxf_sort_indices -- Wrong sorting algorithm
- **Spec says:** Hybrid quicksort + shellsort. Quicksort uses three-way partitioning (Dutch National Flag / Dijkstra 1976) with median-of-three pivot. Threshold for quicksort is 100 elements. Recursion depth fallback is to shellsort (gap sequence dividing by ~2.2). Shellsort final pass uses gap 1 (insertion sort).
- **Code does:** Uses introsort: quicksort with Hoare partitioning (NOT three-way/Dutch National Flag) + heapsort fallback (NOT shellsort). Threshold for base case is 16 (NOT 100). Fallback is heapsort (NOT shellsort with diminishing gap sequence).
- **File:** `src/matrix/sort.c:102-141`
- **Severity:** MEDIUM -- different algorithm family, but both achieve O(n log n). Three-way partitioning handles duplicates more efficiently, which matters for LP pricing.

### [V3] cxf_sort_indices -- count parameter type mismatch
- **Spec says:** `count : int64`
- **Code does:** `int n`
- **File:** `src/matrix/sort.c:150,163`
- **Severity:** LOW -- limits sortable array size to 2^31 instead of 2^63

### [V4] cxf_prepare_row_data -- Completely wrong semantics
- **Spec says:** Takes `pointer-to-Model` and `int mode`. Reverses scaling transformation (D_r * A * D_c * g), handles swap data entry/exit, manages state (penalty release, model lock, scaling state save/clear), handles range constraint reversal (mode 0), issues matrix change notifications.
- **Code does:** Takes `MatrixData*` (not Model). Validates CSC structure and allocates CSR arrays (row_ptr, col_idx, row_values). No scaling reversal, no swap data handling, no mode parameter, no state management. The implementation is a simple "allocate CSR arrays" function, not the multi-phase unscaling/state-management pipeline described in the spec.
- **File:** `src/matrix/row_major.c:34-76`
- **Severity:** CRITICAL -- entirely different function. The spec's cxf_prepare_row_data is the first stage of a lazy CSR conversion pipeline that handles unscaling; the implementation is just a CSR array allocator.

### [V5] cxf_build_row_major -- Signature mismatch and missing features
- **Spec says:** Takes `pointer-to-Model`, returns `int`. Performs full two-pass CSC-to-CSR for linear constraints, builds variable-to-constraint reverse mapping for general constraints, builds quadratic constraint CSR, delegates SOS constraint CSR to a helper. Frees all existing CSR arrays first (cache invalidation). Handles edge cases for no variables or no constraints.
- **Code does:** Takes `MatrixData*` (not Model), returns `int`. Performs basic two-pass CSC-to-CSR only for linear constraints. No general constraint mapping, no quadratic constraint CSR, no SOS constraint delegation, no cache invalidation (expects caller to have called cxf_prepare_row_data first).
- **File:** `src/matrix/row_major.c:94-140`
- **Severity:** HIGH -- missing general/quadratic/SOS constraint support. Signature uses MatrixData* instead of Model*.

### [V6] cxf_calloc -- Missing environment parameter and memory tracking
- **Spec says:** Signature is `(environment: pointer-to-Environment (nullable), count: unsigned int, element_size: unsigned int) -> pointer`. When environment is provided: checks memory limit, supports custom allocator callback, tracks usage with thread-local batching, updates peak usage, prepends size header for custom allocators.
- **Code does:** Signature is `(size_t count, size_t size) -> pointer`. No environment parameter. No memory limit enforcement. No custom allocator support. No usage tracking. No thread-local batching. Plain wrapper around system `calloc()`.
- **File:** `src/memory/alloc.c:53-58`
- **Severity:** HIGH -- entire memory tracking infrastructure is missing. Comment in code acknowledges this: "Environment-scoped allocation with memory tracking and thread safety will be added in M3."

### [V7] cxf_realloc -- Missing environment parameter and memory tracking
- **Spec says:** Signature is `(environment: pointer-to-Environment (nullable), existing_pointer: pointer, new_size: unsigned int) -> pointer`. Checks memory limit on growth, updates usage counter by delta, supports custom allocators, handles size-tracking header.
- **Code does:** Signature is `(void *ptr, size_t new_size) -> pointer`. No environment parameter. No memory tracking. Plain wrapper around system `realloc()`.
- **File:** `src/memory/alloc.c:77-90`
- **Severity:** HIGH -- same gap as cxf_calloc

### [V8] cxf_vector_free -- Completely wrong semantics
- **Spec says:** Recursively frees a **Model** (or solver context) structure and ALL owned sub-structures. Signature is `(pointer-to-pointer-to-Model)`. Sets the caller's pointer to NULL. Clears validity sentinel. Updates environment's model tracking. Frees nested contexts (up to 7), timing state, MIP hints, basis arrays, matrix scaling, solution state, SOS/general constraints, IIS/LP state, warm-start data, barrier state, etc. Waits for async threads. Invalidates attribute cache before freeing solution data.
- **Code does:** Frees a **VectorContainer** (indices, values, auxData arrays). Signature is `(VectorContainer*)`. Does NOT null out the caller's pointer (takes single pointer, not double pointer). Does NOT free Model sub-structures.
- **File:** `src/memory/vectors.c:29-41`
- **Severity:** CRITICAL -- entirely different function. The spec describes the top-level Model destructor; the implementation is a trivial vector container deallocator. Note: `cxf_free_attribute_table` in `src/memory/state_cleanup.c` partially overlaps with the spec's cxf_vector_free behavior (frees SolverState), but does not match the full Model-level recursive destructor.

### [V9] cxf_alloc_eta -- pool_state type name mismatch
- **Spec says:** `pool_state : pointer-to-MemoryPoolState`
- **Code does:** `buffer : EtaBuffer*` -- The type is named `EtaBuffer`, not `MemoryPoolState`.
- **File:** `src/memory/vectors.c:119`
- **Severity:** LOW -- functionally equivalent, naming differs

### [V10] cxf_alloc_eta -- Memory not zero-initialized on fast path as expected
- **Spec says:** "The allocated memory is NOT zero-initialized (the caller is responsible for initialization)" -- this is spec-compliant.
- **Code does:** Fast path returns pointer into existing buffer (not zeroed) -- compliant. Slow path uses `cxf_malloc` (not zeroed) -- compliant. However, chunk header allocation uses `cxf_calloc` (zeroed) which is fine since it's the header not the user data.
- **Status:** COMPLIANT (false alarm, included for completeness)

### [V11] cxf_alloc_eta -- Missing check for existing first chunk
- **Spec says:** "pool_state must be a valid, initialized MemoryPoolState with at least one chunk already allocated"
- **Code does:** Handles the case where `active == NULL` and `firstChunk == NULL` by creating a first chunk. The implementation is more robust than the spec's precondition requires, which is acceptable.
- **Status:** COMPLIANT (implementation is more permissive than spec precondition)

### [V12] cxf_matrix_setup -- Not implemented
- **Spec says:** Function `cxf_matrix_setup` should partition each column's nonzero entries so active constraints appear before removed ones (Hoare two-pointer partition), manage swap data reference counting, swap column length and bounds array pointers.
- **Code does:** No implementation found anywhere in the codebase (searched all `src/` files).
- **File:** N/A
- **Severity:** HIGH -- spec function entirely missing

### [V13] cxf_finalize_row_data -- Not implemented
- **Spec says:** Function `cxf_finalize_row_data` in the Matrix Finalization module should compute and apply matrix scaling factors (Ruiz equilibration, Curtis-Reid, etc.), handle constraint sense normalization, manage saved scaling factor reuse, propagate scaling to bounds/RHS/quadratic/PWL constraints.
- **Code does:** No implementation found anywhere in the codebase (searched all `src/` files).
- **File:** N/A
- **Severity:** HIGH -- spec function entirely missing. This is the scaling engine.

### [V14] cxf_model_alloc -- Not implemented
- **Spec says:** Function `cxf_model_alloc` should allocate and initialize a Model structure with sentinel values, self-reference, optional child environment creation, and attribute table allocation.
- **Code does:** No implementation found anywhere in the codebase (searched all `src/` files).
- **File:** N/A
- **Severity:** HIGH -- spec function entirely missing

### [V15] cxf_alloc_work_arrays -- Not implemented
- **Spec says:** Function `cxf_alloc_work_arrays` should allocate/reinitialize the WorkArrays solution data container on a Model, setting active flag, computing scale factors, initializing anti-cycling sentinels, supporting template copying.
- **Code does:** No implementation found anywhere in the codebase (searched all `src/` files).
- **File:** N/A
- **Severity:** MEDIUM -- spec function missing, but SolverState allocation in state_cleanup.c provides some overlapping cleanup functionality

### [V16] cxf_setup_resources -- Not implemented
- **Spec says:** Function `cxf_setup_resources` should validate license, check model size against license limits, handle cloud/WLS token validation, check batch mode.
- **Code does:** No implementation found anywhere in the codebase (searched all `src/` files).
- **File:** N/A
- **Severity:** LOW -- license validation is not critical for the solver's mathematical correctness

---

## Missing Functions

| Spec Module | Function | Status |
|---|---|---|
| Matrix Core | `cxf_matrix_setup` | NOT IMPLEMENTED |
| Matrix Finalization | `cxf_finalize_row_data` | NOT IMPLEMENTED |
| Memory Primitives | `cxf_model_alloc` | NOT IMPLEMENTED |
| Memory Primitives | `cxf_vector_free` (Model destructor) | WRONG SEMANTICS (frees VectorContainer, not Model) |
| Allocation Helpers | `cxf_alloc_work_arrays` | NOT IMPLEMENTED |
| Allocation Helpers | `cxf_setup_resources` | NOT IMPLEMENTED |

## Extra Functions (not in spec)

| File | Function | Notes |
|---|---|---|
| `src/matrix/multiply.c` | `cxf_matrix_multiply` | Not in any of the 4 audited specs; may be in another spec module |
| `src/matrix/multiply.c` | `cxf_matrix_transpose_multiply` | Not in any of the 4 audited specs |
| `src/matrix/vectors.c` | `cxf_dot_product` | Not in any of the 4 audited specs |
| `src/matrix/vectors.c` | `cxf_dot_product_sparse` | Not in any of the 4 audited specs |
| `src/matrix/vectors.c` | `cxf_vector_norm` | Not in any of the 4 audited specs |
| `src/matrix/sparse_matrix.c` | `cxf_sparse_validate` | Not in any of the 4 audited specs |
| `src/matrix/sparse_matrix.c` | `cxf_sparse_build_csr` | Not in any of the 4 audited specs (duplicates cxf_build_row_major functionality) |
| `src/matrix/sparse_matrix.c` | `cxf_sparse_free_csr` | Not in any of the 4 audited specs |
| `src/matrix/sparse_stub.c` | `cxf_sparse_create` | Not in any of the 4 audited specs |
| `src/matrix/sparse_stub.c` | `cxf_sparse_free` | Not in any of the 4 audited specs |
| `src/matrix/sparse_stub.c` | `cxf_sparse_init_csc` | Not in any of the 4 audited specs |
| `src/matrix/sort.c` | `cxf_sort_by_values` | Replaces spec's cxf_sort_indices but with different semantics |
| `src/matrix/sort.c` | `cxf_sort_by_values_paired` | Replaces spec's cxf_sort_indices but with different semantics |
| `src/memory/alloc.c` | `cxf_malloc` | Not in spec (spec only defines cxf_calloc and cxf_realloc) |
| `src/memory/alloc.c` | `cxf_free` | Not in spec as standalone (spec mentions deallocation as part of cxf_calloc dependencies) |
| `src/memory/vectors.c` | `cxf_eta_buffer_init` | Not in spec (spec assumes pool_state is pre-initialized) |
| `src/memory/vectors.c` | `cxf_eta_buffer_free` | Not in spec |
| `src/memory/vectors.c` | `cxf_eta_buffer_reset` | Not in spec |
| `src/memory/state_cleanup.c` | `cxf_free_attribute_table` | Not in spec (partially overlaps cxf_vector_free spec) |
| `src/memory/state_cleanup.c` | `cxf_free_basis_state` | Not in spec |
| `src/memory/state_cleanup.c` | `cxf_free_callback_state` | Not in spec |

## Notes

1. **The implementation is a simplified, early-stage version of what the spec describes.** The spec modules describe a production LP solver with Model objects, Environment-scoped memory tracking, matrix scaling, constraint partitioning, and license validation. The implementation provides basic CSC/CSR matrix operations, plain system memory allocation, and a correctly-implemented eta arena allocator. The gap is architectural, not just algorithmic.

2. **cxf_prepare_row_data and cxf_build_row_major have been reimagined.** The spec describes these as part of a lazy CSR conversion pipeline operating on a Model with scaling reversal and constraint partitioning. The implementation operates directly on a MatrixData struct and performs only the CSC-to-CSR conversion step (stage 2 of the spec's pipeline). The spec's stage 1 (unscaling) and stage 1.5 (partitioning) are entirely absent.

3. **Memory primitives are placeholder implementations.** The `alloc.c` comment explicitly states: "Environment-scoped allocation with memory tracking and thread safety will be added in M3 (Threading)." The spec describes a full memory tracking system with atomic counters, thread-local batching, memory limits, custom allocators, and size-tracking headers. None of this exists.

4. **cxf_vector_free name collision.** The spec uses `cxf_vector_free` for the Model destructor (recursive deallocation of all Model sub-structures). The implementation uses the same name for a VectorContainer deallocator. This is a significant naming conflict that will need resolution when the Model destructor is implemented.

5. **cxf_sort_indices semantic inversion.** The spec sorts by double values with int indices as satellites. The implementation sorts by int indices with double values as satellites. The function names also differ (cxf_sort_indices vs cxf_sort_by_values/cxf_sort_by_values_paired). This inversion affects all callers that depend on value-ordered sorting (pricing candidates by reduced cost, quadratic terms by constraint index).

6. **Duplicate CSR construction.** Both `cxf_sparse_build_csr` (sparse_matrix.c) and `cxf_build_row_major` (row_major.c) implement the same two-pass CSC-to-CSR conversion algorithm. This duplication should be resolved.
