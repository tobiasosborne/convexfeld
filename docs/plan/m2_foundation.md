# Milestone 2: Foundation Layer (Level 0)

**Goal:** Complete Memory, Parameters, Validation modules
**Parallelizable:** All three modules can run in parallel
**Spec References:**
- `docs/specs/modules/01_memory.md`
- `docs/specs/modules/04_parameters.md`
- `docs/specs/modules/05_validation.md`

---

## Module: Memory Management (9 functions)

**Spec Directory:** `docs/specs/functions/memory/`

### Step 2.1.1: Memory Tests

**LOC:** ~150
**File:** `tests/unit/test_memory.c`

Tests for: malloc, calloc, realloc, free, vector_free, alloc_eta, state deallocators

### Step 2.1.2: cxf_malloc, cxf_calloc, cxf_realloc, cxf_free

**LOC:** ~100
**File:** `src/memory/alloc.c`
**Specs:**
- `docs/specs/functions/memory/cxf_malloc.md`
- `docs/specs/functions/memory/cxf_calloc.md`
- `docs/specs/functions/memory/cxf_realloc.md`
- `docs/specs/functions/memory/cxf_free.md`

### Step 2.1.3: cxf_vector_free, cxf_alloc_eta

**LOC:** ~100
**File:** `src/memory/vectors.c`
**Specs:**
- `docs/specs/functions/memory/cxf_vector_free.md`
- `docs/specs/functions/memory/cxf_alloc_eta.md`

### Step 2.1.4: State Deallocators

**LOC:** ~120
**File:** `src/memory/state_cleanup.c`
**Specs:**
- `docs/specs/functions/memory/cxf_free_solver_state.md`
- `docs/specs/functions/memory/cxf_free_basis_state.md`
- `docs/specs/functions/memory/cxf_free_callback_state.md`

---

## Module: Parameters (4 functions)

**Spec Directory:** `docs/specs/functions/parameters/`

### Step 2.2.1: Parameters Tests

**LOC:** ~80
**File:** `tests/unit/test_parameters.c`

### Step 2.2.2: Parameter Getters

**LOC:** ~100
**File:** `src/parameters/getters.c`
**Specs:**
- `docs/specs/functions/parameters/cxf_getdblparam.md`
- `docs/specs/functions/parameters/cxf_get_feasibility_tol.md`
- `docs/specs/functions/parameters/cxf_get_optimality_tol.md`
- `docs/specs/functions/parameters/cxf_get_infinity.md`

---

## Module: Validation (2 functions)

**Spec Directory:** `docs/specs/functions/validation/`

### Step 2.3.1: Validation Tests

**LOC:** ~100
**File:** `tests/unit/test_validation.c`

### Step 2.3.2: Array Validation

**LOC:** ~80
**File:** `src/validation/arrays.c`
**Specs:**
- `docs/specs/functions/validation/cxf_validate_array.md`
- `docs/specs/functions/validation/cxf_validate_vartypes.md`
