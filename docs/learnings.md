# Project Learnings

This file captures learnings, gotchas, and useful patterns discovered during development.

**Rule: ERRORS = SUCCESS if learnings are recorded here.**

---

## 2026-01-26: M6.1.3 cxf_pricing_init

### SUCCESS: Full pricing initialization with strategy-specific allocation

**File created:**
- `src/pricing/init.c` (179 LOC) - Full pricing initialization

**Function implemented:**
- `cxf_pricing_init(ctx, num_vars, strategy)` - Initialize pricing context for new solve

**Algorithm:**
1. Auto-select strategy if requested (strategy=0, threshold n<1000 for small)
2. Store num_vars and strategy in context
3. Allocate candidate arrays per level:
   - Level 0: full (all variables)
   - Level 1+: sqrt(n) for partial pricing, minimum MIN_CANDIDATES=100
4. For steepest edge/Devex: allocate weights array, initialize to 1.0
5. Reset statistics and invalidate all caches

**PricingContext structure extended with:**
- `num_vars` - problem size
- `strategy` - pricing strategy (0=auto, 1=partial, 2=SE, 3=Devex)
- `candidate_sizes` - allocated size per level
- `weights` - steepest edge weights array

**Key implementation pattern:**
- Helper function `compute_level_size()` for determining per-level allocation
- Proper cleanup on allocation failure (free partial allocations)
- Reinit case: free existing arrays before reallocating
- Forward declaration needed when function called before definition

**Files modified:**
- `include/convexfeld/cxf_pricing.h` - Extended PricingContext
- `src/pricing/context.c` - Handle new fields, moved init to init.c
- `CMakeLists.txt` - Added init.c

**Test results:**
- All 8 test suites PASS (100% tests passed)

---

## 2026-01-26: M5.1.5 cxf_btran

### SUCCESS: Backward transformation with Product Form of Inverse

**File created:**
- `src/basis/btran.c` (126 LOC) - Backward transformation implementation

**Function implemented:**
- `cxf_btran(basis, row, result)` - Solve y^T B = e_row^T using eta representation

**Algorithm (Product Form of Inverse, REVERSE order):**
1. Initialize result as unit vector e_row
2. Collect eta pointers into array (singly-linked list only has next)
3. Apply eta vectors in REVERSE order (newest to oldest):
   - Compute dot product: temp = sum(eta_val[k] * result[indices[k]])
   - Update pivot: result[pivot_row] = (result[pivot_row] - temp) / pivot_elem
   - Other positions UNCHANGED (key difference from FTRAN)

**Key mathematical insight - BTRAN vs FTRAN:**
- FTRAN applies E^(-1): updates pivot AND off-diagonal positions
- BTRAN applies (E^(-1))^T: updates ONLY pivot position
- BTRAN traverses eta list in REVERSE order (newest to oldest)

**Implementation pattern for reverse traversal:**
- Singly-linked list with only `next` pointers
- Collect pointers into array, then traverse backwards
- Stack allocation for ≤64 etas, heap for larger (MAX_STACK_ETAS = 64)
- This avoids recursion overhead and is cache-friendly

**Files modified:**
- `CMakeLists.txt` - Added btran.c to build
- `src/basis/basis_stub.c` - Removed cxf_btran stub (now in btran.c)

**Test results:**
- All 8 test suites PASS (100% tests passed)

---

## 2026-01-26: M5.1.4 cxf_ftran

### SUCCESS: Forward transformation with Product Form of Inverse

**File created:**
- `src/basis/ftran.c` (95 LOC) - Forward transformation implementation

**Function implemented:**
- `cxf_ftran(basis, column, result)` - Solve Bx = b using eta representation

**Algorithm (Product Form of Inverse):**
1. Copy input column to result (identity basis case)
2. Apply eta vectors in chronological order (oldest to newest via linked list)
3. For each eta E with pivot row r and pivot element p:
   - Save temp = result[r]
   - result[r] = temp / pivot_elem
   - For each off-diagonal entry: result[j] -= eta_value[j] * temp

