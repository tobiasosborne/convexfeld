# Architecture Review: Data Structures

**Reviewer:** Claude Opus 4.5 (Architecture Review)
**Date:** 2026-01-27
**Project:** ConvexFeld LP Solver
**Scope:** Data structure completeness, correctness, and scalability

---

## Executive Summary

**Will these data structures work? YES, with caveats.**

The core data structures are well-designed and follow established patterns from production LP solvers. The implementation closely adheres to the detailed specifications. However, there are several issues that will cause problems at scale or in edge cases:

1. **Critical:** Index types (int vs int64_t) are inconsistent, limiting scalability
2. **Serious:** Missing fields in several structures compared to spec
3. **Moderate:** Memory layout not optimized for cache efficiency
4. **Minor:** Some defensive validation missing

For problems up to ~100K variables/constraints, these structures will work. For million-scale problems, significant refactoring of index types is required.

**Confidence Level:** 75% - The design is sound, but implementation gaps exist.

---

## Structure-by-Structure Analysis

### CxfEnv

**Header:** `/home/tobias/Projects/convexfeld/include/convexfeld/cxf_env.h`
**Spec:** `/home/tobias/Projects/convexfeld/docs/specs/structures/cxf_env.md`

#### Completeness: GOOD

The implementation includes all critical fields:
- Magic number validation (0xC0FEFE1D)
- Active flag for lifecycle management
- Tolerances (feasibility, optimality, infinity)
- Termination flags (both internal and external pointer)
- Reference counting
- Logging infrastructure

**Missing from spec:**
- `paramTablePtr` - Parameter name-to-value lookup (spec mentions ~200+ parameters)
- `criticalSection` - Thread synchronization lock
- `asyncState` - Async optimization state
- `modelManager` - Model tracking

These are labeled as "internal/bookkeeping" in the spec, so their absence is acceptable for MVP.

#### Type Correctness: GOOD

```c
uint32_t magic;           // Correct - 32-bit validation marker
int active;               // Acceptable - boolean flag
char error_buffer[512];   // Matches spec (256 char minimum)
double feasibility_tol;   // Correct for IEEE 754
```

#### Issues:
1. **error_buffer size:** Implementation uses 512 bytes, spec says "Max 256 chars" - this is fine (conservative)
2. **terminate_flag_ptr:** Correct use of `volatile int*` for async termination
3. **session_id:** Uses uint64_t - good for uniqueness

#### Verdict: PASS

---

### CxfModel

**Header:** `/home/tobias/Projects/convexfeld/include/convexfeld/cxf_model.h`
**Spec:** `/home/tobias/Projects/convexfeld/docs/specs/structures/cxf_model.md`

#### Completeness: ADEQUATE

Core fields present:
- Magic number, env reference, name
- Dimensions (num_vars, num_constrs)
- Variable data arrays (obj_coeffs, lb, ub, vtype)
- Solution data (solution, pi, status, obj_val)
- Extended fields (fingerprint, update_time, pending_buffer, etc.)

**Missing from spec:**
- `primaryMatrix` / `workMatrix` - Backup and working copies for presolve
- `attrTablePtr` - Attribute metadata table
- `concurrentEnvs` / `concurrentEnvCount` - Concurrent optimization support
- `vectors[7]` - Internal vector containers

**Critical missing field:** The `matrix` field is declared as `SparseMatrix *matrix` but the model implementation in `src/api/model.c` does NOT allocate or initialize this field. This will cause NULL pointer dereference when trying to access the constraint matrix.

#### Type Issues:

```c
int num_vars;             // PROBLEM: Should be int32_t or size_t for clarity
int num_constrs;          // PROBLEM: Same issue
int var_capacity;         // OK for internal use
```

The use of bare `int` is concerning because:
- On LP64 systems (Linux/macOS 64-bit), `int` is 32-bit
- This limits problem size to ~2.1B variables
- But the spec mentions problems up to 1M variables, so this is acceptable

#### Memory Layout:

