# Implementation Audit: ConvexFeld vs Cleanroom Spec

**Date:** 2026-02-22
**Scope:** Five subsystems compared across ~30 source files against analyzed reference implementation and v2 specs
**Method:** Parallel subagent investigation of simplex step/iteration, basis/LU/refactorization, pricing system, Phase I/solve driver, and ratio test/pivot operations

---

## Executive Summary

15 of 36 Netlib instances fail. This audit found **14 significant deviations** between the convexfeld implementation and the cleanroom spec. Of these:
- **12 are implementation gaps** (the spec already describes the correct behavior)
- **3 warrant minor spec improvements** (the spec is correct but could be more prescriptive)

The single highest-impact finding: **Kahan-stable addition is entirely absent from the codebase**, and `cxf_pivot_update` has an API design that prevents correct implementation. Together with a stub `cxf_pivot_bound`, these two issues explain the 10 "numerical drift" failures.

---

## CRITICAL — Directly Causing Observed Failures

### C1. No Kahan-stable addition anywhere in the codebase

**Location:** `src/simplex/pivot_update.c:40-50`

**Problem:** All activity bound updates use plain `+=`. The reference implementation uses a cancellation-detecting `stable_add` that applies conservative rounding multipliers (`0.999999` / `1.000001`) when floating-point cancellation is detected. ConvexFeld also lacks `negUnbdCount`/`posUnbdCount` arrays for tracking per-constraint infinite-bound variable counts.

**Consequence:** `min_activity` and `max_activity` accumulate floating-point error on problems with large coefficient magnitudes where bound contributions nearly cancel. This causes incorrect bound propagation, which cascades into objective drift.

**Impact:** All 10 "numerical drift" failures (scorpion, kb2, stair, etamacro, finnis, grow7, recipe, boeing1, e226, bore3d).

**Spec status:** Covered explicitly in `numerical_stability.md` Section B ("Cancellation Detection and Mitigation") and in `pivot_operations.md` for `cxf_pivot_update`. **Not a spec gap.**

---

### C2. `cxf_pivot_bound` is a stub

**Location:** `src/simplex/pivot_special.c:54-105`

**Problem:** The function only updates the objective and sets `lb=ub`. It is missing:
- **Eta vector creation** — breaks the PFI chain; basis factorization cannot be reconstructed after variable fixings
- **Activity bound propagation** — `min_activity`/`max_activity` become stale after every call
- **CSC/CSR matrix entry invalidation** — fixed variables remain "active" in the constraint matrix
- **Quadratic objective terms** — Q-matrix contributions ignored
- **Pricing notification** — pricing subsystem not informed of the variable change

Additionally, there is a **status assignment bug** at line 97: after setting `work_lb[var] = new_value` and `work_ub[var] = new_value`, the comparison `fabs(new_value - work_lb[var])` is always exactly 0, so variables always get `AT_LOWER` regardless of whether they were moved to their upper bound.

**Consequence:** Every call to `cxf_pivot_bound` (from step2/step3 when both bounds tighten, from `cxf_pivot_special` for bound flips) silently corrupts the basis representation.

**Spec status:** Fully specified in `pivot_operations.md` (7 behavioral phases). **Not a spec gap.**

---

### C3. `cxf_pivot_update` API prevents correct implementation

**Location:** `src/simplex/pivot_update.c:24`

**Problem:** Function signature is `(state, var, delta, is_lb)` — takes only a delta. The reference signature is `(state, col, oldLB, newLB, oldUB, newUB, infinity)` — takes old and new bounds for both sides. The "remove old contribution, add new contribution" pattern required for Kahan-stable updates is impossible with the delta-only interface. The three-case dispatch (both bounds changed / lower only / upper only) and the infinity-threshold transitions (finite-to-infinite, infinite-to-finite) also require old+new values.

**Consequence:** Even if Kahan-stable addition were added, the API doesn't provide enough information to implement it correctly.

**Spec status:** The spec describes the old/new bound interface in `pivot_operations.md`. **Not a spec gap — wrong API design.**

---

### C4. `cxf_pivot_special` missing Phase I unboundedness suppression

**Location:** `src/simplex/pivot_special.c:129-191`

