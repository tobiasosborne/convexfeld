# V2 Spec Compliance Roadmap

*Generated: 2026-02-21 — 12-agent multi-scale review*

---

## Executive Summary

**Status: 18/59 Netlib pass. 27 false INFEASIBLE. 14 timeout.**

A 12-agent code review against the v2 spec found **~25 CRITICAL**, **~35 HIGH**, and **~20 LOW** divergences. The findings cluster into six themes:

1. **Active correctness bugs** — ratio test direction, objective sign, leaving var status (produce wrong answers NOW)
2. **Phase I false INFEASIBLE** — `correct_basic_variables` hack, `phase_end` blocked in Phase I, no artificial pivot-out
3. **LU foundation** — dense O(m^4) Markowitz, no residual monitoring, no memory pool
4. **Pricing subsystem** — entire v2 multi-level queue architecture absent; needs ground-up rebuild
5. **Data model gaps** — no CSR/CSC working copies, no SolutionData, missing ~30 struct fields
6. **Orchestration** — `cxf_simplex_postsolve` dead, `cxf_solver_dispatch` missing, parameter management absent

The roadmap below is ordered by **dependency and impact**. Phase 0 fixes bugs that produce wrong answers independently. Phases 1-2 attack the false INFEASIBLE root cause. Phases 3-5 build out v2 infrastructure. Phase 6 completes the API.

---

## Phase 0: Critical Bug Fixes (Immediate)

*These produce wrong answers on their own, independent of missing v2 infrastructure.*

### P0.1 Ratio test ignores entering variable direction `s`
- **File:** `ratio_test.c:97-111`, `step.c:155-159` (`find_next_blocker`)
- **Bug:** Code assumes entering variable always increases from lower bound. When `var_status[entering] == CXF_VAR_AT_UPPER`, the ratio direction signs flip. Wrong leaving variable selected.
- **Fix:** Read `var_status[entering]` to determine `s = +1/-1`. Multiply `d_i` by `s` in both ratio passes and in `find_next_blocker`.
- **Spec:** harris_ratio_test.md Stage 1 "Entering direction"

### P0.2 Objective update sign wrong for upper-bound entrants
- **File:** `step.c:501`
- **Bug:** `obj_value += d_entering * stepSize` — correct only for lower-bound entrants. For upper-bound entrants, the sign must be negated.
- **Fix:** `obj_value += s * d_entering * stepSize` where `s` is the entering direction from P0.1.
- **Spec:** revised_simplex.md Step 4 item 4

### P0.3 Leaving variable status never explicitly reset
- **File:** `step.c:110-116` (`cxf_apply_pivot`)
- **Bug:** Only sets `AT_UPPER` conditionally; never sets `AT_LOWER` as default. Status can remain stale (e.g., still marked BASIC).
- **Fix:** Unconditionally set `AT_LOWER` first, then conditionally override with `AT_UPPER`.
- **Spec:** revised_simplex.md Step 4 item 3

### P0.4 Auxiliary coefficient sign inconsistency
- **File:** `step.c:48-56` vs `reduced_costs.c:26-34`
- **Bug:** For `>=` constraints, `step.c` returns `+1.0` if rhs>0, `reduced_costs.c` returns `-1.0` always. Incremental RC update and full recompute produce different reduced costs.
- **Fix:** Unify to single function. The `>=` coefficient should be `-1.0` unconditionally.

### P0.5 BTRAN failure silently substitutes pi = c_B
- **File:** `reduced_costs.c:47-62`
- **Bug:** When `calloc` fails or BTRAN errors, fallback sets `pi = c_B` — only correct for identity basis. On any real problem this produces completely wrong reduced costs with no error signal.
- **Fix:** Propagate error. Do not silently fall back to wrong values.

### P0.6 Ratio test pivot element filter too coarse
- **File:** `ratio_test.c:73,97,103`
- **Bug:** Uses `relaxedTol = 10 * feasTol = 1e-5` to skip pivot elements. Spec says Harris pivot threshold is ~1e-9. Elements between 1e-9 and 1e-5 are silently rejected.
- **Fix:** Use `CXF_PIVOT_TOL` (1e-9) for element skip, `feasTol` only for ratio window.
- **Spec:** tolerances_constants.md §3

