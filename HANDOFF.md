# Agent Handoff

*Last updated: 2026-02-22*

---

## STATUS: 21/36 Netlib pass. Numerical maintenance implemented. 40/40 unit tests pass.

### Scorecard

**PASS (21):** afiro, sc50b, sc105, share2b, israel, adlittle, blend, lotfi, beaconfd, stocfor1, ship04l, scfxm1, standata, standgub, standmps, scagr7, sctap1, brandy, **scorpion**, **kb2**, **stair**

**FAIL (15):** See root cause table below.

### What Changed This Session (3 new passes, 0 regressions)

1. **Numerical maintenance (recompute.c)** — New file with `cxf_recompute_xB`, `cxf_recompute_objective`, `cxf_ftran_residual`. After every refactorization: recompute x_B = B^{-1}(b - Nx_N) from scratch, recompute objective from scratch, snap nonbasic vars to exact bounds.

2. **Adaptive refactorization interval** — Changed `cxf_refactor_check` to use `min(100, max(50, m/4))` instead of fixed 100. Added residual-triggered adaptive reduction via `thresholds[5]`.

3. **FTRAN residual monitoring** — Every 20 iterations when eta_count > 10, compute ||a - B*(B^{-1}a)||_inf. If > 10 * feasibility_tol, force refactorization + recompute.

4. **Final accuracy pass at OPTIMAL** — Force refactorization + x_B recompute + objective recompute before extracting solution. Fixed scorpion (0.017% → PASS) and kb2 (0.024% → PASS).

5. **Phase I infeasibility safety net** — Before declaring INFEASIBLE, force refactorization + recompute to clear drift. If fresh RC scan finds improving direction, continue. Fixed capri (INFEASIBLE → OPTIMAL 10%) and improved boeing2.

6. **Phase I near-feasibility tolerance** — If Phase I obj < 100 * feas_tol, try 100x tighter pricing tolerance.

---

## Remaining Failures (Priority Order)

### 1. Large Numerical Drift — 7 instances

These reach OPTIMAL but with significant objective error. The x_B recomputation helps at refactorization points, but drift re-accumulates between refactorizations, leading to wrong simplex iteration paths.

| Instance | Error | Notes |
|----------|-------|-------|
| etamacro | 0.016% | Just above 0.01% threshold. Suboptimal vertex. |
| boeing2 | 0.09% | Was INFEASIBLE, now OPTIMAL. Close to pass. |
| recipe | 0.76% | Small problem (6 vars). Unchanged. |
| scagr25 | 2.2% | Was UNBOUNDED, now OPTIMAL. |
| bore3d | 7.5% | Phase II basic vars past bounds (pre-existing). |
| grow7 | 12.5% | Phase II basic vars past bounds (pre-existing). |
| finnis | 7.3% | Degeneracy + numerical drift. |

**Root cause:** Basic variables go past bounds during Phase II due to accumulated error in primal updates. The ratio test prevents bound violations theoretically, but floating-point errors in pivotCol and step computation allow small violations that compound.

**What would fix it:** More frequent refactorization + x_B recomputation. Currently every ~87-100 pivots. Consider every 25-50 pivots for problems with high constraint counts. Also: verify that `cxf_simplex_refine` Pass 1 (nonbasic snap) + Pass 2 (basic recovery) are working correctly.

### 2. Boeing1 — Special Case

**boeing1**: OPTIMAL but err=32.7% (improved from 176%). Phase II has 20+ basic vars past bounds with worst_infeas=1000. The problem has 570 vars with infinite upper bounds (ub_inf), creating a poorly-conditioned basis.

**Likely fix:** Matrix scaling (Priority from HANDOFF v1). boeing1's coefficient range is extreme (~1 to ~3000). Without scaling, the LU factorization amplifies errors.

### 3. E226 — Dual Degeneracy

**e226**: OPTIMAL but err=61%. Known dual degeneracy issue (documented in gotchas.md). Phase I passes but Phase II drifts massively. The dual degenerate point traps the solver in a suboptimal region.

**What would fix it:** Steepest edge pricing (better direction selection) or advanced anti-cycling (Wolfe perturbation).

### 4. Capri — Phase I Difficulty

**capri**: OPTIMAL but err=10.3% (was INFEASIBLE). The forced refactorization safety net helps, but Phase I still struggles to drive infeasibility below 12.3 (from starting 33780). High degeneracy (many consecutive step=0 pivots).

**What would fix it:** Better crash basis (fewer artificial-like entries), EXPAND perturbation effectiveness improvement.

### 5. Forplan — RANGES Parsing Missing

**forplan**: OPTIMAL but err=42.9%. MPS RANGES section is parsed as a section header but lines are silently ignored. Beads issue: convexfeld-g3z4.

### 6. Timeouts — 3 instances

| Instance | Status | Notes |
|----------|--------|-------|
| scsd1 | TIMEOUT | Phase I cycling: obj=1.0 stuck with 77 equality constraints, all degenerate pivots |
| bandm | TIMEOUT | Performance — needs sparse LU forward/back substitution |
| tuff | TIMEOUT | Performance — needs sparse LU forward/back substitution |

### 7. vtp.base — Now OPTIMAL (20% error)

Was TIMEOUT, now OPTIMAL. Same root cause as #1 (numerical drift).

---

## Key Files Changed

| File | Change |
|------|--------|
| `src/simplex/recompute.c` | **NEW** — cxf_recompute_xB, cxf_recompute_objective, cxf_ftran_residual |
| `src/simplex/step.c` | FTRAN residual monitoring, recompute calls after refactorization |
| `src/simplex/solve_lp.c` | Final accuracy pass at OPTIMAL |
| `src/simplex/phase_loop.c` | Forced recompute + tighter tolerance before INFEASIBLE |
| `src/basis/refactor.c` | Adaptive eta threshold in cxf_refactor_check |
| `CMakeLists.txt` | Added recompute.c |

## DO NOT

- Re-introduce per-pivot bound snapping in cxf_apply_pivot (causes butterfly-effect regressions)
- Recompute Phase I objective without updating work_obj[] (w-coefficients)
- Claim BFRT row negation is a spec bug (it's not — see MEMORY.md)
- Skip reading this file and `docs/remediation_plan.md`
