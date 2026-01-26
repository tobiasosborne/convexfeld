# Milestone 4: Data Layer (Level 2)

**Goal:** Complete Matrix, Timing, Model Analysis modules
**Parallelizable:** All three modules can run in parallel
**Spec References:**
- `docs/specs/modules/09_matrix.md`
- `docs/specs/modules/11_timing.md`
- `docs/specs/modules/14_model_analysis.md`

---

## Module: Matrix Operations (7 functions)

**Spec Directory:** `docs/specs/functions/matrix/`

### Step 4.1.1: Matrix Tests

**LOC:** ~200
**File:** `tests/unit/test_matrix.c`

Comprehensive tests including SpMV, dot product, norms.

### Step 4.1.2: SparseMatrix Structure (Full)

**LOC:** ~150
**File:** `src/matrix/sparse_matrix.c`
**Spec:** `docs/specs/structures/sparse_matrix.md`

Full CSC/CSR implementation with:
- Creation/destruction
- Validation
- Format conversion

### Step 4.1.3: cxf_matrix_multiply

**LOC:** ~100
**File:** `src/matrix/multiply.c`
**Spec:** `docs/specs/functions/matrix/cxf_matrix_multiply.md`

### Step 4.1.4: cxf_dot_product, cxf_vector_norm

**LOC:** ~100
**File:** `src/matrix/vectors.c`
**Specs:**
- `docs/specs/functions/matrix/cxf_dot_product.md`
- `docs/specs/functions/matrix/cxf_vector_norm.md`

### Step 4.1.5: Row-Major Conversion

**LOC:** ~150
**File:** `src/matrix/row_major.c`
**Specs:**
- `docs/specs/functions/matrix/cxf_build_row_major.md`
- `docs/specs/functions/matrix/cxf_prepare_row_data.md`
- `docs/specs/functions/matrix/cxf_finalize_row_data.md`

### Step 4.1.6: cxf_sort_indices

**LOC:** ~80
**File:** `src/matrix/sort.c`
**Spec:** `docs/specs/functions/matrix/cxf_sort_indices.md`

---

## Module: Timing (5 functions)

**Spec Directory:** `docs/specs/functions/timing/`

### Step 4.2.1: Timing Tests

**LOC:** ~100
**File:** `tests/unit/test_timing.c`

### Step 4.2.2: Timestamp

**LOC:** ~60
**File:** `src/timing/timestamp.c`
**Spec:** `docs/specs/functions/timing/cxf_get_timestamp.md`

```c
#define _POSIX_C_SOURCE 199309L
#include <time.h>

double cxf_get_timestamp(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}
```

### Step 4.2.3: Timing Sections

**LOC:** ~100
**File:** `src/timing/sections.c`
**Specs:**
- `docs/specs/functions/timing/cxf_timing_start.md`
- `docs/specs/functions/timing/cxf_timing_end.md`
- `docs/specs/functions/timing/cxf_timing_update.md`

### Step 4.2.4: Operation Timing

**LOC:** ~80
**File:** `src/timing/operations.c`
**Specs:**
- `docs/specs/functions/timing/cxf_timing_pivot.md`
- `docs/specs/functions/basis/cxf_timing_refactor.md`

---

## Module: Model Analysis (6 functions)

**Spec Directory:** `docs/specs/functions/validation/`, `docs/specs/functions/statistics/`

### Step 4.3.1: Analysis Tests

**LOC:** ~100
**File:** `tests/unit/test_analysis.c`

### Step 4.3.2: Model Type Checks

**LOC:** ~80
**File:** `src/analysis/model_type.c`
**Specs:**
- `docs/specs/functions/validation/cxf_is_mip_model.md`
- `docs/specs/functions/validation/cxf_is_quadratic.md`
- `docs/specs/functions/validation/cxf_is_socp.md`

### Step 4.3.3: Coefficient Statistics

**LOC:** ~120
**File:** `src/analysis/coef_stats.c`
**Specs:**
- `docs/specs/functions/statistics/cxf_coefficient_stats.md`
- `docs/specs/functions/statistics/cxf_compute_coef_stats.md`

### Step 4.3.4: Presolve Statistics

**LOC:** ~80
**File:** `src/analysis/presolve_stats.c`
**Spec:** `docs/specs/functions/statistics/cxf_presolve_stats.md`
