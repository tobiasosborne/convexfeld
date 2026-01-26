# M2-M4: Foundation, Infrastructure, Data Layer Learnings

## M2: Foundation Layer (Level 0)

### 2026-01-26: M2.1.1 Memory Tests - TDD Tests Complete

**File created:** `tests/unit/test_memory.c` (154 LOC) - 12 tests

**Tests implemented:**
- `test_cxf_malloc_basic` - basic allocation works
- `test_cxf_malloc_zero_size` - returns NULL for size=0
- `test_cxf_malloc_large_size` - 1MB allocation succeeds
- `test_cxf_calloc_zeroed` - memory is zero-initialized
- `test_cxf_calloc_zero_count` - returns NULL for count=0
- `test_cxf_calloc_zero_size` - returns NULL for size=0
- `test_cxf_realloc_grow` - preserves data when growing
- `test_cxf_realloc_shrink` - preserves data when shrinking
- `test_cxf_realloc_null_ptr` - NULL ptr acts like malloc
- `test_cxf_realloc_zero_size` - returns NULL and frees
- `test_cxf_free_null_safe` - NULL is safe to free
- `test_cxf_free_after_malloc` - normal free works

**Key decisions:**
- Tests use external function declarations (not header include) since memory module doesn't have a dedicated header yet
- `cxf_realloc(ptr, 0)` frees memory and returns NULL (consistent with free-like behavior)
- Tests are grouped by function type with clear section headers

---

### 2026-01-26: M2.1.2 Memory Implementation - Full Implementation

**File created:** `src/memory/alloc.c` (103 LOC)
**File removed:** `src/memory/alloc_stub.c`

**Functions implemented:**
- `cxf_malloc(size)` - allocates memory, returns NULL for size=0
- `cxf_calloc(count, size)` - allocates zero-initialized memory
- `cxf_realloc(ptr, size)` - resizes memory with proper edge cases
- `cxf_free(ptr)` - frees memory, NULL-safe

**Key decisions:**
- Kept simple signatures (no env parameter) - env-scoped allocation comes with M3 Threading
- Spec describes full environment tracking, but that requires threading infrastructure
- Current implementation passes all TDD tests from M2.1.1
- Added detailed documentation referencing future env-scoped enhancements

**Lesson learned:** Specs describe the full-featured version, but implementation can be staged. Simple working implementation first, then add features as dependencies are built.

---

### 2026-01-26: M2.1.3 cxf_vector_free, cxf_alloc_eta

**File created:** `src/memory/vectors.c` (100 LOC)

**Types added to cxf_types.h:**
- `VectorContainer` - Sparse vector with indices, values, auxData
- `EtaChunk` - Chunk in arena allocator chain
- `EtaBuffer` - Arena allocator state

**Functions implemented:**
- `cxf_vector_free(vec)` - Deallocate VectorContainer and all arrays (NULL-safe)
- `cxf_eta_buffer_init(buffer, min_chunk_size)` - Initialize arena allocator
- `cxf_eta_buffer_free(buffer)` - Free all chunks in arena
- `cxf_eta_buffer_reset(buffer)` - Reset for reuse without freeing
- `cxf_alloc_eta(env, buffer, size)` - Arena allocation with exponential growth

**Arena allocator pattern:**
- Fast path: bump pointer in active chunk O(1)
- Slow path: allocate new chunk, link to chain, double chunk size
- Exponential growth caps at CXF_MAX_CHUNK_SIZE (64KB)
- Reset enables reuse without deallocation (good for refactorization cycles)

---

### 2026-01-26: M2.1.4 State Deallocators

**File created:** `src/memory/state_cleanup.c` (107 LOC)

**Functions implemented:**
- `cxf_free_solver_state(ctx)` - Free SolverContext with working arrays and subcomponents
- `cxf_free_basis_state(basis)` - Wrapper for cxf_basis_free
- `cxf_free_callback_state(ctx)` - Free CallbackContext (does NOT free user_data)

**Key design decisions:**
- SolverContext frees all 6 working arrays plus basis and pricing subcomponents
- Does NOT free model_ref (owned by caller)
- Clears magic numbers before freeing CallbackContext
- All functions are NULL-safe

---

## M3: Infrastructure Layer (Level 1)

### 2026-01-26: M3.1.1 Error Tests

**File created:** `tests/unit/test_error.c` (276 LOC) - 26 tests

**Tests cover:**
- cxf_error: basic message, formatted, null env, empty message
- cxf_geterrormsg: null env handling
- cxf_errorlog: null safety
- cxf_check_nan: clean array, with NaN, empty, null, Inf not detected
- cxf_check_nan_or_inf: finite detection, NaN, positive/negative infinity
- cxf_checkenv: valid, null, invalid magic number
- cxf_pivot_check: valid, too small, zero, negative, NaN

