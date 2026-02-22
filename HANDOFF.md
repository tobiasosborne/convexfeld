# Agent Handoff

*Last updated: 2026-02-22*

---

## STATUS: 22/35 Netlib pass. 40/40 unit tests. Scaling infrastructure ready but disabled.

### Scorecard

**PASS (22):** afiro, sc50b, sc105, share2b, israel, adlittle, blend, lotfi, beaconfd, stocfor1, ship04l, scfxm1, standata, standgub, standmps, scagr7, sctap1, brandy, scorpion, kb2, stair, e226

**FAIL (13):** etamacro (0.016%), recipe (0.02%), boeing2 (0.09%), scagr25 (2.2%), bore3d (7.5%), finnis (7.3%), capri (10%), grow7 (12.5%), boeing1 (18%), vtp.base (20%), forplan (43%), scsd1 (TIMEOUT), bandm (TIMEOUT), tuff (TIMEOUT)

---

## What Changed This Session

### Code Changes
1. **ftran.c** — Fixed FTRAN diagonal fallback: `*= diag_coeff` → `/= diag_coeff`. This is backward-compatible (1/±1 = ±1) but required for non-±1 diag_coeff when scaling is active.
2. **btran.c** — Same fix for BTRAN diagonal fallback.
3. **cxf_solver.h** — Added `row_scale`, `col_scale` fields to SolverState (NULL when no scaling).
4. **cxf_basis.h** — Updated diag_coeff comment to note scaling support.
5. **context.c** — Added free calls for row_scale/col_scale.
6. **scaling.c** — Complete rewrite: full row+column Ruiz equilibration (matrix_finalization.md Strategy 3). Includes slack-aware row norms (diag_coeff included in row infinity norm). DISABLED via threshold=1e30.
7. **solve_lp.c** — Wired scaling call + unscaling before solution extraction.
8. **phase_one.c** — Phase I sets `diag_coeff = row_scale * sense_sign` when scaling active. Phase II transition re-scales restored objective.

### Key Finding: Scaling Is Systems-Limited

The scaling implementation is mathematically correct:
- `A' = D_r * A * D_c` with consistent bound/RHS/obj/diag_coeff transformation
- Objective value invariant (c_s^T y = c^T x)
- FTRAN/BTRAN correctly divide by diag_coeff (not multiply)
- Slack-aware row norms prevent over-scaling

But enabling scaling **regresses every tested problem**:
- blend: PASS → UNBOUNDED
- stair: PASS → INFEASIBLE
- boeing1: 18% → 41% error (worse)
- grow7: 12.5% → 15% error (worse)

Root cause: the solver's anti-degeneracy, ratio test, and Phase I convergence aren't robust enough to handle the different iteration path scaling creates.

---

## Next Steps

### To Enable Scaling (requires solver robustness first)
1. Improve anti-cycling: EXPAND perturbation must work reliably
2. Ratio test fallback: don't return UNBOUNDED when no blockers — try alternate candidates
3. Phase I convergence: forced refactorize + recompute cycle on stall
4. Set threshold to ~1e4 (catches boeing1/grow7/bore3d but not blend/stair)
5. Run full 35-problem regression

### Other Priorities
- etamacro, recipe, boeing2: near-miss problems (< 0.1% error)
- scsd1: Phase I cycling on all-equality problem
- bandm, tuff: dense LU performance (timeout)
- finnis: solver-vs-diagnose discrepancy (see previous HANDOFF)

---

## DO NOT
- Enable scaling without fixing ratio test UNBOUNDED fallback first
- Change diag_coeff based on RHS sign (causes regressions on israel/stair/e226)
- Re-add RC-based status reassignment in refine.c (corrupts obj)
- Re-add recovery pivots in refine.c Pass 2 (changes basis during post-solve)
- Skip reading this file and docs/learnings/gotchas.md
