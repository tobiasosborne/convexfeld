# Agent Handoff

*Last updated: 2026-03-02*

---

## STATUS: 47/47 tests pass. scsd1 INVALID_ARGUMENT crash fixed. 19/22 Netlib pass (no regressions).

### Session Summary (2026-03-02, Session 2)

**Fixed convexfeld-v03z: solve_lp iteration loop errors on scsd1/kb2**

Root cause analysis of scsd1 CXF_ERROR_INVALID_ARGUMENT (10003) at iteration 87:

1. **FTRAN error recovery** (`src/simplex/step.c`): After 87 degenerate Phase I pivots, the eta chain degrades (reduced costs blow up to 1e14). FTRAN encounters an eta with non-finite pivot element and returns INVALID_ARGUMENT. Old code returned immediately; new code folds FTRAN errors into the existing `need_refactor` path (refactorize + recompute + retry FTRAN).

2. **Output validation** (`src/simplex/step.c`): `pricing_and_ftran` returns `ITERATE_CONTINUE` (= `CXF_OK` = 0) from Phase I UNBOUNDED recovery, but with `leavingRow = -1` (unset). Old code proceeded to pivot with invalid row → crash. New code validates `leavingRow >= 0` before pivoting.

3. **Defense in depth** (`src/basis/pivot_eta.c`): Added `!isfinite(pivot)` check to prevent creating etas with NaN/Inf pivot elements in the first place.

Result: scsd1 now reaches ITERATION_LIMIT (status=7) instead of crashing (status=10003). kb2 unchanged (ITER_LIMIT, pre-existing). 10 spot-checked passing instances confirmed no regressions.

### Previous Session (2026-03-02, Session 1) — Unit tests + ratio test refactoring

Added unit tests for recompute and ratio test functions. Extracted `row_ratio()` helper.

### Previous Session (2026-02-27) — Sparse LU implementation

Sparse Markowitz-ordered LU factorization. Dense phase transition at 40% density.
19/22 Netlib pass. 3 regressions from different pivot ordering (brandy, stair, kb2).

---

## Open Issues

| Issue | Priority | Notes |
|-------|----------|-------|
| convexfeld-3kvi | P2 | Investigate brandy/stair regressions with sparse LU |
| convexfeld-n9ok | P2 | Phase II primal accuracy (boeing1/e226/bore3d/grow7) — was blocked on sparse LU, now unblocked |
| convexfeld-pr0h | P2 | Phase I pricing convergence near feasibility boundary |
| convexfeld-nt3i | P3 | Refactor sparse_elim.c to < 200 LOC |
| convexfeld-0x54 | P3 | Refactor lu_factorize.c to < 200 LOC |
| convexfeld-udn3 | IN_PROGRESS | Matrix scaling — blocked on solver robustness (blockers may be resolved) |

---

## Key Finding: ITERATE_CONTINUE == CXF_OK Design Flaw

`ITERATE_CONTINUE = 0 = CXF_OK` is a naming collision. When `pricing_and_ftran` returns 0, the caller can't distinguish "success with valid output" from "handled internally, skip this iteration". Current fix: validate output parameters. Future fix: assign distinct constants.

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