### P0.7 Refactorization bypasses `cxf_refactor_check()`
- **File:** `step.c:518-521`
- **Bug:** Hardcoded `REFACTOR_INTERVAL = 100` instead of calling `cxf_refactor_check()` which checks `env->refactor_interval` (default 50), eta count limits, eta memory limits, and FTRAN degradation.
- **Fix:** Replace hardcoded check with `cxf_refactor_check(state, env)`.

### P0.8 `cxf_extract_solution` unconditionally sets OPTIMAL
- **File:** `extract.c:88-90`
- **Bug:** `if (state->phase == 2) model->status = CXF_OPTIMAL` — overwrites status even after non-optimal termination (e.g., iteration limit hit while in Phase II).
- **Fix:** Only set OPTIMAL if the solve actually terminated with optimality, not just because we reached Phase II.

### P0.9 BFRT flipped variables not cascade-notified
- **File:** `step.c:511-515`
- **Bug:** Only `entering` and `leaving` get `cxf_pricing_cascade_update`. The up-to-10 BFRT-flipped variables are not notified. Their structural neighbors become stale in pricing queues.
- **Fix:** Loop over `flipped_rows[]` and cascade-update each.

### P0.10 `cxf_fix_variables_at_bounds` resets all diag_coeff to +1.0
- **File:** `refactor.c:76-80`
- **Bug:** Ignores `>=` constraints that need `-1`. Corrupts FTRAN for mixed-sense problems.
- **Fix:** Preserve or recompute diag_coeff per constraint sense.

---

## Phase 1: Phase I Critical Path (False INFEASIBLE Root Cause)

*The 27 false INFEASIBLE Netlib failures are caused by this cluster. All items must ship together — partial fixes produce no improvement.*

### P1.1 Remove `correct_basic_variables` hack
- **File:** `phase_loop.c:67-131`
- **Problem:** Gauss-Seidel iterative correction of x_B without updating LU factors. Corrupts simplex state — all subsequent FTRAN/BTRAN operate on inconsistent data.
- **Replace with:** Proper refactorization (P1.3) followed by direct x_B = B^{-1}b computation.
- **Spec:** v2 requires accurate LU + residual monitoring, not iterative correction
- **Beads:** `x5dj`

### P1.2 Enable `phase_end` in Phase I
- **File:** `solve_lp.c:136`
- **Problem:** `if (state->phase == 2)` guard prevents `cxf_simplex_phase_end` from running during Phase I. Free variable dual infeasibility check, inactive constraint removal, and small-contribution variable scan are all blocked.
- **Fix:** Remove the phase guard. The spec (simplex_iteration.md item 7) calls phase_end unconditionally.
- **Beads:** `fiyt`

### P1.3 Force refactorization at Phase I → Phase II transition
- **File:** `phase_one.c:138` (`cxf_transition_to_phase_two`)
- **Problem:** Transition uses accumulated (potentially inaccurate) LU from Phase I pivots. Spec explicitly lists phase transition as a refactorization trigger.
- **Fix:** Call `cxf_solver_refactor(state, env)` inside `cxf_transition_to_phase_two`.
- **Spec:** numerical_stability.md §A.4

### P1.4 Implement artificial variable pivot-out at transition
- **File:** `phase_one.c:138`
- **Problem:** Zero-value artificial basic variables are not pivoted out. They remain in the Phase II basis, causing incorrect reduced costs and potential cycling.
- **Fix:** After Phase I objective reaches zero, scan basis for remaining artificials at zero value and pivot them out using the leaving row mechanism.
- **Spec:** simplex_phases.md "Phase I to Phase II Transition"

