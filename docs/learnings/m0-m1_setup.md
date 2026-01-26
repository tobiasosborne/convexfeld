# M0-M1: Project Setup & Tracer Bullet Learnings

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

## 2026-01-26: M1.1-M1.7 Stub Implementations

### M1.1: Stub CxfEnv Structure
**File created:** `src/api/env_stub.c` (74 LOC)
- `cxf_loadenv()` - Allocates and initializes CxfEnv with defaults
- `cxf_freeenv()` - Frees environment memory
- Uses cxf_calloc for zero-initialized allocation
- Sets magic number for validation (CXF_ENV_MAGIC = 0xC0FEFE1D)
- logfilename parameter ignored in stub (logging comes later)

### M1.2: Stub CxfModel Structure
**File created:** `src/api/model_stub.c` (129 LOC)
- `cxf_newmodel()` - Allocates model with initial variable arrays
- `cxf_freemodel()` - Frees model and all arrays
- `cxf_addvar()` - Adds variable to arrays
- Pre-allocate arrays with INITIAL_VAR_CAPACITY (16) to avoid realloc in stub

### M1.3: Stub SparseMatrix Structure
**File created:** `src/matrix/sparse_stub.c` (103 LOC)
- `cxf_sparse_create()` - Allocates empty SparseMatrix
- `cxf_sparse_free()` - Frees all arrays and structure
- `cxf_sparse_init_csc()` - Initializes CSC arrays
- CSR arrays remain NULL until explicitly built (lazy construction pattern)

### M1.4: Stub API Functions
**File created:** `src/api/api_stub.c` (115 LOC)
- `cxf_optimize()` - Trivial unconstrained LP solver
- `cxf_getintattr()` - Gets Status, NumVars, NumConstrs
- `cxf_getdblattr()` - Gets ObjVal
- Trivial solver: if obj_coeff >= 0, use lb; else use ub

### M1.5: Stub Simplex Entry
**File created:** `src/simplex/solve_lp_stub.c` (79 LOC)
- Extracted simplex logic from `api_stub.c` into `cxf_solve_lp()`
- Added empty model handling (0 vars = optimal with obj=0)
- Forward declaration in api_stub.c instead of header (stub pattern)

### M1.6: Stub Memory Functions
**File created:** `src/memory/alloc_stub.c` (44 LOC)
- `cxf_malloc()` - wraps malloc, returns NULL for size=0
- `cxf_calloc()` - wraps calloc, returns NULL for count=0 or size=0
- `cxf_free()` - wraps free, NULL-safe

### M1.7: Stub Error Functions
**File created:** `src/error/error_stub.c` (48 LOC)
- `cxf_error()` - Formats and stores error message using vsnprintf
- `cxf_geterrormsg()` - Returns error_buffer contents
- NULL env check returns early (no crash)

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

**MILESTONE 1 COMPLETE:** Tracer bullet test passes end-to-end!
