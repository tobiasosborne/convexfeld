# Milestone 5: Core Operations (Level 3)

**Goal:** Complete Basis, Callbacks, Solver State modules
**Parallelizable:** All three modules can run in parallel
**Spec References:**
- `docs/specs/modules/07_basis.md`
- `docs/specs/modules/13_callbacks.md`
- `docs/specs/modules/10_solver_state.md`

---

## Module: Basis Operations (8 functions)

**Spec Directory:** `docs/specs/functions/basis/`
**Structure Specs:** `docs/specs/structures/basis_state.md`, `docs/specs/structures/eta_factors.md`

### Step 5.1.1: Basis Tests

**LOC:** ~250
**File:** `tests/unit/test_basis.c`

Comprehensive tests for FTRAN, BTRAN, refactorization.

### Step 5.1.2: BasisState Structure

**LOC:** ~150
**Files:** `include/convexfeld/cxf_basis.h`, `src/basis/basis_state.c`
**Spec:** `docs/specs/structures/basis_state.md`

### Step 5.1.3: EtaFactors Structure

**LOC:** ~100
**File:** `src/basis/eta_factors.c`
**Spec:** `docs/specs/structures/eta_factors.md`

### Step 5.1.4: cxf_ftran

**LOC:** ~150
**File:** `src/basis/ftran.c`
**Spec:** `docs/specs/functions/basis/cxf_ftran.md`

Forward transformation: solve Bx = b using eta representation.

### Step 5.1.5: cxf_btran

**LOC:** ~150
**File:** `src/basis/btran.c`
**Spec:** `docs/specs/functions/basis/cxf_btran.md`

Backward transformation: solve y^T B = c^T.

### Step 5.1.6: cxf_basis_refactor

**LOC:** ~200
**File:** `src/basis/refactor.c`
**Spec:** `docs/specs/functions/basis/cxf_basis_refactor.md`

LU factorization of basis matrix.

### Step 5.1.7: Basis Snapshots

**LOC:** ~120
**File:** `src/basis/snapshot.c`
**Specs:**
- `docs/specs/functions/basis/cxf_basis_snapshot.md`
- `docs/specs/functions/basis/cxf_basis_diff.md`
- `docs/specs/functions/basis/cxf_basis_equal.md`

### Step 5.1.8: Basis Validation/Warm Start

**LOC:** ~100
**File:** `src/basis/warm.c`
**Specs:**
- `docs/specs/functions/basis/cxf_basis_validate.md`
- `docs/specs/functions/basis/cxf_basis_warm.md`

---

## Module: Callbacks (6 functions)

**Spec Directory:** `docs/specs/functions/callbacks/`
**Structure Spec:** `docs/specs/structures/callback_context.md`

### Step 5.2.1: Callbacks Tests

**LOC:** ~120
**File:** `tests/unit/test_callbacks.c`

### Step 5.2.2: CallbackContext Structure

**LOC:** ~100
**Files:** `include/convexfeld/cxf_callback.h`, `src/callbacks/context.c`
**Spec:** `docs/specs/structures/callback_context.md`

### Step 5.2.3: Callback Initialization

**LOC:** ~80
**File:** `src/callbacks/init.c`
**Specs:**
- `docs/specs/functions/callbacks/cxf_init_callback_struct.md`
- `docs/specs/functions/callbacks/cxf_reset_callback_state.md`

### Step 5.2.4: Callback Invocation

**LOC:** ~100
**File:** `src/callbacks/invoke.c`
**Specs:**
- `docs/specs/functions/callbacks/cxf_pre_optimize_callback.md`
- `docs/specs/functions/callbacks/cxf_post_optimize_callback.md`

### Step 5.2.5: Termination Handling

**LOC:** ~80
**File:** `src/callbacks/terminate.c`
**Specs:**
- `docs/specs/functions/callbacks/cxf_callback_terminate.md`
- `docs/specs/functions/callbacks/cxf_set_terminate.md`

---

## Module: Solver State (4 functions)

**Spec Directory:** `docs/specs/functions/memory/`, `docs/specs/functions/utilities/`
**Structure Spec:** `docs/specs/structures/solver_context.md`

### Step 5.3.1: Solver State Tests

**LOC:** ~100
**File:** `tests/unit/test_solver_state.c`

### Step 5.3.2: SolverContext Structure

**LOC:** ~150
**Files:** `include/convexfeld/cxf_solver.h`, `src/solver_state/context.c`
**Spec:** `docs/specs/structures/solver_context.md`

### Step 5.3.3: State Initialization

**LOC:** ~100
**File:** `src/solver_state/init.c`
**Specs:**
- `docs/specs/functions/memory/cxf_init_solve_state.md`
- `docs/specs/functions/memory/cxf_cleanup_solve_state.md`

### Step 5.3.4: Helper Functions

**LOC:** ~80
**File:** `src/solver_state/helpers.c`
**Spec:** `docs/specs/functions/utilities/cxf_cleanup_helper.md`

### Step 5.3.5: Solution Extraction

**LOC:** ~100
**File:** `src/solver_state/extract.c`
**Spec:** `docs/specs/functions/utilities/cxf_extract_solution.md`