### P1.5 Reset pricing state at Phase I → Phase II boundary
- **File:** `phase_one.c:138`
- **Problem:** Multi-level pricing tolerance (d1th) and candidate sets carry over from Phase I objective to Phase II objective without reset.
- **Fix:** Call pricing reset (clear dirty arrays, reset current_level to 0, invalidate caches) inside transition.

### P1.6 Implement proactive perturbation
- **Problem:** Perturbation is only reactive (triggered by stall/degeneracy). Spec says apply in first 1-2 iterations of round 0.
- **Fix:** In `cxf_solve_lp`, call `cxf_simplex_perturbation` for the first 1-2 iterations of round 0 unconditionally.
- **Beads:** `zr5l`

### P1.7 Fix perturbation mechanism (candidate removal, not bound modification)
- **File:** `perturbation.c:148-149`
- **Problem:** Code sets `work_x[j] = ub`, `var_status = AT_UPPER` — this modifies bounds. Spec P2.6 says "removes degenerate candidates from pricing set... avoids modifying bound arrays."
- **Fix:** Use `cxf_pricing_candidates` to identify degenerate candidates, mark them as excluded from pricing rather than flipping their bounds.
- **Beads:** `a5vp`

---

## Phase 2: LU / Basis Foundation

*The LU factorization is the numerical foundation. It must be solid before anything else matters.*

### P2.1 Sparse Markowitz LU (replace dense O(m^4))
- **File:** `lu_factorize.c:56-310`
- **Problem:** Dense m×m working matrix. O(m^3) per elimination step due to full column scan for col_max at lines 135-140. This explains the 47-65% LU runtime.
- **Fix:** Implement sparse Markowitz with: (a) compressed sparse column storage, (b) maintained column maxima, (c) linked-list row/column count structures. Target: O(nnz * fill_in) per factorization.
- **Spec:** product_form_inverse.md Step 1 — "sparse Gaussian elimination"
- **Beads:** `uxae`

### P2.2 FTRAN residual monitoring
- **Problem:** No post-FTRAN `||Bx - a||` check exists anywhere. Numerical drift goes undetected until hardcoded refactorization interval.
- **Fix:** After each FTRAN, compute residual. If `||r|| > 10 * epsilon_feas`, trigger immediate refactorization.
- **Spec:** numerical_stability.md §A, product_form_inverse.md "Stability Monitoring"

### P2.3 Eta memory pool (bump allocator)
- **File:** `pivot_eta.c:73` (individual calloc), `refactor.c:34-49` (O(k) free chain)
- **Fix:** Implement bump allocator with bulk deallocation. Allocate eta vectors from pool; at refactorization, reset pool pointer to zero. O(1) dealloc instead of O(k).
- **Spec:** basis_state.md Memory Pool, eta_vector.md Arena Allocation

### P2.4 Implement `cxf_fix_variables_at_bounds` properly
- **File:** `refactor.c:61` (currently empty stub)
- **Problem:** All spec behavior missing (constraint-driven candidate identification, ratio-based variable selection, eta vector creation, objective update, pricing notification).
- **Fix:** Implement per basis_operations.md Phases 1-4.

### P2.5 Hyper-sparse FTRAN/BTRAN
- **File:** `ftran.c:147`, `btran.c:161`
- **Problem:** Neither checks `result[pivot_row] == 0` before processing each eta. Significant performance loss on sparse RHS.
- **Fix:** Skip eta application when `result[pivot_row] == 0` (PFI Step 3.3).

### P2.6 Unify FTRAN/BTRAN tolerance constants
- **File:** `ftran.c:45,61`, `btran.c:49`
- **Problem:** Hardcoded `1e-15` inconsistent with `CXF_MIN_PIVOT` (1e-13) and `CXF_PIVOT_TOL` (1e-9).
- **Fix:** Use named constant derived from `CXF_MIN_PIVOT`.

---

## Phase 3: Data Model Alignment

*Add the structural foundations missing from the data layer.*

### P3.1 CSR/CSC working copies on SolverState
- **Problem:** Code reads model's matrix directly. Any row-negating operation (BFRT bound flipping) during solve corrupts the original model permanently.
- **Fix:** In `cxf_simplex_init`, copy model's CSR+CSC into SolverState-owned arrays.
- **Spec:** solver_state.md — SolverState owns CSR+CSC