```c
struct CxfModel {
    uint32_t magic;           // 4 bytes, offset 0
    CxfEnv *env;              // 8 bytes, offset 8 (padding inserted)
    char name[256];           // 256 bytes, offset 16
    int num_vars;             // 4 bytes, offset 272
    int num_constrs;          // 4 bytes, offset 276
    int var_capacity;         // 4 bytes, offset 280
    // ... more fields
};
```

**Issue:** The name array (256 bytes) sits between frequently-accessed fields (magic, env) and dimension fields. This causes cache misses when iterating through many models.

**Recommendation:** Move `name` to the end of the structure.

#### Critical Bug Found:

In `src/api/model.c`, `cxf_newmodel()` allocates variable arrays but never allocates `model->matrix`:

```c
/* Allocate initial variable arrays */
model->obj_coeffs = (double *)cxf_malloc(...);
model->lb = (double *)cxf_malloc(...);
// ... but no: model->matrix = cxf_sparse_create(...);
```

When `cxf_optimize()` tries to access `model->matrix`, it will crash.

#### Verdict: PARTIAL PASS - Critical matrix allocation bug

---

### SparseMatrix

**Header:** `/home/tobias/Projects/convexfeld/include/convexfeld/cxf_matrix.h`
**Spec:** `/home/tobias/Projects/convexfeld/docs/specs/structures/sparse_matrix.md`

#### Completeness: GOOD

All critical CSC fields present:
- Dimensions (num_rows, num_cols, nnz)
- CSC arrays (col_ptr, row_idx, values)
- CSR arrays (row_ptr, col_idx, row_values) - correctly optional
- Constraint data (rhs, sense)

**Missing from spec:**
- `objCoeffs` - Objective coefficients (stored in CxfModel instead)
- `lb`, `ub` - Variable bounds (stored in CxfModel instead)
- `vtype` - Variable types (stored in CxfModel instead)
- `colLen` - Column lengths cache
- `varnames`, `constrnames` - Name arrays

The decision to keep bounds/objective in CxfModel rather than SparseMatrix is reasonable for separation of concerns, but differs from the spec.

#### Type Analysis - CRITICAL ISSUE:

```c
int num_rows;             // 32-bit signed
int num_cols;             // 32-bit signed
int64_t nnz;              // 64-bit - GOOD for large problems
int64_t *col_ptr;         // 64-bit pointers
int *row_idx;             // 32-bit - INCONSISTENT!
```

**The row_idx and col_idx arrays use `int` (32-bit) but col_ptr uses `int64_t`.**

This is a critical scalability bug:
- If `num_rows > 2^31-1`, row indices overflow
- If `num_cols > 2^31-1`, column indices overflow
- But nnz can be 64-bit, so the pointers work for large problems

For a problem with 1M constraints and 10M non-zeros, `int` is sufficient. But this limits future scalability.

**Recommendation:** Either:
1. Make row_idx/col_idx `int64_t` for consistency
2. Or document the 2B constraint limit

#### Memory Efficiency:

For typical LP problems:
- CSC storage: 8*(n+1) + 4*nnz + 8*nnz = 8n + 12*nnz bytes
- With CSR: + 8*(m+1) + 4*nnz + 8*nnz = additional 8m + 12*nnz bytes

Example: 1M vars, 500K constraints, 10M non-zeros
- CSC: 8MB + 120MB = 128MB
- CSR (if built): + 4MB + 120MB = 124MB additional
- Total with both: 252MB

This is reasonable for modern systems.

#### Verdict: PASS with scalability concerns

---

### BasisState

**Header:** `/home/tobias/Projects/convexfeld/include/convexfeld/cxf_basis.h`
**Spec:** `/home/tobias/Projects/convexfeld/docs/specs/structures/basis_state.md`

#### Completeness: ADEQUATE

Present:
- Dimensions (m, n)
- Basic variable tracking (basic_vars, var_status)
- Eta factorization (eta_count, eta_capacity, eta_head)
- Working storage (work)
- Refactorization control (refactor_freq, pivots_since_refactor, iteration)

