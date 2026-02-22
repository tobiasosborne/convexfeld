# Scaling Implementation Report

## Status: Infrastructure complete, disabled pending solver robustness

## What Was Implemented

Full row+column Ruiz equilibration per `matrix_finalization.md` Strategy 3:

1. **FTRAN/BTRAN diagonal fallback** (`ftran.c:114`, `btran.c:95`): changed `*= diag_coeff` to `/= diag_coeff`. The initial diagonal basis B_0 = diag(d) has inverse diag(1/d). For ±1 values 1/d = d so multiplication worked. For scaled values D_r*(±1), division is required. The LU path already divides (ftran.c:62 divides by U_diag[k]).

2. **Ruiz iteration** (`scaling.c`): alternating row/column infinity-norm equilibration, 10 iterations max, clamped to [1e-6, 1e6]. Row norms include implicit slack coefficient (diag_coeff) to prevent over-scaling rows where slack dominates.

3. **Scaling application**: A'[i,j] = A[i,j] * row_scale[i] * col_scale[j]; rhs[i] *= row_scale[i]; diag_coeff[i] = row_scale[i] * sense_sign; obj[j] *= col_scale[j]; lb[j] /= col_scale[j]; ub[j] /= col_scale[j]. Slack bounds unchanged.

4. **Phase I integration**: `cxf_setup_phase_one` sets `diag_coeff[i] = row_scale[i] * (±1)`. Phase II transition re-scales restored objective by col_scale[j].

5. **Unscaling**: After final accuracy pass, before extraction: x[j] *= col_scale[j] for structural vars; restore original bounds from model.

## Mathematical Verification

- Objective invariant: c_s^T y = sum(c[j]*D_c[j] * x[j]/D_c[j]) = c^T x ✓
- Slack values invariant: s = (D_r*b - D_r*A*D_c*y) / (D_r*diag) = (b - A*x) / diag ✓
- FTRAN/BTRAN division: B_0^{-1} = diag(1/d), applied as result[i] /= d[i] ✓
- LU factorization: U_diag[i] = diag_coeff[i], LU solve divides by U_diag ✓
- Column extraction: uses diag_coeff directly as column coefficient ✓
- Reduced costs: dj = c_j - pi * col_j, uses diag_coeff for slack columns ✓

## Empirical Results: Scaling Makes Everything Worse

| Problem | Without Scaling | With Scaling | Change |
|---------|----------------|--------------|--------|
| blend | PASS | UNBOUNDED | regression |
| stair | PASS | INFEASIBLE | regression |
| boeing1 | 18% error | 41% error | worse |
| grow7 | 12.5% error | 15% error | worse |
| bore3d | 7.5% error | TIMEOUT | worse |
| israel | PASS | PASS | no change |
| share2b | PASS | PASS | no change |
| afiro | PASS | PASS | no change |

Column-only scaling (no row scaling) is equally bad — boeing1 goes to 80% error.

## Detailed Failure Analysis

### blend → UNBOUNDED (iter 99, Phase II)
- Entering var 55 has dj = -1.248 (genuinely attractive)
- Pivot column has 0 positive elements, 3 negative, 71 zero
- 70 of 74 basic variables have ub = +inf (slack variables)
- No blocker exists → ratio test returns UNBOUNDED
- Without scaling, blend cycles degenerately (obj=0 for 200+ iters) but doesn't hit UNBOUNDED
- Scaling changes the iteration path to reach a basis where this column is all-negative

### stair → INFEASIBLE
- Post-scale coefficient ratio: 194,000 (scaling barely helped)
- Phase I fails to find feasibility in scaled space

### boeing1 → worse error (18% → 41%)
- Post-scale ratio: 3,264 (good reduction from 274,000)
- But the changed iteration path leads to a worse optimum

## Diagnosis: Why Scaling Hurts

The scaling math is correct. The problem is downstream:

1. **Ratio test is fragile**: When scaling changes which basis the solver visits, it can encounter bases where the pivot column has no positive elements for the entering direction. The ratio test returns UNBOUNDED instead of trying an alternative candidate or refactorizing.

2. **Anti-degeneracy is insufficient**: blend has 200+ degenerate pivots without scaling. Scaling changes which pivots are degenerate. The EXPAND perturbation and Bland's rule aren't robust enough to navigate the scaled degenerate landscape.

