# Agent Handoff

*Last updated: 2026-02-22*

---

## STATUS: 22/35 Netlib pass. 40/40 unit tests. Implementation audit complete.

### Scorecard

**PASS (22):** afiro, sc50b, sc105, share2b, israel, adlittle, blend, lotfi, beaconfd, stocfor1, ship04l, scfxm1, standata, standgub, standmps, scagr7, sctap1, brandy, scorpion, kb2, stair, e226

**FAIL (13):** etamacro (0.016%), recipe (0.02%), boeing2 (0.09%), scagr25 (2.2%), bore3d (7.5%), finnis (7.3%), capri (10%), grow7 (12.5%), boeing1 (18%), vtp.base (20%), forplan (43%), scsd1 (TIMEOUT), bandm (TIMEOUT), tuff (TIMEOUT)

---

## Implementation Audit (2026-02-22)

**Full analysis: `docs/learnings/implementation_audit.md`**

The spec agent audited 5 subsystems across ~30 source files and found **14 significant deviations** from the cleanroom spec. The single highest-impact finding: Kahan-stable addition is entirely absent, and `cxf_pivot_update` has a wrong API that prevents correct implementation.

### Priority Fix Order (items 1-6 fix ~12 of 13 failures)

| # | Issue ID | Finding | Fixes | Effort |
|---|----------|---------|-------|--------|
| 1 | convexfeld-heyz | **C1+C3**: Redesign `cxf_pivot_update` — old/new bounds API + Kahan-stable add | 10 numerical drift | Medium |
| 2 | convexfeld-h8xm | **C2**: Full `cxf_pivot_bound` — eta, activity, matrix cleanup | All bound-tightening | Medium |
| 3 | convexfeld-bgl4 | **H4**: Phase I w-coefficients updated per pivot | boeing2, capri | Low |
| 4 | convexfeld-xd5l | **C4**: `specialMode` flag + Phase I UNBOUNDED suppression | scsd1, scagr25 | Low |
| 5 | convexfeld-70c0 | **H2**: Two-stage infeasibility confirmation in step2/step3 | boeing2, capri | Low |
| 6 | convexfeld-ifo2 | **H1**: Harris tolerance `feasTol` → `10*feasTol` | All (pivot quality) | Trivial |
| 7 | convexfeld-94em | **H3**: Inner loop convergence — remove dimension scaling, add dead zone | Large problems | Low |
| 8 | convexfeld-mbbr | **M2**: Sparse LU factorization | bandm, tuff, vtp.base | High |

### Other Audit Issues Created

| Issue ID | Finding | Priority |
|----------|---------|----------|
| convexfeld-s9am | M1: Re-enable BFRT | P2 |
| convexfeld-36qh | M3: Basis snapshot → progress counters | P2 |
| convexfeld-l0ca | M4: V1 pricing weight update drops nonbasic | P2 |
| convexfeld-ro9z | M6: refine.c must create eta records | P2 |
| convexfeld-vwpt | M5: Pricing level init 1→0 | P3 |
| convexfeld-muxv | M7: Primal crash second pass | P3 |
| convexfeld-yf1c | M8: fix_variables_at_bounds stub (deferred) | P4 |

---

## What Changed This Session

### Code Changes
1. **ftran.c** — FTRAN diagonal fallback: `*=` → `/=` (backward-compatible)
2. **btran.c** — BTRAN diagonal fallback: same fix
3. **scaling.c** — Full row+column Ruiz equilibration (DISABLED, threshold=1e30)
4. **solve_lp.c** — Scaling wiring + unscaling before extraction
5. **phase_one.c** — Phase I diag_coeff with row_scale; Phase II obj re-scale
6. **step.c** — Ratio test: bound-flip fallback + column rejection across candidates
7. **cxf_solver.h** — `row_scale`, `col_scale` fields
8. **context.c** — Scale factor allocation/freeing

### Scaling Analysis

Scaling infrastructure is complete and mathematically correct. Disabled because enabling it regresses all tested problems. Root cause: solver baseline robustness insufficient. The spec agent confirmed the reference solver handles scaling through general robustness (residual monitoring, column rejection, EXPAND), not scaling-aware mechanisms. See `docs/learnings/scaling_report.md`.

---

## DO NOT
- Enable scaling without fixing C1+C3 (pivot_update) and H1 (Harris tolerance) first
- Change diag_coeff based on RHS sign (causes regressions on israel/stair/e226)
- Re-add RC-based status reassignment in refine.c (corrupts obj)
- Re-add recovery pivots in refine.c Pass 2 (changes basis during post-solve)
- Skip reading this file and `docs/learnings/implementation_audit.md`