**Key design decisions:**
- Follows linked list from eta_head → next → next (oldest to newest)
- Separate input/output arrays to preserve original column
- Bounds checking on pivot_row and division by zero check on pivot_elem
- Sparse off-diagonal entries stored in indices/values arrays

**Files modified:**
- `CMakeLists.txt` - Added ftran.c to build
- `src/basis/basis_stub.c` - Removed cxf_ftran stub (now in ftran.c)

**Test results:**
- All 8 test suites PASS (100% tests passed)

---

## 2026-01-26: M2.1.3 cxf_vector_free, cxf_alloc_eta

### SUCCESS: Vector memory management and eta arena allocator

**File created:**
- `src/memory/vectors.c` (100 LOC) - VectorContainer free and EtaBuffer arena allocator

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

**TDD tests (16 total):**
- VectorContainer free tests (4) - NULL, empty, partial, full
- EtaBuffer init tests (2) - basic, custom size
- cxf_alloc_eta tests (7) - NULL, first alloc, fast/slow path, large, growth, max
- EtaBuffer free/reset tests (3) - empty, with chunks, reset

**Test results:**
- All 16 new tests PASS
- All existing tests still PASS

---

## 2026-01-26: M6.1.2 PricingContext Structure

### SUCCESS: PricingContext lifecycle extracted to dedicated file

**File created:**
- `src/pricing/context.c` (118 LOC) - PricingContext lifecycle management

**Functions implemented:**
- `cxf_pricing_create(num_vars, max_levels)` - Allocates PricingContext with level arrays
- `cxf_pricing_free(ctx)` - Frees context and all arrays (NULL-safe)
- `cxf_pricing_init(ctx, num_vars, strategy)` - Reinitialize for new solve

**Extraction pattern:**
- Original lifecycle functions lived in `pricing_stub.c` for TDD
- Extracted to `context.c` as proper implementation
- Stub file now only contains algorithm functions (candidates, steepest, update, etc.)
- Both files linked - no duplicate symbols

**Key fields in PricingContext:**
- current_level, max_levels - level management
- candidate_counts, candidate_arrays, cached_counts - per-level arrays
- last_pivot_iteration, total_candidates_scanned, level_escalations - statistics

**Files modified:**
- `src/pricing/pricing_stub.c` - Removed lifecycle functions (now in context.c)
- `CMakeLists.txt` - Added context.c to build

**Test results:**
- All 24 pricing tests PASS

---

## 2026-01-26: M4.1.3 cxf_matrix_multiply

### SUCCESS: Sparse matrix-vector multiply implemented

**File created:**
- `src/matrix/multiply.c` (103 LOC) - SpMV implementation

**Functions implemented:**
- `cxf_matrix_multiply(x, y, num_vars, num_constrs, col_start, row_indices, coeff_values, accumulate)` - y = Ax or y += Ax
- `cxf_matrix_transpose_multiply(...)` - y = A^T x or y += A^T x (bonus)

**Algorithm:**
1. If accumulate=0, zero out y with memset
2. For each column j where x[j] != 0:
   - Get column range from col_start[j] to col_start[j+1]
   - For each entry k in range: y[row_indices[k]] += coeff_values[k] * x[j]

**Key optimization:**
- Skip columns where x[j] = 0.0 (common in simplex with sparse vectors)

**Test results:**
- All 4 SpMV tests pass (simple 2x2, accumulate, sparse column, zero skip)
- Remaining M4.1.4 tests (dot product, norms) still fail as expected

**Files modified:**
- `src/matrix/sparse_stub.c` - Removed cxf_matrix_multiply stub
- `CMakeLists.txt` - Added multiply.c to build

---

## 2026-01-26: M5.1.3 EtaFactors Structure

### SUCCESS: EtaFactors lifecycle extracted to dedicated file

**File created:**
- `src/basis/eta_factors.c` (192 LOC) - EtaFactors lifecycle and utilities

