# Agent Handoff

*Last updated: 2026-02-26*

---

## STATUS: 45/45 unit tests pass. Sparse LU plan ready for implementation.

### Session Summary (2026-02-26) — Research session, no code changes

Researched and designed sparse LU factorization (convexfeld-mbbr). Four parallel research agents analyzed: V2 spec requirements, gotchas/audit docs, current dense LU implementation, and sparse LU algorithms from literature (Gilbert-Peierls, GLPK Markowitz, LUSOL, BASICLU).

### Priority: Implement Sparse LU (convexfeld-mbbr)

**The single highest-impact performance fix remaining.** Dense `calloc(m*m)` in `lu_factorize.c:295` causes timeouts on bandm (m=305) and tuff (m=333). All other infrastructure (FTRAN/BTRAN, eta vectors, refactorization) already works with sparse L/U.

---

## SPARSE LU IMPLEMENTATION PLAN

### Approach: Right-Looking Sparse Markowitz

Two candidates were evaluated:

| | Gilbert-Peierls (Left-Looking) | Sparse Markowitz (Right-Looking) |
|---|---|---|
| Pivot strategy | Column partial pivoting + static pre-ordering | Dynamic Markowitz (already used) |
| Fill-in control | Requires AMD/COLAMD pre-ordering | Dynamic per-step |
| LP singleton handling | Through pre-ordering | Direct: cost 0, selected first |
| Code complexity | ~150 lines + ~250 lines pre-ordering | ~300 lines, self-contained |
| External dependencies | Needs AMD/COLAMD implementation | None |

**Decision: Sparse Markowitz.** Rationale:
1. Current code already uses Markowitz — minimal conceptual change
2. No pre-ordering infrastructure needed — self-contained
3. Natural for LP bases — singletons (slacks) get Markowitz cost 0
4. Dense phase transition at tail provides robustness
5. Battle-tested in GLPK for decades

### Output Contract (UNCHANGED)

`cxf_lu_factorize(LUFactors *lu, SolverState *ctx)` produces identical output:
- L in CSC (`L_col_ptr`, `L_row_idx`, `L_values`) — unit diagonal implicit
- U in CSC (`U_col_ptr`, `U_row_idx`, `U_values`) + `U_diag`
- `perm_row`, `perm_col` — row/column permutations
- Returns 0/3/1001

**FTRAN, BTRAN, refactor.c, eta infrastructure — ALL UNCHANGED.**

### Data Structure

```c
/* Per-column sparse vector */
typedef struct {
    int *row_idx;    /* row indices */
    double *values;  /* values */
    int len;         /* current entries */
    int cap;         /* capacity */
} SparseCol;

/* Sparse working matrix for LU factorization */
typedef struct {
    int m;
    SparseCol *cols;     /* [m] column vectors */
    int *row_count;      /* [m] nonzeros per active row */
    double *col_max;     /* [m] column maxima for Markowitz threshold */
    int *row_active;     /* [m] 1=active, 0=eliminated */
    int *col_active;     /* [m] 1=active, 0=eliminated */
    int active_count;    /* remaining active rows/cols */
    int total_nnz;       /* current total nonzeros */
} SparseWork;
```

### File Plan (3 new/modified files, 200 LOC limit respected)

#### NEW: `src/basis/sparse_work.c` (~130 LOC)

Sparse working matrix lifecycle + extraction from basis.

- `sparse_work_create(int m)` — allocate structure, columns start empty
- `sparse_work_extract(SparseWork *sw, SolverState *ctx)` — populate from basis columns. Structural vars: read CSC entries. Slacks: single entry from diag_coeff. Compute row_count and col_max.
- `sparse_work_free(SparseWork *sw)` — deallocate everything
- `sparse_col_append(SparseCol *col, int row, double val)` — append entry, double capacity on growth (start cap=8)
- `sparse_work_density(SparseWork *sw)` — `total_nnz / (active_count^2)`

#### NEW: `src/basis/sparse_elim.c` (~170 LOC)

Sparse Markowitz pivot search + Gaussian elimination step.

- `sparse_find_pivot(SparseWork *sw, int *out_row, int *out_col, double *out_val)` — For each active column: threshold = MARKOWITZ_TOL × col_max[j]. Scan column entries for active rows passing threshold. Score = (row_count[i]-1) × (col_len-1). Track minimum score, break ties by largest |value|.
- `sparse_eliminate(SparseWork *sw, int piv_row, int piv_col, double piv_val, COO L arrays, int step)` — For each row i with entry in pivot column (i != piv_row): compute mult = a[i,q]/a[p,q], store L entry. Then for each pivot row entry (col j): find/update a[i,j] in column j (scan column for row i). Fill-in: append new entry. Cancellation: set to 0, decrement counts. Update col_max for affected columns.
- `sparse_extract_pivot_row(SparseWork *sw, int piv_row, int *cols, double *vals)` — Scan all active columns, collect entries at piv_row. O(total_nnz) but done once per step; singletons eliminated first so active set shrinks fast.
- `sparse_to_dense(SparseWork *sw, double *D, int *map_row, int *map_col)` — Copy remaining active submatrix to dense array for dense phase transition.

#### REWRITE: `src/basis/lu_factorize.c` (~170 LOC, down from 349)

New main loop flow:

