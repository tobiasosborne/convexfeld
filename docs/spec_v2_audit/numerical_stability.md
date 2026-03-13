# Spec V2 Audit: Numerical Stability, Tolerances, EXPAND, Recomputation

Auditor: Claude Opus 4.6
Date: 2026-03-13

## Files Reviewed

### Spec files
- `docs/specs-v2/specs/reference/numerical_stability.md`
- `docs/specs-v2/specs/reference/tolerances_constants.md`
- `docs/specs-v2/specs/algorithms/perturbation.md`

### Implementation files
- `include/convexfeld/cxf_types.h` (tolerance #defines)
- `src/simplex/recompute.c` (x_B and objective recomputation)
- `src/simplex/expand.c` (EXPAND Mechanism B)
- `src/simplex/refine.c` (post-solve refinement)
- `src/basis/refactor.c` (refactorization trigger and LU driver)
- `src/basis/lu_factorize.c` (Markowitz LU factorization)
- `src/basis/sparse_elim.c` (sparse Markowitz pivot search)
- `src/simplex/step.c` (simplex iteration engine)
- `src/simplex/step2.c` (variable-side bound propagation)
- `src/simplex/step3.c` (constraint-side bound propagation)
- `src/simplex/ratio_test.c` (Harris two-pass ratio test)
- `src/simplex/perturbation.c` (Mechanism A + orchestration)
- `src/simplex/solve_lp.c` (main solve loop)
- `src/simplex/scaling.c` (Ruiz equilibration)
- `src/error/nan_check.c` (NaN/Inf detection)
- `src/error/pivot_check.c` (pivot validation)
- `src/api/env.c` (default tolerance initialization)

---

## Compliant Items

### Tolerance Values (cxf_types.h)
- **CXF_INFINITY = 1e100** -- matches spec Section 6 (solver infinity).
- **CXF_FEASIBILITY_TOL = 1e-6** -- matches spec Section 1 (primal feasibility tolerance).
- **CXF_OPTIMALITY_TOL = 1e-6** -- matches spec Section 2 (dual/optimality tolerance).
- **CXF_PIVOT_TOL = 1e-9** -- matches spec Section 4 (Harris pivot tolerance).
- **CXF_MIN_PIVOT = 1e-13** -- matches spec Section 4 (minimum pivot threshold).
- **CXF_MARKOWITZ_TOL = 0.0078125** -- matches spec Section 4 (1/128, power-of-two fraction).
- **CXF_BOUND_EQUALITY_TOL = 1e-10** -- matches spec Section 9.
- **CXF_PERTURB_FLOOR = 1e-10** -- matches spec Section 7 (minimum bound range).
- **CXF_PERTURB_CEILING = 1e-6** -- matches spec Section 7 (maximum perturbation magnitude).
- **CXF_LARGE_BOUND_MARKER = 1e20** -- matches spec Section 12.

### Tolerance Hierarchy
- The ordering CXF_BOUND_EQUALITY_TOL (1e-10) < CXF_PIVOT_TOL (1e-9) < CXF_FEASIBILITY_TOL (1e-6) = CXF_OPTIMALITY_TOL (1e-6) is correct per numerical_stability.md Section E.

### Refactorization (refactor.c)
- Adaptive eta count threshold: `min(100, max(50, m/4))` matches spec Section A.
- Residual-triggered adaptive reduction via `thresholds[5]` is implemented.
- Multiple trigger conditions present (eta count, memory limit, iteration interval, FTRAN degradation).

### Residual Monitoring (step.c)
- FTRAN residual checked periodically (`state->iteration % 20 == 0`) when `eta_count > 10`.
- Threshold is `10.0 * env->feasibility_tol` = 10 * eps_feas, matching spec Section A.
- On residual failure, refactorization triggered and adaptive eta limit tightened.

### NaN/Inf Detection (nan_check.c, step.c)
- `cxf_check_nan` and `cxf_is_finite` utility functions present.
- FTRAN result checked for non-finite values in step.c before use.
- On NaN/Inf detection: immediate refactorization + recompute (matching spec Section C).

### Step Length Clamping (step.c)
- `STEP_CLAMP = 1e15` matches spec Section C recommendation.
- Clamping applied before pivot.
- Large step triggers immediate refactorization (post_pivot_updates Phase 9).

### Post-pivot Bound Projection (step.c)
- Phase II basic variables projected to bounds after pivot, matching spec Section C.
- Correctly skipped in Phase I (where violations are tracked by objective).

### x_B Recomputation (recompute.c)
- Correct formula: x_B = B^{-1}(b - N*x_N).
- Nonbasic variables snapped to exact bounds before recomputation.
- Iterative refinement implemented (one round when worst infeasibility > 100 * eps_feas).

### Objective Recomputation (recompute.c)
- Phase II: obj = sum(c_j * x_j) -- correct.
- Phase I: obj = sum of bound violations, with work_obj reset -- correct.

### Optimality Verification (solve_lp.c)
- At ITERATE_OPTIMAL in Phase II: refactorize + recompute_xB + recompute_objective + recompute reduced costs, then verify all RCs satisfy optimality. Matches spec Section F.6.

### Markowitz Pivot Selection (sparse_elim.c)
- Score: `(row_count - 1) * (col_count - 1)` -- correct Markowitz count.
- Threshold pivoting: `|a_ij| >= MARKOWITZ_TOL * col_max[j]` -- correct.
- Relative tie-breaking (Suhl & Suhl): `rel = av / cmax`, prefer higher `rel` on score ties -- correct.

### Growth Factor Monitoring (lu_factorize.c)
- `GROWTH_LIMIT = 1e8` -- within spec range of 1e8 to 1e10.
- Computed as `max_u / max_initial`.
- Sets `basis->numerical_flag = 1` on exceedance.

### EXPAND Perturbation (perturbation.c, expand.c)
- Two-mechanism approach (A: pricing restriction, B: bound widening) matches spec.
- Mechanism A applied first; Mechanism B only when A was already applied and stalling persists.
- Saved vs. working bounds maintained correctly.
- Bound restoration in unperturb correctly copies saved bounds back.
- EXPAND epsilon formula: `eps_base * (1 + |bound_value|) * (1 + hash(i))` matches spec.

### Scaling (scaling.c)
- Ruiz equilibration with up to 10 iterations (RUIZ_MAX_ITERS = 10) -- matches spec Section 8.
- Scale factor clamping: [1e-6, 1e6] matches spec Section 8.
- Scaling effectively disabled via RANGE_THRESHOLD = 1e30 (documented, intentional).

### Default Environment (env.c)
- feasibility_tol and optimality_tol initialized to correct defaults.
- infinity initialized to CXF_INFINITY.

---

## VIOLATIONS

### [V1] CXF_ZERO_TOL mismatch with spec "Numerical zero (tight)"

- **Spec says:** tolerances_constants.md Section 6 defines "Numerical zero (tight)" as **1e-10**, used "for testing whether a bound gap is negligible, whether a variable is effectively fixed, and for other tight numerical comparisons."
- **Code does:** `CXF_ZERO_TOL = 1e-12` (cxf_types.h:210). The comment says "Zero tolerance for numerical comparisons (significant bound change)" but the value is two orders of magnitude smaller than the spec. The spec also defines "Significant bound change" as 1e-12 as a separate constant, so it appears the code conflated two distinct constants: the "numerical zero (tight)" threshold (1e-10) and the "significant bound change" threshold (1e-12).
- **File:** `include/convexfeld/cxf_types.h:210-211`
- **Impact:** CXF_ZERO_TOL is used in recompute.c to skip near-zero values during B*x computation, in pivot_eta.c for eta sparsity filtering, and in scaling.c for coefficient range analysis. Using 1e-12 instead of 1e-10 means slightly more entries are retained (conservative), which is likely benign but does not match the spec.
- **Severity:** Low. The code's value (1e-12) is more conservative (retains more entries).

### [V2] step2.c and step3.c use CXF_PIVOT_TOL (1e-9) instead of CXF_MIN_PIVOT (1e-13) for bound propagation

- **Spec says:** tolerances_constants.md Section 4 specifies the "Minimum pivot threshold" (1e-13) for "bound propagation steps" (step2 and step3). numerical_stability.md Section D: "A secondary, more conservative threshold (approximately 1e-13) is used in bound propagation steps (cxf_simplex_step2 and cxf_simplex_step3)."
- **Code does:** Both step2.c (line 109, 150) and step3.c (line 155) use `CXF_PIVOT_TOL` (1e-9) as the drop threshold for coefficient magnitude.
- **File:** `src/simplex/step2.c:109,150` and `src/simplex/step3.c:155`
- **Impact:** Coefficients with magnitude between 1e-13 and 1e-9 are skipped in bound propagation, potentially missing valid implied-bound tightenings. The spec explicitly specifies 1e-13 here because "bound propagation operations are conservative (they widen rather than tighten bounds on failure) and do not commit to a basis exchange."
- **Severity:** Medium. Could miss bound-tightening opportunities on problems with small coefficients.

### [V3] Multi-level pricing tolerance values deviate from spec

- **Spec says:** tolerances_constants.md Section 4 "Adaptive Pivot Tolerance": Fast phase ~1e-6, Standard phase ~1e-10, Aggressive phase ~1e-9.
- **Code does:** step.c:270-272 computes pricing levels as:
  - Level 0 (Fast): `env->optimality_tol * 10.0` = 1e-5
  - Level 1 (Standard): `env->optimality_tol` = 1e-6
  - Level 2 (Aggressive): `env->optimality_tol * 0.1` = 1e-7
- **File:** `src/simplex/step.c:270-272`
- **Impact:** All three levels differ from spec values. The Fast phase (1e-5) is looser than spec (1e-6). The Standard phase (1e-6) is much looser than spec (1e-10). The Aggressive phase (1e-7) is much looser than spec (1e-9). The Standard fallback, intended as a "very tight tolerance used when the solver encounters difficulties," is actually the same as the default optimality tolerance, providing no tightening at all.
- **Severity:** Medium-High. The standard/fallback phase was designed to be very tight (1e-10) to reject false candidates from reduced cost drift. Using 1e-6 instead may allow numerically unreliable entering candidates during difficulty.

### [V4] No explicit Phase I-to-Phase II refactorization trigger

- **Spec says:** numerical_stability.md Section A item 4: "Certain algorithmic transitions should force a refactorization to ensure a clean numerical state: Transition from Phase I to Phase II of two-phase simplex."
- **Code does:** In solve_lp.c, `cxf_check_phase_one_end` handles the Phase I -> Phase II transition. The function is not shown in the reviewed files, but there is no explicit refactorization call at the transition point in the main loop (lines 195-199). The transition goes through `cxf_check_phase_one_end` which may or may not refactorize internally.
- **File:** `src/simplex/solve_lp.c:195-199`
- **Impact:** Cannot confirm compliance without reading cxf_check_phase_one_end. If it does not refactorize, accumulated Phase I errors carry into Phase II.
- **Severity:** Needs investigation.

### [V5] Missing NaN/Inf detection after objective update

- **Spec says:** numerical_stability.md Section C: "After objective value updates, before comparing with convergence criteria."
- **Code does:** The objective is updated in post_pivot_updates (step.c:498-499) and recompute_objective, but there is no NaN/Inf check on `state->obj_value` after updates.
- **File:** `src/simplex/step.c:498-499`
- **Impact:** A NaN objective would silently propagate through convergence checks. While NaN objective typically implies NaN in x_B or reduced costs (which may be caught at FTRAN time), the spec explicitly requires this checkpoint.
- **Severity:** Low-Medium. Defense-in-depth issue.

### [V6] Missing NaN/Inf detection after step length computation

- **Spec says:** numerical_stability.md Section C: "After step length computation in the ratio test, before applying the pivot."
- **Code does:** ratio_test.c computes theta and returns it. step.c applies stepSize without checking for NaN/Inf. The STEP_CLAMP check (`stepSize > STEP_CLAMP`) would fail silently if stepSize is NaN (NaN comparisons return false).
- **File:** `src/simplex/step.c:603` and `src/simplex/ratio_test.c`
- **Impact:** NaN step would propagate into primal value updates, corrupting x_B.
- **Severity:** Medium. NaN propagation is dangerous.

### [V7] EXPAND epsilon_base clamped to [1e-8, 1e-6] but spec says "1e-6 to 1e-8 scaled from feasibility tolerance"

- **Spec says:** perturbation.md Phase 5: "epsilon_base is typically on the order of 1e-6 to 1e-8 (scaled from the feasibility tolerance)."
- **Code does:** expand.c:124-126 sets `eps_base = feas_tol` then clamps to [1e-8, 1e-6]. With default feas_tol = 1e-6, eps_base = 1e-6 (the maximum of the range).
- **File:** `src/simplex/expand.c:124-126`
- **Impact:** This is technically within spec range but always at the maximum. A more moderate default (e.g., 1e-7 or feas_tol/10) would produce smaller perturbations closer to the middle of the spec range, reducing solution distortion. Not a strict violation.
- **Severity:** Low (borderline compliant).

### [V8] EXPAND activation threshold hardcoded to 100 degenerate pivots, not linked to stalling detection

- **Spec says:** perturbation.md Phase 5: "When stalling persists after pricing restriction (detected by a secondary stalling check or by the outer iteration loop re-entering the perturbation procedure), apply EXPAND-style bound perturbation."
- **Code does:** expand.c:109 uses `state->degenerate_count > 100` as the primary trigger, with a fallback of `state->iteration > 3 * m && state->degenerate_count > 0`.
- **File:** `src/simplex/expand.c:106-117`
- **Impact:** The spec envisions EXPAND triggered by confirmed stalling (basis snapshot comparison showing no progress after Mechanism A), not a raw degenerate pivot counter. The hardcoded 100 may be too high for small problems or too low for large ones.
- **Severity:** Low-Medium. The current heuristic works pragmatically but doesn't match the spec's stalling-detection-driven design.

### [V9] refactor_check does not implement consecutive small pivot trigger

- **Spec says:** numerical_stability.md Section A item 3: "After detecting multiple consecutive small pivots, the solver should trigger an early refactorization regardless of the eta count."
- **Code does:** refactor.c `cxf_refactor_check` checks eta count, memory, iteration interval, and FTRAN degradation. It does not track or check for consecutive small pivots.
- **File:** `src/basis/refactor.c:158-201`
- **Impact:** A sequence of ill-conditioned pivots (pivot elements near 1e-9) can degrade accuracy faster than normal, but refactorization won't be triggered until the eta count threshold or residual check catches it.
- **Severity:** Medium. Missing a spec-required trigger for early refactorization.

### [V10] Missing "stalling detection" and "objective stagnation" refactorization triggers

- **Spec says:** numerical_stability.md Section A item 4: "Detection of stalling or objective stagnation" should force a refactorization.
- **Code does:** Stalling triggers perturbation (perturbation.c) but does not explicitly force refactorization. The refactorization check in refactor.c has no stalling-aware path.
- **File:** `src/basis/refactor.c:158-201`
- **Impact:** When stalling is detected, only perturbation is applied. A refactorization would clear accumulated drift that might be contributing to the stalling.
- **Severity:** Low-Medium.

### [V11] Fixed variable handling does not snap to midpoint

- **Spec says:** numerical_stability.md Section C: "Should be fixed at the midpoint (lb + ub) / 2 when the gap is nonzero but within tolerance."
- **Code does:** No evidence of midpoint snapping for nearly-fixed variables. CXF_VAR_FIXED status is defined (cxf_types.h:169) but the code that assigns it (not in reviewed files) may not compute the midpoint.
- **File:** Not found in reviewed files.
- **Impact:** Variables with tiny but nonzero bound gaps are fixed at one of their bounds rather than the midpoint, which could introduce larger constraint perturbation.
- **Severity:** Low.

### [V12] No compensated (Kahan) summation anywhere

- **Spec says:** numerical_stability.md Section B recommends compensated summation for "Activity bound recomputation during refactorization," "Objective value computation after many pivots," and "Reduced cost recomputation from scratch."
- **Code does:** All summations in recompute.c, step.c, ratio_test.c use naive accumulation. No Kahan summation is implemented anywhere.
- **File:** All computation files.
- **Impact:** Error accumulation in long sums scales as O(n * eps_machine) rather than O(eps_machine). For large problems, this degrades recomputation accuracy.
- **Severity:** Low-Medium. Spec marks this as "Medium" priority. Periodic recomputation partially mitigates.

### [V13] Reduced costs recomputed from scratch only in Phase I; Phase II uses incremental updates

- **Spec says:** numerical_stability.md Section A (Practical Guidance): "After refactorization, the solver should recompute all reduced costs from scratch using the fresh LU factors."
- **Code does:** In post_pivot_updates (step.c:516-517), Phase I always calls `cxf_compute_reduced_costs(state)` (from scratch), but Phase II uses incremental `update_reduced_costs` when BTRAN succeeded. After refactorization (step.c:550), reduced costs ARE recomputed from scratch.
- **File:** `src/simplex/step.c:511-522, 550`
- **Impact:** Compliant at refactorization boundaries. Between refactorizations, Phase II accumulates drift in reduced costs via incremental updates, which is standard practice. Not a violation per se -- the spec requirement is specifically "after refactorization."
- **Severity:** Compliant (no violation).

### [V14] No cancellation detection in pivot_update (activity bound updates)

- **Spec says:** numerical_stability.md Section B: "The pivot operations module (P3.19, cxf_pivot_update) implements a cancellation detection test: after computing result = existing + delta, it checks whether (result - delta) recovers the original value."
- **Code does:** cxf_pivot_update is called from step2.c and step3.c tighten_bound functions. The implementation of cxf_pivot_update was not in the reviewed files, so this cannot be confirmed.
- **File:** Implementation not reviewed (presumably in pivot_update.c).
- **Impact:** Needs investigation -- if cxf_pivot_update does not implement cancellation detection, activity bounds could be corrupted on ill-conditioned problems.
- **Severity:** Needs investigation.

---

## Missing Procedures

### [M1] No input data NaN/Inf validation during model loading
- **Spec says:** numerical_stability.md Section C: "On input data during model loading, to reject models with NaN or Inf coefficients."
- **Status:** Not found in reviewed files. May exist in model loading code (not in scope).

### [M2] No adaptive Markowitz tolerance increase on high growth factor
- **Spec says:** numerical_stability.md Section D: "If the growth factor is unacceptably large, the solver may: Increase the Markowitz pivot tolerance u."
- **Status:** Growth factor is monitored and flagged (`basis->numerical_flag = 1`) but no action is taken to increase the Markowitz tolerance. The flag appears to be informational only.
- **File:** `src/basis/lu_factorize.c:171-172`

### [M3] No "recovery from numerical difficulty" refactorization trigger
- **Spec says:** numerical_stability.md Section A item 4: "Recovery from a numerical difficulty (rejected pivot column, NaN detection)."
- **Status:** NaN in FTRAN result does trigger refactorization (step.c:381-384). However, rejected pivot columns (CXF_NUMERIC return from step.c:440) do NOT trigger refactorization -- they simply return the error.

---

## Notes

1. **CXF_ZERO_TOL dual purpose:** The code uses a single constant (1e-12) for what the spec defines as two separate thresholds: "numerical zero (tight)" at 1e-10 and "significant bound change" at 1e-12. The current value matches the latter but not the former.

2. **Scaling is intentionally disabled:** RANGE_THRESHOLD = 1e30 effectively disables scaling. This is documented in the code as intentional, pending multi-candidate pricing support.

3. **Iterative refinement in recompute.c:** The implementation includes one round of iterative refinement when worst infeasibility exceeds 100 * eps_feas. This is not explicitly specified but is consistent with the spec's emphasis on numerical accuracy. The threshold of 100 * eps_feas = 1e-4 is reasonable.

4. **EXPAND Mechanism B Phase I objective recomputation:** expand.c lines 165-176 manually recompute the Phase I objective after bound widening but do NOT reset work_obj[] (the w-coefficients). This could lead to stale w-coefficients until the next full recompute_objective call. Compare with recompute.c:187-203 which does reset work_obj[].

5. **Bland's rule activation thresholds:** step.c:595 activates Bland's rule after 50 consecutive degenerate pivots. solve_lp.c:167 activates after `3 * m` degenerate pivots when perturbation has been tried. These are heuristic choices not explicitly specified in the V2 spec but consistent with the spec's mention of Bland's rule as a "last resort."

6. **env refactor_interval default:** env.c sets `DEFAULT_REFACTOR_INTERVAL = 50`, which is separate from the adaptive eta count threshold in refactor_check (min(100, max(50, m/4))). The refactor_interval is checked as an iteration-based trigger, not an eta-count trigger. Both are valid per the spec.