**Functions implemented:**
- `cxf_eta_create(type, pivot_row, nnz)` - Allocates eta with sparse arrays
- `cxf_eta_free(eta)` - Frees eta and arrays (NULL-safe)
- `cxf_eta_init(eta, type, pivot_row, nnz)` - Reinitialize existing eta
- `cxf_eta_validate(eta, max_rows)` - Validate eta invariants
- `cxf_eta_set(eta, indices, values)` - Copy data into eta arrays

**Key validations in cxf_eta_validate:**
- nnz >= 0
- type is 1 (refactor) or 2 (pivot)
- pivot_row in range [0, max_rows)
- pivot_elem is finite and non-zero
- All indices in range and values finite

**Error code gotcha:**
- Use `CXF_ERROR_OUT_OF_MEMORY`, not `CXF_ERROR_MEMORY`
- Check cxf_types.h for correct error codes

**Pattern: Stub extraction**
- Original functions in basis_stub.c for TDD
- Extract to dedicated file (eta_factors.c) when implementing
- Update stub file to reference new location
- Add new file to CMakeLists.txt

**Files modified:**
- `src/basis/basis_stub.c` - Removed duplicated functions
- `CMakeLists.txt` - Added eta_factors.c to build

---

## 2026-01-26: M6.1.1 Pricing Tests

### SUCCESS: TDD tests for pricing operations

**Files created:**
- `tests/unit/test_pricing.c` (310 LOC) - 24 comprehensive TDD tests
- `src/pricing/pricing_stub.c` (233 LOC) - Stub implementations for TDD

**Tests written (24 total):**
- PricingContext create/free (4 tests) - PASS
- cxf_pricing_init (4 tests) - PASS
- cxf_pricing_candidates (4 tests) - PASS
- cxf_pricing_steepest (4 tests) - PASS
- cxf_pricing_update (2 tests) - PASS
- cxf_pricing_invalidate (3 tests) - PASS
- cxf_pricing_step2 (2 tests) - PASS
- Statistics tracking (1 test) - PASS

**TDD pattern for pricing:**
- Tests define expected behavior for partial pricing, steepest edge, Dantzig
- Stubs implement working versions to enable test-first development
- Variable status codes: VAR_BASIC (>=0), VAR_AT_LOWER (-1), VAR_AT_UPPER (-2)
- Reduced cost attractiveness:
  - At lower bound: attractive if RC < -tolerance
  - At upper bound: attractive if RC > tolerance
- Steepest edge ratio: |RC| / sqrt(weight)

**Key design decisions:**
- PricingContext owns candidate_counts, candidate_arrays, cached_counts arrays
- Multi-level partial pricing support (max_levels typically 3-5)
- Cache invalidation via flags (INVALID_CANDIDATES, INVALID_WEIGHTS, etc.)
- Weights safeguard: zero/negative weights replaced with 1.0

**Files modified:**
- `tests/CMakeLists.txt` - Added test_pricing target
- `CMakeLists.txt` - Added pricing_stub.c to library

---

## 2026-01-26: M4.1.2 SparseMatrix Structure (Full)

### SUCCESS: CSC validation and CSR conversion implemented

**Files created:**
- `src/matrix/sparse_matrix.c` (171 LOC) - Full CSC/CSR implementation

**Files modified:**
- `CMakeLists.txt` - Added sparse_matrix.c to build

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

**TDD pattern:**
- sparse_stub.c retained for create/free/init_csc and M4.1.3/M4.1.4 stubs
- sparse_matrix.c adds validation and format conversion
- Both files linked - no duplicate symbols

---

## 2026-01-26: M5.1.2 BasisState Structure

### SUCCESS: BasisState lifecycle extracted to dedicated file

**Files created:**
- `src/basis/basis_state.c` (127 LOC) - BasisState lifecycle functions

**Files modified:**
- `src/basis/basis_stub.c` - Removed BasisState lifecycle (now in basis_state.c)
- `CMakeLists.txt` - Added basis_state.c to build

