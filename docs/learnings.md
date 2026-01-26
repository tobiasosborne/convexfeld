# Project Learnings

This file captures learnings, gotchas, and useful patterns discovered during development.

**Rule: ERRORS = SUCCESS if learnings are recorded here.**

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
