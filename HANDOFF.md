# Agent Handoff

*Last updated: 2026-03-02*

---

## STATUS: 47/47 tests pass. Two issues closed this session.

### Session Summary (2026-03-02)

**1. Core algorithm unit tests (convexfeld-sxgk, P1)**

Added unit tests for previously-untested core functions:
- `tests/unit/test_recompute.c` (226 LOC, 10 tests): cxf_recompute_xB (identity + structural basis), cxf_recompute_objective (Phase I/II), cxf_ftran_residual
- `tests/unit/test_ratio_test.c` (+2 tests): degenerate pivot, Bland's rule

**2. Ratio test refactoring (convexfeld-gx1f, P2)**

Extracted `row_ratio()` helper from duplicated Pass 1/Pass 2 logic:
- `src/simplex/ratio_test.c`: 239 LOC → 151 LOC (37% reduction)
- Two-pass Harris structure preserved (correct per Maros 2003 Ch. 8)
- Band parameter: `band > 0` = Harris relaxation (Pass 1), `band = 0` = strict (Pass 2)
- Cached local pointers for readability: `bv`, `wx`, `wlb`, `wub`

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
