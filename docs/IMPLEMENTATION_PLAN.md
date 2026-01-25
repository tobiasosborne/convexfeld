# ConvexFeld Implementation Plan

## Overview

**Total Functions:** 142 (30 public API + 112 internal)
**Target:** C99 with TDD (1:3-1:4 test:code ratio)
**File Size Limit:** 100-200 LOC per file
**Task Granularity:** ~100 LOC per implementation step

---

## Milestone 0: Tracer Bullet (Vertical Slice)

The tracer bullet provides a minimal end-to-end test of every layer. It solves a trivial 2-variable LP problem.

### Tracer Bullet Scope

```
Input: min c^T x subject to Ax = b, l <= x <= u (2 vars, 1 constraint)
Output: Optimal x, objective value, status
```

**Required Structures (6):**
- CxfEnv, CxfModel, SparseMatrix, SolverContext, BasisState, EtaFactors

**Required Functions (~25):**
- Layer 0: cxf_malloc, cxf_calloc, cxf_free (3)
- Layer 1: cxf_error, cxf_checkenv (2)
- Layer 2: cxf_get_timestamp (1)
- Layer 3: cxf_ftran_simple, cxf_btran_simple (2 simplified stubs)
- Layer 4: cxf_pricing_simple (1 simplified stub)
- Layer 5: cxf_simplex_init, cxf_simplex_crash, cxf_simplex_setup, cxf_simplex_iterate, cxf_simplex_step, cxf_extract_solution, cxf_simplex_final, cxf_ratio_test_simple (8)
- Layer 6: cxf_emptyenv, cxf_freeenv, cxf_newmodel, cxf_freemodel, cxf_addvar, cxf_addconstr, cxf_optimize, cxf_getdblattr (8)

**Estimated LOC:** ~1,500 (implementation) + ~500 (tests) = ~2,000 total

---

## Directory Structure