**Functions implemented:**
- `cxf_basis_create(m, n)` - Allocates BasisState with all arrays
- `cxf_basis_free(basis)` - Frees BasisState and eta linked list
- `cxf_basis_init(basis, m, n)` - Reinitializes existing BasisState

**TDD integration pattern:**
- Stub file (basis_stub.c) originally contained all functions for TDD
- As implementations are completed, extract to dedicated files
- Stub file retains only unimplemented functions (FTRAN, BTRAN, etc.)
- Both files linked in CMakeLists.txt; no duplicate symbols

**Key decisions:**
- Added negative dimension checks (m < 0 || n < 0 return NULL/error)
- Enhanced cxf_basis_init to clear arrays and validate dimension match
- DEFAULT_REFACTOR_FREQ = 100 (pivots between refactorizations)

---

## 2026-01-26: M5.1.1 Basis Tests

### SUCCESS: TDD tests for basis operations

**Files created:**
- `tests/unit/test_basis.c` (468 LOC) - Comprehensive TDD tests
- `src/basis/basis_stub.c` (337 LOC) - Stub implementations for TDD

**Tests written (29 total):**
- BasisState create/free/init (4 tests) - PASS (stub implements)
- EtaFactors create/free (4 tests) - PASS (stub implements)
- cxf_ftran FTRAN tests (4 tests) - PASS (identity basis stub)
- cxf_btran BTRAN tests (4 tests) - PASS (identity basis stub)
- cxf_basis_refactor tests (3 tests) - PASS (stub implements)
- Snapshot/comparison tests (6 tests) - PASS (stub implements)
- Validation/warm start tests (4 tests) - PASS (stub implements)

**TDD pattern difference from test_matrix:**
- Basis stubs implement identity basis behavior (trivial case)
- All tests pass with stubs because they test identity basis
- More complex tests (non-identity basis) will fail until full implementation
- This is still valid TDD - tests define expected behavior

**Key decisions:**
- Tests use explicit function declarations (not headers) for TDD
- Stubs handle identity basis case correctly (x = Ax when A = I)
- Validation tests check for duplicate basic variables
- Snapshot tests verify basis state can be saved/compared/restored

**Files modified:**
- `tests/CMakeLists.txt` - Added test_basis target
- `CMakeLists.txt` - Added basis_stub.c to library

---

## 2026-01-26: M4.1.1 Matrix Tests

### SUCCESS: TDD tests for matrix operations

**File created:** `tests/unit/test_matrix.c` (197 LOC)

**Tests written (20 total):**
- SparseMatrix creation/init/free (5 tests) - PASS (stub exists)
- cxf_matrix_multiply SpMV (4 tests) - FAIL (awaiting M4.1.3)
- cxf_dot_product dense/sparse (6 tests) - FAIL (awaiting M4.1.4)
- cxf_vector_norm L1/L2/L∞ (5 tests) - FAIL (awaiting M4.1.4)

**TDD pattern:**
- Added function stubs that return 0 so tests link and run
- Stubs live in sparse_stub.c until proper implementation
- Edge cases (zero vectors, orthogonal, empty sparse) pass by design

**Key decisions:**
- Tests use explicit function declarations (not headers) for TDD
- Tolerance of 1e-10 for floating-point comparisons
- Tests cover basic cases, edge cases, and accumulate modes

---

## 2026-01-26: M1.8 Tracer Bullet Benchmark

### SUCCESS: Benchmark established baseline performance

**File created:** `benchmarks/bench_tracer.c` (52 LOC)

**Performance achieved:**
- 0.114 us/iteration (well under 1000 us target)
- 10,000 iterations in ~1ms total

**Key decisions:**
- Used `clock()` from `<time.h>` for portable timing
- Return exit code 0 for PASS, 1 for SLOW (enables CI integration)
- Simple loop without warmup - benchmark is fast enough

**Lesson:** The tracer bullet stubs are highly optimized since they do minimal work. Full implementation will be slower but should still meet targets.

---

## 2026-01-26: M1.5 Stub Simplex Entry

### SUCCESS: Extracted cxf_solve_lp() to separate module

