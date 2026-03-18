# Convexfeld vs Decompiled Source: Direct Comparison Report

**Date:** 2026-03-18
**Scope:** Solver core (simplex iteration, ratio test, step2/step3, phase I, pivots, perturbation, final/cleanup, pricing)
**Method:** 8 parallel agents read both codebases and compared function-by-function

---

## Executive Summary

The cleanroom implementation has **three catastrophic architectural misidentifications** and **dozens of formula/threshold/polarity bugs**. The three architectural problems alone explain the majority of the 68 Netlib failures:

1. **step2/step3 implement the wrong algorithms entirely** (FBBT instead of BFRT/constraint-elimination)
2. **The perturbation system doesn't actually perturb** (marks dirty instead of removing from pricing)
3. **The ratio test uses a fundamentally different algorithm** (textbook Harris two-pass instead of steepest-edge-weighted single-pass)

Beyond these, there are ~15 critical-severity bugs in pivots, phase transitions, pricing, and post-solve that each individually cause specific Netlib failures.

---

## Table of Contents

- [Tier 1: Wrong Algorithm (3 issues)](#tier-1-wrong-algorithm)
- [Tier 2: Critical Bugs (15 issues)](#tier-2-critical-bugs)
- [Tier 3: High-Severity Bugs (12 issues)](#tier-3-high-severity-bugs)
- [Failure Root Cause Map](#failure-root-cause-map)

---

## Tier 1: Wrong Algorithm

These are cases where the cleanroom implements a completely different algorithm than what the binary does. No amount of threshold tuning will fix these.

### T1.1: step2 is BFRT Post-Processing, NOT Feasibility-Based Bound Tightening

**Files:** `convexfeld/src/simplex/step2.c` vs decompiled `cvx_simplex_step2.c`

**What the binary does:** step2 implements **Bound Flipping Ratio Test (BFRT) deferred processing**. It:
1. Gets candidates from a step2-specific pricing queue
2. For each candidate with `var_status == 1` (basic in a specific row), finds the pivot element in the CSR row
3. Computes `ratio = primal_value / pivot_coefficient`
4. Classifies the ratio against variable bounds into flip types (0=none, 1=flip-upper, 2=flip-lower, 3=flip-both, 4=infeasible)
5. Creates **Type 7** eta vectors recording bound flips
6. Calls `cvx_pivot_bound` only when `flip_type == 3` (both bounds)

**What the cleanroom does:** Savelsbergh-style FBBT on the variable side. Iterates all dirty variables, computes implied bounds from constraint activity, tightens bounds via helper, calls `cxf_pivot_bound` when propagation effectively fixes a variable. Includes a two-stage infeasibility check that **does not exist in the binary**.

**Impact:** Without BFRT post-processing, the dual simplex takes far more iterations (one flip per iteration instead of many). Directly causes TIMEOUT on bounded-variable problems and contributes to grow/boeing UNKNOWN (solver stalls) and scsd8 false UNBOUNDED.

### T1.2: step3 is Constraint Elimination, NOT Constraint-Side FBBT

**Files:** `convexfeld/src/simplex/step3.c` vs decompiled `cvx_simplex_step3.c` (raw: `FUN_1804654b0.c`)

**What the binary does (from raw decompilation):** step3 performs **constraint-based variable elimination**:
1. Gets constraint candidates from a step3-specific queue
2. Pre-screens each constraint with a **range * coefficient significance test** -- if any variable has `|(ub-lb) * coeff| >= threshold * feas_tol`, the constraint is skipped entirely
3. Creates **Type 8** eta vectors (variable-size, containing full row data: indices + coefficients)
4. For each eligible variable in the constraint row, calls `cvx_pivot_bound(env, state, var_idx, lb_or_ub_value)` with **4 arguments** -- the bound value is chosen by `sign(dVar39 * coeff)`
5. Sets `row_basic_var[constr] = -2` (eliminated)
6. The entire row is stripped from the basis

**What the cleanroom does:** FBBT on the constraint side. No pre-screening. Creates lightweight `CXF_ETA_BOUND_CHANGE` eta records with no row data. Only calls `cxf_pivot_bound` when bounds converge to effectively-fixed. Has a **fabricated infeasibility detection** path (two-stage check) that does not exist in the binary -- this is the **direct cause of sierra/maros false INFEASIBLE**.

**Key sub-differences:**
- **pivot_bound argument count:** Binary uses 4 args `(env, state, var, bound_value)`. Cleanroom uses 5 args `(env, state, j, fix, ub, 0)` with `fix = 0.5*(lb+ub)` (midpoint). Fixing at midpoint instead of the correct bound shifts variables to wrong values.
- **Bulk fixing vs single fixing:** Binary fixes ALL eligible variables in a dominated constraint. Cleanroom only fixes variables whose bounds converge. The constraint elimination that removes redundant rows is completely absent.
- **Fabricated infeasibility detection:** Cleanroom returns `CXF_INFEASIBLE` during step3, which the binary never does. step3 in the binary is purely a propagation/elimination pass. Infeasibility detection happens elsewhere. This directly causes **sierra and maros false INFEASIBLE**.
- **step2/step3 feedback loop:** Cleanroom's tighten_bound marks vars dirty, creating cascading propagation between step2 and step3 that doesn't exist in the binary (they consume from separate independent queues).

### T1.3: Ratio Test Uses Wrong Algorithm (Single-Pass Weighted vs Two-Pass Harris)

**Files:** `convexfeld/src/simplex/ratio_test.c` vs decompiled `cvx_simplex_step.c`

**What the binary does:** The ratio test is embedded in cvx_simplex_step. It:
1. Iterates over the **leaving row in the original constraint matrix** (row-based, not column-based)
2. Uses **steepest-edge weight arrays** (`steepest_lb`, `steepest_ub`) to compute weighted ratio bounds
3. Has early-exit conditions based on `pricing_tolerance` thresholds
4. Is a **single-pass** algorithm that computes both lower and upper ratio bounds simultaneously
5. Has a **degenerate direction pre-filter** that rejects pivots where reduced cost and pivot coefficient have same sign
6. Calls `FUN_180463010` (cvx_compute_ratio_bounds) for auxiliary column-based ratio bounds as a feasibility pre-check

**What the cleanroom does:** Textbook Harris two-pass ratio test:
- Pass 1: Compute theta_max with relaxed band (epsilon_feas)
- Pass 2: Among eligible rows, select argmax |d_i| with strict band (0.0)
- Iterates over the **dense FTRAN'd pivot column** (column-based, not row-based)
- No steepest-edge weighting in the ratio test itself
- No degenerate direction pre-filter
- No auxiliary ratio bounds pre-check

**Impact:** Different leaving variable selection on every single iteration. The binary's steepest-edge weighted ratios produce dramatically better pivot choices on degenerate problems. The missing pre-filter allows pivots in wrong directions. The missing auxiliary bounds check allows pivots the binary would reject. This affects **every single Netlib problem**.

**Additional ratio test differences:**
- **Unbounded detection:** Cleanroom returns `CXF_UNBOUNDED` eagerly when no row passes the pivot filter. Binary silently skips and tries next candidate. This causes **false UNBOUNDED** on problems with numerically near-zero columns.
- **BFRT:** Binary does single-flip per candidate. Cleanroom does multi-flip loop. Fundamentally different BFRT behavior.
- **Equality constraint special path:** Binary has distinct code path for equality constraints with additional guards (reduced cost magnitude, row_basic status). Cleanroom treats all constraints uniformly.
- **Degenerate threshold:** Cleanroom uses `feasTol` (1e-6). Binary doesn't have an explicit degenerate status -- degeneracy is handled implicitly via steepest-edge weighting.

---

## Tier 2: Critical Bugs

Individual bugs that each cause specific Netlib failures.

### T2.1: Outer Loop Hardcoded to 5 Rounds (Should Be Mode-Dependent)

**Location:** `convexfeld/src/simplex/solve_lp.c:52` -- `MAX_OUTER_ROUNDS = 5`

**Binary:** `part3_main_loop.c:69-88` -- Primal=5, Dual/Auto=100, Crossover=10

The cleanroom gives the solver 20x fewer recovery rounds for dual simplex (the default for most LP solves). Every problem that needs >5 perturbation/re-stabilization cycles will hit the round limit and TIMEOUT.

**Impact:** Explains a large fraction of the 57 TIMEOUTs.

### T2.2: Perturbation Doesn't Remove Variables from Pricing

**Location:** `convexfeld/src/simplex/perturbation_candidates.c:48-67` vs decompiled `cvx_simplex_perturbation.c:248-263`

**Binary:** Sets `varStatus[j] = -1` (physically removes variable from pricing pool) and for basic degenerate variables, strips the entire row by iterating all columns, removing each from pricing, decrementing basis status, and setting row index to -1.

**Cleanroom:** Calls `cxf_pricing_mark_dirty(state->pricing, j)` which merely flags the variable for repricing. The variable remains eligible for selection on the very next iteration. The row-stripping for basic degenerate variables is completely absent.

**Impact:** The core anti-cycling mechanism has no teeth. Degenerate variables are re-selected immediately. Directly causes **kb2/recipe cycling** and contributes to timeouts on degenerate problems.

### T2.3: Perturbation is Reactive-Only (Binary is Proactive)

**Location:** `convexfeld/src/simplex/solve_lp.c:193-196` vs `part3_main_loop.c:178-196`

**Binary:** Fires perturbation proactively in the first outer round (`iVar21 == 0`) during the first two inner iterations (`iVar15 < 2`), gated by env parameter. Runs every iteration in early cycles regardless of stalling.

**Cleanroom:** Only fires on `stall || degenerate_count > 50 || cumulative_degenerate > 200`. Must accumulate 50 degenerate pivots before any anti-cycling activates.

**Impact:** 50 wasted degenerate pivots before perturbation kicks in on every degenerate problem.

### T2.4: Pricing Queue Filter Polarity Contradiction

**Location:** `convexfeld/src/pricing/update.c:52-53` (keeps `status < 0`) vs `convexfeld/src/pricing/end_level.c:75` (keeps `status >= 0`)

These two files **contradict each other** for L0 variable queue filtering. `update.c` keeps nonbasic variables; `end_level.c` keeps basic variables. Since `end_level.c` appears to be the primary consumer (called from step.c), basic variables fill the queue and nonbasic variables are discarded. This forces escalation to L1/L2 full scans on every iteration.

The same polarity issue exists at L1/L2 (`update.c:90-91` keeps `status < 0`; binary keeps `status >= 0`).

**Impact:** L0 fast-path is broken. Every pricing pass does a full scan. Primary cause of **57 TIMEOUTs**.

### T2.5: pivot_special Unbounded Detection is Completely Wrong

**Location:** `convexfeld/src/simplex/pivot_special.c` vs decompiled `cvx_pivot_special.c`

Four compounding errors:

1. **Missing column coefficient scan:** Binary narrows `canIncrease`/`canDecrease` by checking each inequality constraint's coefficient sign. A single positive coefficient blocks canDecrease. Cleanroom skips this entirely -- no inequality structure gating.

2. **Symmetric RC thresholds (should be asymmetric):** Binary uses `canIncrease = (rc < -1e-10)` but `canDecrease = (rc > 1e30)` (nearly unreachable). Cleanroom uses symmetric `1e-10` for both. This makes canDecrease trigger 1e20x more aggressively.

3. **Wrong bound-flip direction:** Binary's canDecrease flips to upperBound. Cleanroom's can_decrease flips to lowerBound. These are opposite operations.

4. **UNBOUNDED on bounds alone, no RC magnitude check:** Binary checks `reducedCost < -ub_limit` for UNBOUNDED. Cleanroom checks `ub >= ub_limit` (bound infinity only). Variables with modest RC get false UNBOUNDED.

5. **Missing row elimination path:** When variables have infinite bounds and moderate RC, binary calls `cvx_fix_variable()` and eliminates all constraint rows. Cleanroom returns `CXF_OK` (no-op).

**Impact:** Direct cause of **capri and scsd8 false UNBOUNDED**.

### T2.6: simplex_final Phase 1 Filters Wrong Variables

**Location:** `convexfeld/src/simplex/final.c:66-89` vs decompiled `cvx_simplex_final.c:305-364`

**Binary:** Phase 1 iterates variables with `varStatus[j] >= 0` (basic/active). Checks dual values to assign target bounds.

**Cleanroom:** Phase 1 skips `var_status[j] >= 0` (basic) and only processes `status < 0` (non-basic). This is the **opposite filtering**. The target array is computed for completely different variables.

**Impact:** Direct cause of wrong objectives on **boeing2 (604%), fit1p (65%), ganges (111%)**.

### T2.7: simplex_final Phase 5 Objective Double-Counting

**Location:** `convexfeld/src/simplex/final.c:208-221`

Cleanroom pre-subtracts `obj -= c[j]*x_old` then pivot_bound adds `obj += c[j]*x_new`. This formula is only correct if obj_value already contains the full `c^T x`. If it was maintained incrementally (as the binary does), the subtraction removes the wrong amount.

The post-fixing recomputation guard (relative diff > 1e-6) has an enormous threshold for large objectives, masking even large absolute errors.

**Impact:** Primary contributor to **boeing2 604%** objective error.

### T2.8: simplex_cleanup Has 8 of 11 Phases Stubbed

**Location:** `convexfeld/src/simplex/simplex_cleanup.c:81-105`

Binary has 11 phases: basis index adjustment, working array allocation, variable classification, implied bound computation (2-pass), activity initialization, basis restoration, activity computation (Kahan summation), core bound propagation, variable fixing (conservative + aggressive), constraint conversion, cleanup.

Cleanroom has 2 phases: convert tight constraints + memory cleanup. Phases 1-8 are "tracked under separate issues."

**Impact:** The entire implied-bound propagation engine is missing. Variables fixable only through constraint activity analysis remain unfixed. Second major contributor to **wrong objectives**.

### T2.9: Phase I check_phase_one_end Has False Improving Direction Detection

**Location:** `convexfeld/src/simplex/phase_loop.c:116-121`

When Phase I objective is near-zero but positive, the cleanroom does a tighter-tolerance scan (`0.01 * optimality_tol`) for improving directions. This finds false improving directions in near-degenerate problems, sending the solver back into Phase I indefinitely.

Additionally, `phase_loop.c:82-84` takes `max(fresh_obj, running_obj)` as the Phase I objective, which inflates it when `state->obj_value` has drifted upward. This prevents the `<= tol` check from recognizing feasibility.

**Impact:** Primary cause of **boeing1, grow15, grow22 UNKNOWN** status. Contributes to **grow7 wrong objective** (transitions at suboptimal point).

### T2.10: Double phase_end Call Per Iteration

**Location:** `convexfeld/src/simplex/solve_lp.c:183,289`

Binary calls `cvx_simplex_phase_end` once per iteration (after step3). Cleanroom calls it twice (pre-pivot at line 183, post-pivot at line 289). The pre-pivot call can return INFEASIBLE before the step even runs.

**Impact:** Pre-pivot phase_end during Phase I can flag free variables as dual-infeasible before they've been corrected by the current iteration's pivot. Causes premature INFEASIBLE returns.

### T2.11: simplex_final Missing Partial Fixing Path

**Location:** `convexfeld/src/simplex/final.c` vs decompiled `cvx_simplex_final.c:828-891`

Binary has three fixing paths: `phase_apply_all`, `phase_verify_fixings`, and `phase_apply_partial`. The cleanroom has only all-or-nothing fixing. For problems where some fixings violate constraints, the binary selectively applies safe fixings while the cleanroom skips all of them.

**Impact:** Fewer variables get fixed on problems with mixed feasible/infeasible fixings. Contributes to **wrong objectives**.

### T2.12: EXPAND Bound Widening is Fabricated

**Location:** `convexfeld/src/simplex/expand.c:102-181`

The binary's `cvx_simplex_perturbation` has **no EXPAND bound widening**. Its two actions are: (1) remove nonbasic degenerate variables from pricing, (2) remove entire rows of basic degenerate variables. The cleanroom's `cxf_expand_widen_bounds` is entirely invented -- the epsilon formula, the bound modification, the activity recomputation, all of it.

**Impact:** Introduces bound modifications and numerical drift that don't exist in the binary. Could cause incorrect feasibility conclusions.

### T2.13: pivot_primal Missing Activity Update (Does Soft RHS Update Instead)

**Location:** `convexfeld/src/simplex/pivot_primal.c:190-208` vs decompiled `cvx_pivot_primal.c:393-426`

Binary Phase 9 updates `activity[row]`, decrements `constrStatus[row]`, marks column entries as -1 (deletes variable from CSC), and tracks touched constraints for CSR cleanup. Cleanroom updates `work_rhs[row]` only -- a different array. Does NOT decrement constraint status, delete from CSC column, or clean CSR. The variable remains in the constraint matrix after being pivoted.

**Impact:** Accumulated matrix inconsistency after multiple pivot_primal calls. Affects future iterations.

### T2.14: pivot_primal Missing Max-Coefficient Feasibility Guard

**Location:** `convexfeld/src/simplex/pivot_primal.c` (absent) vs decompiled `cvx_pivot_primal.c:113-157`

Binary Phase 2 computes `max(|coeff|) * bound_range` and skips the fix if it exceeds tolerance. Cleanroom has no such guard, fixing variables that the binary would skip.

**Impact:** Contributes to **boeing2 wrong objective**.

### T2.15: pivot_primal Uses Wrong Tolerance for Bound Range Check

**Location:** `convexfeld/src/simplex/pivot_primal.c:100-107`

Binary: `2 * caller_tolerance >= bound_range` (dynamic, typically 1e-6 to 1e-9).
Cleanroom: `bound_range < CXF_BOUND_EQUALITY_TOL` (static 1e-10).

The cleanroom's 1e-10 is much tighter, causing it to pivot variables with tiny bound ranges (say 1e-8) that the binary would reject.

**Impact:** Contributes to **boeing2 wrong objective**.

---

## Tier 3: High-Severity Bugs

Issues that contribute to failures but are less individually impactful.

### T3.1: Bland's Rule Activation (Not in Binary)

`solve_lp.c:173` activates after `perturb_count > 0 && degenerate_count > 3*m`.
`step.c:680` activates after 50 consecutive degenerate pivots.
**The binary has no Bland's rule.** It relies on perturbation + pricing-phase escalation. Bland's forces worst-case pricing and causes massive slowdown.

### T3.2: Convergence Detection Fires Too Infrequently

Binary checks basis_diff every inner-loop pass. Cleanroom checks every `m+1` iterations (`iteration % (num_constrs + 1) == 0`). Missing outer-loop convergence check entirely.

### T3.3: Phase I Pricing Reset at Transition

Cleanroom resets pricing to level 0 (loose tolerance) at Phase I->II. Binary carries forward current pricing level (potentially level 2/aggressive). Starting Phase II with loose pricing selects suboptimal entering variables.

### T3.4: Phase I max(fresh, running) Objective Floor

`phase_loop.c:82`: `phase1_obj = max(fresh_obj, state->obj_value)`. If obj_value drifted upward, this floor prevents feasibility recognition. Directly affects grow problems.

### T3.5: Stall Detection Missing 7 Auxiliary Counters

Binary uses 5 progress counters + 5 size components. Cleanroom uses 3 counters only. Under-counting progress makes stall detection trigger too aggressively (false positives → premature perturbation).

### T3.6: Missing Work Counter Scale Factor in Pricing

Binary multiplies all work counter updates by `scaleFactor` at `solverState+0x438`. Cleanroom adds raw counts. Causes incorrect refactorization pacing.

### T3.7: Missing Eta-Mode Expansion in Pricing

Binary has two expansion modes: matrix mode (CSC) and eta mode (linked list traversal). Cleanroom only has CSR-based expansion. Between refactorizations, cleanroom cannot find dynamic neighbors.

### T3.8: simplex_final Target Array Includes Slacks

Binary allocates `numVars` target values. Cleanroom allocates `numVars + numConstrs` and includes slack variables. Over-fixing slacks distorts constraint activities.

### T3.9: simplex_final Missing AT_LOWER_BOUND_MARKER

Binary writes special IEEE marker `0x54e6dc186ef9f45c` (~-1e-200) for zero-dual variables with both bounds active at zero. Cleanroom skips these variables entirely.

### T3.10: pivot_bound Missing CSR Cleanup

Binary marks variable as removed in row-major representation, zeros column length. Cleanroom does neither. Subsequent row scans still see fixed variables.

### T3.11: Cancellation Rounding Factors Off by 6 Orders

Binary: rounding multiplier = `1 - 1e-6` (0.999999). Cleanroom: `1 - 1e-12`. Much less conservative rounding means less numerical safety margin on problems with significant cancellation.

### T3.12: simplex_cleanup Tight Constraint Conversion Uses Stale Activity Bounds

Binary uses freshly computed activity bounds from the full propagation (phases 5-7). Cleanroom uses whatever was in `min_activity`/`max_activity` from before simplex_final ran. Constraints that became tight due to fixings are not detected.

---

## Failure Root Cause Map

| Failure Category | Primary Root Causes | Secondary Causes |
|---|---|---|
| **TIMEOUT (57)** | T2.1 (5 vs 100 outer rounds), T2.4 (pricing polarity), T1.3 (ratio test algorithm) | T3.1 (Bland's rule), T3.2 (convergence freq), T2.2 (perturbation no-op), T2.3 (reactive-only), T3.6 (work counter) |
| **Wrong Objective (boeing2 604%)** | T2.6 (final filters wrong vars), T2.7 (obj double-counting), T2.8 (cleanup stubbed) | T2.14 (pivot_primal guard), T2.15 (wrong tolerance), T3.11 (rounding factors) |
| **Wrong Objective (grow7 20.8%)** | T2.9 (phase I false improving), T3.3 (pricing reset), T3.4 (obj floor) | T2.8 (cleanup stubbed) |
| **Wrong Objective (fit1p 65%, ganges 111%)** | T2.6 (final wrong vars), T2.8 (cleanup stubbed), T3.9 (missing marker) | T2.11 (no partial fixing), T3.8 (slack targets) |
| **Wrong Objective (tuff 0.16%)** | T3.11 (rounding factors), T2.8 (cleanup stubbed) | T3.10 (CSR cleanup) |
| **False UNBOUNDED (capri, scsd8)** | T2.5 (pivot_special completely wrong) | T1.3 (ratio test eager UNBOUNDED) |
| **False INFEASIBLE (sierra, maros)** | T1.2 (step3 fabricated infeasibility), T1.2 (pivot_bound midpoint fixing) | T2.10 (double phase_end) |
| **UNKNOWN (boeing1, grow15, grow22)** | T2.9 (phase I false improving + obj floor) | T1.1 (step2 wrong algo → stalling), T3.4 (max obj floor) |
| **Cycling (kb2, recipe)** | T2.2 (perturbation no-op), T2.3 (reactive only) | T2.12 (fabricated EXPAND), T3.1 (Bland's interaction) |

---

## Priority Fix Order

Based on impact breadth and implementation effort:

| Priority | Fix | Affects | Effort |
|---|---|---|---|
| **P0** | Fix outer loop: 5→100 for dual/auto, 10 for crossover | 57 TIMEOUTs | Trivial |
| **P0** | Fix pricing queue filter polarity (update.c vs end_level.c) | 57 TIMEOUTs | Trivial |
| **P0** | Fix pivot_special: add column coeff scan, asymmetric RC thresholds, correct flip direction, RC magnitude check for UNBOUNDED | capri, scsd8 | Medium |
| **P0** | Remove fabricated infeasibility detection from step3 | sierra, maros | Trivial |
| **P0** | Fix simplex_final Phase 1: filter basic vars (status >= 0), not non-basic | All wrong objectives | Trivial |
| **P1** | Rewrite perturbation: set varStatus=-1 (actual removal from pricing), implement row stripping for basic degenerate vars | kb2, recipe, timeouts | Medium |
| **P1** | Fix Phase I: remove max(fresh,running) floor, remove tighter-tolerance scan, remove pricing level reset at transition | UNKNOWN cluster | Medium |
| **P1** | Rewrite step2 as BFRT post-processing | Timeouts, stalling | High |
| **P1** | Rewrite step3 as constraint elimination (Type 8 etas, bulk fixing, pre-screening) | sierra, maros, performance | High |
| **P2** | Implement simplex_cleanup phases 1-8 (implied bound propagation) | Wrong objectives | High |
| **P2** | Rewrite ratio test to match binary (row-based, steepest-edge weighted, single-pass, pre-filter) | All problems | Very High |
| **P2** | Fix simplex_final Phase 5 objective formula (remove pre-subtraction) | boeing2 | Low |
| **P2** | Fix pivot_primal: add max-coeff guard, correct tolerance, implement matrix elimination | boeing2, matrix consistency | Medium |
| **P3** | Remove Bland's rule (not in binary) | Timeouts | Trivial |
| **P3** | Fix convergence detection frequency | Timeouts | Low |
| **P3** | Add work counter scale factor to pricing | Refactorization pacing | Low |
| **P3** | Fix cancellation rounding factors (1e-12 → 1e-6) | Numerical edge cases | Trivial |
| **P3** | Remove double phase_end call (keep post-step only) | Edge case infeasible | Trivial |

The P0 fixes are all surgical (threshold changes, polarity flips, filter inversions). Implementing just the P0 fixes should move from 46/114 to roughly 60-70/114. The P1 fixes (perturbation rewrite, step2/step3 rewrite, Phase I fixes) should push toward 90+/114. The P2 ratio test rewrite would be needed for the final push to full Netlib pass.