**Problem:** Three mechanisms are absent:
1. **No `specialMode` check.** The reference reads a mode flag from the solver state and suppresses unboundedness detection when Phase I is active (where unboundedness of the auxiliary problem doesn't imply unboundedness of the original). ConvexFeld has no equivalent flag or check.
2. **No equality constraint column scan.** The reference scans the variable's CSC column to detect equality constraints; if any equality is found, the function returns SUCCESS immediately (the variable cannot be freely moved without violating the equality). ConvexFeld does not scan the column at all.
3. **No special flag validation.** The reference checks variable flags and validates SOS/indicator constraint compatibility. ConvexFeld has no equivalent.

**Consequence:** Spurious UNBOUNDED during Phase I. Variables in equality constraints can be incorrectly moved, violating feasibility.

**Impact:** Likely cause of scsd1/scagr25 UNBOUNDED failures.

**Spec status:** Described in `pivot_operations.md` line 247: "A special mode flag on the solver state can disable unboundedness detection; this is used during Phase I of two-phase simplex." **Not a spec gap.**

---

## HIGH — Causing Correctness Issues

### H1. Harris ratio test tolerance band is 10x too tight

**Location:** `src/simplex/ratio_test.c:152`

**Problem:** Pass 2 threshold is `minRatio + feasTol` where `feasTol = 1e-6`. The reference uses approximately `10 * feasTol` for the Harris band. A tighter band admits fewer tied candidates in pass 2, reducing the pool from which the largest pivot element is selected, leading to smaller (less stable) pivot elements.

**Impact:** Contributes to numerical drift on all problems, especially near-degenerate ones.

**Spec status:** `harris_ratio_test.md` line 98 says `(slack_i + epsilon) / |d_i|` where "epsilon is the feasibility tolerance." **Minor spec improvement possible** — clarify that epsilon should be a multiple (e.g., 10x) of feasibility tolerance for numerical stability, not the raw tolerance.

---

### H2. step2/step3 missing two-stage infeasibility confirmation

**Location:** `src/simplex/phase_steps.c:82` (step2), `src/simplex/phase_steps.c:187-194` (step3)

**Problem:** ConvexFeld returns `CXF_INFEASIBLE` immediately on bound violation (`lb > ub + tol` in step2, `min_act > tol` in step3). The reference implements a two-stage procedure: the initial violation is cross-checked against dual activity bounds (`dual_ub`/`dual_lb`) before confirming infeasibility. If confirmation fails, the candidate entry is restored and processing continues.

**Consequence:** False infeasibility on near-degenerate problems where numerical noise causes temporary bound violations that would self-correct after refactorization.

**Impact:** boeing2, capri (false INFEASIBLE).

**Spec status:** Described in `simplex_iteration.md` "Two-Stage Infeasibility Detection" section. **Not a spec gap.**

---

### H3. Inner loop convergence criterion is wrong

**Location:** `src/simplex/solve_lp.c:246-257`, `src/simplex/basis_stub.c:66-75`

**Problem:** Two deviations:
1. `cxf_basis_diff` returns a dimension-scaled value `(delta_iter + delta_rows + delta_cols) / max(n, m, 1)`. The reference returns a raw weighted score WITHOUT dimension scaling. Dimension scaling means large problems appear to converge faster (lower diff per variable).
2. The convergence threshold uses `CONVERGENCE_BASE / (1.0 + round)` with no dead zone. The reference uses `max(0, inner_iter - 5) * threshold` — a formula with a 5-iteration dead zone before convergence is tested, and increasing tolerance as stability grows.

**Consequence:** Premature inner loop termination on large problems; insufficient iteration on small problems.

**Spec status:** The stall detection formulas are described in `simplex_iteration.md`. The basis diff threshold is less precisely specified. **Minor spec improvement possible** — add the explicit dead-zone formula.

---

### H4. Phase I w-coefficients not updated per pivot

**Location:** `src/simplex/phase_one.c:171-193`

**Problem:** `cxf_setup_phase_one` sets `work_obj[bv] = -1/+1/0` based on bound violation at Phase I initialization. Between pivots, entering/leaving variables change which variables are basic, but `work_obj` is not dynamically updated. Stale coefficients mean pricing computes incorrect Phase I reduced costs, selecting wrong entering variables.

**Impact:** Phase I convergence failures — pricing uses stale Phase I reduced costs, preventing the solver from reaching feasibility.

**Spec status:** `two_phase_method.md` line 114 says: "These coefficients are updated dynamically as the basis changes: when a basic variable becomes feasible, its Phase I coefficient changes from +/-1 to 0. When a newly basic variable violates a bound, its coefficient changes from 0 to +/-1." **The spec is clear; the implementation doesn't follow it.**

---

## MEDIUM — Performance and Robustness Issues

### M1. BFRT completely disabled

**Location:** `src/simplex/step.c:546-557`

`num_flips` is always 0. The comment explains: row negation was implemented without updating the LU/eta factorization, corrupting FTRAN/BTRAN. Every iteration stops at the first blocking basic variable rather than collecting bound-flippable variables and executing a long step. On highly bounded problems this produces 2-5x more iterations.

**Spec status:** Fully described in `harris_ratio_test.md` Stage 3. **Not a spec gap.**

---

### M2. Dense LU working matrix

**Location:** `src/basis/lu_factorize.c:293`

`calloc(m * m, sizeof(double))` — O(m^2) memory regardless of sparsity. Markowitz ordering is correctly implemented, but the working matrix is dense. Causes timeouts on bandm (m=305), tuff (m=333), vtp.base (m=198).

**Spec status:** Spec says "sparse Gaussian elimination." **Not a spec gap.**

---

### M3. Basis snapshot/diff structural mismatch

**Location:** `src/basis/snapshot.c:34-79`

ConvexFeld copies the full `basic_vars[m]` + `var_status[n]` arrays (O(m+n) time and space). The reference captures 17 integer progress counters (O(1)). `cxf_basis_diff` returns a raw element-count diff divided by `max(n,m)`. The reference returns a weighted, multi-dimensional normalized score using separate denominators per category.

These produce different convergence signals and different stall detection behavior.

**Spec status:** The snapshot content and diff formula are not precisely specified. **Minor spec improvement possible** — specify that the snapshot captures progress counters, not the full basis, and provide the weighted-score formula.

---

### M4. V1 pricing weight update drops all nonbasic updates

**Location:** `src/pricing/update.c:60-79`

The V1 `cxf_pricing_update` suppresses all nonbasic steepest-edge weight updates with `(void)gamma_entering`. Only the entering variable's weight is reset to 1.0; all other `gamma_j` stay stale. If the V1 path is ever called after a pivot, all steepest edge weights become incorrect, degrading pricing quality.

**Spec status:** **Not a spec gap — code bug.** The V2 path (`cxf_pricing_update_weights`) is correct.

---

### M5. Pricing level initializes to 1, not 0

**Location:** `src/pricing/context.c:41`, `src/pricing/init.c:116`

Both set `current_level = 1` at construction. The reference starts at level 0 (the O(1) fast path). Starting at level 1 bypasses the fast path on the first pricing call, wasting time on an unnecessary expanded search.

**Spec status:** **Not a spec gap.**

---

### M6. `cxf_simplex_refine` doesn't create eta records

**Location:** `src/simplex/refine.c:51-63`

Only snaps nonbasic variable values to bounds. Does not create eta vectors to maintain PFI chain consistency. Also, the final accuracy pass (`cxf_recompute_xB`) is skipped when termination status is ITERATION_LIMIT or TIME_LIMIT (`solve_lp.c:289-295`), potentially leaving `work_x` inconsistent with the snapped nonbasic values.

**Spec status:** Described in `simplex_phases.md` (cxf_simplex_refine creates Variant 2 eta records). **Not a spec gap.**

---

### M7. Missing primal crash second pass

**Location:** `src/simplex/crash.c` (entire file)

The reference has a second crash pass (`simplex_crash_primal`) that further improves the initial basis quality when iteration limits allow. ConvexFeld has only the first pass. The second pass is not explicitly named in the spec's function map.

**Spec status:** The spec references the crash in `solve_lp_core.md` Phase 4: "For certain problem types and modes, an additional primal crash variant is also invoked." **Minor spec gap** — the function is referenced but not individually specified.

---

### M8. `cxf_fix_variables_at_bounds` is a stub

**Location:** `src/basis/refactor.c:43-51`

The function only clears the eta list. The reference equivalent is a 953-line function that performs constraint-based candidate identification, ratio-test-sorted variable selection, PWL breakpoint traversal, eta record creation, Q-matrix neighbor updates, matrix sparsity maintenance, activity bound updates, and pricing state invalidation. This is a crossover-specific optimization that is deferred until crossover is implemented.

**Spec status:** Described in `basis_operations.md`. **Known gap, deferred by design.**

---

## Spec Improvements Needed

Only 3 items warrant spec changes:

| Item | Spec File | Recommended Change |
|------|-----------|-------------------|
| **H1** | `harris_ratio_test.md` | Clarify that epsilon in the pass 2 threshold formula should be a multiple of the feasibility tolerance (e.g., `10 * epsilon_feas`) for numerical stability, not the raw tolerance value. Add: "Using the raw feasibility tolerance produces a band that is too narrow to provide effective tie-breaking on near-degenerate problems." |
| **H3** | `solve_lp_core.md` or `simplex_iteration.md` | Add the explicit inner loop convergence formula: `exit inner loop when cxf_basis_diff(state, snapshot) <= max(0, inner_iter - 5) * DIFF_THRESHOLD`. Document that the 5-iteration dead zone allows the solver to establish its working basis before convergence is tested. Note that basis_diff should NOT be dimension-scaled (the threshold already accounts for problem size). |
| **M3** | `basis_operations.md` | Specify that `cxf_progress_snapshot` captures a fixed-size vector of progress counters (iteration count, pivot count, removed rows/cols, inequality-to-equality conversions, propagation counts — approximately 17 integers), NOT the full basis column array. Specify that `cxf_basis_diff` returns a weighted normalized score: `D = sum_i (delta_i * w_i) / N_i` where `N_i` is a category-specific denominator (e.g., `numVars - removedCols` for column metrics, `numConstrs - removedRows` for row metrics). |

---

## Priority Order for Fixes

| Priority | Fix | What It Addresses | Failures Fixed | Effort |
|----------|-----|-------------------|----------------|--------|
| 1 | **C1+C3**: Redesign `cxf_pivot_update` to take `(oldLB, newLB, oldUB, newUB, infinity)`; add Kahan-stable addition; add `negUnbdCount`/`posUnbdCount` arrays | Activity bound accuracy | 10 numerical drift | Medium |
| 2 | **C2**: Implement full `cxf_pivot_bound` — eta creation, activity propagation, matrix cleanup, fix status bug | PFI chain + activity correctness | All bound-tightening paths | Medium |
| 3 | **H4**: Update Phase I `work_obj` coefficients after each pivot (entering gets +/-1 based on feasibility; leaving gets 0 if non-basic) | Phase I pricing accuracy | boeing2, capri | Low |
| 4 | **C4**: Add `specialMode` flag to SolverState; suppress UNBOUNDED in `cxf_pivot_special` when set; add equality constraint column scan | Phase I false UNBOUNDED | scsd1, scagr25 | Low |
| 5 | **H2**: Add dual-bound confirmation gate to step2/step3 before returning CXF_INFEASIBLE | False infeasibility prevention | boeing2, capri | Low |
| 6 | **H1**: Change `ratio_test.c:152` from `feasTol` to `10.0 * feasTol` | Better pivot selection | All problems | Trivial |
| 7 | **H3**: Fix inner loop convergence formula — remove dimension scaling from `cxf_basis_diff`, add 5-iteration dead zone | Loop termination accuracy | Large/small problems | Low |
| 8 | **M2**: Sparse LU (sparse column/row data structures in factorization) | Timeout elimination | bandm, tuff, vtp.base | High |

**Items 1-6 together would likely fix 12 of the 15 non-passing Netlib instances.** Item 8 fixes the remaining 3 timeouts.

---

## Verification Checklist

```
[x] No hex addresses or offsets
[x] No proprietary solver names or references
[x] All file paths reference convexfeld source tree
[x] All spec references use cxf_ naming convention
[x] Deviations verified against both spec and reference source
```