**Missing from spec:**
- `workCounter` - Cumulative computational work tracking
- `etaAllocator` - Memory pool for efficient eta allocation
- `tempIndices` - Temporary index arrays for sparse operations

The missing memory pool (`etaAllocator`) is significant. Without it, each eta allocation goes through malloc/free, which is slow during rapid iteration.

#### Implementation Divergence:

The spec describes eta vectors stored in a singly-linked list (`eta_head -> eta1 -> eta2 -> ...`).

The plan document shows:
```c
EtaFactors **eta_list;    // Array of eta factor pointers
```

But the actual header uses:
```c
EtaFactors *eta_head;     // Head of eta linked list
```

The linked list implementation is correct per spec, but differs from the plan's array approach. The linked list is more memory-efficient but has worse cache locality.

#### Type Analysis:

```c
int m;                    // 32-bit - limits to 2B constraints
int n;                    // 32-bit - limits to 2B variables
int *basic_vars;          // 32-bit indices
int *var_status;          // 32-bit status codes
```

All 32-bit, which is consistent but limits scalability.

#### Critical Observation:

The BasisSnapshot structure is well-designed:
```c
typedef struct BasisSnapshot {
    int numVars;
    int numConstrs;
    int *basisHeader;
    int *varStatus;
    int valid;
    int iteration;
    void *L;              // L factor copy - for LU factorization
    void *U;              // U factor copy
    int *pivotPerm;
} BasisSnapshot;
```

The L/U factor fields suggest the design anticipates moving from PFI to LU factorization in the future. This is good forward planning.

#### Verdict: PASS

---

### EtaFactors

**Header:** `/home/tobias/Projects/convexfeld/include/convexfeld/cxf_basis.h`
**Spec:** `/home/tobias/Projects/convexfeld/docs/specs/structures/eta_factors.md`

#### Completeness: MINIMAL

Present:
- type, pivot_row, nnz
- indices, values (sparse representation)
- pivot_elem
- next (linked list)

**Missing from spec:**
- `pivotVar` - Variable index involved in transformation
- `pivotValue` - Value at which variable was fixed/pivoted
- `objCoeff` - Objective coefficient (for objective updates)
- `status` - New status of pivot variable
- `leavingRow` - For Type 2 etas
- `colIndices`, `colValues`, `colCount` - Column data for Type 2 etas
- `allocSize` - For pool management

The missing fields are significant for a full simplex implementation. Without `objCoeff`, objective updates during pivots will be incorrect.

#### Implementation Quality:

The `src/basis/eta_factors.c` implementation is clean:
```c
EtaFactors *cxf_eta_create(int type, int pivot_row, int nnz);
void cxf_eta_free(EtaFactors *eta);
int cxf_eta_init(EtaFactors *eta, int type, int pivot_row, int nnz);
int cxf_eta_validate(const EtaFactors *eta, int max_rows);
int cxf_eta_set(EtaFactors *eta, const int *indices, const double *values);
```

The validation function is well-implemented with NaN/Inf checks.

#### Verdict: PARTIAL PASS - Missing fields for full simplex

---

### SolverContext

**Header:** `/home/tobias/Projects/convexfeld/include/convexfeld/cxf_solver.h`
**Spec:** `/home/tobias/Projects/convexfeld/docs/specs/structures/solver_context.md`

#### Completeness: GOOD

Present:
- Model reference and dimensions
- Solver state (phase, solve_mode, max_iterations, tolerance, obj_value)
- All working arrays (work_lb, work_ub, work_obj, work_x, work_pi, work_dj)
- Subcomponents (basis, pricing)
- Timing state
- Refactorization tracking (eta_count, eta_memory, etc.)

**Missing from spec:**
- `hasQuadratic` - QP support flag
- `removedRows`, `removedCols` - Presolve tracking
- `presolveTime` - Timing for presolve
- `qpData` - Quadratic programming data
- `startBasis` - Warm start basis data
- Various internal flags (iterationMode, specialFlag1, etc.)