**File created:** `src/simplex/solve_lp_stub.c` (79 LOC)

**Refactoring done:**
- Extracted simplex logic from `api_stub.c` into `cxf_solve_lp()`
- `cxf_optimize()` now delegates to `cxf_solve_lp()`
- Proper separation of concerns: API layer vs solver layer

**Key decisions:**
- Added empty model handling (0 vars = optimal with obj=0)
- Forward declaration in api_stub.c instead of header (stub pattern)
- Full simplex implementation will replace this in M7

**Architecture benefit:** API layer now cleanly separated from solver logic.

---

## 2026-01-26: M1.3 Stub SparseMatrix Structure

### SUCCESS: SparseMatrix stub implementation created

**File created:** `src/matrix/sparse_stub.c` (103 LOC)

**Functions implemented:**
- `cxf_sparse_create()` - Allocates empty SparseMatrix
- `cxf_sparse_free(mat)` - Frees all arrays and structure
- `cxf_sparse_init_csc(mat, rows, cols, nnz)` - Initializes CSC arrays

**Key decisions:**
- Header `cxf_matrix.h` already had the structure defined from M0.4
- Stub provides basic create/free/init - full implementation in M4.1
- Follows same pattern as memory stubs (calloc for zero-init, NULL-safe free)
- CSR arrays remain NULL until explicitly built (lazy construction pattern)

**Build status:**
- New source added to CMakeLists.txt
- All 3 tests pass (smoke, memory, tracer bullet)

---

## 2026-01-26: M2.1.2 Memory Implementation - Full Implementation

### SUCCESS: Promoted stub to proper implementation

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

## 2026-01-26: M2.1.1 Memory Tests - TDD Tests Complete

### SUCCESS: 12 tests for memory management module

**File created:** `tests/unit/test_memory.c` (154 LOC)

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

**Also done:** Added `cxf_realloc` to stub (was missing from M1.6 stub).

**Key decisions:**
- Tests use external function declarations (not header include) since memory module doesn't have a dedicated header yet
- `cxf_realloc(ptr, 0)` frees memory and returns NULL (consistent with free-like behavior)
- Tests are grouped by function type with clear section headers

---

## 2026-01-26: M1.4 Stub API Functions - TRACER BULLET COMPLETE

### SUCCESS: Tracer bullet test passes end-to-end!

**File created:** `src/api/api_stub.c` (115 LOC)

**Functions implemented:**
- `cxf_optimize(model)` - Trivial unconstrained LP solver
- `cxf_getintattr(model, attrname, valueP)` - Gets Status, NumVars, NumConstrs
- `cxf_getdblattr(model, attrname, valueP)` - Gets ObjVal

**Trivial solver algorithm:**
- For minimization: if obj_coeff >= 0, use lb; else use ub
- Computes objective value as sum of coeff * solution
- Sets status to CXF_OPTIMAL (or CXF_UNBOUNDED if bound is infinity)

**Milestone achieved:**
- `test_tracer_bullet` passes: solves `min x s.t. x >= 0` → x*=0, obj*=0
- End-to-end architecture proven: API → Solver → Solution extraction

---

## 2026-01-26: M1.2 Stub CxfModel Structure Created

### SUCCESS: Model stubs for tracer bullet

**File created:** `src/api/model_stub.c` (129 LOC)

**Functions implemented:**
- `cxf_newmodel(env, modelP, name)` - Allocates model with initial variable arrays
- `cxf_freemodel(model)` - Frees model and all arrays
- `cxf_addvar(model, lb, ub, obj, vtype, name)` - Adds variable to arrays

**Key decisions:**
- Pre-allocate arrays with INITIAL_VAR_CAPACITY (16) to avoid realloc in stub
- vtype and variable names ignored in stub
- solution array initialized to 0.0

**Build status:**
- Model stubs compile successfully
- Linker errors reduced to 3 (cxf_optimize, cxf_getintattr, cxf_getdblattr)

---