```
convexfeld/
├── include/
│   └── convexfeld.h              # Public API header (100 LOC)
├── src/
│   ├── internal.h                # Internal declarations (150 LOC)
│   ├── types.h                   # Structure definitions (200 LOC)
│   ├── constants.h               # Error codes, limits (50 LOC)
│   │
│   ├── memory/
│   │   ├── alloc.c               # cxf_malloc, cxf_calloc, cxf_realloc (100 LOC)
│   │   ├── free.c                # cxf_free, cxf_vector_free (60 LOC)
│   │   ├── state_alloc.c         # cxf_init_solve_state, cxf_alloc_eta (80 LOC)
│   │   └── state_free.c          # cxf_free_solver_state, etc. (120 LOC)
│   │
│   ├── error/
│   │   ├── error.c               # cxf_error, cxf_errorlog (80 LOC)
│   │   ├── logging.c             # cxf_log_printf (50 LOC)
│   │   └── validation.c          # cxf_checkenv, cxf_check_nan, etc. (100 LOC)
│   │
│   ├── matrix/
│   │   ├── linalg.c              # cxf_dot_product, cxf_vector_norm (100 LOC)
│   │   ├── sparse_ops.c          # cxf_matrix_multiply, cxf_sort_indices (120 LOC)
│   │   └── csr_convert.c         # cxf_build_row_major, etc. (100 LOC)
│   │
│   ├── basis/
│   │   ├── ftran.c               # cxf_ftran (100 LOC)
│   │   ├── btran.c               # cxf_btran (100 LOC)
│   │   ├── refactor.c            # cxf_basis_refactor (200 LOC)
│   │   ├── eta.c                 # cxf_pivot_with_eta (80 LOC)
│   │   ├── snapshot.c            # cxf_basis_snapshot, cxf_basis_equal (100 LOC)
│   │   └── warm.c                # cxf_basis_warm, cxf_basis_validate (150 LOC)
│   │
│   ├── pricing/
│   │   ├── init.c                # cxf_pricing_init, cxf_pricing_invalidate (80 LOC)
│   │   ├── candidates.c          # cxf_pricing_candidates (60 LOC)
│   │   ├── steepest.c            # cxf_pricing_steepest (60 LOC)
│   │   └── update.c              # cxf_pricing_update, cxf_pricing_step2 (100 LOC)
│   │
│   ├── ratio/
│   │   ├── ratio_test.c          # cxf_ratio_test (Harris two-pass) (150 LOC)
│   │   ├── pivot_check.c         # cxf_pivot_check, cxf_pivot_bound (100 LOC)
│   │   └── pivot_special.c       # cxf_pivot_special (120 LOC)
│   │
│   ├── pivot/
│   │   ├── primal.c              # cxf_pivot_primal (180 LOC)
│   │   └── fix.c                 # cxf_fix_variable (120 LOC)
│   │
│   ├── simplex/
│   │   ├── init.c                # cxf_simplex_init (120 LOC)
│   │   ├── crash.c               # cxf_simplex_crash (100 LOC)
│   │   ├── setup.c               # cxf_simplex_setup (90 LOC)
│   │   ├── iterate.c             # cxf_simplex_iterate (120 LOC)
│   │   ├── step.c                # cxf_simplex_step (80 LOC)
│   │   ├── step2.c               # cxf_simplex_step2 (100 LOC)
│   │   ├── step3.c               # cxf_simplex_step3 (100 LOC)
│   │   ├── phase.c               # cxf_simplex_phase_end (90 LOC)
│   │   ├── perturb.c             # cxf_simplex_perturbation, unperturb (140 LOC)
│   │   ├── refine.c              # cxf_simplex_refine, cleanup (120 LOC)
│   │   ├── extract.c             # cxf_extract_solution (60 LOC)
│   │   ├── final.c               # cxf_simplex_final (70 LOC)
│   │   └── solve.c               # cxf_solve_lp (180 LOC)
│   │
│   ├── api/
│   │   ├── env.c                 # cxf_emptyenv, cxf_freeenv (80 LOC)
│   │   ├── model.c               # cxf_newmodel, cxf_freemodel, etc. (150 LOC)
│   │   ├── vars.c                # cxf_addvar, cxf_addvars, cxf_delvars (120 LOC)
│   │   ├── constrs.c             # cxf_addconstr, cxf_addconstrs (120 LOC)
│   │   ├── optimize.c            # cxf_optimize, cxf_terminate (100 LOC)
│   │   ├── attrs.c               # cxf_getintattr, cxf_getdblattr (100 LOC)
│   │   ├── params.c              # cxf_setintparam, cxf_getintparam (80 LOC)
│   │   └── io.c                  # cxf_read, cxf_write (200 LOC)
│   │
│   ├── util/
│   │   ├── timing.c              # cxf_get_timestamp, timing functions (80 LOC)
│   │   ├── threading.c           # Lock functions (optional) (100 LOC)
│   │   └── misc.c                # cxf_snprintf_wrapper, etc. (80 LOC)
│   │
│   └── crossover/
│       └── crossover.c           # cxf_crossover, cxf_crossover_bounds (200 LOC)
│
├── tests/
│   ├── unit/                     # Unit tests per module
│   ├── integration/              # Integration tests
│   ├── benchmarks/               # Performance benchmarks
│   └── fixtures/                 # Test data and generators
│
└── benchmarks/
    ├── netlib/                   # Netlib test problems
    └── synthetic/                # Generated problems
```

---

## Phase 1: Foundation Layer (Layers 0-1)

### Phase 1A: Types & Constants [PARALLEL]

| Task | File | LOC | Dependencies |
|------|------|-----|--------------|
| 1A.1 | `include/convexfeld.h` | 100 | None |
| 1A.2 | `src/types.h` | 200 | None |
| 1A.3 | `src/constants.h` | 50 | None |
| 1A.4 | `src/internal.h` | 150 | 1A.2, 1A.3 |