The missing QP fields are acceptable since this is an LP solver.

#### Critical Issue - No Creation Function:

The header declares `struct SolverContext` but there is no `cxf_solver_create()` function visible in the codebase. The cleanup function exists in `state_cleanup.c`:
```c
void cxf_free_solver_state(SolverContext *ctx);
```

But no corresponding creation function. This suggests the simplex initialization (`cxf_simplex_init`) is supposed to create and return a SolverContext, but that function is not yet implemented.

#### Verdict: PASS with implementation gap

---

### PricingContext

**Header:** `/home/tobias/Projects/convexfeld/include/convexfeld/cxf_pricing.h`
**Spec:** `/home/tobias/Projects/convexfeld/docs/specs/structures/pricing_context.md`

#### Completeness: GOOD

Present:
- Level management (current_level, max_levels)
- Problem dimensions (num_vars, strategy)
- Candidate arrays per level
- Steepest edge weights
- Cache and statistics

**Divergence from spec:**

Spec shows:
```c
int64_t *candidateStarts;     // 64-bit
int64_t **candidateArrays;    // 64-bit pointers
```

Implementation uses:
```c
int *candidate_counts;        // 32-bit
int **candidate_arrays;       // int pointers (32-bit indices)
```

The 32-bit candidate indices are fine since they index into the variable array, which is also 32-bit.

**Missing:**
- `outputBuffers` - Per-level output buffers
- `selectionFlags` - Boolean array for partial pricing
- `neighborLists` - Structure-aware pricing

#### Implementation Quality:

The `src/pricing/context.c` implementation is clean with proper error handling:
```c
PricingContext *cxf_pricing_create(int num_vars, int max_levels);
void cxf_pricing_free(PricingContext *ctx);
```

#### Verdict: PASS

---

### CallbackContext

**Header:** `/home/tobias/Projects/convexfeld/include/convexfeld/cxf_callback.h`
**Spec:** `/home/tobias/Projects/convexfeld/docs/specs/structures/callback_context.md`

#### Completeness: MINIMAL

Present:
- Dual magic numbers (32-bit and 64-bit)
- Callback function pointer and user data
- State flags (terminate_requested, enabled)
- Basic timing and statistics

**Missing from spec:**
- `env` - Parent environment reference
- `primaryModel` - Primary model for callback session
- `suppressCallbackLog` - Log suppression flag
- `timestamp1`, `timestamp2` - Session timestamps
- `callbackSubStruct` (48 bytes) - Embedded metadata
- Various sentinel fields

The spec describes a 848-byte structure with extensive internal fields. The implementation is much simpler (~80 bytes).

#### Implementation Quality:

The implementation provides:
```c
CallbackContext *cxf_callback_create(void);
void cxf_callback_free(CallbackContext *ctx);
int cxf_callback_validate(const CallbackContext *ctx);
int cxf_callback_reset_stats(CallbackContext *ctx);
```

This is sufficient for basic callback functionality.

#### Verdict: MINIMAL PASS - Simplified from spec

---

## Critical Issues

### 1. Matrix Not Allocated in CxfModel

**Severity:** CRITICAL
**Location:** `src/api/model.c:cxf_newmodel()`

The model creation function allocates variable arrays but never creates the SparseMatrix. Any attempt to access `model->matrix` will crash.

**Fix Required:** Add matrix allocation in `cxf_newmodel()`:
```c
model->matrix = cxf_sparse_create(0, 0);  // Empty initial matrix
```

### 2. Index Type Inconsistency (int vs int64_t)

**Severity:** HIGH (scalability)
**Location:** Multiple structures

The SparseMatrix uses `int64_t` for `nnz` and `col_ptr` but `int` for `row_idx`. This creates a hard limit of ~2B rows/columns even though the non-zero count can be larger.

**Recommendation:** Document the limit or make all indices 64-bit.

### 3. Missing EtaFactors Fields

**Severity:** MEDIUM
**Location:** `include/convexfeld/cxf_basis.h`