3. **Phase I convergence is brittle**: stair's Phase I fails in scaled space because the near-zero scaled coefficients (ratio 194K after scaling) create near-singular bases that prevent Phase I from reducing infeasibility.

4. **No iteration path recovery**: When scaling leads to a bad basis, the solver has no mechanism to detect "this basis is worse than what I'd have without scaling" and recover.

## Question for Spec Agent

The v2 spec describes matrix scaling in `matrix_finalization.md` (Ruiz equilibration, Phase 5 bound propagation). The spec's solver presumably handles scaled problems without the regressions we observe. Specifically:

**How does the spec's simplex iteration handle the following scenarios that arise after scaling?**

1. **UNBOUNDED from ratio test after scaling changes iteration path**: In `simplex_iteration.md` / `harris_ratio_test.md`, when the ratio test finds zero valid candidates (all pivot column elements are zero or wrong-sign for the entering direction), does the spec's solver:
   - Return UNBOUNDED immediately (our behavior)?
   - Try the next pricing candidate?
   - Force a refactorization and retry?
   - Apply perturbation to create artificial blockers?

2. **Phase I failure on scaled problems**: In `two_phase_method.md` / `simplex_phases.md`, when Phase I stalls with nonzero infeasibility after scaling changes the basis path, does the spec's solver:
   - Declare INFEASIBLE immediately?
   - Have a tolerance-scaling mechanism that adapts to the scaled coefficient range?
   - Use a different Phase I strategy for scaled problems (e.g., big-M)?

3. **Pricing after scaling**: In `pricing_core.md`, does the pricing mechanism account for the fact that reduced costs in scaled space have different magnitudes than in original space? Specifically, is the pricing tolerance (optimality tolerance) adjusted based on the scaling factors?

4. **Refactorization frequency after scaling**: In `basis_operations.md`, does the spec increase refactorization frequency for scaled problems (since the scaled basis may have worse condition numbers than the unscaled one)?

5. **Ratio test tolerance**: In `harris_ratio_test.md`, is the Harris tolerance (feasibility tolerance for the relaxed ratio test) adjusted based on the scaling factors? A fixed absolute tolerance of 1e-6 may be too tight or too loose after scaling.

The answers to these questions would tell us whether the spec's solver has explicit scaling-aware mechanisms, or whether scaling "just works" because the spec's solver is more robust in general (better anti-cycling, better ratio test fallbacks, better Phase I).

---

## Spec Agent Analysis (2026-02-22)

### Summary

The scaling math is correct. The regressions are caused by **pre-existing solver robustness issues** that scaling merely exposes by changing the iteration path. The reference solver handles scaling not because it has special scaling-aware mechanisms, but because it is more robust in general.

### Q1: UNBOUNDED from ratio test after scaling

**The reference solver does NOT immediately return UNBOUNDED from the standard ratio test.** When the ratio test finds no valid ratio (both step bounds are infinity), the solver enters a "skip or bound flip" branch — it does NOT return UNBOUNDED. The UNBOUNDED return (code 5) only occurs in two specific places:

1. **Free variable handling**: For superbasic/free variables (status=0) with both bounds infinite and a significant reduced cost. This is genuine unboundedness.

2. **Special pivot path**: For variables processed by `cxf_pivot_special`, where unboundedness detection is controlled by a `specialMode` flag on the solver state. **Crucially, when `specialMode != 0`, unboundedness detection is SUPPRESSED** (the function skips to `execute_action` instead of returning UNBOUNDED). This flag is set during Phase I to prevent false unboundedness reports.

**What blend's solver is doing wrong:** It returns UNBOUNDED from the ratio test when it finds no positive pivot column elements. The reference solver would instead:
- Skip that entering variable and try the next pricing candidate
- Or, if the pivot column is numerically zero (all elements below pivot tolerance), reject the column and reprice (per `harris_ratio_test.md` lines 226-227: "the entering variable is rejected. Control returns to the pricing system to select a different entering variable")

**Fix:** When the ratio test finds no blocker, do NOT return UNBOUNDED. Instead:
1. If the entering variable has finite bounds, flip it to its opposite bound (BOUND_FLIP_ONLY status)
2. If the entering column has no elements above pivot tolerance, reject the column and reprice
3. Only return UNBOUNDED if this is a genuinely free variable with a significant reduced cost AND `specialMode` is not set

### Q2: Phase I failure on scaled problems