**Tests (write first):**
- `tests/unit/test_types.c` - Structure size/alignment validation (50 LOC)

### Phase 1B: Memory Management [SEQUENTIAL]

| Task | File | LOC | Dependencies | Tests First |
|------|------|-----|--------------|-------------|
| 1B.1 | `tests/unit/test_memory.c` | 150 | 1A.* | - |
| 1B.2 | `src/memory/alloc.c` | 100 | 1A.4 | 1B.1 |
| 1B.3 | `src/memory/free.c` | 60 | 1B.2 | 1B.1 |
| 1B.4 | `src/memory/state_alloc.c` | 80 | 1B.2 | 1B.1 |
| 1B.5 | `src/memory/state_free.c` | 120 | 1B.3 | 1B.1 |

**Test Coverage:**
- `test_malloc_zero_bytes` - Edge case
- `test_malloc_small` - 64 bytes
- `test_malloc_large` - 1GB
- `test_calloc_zeroed` - Verify initialization
- `test_realloc_grow` - Size increase
- `test_realloc_shrink` - Size decrease
- `test_free_null` - NULL safety
- `test_double_free_detection` - Debug builds

### Phase 1C: Error Handling [PARALLEL with 1B]

| Task | File | LOC | Dependencies | Tests First |
|------|------|-----|--------------|-------------|
| 1C.1 | `tests/unit/test_error.c` | 100 | 1A.* | - |
| 1C.2 | `src/error/error.c` | 80 | 1B.2 | 1C.1 |
| 1C.3 | `src/error/logging.c` | 50 | 1C.2 | 1C.1 |
| 1C.4 | `src/error/validation.c` | 100 | 1C.2 | 1C.1 |

**Test Coverage:**
- `test_error_format` - Printf-style formatting
- `test_error_overflow` - Buffer overflow protection
- `test_checkenv_null` - NULL environment
- `test_checkenv_valid` - Valid environment
- `test_check_nan` - IEEE 754 NaN detection
- `test_check_inf` - Infinity detection

---

## Phase 2: Data Layer (Layer 2)

### Phase 2A: Timing [PARALLEL]

| Task | File | LOC | Dependencies | Tests First |
|------|------|-----|--------------|-------------|
| 2A.1 | `tests/unit/test_timing.c` | 60 | 1A.* | - |
| 2A.2 | `src/util/timing.c` | 80 | 1B.2 | 2A.1 |

### Phase 2B: Matrix Operations [PARALLEL]

| Task | File | LOC | Dependencies | Tests First |
|------|------|-----|--------------|-------------|
| 2B.1 | `tests/unit/test_matrix.c` | 200 | 1A.*, 1B.* | - |
| 2B.2 | `src/matrix/linalg.c` | 100 | 1B.2 | 2B.1 |
| 2B.3 | `src/matrix/sparse_ops.c` | 120 | 2B.2 | 2B.1 |
| 2B.4 | `src/matrix/csr_convert.c` | 100 | 2B.3 | 2B.1 |

**Test Coverage:**
- `test_dot_product_dense` - Dense vectors
- `test_dot_product_sparse` - Sparse-dense
- `test_vector_norm_l1` - L1 norm
- `test_vector_norm_l2` - L2 norm (Kahan)
- `test_vector_norm_linf` - L∞ norm
- `test_matrix_multiply_identity` - A=I
- `test_matrix_multiply_sparse` - Sparse A
- `test_sort_indices_sorted` - Already sorted
- `test_sort_indices_reverse` - Reverse order
- `test_csc_to_csr` - Format conversion

---

## Phase 3: Core Operations (Layer 3)

### Phase 3A: Basis Factorization [CRITICAL PATH]