**Key learnings:**
- Use `isfinite()` from math.h for proper NaN/Inf detection
- Simple `val > 1e308` check fails for DBL_MAX (~1.8e308)
- NaN detection: `val != val` is the canonical portable check

**Note:** test_error.c exceeds 200 LOC (276 LOC) - refactor issue created (convexfeld-afb)

---

### 2026-01-26: M3.1.2-M3.1.4 Core Error Functions

**Files created:**
- `src/error/core.c` (82 LOC) - cxf_error, cxf_geterrormsg, cxf_errorlog
- `src/error/nan_check.c` (51 LOC) - cxf_check_nan, cxf_check_nan_or_inf
- `src/error/env_check.c` (28 LOC) - cxf_checkenv

**Key learnings:**
- cxf_errorlog uses output_flag for console control (>= 1 enables)
- SE weight update deferred until SolverContext integration
- Invalidation flags defined locally in update.c (not in header yet)

---

## M4: Data Layer (Level 2)

### 2026-01-26: M4.1.1 Matrix Tests

**File created:** `tests/unit/test_matrix.c` (197 LOC) - 20 tests

**Tests written:**
- SparseMatrix creation/init/free (5 tests) - PASS
- cxf_matrix_multiply SpMV (4 tests) - Implemented in M4.1.3
- cxf_dot_product dense/sparse (6 tests) - Implemented in M4.1.4
- cxf_vector_norm L1/L2/L∞ (5 tests) - Implemented in M4.1.4

**TDD pattern:**
- Added function stubs that return 0 so tests link and run
- Stubs live in sparse_stub.c until proper implementation
- Edge cases (zero vectors, orthogonal, empty sparse) pass by design

---

### 2026-01-26: M4.1.2 SparseMatrix Structure (Full)

**File created:** `src/matrix/sparse_matrix.c` (171 LOC)

**Functions implemented:**
- `cxf_sparse_validate(mat)` - Validates CSC structure invariants
- `cxf_sparse_build_csr(mat)` - Builds CSR format from existing CSC
- `cxf_sparse_free_csr(mat)` - Frees only CSR arrays (keeps CSC)

**Key validations checked:**
- col_ptr monotonically non-decreasing
- col_ptr[0] == 0, col_ptr[num_cols] == nnz
- All row indices in range [0, num_rows)
- Required arrays not NULL when dimensions > 0

**CSR build algorithm:**
1. Count entries per row (traverse CSC row_idx)
2. Convert counts to cumulative offsets (prefix sum)
3. Transpose CSC to CSR using working copy of row_ptr

---

### 2026-01-26: M4.1.3 cxf_matrix_multiply

**File created:** `src/matrix/multiply.c` (103 LOC)

**Functions implemented:**
- `cxf_matrix_multiply(x, y, ...)` - y = Ax or y += Ax
- `cxf_matrix_transpose_multiply(...)` - y = A^T x (bonus)

**Algorithm:**
1. If accumulate=0, zero out y with memset
2. For each column j where x[j] != 0:
   - Get column range from col_start[j] to col_start[j+1]
   - For each entry k in range: y[row_indices[k]] += coeff_values[k] * x[j]

**Key optimization:**
- Skip columns where x[j] = 0.0 (common in simplex with sparse vectors)

---

### 2026-01-26: M4.1.5 Row-Major Conversion

**File created:** `src/matrix/row_major.c` (167 LOC)

**Functions implemented:**
- `cxf_prepare_row_data(mat)` - Validate CSC, allocate CSR arrays
- `cxf_build_row_major(mat)` - Two-pass transpose algorithm
- `cxf_finalize_row_data(mat)` - Finalize conversion state

**Algorithm (cxf_build_row_major):**
1. Pass 1: Count entries per row, compute cumulative offsets
2. Pass 2: Fill CSR arrays using working pointer copy

**Key design decisions:**
- Separate prepare/build/finalize for fine-grained control
- Empty matrix handled correctly (row_ptr zeroed)
- Returns error if prepare not called before build

---

### 2026-01-26: M4.1.6 cxf_sort_indices

**File created:** `src/matrix/sort.c` (83 LOC)

**Functions implemented:**
- `cxf_sort_indices(indices, n)` - Sort indices only
- `cxf_sort_indices_values(indices, values, n)` - Sort with synchronized values

**Design decision:**
- Used insertion sort instead of full introsort (spec describes introsort)
- Insertion sort is optimal for small arrays (n < 16) typical in sparse matrix operations
- Full introsort can be added later if needed for large arrays

---

### 2026-01-26: M4.2.2 Timestamp

**File created:** `src/timing/timestamp.c` (43 LOC)

**Function implemented:**
- `cxf_get_timestamp()` - High-resolution timestamp using CLOCK_MONOTONIC

**Key learning:**
- `clock_gettime(CLOCK_MONOTONIC)` requires `#define _POSIX_C_SOURCE 199309L`
