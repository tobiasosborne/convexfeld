# Audit Report: Basis & Matrix Modules

**Auditor:** Agent C3
**Date:** 2026-02-16
**Scope:** Basis Operations and Matrix Core/Finalization modules
**Implementation Files:** 10 files (basis: 4, matrix: 6)
**Specification Files:** 3 v2 module specs

---

## Executive Summary

**CRITICAL VIOLATIONS FOUND: Multiple spec mismatches detected**

The basis and matrix implementations show significant deviations from the v2 specifications. The most serious issues:

1. **COMPLETE MODULE MISMATCH**: Spec defines 5 functions for Basis Operations, implementation provides none of them
2. **COMPLETE MODULE MISMATCH**: Spec defines 4 functions for Matrix Core, implementation provides none of them
3. **CSC/CSR format appears correct** - implementation uses proper CSC (CSC as primary) and CSR (lazy conversion)
4. **Wrong function scope**: Implementation provides lifecycle/snapshot/validation functions that belong to different modules than specified

**Verdict:** Major redesign needed. Implementation is for infrastructure (lifecycle, storage), spec is for algorithms (PFI operations, scaling, partitioning).

---

## Module 1: Basis Operations

### Specification Requirements (basis_operations.md)

The spec defines **5 functions** for managing the Product Form of Inverse during simplex:

1. **cxf_fix_variables_at_bounds** - Identify and fix variables at bounds, creating eta vectors
2. **cxf_progress_snapshot** - Capture solver progress counters for cycling detection
3. **cxf_basis_diff** - Compute progress score from snapshot comparison
4. **cxf_basis_warm** - Create quadratic warm-start eta vector for Q-matrix contributions
5. **cxf_pivot_with_eta** - Record simplex pivot as eta vector (core PFI update)

**Key spec characteristics:**
- All functions work with EtaVector structures (Variants 1, 2, 3 per P1.08)
- Focus on PFI algorithm (Product Form of Inverse) operations
- Interface with SolverState, not standalone BasisState
- Memory allocation from arena (bump allocator)
- Eta vectors prepended to chain, never individually freed

### Implementation Analysis

**Files examined:**
- `/home/tobiasosborne/Projects/convexfeld/src/basis/basis_state.c` (160 lines)
- `/home/tobiasosborne/Projects/convexfeld/src/basis/warm.c` (258 lines)
- `/home/tobiasosborne/Projects/convexfeld/src/basis/snapshot.c` (159 lines)
- `/home/tobiasosborne/Projects/convexfeld/src/basis/basis_stub.c` (101 lines)

**What implementation provides:**

#### basis_state.c
- `cxf_basis_create()` - Allocate BasisState structure
- `cxf_basis_free()` - Free BasisState and eta list
- `cxf_basis_init()` - Initialize/reinitialize BasisState

#### warm.c
- `cxf_basis_validate()` - Simple validation (bounds, duplicates)
- `cxf_basis_validate_ex()` - Extended validation with flags
- `cxf_basis_warm()` - Copy basic variables from array, clear eta list
- `cxf_basis_warm_snapshot()` - Copy from BasisSnapshot, clear eta list

#### snapshot.c
- `cxf_basis_snapshot_create()` - Create BasisSnapshot (copy arrays)
- `cxf_basis_snapshot_diff()` - Count differences between snapshots
- `cxf_basis_snapshot_equal()` - Check equality
- `cxf_basis_snapshot_free()` - Free snapshot arrays

#### basis_stub.c
- `cxf_basis_snapshot()` - Simple array copy stub
- `cxf_basis_diff()` - Simple diff stub
- `cxf_basis_equal()` - Simple equality stub

### Violations Found

#### V1: COMPLETE FUNCTION SET MISMATCH

**Severity:** CRITICAL

**Expected (from spec):**
```
cxf_fix_variables_at_bounds()    - Variable fixing with eta creation
cxf_progress_snapshot()          - Scalar counter snapshot
cxf_basis_diff()                 - Weighted progress score
cxf_basis_warm()                 - Quadratic eta creation
cxf_pivot_with_eta()             - Pivot eta creation
```