| Task | File | LOC | Dependencies | Tests First |
|------|------|-----|--------------|-------------|
| 3A.1 | `tests/unit/test_basis.c` | 300 | 2B.* | - |
| 3A.2 | `src/basis/refactor.c` | 200 | 2B.* | 3A.1 |
| 3A.3 | `src/basis/ftran.c` | 100 | 3A.2 | 3A.1 |
| 3A.4 | `src/basis/btran.c` | 100 | 3A.2 | 3A.1 |
| 3A.5 | `src/basis/eta.c` | 80 | 3A.3, 3A.4 | 3A.1 |

**Test Coverage:**
- `test_ftran_identity` - B=I, verify x=a
- `test_ftran_single_pivot` - One eta vector
- `test_ftran_many_etas` - 100 eta vectors
- `test_ftran_accuracy` - ||Bx - a|| < tol
- `test_btran_identity` - B=I
- `test_btran_accuracy` - ||B^T y - c|| < tol
- `test_refactor_fresh` - Initial factorization
- `test_refactor_singular` - Singular basis detection
- `test_eta_sparse` - Sparse eta creation
- `test_eta_dense` - Dense eta handling

### Phase 3B: Basis State Management [PARALLEL with 3A.3+]

| Task | File | LOC | Dependencies | Tests First |
|------|------|-----|--------------|-------------|
| 3B.1 | `tests/unit/test_basis_state.c` | 100 | 3A.2 | - |
| 3B.2 | `src/basis/snapshot.c` | 100 | 3A.2 | 3B.1 |
| 3B.3 | `src/basis/warm.c` | 150 | 3B.2 | 3B.1 |

---

## Phase 4: Algorithm Components (Layer 4)

### Phase 4A: Pricing [PARALLEL]

| Task | File | LOC | Dependencies | Tests First |
|------|------|-----|--------------|-------------|
| 4A.1 | `tests/unit/test_pricing.c` | 150 | 3A.* | - |
| 4A.2 | `src/pricing/init.c` | 80 | 3A.* | 4A.1 |
| 4A.3 | `src/pricing/candidates.c` | 60 | 4A.2 | 4A.1 |
| 4A.4 | `src/pricing/steepest.c` | 60 | 4A.2 | 4A.1 |
| 4A.5 | `src/pricing/update.c` | 100 | 4A.3, 4A.4 | 4A.1 |

**Test Coverage:**
- `test_pricing_init_dantzig` - Simple pricing
- `test_pricing_init_steepest` - Steepest edge
- `test_pricing_optimal` - All reduced costs optimal
- `test_pricing_tie_breaking` - Equal costs
- `test_pricing_update_weights` - SE weight update

### Phase 4B: Ratio Test [PARALLEL with 4A]

| Task | File | LOC | Dependencies | Tests First |
|------|------|-----|--------------|-------------|
| 4B.1 | `tests/unit/test_ratio.c` | 150 | 3A.* | - |
| 4B.2 | `src/ratio/ratio_test.c` | 150 | 3A.* | 4B.1 |
| 4B.3 | `src/ratio/pivot_check.c` | 100 | 4B.2 | 4B.1 |
| 4B.4 | `src/ratio/pivot_special.c` | 120 | 4B.3 | 4B.1 |

**Test Coverage:**
- `test_ratio_normal` - Clear winner
- `test_ratio_degenerate` - Ties in ratio
- `test_ratio_unbounded` - All ratios infinite
- `test_harris_two_pass` - Numerical stability
- `test_pivot_tolerance` - Near-zero pivots

### Phase 4C: Pivot Operations [DEPENDS on 4A, 4B]

| Task | File | LOC | Dependencies | Tests First |
|------|------|-----|--------------|-------------|
| 4C.1 | `tests/unit/test_pivot.c` | 100 | 4A.*, 4B.* | - |
| 4C.2 | `src/pivot/primal.c` | 180 | 4A.*, 4B.* | 4C.1 |
| 4C.3 | `src/pivot/fix.c` | 120 | 4C.2 | 4C.1 |