### P3.2 SolutionData struct
- **Problem:** Spec defines a model-level container for results (iteration counts, objective bounds, adaptive thresholds, anti-cycling history). None exists.
- **Fix:** Create struct per work_arrays.md. Wire into solve pipeline.

### P3.3 Add missing SolverState fields
- Key missing fields: `numSlacks`, `solveModeAlt`, `initMode`, `iterLimit`, `varFlags`, `steepestEdgeLB/UB`, `dualSteepestLB/UB`, `solStatus`, `constraintSense` copy, `constraintRHS` copy, `thresholds[6]`
- **Spec:** solver_state.md Fields section

### P3.4 Add missing BasisState fields
- Key missing: `etaRowCount`, `numericalStabilityFlag`, `fillInEstimate`, memory pool fields
- **Spec:** basis_state.md Fields

### P3.5 Fix BFRT matrix coefficient negation
- **File:** `step.c:464-471`
- **Problem:** BFRT flips don't negate constraint matrix row coefficients per harris_ratio_test.md Stage 3 Step 6c.
- **Depends on:** P3.1 (need working copies to negate safely)

---

## Phase 4: Pricing Subsystem Rebuild

*The entire v2 pricing architecture is absent. This is a ground-up reimplementation.*

### P4.1 Rebuild PricingState structure
- Add: `levelActive[MAX_LEVELS]`, `constrFlags[]` (4-bit byte), `varFlags[]` (4-bit byte), `constrQueue[MAX_LEVELS][]`, `varQueue[MAX_LEVELS][]`, committed/total counts per level, 6-slot cached counts, output buffers
- **Spec:** pricing_state.md

### P4.2 Implement cxf_pricing_update_var
- Producer: marks structurally adjacent constraints dirty in constraint queues via CSC traversal
- **Spec:** pricing_core.md

### P4.3 Implement cxf_pricing_update_constr
- Producer: marks structurally adjacent variables dirty in variable queues via CSR traversal
- **Spec:** pricing_core.md

### P4.4 Implement cxf_pricing_update (queue consumer)
- Processes both queues: filters invalid entries, promotes pending→committed, invalidates caches
- **Spec:** pricing_core.md

### P4.5 Rewrite cxf_pricing_candidates with adaptive strategy
- Three-threshold adaptive: expansion multiplier, coverage fraction, work factor
- Multi-level queue retrieval (not raw dirty-flag scan)
- **Spec:** pricing_core.md, partial_pricing.md Phase 4

### P4.6 Rewrite cxf_pricing_end_level
- Queue compaction with committed/pending promotion/demotion, status-array-based filtering
- Must take `(PricingState*, SolverState*)` per spec
- **Spec:** pricing_support.md

### P4.7 Implement cxf_pricing_mark_dirty with 4-bit flags
- Replace boolean `int` dirty flag with 4-bit flag byte encoding committed/pending at each of 2 levels
- **Spec:** pricing_support.md

### P4.8 Wire into iteration loop
- Replace `cxf_pricing_cascade_update(entering)` + `cxf_pricing_cascade_update(leaving)` with spec's `cxf_pricing_update_var(entering)` + `cxf_pricing_update_constr(leaving)` + `cxf_pricing_update()`
- **File:** `step.c:511-515`

### P4.9 Implement steepest edge weight updates
- **Problem:** Pricing degenerates to Dantzig (most-negative RC) because `gamma_j` weights are never updated.
- **Fix:** Implement DSE/Devex update formulas per revised_simplex.md Step 6.
- Add weight arrays to SolverState.

---

## Phase 5: Perturbation / Robustness Completion

### P5.1 EXPAND perturbation per P2.6
- Phase 1: Apply perturbation to basic variables at bounds
- Phase 2: Use `cxf_pricing_candidates` (from P4.5) to determine which nonbasics to perturb
- Phase 3: Restore bounds before optimality analysis
- **Beads:** `a5vp`, `9yi2`