The EtaFactors structure is missing several fields needed for correct simplex pivoting:
- `objCoeff` - Required for objective value updates
- `pivotVar` - Required to track which variable changed

### 4. No SolverContext Creation Function

**Severity:** MEDIUM (implementation gap)
**Location:** Missing from codebase

The SolverContext structure exists but no creation function is implemented. This blocks simplex initialization.

---

## Scalability Assessment

### Can this handle production workloads?

| Workload | Support | Notes |
|----------|---------|-------|
| 10K vars, 5K constraints | YES | Well within limits |
| 100K vars, 50K constraints | YES | Comfortable |
| 1M vars, 500K constraints | MAYBE | Memory pressure on eta storage |
| 1M constraints | NO | int row indices will work, but eta list grows huge |
| 10M non-zeros | YES | int64_t nnz handles this |

### Memory Estimates for Large Problems

For 1M variables, 500K constraints, 5M non-zeros:

| Component | Size |
|-----------|------|
| SparseMatrix (CSC) | ~68 MB |
| SparseMatrix (CSR) | ~64 MB |
| CxfModel arrays | ~48 MB |
| BasisState | ~6 MB |
| SolverContext arrays | ~48 MB |
| Eta storage (1000 etas, avg 100 nnz) | ~2.4 MB |
| **Total** | ~237 MB |

This is reasonable for modern servers.

### Performance Concerns

1. **Eta linked list:** Poor cache locality during FTRAN/BTRAN. Consider switching to array-based storage.

2. **CSR lazy build:** Building CSR from CSC is O(nnz) and allocates memory. Consider pre-building for frequently-accessed rows.

3. **No SIMD alignment:** Coefficient arrays are not guaranteed 16-byte aligned for vectorization.

---

## Comparison to Spec

| Structure | Spec Fields | Impl Fields | Match % |
|-----------|-------------|-------------|---------|
| CxfEnv | ~25 | ~20 | 80% |
| CxfModel | ~30 | ~22 | 73% |
| SparseMatrix | ~20 | ~12 | 60% |
| BasisState | ~15 | ~12 | 80% |
| EtaFactors | ~15 | ~7 | 47% |
| SolverContext | ~25 | ~20 | 80% |
| PricingContext | ~15 | ~12 | 80% |
| CallbackContext | ~20 | ~10 | 50% |

**Overall Spec Compliance:** ~70%

The missing fields are mostly:
- Performance tracking (counters, timers)
- Thread safety (locks)
- Advanced features (QP, concurrent solve)

Core functionality fields are present.

---

## Verdict

### Summary Table

| Structure | Completeness | Correctness | Scalability | Verdict |
|-----------|--------------|-------------|-------------|---------|
| CxfEnv | Good | Good | Good | PASS |
| CxfModel | Adequate | BUG | Good | NEEDS FIX |
| SparseMatrix | Good | Mixed | Limited | PASS* |
| BasisState | Adequate | Good | Good | PASS |
| EtaFactors | Minimal | Good | Good | PARTIAL |
| SolverContext | Good | Good | Good | PASS |
| PricingContext | Good | Good | Good | PASS |
| CallbackContext | Minimal | Good | Good | PASS |

### Final Assessment

**The data structures are fundamentally sound for implementing a revised simplex LP solver.**

The design follows industry patterns and the spec is well-researched (citing Dantzig, Forrest, Maros, etc.).

**Immediate blockers:**
1. Fix matrix allocation in CxfModel (CRITICAL)
2. Add missing EtaFactors fields (before simplex implementation)

**For production readiness:**
1. Resolve int/int64_t inconsistency
2. Implement memory pool for eta allocation
3. Add SIMD-aligned allocations for coefficient arrays
4. Consider array-based eta storage for cache efficiency

**Recommendation:** Proceed with implementation after fixing the critical matrix allocation bug. The other issues can be addressed incrementally.

---

*Review completed: 2026-01-27*
*Reviewer: Claude Opus 4.5*