---

## Phase 5: Simplex Core (Layer 5)

### Phase 5A: Simplex Foundation [SEQUENTIAL]

| Task | File | LOC | Dependencies | Tests First |
|------|------|-----|--------------|-------------|
| 5A.1 | `tests/unit/test_simplex.c` | 250 | 4.* | - |
| 5A.2 | `src/simplex/init.c` | 120 | 4.* | 5A.1 |
| 5A.3 | `src/simplex/crash.c` | 100 | 5A.2 | 5A.1 |
| 5A.4 | `src/simplex/setup.c` | 90 | 5A.3 | 5A.1 |
| 5A.5 | `src/simplex/final.c` | 70 | 5A.2 | 5A.1 |

### Phase 5B: Iteration Loop [DEPENDS on 5A]

| Task | File | LOC | Dependencies | Tests First |
|------|------|-----|--------------|-------------|
| 5B.1 | `src/simplex/iterate.c` | 120 | 5A.* | 5A.1 |
| 5B.2 | `src/simplex/step.c` | 80 | 5B.1 | 5A.1 |
| 5B.3 | `src/simplex/extract.c` | 60 | 5B.1 | 5A.1 |

### Phase 5C: Phase Management [PARALLEL with 5B]

| Task | File | LOC | Dependencies | Tests First |
|------|------|-----|--------------|-------------|
| 5C.1 | `src/simplex/phase.c` | 90 | 5A.* | 5A.1 |
| 5C.2 | `src/simplex/perturb.c` | 140 | 5A.* | 5A.1 |
| 5C.3 | `src/simplex/refine.c` | 120 | 5B.* | 5A.1 |

### Phase 5D: Orchestrator [DEPENDS on 5B, 5C]

| Task | File | LOC | Dependencies | Tests First |
|------|------|-----|--------------|-------------|
| 5D.1 | `src/simplex/solve.c` | 180 | 5B.*, 5C.* | 5A.1 |

### Phase 5E: Advanced Steps [OPTIONAL, PARALLEL]

| Task | File | LOC | Dependencies | Tests First |
|------|------|-----|--------------|-------------|
| 5E.1 | `src/simplex/step2.c` | 100 | 5B.2 | 5A.1 |
| 5E.2 | `src/simplex/step3.c` | 100 | 5B.2 | 5A.1 |

**Test Coverage:**
- `test_simplex_trivial` - 1 var, 0 constraints
- `test_simplex_2var` - 2 variables
- `test_simplex_feasible` - Phase I success
- `test_simplex_infeasible` - Phase I detects
- `test_simplex_optimal` - Phase II finds optimum
- `test_simplex_unbounded` - Unboundedness detection
- `test_simplex_degenerate` - Cycling prevention
- `test_simplex_refactorization` - Numerical stability

---

## Phase 6: Public API (Layer 6)

### Phase 6A: Environment & Model [PARALLEL]

| Task | File | LOC | Dependencies | Tests First |
|------|------|-----|--------------|-------------|
| 6A.1 | `tests/unit/test_api.c` | 200 | 5.* | - |
| 6A.2 | `src/api/env.c` | 80 | 5.* | 6A.1 |
| 6A.3 | `src/api/model.c` | 150 | 6A.2 | 6A.1 |

### Phase 6B: Variables & Constraints [PARALLEL]

| Task | File | LOC | Dependencies | Tests First |
|------|------|-----|--------------|-------------|
| 6B.1 | `src/api/vars.c` | 120 | 6A.3 | 6A.1 |
| 6B.2 | `src/api/constrs.c` | 120 | 6A.3 | 6A.1 |

### Phase 6C: Solving & Results [DEPENDS on 6B]