### P5.2 Perturbation bound restoration before analysis
- **File:** `perturbation.c:122-128`
- **Problem:** Saved bounds not restored before `analyze_basic()`. Analysis operates on already-perturbed bounds.
- **Beads:** `9yi2`

### P5.3 Crash basis implementation (P2.5)
- **Problem:** Current crash output (`row_status`) completely ignored by `cxf_setup_phase_one`. Crash provides no benefit — all constraints still get artificial variables.
- **Fix:** Implement spec's two-step crash (feasibility-based BASIC_LOWER + sparsity-based BASIC_UPPER). Use crash result to reduce artificial count.
- **Beads:** `snwu`

### P5.4 Bland's rule trigger improvement
- **File:** `solve_lp.c:125-127`
- **Problem:** Bland's rule activated after `3 * num_constrs` iterations regardless of cycling. Spec says perturbation is primary anti-cycling; Bland's is last resort.
- **Fix:** Trigger only after perturbation fails to break cycling.

---

## Phase 6: Orchestration & API

### P6.1 Implement `cxf_simplex_postsolve`
- **File:** `cleanup.c:37-76` (dead stub)
- Implement: unscale primal, unscale dual, restore fixed variables, unscale reduced costs
- Wire into `cxf_solve_lp` post-solve sequence

### P6.2 Implement `cxf_simplex_final` analysis
- **File:** `context.c:208` (memory-free only)
- Add: dual-feasibility variable fixing (complementary slackness), activity verification
- **Spec:** P3.22

### P6.3 Solution extraction for non-OPTIMAL termination
- **File:** `solve_lp.c:225`
- **Problem:** `cxf_extract_solution` only called on OPTIMAL. Iteration-limit and time-limit should still extract best-available solution.

### P6.4 Parameter backup/restore
- Save environment parameters before solve, restore after
- **Spec:** P3.25 Phase 1

### P6.5 Implement solve entry chain
- `cxf_optimize` → `cxf_optimize_internal` → `cxf_solve_entry` → `cxf_solve_dispatch` → `cxf_solve_lp`
- Method selection (primal/dual/auto), presolve-solve-uncrush cycle, callback paths
- **Spec:** P3.24

### P6.6 Implement progress logging
- **File:** `iterate.c:33-37` (no-op stub)
- Time-throttled iteration logging with external callback invocation
- **Beads:** `1lkf`

### P6.7 Remove dead code
- `cxf_run_phase_one` / `cxf_run_phase_two` in `phase_loop.c` — never called
- Duplicate `clear_eta_list` in `warm.c` and `refactor.c`
- `eta_capacity` field — declared but never used

---

## Dependency Graph

```
Phase 0 (bug fixes)
  └──→ Phase 1 (Phase I false INFEASIBLE)
         ├──→ Phase 2 (LU foundation)
         │      └──→ Phase 3 (data model)
         │             ├──→ Phase 4 (pricing rebuild)
         │             │      └──→ Phase 5 (perturbation/robustness)
         │             │             └──→ Phase 6 (orchestration/API)
         │             └──→ Phase 5.3 (crash basis, independent)
         └──→ Phase 1.6-1.7 (perturbation, partial — can start with current pricing)
```

**Critical path:** P0 → P1 → P2.1 → P3.1 → P4 → P5.1

**Parallel tracks:**
- P2.2-P2.6 (FTRAN/BTRAN improvements) can proceed alongside P1
- P3.2-P3.4 (data model structs) can proceed alongside P2
- P6 (orchestration) can proceed alongside P4-P5

---

## Impact Prediction

| Milestone | Expected Netlib Change | Why |
|-----------|----------------------|-----|
| Phase 0 complete | 18→22-25 pass | Fixes wrong answers on problems that were close to passing |
| Phase 0+1 complete | 25→35-40 pass | Eliminates false INFEASIBLE root cause cluster |
| Phase 0+1+2 complete | 35→45-50 pass | Accurate LU reduces timeouts and numerical drift |
| Full P0-P5 | 50-55+ pass | V2 defense layers working as a system |