**Actual (from implementation):**
```
cxf_basis_create()               - BasisState lifecycle
cxf_basis_free()                 - BasisState lifecycle
cxf_basis_init()                 - BasisState lifecycle
cxf_basis_validate()             - Validation
cxf_basis_validate_ex()          - Validation
cxf_basis_warm()                 - DIFFERENT SIGNATURE/PURPOSE
cxf_basis_warm_snapshot()        - Not in spec
cxf_basis_snapshot_create()      - DIFFERENT from spec's snapshot
cxf_basis_snapshot_diff()        - DIFFERENT from spec's diff
```

**Analysis:**
- **0 out of 5** spec functions are present
- Implementation `cxf_basis_warm()` exists but has completely different signature and purpose:
  - **Spec signature:** `(Environment*, SolverState*, varIndex: int, varValue: double) -> int`
  - **Spec purpose:** Create quadratic warm-start eta vector for Q-matrix
  - **Impl signature:** `(BasisState*, basic_vars: const int*, m: int) -> int`
  - **Impl purpose:** Copy basic variables array, clear eta list
- Implementation `cxf_basis_snapshot_*` functions work with BasisSnapshot struct (array copies)
- Spec `cxf_progress_snapshot()` works with scalar counters (SNAPSHOT_SIZE integers)
- Spec `cxf_basis_diff()` computes weighted progress score (double), impl computes element count (int)

#### V2: MISSING ETA VECTOR CREATION LOGIC

**Severity:** CRITICAL

**Spec requirement:**
All 3 core functions create eta vectors:
- `cxf_fix_variables_at_bounds` creates VARIABLE_FIX eta (Variant 2)
- `cxf_basis_warm` creates WARM_START eta (Variant 3)
- `cxf_pivot_with_eta` creates PIVOT eta (Variant 1)

**Implementation:**
- No eta creation logic found in any basis file
- `warm.c` and `snapshot.c` only manipulate existing eta chains (clear via `clear_eta_list()`)
- No arena allocation from memory pool
- No eta prepending to chain

**Evidence:**
```c
// warm.c:43-55 - Only clears eta list, doesn't create
static void clear_eta_list(BasisState *basis) {
    EtaFactors *eta = basis->eta_head;
    while (eta != NULL) {
        EtaFactors *next = eta->next;
        free(eta->indices);
        free(eta->values);
        free(eta);
        eta = next;
    }
    basis->eta_head = NULL;
    basis->eta_count = 0;
}
```

#### V3: WRONG STRUCTURE USAGE

**Severity:** HIGH

**Spec:** Functions operate on `SolverState` (contains basis + matrix + pricing state)

**Implementation:** Functions operate on standalone `BasisState`

**Impact:** Cannot access constraint matrix, Q-matrix, pricing subsystem, environment parameters

**Example:**
- Spec `cxf_fix_variables_at_bounds(state: SolverState*, env: Environment*)` needs matrix data to scan constraint rows
- Impl `cxf_basis_validate(basis: BasisState*)` only has basis arrays

#### V4: SNAPSHOT SEMANTICS MISMATCH

**Severity:** MEDIUM

**Spec `cxf_progress_snapshot()`:**
- Copies SNAPSHOT_SIZE scalar integer counters (iteration counts, work metrics)
- O(1) lightweight operation, no allocation
- Purpose: cycling detection via counter deltas
- Output: int array buffer (pre-allocated by caller)

**Impl `cxf_basis_snapshot_create()`:**
- Copies variable status arrays and basis header (O(n+m) data)
- Allocates memory for arrays
- Purpose: full state capture for warm-start
- Output: BasisSnapshot struct with allocated arrays

**These are completely different operations.**

#### V5: DIFF COMPUTATION MISMATCH

**Severity:** MEDIUM

**Spec `cxf_basis_diff()`:**
- Returns `double` (weighted, normalized progress score)
- Combines 6 weighted terms (structural changes, column reduction, iterations, row stats, conversions, work)
- Normalizes by problem size (column/row denominators, nnz)
- Purpose: anti-cycling trigger (low score = potential cycling)

**Impl `cxf_basis_snapshot_diff()`:**
- Returns `int` (count of differing array elements)
- Simple element-by-element comparison
- No weighting, no normalization
- Purpose: measure similarity of two basis snapshots

**These compute fundamentally different metrics.**

---

## Module 2: Matrix Core

### Specification Requirements (matrix_core.md)

The spec defines **4 functions** for CSC-to-CSR lazy conversion pipeline:

