# Agent Handoff

*Last updated: 2026-02-22*

---

## STATUS: 22/36 Netlib pass. Numerical maintenance implemented. 40/40 unit tests pass.

### Scorecard

**PASS (22):** afiro, sc50b, sc105, share2b, israel, adlittle, blend, lotfi, beaconfd, stocfor1, ship04l, scfxm1, standata, standgub, standmps, scagr7, sctap1, brandy, **scorpion**, **kb2**, **stair**, **e226**

**FAIL (14):** See root cause table below.

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

### 3. E226 — FIXED (reference CSV was wrong)

**e226** now PASS. The reference CSV had -11.64 from the presolved version (p_e226.mps). Our answer of -18.75 matches the spec oracle and published Netlib optimal.

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

## Diagnostic Findings (tools/diagnose.c)

Detailed per-problem root cause analysis from this session:

### etamacro (err=0.016%, needs 0.01%)
- Phase II: PIVCOL max reaches 37300 at iter 742 (severe ill-conditioning)
- worst_infeas jumps from 0.057 to 1.73 between iter 699-749
- The bound violation shifts the solver to a wrong vertex
- **Fix:** Trigger refactorization when worst_infeas exceeds feasibility_tol

### bore3d (never exits Phase I)
- 248 consecutive degenerate pivots, then numerical 2-cycle on var 218
- Pivot column max/min ratio reaches 10^16
- Reduced costs blow up to 10^6 during cycling
- **Fix:** Force refactorization every 50-100 degenerate pivots in Phase I

### grow7 (12.5% — overshoots optimal)
- Starts in Phase II (no Phase I needed), 36+48 iter degenerate plateaus
- 31 basic vars past bounds at termination, worst_infeas=144000
- Solver overshoots optimal because x_B values are corrupt
- **Fix:** Bound enforcement after x_B recompute + matrix scaling

### finnis (7.3% — diagnose tool gets 0.2%!)
- Diagnose tool solves nearly correctly (943 iters, 0.2% error)
- Actual solver may differ — investigate solve_lp.c flow vs diagnose.c flow
- **Fix:** Harmonize solve_lp.c and diagnose.c iteration loops

### scsd1 (TIMEOUT → false UNBOUNDED)
- All 77 constraints are equalities, single infeasible slack on row 5
- Every iteration is degenerate (448/448), Phase I obj never decreases from 1.0
- After Bland's rule, RCs blow up to 10^9 → false UNBOUNDED
- DIAG_MISMATCH on row 5 but RHS-dependent fix causes regressions elsewhere
- **Fix:** Forced refactorization during degeneracy, NOT diag_coeff change

### scagr25 (2.2% — diagnose tool gets correct answer!)
- Diagnose tool solves to machine precision (-1.475343e7)
- 4 diag mismatches but converge despite them
- Actual solver may differ from diagnose tool flow
- **Fix:** Same as finnis — investigate solver vs diagnose discrepancy

### recipe (0.76% — diagnose tool gets correct answer!)
- Diagnose tool shows obj=-266.616 matching reference
- 63% degenerate pivots in Phase II but converges
- Discrepancy likely from different refactorization timing in actual solver

## DO NOT

- Re-introduce per-pivot bound snapping in cxf_apply_pivot (causes butterfly-effect regressions)
- Recompute Phase I objective without updating work_obj[] (w-coefficients)
- Claim BFRT row negation is a spec bug (it's not — see MEMORY.md)
- Skip reading this file and `docs/remediation_plan.md`