| Task | File | LOC | Dependencies | Tests First |
|------|------|-----|--------------|-------------|
| 6C.1 | `src/api/optimize.c` | 100 | 6B.* | 6A.1 |
| 6C.2 | `src/api/attrs.c` | 100 | 6C.1 | 6A.1 |
| 6C.3 | `src/api/params.c` | 80 | 6A.2 | 6A.1 |

### Phase 6D: File I/O [OPTIONAL]

| Task | File | LOC | Dependencies | Tests First |
|------|------|-----|--------------|-------------|
| 6D.1 | `tests/unit/test_io.c` | 100 | 6C.* | - |
| 6D.2 | `src/api/io.c` | 200 | 6C.* | 6D.1 |

---

## Benchmark Milestones

### Benchmark 0: Tracer Bullet
**Criteria:** Solve 2-variable LP correctly
**Files:** Minimal subset from Phases 1-6
**Expected:** ~2,000 LOC total

### Benchmark 1: Small Problems
**Criteria:** Solve 10-variable LPs in < 1ms
**Test Set:** 10 hand-crafted problems
**Expected:** After Phase 5B complete

### Benchmark 2: Netlib Subset (10 problems)
**Criteria:** Solve afiro, blend, kb2, sc50a, sc50b, sc105, sc205, share1b, share2b, stocfor1
**Expected:** After Phase 5D complete

### Benchmark 3: Full Netlib (95 problems)
**Criteria:** 100% correct solutions, within 10x of commercial time
**Expected:** After Phase 6C complete

### Benchmark 4: Large Scale
**Criteria:** 10,000+ variable problems
**Expected:** After optimization passes

---

## Parallelization Map

```
         Phase 1A (Types)
              │
    ┌─────────┴─────────┐
    │                   │
Phase 1B (Memory)  Phase 1C (Error)
    │                   │
    └─────────┬─────────┘
              │
    ┌─────────┴─────────┐
    │                   │
Phase 2A (Timing)  Phase 2B (Matrix)
    │                   │
    └─────────┬─────────┘
              │
         Phase 3A (Basis) ─────────────┐
              │                        │
    ┌─────────┴─────────┐              │
    │                   │              │
Phase 3B (State)   Phase 4A (Pricing)  │
    │                   │              │
    │              Phase 4B (Ratio) ───┤
    │                   │              │
    └─────────┬─────────┘              │
              │                        │
         Phase 4C (Pivot) ─────────────┘
              │
         Phase 5A (Simplex Foundation)
              │
    ┌─────────┴─────────┐
    │                   │
Phase 5B (Iterate)  Phase 5C (Phase Mgmt)
    │                   │
    └─────────┬─────────┘
              │
         Phase 5D (Orchestrator)
              │
    ┌─────────┴─────────┐
    │                   │
Phase 6A (Env/Model)    │
    │                   │
Phase 6B (Vars/Constrs) │
    │                   │
Phase 6C (Solve/Attrs)  Phase 5E (Advanced)
    │
Phase 6D (File I/O)
```

**Maximum Parallel Workers:** 4 (during Phases 2-4)

---

## Task Summary

| Phase | Tasks | LOC (Impl) | LOC (Tests) | Parallelizable |
|-------|-------|------------|-------------|----------------|
| 1A | 4 | 500 | 50 | Yes (4) |
| 1B | 5 | 360 | 150 | No |
| 1C | 4 | 230 | 100 | Yes with 1B |
| 2A | 2 | 80 | 60 | Yes (2) |
| 2B | 4 | 320 | 200 | Yes with 2A |
| 3A | 5 | 480 | 300 | No |
| 3B | 3 | 250 | 100 | Yes with 3A.3+ |
| 4A | 5 | 300 | 150 | Yes (2) |
| 4B | 4 | 370 | 150 | Yes with 4A |
| 4C | 3 | 300 | 100 | No |
| 5A | 5 | 380 | 250 | No |
| 5B | 3 | 260 | - | No |
| 5C | 3 | 350 | - | Yes with 5B |
| 5D | 1 | 180 | - | No |
| 5E | 2 | 200 | - | Yes |
| 6A | 3 | 230 | 200 | Yes |
| 6B | 2 | 240 | - | Yes |
| 6C | 3 | 280 | - | No |
| 6D | 2 | 200 | 100 | Optional |
| **Total** | **63** | **~5,500** | **~1,900** | |

