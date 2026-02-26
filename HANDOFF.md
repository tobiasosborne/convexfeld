# Agent Handoff

*Last updated: 2026-02-26*

---

## STATUS: 45/45 unit tests pass. Three bugs fixed (col_nz_count init, non-spec dead code, row classification).

### Session Summary (2026-02-26) — 1 issue closed, 1 new issue filed

| Issue | Priority | What |
|-------|----------|------|
| convexfeld-q45o | P1 | Fixed col_nz_count init order, removed non-spec Phase I Step 3, completed row classification |

### What Changed

1. **context.c** — `col_nz_count` was populated from `csc_col_ptr` BEFORE the CSC copy created it. Every element was always zero. Moved population after CSC copy. This is a prerequisite for any future crash candidate pre-classification.

2. **phase_one.c** — Removed Phase I Step 3 structural swap (53 lines). V2 crash spec line 176 explicitly says crash constructs a slack-only basis. Step 3 violated this. It was dead code (col_nz_count always zero) that caused regressions when activated.

3. **solve_lp.c** — Added 8-line loop after crash to mark remaining UNASSIGNED rows as BASIC_LOWER. Crash returns early on infeasible equality rows (V2 spec), leaving subsequent rows unclassified. This satisfies spec postcondition 1.

### Pre-Existing Issues Discovered

- **scsd1**: Returns CXF_ERROR_INVALID_ARGUMENT (10003) at iteration 87 through the full `cxf_solve_lp` path. The diagnose tool doesn't hit this because it uses a simpler loop (no phase_end, step2, step3, perturbation). Root cause is likely numerical corruption during degenerate Phase I cycling on the all-equality problem. Filed as convexfeld-5z94 update needed.

- **kb2**: Returns ITERATION_LIMIT through `cxf_solve_lp` but works via diagnose tool. Same class of issue — the full iteration loop (with phase_end/step2/step3/perturbation) diverges from the diagnose loop.

### Audit Status

All previous audit items unchanged. New items:

| Audit Item | Status | Notes |
|------------|--------|-------|
| col_nz_count init order | DONE | Was always zero; now correct |
| Phase I Step 3 (non-spec) | DONE | Removed — violated V2 crash spec |
| Row classification completion | DONE | solve_lp.c completes after crash |
| M2: Dense LU | TODO | Causes timeouts on bandm/tuff |
| scsd1 iter-87 error | TODO | Pre-existing, needs investigation |

### Next Steps (for next agent)

1. **Investigate scsd1/kb2 iteration loop errors** — The full solve_lp iteration loop (phase_end + step2 + step3 + perturbation + post_iterate) produces errors that the diagnose tool's simpler loop doesn't. Something in the additional functions corrupts state during degenerate Phase I cycling.

2. **Crash candidate pre-classification** — col_nz_count now works. The V2 spec (crash_basis.md line 82) describes a "separate initialization step" that marks rows with positive status for crash candidate removal. This is not yet implemented.

3. **Sparse LU** (M2) — Dense `calloc(m*m)` causes timeouts on bandm/tuff.

---

## DO NOT
- Add structural variable insertion to Phase I setup (violates V2 crash spec line 176)
- Change diag_coeff based on RHS sign (causes regressions on israel/stair/e226)
- Re-add RC-based status reassignment in refine.c (corrupts obj)
- Hack refactorization parameters to fix primal accuracy — needs sparse LU
- Use row negation for BFRT (corrupts LU/eta factorization)