1. **cxf_prepare_row_data** - Reverse scaling, prepare for row-major construction
2. **cxf_matrix_setup** - Partition active/removed constraints using swap data
3. **cxf_build_row_major** - Two-pass CSC-to-CSR conversion
4. **cxf_sort_by_values** - Hybrid sort (quicksort + shellsort) for value,index pairs

**Key spec characteristics:**
- Works with `Model` structure (has MatrixData, Environment)
- Handles swap data for active constraint partitioning (Hoare partition)
- Unscaling/rescaling around CSR construction
- Range constraint negation reversal
- Multi-constraint type CSR (linear, quadratic, SOS, general)

### Implementation Analysis

**Files examined:**
- `/home/tobiasosborne/Projects/convexfeld/src/matrix/sparse_matrix.c` (192 lines)
- `/home/tobiasosborne/Projects/convexfeld/src/matrix/row_major.c` (167 lines)
- `/home/tobiasosborne/Projects/convexfeld/src/matrix/multiply.c` (110 lines)
- `/home/tobiasosborne/Projects/convexfeld/src/matrix/vectors.c` (109 lines)
- `/home/tobiasosborne/Projects/convexfeld/src/matrix/sort.c` (83 lines)
- `/home/tobiasosborne/Projects/convexfeld/src/matrix/sparse_stub.c` (114 lines)

**What implementation provides:**

#### sparse_matrix.c
- `cxf_sparse_validate()` - Validate CSC invariants
- `cxf_sparse_build_csr()` - Build CSR from CSC (two-pass)
- `cxf_sparse_free_csr()` - Free CSR arrays

#### row_major.c
- `cxf_prepare_row_data()` - Allocate CSR arrays (validation + allocation)
- `cxf_build_row_major()` - Fill CSR arrays (two-pass transpose)
- `cxf_finalize_row_data()` - Finalize (no-op validation)

#### multiply.c
- `cxf_matrix_multiply()` - SpMV: y = Ax (CSC format)
- `cxf_matrix_transpose_multiply()` - SpMV: y = A^T x

#### vectors.c
- `cxf_dot_product()` - Dense dot product
- `cxf_dot_product_sparse()` - Sparse-dense dot product
- `cxf_vector_norm()` - L_inf, L1, L2 norms

#### sort.c
- `cxf_sort_indices()` - Sort indices only (insertion sort)
- `cxf_sort_indices_values()` - Sort indices with synchronized values

#### sparse_stub.c
- `cxf_sparse_create()` - Allocate SparseMatrix
- `cxf_sparse_free()` - Free SparseMatrix
- `cxf_sparse_init_csc()` - Initialize CSC arrays

### Violations Found

#### V6: FUNCTION SIGNATURE MISMATCH (cxf_prepare_row_data)

**Severity:** CRITICAL

**Spec signature:**
```c
void cxf_prepare_row_data(Model *model, int mode)
```
- Takes Model (has MatrixData, Environment)
- Mode controls unscaling behavior (0=full, 1=partial, 2+=finalize only)
- Reverses scaling: coefficients, bounds, Q-matrix, PWL data
- Handles swap data column length restoration
- Returns void (no error handling)

**Impl signature:**
```c
int cxf_prepare_row_data(SparseMatrix *mat)
```
- Takes SparseMatrix (standalone structure, no Model/Environment context)
- No mode parameter
- Only allocates CSR arrays (row_ptr, col_idx, row_values)
- Returns int error code
- No scaling operations, no swap data

**Analysis:**
This is a **NAME COLLISION** - same function name, completely different purposes:
- Spec: Multi-phase unscaling and state preparation (part of lazy CSR pipeline)
- Impl: Simple CSR array allocation (helper for CSR construction)

#### V7: MISSING cxf_matrix_setup

**Severity:** CRITICAL

**Spec requirement:**
```c
void cxf_matrix_setup(MatrixData *matrix)
```
- Partition CSC columns using Hoare two-pointer scheme
- Separate active vs removed constraints (based on constraint status)
- Swap column length array pointer with active count array
- Copy and prepare column bounds (zero active bounds)
- Increment swap data reference count

**Implementation:** NOT FOUND

**Impact:** Cannot handle presolved models with removed constraints. Spec's partitioning optimization is absent.

#### V8: FUNCTION SIGNATURE MISMATCH (cxf_build_row_major)

**Severity:** HIGH

