# Agent Handoff

*Last updated: 2026-02-27*

---

## STATUS: 46/46 tests pass. Sparse LU factorization implemented.

### Session Summary (2026-02-27) — Sparse LU implementation

Implemented sparse Markowitz-ordered LU factorization per Suhl & Suhl (1990)
and Maros (2003) Chapter 5. The dense `calloc(m*m)` working matrix is replaced
with column-oriented sparse storage. Dense phase transition at 40% density
handles the tail of elimination. Growth factor monitoring per spec.

**Files created:**
- `src/basis/sparse_work.c` (132 LOC) — Sparse working matrix lifecycle + basis extraction
- `src/basis/sparse_elim.c` (239 LOC) — Markowitz pivot search + Gaussian elimination
- `tests/unit/test_sparse_lu.c` (326 LOC) — 11 unit tests

**Files modified:**
- `src/basis/lu_factorize.c` (315 LOC, was 349) — Rewritten with sparse main loop + dense fallback
- `src/basis/basis_internal.h` — Added SparseCol/SparseWork struct definitions
- `CMakeLists.txt` — Added 2 source files
- `tests/CMakeLists.txt` — Added test registration

**Output contract UNCHANGED:** LUFactors struct, FTRAN, BTRAN, refactor.c — all untouched.

### Results

- **46/46 tests pass** (45 existing + 1 new test_sparse_lu)
- **19/22 Netlib pass** (all with correct reference objectives)
- **3 regressions:** brandy (ITER_LIMIT), kb2 (ITER_LIMIT — pre-existing issue per MEMORY.md), stair (timeout)
- **bandm (m=305):** No longer O(m²) memory timeout. Runs through iterations but hits solver ITER_LIMIT (Phase I cycling — pre-existing solver issue, not LU-related)

### Regression Root Cause

Sparse LU picks different pivots than dense LU (same Markowitz criterion, different column scan order in sparse vs dense storage). This produces a mathematically equivalent but numerically different factorization, changing the simplex trajectory. For brandy/stair, the new trajectory needs more iterations than the 200-iteration inner loop limit.

### Bug Found and Fixed During Implementation

**Dense phase single-elim-array bug:** Initial dense phase code used one `elim[]` array for both rows and columns. Setting `elim[piv_row]=1` also prevented column `piv_row` from being considered (and vice versa). Fixed by splitting into separate `d_relim[]` and `d_celim[]` arrays. This caused the `test_constraint_satisfaction` integration test to fail with obj=6.0 instead of -15.0.

**Count maintenance bug:** `sparse_eliminate` didn't handle the dead-entry ↔ live-entry transition correctly (cancellation decrementing counts when old value was already dead, fill-in from dead entry not incrementing counts). Fixed to check `fabs(old_val) >= MIN_PIVOT` before updating counts, matching the dense code's logic.

---

## Open Issues

| Issue | Priority | Notes |
|-------|----------|-------|
| convexfeld-3kvi | P2 | Investigate brandy/stair regressions with sparse LU |
| convexfeld-nt3i | P3 | Refactor sparse_elim.c to < 200 LOC |
| convexfeld-0x54 | P3 | Refactor lu_factorize.c to < 200 LOC |
| convexfeld-v03z | P2 | scsd1/kb2 iteration loop errors (pre-existing) |

---

## DO NOT
- Add structural variable insertion to Phase I setup (violates V2 crash spec line 176)
- Change diag_coeff based on RHS sign (causes regressions on israel/stair/e226)
- Re-add RC-based status reassignment in refine.c (corrupts obj)
- Hack refactorization parameters to fix primal accuracy
- Use row negation for BFRT (corrupts LU/eta factorization)
- Use a single elim array for rows AND columns in dense elimination
- Change `LUFactors` struct or FTRAN/BTRAN interfaces
- Reference GLPK or other solver implementations (cleanroom project — cite only Maros 2003, Suhl & Suhl 1990, Markowitz 1957)