## 2026-01-26: M1.1 Stub CxfEnv Structure Created

### SUCCESS: Environment stubs for tracer bullet

**File created:** `src/api/env_stub.c` (74 LOC)

**Functions implemented:**
- `cxf_loadenv(envP, logfilename)` - Allocates and initializes CxfEnv with defaults
- `cxf_freeenv(env)` - Frees environment memory

**Key decisions:**
- Uses cxf_calloc for zero-initialized allocation
- Sets magic number for validation (CXF_ENV_MAGIC = 0xC0FEFE1D)
- Initializes tolerances to defaults (CXF_FEASIBILITY_TOL, CXF_OPTIMALITY_TOL)
- logfilename parameter ignored in stub (logging comes later)

**Build status:**
- Env stubs compile successfully
- Linker errors reduced to 6 (remaining M1.2-M1.5 stubs needed)

---

## 2026-01-26: M1.7 Stub Error Functions Created

### SUCCESS: Error handling stubs for tracer bullet

**File created:** `src/error/error_stub.c` (48 LOC)

**Functions implemented:**
- `cxf_error(env, format, ...)` - Formats and stores error message in env->error_buffer using vsnprintf
- `cxf_geterrormsg(env)` - Returns error_buffer contents

**Key decisions:**
- Stub omits thread safety (no critical section locking) - full implementation later
- NULL env check returns early (no crash)
- Defensive null termination at buffer[511]

**Build status:**
- Error stubs compile successfully into libconvexfeld.a
- Smoke tests pass
- Remaining linker errors for API functions (M1.1-M1.5)

---

## 2026-01-26: M1.0 Tracer Bullet Test Created

### SUCCESS: Integration test written following TDD

**File created:** `tests/integration/test_tracer_bullet.c` (87 LOC)