**Spec signature:**
```c
int cxf_build_row_major(Model *model)
```
- Takes Model (has primary MatrixData, Environment)
- Builds CSR for ALL constraint types (linear, quadratic, SOS, general)
- Creates variable-to-constraint reverse mapping
- Builds per-variable quadratic constraint index
- Returns error codes (out-of-memory, size limit)

**Impl signature:**
```c
int cxf_build_row_major(SparseMatrix *mat)
```
- Takes SparseMatrix (only linear constraints)
- Only fills CSR arrays for linear constraints
- No quadratic/SOS/general constraint handling
- No variable-to-constraint mapping

**Analysis:**
Another **NAME COLLISION**:
- Spec: Comprehensive CSR construction for all constraint types on Model
- Impl: Simple two-pass linear CSR fill on SparseMatrix

#### V9: SORT FUNCTION MISMATCH

**Severity:** MEDIUM

**Spec:** `cxf_sort_by_values(count: int64, values: double*, indices: int*)`
- Primary sort key is **values** (ascending)
- Indices are permuted as satellites
- Hybrid quicksort (3-way partition, median-of-three) + shellsort
- Recursion depth limiting (like introsort)
- No stability guarantee

**Impl:** `cxf_sort_indices_values(indices: int*, values: double*, n: int)`
- Parameter order suggests **indices** as primary (but actually sorts by indices ascending)
- Always uses insertion sort (no hybrid algorithm)
- No quicksort, no recursion limiting
- Simpler algorithm for small arrays

**Evidence from sort.c:**
```c
// Line 24-42: insertion_sort always compares indices[j] > key_idx
static void insertion_sort(int *indices, double *values, int n) {
    for (int i = 1; i < n; i++) {
        int key_idx = indices[i];
        double key_val = (values != NULL) ? values[i] : 0.0;
        int j = i - 1;

        while (j >= 0 && indices[j] > key_idx) {  // Sorts by indices!
            indices[j + 1] = indices[j];
            if (values != NULL) {
                values[j + 1] = values[j];
            }
            j--;
        }
        // ...
    }
}
```

**Impact:** Implementation sorts by indices, spec sorts by values. Different use case.

#### V10: MISSING OPERATIONS

**Severity:** MEDIUM-HIGH

**Spec functions not found in implementation:**

1. **Scaling reversal in cxf_prepare_row_data:**
   - Division by row/column scale factors
   - Quadratic coefficient unscaling
   - PWL breakpoint unscaling
   - Range constraint negation reversal
   - Global scale factor reset

2. **Constraint partitioning in cxf_matrix_setup:**
   - Hoare partition scheme
   - Active count recording
   - Pointer swapping for column lengths/bounds
   - Reference counting

3. **Multi-constraint CSR in cxf_build_row_major:**
   - Quadratic constraint CSR index
   - SOS constraint CSR
   - Variable-to-constraint mapping
   - Single-variable quadratic optimization

**These are algorithmic gaps, not just missing utility functions.**

---

## Module 3: Matrix Finalization

### Specification Requirements (matrix_finalization.md)

The spec defines **1 function** for matrix scaling:

1. **cxf_finalize_row_data** - Compute and apply matrix scaling factors

**Key spec characteristics:**
- Multi-phase pipeline (6 phases: validation, allocation, analysis, algorithm, propagation, storage)
- 4 scaling strategies (single-pass, quick validation, iterative Ruiz, extended restructure)
- Handles all constraint types (linear, quadratic, PWL)
- Constraint sense normalization (>= to <= via sign-flip)
- Saved scaling factor reuse
- Mode parameter (auto, none, forced strategy)

### Implementation Analysis

**cxf_finalize_row_data() in row_major.c:**
```c
int cxf_finalize_row_data(SparseMatrix *mat) {
    if (mat == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    /* Verify CSR was built */
    if (mat->row_ptr == NULL) {
        return CXF_ERROR_INVALID_ARGUMENT;
    }

    /* Currently a no-op - could add validation or state flags in future */
    return CXF_OK;
}
```

### Violations Found

#### V11: COMPLETE IMPLEMENTATION MISSING

**Severity:** CRITICAL

**Spec:** 251-line module spec describing:
- Scaling factor computation (Ruiz, Curtis-Reid, geometric mean)
- Row/column equilibration
- Global objective scale
- Quadratic constraint scaling
- PWL constraint scaling
- Constraint sense normalization
- Mode selection logic
- Saved scaling reuse

**Implementation:** 13-line no-op function

