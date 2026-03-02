# Agent Handoff

*Last updated: 2026-03-02*

---

## STATUS: 47/47 tests pass. 20/22 Netlib pass (bore3d newly solved). No regressions.

### Session Summary (2026-03-02, Session 4)

**Fixed convexfeld-pr0h: EXPAND Mechanism B activation + eps_base scaling**

Root cause analysis of Phase I cycling on bore3d (486/500 degenerate pivots):

1. **EXPAND activation unreachable** (`src/simplex/perturbation.c`): The threshold `degenerate_count > 100` was unreachable because `degenerate_count` resets on ANY non-degenerate pivot. bore3d has 486 degenerate out of 500 pivots, but the 14 non-degenerate ones are scattered, preventing the counter from reaching 100. Added a fallback: `iteration > 3*m && degenerate_count > 0` — fires when Phase I has run for many iterations without reaching feasibility AND is currently in a degenerate streak.

2. **eps_base scaling** (`src/simplex/perturbation.c`): The spec says "epsilon_base is typically 1e-6 to 1e-8" but this assumes feas_tol in the 1e-8 to 1e-10 range. For our feas_tol = 1e-6, the perturbation needs to be ~100x feas_tol to produce nonzero step lengths. Changed from `feas_tol * 1000` (clamped to 1e-4) to `100 * feas_tol` (clamped to [1e-8, 1e-4]). Functionally equivalent for our tolerance but properly scaled.

Result: bore3d now solves (0.013s). 47/47 tests pass. 20/22 Netlib pass (was 19/22). Zero regressions.

### Previous Session (2026-03-02, Session 3) — FTRAN error recovery + optimality verification

Fixed scsd1/kb2 crash on FTRAN errors. Added step clamping and optimality verification. 19/22 Netlib pass.

### Previous Session (2026-03-02, Session 1) — Unit tests + ratio test refactoring

Added unit tests for recompute and ratio test functions. Extracted `row_ratio()` helper.

### Previous Session (2026-02-27) — Sparse LU implementation

Sparse Markowitz-ordered LU factorization. Dense phase transition at 40% density.

---

## Open Issues

| Issue | Priority | Notes |
|-------|----------|-------|
| convexfeld-3kvi | P2 | Investigate brandy/stair regressions with sparse LU |
| convexfeld-n9ok | P2 | Phase I cycling on bore3d/grow7 — bore3d SOLVED, grow7 still fails |
| convexfeld-nt3i | P3 | Refactor sparse_elim.c to < 200 LOC |
| convexfeld-0x54 | P3 | Refactor lu_factorize.c to < 200 LOC |
| convexfeld-udn3 | IN_PROGRESS | Matrix scaling — blocked on solver robustness |

---

## Key Findings

### EXPAND activation needs dual conditions
- `degenerate_count > 100` catches pure degenerate streaks (scfxm1)
- `iteration > 3*m && degenerate_count > 0` catches scattered degeneracy (bore3d)
- Both are needed because degenerate_count resets on non-degenerate pivots

### eps_base must be proportional to feas_tol
- Spec's "1e-6 to 1e-8" assumes feas_tol of 1e-8 to 1e-10
- For our feas_tol = 1e-6, eps_base = 100*feas_tol = 1e-4
- Too small → perturbation at noise level → ratio test still produces θ=0

---

## DO NOT
- Set eps_base = feas_tol directly (too small, breaks scfxm1)
- Use struct layout changes to SolverState without verifying ALL 19 Netlib instances
- Change diag_coeff based on RHS sign (causes regressions on israel/stair/e226)
- Re-add RC-based status reassignment in refine.c (corrupts obj)
- Reference GLPK or other solver implementations (cleanroom project)