**What was done:**
1. Created `tests/integration/` directory
2. Wrote the tracer bullet test exercising all 8 API functions
3. Updated `tests/CMakeLists.txt` to include the integration test
4. Verified test compiles successfully (linker errors expected since implementations don't exist yet)

**Key TDD insight:**
- Test compiles to `.o` but fails to link - this is CORRECT for TDD
- All 8 undefined references (cxf_loadenv, cxf_newmodel, cxf_addvar, cxf_optimize, cxf_getintattr, cxf_getdblattr, cxf_freemodel, cxf_freeenv) prove the test is properly calling the API
- Implementations will be added in M1.1-M1.7

**Test coverage:**
- Problem: `min x subject to x >= 0`
- Expected: `x* = 0`, `obj* = 0`
- Uses `TEST_ASSERT_DOUBLE_WITHIN` for floating-point tolerance

---

## 2026-01-26: M1.6 Stub Memory Functions Created

### SUCCESS: Simple memory wrappers for tracer bullet

**File created:** `src/memory/alloc_stub.c` (44 LOC)

**Functions implemented:**
- `cxf_malloc(size)` - wraps malloc, returns NULL for size=0
- `cxf_calloc(count, size)` - wraps calloc, returns NULL for count=0 or size=0
- `cxf_free(ptr)` - wraps free, NULL-safe

**Key decisions:**
- Used simple signatures (no env parameter) for stubs
- Full implementation with env tracking will come in M2.1
- Removed placeholder.c - no longer needed

**Build status:**
- Memory stubs compile successfully into libconvexfeld.a
- Remaining linker errors are for API functions (M1.1-M1.5, M1.7)

---

## 2026-01-26: M0.4 Module Headers (Stubs) Created

### SUCCESS: All 8 structure headers created

**Headers created (~280 LOC total):**
- `cxf_env.h` - CxfEnv structure (environment)
- `cxf_model.h` - CxfModel structure (problem instance)
- `cxf_matrix.h` - SparseMatrix structure (CSC/CSR format)
- `cxf_solver.h` - SolverContext structure (runtime state)
- `cxf_basis.h` - BasisState + EtaFactors (basis representation)
- `cxf_pricing.h` - PricingContext (partial pricing)
- `cxf_callback.h` - CallbackContext + CxfCallbackFunc typedef
- `convexfeld.h` - Main API header (includes all others)

**Key design decisions:**
- All headers include only `cxf_types.h` (forward declarations there)
- Minimal fields for tracer bullet, full fields to be added later
- Function declarations in structure headers (e.g., `cxf_loadenv` in `cxf_env.h`)
- Version macros in `convexfeld.h`

**M0 Complete!** Ready to start M1 (Tracer Bullet).

---

## 2026-01-26: M0.3 Unity Test Framework Setup

### SUCCESS: Unity installed and verified with smoke test

**Setup:**
- Downloaded unity.c, unity.h, unity_internals.h from ThrowTheSwitch/Unity
- Updated tests/CMakeLists.txt with Unity library and `add_cxf_test()` helper
- Created smoke test with 3 assertions (true, int equality, double within)

**Configuration:**
- `UNITY_INCLUDE_DOUBLE` - enables double precision assertions
- `UNITY_DOUBLE_PRECISION=1e-12` - sets tolerance for double comparisons

**Note:** Unity's own code has `-Wdouble-promotion` warnings (float→double), but this is expected from third-party code and doesn't affect functionality.

---

## 2026-01-26: M0.2 Core Types Header Created

### SUCCESS: cxf_types.h created with all enums, constants, and forward declarations

**Contents (~170 LOC):**
- `CxfStatus` enum - 8 success codes + 4 error codes
- `CxfVarType` enum - 5 variable types (C, B, I, S, N)
- `CxfSense` enum - 3 constraint senses (<, >, =)
- `CxfObjSense` enum - minimize/maximize
- `CxfVarStatus` enum - 5 basis status values
- Constants: CXF_INFINITY, tolerances, CXF_MAX_NAME_LEN
- Magic numbers: CXF_ENV_MAGIC, CXF_MODEL_MAGIC, CXF_CALLBACK_MAGIC/2
- Forward declarations for all 8 structures

**Note:** Spec file path in issue was wrong (`docs/specs/arch/` vs `docs/specs/architecture/`), but implementation plan had exact code.

---

## 2026-01-26: M0.1 CMakeLists.txt Created

### SUCCESS: Project build system established

**What worked:**
1. Followed the implementation plan exactly for Step 0.1
2. Created root CMakeLists.txt with:
   - CMake 3.16+ requirement
   - C99 standard enforcement (CMAKE_C_STANDARD 99, CMAKE_C_STANDARD_REQUIRED ON)
   - Compiler flags: -Wall -Wextra -Wpedantic -O2 plus additional warnings
   - Static library target with proper include directories
   - Conditional tests and benchmarks subdirectories
3. Created tests/CMakeLists.txt and benchmarks/CMakeLists.txt stubs
4. Added placeholder.c so library builds (CMake requires at least one source file)

**Build verification:**
- `cmake ..` succeeded
- `cmake --build .` produced `libconvexfeld.a`

**Gotcha discovered:**
- CMake STATIC libraries require at least one source file
- Created src/placeholder.c as a workaround until real modules added

---

## 2026-01-26: Beads Issues Created Successfully

### SUCCESS: All 122 Implementation Issues Created

Created beads issues for every step in the implementation plan using parallel subagents.

**What worked:**
1. Read HANDOFF.md first - understood the task was ONLY issue creation
2. Spawned 9 parallel subagents (one per milestone) for efficiency
3. Each issue includes:
   - Title matching plan step (e.g., "M0.1: Create CMakeLists.txt")
   - Detailed description with file paths and LOC estimates
   - Spec file references (e.g., "Spec: docs/specs/functions/memory/cxf_malloc.md")
   - Priority based on milestone (M0=P0, M1=P1, M2-M8=P2)

**Issue breakdown:**
- M0: 4 issues (P0 - critical, must complete first)
- M1: 9 issues (P1 - tracer bullet proves architecture)
- M2-M8: 109 issues (P2 - main implementation)

**Lesson: Parallel subagents are efficient for bulk issue creation. Each agent handles one milestone independently.**

---

## 2026-01-25: Agent Started Implementing Instead of Creating Issues

### CRITICAL FAILURE: Jumped to Implementation Without Creating Beads Issues

**FAILURE: Agent started writing code (CMakeLists.txt, headers, etc.) instead of creating beads issues for each step in the implementation plan.**

- The task was ONLY to create beads issues for each step in the plan
- Agent misread the workflow and started implementing M0
- Created ~10 files that had to be deleted
- User had to intervene forcefully

**Lesson: After a plan is complete, the NEXT STEP is to create beads issues for tracking. DO NOT START IMPLEMENTING until issues exist for every step.**

**Correct Workflow:**
1. Plan is written ✓
2. Create beads issues for EVERY step in the plan ← NEXT STEP
3. THEN start implementing (claiming issues one at a time)

---

## 2026-01-25: C99 Implementation Plan Complete

### SUCCESS: Implementation Plan Rewritten for C99

After 3 failed attempts by previous agents (all wrote for Rust), this session successfully rewrote the plan for C99.

**What worked:**
1. Read HANDOFF.md first to understand the critical error
2. Spawned 5 parallel research agents to gather all spec details
3. Read all 8 structure specs directly to get C99 field types
4. Systematically mapped all 142 functions to their spec files
5. Included concrete C99 code examples throughout

---

## 2025-01-25: Initial Setup (Previous Session)

### CRITICAL ERROR: Wrong Language

**FAILURE: Implementation plan written for Rust instead of C99.**

- The PRD explicitly states C99
- Agent failed to read the PRD carefully
- Entire implementation plan had wrong file structure, syntax, tooling
- Required full rewrite

**Lesson: ALWAYS verify the target language from PRD before writing ANY implementation details.**

---

### Planning Phase Learnings

1. **Previous planning attempts failed** because they didn't:
   - Map every function to its spec file
   - Include explicit checklists for all 142 functions
   - Define a tracer bullet milestone to prove architecture
   - Specify parallelization strategy
   - **Use the correct language (C99)**

2. **Tracer bullet pattern is critical**
   - Prove end-to-end works before investing in full implementation
   - A 1-variable LP test exercises: API -> Simplex -> Solution extraction

3. **File size discipline**
   - 200 LOC limit prevents complexity accumulation
   - Forces proper decomposition from the start

4. **Spec structure**
   - 142 functions across 17 modules in 6 layers
   - 8 core data structures
   - Implementation language: C99

5. **Parallel research is efficient**
   - Spawning multiple research agents simultaneously speeds discovery
   - Agent results can be combined for comprehensive understanding

---

## Gotchas

### C99 Specific

1. **Magic number validation** - Use `uint32_t` for 32-bit magic, `uint64_t` for 64-bit
2. **Forward declarations** - Required for circular struct references
3. **Header guards** - Essential for all `.h` files: `#ifndef FOO_H / #define FOO_H / #endif`
4. **Include order** - Standard headers first, then project headers

### Build System

1. **CMake 3.16+** - Required for modern C99 support and Unity integration
2. **Unity test framework** - Lightweight, C-native, no external dependencies

---

## Useful Patterns

### C99 Structure Definition Pattern
```c
struct CxfEnv {
    uint32_t magic;           /* 0xC0FEFE1D for validation */
    int active;               /* 1 if environment is active */
    char error_buffer[512];   /* Last error message */
    /* ... fields ... */
};
```

### Unity Test Pattern
```c
#include "unity.h"
void setUp(void) {}
void tearDown(void) {}
void test_function_name(void) {
    /* Test code */
    TEST_ASSERT_EQUAL_INT(expected, actual);
}
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_function_name);
    return UNITY_END();
}
```

---

## Things That Didn't Work

1. **Writing implementation plan in Rust when spec says C99** - Major failure, caused 3 agent resets
2. **Not reading HANDOFF.md first** - Agents repeated same mistakes
3. **Not consulting PRD for language requirement** - Root cause of Rust error
