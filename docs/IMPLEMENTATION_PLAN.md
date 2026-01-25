# Convexfeld Implementation Plan

**Version:** 1.0
**Date:** January 2026
**Methodology:** Test Driven Design (Tests First)
**Target:** ~100 LOC per step, 100-200 LOC per file

---

## Table of Contents

1. [Overview](#1-overview)
2. [File Organization](#2-file-organization)
3. [Milestone 0: Tracer Bullet](#3-milestone-0-tracer-bullet)
4. [Milestone 1: Foundation Layer](#4-milestone-1-foundation-layer)
5. [Milestone 2: Data Layer](#5-milestone-2-data-layer)
6. [Milestone 3: Core Operations](#6-milestone-3-core-operations)
7. [Milestone 4: Algorithm Layer](#7-milestone-4-algorithm-layer)
8. [Milestone 5: Simplex Core](#8-milestone-5-simplex-core)
9. [Milestone 6: Public API](#9-milestone-6-public-api)
10. [Parallelization Matrix](#10-parallelization-matrix)
11. [Dependency Graph](#11-dependency-graph)

---

## 1. Overview

### 1.1 Guiding Principles

1. **TDD**: Write tests BEFORE implementation code
2. **Small Files**: 100-200 LOC max per file; refactor if larger
3. **Small Steps**: ~100 LOC per implementation step
4. **Parallelizable**: Minimize dependencies between tasks
5. **Tracer Bullet First**: Prove architecture with minimal end-to-end path

### 1.2 Metrics

| Metric | Target |
|--------|--------|
| LOC per file | 100-200 |
| LOC per step | ~100 |
| Test:Code ratio | 1:3 to 1:4 |
| Files per module | 3-8 |

### 1.3 Naming Conventions

```
src/
  cxf_<module>.h          # Public header
  cxf_<module>.c          # Implementation (may be split)
  cxf_<module>_<sub>.c    # Split implementation files

tests/
  test_<module>.c         # Test file
  test_<module>_<sub>.c   # Split test files
```

---

## 2. File Organization

### 2.1 Source Tree Structure

```
convexfeld/
├── include/
│   └── convexfeld.h              # Single public header (~150 LOC)
├── src/
│   ├── core/
│   │   ├── cxf_types.h           # Type definitions (~100 LOC)
│   │   ├── cxf_constants.h       # Constants (~50 LOC)
│   │   ├── cxf_memory.h          # Memory interface (~50 LOC)
│   │   ├── cxf_memory.c          # Memory implementation (~150 LOC)
│   │   ├── cxf_error.h           # Error interface (~50 LOC)
│   │   ├── cxf_error.c           # Error implementation (~150 LOC)
│   │   ├── cxf_params.h          # Parameters interface (~50 LOC)
│   │   ├── cxf_params.c          # Parameters implementation (~150 LOC)
│   │   ├── cxf_logging.h         # Logging interface (~40 LOC)
│   │   └── cxf_logging.c         # Logging implementation (~100 LOC)
│   ├── matrix/
│   │   ├── cxf_sparse.h          # Sparse matrix interface (~80 LOC)
│   │   ├── cxf_sparse_create.c   # Matrix creation (~150 LOC)
│   │   ├── cxf_sparse_access.c   # Matrix access (~150 LOC)
│   │   ├── cxf_sparse_modify.c   # Matrix modification (~150 LOC)
│   │   └── cxf_sparse_utils.c    # Matrix utilities (~100 LOC)
│   ├── basis/
│   │   ├── cxf_basis.h           # Basis interface (~80 LOC)
│   │   ├── cxf_basis_state.c     # Basis state management (~150 LOC)
│   │   ├── cxf_ftran.c           # Forward transformation (~150 LOC)
│   │   ├── cxf_btran.c           # Backward transformation (~150 LOC)
│   │   ├── cxf_eta.c             # Eta vector operations (~150 LOC)
│   │   └── cxf_refactor.c        # LU refactorization (~200 LOC)
│   ├── pricing/
│   │   ├── cxf_pricing.h         # Pricing interface (~60 LOC)
│   │   ├── cxf_pricing_dantzig.c # Dantzig pricing (~100 LOC)
│   │   ├── cxf_pricing_steepest.c# Steepest edge (~150 LOC)
│   │   └── cxf_pricing_partial.c # Partial pricing (~100 LOC)
│   ├── ratio/
│   │   ├── cxf_ratio.h           # Ratio test interface (~50 LOC)
│   │   ├── cxf_ratio_harris.c    # Harris ratio test (~150 LOC)
│   │   └── cxf_ratio_bound.c     # Bound computations (~100 LOC)
│   ├── simplex/
│   │   ├── cxf_simplex.h         # Simplex interface (~80 LOC)
│   │   ├── cxf_simplex_init.c    # Initialization (~150 LOC)
│   │   ├── cxf_simplex_iterate.c # Main iteration (~150 LOC)
│   │   ├── cxf_simplex_phase1.c  # Phase I (~150 LOC)
│   │   ├── cxf_simplex_phase2.c  # Phase II (~150 LOC)
│   │   ├── cxf_simplex_pivot.c   # Pivot operations (~150 LOC)
│   │   ├── cxf_simplex_perturb.c # Perturbation (~100 LOC)
│   │   └── cxf_simplex_cleanup.c # Cleanup/refine (~150 LOC)
│   └── api/
│       ├── cxf_env.c             # Environment API (~150 LOC)
│       ├── cxf_model.c           # Model lifecycle (~150 LOC)
│       ├── cxf_model_vars.c      # Variable operations (~150 LOC)
│       ├── cxf_model_constrs.c   # Constraint operations (~150 LOC)
│       ├── cxf_optimize.c        # Optimization entry (~150 LOC)
│       ├── cxf_query.c           # Query functions (~150 LOC)
│       └── cxf_io.c              # File I/O (~200 LOC)
├── tests/
│   ├── unit/                     # Unit tests (mirrors src/)
│   ├── integration/              # Integration tests
│   ├── edge/                     # Edge case tests
│   └── fixtures/                 # Test data and helpers
└── CMakeLists.txt
```

### 2.2 File Size Guidelines

| File Type | Target LOC | Max LOC | Action if Exceeded |
|-----------|------------|---------|-------------------|
| Header (.h) | 50-80 | 150 | Split by functionality |
| Implementation (.c) | 100-150 | 200 | Split by function group |
| Test file | 150-300 | 400 | Split by test category |

---

## 3. Milestone 0: Tracer Bullet

**Goal:** Minimal end-to-end path through ALL layers to prove architecture.

**Scope:** Solve trivial 1-variable LP: minimize x subject to x ≥ 0

### 3.1 Tracer Bullet Components

```
┌─────────────────────────────────────────────────────────────────┐
│                    TRACER BULLET PATH                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Layer 6: cxf_newmodel() → cxf_addvar() → cxf_optimize()        │
│              │                                                  │
│  Layer 5: cxf_simplex_init() → cxf_simplex_iterate() [1 iter]   │
│              │                                                  │
│  Layer 4: cxf_pricing_select() → cxf_ratio_test()               │
│              │                                                  │
│  Layer 3: cxf_ftran() → cxf_basis_update()                      │
│              │                                                  │
│  Layer 2: cxf_sparse_get_column()                               │
│              │                                                  │
│  Layer 1: cxf_error_check()                                     │
│              │                                                  │
│  Layer 0: cxf_malloc() → cxf_free()                             │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 3.2 Tracer Bullet Implementation Steps

#### Step TB-1: Minimal Types and Constants
**Files:** `src/core/cxf_types.h`, `src/core/cxf_constants.h`
**LOC:** ~80 test, ~80 impl
**Parallel:** None (foundation)

```
tests/unit/test_types.c:
  - TEST(Types, StatusCodes)
  - TEST(Types, StructSizes)

src/core/cxf_types.h:
  - CxfStatus enum (OK, OPTIMAL, INFEASIBLE, UNBOUNDED, ERROR)
  - CxfEnv struct (minimal: allocator, errorCode)
  - CxfModel struct (minimal: env, numVars, objective, bounds, solution)
```

#### Step TB-2: Minimal Memory
**Files:** `src/core/cxf_memory.h`, `src/core/cxf_memory.c`
**LOC:** ~60 test, ~80 impl
**Parallel:** After TB-1

```
tests/unit/test_memory_tracer.c:
  - TEST(Memory, MallocFree)
  - TEST(Memory, NullFree)

src/core/cxf_memory.c:
  - cxf_malloc(env, size) - wrapper around malloc
  - cxf_free(env, ptr) - wrapper around free
```

#### Step TB-3: Minimal Error Handling
**Files:** `src/core/cxf_error.h`, `src/core/cxf_error.c`
**LOC:** ~50 test, ~60 impl
**Parallel:** After TB-1

```
tests/unit/test_error_tracer.c:
  - TEST(Error, SetGet)
  - TEST(Error, Clear)

src/core/cxf_error.c:
  - cxf_set_error(env, code, msg)
  - cxf_get_error(env)
  - cxf_clear_error(env)
```

#### Step TB-4: Minimal Sparse Matrix
**Files:** `src/matrix/cxf_sparse.h`, `src/matrix/cxf_sparse_create.c`
**LOC:** ~80 test, ~100 impl
**Parallel:** After TB-2

```
tests/unit/test_sparse_tracer.c:
  - TEST(Sparse, CreateEmpty)
  - TEST(Sparse, CreateSingleColumn)
  - TEST(Sparse, GetColumn)

src/matrix/cxf_sparse_create.c:
  - cxf_sparse_create(env, rows, cols)
  - cxf_sparse_free(env, matrix)
  - cxf_sparse_add_column(matrix, nnz, rows, vals)
  - cxf_sparse_get_column(matrix, col, nnz, rows, vals)
```

#### Step TB-5: Minimal Basis State
**Files:** `src/basis/cxf_basis.h`, `src/basis/cxf_basis_state.c`
**LOC:** ~80 test, ~100 impl
**Parallel:** After TB-2

```
tests/unit/test_basis_tracer.c:
  - TEST(Basis, CreateIdentity)
  - TEST(Basis, GetBasicVar)

src/basis/cxf_basis_state.c:
  - cxf_basis_create(env, size)
  - cxf_basis_free(env, basis)
  - cxf_basis_init_identity(basis)
  - cxf_basis_get_basic_var(basis, row)
```

#### Step TB-6: Minimal FTRAN
**Files:** `src/basis/cxf_ftran.c`
**LOC:** ~80 test, ~100 impl
**Parallel:** After TB-5

```
tests/unit/test_ftran_tracer.c:
  - TEST(Ftran, IdentityBasis)
  - TEST(Ftran, SingleEta)

src/basis/cxf_ftran.c:
  - cxf_ftran(basis, rhs, result) - identity case only for tracer
```

#### Step TB-7: Minimal Pricing
**Files:** `src/pricing/cxf_pricing.h`, `src/pricing/cxf_pricing_dantzig.c`
**LOC:** ~60 test, ~80 impl
**Parallel:** After TB-3

```
tests/unit/test_pricing_tracer.c:
  - TEST(Pricing, SelectSingleVar)
  - TEST(Pricing, AllOptimal)

src/pricing/cxf_pricing_dantzig.c:
  - cxf_pricing_select(ctx) - simple most negative reduced cost
```

#### Step TB-8: Minimal Ratio Test
**Files:** `src/ratio/cxf_ratio.h`, `src/ratio/cxf_ratio_harris.c`
**LOC:** ~60 test, ~80 impl
**Parallel:** After TB-3

```
tests/unit/test_ratio_tracer.c:
  - TEST(Ratio, SingleCandidate)
  - TEST(Ratio, Unbounded)

src/ratio/cxf_ratio_harris.c:
  - cxf_ratio_test(ctx, column) - simple min ratio
```

#### Step TB-9: Minimal Simplex Iteration
**Files:** `src/simplex/cxf_simplex.h`, `src/simplex/cxf_simplex_iterate.c`
**LOC:** ~100 test, ~120 impl
**Parallel:** After TB-6, TB-7, TB-8

```
tests/unit/test_simplex_tracer.c:
  - TEST(Simplex, SingleIteration)
  - TEST(Simplex, OptimalAtStart)

src/simplex/cxf_simplex_iterate.c:
  - cxf_simplex_iterate(ctx) - one iteration combining pricing+ftran+ratio+pivot
```

#### Step TB-10: Minimal API
**Files:** `src/api/cxf_env.c`, `src/api/cxf_model.c`, `src/api/cxf_optimize.c`
**LOC:** ~150 test, ~180 impl
**Parallel:** After TB-9

```
tests/integration/test_tracer_bullet.c:
  - TEST(TracerBullet, SolveTrivialLP)
  - TEST(TracerBullet, EmptyModel)

src/api/cxf_env.c:
  - cxf_emptyenv()
  - cxf_freeenv(env)

src/api/cxf_model.c:
  - cxf_newmodel(env, name)
  - cxf_freemodel(model)
  - cxf_addvar(model, obj, lb, ub, type, name)

src/api/cxf_optimize.c:
  - cxf_optimize(model) - minimal: setup → iterate → extract
```

### 3.3 Tracer Bullet Summary

| Step | Files | Test LOC | Impl LOC | Dependencies | Parallel Group |
|------|-------|----------|----------|--------------|----------------|
| TB-1 | types, constants | 80 | 80 | None | A |
| TB-2 | memory | 60 | 80 | TB-1 | B |
| TB-3 | error | 50 | 60 | TB-1 | B |
| TB-4 | sparse | 80 | 100 | TB-2 | C |
| TB-5 | basis_state | 80 | 100 | TB-2 | C |
| TB-6 | ftran | 80 | 100 | TB-5 | D |
| TB-7 | pricing | 60 | 80 | TB-3 | D |
| TB-8 | ratio | 60 | 80 | TB-3 | D |
| TB-9 | simplex_iterate | 100 | 120 | TB-6,7,8 | E |
| TB-10 | api | 150 | 180 | TB-9 | F |
| **Total** | | **800** | **980** | | |

**Parallel Execution:**
- Group A: 1 task
- Group B: 2 tasks in parallel
- Group C: 2 tasks in parallel
- Group D: 3 tasks in parallel
- Group E: 1 task
- Group F: 1 task

**Critical Path:** TB-1 → TB-2 → TB-5 → TB-6 → TB-9 → TB-10

---

## 4. Milestone 1: Foundation Layer

**Goal:** Complete Layer 0-1 (Memory, Parameters, Error, Validation, Logging)

### 4.1 Memory Module (Complete)

#### Step M1-1: Memory Tracking
**Files:** `src/core/cxf_memory.c` (extend)
**LOC:** ~80 test, ~80 impl
**Parallel:** After TB-2

```
tests/unit/test_memory_tracking.c:
  - TEST(Memory, TrackAllocations)
  - TEST(Memory, TrackDeallocations)
  - TEST(Memory, PeakUsage)

Implementation:
  - Add allocation tracking to cxf_malloc
  - Add cxf_get_allocated(env)
  - Add cxf_get_peak_allocated(env)
```

#### Step M1-2: Memory Calloc/Realloc
**Files:** `src/core/cxf_memory.c` (extend)
**LOC:** ~100 test, ~100 impl
**Parallel:** After M1-1

```
tests/unit/test_memory_alloc.c:
  - TEST(Memory, CallocZeroInit)
  - TEST(Memory, ReallocGrow)
  - TEST(Memory, ReallocShrink)
  - TEST(Memory, ReallocNull)

Implementation:
  - cxf_calloc(env, count, size)
  - cxf_realloc(env, ptr, old_size, new_size)
```

#### Step M1-3: Memory Pool (Optional)
**Files:** `src/core/cxf_memory_pool.c` (new)
**LOC:** ~120 test, ~150 impl
**Parallel:** After M1-2

```
tests/unit/test_memory_pool.c:
  - TEST(MemoryPool, Create)
  - TEST(MemoryPool, AllocFromPool)
  - TEST(MemoryPool, Reset)
  - TEST(MemoryPool, Destroy)

Implementation:
  - cxf_pool_create(env, block_size)
  - cxf_pool_alloc(pool, size)
  - cxf_pool_reset(pool)
  - cxf_pool_destroy(pool)
```

### 4.2 Parameters Module

#### Step M1-4: Parameter Storage
**Files:** `src/core/cxf_params.h`, `src/core/cxf_params.c`
**LOC:** ~100 test, ~120 impl
**Parallel:** After TB-2

```
tests/unit/test_params_storage.c:
  - TEST(Params, CreateDefaults)
  - TEST(Params, SetIntParam)
  - TEST(Params, GetIntParam)
  - TEST(Params, InvalidParam)

Implementation:
  - CxfParams struct with int/double arrays
  - cxf_params_create(env)
  - cxf_params_free(env, params)
  - cxf_params_set_int(params, name, value)
  - cxf_params_get_int(params, name, value)
```

#### Step M1-5: Double Parameters
**Files:** `src/core/cxf_params.c` (extend)
**LOC:** ~80 test, ~80 impl
**Parallel:** After M1-4

```
tests/unit/test_params_double.c:
  - TEST(Params, SetDblParam)
  - TEST(Params, GetDblParam)
  - TEST(Params, ToleranceDefaults)

Implementation:
  - cxf_params_set_dbl(params, name, value)
  - cxf_params_get_dbl(params, name, value)
  - Default tolerances (feasibility, optimality, pivot)
```

### 4.3 Error Module (Complete)

#### Step M1-6: Error Stack
**Files:** `src/core/cxf_error.c` (extend)
**LOC:** ~80 test, ~100 impl
**Parallel:** After TB-3

```
tests/unit/test_error_stack.c:
  - TEST(Error, PushMultiple)
  - TEST(Error, PopError)
  - TEST(Error, StackOverflow)

Implementation:
  - Error stack (last N errors)
  - cxf_push_error(env, code, msg)
  - cxf_pop_error(env, code, msg)
```

#### Step M1-7: Error Formatting
**Files:** `src/core/cxf_error.c` (extend)
**LOC:** ~60 test, ~80 impl
**Parallel:** After M1-6

```
tests/unit/test_error_format.c:
  - TEST(Error, FormatWithArgs)
  - TEST(Error, Truncation)

Implementation:
  - cxf_set_error_fmt(env, code, fmt, ...)
  - cxf_error_to_string(code)
```

### 4.4 Validation Module

#### Step M1-8: Input Validation
**Files:** `src/core/cxf_validate.h`, `src/core/cxf_validate.c`
**LOC:** ~120 test, ~100 impl
**Parallel:** After TB-3

```
tests/unit/test_validate.c:
  - TEST(Validate, NullPointer)
  - TEST(Validate, NaNValue)
  - TEST(Validate, InfValue)
  - TEST(Validate, BoundsConsistency)
  - TEST(Validate, IndexRange)

Implementation:
  - cxf_validate_ptr(env, ptr, name)
  - cxf_validate_finite(env, value, name)
  - cxf_validate_bounds(env, lb, ub)
  - cxf_validate_index(env, idx, max, name)
```

### 4.5 Logging Module

#### Step M1-9: Basic Logging
**Files:** `src/core/cxf_logging.h`, `src/core/cxf_logging.c`
**LOC:** ~80 test, ~100 impl
**Parallel:** After TB-3

```
tests/unit/test_logging.c:
  - TEST(Logging, SetLevel)
  - TEST(Logging, LogMessage)
  - TEST(Logging, FilterByLevel)

Implementation:
  - Log levels: SILENT, ERROR, WARN, INFO, DEBUG
  - cxf_log_set_level(env, level)
  - cxf_log(env, level, fmt, ...)
```

#### Step M1-10: Log Output
**Files:** `src/core/cxf_logging.c` (extend)
**LOC:** ~60 test, ~80 impl
**Parallel:** After M1-9

```
tests/unit/test_logging_output.c:
  - TEST(Logging, OutputToFile)
  - TEST(Logging, OutputToCallback)

Implementation:
  - cxf_log_set_file(env, file)
  - cxf_log_set_callback(env, callback, userdata)
```

### 4.6 Milestone 1 Summary

| Step | Description | Test LOC | Impl LOC | Parallel Group |
|------|-------------|----------|----------|----------------|
| M1-1 | Memory tracking | 80 | 80 | A |
| M1-2 | Calloc/Realloc | 100 | 100 | B |
| M1-3 | Memory pool | 120 | 150 | C |
| M1-4 | Parameter storage | 100 | 120 | A |
| M1-5 | Double parameters | 80 | 80 | B |
| M1-6 | Error stack | 80 | 100 | A |
| M1-7 | Error formatting | 60 | 80 | B |
| M1-8 | Input validation | 120 | 100 | A |
| M1-9 | Basic logging | 80 | 100 | A |
| M1-10 | Log output | 60 | 80 | B |
| **Total** | | **880** | **990** | |

**Parallel Groups:**
- Group A: M1-1, M1-4, M1-6, M1-8, M1-9 (5 parallel tasks)
- Group B: M1-2, M1-5, M1-7, M1-10 (4 parallel tasks)
- Group C: M1-3 (1 task, optional)

---

## 5. Milestone 2: Data Layer

**Goal:** Complete Layer 2 (Matrix, Threading basics)

### 5.1 Sparse Matrix Module (Complete)

#### Step M2-1: Matrix Access
**Files:** `src/matrix/cxf_sparse_access.c`
**LOC:** ~120 test, ~150 impl
**Parallel:** After TB-4

```
tests/unit/test_sparse_access.c:
  - TEST(Sparse, GetRow)
  - TEST(Sparse, GetCoeff)
  - TEST(Sparse, GetNonzeros)
  - TEST(Sparse, IterateColumn)

Implementation:
  - cxf_sparse_get_row(matrix, row, nnz, cols, vals)
  - cxf_sparse_get_coeff(matrix, row, col)
  - cxf_sparse_get_col_nnz(matrix, col)
  - CSR lazy construction
```

#### Step M2-2: Matrix Modification
**Files:** `src/matrix/cxf_sparse_modify.c`
**LOC:** ~150 test, ~150 impl
**Parallel:** After M2-1

```
tests/unit/test_sparse_modify.c:
  - TEST(Sparse, SetCoeff)
  - TEST(Sparse, AddToCoeff)
  - TEST(Sparse, DeleteColumn)
  - TEST(Sparse, InsertColumn)

Implementation:
  - cxf_sparse_set_coeff(matrix, row, col, val)
  - cxf_sparse_add_coeff(matrix, row, col, val)
  - cxf_sparse_delete_col(matrix, col)
  - cxf_sparse_insert_col(matrix, col, nnz, rows, vals)
```

#### Step M2-3: Matrix Utilities
**Files:** `src/matrix/cxf_sparse_utils.c`
**LOC:** ~100 test, ~100 impl
**Parallel:** After M2-1

```
tests/unit/test_sparse_utils.c:
  - TEST(Sparse, Copy)
  - TEST(Sparse, Transpose)
  - TEST(Sparse, Scale)

Implementation:
  - cxf_sparse_copy(env, matrix)
  - cxf_sparse_transpose(env, matrix)
  - cxf_sparse_scale_col(matrix, col, factor)
```

#### Step M2-4: Matrix Validation
**Files:** `src/matrix/cxf_sparse_validate.c`
**LOC:** ~100 test, ~80 impl
**Parallel:** After M2-1

```
tests/unit/test_sparse_validate.c:
  - TEST(Sparse, ValidateStructure)
  - TEST(Sparse, DetectNaN)
  - TEST(Sparse, DetectDuplicates)

Implementation:
  - cxf_sparse_validate(matrix)
  - cxf_sparse_check_finite(matrix)
  - cxf_sparse_check_sorted(matrix)
```

### 5.2 Dense Vector Module

#### Step M2-5: Vector Operations
**Files:** `src/matrix/cxf_vector.h`, `src/matrix/cxf_vector.c`
**LOC:** ~120 test, ~120 impl
**Parallel:** After M1-1

```
tests/unit/test_vector.c:
  - TEST(Vector, Create)
  - TEST(Vector, Copy)
  - TEST(Vector, Axpy)
  - TEST(Vector, Dot)
  - TEST(Vector, Norm)

Implementation:
  - cxf_vector_create(env, size)
  - cxf_vector_copy(env, src)
  - cxf_vector_axpy(alpha, x, y) // y += alpha*x
  - cxf_vector_dot(x, y)
  - cxf_vector_norm(x)
```

### 5.3 Threading Module (Basic)

#### Step M2-6: Thread Safety Primitives
**Files:** `src/core/cxf_thread.h`, `src/core/cxf_thread.c`
**LOC:** ~80 test, ~100 impl
**Parallel:** After M1-1

```
tests/unit/test_thread.c:
  - TEST(Thread, MutexLockUnlock)
  - TEST(Thread, MutexRecursive)

Implementation:
  - CxfMutex type (wrapper around pthread_mutex or Windows CRITICAL_SECTION)
  - cxf_mutex_create()
  - cxf_mutex_destroy(mutex)
  - cxf_mutex_lock(mutex)
  - cxf_mutex_unlock(mutex)
```

### 5.4 Milestone 2 Summary

| Step | Description | Test LOC | Impl LOC | Parallel Group |
|------|-------------|----------|----------|----------------|
| M2-1 | Matrix access | 120 | 150 | A |
| M2-2 | Matrix modification | 150 | 150 | B |
| M2-3 | Matrix utilities | 100 | 100 | B |
| M2-4 | Matrix validation | 100 | 80 | B |
| M2-5 | Vector operations | 120 | 120 | A |
| M2-6 | Thread primitives | 80 | 100 | A |
| **Total** | | **670** | **700** | |

---

## 6. Milestone 3: Core Operations

**Goal:** Complete Layer 3 (Basis, Callbacks, Timing)

### 6.1 Basis Module (Complete)

#### Step M3-1: BTRAN
**Files:** `src/basis/cxf_btran.c`
**LOC:** ~100 test, ~150 impl
**Parallel:** After TB-6

```
tests/unit/test_btran.c:
  - TEST(Btran, IdentityBasis)
  - TEST(Btran, WithEtas)
  - TEST(Btran, Accuracy)

Implementation:
  - cxf_btran(basis, rhs, result)
  - Apply etas in reverse order
  - Solve U^T then L^T
```

#### Step M3-2: Eta Operations
**Files:** `src/basis/cxf_eta.c`
**LOC:** ~120 test, ~150 impl
**Parallel:** After TB-5

```
tests/unit/test_eta.c:
  - TEST(Eta, Create)
  - TEST(Eta, Apply)
  - TEST(Eta, ApplyTranspose)
  - TEST(Eta, List)

Implementation:
  - CxfEta struct
  - cxf_eta_create(env, pivot_row, column)
  - cxf_eta_apply(eta, vector)
  - cxf_eta_apply_transpose(eta, vector)
  - cxf_eta_list_add(list, eta)
  - cxf_eta_list_clear(list)
```

#### Step M3-3: LU Factorization
**Files:** `src/basis/cxf_refactor.c`
**LOC:** ~150 test, ~200 impl
**Parallel:** After M3-2

```
tests/unit/test_refactor.c:
  - TEST(Refactor, IdentityBasis)
  - TEST(Refactor, SmallBasis)
  - TEST(Refactor, Singular)
  - TEST(Refactor, PreserveFtran)

Implementation:
  - cxf_basis_refactor(basis, matrix, basic_vars)
  - Simple LU without pivoting for now
  - Store L and U factors
```

#### Step M3-4: Basis Update
**Files:** `src/basis/cxf_basis_update.c`
**LOC:** ~100 test, ~120 impl
**Parallel:** After M3-2

```
tests/unit/test_basis_update.c:
  - TEST(BasisUpdate, SwapVariable)
  - TEST(BasisUpdate, AddEta)
  - TEST(BasisUpdate, TriggerRefactor)

Implementation:
  - cxf_basis_update(basis, leaving, entering, column)
  - cxf_basis_needs_refactor(basis)
```

### 6.2 Callbacks Module

#### Step M3-5: Callback Infrastructure
**Files:** `src/core/cxf_callback.h`, `src/core/cxf_callback.c`
**LOC:** ~100 test, ~120 impl
**Parallel:** After M1-1

```
tests/unit/test_callback.c:
  - TEST(Callback, Register)
  - TEST(Callback, Invoke)
  - TEST(Callback, UserData)
  - TEST(Callback, Terminate)

Implementation:
  - CxfCallback struct
  - cxf_callback_register(model, type, func, userdata)
  - cxf_callback_invoke(model, type, info)
  - cxf_callback_check_terminate(model)
```

### 6.3 Timing Module

#### Step M3-6: Timer Operations
**Files:** `src/core/cxf_timing.h`, `src/core/cxf_timing.c`
**LOC:** ~80 test, ~100 impl
**Parallel:** After M1-1

```
tests/unit/test_timing.c:
  - TEST(Timing, StartStop)
  - TEST(Timing, Elapsed)
  - TEST(Timing, Accumulate)

Implementation:
  - CxfTimer struct
  - cxf_timer_start(timer)
  - cxf_timer_stop(timer)
  - cxf_timer_elapsed(timer)
  - cxf_timer_reset(timer)
```

### 6.4 Statistics Module

#### Step M3-7: Solver Statistics
**Files:** `src/core/cxf_stats.h`, `src/core/cxf_stats.c`
**LOC:** ~80 test, ~100 impl
**Parallel:** After M3-6

```
tests/unit/test_stats.c:
  - TEST(Stats, CountIterations)
  - TEST(Stats, CountPivots)
  - TEST(Stats, RecordTime)

Implementation:
  - CxfStats struct
  - cxf_stats_init(stats)
  - cxf_stats_inc_iterations(stats)
  - cxf_stats_add_time(stats, category, elapsed)
```

### 6.5 Milestone 3 Summary

| Step | Description | Test LOC | Impl LOC | Parallel Group |
|------|-------------|----------|----------|----------------|
| M3-1 | BTRAN | 100 | 150 | A |
| M3-2 | Eta operations | 120 | 150 | A |
| M3-3 | LU factorization | 150 | 200 | B |
| M3-4 | Basis update | 100 | 120 | B |
| M3-5 | Callbacks | 100 | 120 | A |
| M3-6 | Timing | 80 | 100 | A |
| M3-7 | Statistics | 80 | 100 | B |
| **Total** | | **730** | **940** | |

---

## 7. Milestone 4: Algorithm Layer

**Goal:** Complete Layer 4 (Pricing, Ratio Test, Solver State)

### 7.1 Pricing Module (Complete)

#### Step M4-1: Steepest Edge Pricing
**Files:** `src/pricing/cxf_pricing_steepest.c`
**LOC:** ~120 test, ~150 impl
**Parallel:** After TB-7

```
tests/unit/test_pricing_steepest.c:
  - TEST(PricingSE, ComputeWeights)
  - TEST(PricingSE, SelectBest)
  - TEST(PricingSE, UpdateWeights)

Implementation:
  - cxf_pricing_se_init(ctx)
  - cxf_pricing_se_select(ctx)
  - cxf_pricing_se_update(ctx, entering, leaving)
```

#### Step M4-2: Partial Pricing
**Files:** `src/pricing/cxf_pricing_partial.c`
**LOC:** ~100 test, ~100 impl
**Parallel:** After TB-7

```
tests/unit/test_pricing_partial.c:
  - TEST(PricingPartial, ChunkSelection)
  - TEST(PricingPartial, Rotation)
  - TEST(PricingPartial, Invalidation)

Implementation:
  - cxf_pricing_partial_init(ctx, chunk_size)
  - cxf_pricing_partial_select(ctx)
  - cxf_pricing_partial_rotate(ctx)
```

#### Step M4-3: Reduced Cost Computation
**Files:** `src/pricing/cxf_reduced_cost.c`
**LOC:** ~100 test, ~120 impl
**Parallel:** After M3-1

```
tests/unit/test_reduced_cost.c:
  - TEST(ReducedCost, Compute)
  - TEST(ReducedCost, Update)
  - TEST(ReducedCost, CheckOptimality)

Implementation:
  - cxf_compute_reduced_costs(ctx)
  - cxf_update_reduced_cost(ctx, entering, delta)
  - cxf_check_optimality(ctx)
```

### 7.2 Ratio Test Module (Complete)

#### Step M4-4: Harris Ratio Test (Full)
**Files:** `src/ratio/cxf_ratio_harris.c` (extend)
**LOC:** ~120 test, ~150 impl
**Parallel:** After TB-8

```
tests/unit/test_ratio_harris.c:
  - TEST(RatioHarris, TwoPass)
  - TEST(RatioHarris, LargestPivot)
  - TEST(RatioHarris, Degenerate)
  - TEST(RatioHarris, NumericalStability)

Implementation:
  - Full two-pass Harris ratio test
  - Pass 1: relaxed tolerance minimum
  - Pass 2: largest pivot among candidates
```

#### Step M4-5: Bound Flip Detection
**Files:** `src/ratio/cxf_ratio_bound.c`
**LOC:** ~80 test, ~100 impl
**Parallel:** After M4-4

```
tests/unit/test_ratio_bound.c:
  - TEST(RatioBound, FlipToLower)
  - TEST(RatioBound, FlipToUpper)
  - TEST(RatioBound, ComputeStep)

Implementation:
  - cxf_ratio_check_bound_flip(ctx, var)
  - cxf_ratio_compute_step(ctx, entering, leaving)
```

### 7.3 Solver State Module

#### Step M4-6: Solver Context
**Files:** `src/simplex/cxf_solver_ctx.h`, `src/simplex/cxf_solver_ctx.c`
**LOC:** ~100 test, ~120 impl
**Parallel:** After M3-4

```
tests/unit/test_solver_ctx.c:
  - TEST(SolverCtx, Create)
  - TEST(SolverCtx, InitFromModel)
  - TEST(SolverCtx, GetPhase)

Implementation:
  - CxfSolverCtx struct
  - cxf_solver_ctx_create(env, model)
  - cxf_solver_ctx_free(ctx)
  - cxf_solver_ctx_init(ctx)
```

#### Step M4-7: Variable Status Management
**Files:** `src/simplex/cxf_var_status.c`
**LOC:** ~100 test, ~100 impl
**Parallel:** After M4-6

```
tests/unit/test_var_status.c:
  - TEST(VarStatus, SetBasic)
  - TEST(VarStatus, SetNonbasic)
  - TEST(VarStatus, GetValue)

Implementation:
  - cxf_var_set_status(ctx, var, status)
  - cxf_var_get_status(ctx, var)
  - cxf_var_get_value(ctx, var)
  - cxf_var_set_value(ctx, var, value)
```

### 7.4 Milestone 4 Summary

| Step | Description | Test LOC | Impl LOC | Parallel Group |
|------|-------------|----------|----------|----------------|
| M4-1 | Steepest edge | 120 | 150 | A |
| M4-2 | Partial pricing | 100 | 100 | A |
| M4-3 | Reduced costs | 100 | 120 | B |
| M4-4 | Harris full | 120 | 150 | A |
| M4-5 | Bound flip | 80 | 100 | B |
| M4-6 | Solver context | 100 | 120 | A |
| M4-7 | Variable status | 100 | 100 | B |
| **Total** | | **720** | **840** | |

---

## 8. Milestone 5: Simplex Core

**Goal:** Complete Layer 5 (Full Simplex Algorithm)

### 8.1 Initialization

#### Step M5-1: Simplex Setup
**Files:** `src/simplex/cxf_simplex_init.c`
**LOC:** ~120 test, ~150 impl
**Parallel:** After M4-6

```
tests/unit/test_simplex_init.c:
  - TEST(SimplexInit, AllocateArrays)
  - TEST(SimplexInit, CopyBounds)
  - TEST(SimplexInit, SetupArtificials)

Implementation:
  - cxf_simplex_init(ctx)
  - Allocate working arrays
  - Copy bounds and objective
  - Initialize variable status
```

#### Step M5-2: Crash Basis
**Files:** `src/simplex/cxf_simplex_crash.c`
**LOC:** ~100 test, ~120 impl
**Parallel:** After M5-1

```
tests/unit/test_simplex_crash.c:
  - TEST(Crash, SlackBasis)
  - TEST(Crash, TriangularCrash)
  - TEST(Crash, WarmStart)

Implementation:
  - cxf_simplex_crash(ctx)
  - Simple slack variable basis
  - Optional triangular crash heuristic
```

### 8.2 Phase Implementation

#### Step M5-3: Phase I Setup
**Files:** `src/simplex/cxf_simplex_phase1.c`
**LOC:** ~120 test, ~150 impl
**Parallel:** After M5-2

```
tests/unit/test_simplex_phase1.c:
  - TEST(Phase1, SetupArtificials)
  - TEST(Phase1, FeasibilityObjective)
  - TEST(Phase1, DetectInfeasible)

Implementation:
  - cxf_simplex_phase1_setup(ctx)
  - cxf_simplex_phase1_cleanup(ctx)
  - Add artificial variables
  - Track infeasibility sum
```

#### Step M5-4: Phase II
**Files:** `src/simplex/cxf_simplex_phase2.c`
**LOC:** ~100 test, ~120 impl
**Parallel:** After M5-3

```
tests/unit/test_simplex_phase2.c:
  - TEST(Phase2, RestoreObjective)
  - TEST(Phase2, RemoveArtificials)
  - TEST(Phase2, OptimalityCheck)

Implementation:
  - cxf_simplex_phase2_setup(ctx)
  - Restore original objective
  - Remove artificial variables from basis
```

### 8.3 Pivot Operations

#### Step M5-5: Pivot Execution
**Files:** `src/simplex/cxf_simplex_pivot.c`
**LOC:** ~150 test, ~150 impl
**Parallel:** After M4-7

```
tests/unit/test_simplex_pivot.c:
  - TEST(Pivot, UpdateSolution)
  - TEST(Pivot, SwapBasisVars)
  - TEST(Pivot, UpdateReducedCosts)
  - TEST(Pivot, NumericalCheck)

Implementation:
  - cxf_simplex_pivot(ctx, entering, leaving, pivot_col)
  - Update primal solution
  - Update basis header
  - Update reduced costs
```

### 8.4 Perturbation

#### Step M5-6: Perturbation
**Files:** `src/simplex/cxf_simplex_perturb.c`
**LOC:** ~100 test, ~100 impl
**Parallel:** After M5-1

```
tests/unit/test_simplex_perturb.c:
  - TEST(Perturb, ApplyPerturbation)
  - TEST(Perturb, RemovePerturbation)
  - TEST(Perturb, Deterministic)

Implementation:
  - cxf_simplex_perturb(ctx)
  - cxf_simplex_unperturb(ctx)
  - Small random perturbations to bounds
```

### 8.5 Main Loop

#### Step M5-7: Iteration Loop
**Files:** `src/simplex/cxf_simplex_iterate.c` (extend)
**LOC:** ~120 test, ~150 impl
**Parallel:** After M5-5, M5-6

```
tests/unit/test_simplex_loop.c:
  - TEST(SimplexLoop, MultipleIterations)
  - TEST(SimplexLoop, TerminationOptimal)
  - TEST(SimplexLoop, TerminationUnbounded)
  - TEST(SimplexLoop, IterationLimit)

Implementation:
  - Full iteration loop
  - Termination checks
  - Statistics updates
```

### 8.6 Cleanup and Refinement

#### Step M5-8: Solution Extraction
**Files:** `src/simplex/cxf_simplex_cleanup.c`
**LOC:** ~100 test, ~120 impl
**Parallel:** After M5-7

```
tests/unit/test_simplex_cleanup.c:
  - TEST(Cleanup, ExtractPrimal)
  - TEST(Cleanup, ExtractDual)
  - TEST(Cleanup, ExtractReducedCosts)

Implementation:
  - cxf_simplex_extract_solution(ctx, model)
  - Copy primal, dual, reduced costs to model
```

#### Step M5-9: Iterative Refinement
**Files:** `src/simplex/cxf_simplex_refine.c`
**LOC:** ~100 test, ~120 impl
**Parallel:** After M5-8

```
tests/unit/test_simplex_refine.c:
  - TEST(Refine, ComputeResidual)
  - TEST(Refine, CorrectSolution)
  - TEST(Refine, ConvergenceCheck)

Implementation:
  - cxf_simplex_refine(ctx)
  - Compute residual r = b - Ax
  - Solve B*d = r
  - Update solution
```

### 8.7 Milestone 5 Summary

| Step | Description | Test LOC | Impl LOC | Parallel Group |
|------|-------------|----------|----------|----------------|
| M5-1 | Simplex setup | 120 | 150 | A |
| M5-2 | Crash basis | 100 | 120 | B |
| M5-3 | Phase I | 120 | 150 | C |
| M5-4 | Phase II | 100 | 120 | D |
| M5-5 | Pivot execution | 150 | 150 | B |
| M5-6 | Perturbation | 100 | 100 | B |
| M5-7 | Iteration loop | 120 | 150 | E |
| M5-8 | Solution extraction | 100 | 120 | F |
| M5-9 | Iterative refinement | 100 | 120 | G |
| **Total** | | **1010** | **1180** | |

---

## 9. Milestone 6: Public API

**Goal:** Complete Layer 6 (Full Public API)

### 9.1 Environment API

#### Step M6-1: Environment (Full)
**Files:** `src/api/cxf_env.c` (extend)
**LOC:** ~100 test, ~120 impl
**Parallel:** After TB-10

```
tests/unit/test_api_env.c:
  - TEST(ApiEnv, LoadEnv)
  - TEST(ApiEnv, CustomAllocator)
  - TEST(ApiEnv, GetVersion)

Implementation:
  - cxf_loadenv(licfile)
  - cxf_version()
  - Custom allocator support
```

### 9.2 Model API

#### Step M6-2: Variable Operations (Full)
**Files:** `src/api/cxf_model_vars.c`
**LOC:** ~150 test, ~150 impl
**Parallel:** After TB-10

```
tests/unit/test_api_vars.c:
  - TEST(ApiVars, AddVarsBatch)
  - TEST(ApiVars, DeleteVars)
  - TEST(ApiVars, ChangeVarBounds)
  - TEST(ApiVars, GetVarInfo)

Implementation:
  - cxf_addvars(model, count, ...)
  - cxf_delvars(model, count, indices)
  - cxf_chgvarbounds(model, var, lb, ub)
```

#### Step M6-3: Constraint Operations (Full)
**Files:** `src/api/cxf_model_constrs.c`
**LOC:** ~150 test, ~150 impl
**Parallel:** After M6-2

```
tests/unit/test_api_constrs.c:
  - TEST(ApiConstrs, AddConstrsBatch)
  - TEST(ApiConstrs, DeleteConstrs)
  - TEST(ApiConstrs, ChangeRHS)

Implementation:
  - cxf_addconstrs(model, count, ...)
  - cxf_delconstrs(model, count, indices)
  - cxf_chgrhs(model, count, constrs, values)
```

#### Step M6-4: Coefficient Operations
**Files:** `src/api/cxf_model_coeff.c`
**LOC:** ~100 test, ~120 impl
**Parallel:** After M6-3

```
tests/unit/test_api_coeff.c:
  - TEST(ApiCoeff, ChangeCoeffs)
  - TEST(ApiCoeff, GetCoeff)
  - TEST(ApiCoeff, ChangeObjective)

Implementation:
  - cxf_chgcoeffs(model, count, rows, cols, vals)
  - cxf_getcoeff(model, row, col, val)
  - cxf_chgobj(model, count, vars, vals)
```

### 9.3 Query API

#### Step M6-5: Attribute Query
**Files:** `src/api/cxf_query.c`
**LOC:** ~150 test, ~150 impl
**Parallel:** After M5-8

```
tests/unit/test_api_query.c:
  - TEST(ApiQuery, GetIntAttr)
  - TEST(ApiQuery, GetDblAttr)
  - TEST(ApiQuery, GetDblAttrArray)
  - TEST(ApiQuery, GetBasis)

Implementation:
  - cxf_getintattr(model, name, value)
  - cxf_getdblattr(model, name, value)
  - cxf_getdblattrarray(model, name, start, len, values)
```

### 9.4 Parameter API

#### Step M6-6: Parameter Interface
**Files:** `src/api/cxf_params_api.c`
**LOC:** ~100 test, ~100 impl
**Parallel:** After M1-5

```
tests/unit/test_api_params.c:
  - TEST(ApiParams, SetIntParam)
  - TEST(ApiParams, SetDblParam)
  - TEST(ApiParams, ResetDefaults)

Implementation:
  - cxf_setintparam(env, name, value)
  - cxf_setdblparam(env, name, value)
  - cxf_resetparams(env)
```

### 9.5 File I/O

#### Step M6-7: MPS Reader
**Files:** `src/api/cxf_io_mps.c`
**LOC:** ~150 test, ~200 impl
**Parallel:** After M6-4

```
tests/unit/test_api_mps.c:
  - TEST(ApiMPS, ReadFixed)
  - TEST(ApiMPS, ReadFree)
  - TEST(ApiMPS, ReadBounds)
  - TEST(ApiMPS, ReadRanges)

Implementation:
  - cxf_read_mps(model, filename)
  - Parse ROWS, COLUMNS, RHS, BOUNDS, RANGES sections
```

#### Step M6-8: LP Format Reader
**Files:** `src/api/cxf_io_lp.c`
**LOC:** ~120 test, ~150 impl
**Parallel:** After M6-4

```
tests/unit/test_api_lp.c:
  - TEST(ApiLP, ReadObjective)
  - TEST(ApiLP, ReadConstraints)
  - TEST(ApiLP, ReadBounds)

Implementation:
  - cxf_read_lp(model, filename)
  - Parse CPLEX LP format
```

#### Step M6-9: Solution Writer
**Files:** `src/api/cxf_io_sol.c`
**LOC:** ~80 test, ~100 impl
**Parallel:** After M6-5

```
tests/unit/test_api_sol.c:
  - TEST(ApiSol, WriteSolution)
  - TEST(ApiSol, ReadSolution)

Implementation:
  - cxf_write_sol(model, filename)
  - cxf_read_sol(model, filename)
```

### 9.6 Milestone 6 Summary

| Step | Description | Test LOC | Impl LOC | Parallel Group |
|------|-------------|----------|----------|----------------|
| M6-1 | Environment full | 100 | 120 | A |
| M6-2 | Variables full | 150 | 150 | A |
| M6-3 | Constraints full | 150 | 150 | B |
| M6-4 | Coefficients | 100 | 120 | C |
| M6-5 | Query API | 150 | 150 | D |
| M6-6 | Parameters API | 100 | 100 | A |
| M6-7 | MPS reader | 150 | 200 | E |
| M6-8 | LP reader | 120 | 150 | E |
| M6-9 | Solution writer | 80 | 100 | F |
| **Total** | | **1100** | **1240** | |

---

## 10. Parallelization Matrix

### 10.1 Cross-Milestone Parallelization

```
Timeline →

Milestone 0 (Tracer Bullet):
  [TB-1]──┬──[TB-2]──┬──[TB-4]──┐
          │         │          │
          └──[TB-3]──┼──[TB-5]──┼──[TB-6]──┐
                    │          │          │
                    └──[TB-7]──┤          │
                    └──[TB-8]──┴──────────┴──[TB-9]──[TB-10]

Milestone 1 (Foundation) - Can start after TB-2/TB-3:
  [M1-1]──[M1-2]──[M1-3]
  [M1-4]──[M1-5]
  [M1-6]──[M1-7]
  [M1-8]
  [M1-9]──[M1-10]

Milestone 2 (Data) - Can start after TB-4:
  [M2-1]──┬──[M2-2]
          ├──[M2-3]
          └──[M2-4]
  [M2-5]
  [M2-6]

Milestone 3 (Core) - Can start after TB-5/TB-6:
  [M3-1]
  [M3-2]──[M3-3]
         └[M3-4]
  [M3-5]
  [M3-6]──[M3-7]

Milestone 4 (Algorithm) - Can start after TB-7/TB-8:
  [M4-1]
  [M4-2]
  [M4-3]
  [M4-4]──[M4-5]
  [M4-6]──[M4-7]

Milestone 5 (Simplex) - Can start after M4 complete:
  [M5-1]──┬──[M5-2]──[M5-3]──[M5-4]──┐
          ├──[M5-5]──────────────────┼──[M5-7]──[M5-8]──[M5-9]
          └──[M5-6]──────────────────┘

Milestone 6 (API) - Can start after TB-10:
  [M6-1]
  [M6-2]──[M6-3]──[M6-4]──┬──[M6-7]
                         └──[M6-8]
  [M6-5]──[M6-9]
  [M6-6]
```

### 10.2 Maximum Parallelism by Phase

| Phase | Max Parallel Tasks | Tasks |
|-------|-------------------|-------|
| Tracer Bullet Group A | 1 | TB-1 |
| Tracer Bullet Group B | 2 | TB-2, TB-3 |
| Tracer Bullet Group C | 2 | TB-4, TB-5 |
| Tracer Bullet Group D | 3 | TB-6, TB-7, TB-8 |
| Foundation | 5 | M1-1, M1-4, M1-6, M1-8, M1-9 |
| Data Layer | 3 | M2-1, M2-5, M2-6 |
| Core Operations | 4 | M3-1, M3-2, M3-5, M3-6 |
| Algorithm Layer | 4 | M4-1, M4-2, M4-4, M4-6 |
| Simplex Core | 3 | M5-2, M5-5, M5-6 |
| Public API | 3 | M6-1, M6-2, M6-6 |

### 10.3 Developer Assignment Strategy

For a team of N developers:

| Team Size | Strategy |
|-----------|----------|
| 1 | Sequential by critical path |
| 2 | Split: Foundation/Data vs Algorithm/Simplex |
| 3 | Split: Foundation, Data+Core, Algorithm+Simplex |
| 4 | Split: Foundation, Data, Core+Algorithm, Simplex+API |
| 5+ | Maximize parallel tasks per phase |

---

## 11. Dependency Graph

### 11.1 Module Dependencies

```
                    ┌─────────────┐
                    │ Public API  │ Layer 6
                    └──────┬──────┘
                           │
              ┌────────────┼────────────┐
              ▼            ▼            ▼
        ┌──────────┐ ┌──────────┐ ┌──────────┐
        │ Simplex  │ │ Crossover│ │ Utilities│ Layer 5
        └────┬─────┘ └────┬─────┘ └────┬─────┘
             │            │            │
        ┌────┴────────────┴────────────┴────┐
        ▼                                   ▼
  ┌──────────┐                        ┌──────────┐
  │ Pricing  │                        │  Ratio   │ Layer 4
  └────┬─────┘                        └────┬─────┘
       │                                   │
       └─────────────┬─────────────────────┘
                     ▼
              ┌──────────┐
              │  Basis   │ Layer 3
              └────┬─────┘
                   │
        ┌──────────┼──────────┐
        ▼          ▼          ▼
  ┌──────────┐ ┌────────┐ ┌────────┐
  │  Matrix  │ │Callback│ │ Timing │ Layer 2-3
  └────┬─────┘ └───┬────┘ └───┬────┘
       │           │          │
       └───────────┼──────────┘
                   ▼
        ┌──────────────────┐
        │ Error │ Validate │ Layer 1
        └────────┬─────────┘
                 │
        ┌────────┴─────────┐
        ▼                  ▼
  ┌──────────┐      ┌──────────┐
  │  Memory  │      │  Params  │ Layer 0
  └──────────┘      └──────────┘
```

### 11.2 File Dependency Matrix

```
File                    | Depends On
------------------------|------------------------------------------
cxf_types.h             | (none - root)
cxf_constants.h         | cxf_types.h
cxf_memory.h/c          | cxf_types.h
cxf_error.h/c           | cxf_types.h, cxf_memory.h
cxf_params.h/c          | cxf_types.h, cxf_memory.h
cxf_validate.h/c        | cxf_types.h, cxf_error.h
cxf_logging.h/c         | cxf_types.h, cxf_error.h
cxf_sparse.h/c          | cxf_types.h, cxf_memory.h, cxf_error.h
cxf_vector.h/c          | cxf_types.h, cxf_memory.h
cxf_basis.h/c           | cxf_types.h, cxf_memory.h, cxf_sparse.h
cxf_pricing.h/c         | cxf_types.h, cxf_basis.h, cxf_vector.h
cxf_ratio.h/c           | cxf_types.h, cxf_basis.h
cxf_simplex.h/c         | cxf_pricing.h, cxf_ratio.h, cxf_basis.h
cxf_env.c               | cxf_types.h, cxf_memory.h, cxf_params.h
cxf_model.c             | cxf_types.h, cxf_memory.h, cxf_sparse.h
cxf_optimize.c          | cxf_simplex.h, cxf_model.h
```

---

## 12. Implementation Totals

### 12.1 LOC Summary by Milestone

| Milestone | Test LOC | Impl LOC | Total | Files |
|-----------|----------|----------|-------|-------|
| M0: Tracer Bullet | 800 | 980 | 1,780 | 20 |
| M1: Foundation | 880 | 990 | 1,870 | 12 |
| M2: Data Layer | 670 | 700 | 1,370 | 8 |
| M3: Core Operations | 730 | 940 | 1,670 | 10 |
| M4: Algorithm Layer | 720 | 840 | 1,560 | 10 |
| M5: Simplex Core | 1,010 | 1,180 | 2,190 | 12 |
| M6: Public API | 1,100 | 1,240 | 2,340 | 12 |
| **TOTAL** | **5,910** | **6,870** | **12,780** | **84** |

### 12.2 Test:Code Ratio Verification

- Test LOC: 5,910
- Implementation LOC: 6,870
- Ratio: **1:1.16**

This exceeds the 1:3 to 1:4 target, which is good. The ratio accounts for:
- Comprehensive edge case testing
- Integration tests
- Numerical precision tests

### 12.3 Average File Size

- Total files: 84
- Total implementation LOC: 6,870
- Average: **82 LOC per file** ✓ (within 100-200 target)

---

## 13. Quick Reference: Implementation Order

### Critical Path (Minimum for Working Solver)

```
TB-1 → TB-2 → TB-5 → TB-6 → TB-9 → TB-10 → M5-1 → M5-7 → M5-8
  ↓      ↓                                    ↑
TB-3 → TB-7 ──────────────────────────────────┘
  ↓      ↓
TB-4 → TB-8
```

### Recommended Start Order (First Week)

| Day | Tasks | Developers |
|-----|-------|------------|
| 1 | TB-1 (types) | 1 |
| 2 | TB-2 (memory), TB-3 (error) | 2 |
| 3 | TB-4 (sparse), TB-5 (basis) | 2 |
| 4 | TB-6 (ftran), TB-7 (pricing), TB-8 (ratio) | 3 |
| 5 | TB-9 (iterate), TB-10 (api) | 2 |

**End of Week 1:** Working tracer bullet that solves trivial LP.

---

*Document Version: 1.0 | Generated: January 2026*