**Analysis:**
The implementation comment explicitly states "Currently a no-op - could add validation or state flags in future". This confirms the function is a stub placeholder. The entire matrix scaling subsystem is unimplemented.

#### V12: WRONG SCOPE

**Severity:** HIGH

**Issue:** Implementation `cxf_finalize_row_data()` is in `row_major.c` and operates on `SparseMatrix`, but spec requires operation on `Model` with access to:
- Environment parameters (feasibility_tol, scaling_tol, scaling_hint, solve_method)
- MatrixData with saved scaling factors
- Quadratic constraint arrays
- PWL constraint arrays
- Swap data

**Current impl:** Only has SparseMatrix pointer, cannot perform scaling.

---

## Matrix Format Analysis

### CSC Format (Compressed Sparse Column) - PRIMARY STORAGE

**Spec requirement:** CSC is primary format for column-oriented simplex operations.

**Implementation (SparseMatrix struct):**
```c
/* CSC format (primary) */
int64_t *col_ptr;         /**< Column pointers [num_cols + 1] */
int *row_idx;             /**< Row indices [nnz] */
double *values;           /**< Non-zero values [nnz] */
```

**Validation (sparse_matrix.c:28-78):**
```c
int cxf_sparse_validate(const SparseMatrix *mat) {
    // Check col_ptr[0] == 0
    // Check col_ptr[num_cols] == nnz
    // Check monotonic non-decreasing col_ptr
    // Check row indices in range [0, num_rows)
}
```

**Verdict:** ✅ CORRECT - CSC format is properly implemented and validated.

### CSR Format (Compressed Sparse Row) - LAZY CONVERSION

**Spec requirement:** CSR built lazily when row access needed, cached for reuse.

**Implementation (SparseMatrix struct):**
```c
/* CSR format (optional, built lazily) */
int64_t *row_ptr;         /**< Row pointers [num_rows + 1] (NULL if not built) */
int *col_idx;             /**< Column indices [nnz] */
double *row_values;       /**< Row-major values [nnz] */
```

**Construction (sparse_matrix.c:89-172, row_major.c:94-140):**
- Two-pass algorithm: count per row, then fill
- Working copy of row_ptr for filling
- Transpose CSC to CSR

**Verdict:** ✅ CORRECT - CSR is optional, NULL when not built, standard two-pass algorithm.

### Index Type Design

**Spec:** Not explicitly specified, but Saad (2003) and Duff/Erisman/Reid (1986) references suggest int64_t for nnz, int for row/col indices.

**Implementation:**
```c
int num_rows;             /**< int limits rows to ~2B */
int num_cols;             /**< int limits cols to ~2B */
int64_t nnz;              /**< int64_t supports >2B non-zeros */
```

**Comment in header:**
```
Index Type Design:
- nnz, col_ptr, row_ptr use int64_t to support matrices with >2B non-zeros
- row_idx, col_idx use int to limit row/column count to ~2B (practical for LP)
- This saves 50% memory on index arrays compared to all-int64_t
```

**Verdict:** ✅ REASONABLE - Standard space-saving optimization for LP solvers.

---

## SpMV (Sparse Matrix-Vector Multiply) Analysis

### Implementation (multiply.c)

**cxf_matrix_multiply() - y = Ax (CSC format):**
```c
void cxf_matrix_multiply(const double *x, double *y, int num_vars,
                         int num_constrs, const int64_t *col_start,
                         const int *row_indices, const double *coeff_values,
                         int accumulate)
```

**Algorithm:**
```c
// Initialize y to zero if not accumulating
if (accumulate == 0) {
    memset(y, 0, num_constrs * sizeof(double));
}

// Iterate over columns
for (int j = 0; j < num_vars; j++) {
    double xj = x[j];
    if (xj == 0.0) continue;  // Skip zero entries

    // Accumulate column j's contribution
    for (int64_t k = col_start[j]; k < col_start[j+1]; k++) {
        int row = row_indices[k];
        y[row] += coeff_values[k] * xj;
    }
}
```

**Spec expectation:** Not explicitly specified, but standard CSC-based SpMV.

**Verdict:** ✅ CORRECT - Standard column-wise accumulation for CSC format.

**cxf_matrix_transpose_multiply() - y = A^T x:**
```c
// For A^T x, column j of A becomes row j of A^T
for (int j = 0; j < num_vars; j++) {
    double sum = 0.0;
    for (int64_t k = col_start[j]; k < col_start[j+1]; k++) {
        int row = row_indices[k];
        sum += coeff_values[k] * x[row];
    }
    y[j] = accumulate ? y[j] + sum : sum;
}
```

