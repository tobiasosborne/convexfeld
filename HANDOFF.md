# Agent Handoff

*Last updated: 2026-02-22*

---

## STATUS: 22/35 Netlib pass. 40/40 unit tests. RANGES parsing implemented (RC4).

### Scorecard

**PASS (22):** afiro, sc50b, sc105, share2b, israel, adlittle, blend, lotfi, beaconfd, stocfor1, ship04l, scfxm1, standata, standgub, standmps, scagr7, sctap1, brandy, **scorpion**, **kb2**, **stair**, **e226**

**FAIL (13):** etamacro (0.016%), recipe (0.02%), boeing2 (0.09%), scagr25 (2.2%), bore3d (7.5%), finnis (7.3%), capri (10%), grow7 (12.5%), boeing1 (18%), vtp.base (20%), forplan (43%), scsd1 (TIMEOUT), bandm (TIMEOUT), tuff (TIMEOUT)

---

## What Changed This Session

### Code Changes
1. **recompute.c** (NEW) — x_B + obj recomputation at refactorization. Fixed scorpion, kb2.
2. **refactor.c** — adaptive eta threshold min(100, max(50, m/4))
3. **step.c** — FTRAN residual monitoring + recompute calls after refactorization
4. **solve_lp.c** — final accuracy pass at OPTIMAL
5. **phase_loop.c** — forced refactorize/recompute before INFEASIBLE declaration
6. **refine.c** — CRITICAL: removed RC-based status reassignment (was corrupting obj) and recovery pivots (was changing basis during post-solve). Recipe: 0.76% → 0.02%
7. **post.c** — replaced eta clearing with full refactorization
8. **perturbation.c** — restricted EXPAND to Phase I only
9. **Reference CSV** — fixed e226 from -11.64 (wrong, presolved) to -18.75 (correct)

### Key Bugs Found via Diagnostic Tool
1. **refine.c moved nonbasic vars without maintaining Ax=b** — recipe went from correct -266.6 to wrong -268.6 after refine
2. **post.c cleared etas without refactoring** — destroyed basis inverse representation
3. **EXPAND in Phase II corrupts simplex path** — scagr25 went from correct to 2.2% error
4. **e226 reference CSV was wrong** — our solver was correct all along (-18.75)
5. **scsd1 DIAG_MISMATCH** — equality row 5 has diag=+1 but diagnostic expects -1 (NOT fixable by RHS-dependent diag — causes regressions on israel/stair/e226)

---

## Remaining Failures — Detailed Root Causes

### Near-misses (just above 0.01% threshold)
| Instance | Error | Root Cause |
|----------|-------|-----------|
| etamacro | 0.016% | PIVCOL max=37300 at iter 742, basis ill-conditioning causes bound violation (worst_infeas jumps 0.057→1.73) |
| recipe | 0.02% | Fixed by refine fix, residual from degenerate Phase II pivots |
| boeing2 | 0.09% | Minor numerical drift |

### Moderate errors
| Instance | Error | Root Cause |
|----------|-------|-----------|
| scagr25 | 2.2% | EXPAND in Phase I corrupts Phase I path → different Phase II starting basis. Without EXPAND: PASS. With: FAIL. Trade-off with stair. |
| bore3d | 7.5% | Never exits Phase I: 248 degenerate pivots then var-218 numerical 2-cycle (PIVCOL max/min ratio 10^16) |
| finnis | 7.3% | Diagnose tool gets 0.2%! solver gets 7.3%. NOT refine, NOT step2/step3, NOT doScan, NOT perturbation timing. Unknown remaining architectural difference. |
| capri | 10% | Phase I degeneracy with high residual infeasibility (obj=12.3, 99.96% reduced) |
| grow7 | 12.5% | No Phase I needed. 31 basic vars past bounds (worst=144000). Solver overshoots optimal. |
| boeing1 | 18% | Extreme coefficient range (~1 to ~3000), 570 vars with ub=inf |
| vtp.base | 20% | Same class as grow7 (primal accuracy) |
| forplan | TIMEOUT | RANGES now parsed (RC4 done) but solver can't handle bounded slacks from ranges yet |

### Timeouts
| Instance | Root Cause |
|----------|-----------|
| scsd1 | All-equality Phase I cycling. Every iteration degenerate. RCs blow up to 10^9 → false UNBOUNDED/TIMEOUT |
| bandm | Dense LU performance |
| tuff | Dense LU performance |

---

## Unsolved Mystery: finnis solver-vs-diagnose discrepancy

The diagnose tool gets 0.2% error (943 iters, nearly correct). The solver gets 7.3% (906 iters). Tested and ELIMINATED:
- step2/step3 (bound propagation) — disabling didn't change result
- post-pivot phase_end doScan=1 — disabling didn't change result
- Reactive perturbation timing — disabling didn't change result
- refine.c post-solve — already fixed

**Remaining suspects:**
- Bland's rule activation logic (different threshold: `perturb_count > 0 && degenerate_count > 3*m` vs step.c internal `degenerate_count > 50`)
- Outer loop convergence detection (basis_diff breaking inner loop)
- The two calls to phase_end per iteration (diagnose calls once, solver calls twice)
- Some subtle interaction between all the above

---

## DO NOT
- Change diag_coeff based on RHS sign (causes regressions on israel/stair/e226)
- Re-add RC-based status reassignment in refine.c (corrupts obj)
- Re-add recovery pivots in refine.c Pass 2 (changes basis during post-solve)
- Enable EXPAND Mechanism B in Phase II (corrupts simplex path)
- Skip reading this file and docs/learnings/gotchas.md