```
cxf_lu_factorize(LUFactors *lu, SolverState *ctx):
  1. SparseWork *sw = sparse_work_create(m)
  2. sparse_work_extract(sw, ctx)
  3. COO arrays for L (L_i, L_j, L_v)
  4. Sparse phase — for step = 0..m-1:
     a. If sparse_work_density(sw) > 0.4 → break to dense phase
     b. sparse_find_pivot(sw, ...) → Markowitz on sparse entries
     c. perm_row[step] = piv_row, perm_col[step] = piv_col, U_diag[step] = val
     d. sparse_extract_pivot_row(sw, piv_row, ...) → get pivot row entries
     e. sparse_eliminate(sw, ...) → elimination + L entries
  5. Dense phase (if triggered):
     a. sparse_to_dense(sw, D, map_row, map_col) → copy remaining
     b. Dense Markowitz loop (simplified existing find_pivot + eliminate_step)
  6. build_lu_output(lu, ...) → assemble L, U in CSC (existing function)
  7. sparse_work_free(sw)
```

`build_lu_output()` stays — it builds sparse CSC L/U from COO arrays, unchanged.

#### MINOR: `src/basis/basis_internal.h`
Add forward declarations for SparseWork types.

#### MINOR: `CMakeLists.txt`
Add `src/basis/sparse_work.c` and `src/basis/sparse_elim.c` to source list (after line 95).

### What Changes vs What Doesn't

| Component | Change? | Notes |
|-----------|---------|-------|
| `lu_factorize.c` | REWRITE | Dense → sparse working matrix |
| `sparse_work.c` | NEW | Per-column sparse data structure |
| `sparse_elim.c` | NEW | Sparse Markowitz + elimination |
| `CMakeLists.txt` | ADD 2 files | After line 95 |
| `basis_internal.h` | MINOR | Forward declarations |
| `lu_factors.c` | NO | LUFactors struct unchanged |
| `ftran.c` | NO | Same LU CSC input |
| `btran.c` | NO | Same LU CSC input |
| `refactor.c` | NO | Calls cxf_lu_factorize unchanged |
| `cxf_basis.h` | NO | LUFactors typedef unchanged |
| `pivot_eta.c` | NO | Unrelated |

### Elimination Detail (for implementor)

For each active row i with nonzero in pivot column q:
```
mult = a[i,q] / a[p,q]
Store (i, step, mult) in L_COO

For each column j in pivot row's nonzero set:
  Scan column j's entries for row i:
    If found: new_val = old_val - mult * pivot_row_val
      If |new_val| < MIN_PIVOT → mark cancelled, decrement row_count[i], total_nnz
    If not found: fill-in →
      sparse_col_append(col_j, i, -mult * pivot_row_val)
      Increment row_count[i], total_nnz

Remove pivot column entries, mark row p and col q inactive
Recompute col_max for columns that had pivot row entries (only those are affected)
Decrement active_count
```

### Testing Strategy

1. **Unit tests** — add to test_lu_factors.c:
   - sparse_work_create/free lifecycle
   - sparse_work_extract on known 3×3 basis (identity, then mixed structural+slack)
   - sparse_find_pivot returns correct Markowitz pivot on hand-crafted example
   - sparse_eliminate produces correct L entries
   - Full factorize on 4×4 known matrix, verify L*U matches original

2. **Existing tests** — `ctest` must pass all 45 tests unchanged

3. **Netlib (1-2 instances max)**:
   - `./diagnose benchmarks/netlib/feasible/afiro.mps` — known passing, regression check
   - `./diagnose benchmarks/netlib/feasible/bandm.mps` — THE target (currently timeout)

### Key Constants

- `MARKOWITZ_THRESHOLD = CXF_MARKOWITZ_TOL = 1/128 ≈ 0.0078` — stability threshold for pivot acceptance
- `MIN_PIVOT = CXF_MIN_PIVOT = 1e-13` — absolute zero threshold
- Dense phase transition: density > 0.4 (40% of active submatrix is nonzero)
- Initial column capacity: 8 entries (doubles on growth)

### Risks & Mitigations

| Risk | Mitigation |
|------|-----------|
| Fill-in causes excessive realloc | Start columns at cap=8, double on growth, typical LP fill-in is modest |
| Pivot row extraction O(total_nnz) | Acceptable for m<1000; singletons eliminated first shrinks active set quickly |
| Dense phase transition wrong | Unit test with matrix that triggers it; fallback is current working dense code |
| Numerical differences from tie-breaking | Same Markowitz criterion → near-identical pivots; minor differences are fine |

---

## Previous Session Work (2026-02-26, earlier)

Three bugs fixed:
1. **context.c** — col_nz_count init order (was always zero)
2. **phase_one.c** — Removed non-spec Phase I Step 3 structural swap
3. **solve_lp.c** — Row classification completion after crash

### Pre-Existing Issues (unchanged)
- **scsd1**: CXF_ERROR_INVALID_ARGUMENT at iteration 87 in full solve_lp path
- **kb2**: ITERATION_LIMIT in full solve_lp but works via diagnose tool

### Audit Status

| Audit Item | Status | Notes |
|------------|--------|-------|
| col_nz_count init order | DONE | |
| Phase I Step 3 (non-spec) | DONE | Removed |
| Row classification completion | DONE | |
| M2: Sparse LU | PLANNED | Full plan above |
| scsd1 iter-87 error | TODO | |

---

## DO NOT
- Add structural variable insertion to Phase I setup (violates V2 crash spec line 176)
- Change diag_coeff based on RHS sign (causes regressions on israel/stair/e226)
- Re-add RC-based status reassignment in refine.c (corrupts obj)
- Hack refactorization parameters to fix primal accuracy — needs sparse LU
- Use row negation for BFRT (corrupts LU/eta factorization)
- Use Gilbert-Peierls without implementing AMD/COLAMD pre-ordering (severe fill-in without it)
- Change `LUFactors` struct or FTRAN/BTRAN interfaces