**Verdict:** ✅ CORRECT - CSC acts like CSR for transpose, standard algorithm.

---

## Missing Operations Summary

### Basis Module (from spec)
1. ❌ `cxf_fix_variables_at_bounds()` - Variable fixing with constraint scanning
2. ❌ `cxf_progress_snapshot()` - Scalar counter snapshot
3. ❌ `cxf_basis_diff()` - Weighted progress score (has different impl)
4. ❌ `cxf_basis_warm()` - Quadratic eta creation (has different impl)
5. ❌ `cxf_pivot_with_eta()` - Pivot eta creation

### Matrix Core Module (from spec)
1. ❌ `cxf_prepare_row_data(Model*, mode)` - Unscaling (has different impl)
2. ❌ `cxf_matrix_setup(MatrixData*)` - Constraint partitioning
3. ❌ `cxf_build_row_major(Model*)` - Multi-constraint CSR (has different impl)
4. ❌ `cxf_sort_by_values()` - Hybrid value-sort (has index-sort instead)

### Matrix Finalization Module (from spec)
1. ❌ `cxf_finalize_row_data(Model*, mode)` - Scaling computation (stub only)

**Total spec functions:** 10
**Correctly implemented:** 0
**Name collisions:** 4 (prepare_row_data, build_row_major, basis_warm, basis_diff)
**Completely missing:** 6

---

## Implementation-Only Functions (not in spec)

### Basis Module
1. `cxf_basis_create()` - BasisState allocation
2. `cxf_basis_free()` - BasisState deallocation
3. `cxf_basis_init()` - BasisState initialization
4. `cxf_basis_validate()` - Simple validation
5. `cxf_basis_validate_ex()` - Extended validation
6. `cxf_basis_warm_snapshot()` - Restore from snapshot
7. `cxf_basis_snapshot_create()` - Create array snapshot
8. `cxf_basis_snapshot_diff()` - Count array differences
9. `cxf_basis_snapshot_equal()` - Check snapshot equality
10. `cxf_basis_snapshot_free()` - Free snapshot arrays

### Matrix Module
1. `cxf_sparse_create()` - SparseMatrix allocation
2. `cxf_sparse_free()` - SparseMatrix deallocation
3. `cxf_sparse_init_csc()` - CSC array allocation
4. `cxf_sparse_validate()` - CSC validation
5. `cxf_sparse_build_csr()` - Full CSR construction
6. `cxf_sparse_free_csr()` - CSR deallocation
7. `cxf_matrix_multiply()` - SpMV y = Ax
8. `cxf_matrix_transpose_multiply()` - SpMV y = A^T x
9. `cxf_dot_product()` - Dense dot product
10. `cxf_dot_product_sparse()` - Sparse-dense dot product
11. `cxf_vector_norm()` - L_inf/L1/L2 norms
12. `cxf_sort_indices()` - Index-only sort
13. `cxf_sort_indices_values()` - Index-primary sort with values

**These are all INFRASTRUCTURE functions (lifecycle, storage, utilities), not algorithmic operations.**

---

## Root Cause Analysis

### The Module Purpose Mismatch

**Spec Modules:**
- **Basis Operations:** Algorithmic module for PFI updates during simplex (eta creation, variable fixing, cycling detection)
- **Matrix Core:** Lazy CSR conversion pipeline with scaling/partitioning
- **Matrix Finalization:** Numerical conditioning via matrix scaling

**Implementation Modules:**
- **basis/:** Data structure lifecycle and snapshot utilities
- **matrix/:** Sparse matrix storage, basic operations (SpMV, dot products)

**Conclusion:** Implementation provides **Layer 1 (Data Structures)** while spec defines **Layer 2 (Algorithms)**.

### Why This Happened

Looking at file comments and structure:

1. **basis_state.c header:**
   ```c
   * @brief BasisState structure implementation (M5.1.2)
   * Implements lifecycle functions for the BasisState structure
   * Spec: docs/specs/structures/basis_state.md  // OLD SPEC REFERENCE
   ```

2. **row_major.c header:**
   ```c
   * @brief CSR (row-major) format construction (M4.1.5)
   * Spec: docs/specs/functions/matrix/cxf_build_row_major.md  // OLD SPEC
   ```