---

## Files Requiring Refactor (>200 LOC Rule Violations)

| File | Lines | Action |
|------|-------|--------|
| `step.c` | 525 | Split into step_pricing.c, step_bfrt.c, step_pivot.c, step_rc.c |
| `context.c` | 341 | Split init/final into separate files |
| `lu_factorize.c` | 369 | Rewrite as sparse (Phase 2.1) |
| `btran.c` | 312 | Deduplicate `cxf_btran_vec` into `cxf_btran` |
| `warm.c` | 258 | Extract `clear_eta_list` to shared util |
| `phase_steps.c` | 253 | Consider splitting step2/step3 |
| `perturbation.c` | 243 | Natural split at Phase 1/2/3 boundaries |
| `phase_loop.c` | 242 | Remove dead code (run_phase_one/two) |
| `pivot_primal.c` | 233 | Review after pivot operations implemented |
| `solve_lp.c` | 229 | Extract inner loop to separate function |
| `post.c` | 227 | Split phase_end and post_iterate |
| `setup.c` | 220 | Extract preprocess |
| `presolve.c` | 215 | Consider splitting checks |
| `refactor.c` | 221 | Extract clear_eta_list |
| `eta_factors.c` | 212 | Mild violation, lower priority |

---

## Mapping to Existing Beads Issues

| Beads ID | Title | Roadmap Phase |
|----------|-------|---------------|
| `d1th` | ✅ Pricing tolerance escalation | Done |
| `zr5l` | Proactive perturbation | P1.6 |
| `fiyt` | phase_end in Phase I | P1.2 |
| `x5dj` | correct_basic_variables / LU accuracy | P1.1, P2.1, P2.2 |
| `a5vp` | Perturbation uses pricing candidates | P1.7, P5.1 |
| `9yi2` | Perturbation bound restoration | P5.2 |
| `snwu` | Crash basis | P5.3 |
| `1azn` | EXPAND perturbation | P5.1 |
| `uxae` | LU factorization performance | P2.1 |
| `4gfy` | Remove diag_coeff hack | P2.1 (implicit) |
| `cgjf` | 40/56 Netlib failures | Resolved by P0-P2 |
| `c4bh` | Constraint satisfaction tests | Testing gate for P0-P1 |
| `y1ro` | Presolve performance | P6 (lower priority) |

---

## Review Agent Attribution

| Agent | Scale | Scope | Key Findings |
|-------|-------|-------|-------------|
| 1 | Architecture | Orchestration flow | 28 divergences; phase_end blocked in P1; postsolve dead |
| 2 | Architecture | Data model & state | ~30 missing struct fields; no CSR/CSC copies; no SolutionData |
| 3 | Architecture | Cross-module interactions | 10 interface mismatches; pricing signatures all wrong; refactor bypass |
| 4 | Function | step.c iteration | F2 ratio direction CRITICAL; F4 obj sign CRITICAL; 525 LOC |
| 5 | Function | Pricing subsystem | Entire v2 architecture absent; 10 CRITICAL findings |
| 6 | Function | Perturbation + ratio test | EXPAND mechanism wrong; bound modification vs candidate removal |
| 7 | Function | Basis operations | Dense O(m^4) LU; no memory pool; 4 CRITICAL |
| 8 | Function | Pivot operations | Leaving var status bug; BFRT coefficient negation missing |
| 9 | Line-by-line | Numerical stability | Pivot filter 1e-5 vs 1e-9; no residual monitoring; hardcoded constants |
| 10 | Line-by-line | Phase I critical path | correct_basic_variables corrupts state; 4 CRITICAL false-INFEASIBLE paths |
| 11 | Line-by-line | Setup/presolve/post | postsolve dead; extract unconditional OPTIMAL; pricing memory leak |
| 12 | Line-by-line | Reduced costs/iterate/refine | BTRAN silent corruption; no eta records in refine; free var snap to -inf |
