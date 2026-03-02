# Agent Handoff

*Last updated: 2026-03-02*

---

## STATUS: 47/47 tests pass. Core algorithm unit tests added.

### Session Summary (2026-03-02) — Core algorithm unit tests (convexfeld-sxgk)

Closed P1 issue: added unit tests for previously-untested core algorithm functions.

**Files created:**
- `tests/unit/test_recompute.c` (226 LOC) — 10 tests for cxf_recompute_xB, cxf_recompute_objective, cxf_ftran_residual

**Files modified:**
- `tests/unit/test_ratio_test.c` (+45 LOC) — Added 2 tests: degenerate pivot, Bland's rule tie-breaking
- `tests/CMakeLists.txt` — Registered test_recompute

**Test coverage added:**
- `cxf_recompute_xB`: identity basis, structural 2x2 basis, nonbasic snap-to-bound, null guard
- `cxf_recompute_objective`: Phase II c^T x, Phase I single violation, Phase I dual violation + w-coefficients
- `cxf_ftran_residual`: exact (residual=0), perturbed (known nonzero residual), null guard
- `cxf_ratio_test`: degenerate pivot (ratio=0), Bland vs largest-pivot tie-breaking

### Previous Session (2026-02-27) — Sparse LU implementation

Sparse Markowitz-ordered LU factorization. Dense phase transition at 40% density.
19/22 Netlib pass. 3 regressions from different pivot ordering (brandy, stair, kb2).

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
