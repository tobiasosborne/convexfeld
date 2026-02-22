# Agent Handoff

*Last updated: 2026-02-22*

---

## STATUS: 18/36 Netlib pass. Phase I rewritten. 40/40 unit tests pass. 15 failures are implementation gaps.

### Scorecard

**PASS (18):** afiro, sc50b, sc105, share2b, israel, adlittle, blend, lotfi, beaconfd, stocfor1, ship04l, scfxm1, standata, standgub, standmps, scagr7, sctap1, brandy

**FAIL (18):** See root cause table below.

---

## What Needs Implementing (Priority Order)

All 15 non-timeout failures are **implementation gaps, not spec deficiencies.** The specs prescribe every mechanism needed. Read the specific spec sections cited below.

### 1. Numerical Maintenance — fixes 10 instances

**Instances:** scorpion (0.017%), kb2 (0.024%), stair (0.04%), etamacro (0.07%), finnis (0.2%), grow7 (0.56%), recipe (0.76%), boeing1 (176%), e226 (61%), bore3d (7.5%)

**Root cause:** Accumulated floating-point error over hundreds of pivots. The solver doesn't refactorize often enough and doesn't recompute values from scratch.

**What the spec says to implement:**

**(a) Adaptive refactorization interval** — `docs/specs-v2/specs/reference/numerical_stability.md` lines 25-49
- Use `min(100, max(50, m/4))` for eta count threshold (currently fixed at 100)
- Add residual monitoring: after FTRAN, compute `r = a - B*x`, trigger refactorization if `||r||_inf > 10 * epsilon_feas`
- Adaptive: if residual triggers frequently, reduce the eta count threshold
- **File:** `src/simplex/step.c` Phase 9 (refactorization check), currently at line ~622

**(b) Recompute x_B from scratch at refactorization** — `revised_simplex.md` line 320 (Step 9.4)
- After LU refactorization: `x_B = B^{-1} b` computed fresh, replacing the incrementally-maintained values
- **File:** wherever `cxf_solver_refactor()` is called — add `x_B` recomputation after it

**(c) Recompute objective from scratch at refactorization** — `numerical_stability.md` line 47
- After refactorization: `obj = c^T x` from scratch, not trusting incremental `obj += dj * step`
- Also at OPTIMAL: verify objective from original coefficients (`solve_lp_core.md` line 436)
- **File:** `src/simplex/step.c` after refactorization, and `src/simplex/solve_lp.c` before returning

### 2. UNBOUNDED Regression — fixes 2 instances

**Instances:** scsd1, scagr25

**Root cause:** scagr25 was previously PASS. This is a regression from recent changes (EXPAND bound widening or ratio test bound-crossing guards). Standard debugging.

**Action:** Run diagnostic tool on both. Determine if UNBOUNDED occurs in Phase I or Phase II. If Phase I, the ratio test bound-crossing guards may be mis-triggering. If Phase II, the EXPAND widening may be corrupting bounds. Bisect to the introducing commit if needed.

**Files:** `src/simplex/ratio_test.c` (bound-crossing guards), `src/simplex/perturbation.c` (EXPAND widening)

### 3. Phase I Pricing Convergence — fixes 2 instances

**Instances:** boeing2 (Phase I obj=206, 97% reduced), capri (Phase I obj=12, 99.96% reduced)

**Root cause:** Phase I reaches near-feasibility but pricing exhausts all candidates at tolerance levels 0/1/2. The residual infeasibility is small but nonzero.

**What the spec says:** `two_phase_method.md` line 142: "If the Phase I objective is positive but very small (below a multiple of the feasibility tolerance), the solver may attempt additional iterations with tighter tolerances before declaring infeasibility."

**Action:** In `cxf_check_phase_one_end` (`src/simplex/phase_loop.c`), before returning `CXF_INFEASIBLE`: if Phase I objective < 100 * feasibility_tol, try one more pricing pass with tolerance = 0.01 * optimality_tol. This is ~5 lines.

### 4. RANGES Parsing — fixes 1 instance

**Instance:** forplan

**Root cause:** MPS parser recognizes `RANGES` as a section header but has no handler. Lines in RANGES section are silently ignored.

**Action:** Implement `parse_ranges_line()` in `src/api/mps_parse.c`. MPS RANGES semantics:
- For `L` row with RHS b and range r: becomes `b - |r| <= a'x <= b`
- For `G` row with RHS b and range r: becomes `b <= a'x <= b + |r|`
- For `E` row with RHS b and range r: depends on sign of r

### 5. Performance / Sparse LU — fixes 3 timeout instances

**Instances:** bandm, tuff, vtp.base (all timeout at 10s)

**Root cause:** Dense LU factorization is O(m^3). The Markowitz ordering is implemented in `lu_factorize.c` but forward/back substitution in `ftran.c`/`btran.c` may still be dense.

**Action:** Profile to confirm bottleneck is LU. If so, implement sparse forward/back substitution using the CSC L/U factors that Markowitz already produces.

---

## What Was Done This Session

1. **Architectural audit** — 14 source files vs 17 v2 spec documents
2. **BFRT analysis corrected** — NOT a spec bug. Implementation was incomplete (missing factorization update after row negation). Specs are internally consistent.
3. **Phase I rewritten** — explicit artificials → implicit bound-violation per `two_phase_method.md`. Removed `art_coeff`, arrays shrunk from n+2m to n+m. 15 files changed.
4. **Free variable entering bug fixed** — `x = lb + step` → `x = current_x + step`
5. **EXPAND bound widening added** — Mechanism B for leaving-side degeneracy
6. **10 stale beads issues closed** — outdated understanding cleaned up
7. **40/40 unit tests pass**

## Key Files

| File | Purpose |
|------|---------|
| `src/simplex/step.c` | Iteration engine — Phase I w-update, refactorization check |
| `src/simplex/phase_one.c` | Phase I setup + transition (bound-violation approach) |
| `src/simplex/ratio_test.c` | Harris ratio test with Phase I bound-crossing guards |
| `src/simplex/perturbation.c` | EXPAND widening (Mechanism B) + candidate removal (A) |
| `src/simplex/reduced_costs.c` | Full RC recomputation |
| `src/simplex/phase_loop.c` | Phase I termination + transition orchestration |
| `src/simplex/solve_lp.c` | Top-level solve flow |
| `src/basis/lu_factorize.c` | LU factorization (Markowitz) |
| `src/basis/refactor.c` | Refactorization trigger |
| `tools/diagnose.c` | Diagnostic harness for tracing iterations |
| `docs/architecture_contract_map.md` | Component interface contracts |

## DO NOT
- Re-introduce explicit artificial variables (spec says don't)
- Claim BFRT row negation is a spec bug (it's not — implementation was incomplete)
- Run full Netlib suite (use targeted `--filter` or diagnostic tool)
- Skip reading this file and `docs/remediation_plan.md`
