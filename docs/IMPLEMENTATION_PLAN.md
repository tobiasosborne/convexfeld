# ConvexFeld LP Solver - Implementation Plan

**Total Functions:** 142 (112 internal + 30 API)
**Total Modules:** 17 across 6 layers
**Total Structures:** 8 core data structures
**Target:** 100-200 LOC per file, maximum parallelization, TDD approach

---

## Table of Contents

1. [Overview](#overview)
2. [Milestone 0: Project Setup](#milestone-0-project-setup)
3. [Milestone 1: Tracer Bullet](#milestone-1-tracer-bullet)
4. [Milestone 2: Foundation Layer](#milestone-2-foundation-layer)
5. [Milestone 3: Data Layer](#milestone-3-data-layer)
6. [Milestone 4: Core Operations](#milestone-4-core-operations)
7. [Milestone 5: Algorithm Components](#milestone-5-algorithm-components)
8. [Milestone 6: Simplex Core](#milestone-6-simplex-core)
9. [Milestone 7: Public API](#milestone-7-public-api)
10. [Benchmarks](#benchmarks)
11. [Function Checklist](#function-checklist)

---

## Overview

### Architecture Layers
```
Layer 6: Model API (30 functions)
Layer 5: Simplex Core (21), Crossover (2), Utilities (10)
Layer 4: Pricing (6), Solver State (4)
Layer 3: Basis (8), Callbacks (6), Model Analysis (6), Timing (5)
Layer 2: Matrix (7), Threading (7), Logging (5)
Layer 1: Error Handling (10), Validation (2)
Layer 0: Memory (9), Parameters (4)
```

### Key Constraints
- **TDD Required:** Tests written BEFORE implementation
- **Test:Code Ratio:** 1:3 to 1:4
- **File Size:** 100-200 LOC max per file
- **Parallelization:** Steps within same milestone can run concurrently

---

## Milestone 0: Project Setup

**Goal:** Initialize Rust project structure
**Parallelizable:** No (must complete first)

### Step 0.1: Create Cargo.toml
**LOC:** ~50
**File:** `Cargo.toml`

```toml
[package]
name = "convexfeld"
version = "0.1.0"
edition = "2021"

[dependencies]

[dev-dependencies]
criterion = "0.5"

[[bench]]
name = "benchmarks"
harness = false
```

### Step 0.2: Create Module Structure
**LOC:** ~80
**File:** `src/lib.rs`

Create module declarations for all 17 modules:
- `mod memory;`
- `mod parameters;`
- `mod validation;`
- `mod error;`
- `mod logging;`
- `mod threading;`
- `mod matrix;`
- `mod timing;`
- `mod analysis;`
- `mod basis;`
- `mod callbacks;`
- `mod solver_state;`
- `mod pricing;`
- `mod simplex;`
- `mod crossover;`
- `mod utilities;`
- `mod api;`
- `mod types;` (shared types/constants)

### Step 0.3: Create Type Definitions
**LOC:** ~150
**File:** `src/types.rs`

Define shared types, constants, and error codes:
- `CxfStatus` enum (OK, OPTIMAL, INFEASIBLE, UNBOUNDED, etc.)
- `CxfError` type
- Constants: `CXF_INFINITY`, tolerances
- `Result<T>` type alias

---

## Milestone 1: Tracer Bullet

**Goal:** Minimal end-to-end test from API to simplex core
**Parallelizable:** Steps 1.1-1.8 after Step 1.0

This milestone proves every layer works together by solving a trivial 1-variable LP.

### Step 1.0: Tracer Bullet Test (FIRST)
**LOC:** ~100
**File:** `tests/tracer_bullet.rs`
**Spec:** End-to-end integration test

```rust
#[test]
fn test_tracer_bullet_1var_lp() {
    // Solve: min x subject to x >= 0
    // Expected: x* = 0, obj* = 0
    let env = cxf_loadenv().unwrap();
    let model = cxf_newmodel(&env, "tracer").unwrap();
    cxf_addvar(&model, 0.0, f64::INFINITY, 1.0, 'C', "x").unwrap();
    cxf_optimize(&model).unwrap();
    assert_eq!(cxf_getintattr(&model, "Status"), CXF_OPTIMAL);
    assert!((cxf_getdblattr(&model, "ObjVal") - 0.0).abs() < 1e-6);
}
```

### Step 1.1: Stub CxfEnv Structure
**LOC:** ~80
**File:** `src/types/env.rs`
**Spec:** `docs/specs/structures/cxf_env.md`

Minimal CxfEnv with:
- `active: bool`
- `error_buffer: String`
- `parameters: HashMap<String, f64>`

### Step 1.2: Stub CxfModel Structure
**LOC:** ~100
**File:** `src/types/model.rs`
**Spec:** `docs/specs/structures/cxf_model.md`

Minimal CxfModel with:
- `env: Arc<CxfEnv>`
- `num_vars: usize`
- `obj_coeffs: Vec<f64>`
- `lb: Vec<f64>`
- `ub: Vec<f64>`
- `solution: Option<Vec<f64>>`
- `status: CxfStatus`

### Step 1.3: Stub SparseMatrix Structure
**LOC:** ~80
**File:** `src/types/sparse_matrix.rs`
**Spec:** `docs/specs/structures/sparse_matrix.md`

Minimal CSC format:
- `col_ptr: Vec<usize>`
- `row_idx: Vec<usize>`
- `values: Vec<f64>`
- `num_rows: usize`
- `num_cols: usize`

### Step 1.4: Stub API Layer
**LOC:** ~120
**File:** `src/api/stub.rs`
**Spec:** `docs/specs/modules/17_model_api.md`

Stub implementations:
- `cxf_loadenv()` → returns stub CxfEnv
- `cxf_newmodel()` → returns stub CxfModel
- `cxf_addvar()` → adds to model vectors
- `cxf_optimize()` → calls simplex stub
- `cxf_getintattr()` → returns status
- `cxf_getdblattr()` → returns objective

### Step 1.5: Stub Simplex Entry
**LOC:** ~80
**File:** `src/simplex/stub.rs`
**Spec:** `docs/specs/functions/simplex/cxf_solve_lp.md`

Stub `cxf_solve_lp()` that:
- For 1-var unconstrained: returns x=lb, obj=c*lb
- Sets status to OPTIMAL

### Step 1.6: Stub Memory Functions
**LOC:** ~60
**File:** `src/memory/stub.rs`
**Spec:** `docs/specs/modules/01_memory.md`

Rust-native allocation (no tracking yet):
- `cxf_malloc()` → `Vec::with_capacity()`
- `cxf_free()` → drop

### Step 1.7: Stub Error Handling
**LOC:** ~60
**File:** `src/error/stub.rs`
**Spec:** `docs/specs/functions/error_logging/cxf_error.md`

- `cxf_error()` → sets error_buffer
- `cxf_geterrormsg()` → returns error_buffer

### Step 1.8: Tracer Bullet Benchmark
**LOC:** ~50
**File:** `benches/tracer_bullet.rs`

Criterion benchmark for tracer bullet solve time.

**Milestone 1 Complete When:** `cargo test test_tracer_bullet_1var_lp` passes

---

## Milestone 2: Foundation Layer (Level 0-1)

**Goal:** Complete Memory, Parameters, Validation, Error modules
**Parallelizable:** All steps within each module can run in parallel

### Module: Memory Management (9 functions)
**Spec Directory:** `docs/specs/functions/memory/`

#### Step 2.1.1: Memory Tests
**LOC:** ~150
**File:** `tests/unit/memory_test.rs`

Tests for all 9 memory functions:
- `test_cxf_malloc_basic`
- `test_cxf_malloc_zero_size`
- `test_cxf_calloc_zeroed`
- `test_cxf_realloc_grow`
- `test_cxf_realloc_shrink`
- `test_cxf_free_null_safe`
- `test_cxf_vector_free`
- `test_cxf_alloc_eta`
- `test_cxf_free_solver_state`
- `test_cxf_free_basis_state`
- `test_cxf_free_callback_state`

#### Step 2.1.2: cxf_malloc, cxf_free
**LOC:** ~100
**File:** `src/memory/alloc.rs`
**Spec:** `docs/specs/functions/memory/cxf_malloc.md`, `cxf_free.md`

#### Step 2.1.3: cxf_calloc, cxf_realloc
**LOC:** ~80
**File:** `src/memory/realloc.rs`
**Spec:** `docs/specs/functions/memory/cxf_calloc.md`, `cxf_realloc.md`

#### Step 2.1.4: cxf_vector_free, cxf_alloc_eta
**LOC:** ~100
**File:** `src/memory/vectors.rs`
**Spec:** `docs/specs/functions/memory/cxf_vector_free.md`, `cxf_alloc_eta.md`

#### Step 2.1.5: State Deallocators
**LOC:** ~120
**File:** `src/memory/state_cleanup.rs`
**Spec:** `docs/specs/functions/memory/cxf_free_solver_state.md`, `cxf_free_basis_state.md`, `cxf_free_callback_state.md`

---

### Module: Parameters (4 functions)
**Spec Directory:** `docs/specs/functions/parameters/`

#### Step 2.2.1: Parameters Tests
**LOC:** ~80
**File:** `tests/unit/parameters_test.rs`

Tests:
- `test_cxf_getdblparam_valid`
- `test_cxf_getdblparam_invalid`
- `test_cxf_get_feasibility_tol`
- `test_cxf_get_optimality_tol`
- `test_cxf_get_infinity`

#### Step 2.2.2: Parameter Getters
**LOC:** ~100
**File:** `src/parameters/getters.rs`
**Spec:** `docs/specs/functions/parameters/cxf_getdblparam.md`, `cxf_get_feasibility_tol.md`, `cxf_get_optimality_tol.md`, `cxf_get_infinity.md`

---

### Module: Validation (2 functions)
**Spec Directory:** `docs/specs/functions/validation/`

#### Step 2.3.1: Validation Tests
**LOC:** ~100
**File:** `tests/unit/validation_test.rs`

Tests:
- `test_cxf_validate_array_valid`
- `test_cxf_validate_array_nan`
- `test_cxf_validate_array_inf`
- `test_cxf_validate_array_empty`
- `test_cxf_validate_vartypes_valid`
- `test_cxf_validate_vartypes_invalid`

#### Step 2.3.2: Array Validation
**LOC:** ~80
**File:** `src/validation/arrays.rs`
**Spec:** `docs/specs/functions/validation/cxf_validate_array.md`, `cxf_validate_vartypes.md`

---

### Module: Error Handling (10 functions)
**Spec Directory:** `docs/specs/functions/error_logging/`, `docs/specs/functions/validation/`

#### Step 2.4.1: Error Tests
**LOC:** ~150
**File:** `tests/unit/error_test.rs`

Tests for all 10 functions.

#### Step 2.4.2: Core Error Functions
**LOC:** ~100
**File:** `src/error/core.rs`
**Spec:** `docs/specs/functions/error_logging/cxf_error.md`, `cxf_errorlog.md`

#### Step 2.4.3: NaN/Inf Detection
**LOC:** ~80
**File:** `src/error/nan_check.rs`
**Spec:** `docs/specs/functions/validation/cxf_check_nan.md`, `cxf_check_nan_or_inf.md`

#### Step 2.4.4: Environment Validation
**LOC:** ~60
**File:** `src/error/env_check.rs`
**Spec:** `docs/specs/functions/validation/cxf_checkenv.md`

#### Step 2.4.5: Model Flag Checks
**LOC:** ~100
**File:** `src/error/model_flags.rs`
**Spec:** `docs/specs/functions/validation/cxf_check_model_flags1.md`, `cxf_check_model_flags2.md`

#### Step 2.4.6: Termination Check
**LOC:** ~60
**File:** `src/error/terminate.rs`
**Spec:** `docs/specs/functions/callbacks/cxf_check_terminate.md`

#### Step 2.4.7: Pivot Validation
**LOC:** ~80
**File:** `src/error/pivot_check.rs`
**Spec:** `docs/specs/functions/ratio_test/cxf_pivot_check.md`, `docs/specs/functions/statistics/cxf_special_check.md`

---

## Milestone 3: Data Layer (Level 2)

**Goal:** Complete Matrix, Threading, Logging, Timing modules
**Parallelizable:** All modules can run in parallel

### Module: Matrix Operations (7 functions)
**Spec Directory:** `docs/specs/functions/matrix/`

#### Step 3.1.1: Matrix Tests
**LOC:** ~200
**File:** `tests/unit/matrix_test.rs`

Tests for all 7 functions including edge cases.

#### Step 3.1.2: SparseMatrix Structure (Full)
**LOC:** ~150
**File:** `src/types/sparse_matrix.rs` (expand from stub)
**Spec:** `docs/specs/structures/sparse_matrix.md`

Full CSC/CSR implementation.

#### Step 3.1.3: cxf_matrix_multiply
**LOC:** ~100
**File:** `src/matrix/multiply.rs`
**Spec:** `docs/specs/functions/matrix/cxf_matrix_multiply.md`

#### Step 3.1.4: cxf_dot_product, cxf_vector_norm
**LOC:** ~100
**File:** `src/matrix/vectors.rs`
**Spec:** `docs/specs/functions/matrix/cxf_dot_product.md`, `cxf_vector_norm.md`

#### Step 3.1.5: Row-Major Conversion
**LOC:** ~150
**File:** `src/matrix/row_major.rs`
**Spec:** `docs/specs/functions/matrix/cxf_build_row_major.md`, `cxf_prepare_row_data.md`, `cxf_finalize_row_data.md`

#### Step 3.1.6: cxf_sort_indices
**LOC:** ~80
**File:** `src/matrix/sort.rs`
**Spec:** `docs/specs/functions/matrix/cxf_sort_indices.md`

---

### Module: Threading (7 functions)
**Spec Directory:** `docs/specs/functions/threading/`

#### Step 3.2.1: Threading Tests
**LOC:** ~150
**File:** `tests/unit/threading_test.rs`

#### Step 3.2.2: Lock Management
**LOC:** ~120
**File:** `src/threading/locks.rs`
**Spec:** `docs/specs/functions/threading/cxf_acquire_solve_lock.md`, `cxf_release_solve_lock.md`, `cxf_env_acquire_lock.md`, `cxf_leave_critical_section.md`

#### Step 3.2.3: Thread Configuration
**LOC:** ~100
**File:** `src/threading/config.rs`
**Spec:** `docs/specs/functions/threading/cxf_get_threads.md`, `cxf_set_thread_count.md`

#### Step 3.2.4: CPU Detection
**LOC:** ~80
**File:** `src/threading/cpu.rs`
**Spec:** `docs/specs/functions/threading/cxf_get_logical_processors.md`, `cxf_get_physical_cores.md`

#### Step 3.2.5: Seed Generation
**LOC:** ~60
**File:** `src/threading/seed.rs`
**Spec:** `docs/specs/functions/threading/cxf_generate_seed.md`

---

### Module: Logging (5 functions)
**Spec Directory:** `docs/specs/functions/error_logging/`

#### Step 3.3.1: Logging Tests
**LOC:** ~100
**File:** `tests/unit/logging_test.rs`

#### Step 3.3.2: Log Output
**LOC:** ~100
**File:** `src/logging/output.rs`
**Spec:** `docs/specs/functions/error_logging/cxf_log_printf.md`, `cxf_register_log_callback.md`

#### Step 3.3.3: Format Helpers
**LOC:** ~80
**File:** `src/logging/format.rs`
**Spec:** `docs/specs/functions/utilities/cxf_log10_wrapper.md`, `cxf_snprintf_wrapper.md`

#### Step 3.3.4: System Info
**LOC:** ~60
**File:** `src/logging/system.rs`
**Spec:** (cxf_get_logical_processors for logging context)

---

### Module: Timing (5 functions)
**Spec Directory:** `docs/specs/functions/timing/`

#### Step 3.4.1: Timing Tests
**LOC:** ~100
**File:** `tests/unit/timing_test.rs`

#### Step 3.4.2: Timestamp
**LOC:** ~60
**File:** `src/timing/timestamp.rs`
**Spec:** `docs/specs/functions/timing/cxf_get_timestamp.md`

#### Step 3.4.3: Timing Sections
**LOC:** ~100
**File:** `src/timing/sections.rs`
**Spec:** `docs/specs/functions/timing/cxf_timing_start.md`, `cxf_timing_end.md`, `cxf_timing_update.md`

#### Step 3.4.4: Pivot/Refactor Timing
**LOC:** ~80
**File:** `src/timing/operations.rs`
**Spec:** `docs/specs/functions/timing/cxf_timing_pivot.md`, `docs/specs/functions/basis/cxf_timing_refactor.md`

---

### Module: Model Analysis (6 functions)
**Spec Directory:** `docs/specs/functions/validation/`, `docs/specs/functions/statistics/`

#### Step 3.5.1: Analysis Tests
**LOC:** ~100
**File:** `tests/unit/analysis_test.rs`

#### Step 3.5.2: Model Type Checks
**LOC:** ~80
**File:** `src/analysis/model_type.rs`
**Spec:** `docs/specs/functions/validation/cxf_is_mip_model.md`, `cxf_is_quadratic.md`, `cxf_is_socp.md`

#### Step 3.5.3: Coefficient Statistics
**LOC:** ~120
**File:** `src/analysis/coef_stats.rs`
**Spec:** `docs/specs/functions/statistics/cxf_coefficient_stats.md`, `cxf_compute_coef_stats.md`

#### Step 3.5.4: Presolve Statistics
**LOC:** ~80
**File:** `src/analysis/presolve_stats.rs`
**Spec:** `docs/specs/functions/statistics/cxf_presolve_stats.md`

---

## Milestone 4: Core Operations (Level 3)

**Goal:** Complete Basis, Callbacks, Solver State modules
**Parallelizable:** All modules can run in parallel

### Module: Basis Operations (8 functions)
**Spec Directory:** `docs/specs/functions/basis/`

#### Step 4.1.1: Basis Tests
**LOC:** ~250
**File:** `tests/unit/basis_test.rs`

Comprehensive tests for FTRAN, BTRAN, refactorization.

#### Step 4.1.2: BasisState Structure
**LOC:** ~150
**File:** `src/types/basis_state.rs`
**Spec:** `docs/specs/structures/basis_state.md`

#### Step 4.1.3: EtaFactors Structure
**LOC:** ~100
**File:** `src/types/eta_factors.rs`
**Spec:** `docs/specs/structures/eta_factors.md`

#### Step 4.1.4: cxf_ftran
**LOC:** ~150
**File:** `src/basis/ftran.rs`
**Spec:** `docs/specs/functions/basis/cxf_ftran.md`

Forward transformation: solve Bx = b

#### Step 4.1.5: cxf_btran
**LOC:** ~150
**File:** `src/basis/btran.rs`
**Spec:** `docs/specs/functions/basis/cxf_btran.md`

Backward transformation: solve y^T B = c^T

#### Step 4.1.6: cxf_basis_refactor
**LOC:** ~200
**File:** `src/basis/refactor.rs`
**Spec:** `docs/specs/functions/basis/cxf_basis_refactor.md`

LU factorization.

#### Step 4.1.7: Basis Snapshots
**LOC:** ~120
**File:** `src/basis/snapshot.rs`
**Spec:** `docs/specs/functions/basis/cxf_basis_snapshot.md`, `cxf_basis_diff.md`, `cxf_basis_equal.md`

#### Step 4.1.8: Basis Validation/Warm Start
**LOC:** ~100
**File:** `src/basis/warm.rs`
**Spec:** `docs/specs/functions/basis/cxf_basis_validate.md`, `cxf_basis_warm.md`

#### Step 4.1.9: cxf_pivot_with_eta
**LOC:** ~100
**File:** `src/basis/eta_pivot.rs`
**Spec:** `docs/specs/functions/basis/cxf_pivot_with_eta.md`

---

### Module: Callbacks (6 functions)
**Spec Directory:** `docs/specs/functions/callbacks/`

#### Step 4.2.1: Callbacks Tests
**LOC:** ~120
**File:** `tests/unit/callbacks_test.rs`

#### Step 4.2.2: CallbackContext Structure
**LOC:** ~100
**File:** `src/types/callback_context.rs`
**Spec:** `docs/specs/structures/callback_context.md`

#### Step 4.2.3: Callback Initialization
**LOC:** ~80
**File:** `src/callbacks/init.rs`
**Spec:** `docs/specs/functions/callbacks/cxf_init_callback_struct.md`, `cxf_reset_callback_state.md`

#### Step 4.2.4: Callback Invocation
**LOC:** ~100
**File:** `src/callbacks/invoke.rs`
**Spec:** `docs/specs/functions/callbacks/cxf_pre_optimize_callback.md`, `cxf_post_optimize_callback.md`

#### Step 4.2.5: Termination Handling
**LOC:** ~80
**File:** `src/callbacks/terminate.rs`
**Spec:** `docs/specs/functions/callbacks/cxf_callback_terminate.md`, `cxf_set_terminate.md`

---

### Module: Solver State (4 functions)
**Spec Directory:** `docs/specs/functions/memory/`, `docs/specs/functions/utilities/`

#### Step 4.3.1: Solver State Tests
**LOC:** ~100
**File:** `tests/unit/solver_state_test.rs`

#### Step 4.3.2: SolverContext Structure
**LOC:** ~150
**File:** `src/types/solver_context.rs`
**Spec:** `docs/specs/structures/solver_context.md`

#### Step 4.3.3: State Initialization
**LOC:** ~100
**File:** `src/solver_state/init.rs`
**Spec:** `docs/specs/functions/memory/cxf_init_solve_state.md`, `cxf_cleanup_solve_state.md`

#### Step 4.3.4: Helper Functions
**LOC:** ~80
**File:** `src/solver_state/helpers.rs`
**Spec:** `docs/specs/functions/utilities/cxf_cleanup_helper.md`

#### Step 4.3.5: Solution Extraction
**LOC:** ~100
**File:** `src/solver_state/extract.rs`
**Spec:** `docs/specs/functions/utilities/cxf_extract_solution.md`

---

## Milestone 5: Algorithm Components (Level 4)

**Goal:** Complete Pricing module
**Parallelizable:** All steps can run in parallel

### Module: Pricing (6 functions)
**Spec Directory:** `docs/specs/functions/pricing/`

#### Step 5.1.1: Pricing Tests
**LOC:** ~180
**File:** `tests/unit/pricing_test.rs`

Tests for all pricing strategies.

#### Step 5.1.2: PricingContext Structure
**LOC:** ~120
**File:** `src/types/pricing_context.rs`
**Spec:** `docs/specs/structures/pricing_context.md`

#### Step 5.1.3: cxf_pricing_init
**LOC:** ~100
**File:** `src/pricing/init.rs`
**Spec:** `docs/specs/functions/pricing/cxf_pricing_init.md`

#### Step 5.1.4: cxf_pricing_candidates
**LOC:** ~120
**File:** `src/pricing/candidates.rs`
**Spec:** `docs/specs/functions/pricing/cxf_pricing_candidates.md`

#### Step 5.1.5: cxf_pricing_steepest
**LOC:** ~150
**File:** `src/pricing/steepest.rs`
**Spec:** `docs/specs/functions/pricing/cxf_pricing_steepest.md`

Steepest edge pricing implementation.

#### Step 5.1.6: cxf_pricing_update, cxf_pricing_invalidate
**LOC:** ~100
**File:** `src/pricing/update.rs`
**Spec:** `docs/specs/functions/pricing/cxf_pricing_update.md`, `cxf_pricing_invalidate.md`

#### Step 5.1.7: cxf_pricing_step2
**LOC:** ~80
**File:** `src/pricing/phase.rs`
**Spec:** `docs/specs/functions/pricing/cxf_pricing_step2.md`

---

## Milestone 6: Simplex Core (Level 5)

**Goal:** Complete Simplex Core, Crossover, Utilities modules
**Parallelizable:** Simplex phases can run sequentially; utilities in parallel

### Module: Simplex Core (21 functions)
**Spec Directory:** `docs/specs/functions/simplex/`, `docs/specs/functions/ratio_test/`, `docs/specs/functions/pivot/`

#### Step 6.1.1: Simplex Tests - Basic
**LOC:** ~200
**File:** `tests/unit/simplex_basic_test.rs`

Tests for initialization, setup, cleanup.

#### Step 6.1.2: Simplex Tests - Iteration
**LOC:** ~200
**File:** `tests/unit/simplex_iteration_test.rs`

Tests for iteration loop, phases.

#### Step 6.1.3: Simplex Tests - Edge Cases
**LOC:** ~200
**File:** `tests/unit/simplex_edge_test.rs`

Tests for degeneracy, unbounded, infeasible.

#### Step 6.1.4: cxf_solve_lp
**LOC:** ~150
**File:** `src/simplex/solve_lp.rs`
**Spec:** `docs/specs/functions/simplex/cxf_solve_lp.md`

Main entry point.

#### Step 6.1.5: cxf_simplex_init, cxf_simplex_final
**LOC:** ~120
**File:** `src/simplex/lifecycle.rs`
**Spec:** `docs/specs/functions/simplex/cxf_simplex_init.md`, `cxf_simplex_final.md`

#### Step 6.1.6: cxf_simplex_setup, cxf_simplex_preprocess
**LOC:** ~150
**File:** `src/simplex/setup.rs`
**Spec:** `docs/specs/functions/simplex/cxf_simplex_setup.md`, `cxf_simplex_preprocess.md`

#### Step 6.1.7: cxf_simplex_crash
**LOC:** ~120
**File:** `src/simplex/crash.rs`
**Spec:** `docs/specs/functions/simplex/cxf_simplex_crash.md`

Initial basis heuristic.

#### Step 6.1.8: cxf_simplex_iterate
**LOC:** ~150
**File:** `src/simplex/iterate.rs`
**Spec:** `docs/specs/functions/simplex/cxf_simplex_iterate.md`

Main loop.

#### Step 6.1.9: cxf_simplex_step
**LOC:** ~150
**File:** `src/simplex/step.rs`
**Spec:** `docs/specs/functions/simplex/cxf_simplex_step.md`

Single iteration.

#### Step 6.1.10: cxf_simplex_step2, cxf_simplex_step3
**LOC:** ~120
**File:** `src/simplex/phase_steps.rs`
**Spec:** `docs/specs/functions/simplex/cxf_simplex_step2.md`, `cxf_simplex_step3.md`

Phase-specific steps.

#### Step 6.1.11: cxf_simplex_post_iterate, cxf_simplex_phase_end
**LOC:** ~100
**File:** `src/simplex/post.rs`
**Spec:** `docs/specs/functions/simplex/cxf_simplex_post_iterate.md`, `cxf_simplex_phase_end.md`

#### Step 6.1.12: cxf_simplex_refine
**LOC:** ~120
**File:** `src/simplex/refine.rs`
**Spec:** `docs/specs/functions/simplex/cxf_simplex_refine.md`

Iterative refinement.

#### Step 6.1.13: cxf_simplex_perturbation, cxf_simplex_unperturb
**LOC:** ~120
**File:** `src/simplex/perturbation.rs`
**Spec:** `docs/specs/functions/simplex/cxf_simplex_perturbation.md`, `cxf_simplex_unperturb.md`

Anti-cycling.

#### Step 6.1.14: cxf_simplex_cleanup
**LOC:** ~80
**File:** `src/simplex/cleanup.rs`
**Spec:** `docs/specs/functions/simplex/cxf_simplex_cleanup.md`

#### Step 6.1.15: cxf_pivot_primal
**LOC:** ~150
**File:** `src/simplex/pivot_primal.rs`
**Spec:** `docs/specs/functions/pivot/cxf_pivot_primal.md`

#### Step 6.1.16: cxf_pivot_bound, cxf_pivot_special
**LOC:** ~120
**File:** `src/simplex/pivot_special.rs`
**Spec:** `docs/specs/functions/ratio_test/cxf_pivot_bound.md`, `cxf_pivot_special.md`

#### Step 6.1.17: cxf_ratio_test
**LOC:** ~150
**File:** `src/simplex/ratio_test.rs`
**Spec:** `docs/specs/functions/ratio_test/cxf_ratio_test.md`

Harris two-pass ratio test.

#### Step 6.1.18: cxf_quadratic_adjust
**LOC:** ~80
**File:** `src/simplex/quadratic.rs`
**Spec:** `docs/specs/functions/simplex/cxf_quadratic_adjust.md`

---

### Module: Crossover (2 functions)
**Spec Directory:** `docs/specs/functions/crossover/`

#### Step 6.2.1: Crossover Tests
**LOC:** ~100
**File:** `tests/unit/crossover_test.rs`

#### Step 6.2.2: cxf_crossover
**LOC:** ~150
**File:** `src/crossover/crossover.rs`
**Spec:** `docs/specs/functions/crossover/cxf_crossover.md`

#### Step 6.2.3: cxf_crossover_bounds
**LOC:** ~80
**File:** `src/crossover/bounds.rs`
**Spec:** `docs/specs/functions/crossover/cxf_crossover_bounds.md`

---

### Module: Utilities (10 functions)
**Spec Directory:** `docs/specs/functions/utilities/`

#### Step 6.3.1: Utilities Tests
**LOC:** ~150
**File:** `tests/unit/utilities_test.rs`

#### Step 6.3.2: cxf_fix_variable
**LOC:** ~80
**File:** `src/utilities/fix_var.rs`
**Spec:** `docs/specs/functions/pivot/cxf_fix_variable.md`

#### Step 6.3.3: cxf_generate_seed
**LOC:** ~60
**File:** `src/utilities/seed.rs`
**Spec:** `docs/specs/functions/threading/cxf_generate_seed.md`

#### Step 6.3.4: Math Wrappers
**LOC:** ~80
**File:** `src/utilities/math.rs`
**Spec:** `docs/specs/functions/utilities/cxf_floor_ceil_wrapper.md`, `cxf_log10_wrapper.md`

#### Step 6.3.5: Constraint Helpers
**LOC:** ~100
**File:** `src/utilities/constraints.rs`
**Spec:** `docs/specs/functions/utilities/cxf_count_genconstr_types.md`, `cxf_get_genconstr_name.md`, `cxf_get_qconstr_data.md`

#### Step 6.3.6: Multi-Objective Check
**LOC:** ~60
**File:** `src/utilities/multi_obj.rs`
**Spec:** `docs/specs/functions/utilities/cxf_is_multi_obj.md`

#### Step 6.3.7: Misc Utility
**LOC:** ~60
**File:** `src/utilities/misc.rs`
**Spec:** `docs/specs/functions/utilities/cxf_misc_utility.md`

---

## Milestone 7: Public API (Level 6)

**Goal:** Complete Model API module
**Parallelizable:** All API functions can be implemented in parallel

### Module: Model API (30 functions)
**Spec Directory:** `docs/specs/modules/17_model_api.md`

#### Step 7.1.1: API Tests - Environment
**LOC:** ~100
**File:** `tests/unit/api_env_test.rs`

Tests for `cxf_loadenv`, `cxf_emptyenv`, `cxf_freeenv`.

#### Step 7.1.2: API Tests - Model
**LOC:** ~150
**File:** `tests/unit/api_model_test.rs`

Tests for `cxf_newmodel`, `cxf_freemodel`, `cxf_copymodel`, `cxf_updatemodel`.

#### Step 7.1.3: API Tests - Variables
**LOC:** ~150
**File:** `tests/unit/api_vars_test.rs`

Tests for `cxf_addvar`, `cxf_addvars`, `cxf_delvars`.

#### Step 7.1.4: API Tests - Constraints
**LOC:** ~150
**File:** `tests/unit/api_constrs_test.rs`

Tests for `cxf_addconstr`, `cxf_addconstrs`, `cxf_addqconstr`, `cxf_addgenconstrIndicator`.

#### Step 7.1.5: API Tests - Optimize
**LOC:** ~200
**File:** `tests/unit/api_optimize_test.rs`

Tests for `cxf_optimize`, `cxf_optimize_internal`, `cxf_terminate`.

#### Step 7.1.6: API Tests - Queries
**LOC:** ~150
**File:** `tests/unit/api_query_test.rs`

Tests for `cxf_getintattr`, `cxf_getdblattr`, `cxf_getconstrs`, `cxf_getcoeff`.

#### Step 7.1.7: CxfEnv Structure (Full)
**LOC:** ~200
**File:** `src/types/env.rs` (expand from stub)
**Spec:** `docs/specs/structures/cxf_env.md`

#### Step 7.1.8: CxfModel Structure (Full)
**LOC:** ~200
**File:** `src/types/model.rs` (expand from stub)
**Spec:** `docs/specs/structures/cxf_model.md`

#### Step 7.1.9: Environment API
**LOC:** ~120
**File:** `src/api/env.rs`
**Spec:** Functions: `cxf_loadenv`, `cxf_emptyenv`, `cxf_freeenv`

#### Step 7.1.10: Model Creation API
**LOC:** ~150
**File:** `src/api/model.rs`
**Spec:** Functions: `cxf_newmodel`, `cxf_freemodel`, `cxf_copymodel`, `cxf_updatemodel`

#### Step 7.1.11: Variable API
**LOC:** ~150
**File:** `src/api/vars.rs`
**Spec:** Functions: `cxf_addvar`, `cxf_addvars`, `cxf_delvars`

#### Step 7.1.12: Constraint API
**LOC:** ~200
**File:** `src/api/constrs.rs`
**Spec:** Functions: `cxf_addconstr`, `cxf_addconstrs`, `cxf_chgcoeffs`, `cxf_getconstrs`, `cxf_getcoeff`

#### Step 7.1.13: Quadratic API
**LOC:** ~120
**File:** `src/api/quadratic.rs`
**Spec:** Functions: `cxf_addqpterms`, `cxf_addqconstr`, `cxf_addgenconstrIndicator`

#### Step 7.1.14: Optimize API
**LOC:** ~150
**File:** `src/api/optimize.rs`
**Spec:** Functions: `cxf_optimize`, `cxf_optimize_internal`, `cxf_terminate`

#### Step 7.1.15: Attribute API
**LOC:** ~120
**File:** `src/api/attrs.rs`
**Spec:** Functions: `cxf_getintattr`, `cxf_getdblattr`

#### Step 7.1.16: Parameter API
**LOC:** ~100
**File:** `src/api/params.rs`
**Spec:** Functions: `cxf_setintparam`, `cxf_getintparam`

#### Step 7.1.17: I/O API
**LOC:** ~150
**File:** `src/api/io.rs`
**Spec:** Functions: `cxf_read`, `cxf_write`

#### Step 7.1.18: Info API
**LOC:** ~80
**File:** `src/api/info.rs`
**Spec:** Functions: `cxf_version`, `cxf_geterrormsg`, `cxf_setcallbackfunc`

---

## Benchmarks

### Benchmark Suite
**File:** `benches/benchmarks.rs`

#### Benchmark B.1: Tracer Bullet
Measure time for 1-variable LP.

#### Benchmark B.2: Small LP (10x10)
10 variables, 10 constraints.

#### Benchmark B.3: Medium LP (100x100)
100 variables, 100 constraints.

#### Benchmark B.4: Netlib afiro
First Netlib problem (27 vars, 51 constraints).

#### Benchmark B.5: Netlib adlittle
Netlib problem (97 vars, 138 constraints).

#### Benchmark B.6: Matrix Operations
FTRAN/BTRAN/SpMV benchmarks.

#### Benchmark B.7: Pricing
Steepest edge pricing benchmark.

### Success Criteria
| Benchmark | Target |
|-----------|--------|
| Tracer Bullet | < 1ms |
| Small LP | < 10ms |
| Medium LP | < 100ms |
| Netlib afiro | < 1s |
| Netlib adlittle | < 5s |

---

## Function Checklist

### Memory Management (9 functions)
- [ ] `cxf_malloc` - `docs/specs/functions/memory/cxf_malloc.md`
- [ ] `cxf_calloc` - `docs/specs/functions/memory/cxf_calloc.md`
- [ ] `cxf_realloc` - `docs/specs/functions/memory/cxf_realloc.md`
- [ ] `cxf_free` - `docs/specs/functions/memory/cxf_free.md`
- [ ] `cxf_vector_free` - `docs/specs/functions/memory/cxf_vector_free.md`
- [ ] `cxf_alloc_eta` - `docs/specs/functions/memory/cxf_alloc_eta.md`
- [ ] `cxf_free_solver_state` - `docs/specs/functions/memory/cxf_free_solver_state.md`
- [ ] `cxf_free_basis_state` - `docs/specs/functions/memory/cxf_free_basis_state.md`
- [ ] `cxf_free_callback_state` - `docs/specs/functions/memory/cxf_free_callback_state.md`

### Parameters (4 functions)
- [ ] `cxf_getdblparam` - `docs/specs/functions/parameters/cxf_getdblparam.md`
- [ ] `cxf_get_feasibility_tol` - `docs/specs/functions/parameters/cxf_get_feasibility_tol.md`
- [ ] `cxf_get_optimality_tol` - `docs/specs/functions/parameters/cxf_get_optimality_tol.md`
- [ ] `cxf_get_infinity` - `docs/specs/functions/parameters/cxf_get_infinity.md`

### Validation (2 functions)
- [ ] `cxf_validate_array` - `docs/specs/functions/validation/cxf_validate_array.md`
- [ ] `cxf_validate_vartypes` - `docs/specs/functions/validation/cxf_validate_vartypes.md`

### Error Handling (10 functions)
- [ ] `cxf_error` - `docs/specs/functions/error_logging/cxf_error.md`
- [ ] `cxf_errorlog` - `docs/specs/functions/error_logging/cxf_errorlog.md`
- [ ] `cxf_check_nan` - `docs/specs/functions/validation/cxf_check_nan.md`
- [ ] `cxf_check_nan_or_inf` - `docs/specs/functions/validation/cxf_check_nan_or_inf.md`
- [ ] `cxf_checkenv` - `docs/specs/functions/validation/cxf_checkenv.md`
- [ ] `cxf_pivot_check` - `docs/specs/functions/ratio_test/cxf_pivot_check.md`
- [ ] `cxf_special_check` - `docs/specs/functions/statistics/cxf_special_check.md`
- [ ] `cxf_check_model_flags1` - `docs/specs/functions/validation/cxf_check_model_flags1.md`
- [ ] `cxf_check_model_flags2` - `docs/specs/functions/validation/cxf_check_model_flags2.md`
- [ ] `cxf_check_terminate` - `docs/specs/functions/callbacks/cxf_check_terminate.md`

### Logging (5 functions)
- [ ] `cxf_log_printf` - `docs/specs/functions/error_logging/cxf_log_printf.md`
- [ ] `cxf_log10_wrapper` - `docs/specs/functions/utilities/cxf_log10_wrapper.md`
- [ ] `cxf_snprintf_wrapper` - `docs/specs/functions/utilities/cxf_snprintf_wrapper.md`
- [ ] `cxf_register_log_callback` - `docs/specs/functions/error_logging/cxf_register_log_callback.md`
- [ ] `cxf_get_logical_processors` - `docs/specs/functions/threading/cxf_get_logical_processors.md`

### Threading (7 functions)
- [ ] `cxf_get_threads` - `docs/specs/functions/threading/cxf_get_threads.md`
- [ ] `cxf_set_thread_count` - `docs/specs/functions/threading/cxf_set_thread_count.md`
- [ ] `cxf_get_physical_cores` - `docs/specs/functions/threading/cxf_get_physical_cores.md`
- [ ] `cxf_acquire_solve_lock` - `docs/specs/functions/threading/cxf_acquire_solve_lock.md`
- [ ] `cxf_release_solve_lock` - `docs/specs/functions/threading/cxf_release_solve_lock.md`
- [ ] `cxf_env_acquire_lock` - `docs/specs/functions/threading/cxf_env_acquire_lock.md`
- [ ] `cxf_leave_critical_section` - `docs/specs/functions/threading/cxf_leave_critical_section.md`

### Timing (5 functions)
- [ ] `cxf_get_timestamp` - `docs/specs/functions/timing/cxf_get_timestamp.md`
- [ ] `cxf_timing_start` - `docs/specs/functions/timing/cxf_timing_start.md`
- [ ] `cxf_timing_end` - `docs/specs/functions/timing/cxf_timing_end.md`
- [ ] `cxf_timing_pivot` - `docs/specs/functions/timing/cxf_timing_pivot.md`
- [ ] `cxf_timing_update` - `docs/specs/functions/timing/cxf_timing_update.md`

### Matrix Operations (7 functions)
- [ ] `cxf_matrix_multiply` - `docs/specs/functions/matrix/cxf_matrix_multiply.md`
- [ ] `cxf_dot_product` - `docs/specs/functions/matrix/cxf_dot_product.md`
- [ ] `cxf_vector_norm` - `docs/specs/functions/matrix/cxf_vector_norm.md`
- [ ] `cxf_build_row_major` - `docs/specs/functions/matrix/cxf_build_row_major.md`
- [ ] `cxf_prepare_row_data` - `docs/specs/functions/matrix/cxf_prepare_row_data.md`
- [ ] `cxf_finalize_row_data` - `docs/specs/functions/matrix/cxf_finalize_row_data.md`
- [ ] `cxf_sort_indices` - `docs/specs/functions/matrix/cxf_sort_indices.md`

### Model Analysis (6 functions)
- [ ] `cxf_is_mip_model` - `docs/specs/functions/validation/cxf_is_mip_model.md`
- [ ] `cxf_is_quadratic` - `docs/specs/functions/validation/cxf_is_quadratic.md`
- [ ] `cxf_is_socp` - `docs/specs/functions/validation/cxf_is_socp.md`
- [ ] `cxf_coefficient_stats` - `docs/specs/functions/statistics/cxf_coefficient_stats.md`
- [ ] `cxf_compute_coef_stats` - `docs/specs/functions/statistics/cxf_compute_coef_stats.md`
- [ ] `cxf_presolve_stats` - `docs/specs/functions/statistics/cxf_presolve_stats.md`

### Basis Operations (8 functions)
- [ ] `cxf_ftran` - `docs/specs/functions/basis/cxf_ftran.md`
- [ ] `cxf_btran` - `docs/specs/functions/basis/cxf_btran.md`
- [ ] `cxf_basis_refactor` - `docs/specs/functions/basis/cxf_basis_refactor.md`
- [ ] `cxf_basis_snapshot` - `docs/specs/functions/basis/cxf_basis_snapshot.md`
- [ ] `cxf_basis_diff` - `docs/specs/functions/basis/cxf_basis_diff.md`
- [ ] `cxf_basis_equal` - `docs/specs/functions/basis/cxf_basis_equal.md`
- [ ] `cxf_basis_validate` - `docs/specs/functions/basis/cxf_basis_validate.md`
- [ ] `cxf_basis_warm` - `docs/specs/functions/basis/cxf_basis_warm.md`

### Callbacks (6 functions)
- [ ] `cxf_init_callback_struct` - `docs/specs/functions/callbacks/cxf_init_callback_struct.md`
- [ ] `cxf_reset_callback_state` - `docs/specs/functions/callbacks/cxf_reset_callback_state.md`
- [ ] `cxf_pre_optimize_callback` - `docs/specs/functions/callbacks/cxf_pre_optimize_callback.md`
- [ ] `cxf_post_optimize_callback` - `docs/specs/functions/callbacks/cxf_post_optimize_callback.md`
- [ ] `cxf_callback_terminate` - `docs/specs/functions/callbacks/cxf_callback_terminate.md`
- [ ] `cxf_set_terminate` - `docs/specs/functions/callbacks/cxf_set_terminate.md`

### Solver State (4 functions)
- [ ] `cxf_init_solve_state` - `docs/specs/functions/memory/cxf_init_solve_state.md`
- [ ] `cxf_cleanup_solve_state` - `docs/specs/functions/memory/cxf_cleanup_solve_state.md`
- [ ] `cxf_cleanup_helper` - `docs/specs/functions/utilities/cxf_cleanup_helper.md`
- [ ] `cxf_extract_solution` - `docs/specs/functions/utilities/cxf_extract_solution.md`

### Pricing (6 functions)
- [ ] `cxf_pricing_init` - `docs/specs/functions/pricing/cxf_pricing_init.md`
- [ ] `cxf_pricing_candidates` - `docs/specs/functions/pricing/cxf_pricing_candidates.md`
- [ ] `cxf_pricing_steepest` - `docs/specs/functions/pricing/cxf_pricing_steepest.md`
- [ ] `cxf_pricing_update` - `docs/specs/functions/pricing/cxf_pricing_update.md`
- [ ] `cxf_pricing_invalidate` - `docs/specs/functions/pricing/cxf_pricing_invalidate.md`
- [ ] `cxf_pricing_step2` - `docs/specs/functions/pricing/cxf_pricing_step2.md`

### Simplex Core (21 functions)
- [ ] `cxf_solve_lp` - `docs/specs/functions/simplex/cxf_solve_lp.md`
- [ ] `cxf_simplex_init` - `docs/specs/functions/simplex/cxf_simplex_init.md`
- [ ] `cxf_simplex_setup` - `docs/specs/functions/simplex/cxf_simplex_setup.md`
- [ ] `cxf_simplex_preprocess` - `docs/specs/functions/simplex/cxf_simplex_preprocess.md`
- [ ] `cxf_simplex_crash` - `docs/specs/functions/simplex/cxf_simplex_crash.md`
- [ ] `cxf_simplex_iterate` - `docs/specs/functions/simplex/cxf_simplex_iterate.md`
- [ ] `cxf_simplex_step` - `docs/specs/functions/simplex/cxf_simplex_step.md`
- [ ] `cxf_simplex_step2` - `docs/specs/functions/simplex/cxf_simplex_step2.md`
- [ ] `cxf_simplex_step3` - `docs/specs/functions/simplex/cxf_simplex_step3.md`
- [ ] `cxf_simplex_refine` - `docs/specs/functions/simplex/cxf_simplex_refine.md`
- [ ] `cxf_simplex_perturbation` - `docs/specs/functions/simplex/cxf_simplex_perturbation.md`
- [ ] `cxf_simplex_unperturb` - `docs/specs/functions/simplex/cxf_simplex_unperturb.md`
- [ ] `cxf_simplex_phase_end` - `docs/specs/functions/simplex/cxf_simplex_phase_end.md`
- [ ] `cxf_simplex_post_iterate` - `docs/specs/functions/simplex/cxf_simplex_post_iterate.md`
- [ ] `cxf_simplex_final` - `docs/specs/functions/simplex/cxf_simplex_final.md`
- [ ] `cxf_simplex_cleanup` - `docs/specs/functions/simplex/cxf_simplex_cleanup.md`
- [ ] `cxf_pivot_primal` - `docs/specs/functions/pivot/cxf_pivot_primal.md`
- [ ] `cxf_pivot_bound` - `docs/specs/functions/ratio_test/cxf_pivot_bound.md`
- [ ] `cxf_pivot_special` - `docs/specs/functions/ratio_test/cxf_pivot_special.md`
- [ ] `cxf_pivot_with_eta` - `docs/specs/functions/basis/cxf_pivot_with_eta.md`
- [ ] `cxf_ratio_test` - `docs/specs/functions/ratio_test/cxf_ratio_test.md`

### Crossover (2 functions)
- [ ] `cxf_crossover` - `docs/specs/functions/crossover/cxf_crossover.md`
- [ ] `cxf_crossover_bounds` - `docs/specs/functions/crossover/cxf_crossover_bounds.md`

### Utilities (10 functions)
- [ ] `cxf_fix_variable` - `docs/specs/functions/pivot/cxf_fix_variable.md`
- [ ] `cxf_quadratic_adjust` - `docs/specs/functions/simplex/cxf_quadratic_adjust.md`
- [ ] `cxf_generate_seed` - `docs/specs/functions/threading/cxf_generate_seed.md`
- [ ] `cxf_floor_ceil_wrapper` - `docs/specs/functions/utilities/cxf_floor_ceil_wrapper.md`
- [ ] `cxf_misc_utility` - `docs/specs/functions/utilities/cxf_misc_utility.md`
- [ ] `cxf_is_multi_obj` - `docs/specs/functions/utilities/cxf_is_multi_obj.md`
- [ ] `cxf_get_genconstr_name` - `docs/specs/functions/utilities/cxf_get_genconstr_name.md`
- [ ] `cxf_get_qconstr_data` - `docs/specs/functions/utilities/cxf_get_qconstr_data.md`
- [ ] `cxf_count_genconstr_types` - `docs/specs/functions/utilities/cxf_count_genconstr_types.md`
- [ ] `cxf_timing_refactor` - `docs/specs/functions/basis/cxf_timing_refactor.md`

### Model API (30 functions)
- [ ] `cxf_loadenv` - API for loading environment
- [ ] `cxf_emptyenv` - API for empty environment
- [ ] `cxf_freeenv` - API for freeing environment
- [ ] `cxf_newmodel` - API for new model
- [ ] `cxf_freemodel` - API for freeing model
- [ ] `cxf_copymodel` - API for copying model
- [ ] `cxf_updatemodel` - API for updating model
- [ ] `cxf_addvar` - API for adding variable
- [ ] `cxf_addvars` - API for adding variables
- [ ] `cxf_delvars` - API for deleting variables
- [ ] `cxf_addconstr` - API for adding constraint
- [ ] `cxf_addconstrs` - API for adding constraints
- [ ] `cxf_addqpterms` - API for quadratic terms
- [ ] `cxf_addqconstr` - API for quadratic constraint
- [ ] `cxf_addgenconstrIndicator` - API for indicator constraint
- [ ] `cxf_chgcoeffs` - API for changing coefficients
- [ ] `cxf_getconstrs` - API for getting constraints
- [ ] `cxf_getcoeff` - API for getting coefficient
- [ ] `cxf_optimize` - API for optimization
- [ ] `cxf_optimize_internal` - Internal optimization
- [ ] `cxf_terminate` - API for termination
- [ ] `cxf_getintattr` - API for integer attribute
- [ ] `cxf_getdblattr` - API for double attribute
- [ ] `cxf_setintparam` - API for setting parameter
- [ ] `cxf_getintparam` - API for getting parameter
- [ ] `cxf_read` - API for reading model
- [ ] `cxf_write` - API for writing model
- [ ] `cxf_version` - API for version
- [ ] `cxf_geterrormsg` - API for error message
- [ ] `cxf_setcallbackfunc` - API for callback

---

## Structure Checklist

- [ ] `CxfEnv` - `docs/specs/structures/cxf_env.md`
- [ ] `CxfModel` - `docs/specs/structures/cxf_model.md`
- [ ] `SparseMatrix` - `docs/specs/structures/sparse_matrix.md`
- [ ] `SolverContext` - `docs/specs/structures/solver_context.md`
- [ ] `BasisState` - `docs/specs/structures/basis_state.md`
- [ ] `EtaFactors` - `docs/specs/structures/eta_factors.md`
- [ ] `PricingContext` - `docs/specs/structures/pricing_context.md`
- [ ] `CallbackContext` - `docs/specs/structures/callback_context.md`

---

## Parallelization Guide

### Milestone Dependencies
```
M0 ──> M1 ──> M2 ──> M3 ──> M4 ──> M5 ──> M6 ──> M7
              │     │     │     │     │
              │     │     │     │     └── All API functions parallel
              │     │     │     └── Simplex sequential; Crossover/Utils parallel
              │     │     └── Pricing parallel
              │     └── Basis/Callbacks/SolverState parallel
              └── All Level 0-1 modules parallel
```

### Within-Milestone Parallelism

| Milestone | Parallel Groups |
|-----------|-----------------|
| M2 | Memory, Parameters, Validation, Error - all parallel |
| M3 | Matrix, Threading, Logging, Timing, Analysis - all parallel |
| M4 | Basis, Callbacks, SolverState - all parallel |
| M5 | Pricing steps - all parallel |
| M6 | Crossover + Utilities parallel; Simplex mostly sequential |
| M7 | All 30 API functions - all parallel |

### Recommended Agent Allocation

| Agent | Modules | Functions |
|-------|---------|-----------|
| Agent 1 | Memory, Parameters | 13 functions |
| Agent 2 | Validation, Error | 12 functions |
| Agent 3 | Logging, Threading | 12 functions |
| Agent 4 | Timing, Analysis | 11 functions |
| Agent 5 | Matrix | 7 functions |
| Agent 6 | Basis | 8 functions |
| Agent 7 | Callbacks, SolverState | 10 functions |
| Agent 8 | Pricing | 6 functions |
| Agent 9 | Simplex Core | 21 functions |
| Agent 10 | Crossover, Utilities | 12 functions |
| Agent 11 | API (Environment, Model) | 10 functions |
| Agent 12 | API (Variables, Constraints) | 10 functions |
| Agent 13 | API (Optimize, Query, I/O) | 10 functions |

---

*Total Steps: ~120*
*Total Functions: 142*
*Total Structures: 8*
*Estimated LOC: 15,000-20,000 implementation + 5,000-7,000 tests*