**Total Implementation:** ~7,400 LOC (1:3.9 test:code ratio)

---

## Tracer Bullet Tasks (Ordered)

1. **TB.1** `src/types.h` - Core structures (200 LOC)
2. **TB.2** `src/constants.h` - Error codes (50 LOC)
3. **TB.3** `tests/unit/test_tracer.c` - E2E test (100 LOC)
4. **TB.4** `src/memory/alloc.c` - malloc/calloc/free (100 LOC)
5. **TB.5** `src/error/error.c` - cxf_error (80 LOC)
6. **TB.6** `src/error/validation.c` - cxf_checkenv (50 LOC)
7. **TB.7** `src/util/timing.c` - cxf_get_timestamp (40 LOC)
8. **TB.8** `src/matrix/linalg.c` - dot_product, norm (80 LOC)
9. **TB.9** `src/basis/ftran_simple.c` - Simplified FTRAN (60 LOC)
10. **TB.10** `src/pricing/simple.c` - Simple pricing (40 LOC)
11. **TB.11** `src/ratio/simple.c` - Simple ratio test (60 LOC)
12. **TB.12** `src/simplex/init.c` - cxf_simplex_init (80 LOC)
13. **TB.13** `src/simplex/crash.c` - cxf_simplex_crash (60 LOC)
14. **TB.14** `src/simplex/iterate_simple.c` - Simple iteration (100 LOC)
15. **TB.15** `src/simplex/extract.c` - cxf_extract_solution (60 LOC)
16. **TB.16** `src/api/env.c` - cxf_emptyenv, cxf_freeenv (60 LOC)
17. **TB.17** `src/api/model.c` - cxf_newmodel, cxf_freemodel (80 LOC)
18. **TB.18** `src/api/vars.c` - cxf_addvar (60 LOC)
19. **TB.19** `src/api/constrs.c` - cxf_addconstr (60 LOC)
20. **TB.20** `src/api/optimize.c` - cxf_optimize (60 LOC)
21. **TB.21** `include/convexfeld.h` - Public header (80 LOC)

**Tracer Bullet Total:** ~1,560 LOC implementation + 100 LOC tests = ~1,660 LOC

---

## CI/CD Gates

| Gate | Requirement | Enforcement |
|------|-------------|-------------|
| Unit Tests | 100% pass | Block merge |
| Line Coverage | ≥ 90% | Block merge |
| Branch Coverage | ≥ 85% | Block merge |
| Function Coverage | 100% | Block merge |
| Test:Code Ratio | ≥ 1:4 | Warning |
| Static Analysis | Zero critical | Block merge |
| Benchmark Regression | ≤ 10% slowdown | Warning |

---

## File Naming Conventions

- Implementation: `src/{module}/{function_group}.c`
- Tests: `tests/unit/test_{module}.c`
- Headers: `src/{name}.h` (internal), `include/convexfeld.h` (public)
- Benchmarks: `benchmarks/{suite}/{problem}.lp`

---

## Notes

1. **Tests First:** Every task with a "Tests First" column requires writing the test file before the implementation file.

2. **100 LOC Granularity:** Each implementation file targets 60-120 LOC. Files exceeding 200 LOC should be split.

3. **Tracer Bullet First:** Complete TB.1-TB.21 before expanding to full implementations.

4. **Stub Strategy:** The tracer bullet uses simplified versions (`ftran_simple`, `pricing_simple`) that are replaced by full implementations in later phases.

5. **Benchmark-Driven:** Each benchmark milestone defines acceptance criteria before moving to the next phase.