**The reference solver does NOT adjust Phase I strategy for scaled problems.** Scaling is transparent to the simplex — it happens before `cxf_simplex_init` and the solver operates on the already-scaled problem. What makes the reference solver survive is:

1. **Better refactorization** — the spec prescribes adaptive refactorization intervals and residual monitoring. Scaled problems with large coefficient ratios (like stair's 194,000) produce faster numerical drift, requiring more frequent refactorization.

2. **EXPAND bound widening** (Mechanism B from P2.6) — breaks leaving-side degeneracy that scaling can exacerbate.

3. **Tight-tolerance Phase I retry** — before declaring INFEASIBLE, try one more pricing pass at a tighter tolerance (`two_phase_method.md`: "the solver may attempt additional iterations with tighter tolerances before declaring infeasibility").

**Fix:** The stair failure (Phase I in scaled space with ratio 194K) is the same root cause as the unscaled numerical maintenance issues — insufficient refactorization. Fix refactorization first, then re-test scaling.

### Q3: Pricing tolerance after scaling

**The reference solver does NOT adjust pricing tolerances based on scaling factors.** The optimality tolerance is an absolute tolerance applied in the scaled space. This is correct because after proper equilibration, the coefficient range should be reduced enough that fixed tolerances work. If scaling doesn't reduce the range sufficiently (stair: 194K post-scale), the problem is that scaling didn't help much, not that tolerances need adjusting.

**No change needed.** If scaling barely reduces the coefficient range, disable scaling for that problem (which is what the spec's auto-selection logic does — it checks whether scaling helps before applying it).

### Q4: Refactorization frequency after scaling

**The reference solver does NOT explicitly increase refactorization frequency for scaled problems.** But the spec's **residual monitoring trigger** (`numerical_stability.md` lines 29-35) handles this automatically: if scaling causes faster numerical drift, the residual `||a - B*x||` will exceed the threshold sooner, triggering earlier refactorization. This is why residual monitoring is described as "the most robust trigger" — it adapts to whatever the problem needs.

**Fix:** Implement residual monitoring. This is the single most impactful thing to do. It is already prescribed in the spec and it automatically handles scaling, ill-conditioning, and all other sources of numerical drift.

### Q5: Harris tolerance after scaling

**The reference solver does NOT adjust the Harris tolerance based on scaling.** The feasibility tolerance is absolute and applied in the scaled space. After Ruiz equilibration, the coefficient norms should be near 1.0, so a fixed tolerance is appropriate.

**No change needed.** This is working as designed.

### Root Cause: Scaling Exposes Missing Robustness

The diagnosis in "Why Scaling Hurts" above is correct: "The scaling math is correct. The problem is downstream." Specifically, the solver is missing these robustness features that the spec prescribes:

| Missing Robustness Feature | Spec Reference | Impact |
|---|---|---|
| Residual-based refactorization trigger | `numerical_stability.md` Section A, condition 2 | Prevents numerical drift from causing UNBOUNDED/INFEASIBLE |
| Adaptive refactorization interval | `numerical_stability.md` Section A, practical guidance | Shorter intervals for ill-conditioned problems |
| Column rejection on ratio test failure | `harris_ratio_test.md` Stage 2, "No candidates" case | Prevents false UNBOUNDED |
| EXPAND bound widening | `perturbation.md` Phase 5 (Mechanism B) | Prevents Phase I stalling from leaving-side degeneracy |
| From-scratch recomputation at refact | `revised_simplex.md` Step 9, items 4-5 | Corrects accumulated objective/reduced cost drift |

### Recommended Action

1. **Implement residual monitoring** (`numerical_stability.md` Section A, condition 2) — after FTRAN computes x = B^{-1} a, periodically compute r = a - B*x and trigger refactorization if `||r||_inf > 10 * epsilon_feas`. This single change would likely fix blend, stair, and boeing1 under scaling by triggering refactorization before numerical quality degrades enough to cause UNBOUNDED/INFEASIBLE.

2. **Fix the ratio test UNBOUNDED handling** — reject columns with no valid pivot elements and reprice, instead of returning UNBOUNDED. This is the direct fix for blend.

3. **Disable scaling** until items 1-2 are done. Re-enable and re-test afterward. The remaining failures will fall into the same "numerical maintenance" bucket as the unscaled failures.

Scaling is infrastructure that amplifies whatever the solver's baseline robustness is. Fix the baseline first.