**Evidence:** Implementation was written against **OLD SPECS** (likely docs/specs/ directory, pre-v2). The v2 specs represent a different design.

### The Architecture Gap

**V2 Spec Architecture:**
```
SolverState (top level)
├── BasisState (basis tracking)
├── MatrixData (constraint matrix with scaling)
├── Environment (parameters)
└── PricingContext (pricing state)
```

**Current Implementation Architecture:**
```
Standalone structures:
├── BasisState (independent)
└── SparseMatrix (independent)
```

**Gap:** Spec expects integrated state management, impl provides isolated components.

---

## Recommendations

### Priority 1: Critical Gaps (Blocking Simplex Algorithm)

1. **Implement Eta Vector Creation:**
   - `cxf_pivot_with_eta()` - Core PFI update for every pivot
   - `cxf_fix_variables_at_bounds()` - Variable fixing optimization
   - `cxf_basis_warm()` - Quadratic objective handling

   **Rationale:** Without eta creation, PFI algorithm cannot function. This is the basis inverse representation.

2. **Implement Matrix Scaling:**
   - Full `cxf_finalize_row_data()` implementation
   - Iterative Ruiz equilibration (primary algorithm)
   - Constraint sense normalization

   **Rationale:** Poor numerical conditioning will cause solve failures on real-world problems.

### Priority 2: Infrastructure Alignment

3. **Integrate BasisState with SolverState:**
   - Modify functions to accept `SolverState*` instead of `BasisState*`
   - Add constraint matrix access for eta creation
   - Add pricing subsystem notifications

4. **Create MatrixData wrapper around SparseMatrix:**
   - Add scaling factor storage
   - Add swap data for partitioning
   - Add environment reference

### Priority 3: Missing Optimizations

5. **Implement Active Constraint Partitioning:**
   - `cxf_matrix_setup()` with Hoare partition
   - Swap data pointer management
   - Reference counting

6. **Implement Progress Tracking:**
   - `cxf_progress_snapshot()` for scalar counters
   - `cxf_basis_diff()` weighted progress score
   - Anti-cycling integration

### Priority 4: Naming Conflicts

7. **Resolve Name Collisions:**
   - Rename impl `cxf_prepare_row_data()` → `cxf_sparse_alloc_csr()`
   - Rename impl `cxf_build_row_major()` → `cxf_sparse_fill_csr()`
   - Rename impl `cxf_basis_warm()` → `cxf_basis_restore_variables()`
   - Create new spec-compliant functions with original names

### What to Keep

**Do NOT delete the current implementation.** It provides useful infrastructure:

✅ **Keep:**
- `cxf_basis_create/free/init()` - Lifecycle is needed
- `cxf_sparse_create/free/init_csc()` - Lifecycle is needed
- `cxf_sparse_validate()` - Useful validation
- `cxf_matrix_multiply()` - Correct SpMV
- `cxf_dot_product()`, `cxf_vector_norm()` - Useful utilities
- BasisSnapshot infrastructure - Useful for debugging/warm-start

✅ **Enhance:**
- Add spec-defined algorithmic functions alongside current functions
- Current code becomes infrastructure layer, spec functions become algorithm layer

---

## Verification Checklist

- [x] All 10 implementation files read and analyzed
- [x] All 3 v2 spec files read and analyzed
- [x] Header files examined for structure definitions
- [x] CSC/CSR format verified correct
- [x] SpMV algorithm verified correct
- [x] Name collisions documented with evidence
- [x] Missing functions cataloged
- [x] Root cause identified (old spec vs v2 spec mismatch)
- [x] Recommendations prioritized by criticality

---

## Conclusion

The basis and matrix implementations are **NOT compliant** with the v2 module specifications. However, this appears to be a **phasing issue** rather than incorrect implementation:

1. Current code implements **infrastructure layer** (data structures, lifecycle, basic operations)
2. V2 specs define **algorithm layer** (PFI updates, scaling, partitioning)
3. Both layers are needed; they're complementary, not conflicting

**Next steps:**
1. Preserve current infrastructure code
2. Implement v2 spec functions as new additions
3. Integrate via SolverState/MatrixData wrappers
4. Resolve naming conflicts
5. Add comprehensive integration tests

**Estimated work:** 3-5 implementation sessions to add algorithm layer while preserving infrastructure.

---

**End of Audit Report**
