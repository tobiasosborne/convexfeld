# Agent Handoff

*Last updated: 2026-03-19*

---

## STATUS: 147/147 tests. 25 fixes from source comparison (16 surgical + 6 medium + 3 full rewrites). Master pushed.

### Session 2026-03-19 (continued): T1.3 Ratio Test Rewrite + T2.3 Proactive Perturbation

**T1.3 (convexfeld-xvm6 CLOSED):** Rewrote ratio_test.c from Harris two-pass to
single-pass SE-weighted algorithm. Key changes:
- Single pass with `score = ratio * dir / sqrt(weight)` selection
- Soft degenerate direction filter (`rc * d_i > 0` → 2x penalty)
- Equality constraint path (bound-based detection)
- Phase I dual-bound checking (both bounds per direction)
- Inline single-flip BFRT (extracted next-blocker into bfrt.c)
- ratio_test.c 185 LOC, bfrt.c 98 LOC (both under 200)
- Reviewer confirmed: 8/8 items CORRECT or PARTIALLY CORRECT, no crash bugs

**T2.3 (convexfeld-ulvu CLOSED):** Added proactive perturbation in round 0.
Binary fires perturbation on first 2 iterations of outer round 0 regardless
of stalling (crash basis typically degenerate). Added `PROACTIVE_ITERS` define
and `round_iter` counter. One-file change to solve_lp.c.

**T3.10 (CSR/CSC cleanup):** Added `purge_matrix_entries()` to Phase 7 of
`cxf_pivot_bound`. Walks CSC column, marks entries with sentinel -1, zeros
matching CSR entries. Fixed variables now invisible to row/column scans.

**T3.5 (stall detection counters):** `bounds_propagated`, `ineq_to_eq_count`,
and `matrix_transitions` were declared but never incremented. Now incremented
in step2, step3, simplex_cleanup, and pivot_bound. Progress score no longer
under-counts, reducing false-positive stall detection.

**Remaining convergence items (not yet fixed):**
- T2.5 error 5: Row elimination path in pivot_special (needs infrastructure)
- T2.8: simplex_cleanup 8/11 phases stubbed (high effort)
- T3.9: simplex_final missing AT_LOWER_BOUND_MARKER
- T3.12: simplex_cleanup uses stale activity bounds

### Session 2026-03-19: Source Comparison P0/P1 Surgical Fixes

**15 fixes across 11 source files, 3 test files. All verified by reviewer subagents.**
**Net: -50 lines (163 removed, 113 added). 147/147 tests pass.**

| Fix | Issue | Severity | Description |
|-----|-------|----------|-------------|
| T2.4 | ugbs CLOSED | P0 | Pricing polarity — end_level.c kept basic vars, now keeps nonbasic |
| T2.1 | seay CLOSED | P0 | Outer loop 5→100 for dual/auto, 10 for crossover |
| T2.10 | — | P0 | Remove pre-pivot phase_end (binary: post-pivot only) |
| T3.1 | — | P3 | Bland's rule deactivated (not in binary) |
| T2.5 | 89i6 CLOSED | P0 | pivot_special: 4/5 errors (asymmetric RC, coeff scan, UNBOUNDED, flip) |
| T2.6 | ddpq CLOSED | P0 | simplex_final Phase 1 filter inverted (basic, not nonbasic) |
| T1.2 partial | 6g5e IN_PROGRESS | P0 | step3 fabricated infeasibility removed |
| T2.9 | gjct CLOSED | P0 | Phase I false improving direction scan removed |
| T3.4 | (gjct) | P0 | Phase I max(fresh,running) obj floor removed |
| T3.3 | — | P3 | Pricing level carried forward at Phase I→II |
| T2.12 | (6zjl) | P0 | EXPAND bound widening (Mechanism B) removed — fabricated |
| T2.2 | 6zjl CLOSED | P0 | Perturbation mark_dirty removed (stops re-queuing degenerate vars) |
| T3.2 | — | P3 | Convergence detection modulo gate removed + outer-loop check |
| T3.11 | — | P3 | Cancellation rounding 1e-12 → 1e-6 |

**Expected Netlib impact** (per comparison report estimates):
- P0 surgical → 46→~60-70 Netlib (pricing polarity + outer loop = biggest impact)
- Phase I fixes → UNKNOWN cluster (boeing1, grow15, grow22)
- step3 infeasibility removal → sierra, maros
- pivot_special → capri, scsd8

**Medium-effort fixes also applied (Batch 2):**

| Fix | Issue | Description |
|-----|-------|-------------|
| T2.15 | 44tz CLOSED | pivot_primal: dynamic tolerance (2*tol >= range) |
| T2.14 | 4ks4 CLOSED | pivot_primal: max-coeff feasibility guard |
| T2.13 | jo1l CLOSED | pivot_primal: activity update + CSC invalidation |
| T2.7 | me2m CLOSED | simplex_final: fix Phase 5 obj double-counting |
| T2.11 | qyst CLOSED | simplex_final: partial fixing path |
| T3.8 | — | simplex_final: skip slacks in Phase 1 |

**Full rewrites completed (Batch 3):**

| Fix | Issue | Description |
|-----|-------|-------------|
| T1.1 | lz1g CLOSED | step2 rewritten: BFRT deferred bound-flip processing |
| T1.2 | 6g5e CLOSED | step3 rewritten: constraint elimination |

**Remaining issues (not yet fixed):**
- T1.3 (P0): Ratio test algorithm mismatch — full rewrite (session-scale)
- T2.3 (P1): Proactive perturbation in first outer round
- T2.5 error 5: Row elimination path in pivot_special
- T2.8 (P2): simplex_cleanup 8/11 phases stubbed
- T3.5-T3.7, T3.9-T3.10, T3.12: Various Tier 3 issues

### !! CRITICAL: Source Comparison Report (2026-03-18) !!

**`docs/convexfeld_source_comparison.md`** — 8-agent comparison of cleanroom vs decompiled source
found **3 catastrophic algorithm misidentifications** and **27 critical/high-severity bugs**.

**The spec V2 compliance work was chasing the WRONG targets.** The cleanroom specs
misidentified three core algorithms. No amount of spec-compliance fixes will help:

1. **step2 is BFRT post-processing, NOT FBBT** (T1.1) — Requires full rewrite
2. **step3 is constraint elimination, NOT FBBT** (T1.2) — Requires full rewrite
3. **Ratio test is steepest-edge weighted single-pass, NOT Harris two-pass** (T1.3) — Requires full rewrite

**30 beads issues created** (3 P0 wrong-algorithm, 9 P0 critical, 7 P1 critical, 12 P2 high).

**Recommended fix order (from comparison doc):**
- P0 surgical (trivial effort, ~60+ Netlib): pricing polarity fix, simplex_final filter inversion,
  Phase I obj floor removal, remove fabricated step3 infeasibility, pivot_special 5 errors
- P1 medium (medium effort, ~80+ Netlib): perturbation rewrite, Phase I fixes, step2/step3 rewrite
- P2 high (high effort, ~100+ Netlib): ratio test rewrite, simplex_cleanup full implementation

**Key insight:** The previous session's "spec V2 compliance" work was fixing code to match
specs that THEMSELVES were wrong about the algorithm. The specs described FBBT for step2/step3
when the binary actually does BFRT post-processing and constraint elimination. The specs
described Harris two-pass ratio test when the binary uses steepest-edge weighted single-pass.

### Session 2026-03-18 (continued): Cleanroom Q&A Analysis

**cleanroom_implementation_qa.md** analyzed — answers to 18 questions from source code holder.
11 new beads issues created mapping QA findings to specific fixes:

| ID | Pri | Fix | Expected Impact |
|----|-----|-----|-----------------|
| ke87 | P0 | FTRAN residual refactorization trigger | False UNBOUNDED, obj drift |
| 3d05 | P0 | Perturbation formula: eps*(1+|bound|)*(1+hash(i)) | kb2/recipe cycling |
| zqsd | P0 | Perturbation counter cumulative, not reset | Second perturbation fires sooner |
| 74df | P1 | Unbounded confirmation probe | scsd8/shell false UNBOUNDED |
| x5tq | P1 | Outer loop limits: primal=5 not 100 | Faster termination |
| xmvx | P1 | Small pivot accumulation trigger | Numerical edge cases |
| ahgw | P1 | Adaptive refactorization interval | Long-running problems |
| 2gje | P2 | Post-fixing obj recomputation diagnostic | boeing2/vtp wrong obj |
| cq7p | P2 | Replace Bland trigger with outer loop limits | Cycling |
| sbeo | P2 | Verify Level 0 pricing is O(dirty-set) | 57 TIMEOUTs |
| 6k38 | P2 | Growth factor: track all U entries | tuff/vtp numerical |

**Key insight from Q&A:** The 57 TIMEOUTs are NOT from missing spec compliance — they're from
Level 0 pricing not being fast enough (should be O(dirty-set), handling >90% of iterations).
The 3-level pricing infrastructure EXISTS but may bypass Level 0 with full scans.

### Session 2026-03-18: Solver-Core V2 Compliance (Convergence Focus)

**3 issues closed, 1 new test file, surgical spec-compliant fixes:**

1. **convexfeld-8ogg CLOSED** (P1): `cxf_simplex_final` rewritten from destructor to 5-phase
   dual-feasibility variable fixer per simplex_lifecycle.md. Destructor renamed to
   `cxf_state_free`. All 30+ callers updated. Phase 5 uses `cxf_pivot_bound` with
   objective adjustment to avoid double-counting after `cxf_recompute_objective`.
   New file: `src/simplex/final.c` (~200 LOC). Test: `test_simplex_final_fix.c`.

2. **convexfeld-7e8p CLOSED** (P1): Not a bug — issue claim was outdated. The current code
   DOES filter constraint entries by slack variable status (`var_status[num_vars+idx]`),
   which is the standard revised simplex convention for constraint status.

3. **convexfeld-l9lp CLOSED** (P2): Added eta vector creation (Step 6) and pricing
   notification (Step 7) to `cxf_pivot_primal`. Previously, tight-bound variable
   elimination created no eta record and didn't notify pricing — basis update chain
   and pricing state became stale. Fixed var_status constants to use CXF_VAR_AT_LOWER/
   CXF_VAR_AT_UPPER.

4. **convexfeld-hx6u CLOSED** (P2): Added `cxf_pivot_bound` call in step2.c and step3.c
   when bound propagation tightens a variable to effectively fixed (ub-lb < BOUND_EQUALITY_TOL).
   Prevents degenerate pivots on near-fixed variables during iteration.

5. **convexfeld-rrc5 CLOSED** (P2): Added `CXF_ETA_BOUND_CHANGE` (type 5) eta records
   in `tighten_bound()` for both step2.c and step3.c per simplex_iteration.md.
   Records variable index, old/new bound, direction.

6. **convexfeld-35wl CLOSED** (P2): Implemented `cxf_pricing_get_var_stats` —
   symmetric mirror of `cxf_pricing_get_constr_stats`. Completes pricing accessor API.

**7 false-missing issues closed** (audit searched src/basis/ only, functions exist in src/simplex/):
l22c (pivot_bound), z5co (pivot_special), lmgu (pivot_primal), ubky (pivot_update),
gkmp (simplex_iterate), iwwh (pivot_check), 7jhe (simplex_cleanup).

**2 structural issues closed**: zgwv (cascade update — functionally implemented via
separate calls), fm7g (selfPtr — handled by arena allocator internally).

**Analysis finding:** `cxf_propagate_bounds` (convexfeld-zuat) is DEAD CODE — zero callers.
`cxf_simplex_cleanup` stubs Phases 1-8. Previous HANDOFF claim that zuat causes false
UNBOUNDED/INFEASIBLE was incorrect — those are iteration-time failures, not post-solve.
Issue deferred until simplex_cleanup wiring is implemented.

### Session 2026-03-17: V2 Compliance Sprint + Solver-Core Fixes

**24 issues closed across 4 batches + 3 solver-core fixes:**

**Batch 1 (5 issues):** constr_candidates status filters (u1qe, rcul), pricing_invalidate
semantics (u82g), basis_diff counters (gicm), simplex_cleanup call (gekg)

**Batch 2 (5 issues):** pricing_end_level SolverState param + queue filtering (au3b, 8nlv),
cxf_check_env validation (2z4l), Method parameter (t64j), callback mutex (ughp)

**Batch 3 (6 issues):** pre/post optimize callbacks (aqa5, trfq), param backup/restore (ffz0),
setdblparam + string params (dz9y, gtk4), clear_solution (3mgs)

**Batch 4 (5 issues):** locale save/restore (duix, 8gfk), env_acquire_lock (jiq7),
validate_solution (ru83), clear_pending_buffer (on6h)

**Solver-core fixes (3 issues):**
- **BFRT objective recomputation** (2rnc): Phase II obj recomputed from scratch when flips occur
- **Cumulative degenerate pivot detector** (98bt): Bypasses Bland's reset-at-50 dead end; fires perturbation at 200 cumulative degenerate pivots → unlocked agg2, agg3
- **Consecutive small pivot refactorization** (4llu): Forces refactorization after 5 consecutive small pivots (|α| < 1e-7) per numerical_stability.md §A.3

### Netlib Results (2026-03-15, post V2 sprint)

**46 PASS** (was 22): afiro, adlittle, **agg**, bandm, beaconfd, blend, **bnl1**, brandy,
**czprob**, **degen2**, e226, **fffff800**, israel, lotfi, sc50a, sc50b, sc105, **sc205**,
**scagr25**, scagr7, scfxm1, **scfxm2**, **scfxm3**, scorpion, **scrs8**, scsd1, **scsd6**,
sctap1, **sctap2**, **sctap3**, **share1b**, share2b, ship04l, **ship04s**, **ship08l**,
**ship08s**, **ship12l**, **ship12s**, standata, standgub, standmps, stocfor1, **stocfor2**,
**agg2** (NEW from cumulative stall fix), **agg3** (NEW from cumulative stall fix)

**Still failing (partial full-suite run 2026-03-17):**

| Failure mode | Instances |
|---|---|
| TIMEOUT (10s) | 25fv47, 80bau3b, bnl2, bore3d, cre-a/b/c/d, cycle, d2q06c, d6cube, degen3, dfl001, etamacro, finnis, fit1d, fit2d, fit2p, forplan, gfrd-pnc, greenbea/b, ken-07/11/13/18, maros-r7, modszk1, nesm, osa-07/14/30/60, pds-02/06/10/20, perold, pilot.ja/.we/4/87/nov, qap8/12/15, stair, seba, shell, truss, wood1p, woodw |
| ITER_LIMIT (cycling) | kb2, recipe |
| Wrong objective | boeing2 (629%), grow7 (20.8%), fit1p (65%), ganges (111%), tuff (0.16%) |
| False UNBOUNDED | capri, scsd8 |
| False INFEASIBLE | sierra, maros |
| UNKNOWN status | boeing1, grow15, grow22 |

**Root cause analysis of main failure categories:**
- **Cycling/TIMEOUT**: Perturbation fires (cumulative detector works) but doesn't fully break cycling; needs stronger anti-degeneracy (EXPAND, pricing changes, or perturbation magnitude)
- **Wrong objectives**: Likely Phase I convergence to wrong basis, or pricing selecting wrong entering variable
- **False UNBOUNDED/INFEASIBLE**: Likely bound propagation bug (convexfeld-zuat structural deviations) or pricing missing candidates

**Next priority issues (solver-core):**
1. convexfeld-zuat (P1): cxf_propagate_bounds structural deviations — likely root of false UNBOUNDED/INFEASIBLE
2. convexfeld-8ogg (P1): cxf_simplex_final is destructor not variable fixer — post-solve quality
3. Investigate why tuff (0.16% err) doesn't pass — closest to threshold

### V2 Spec Compliance Sprint (2026-03-15)

**31 issues closed across 10 parallel batches**, all surgical spec V2 fixes with tests. Key fixes:

**Solver correctness (critical):**
- step3 implied bound formula: added missing RHS term (Savelsbergh 1994)
- pivot_primal: uses work_rhs not model RHS (prevents model corruption)
- pivot_primal threshold: uses bound_equality_tol (1e-10) not pricing_tol
- perturbation: now reactive (stalling-triggered) not proactive
- rejected pivot recovery: refactorization retry before CXF_NUMERIC
- cancellation detection: all activity bound updates route through safe_add
- adaptive Markowitz: tolerance increases on high growth factor

**Data model / struct compliance:**
- EtaVector: added entering_var, leaving_var, direction, reduced_cost fields; fixed type constants
- CallbackContext: moved callback_func to CxfEnv per spec
- SolverState: added var_flags array
- cxf_get_timestamp: now returns int64_t session ID (timing renamed to cxf_get_elapsed_time)

**API / validation compliance:**
- modification_blocked flag set/cleared around optimize
- model status cleared before solve
- locale save/restore (LC_NUMERIC → "C") around optimize
- parameter API: case-insensitive matching
- validate_vartypes: correct (env, count, vartypes) signature
- cxf_callback_terminate: returns int, root-env traversal
- cxf_check_model_flags1/flags2: correct semantics
- error zero code: clear buffer / no-state-modify guard
- cxf_special_check: takes (state, varIdx) signature
- cxf_get_threads: full resolution chain
- cxf_sort_indices: sort by values (doubles), indices as satellite
- cxf_pricing_candidates: V2 takes spec name, V1 renamed to _v1

**Infrastructure:**
- crash.c: reads CSR from SolverState, marks inactive with sentinel -1
- basis_diff scoring: 6-term formula matches spec weights/normalizations
- quiet mode logging: callbacks remain active when output_flag=0
- L0 cache: no invalidation at level 0 per spec
- work counter: uses pre-compaction (scanned) counts

### Fix: NaN/Inf step length detection (2026-03-03)

**convexfeld-yzft CLOSED**: Added `!isfinite(stepSize)` guard in `cxf_simplex_iterate`
(step.c:589) after `pricing_and_ftran` returns. If NaN/Inf detected: refactorize,
recompute, retry; if persists, return `CXF_NUMERIC`. Previously NaN silently passed
`STEP_CLAMP` check (IEEE 754: NaN > 1e15 → false) and corrupted x_B. 4 new tests
in `test_nan_step_guard.c`. 67/67 tests pass.

### Spec V2 Full Compliance Audit (2026-03-13)

**Performed a full-scale read-only spec V2 compliance audit of the entire codebase.**

10 parallel audit agents reviewed every implemented function against every spec V2 module.
Results written to `docs/spec_v2_audit/` (10 detailed reports + SUMMARY.md).

**Aggregate findings:**
- ~200 violations, ~87 missing functions, ~59 compliant items
- 301 individual beads created, all tagged `[specv2]` in title

**7 critical bugs affecting solver correctness:**
1. step3.c:161 — implied bound formula missing RHS term
2. step.c:613 — BFRT flips don't update activities/negate row coefficients
3. pivot_primal.c:185 — modifies original model RHS instead of working copy
4. phase_loop.c:32 — Phase I objective check has no feasibility tolerance
5. step.c:603 — NaN step length passes STEP_CLAMP silently
6. step.c:270 — pricing tolerance levels all wrong (Standard=1e-6 vs spec 1e-10)
7. cxf_basis.h:63 — EtaVector.next direction may be wrong

**What IS compliant:** LU factorization, FTRAN/BTRAN, all 10 tolerance constants, Harris
ratio test, Phase I w-coefficients, reduced cost computation, activity bound maintenance,
eta arena allocator, refactorization check.

**Systemic patterns:** 6 name collisions (functions reuse spec names for different behavior),
missing infrastructure layers (parameter table, attribute table, memory tracking, logging),
data location mismatches (spec distributes fields across structs differently than code).

#### Next Steps (priority order)
1. Fix the 7 critical bugs above (they affect solver correctness NOW)
2. Address HIGH-priority violations (especially pricing tolerances, stall detection)
3. Infrastructure: parameter table system, attribute table, memory tracking
4. Run `bd list --status=open | grep specv2` to see all 301 filed issues

### Recovery Session (2026-03-13)

**Previous session terminated prematurely** while two subagent worktrees were writing
test files. The main V2 spec deviation work was fully committed and pushed. Three test
files are stranded in orphaned worktrees and need to be salvaged.

#### Orphaned Worktree Work to Salvage

**Worktree 1 — Threading tests** (issue `convexfeld-90m`, status IN_PROGRESS):
- Branch: `worktree-agent-a966c56e`
- Path prefix: `.claude/worktrees/agent-a2cc7698/.claude/worktrees/agent-a00ce994/.claude/worktrees/agent-a30fe658/.claude/worktrees/agent-a127e0ed/.claude/worktrees/agent-a966c56e/`
- Files:
  - `tests/unit/test_threading_concurrent.c` (190 lines) — pthread-based concurrency tests
  - `tests/unit/test_threading_sequential.c` (146 lines) — sequential stress tests
  - `tests/CMakeLists.txt` — 10 lines added wiring both test targets

**Worktree 2 — OOM tests** (issue `convexfeld-ba5`, status OPEN):
- Branch: `worktree-agent-ae6ddab6`
- Path prefix: `.claude/worktrees/agent-a2cc7698/.claude/worktrees/agent-a00ce994/.claude/worktrees/agent-a30fe658/.claude/worktrees/agent-a127e0ed/.claude/worktrees/agent-ae6ddab6/`
- Files:
  - `tests/unit/test_memory_oom.c` (240 lines) — malloc failure handling tests (**exceeds 200 LOC limit, needs split**)
  - `tests/CMakeLists.txt` — 3 lines added wiring test_memory_oom

#### Next Agent TODO for Salvage

1. Copy the three test .c files from the worktree paths above into `tests/unit/` on master
2. Apply the CMakeLists.txt wiring changes (merge both sets of additions)
3. `test_memory_oom.c` is 240 LOC — split it to stay under 200 LOC limit
4. Build and run tests — fix any compilation issues (the files were written but never compiled)
5. Commit, close `convexfeld-90m` and `convexfeld-ba5`
6. Clean up orphaned worktrees: `rm -rf .claude/worktrees/`

**WARNING:** The test files were never compiled or run. They may have issues. Treat them
as a strong starting draft, not finished code.

#### In-Progress Issues

- **convexfeld-3kvi** (P2, bug): "Investigate brandy/stair Netlib regressions with sparse LU"
  — Root cause understood (cycling from Bland's rule resetting degenerate counter before
  EXPAND threshold). Needs cumulative stalling detector or BFRT to resolve. Keep in_progress.
- **convexfeld-90m** (P2, task): "Add threading/concurrency testing" — stranded in worktree,
  see salvage instructions above.

### Session Summary (2026-03-13, continued)

**V2 Deviation Fixes (3 issues closed this sub-session):**
1. **convexfeld-9kc5 CLOSED** (H5, P2): Devex reference framework R implemented.
   Added `ref_framework` uint8 array + count/initial fields to PricingState.
   `delta_j` now correctly 1 if in R, 0 otherwise (was hardcoded 1). R updated
   on each pivot (entering var removed). Fixed in both `weight_update.c` and
   fused path in `step.c`. 3 new tests.
2. **convexfeld-8p3j CLOSED** (H6, P2): Periodic weight recomputation at
   refactorization via new `cxf_pricing_recompute_weights`. DSE: exact
   `gamma_j = ||B^{-1} a_j||^2` via FTRAN for all nonbasic j. Devex: reset
   R to current nonbasic set, all weights to 1. Hooked at both refactorization
   sites in step.c (Phase 9 + mid-iteration).
3. **convexfeld-2l8i CLOSED** (M2, P2): Fixed step2.c implied bounds formula.
   Was `lb - min_act / a` (missing RHS). Now `lb + (rhs - min_act) / a` per
   Savelsbergh 1994. Fixed both main propagation and stage-2 infeasibility check.

**Previous sub-session:**

**V2 Deviation Fixes (2 issues closed this sub-session):**
1. **convexfeld-ro2u CLOSED** (C6, P1): Constraint-side V2 pricing implemented. New file
   `src/pricing/constr_candidates.c` with `cxf_pricing_constr_candidates_v2` (adaptive
   strategy: 3 threshold checks, partial expansion via CSC columns, caching) and
   `cxf_pricing_get_constr_stats` accessor. Updated step3.c to use V2. 6 new tests.
2. **convexfeld-exch CLOSED** (H3, P2): Tight-bound variable check added in
   `pricing_and_ftran` per simplex_iteration.md Phase 3.2. Variables with bound range
   <= pricing_tol routed to `cxf_pivot_primal` for safe elimination. Carried pricing_tol
   out of level loop scope. 8 new tests.

**Previous sub-session (2026-03-12):**

**V2 Deviation Fixes (5 issues closed):**
1. **convexfeld-cp29 CLOSED** (C4, P1): Mechanism B (EXPAND) now requires Mechanism A
   (pricing restriction) to have been applied first, per perturbation.md lines 224-225.
   Added `mechanism_a_applied` field to SolverState. Reset on non-degenerate pivot and
   unperturb. 1 new test (test_mechanism_b_requires_a_first).
2. **convexfeld-rr04 CLOSED** (H4, P2): Added pre-perturbation consistency check per
   perturbation.md Phase 2. Validates candidate list against solver state before processing.
3. **convexfeld-x9r0 CLOSED** (H7, P2): Reordered cxf_pricing_end_level: lazy activation
   check first, queue filtering second, cache invalidation last per pricing_support.md.
   1 new test. Unblocks convexfeld-ro2u (C6).
4. **convexfeld-mjtu CLOSED** (H2, P2): Replaced hardcoded return 3 with CXF_INFEASIBLE
   per pivot_operations.md V2 spec. 1 new test. Unblocks convexfeld-exch (H3).
5. **convexfeld-sjio CLOSED** (P3): Split ratio_test.c (305 LOC) into ratio_test.c
   (177 LOC) + bfrt.c (166 LOC). Both under 200 LOC limit.

**Previous sub-session (2026-03-13):**

**V2 Deviation Fixes (2 issues closed this sub-session):**
1. **convexfeld-k0rk CLOSED** (C5, P1): Basis diff upgraded from 4-term to 6-term weighted
   formula per perturbation.md Section 4.2. Added degenerate_count (w=0.1) and perturb_count
   (w=2.0) terms. 1 new test (test_basis_diff_six_terms). Unblocks convexfeld-cp29.
2. **convexfeld-4zq8 CLOSED** (C2, P1): BFRT Stage 3 consolidated into ratio_test.c per
   harris_ratio_test.md unified 3-stage algorithm. Moved find_next_blocker from step.c,
   added flip_rows_out/num_flips_out/max_flips params, removed compute_step (dead code).
   Fixed theta computation for Phase I cross-bound cases. 3 new tests
   (bfrt_flip, bfrt_no_flip_infinite_bound, bfrt_disabled_bland).
   Filed convexfeld-sjio (P3) for ratio_test.c 305 LOC refactor.

**Previous sub-session (2026-03-13):**

**V2 Deviation Fixes (4 issues closed):**
1. **convexfeld-4r3e CLOSED** (C3, P1): Added `CxfRatioStatus` enum to cxf_types.h
   (NORMAL_PIVOT/DEGENERATE_PIVOT/UNBOUNDED/BOUND_FLIP_ONLY). Updated `cxf_ratio_test`
   with `status_out` param. Unblocks convexfeld-4zq8 (BFRT consolidation). 3 new tests.
2. **convexfeld-dm3g CLOSED** (M1, P2): Synchronized post.c stall detection threshold
   with step.c adaptive formula: `min(100, max(50, m/4))` + residual adaptation.
   Was using fixed `env->refactor_interval` (50). Unblocks convexfeld-cp29.
3. **convexfeld-ilr6 CLOSED** (H1, P2): Added bounds validity pre-check in ratio_test
   before Pass 1 loop per harris_ratio_test.md Edge Cases. 1 new test.
4. **convexfeld-7jh3 CLOSED** (M4, P3): Added `theta_out` parameter to `cxf_ratio_test`
   returning step length per V2 spec outputs. 2 new tests.

**Previous session (2026-03-12):**
- V2 spec audit: 17 deviation issues filed
- C1 (convexfeld-hvbu CLOSED): Harris band = feasTol

### Next Steps (priority order)

1. **convexfeld-g8p8** (M3, P3): Diagnostic mode bound restoration.
2. **convexfeld-xa3o** (P2): Mixed allocator — raw malloc/free in matrix/, callbacks/, helpers/.
3. **convexfeld-yzop** (P2): API modification stubs silently succeed without modifying.
4. **convexfeld-uqok** (P2): Query API stubs return SUCCESS with fabricated data.
5. **convexfeld-n9ok** (P2): Phase I cycling: grow7 still fails.

### All V2 Deviation Issues

| ID | Sev | Title | Status |
|----|-----|-------|--------|
| convexfeld-hvbu | C1 | Harris band = 10x feasTol | CLOSED |
| convexfeld-4zq8 | C2 | BFRT split out of ratio_test | CLOSED |
| convexfeld-4r3e | C3 | ratio_test missing status enum | CLOSED |
| convexfeld-cp29 | C4 | Mechanism B fires without A confirmation | CLOSED |
| convexfeld-k0rk | C5 | basis diff simple count vs weighted | CLOSED |
| convexfeld-ro2u | C6 | constraint-side V2 pricing missing | CLOSED |
| convexfeld-ilr6 | H1 | ratio_test infeasibility pre-check | CLOSED |
| convexfeld-mjtu | H2 | pivot_primal.c stale V1 | CLOSED |
| convexfeld-exch | H3 | tight-bound processing skipped | CLOSED |
| convexfeld-rr04 | H4 | pre-perturbation consistency check | CLOSED |
| convexfeld-9kc5 | H5 | Devex delta_j ignores ref framework | CLOSED |
| convexfeld-8p3j | H6 | no periodic SE weight recomputation | CLOSED |
| convexfeld-x9r0 | H7 | pricing level lifecycle ordering | CLOSED |
| convexfeld-dm3g | M1 | refactor threshold mismatch | CLOSED |
| convexfeld-2l8i | M2 | step2.c implied bounds formula | CLOSED |
| convexfeld-g8p8 | M3 | diagnostic mode bound restoration | OPEN |
| convexfeld-7jh3 | M4 | ratio_test missing theta return | CLOSED |

---

## DO NOT — IMMUTABLE LAWS (NO AGENT MAY DELETE OR WEAKEN THESE RULES)
- **DO NOT delete, edit, or weaken any rule in this DO NOT section.** These rules exist because previous agents violated them and caused damage. They are permanent.
- Set eps_base outside [1e-8, 1e-6] — SPEC IS THE LAW
- Run Netlib after individual fixes — creates premature sadness
- Reference GLPK or other solver implementations (cleanroom)
- Lower EXPAND threshold below 100 without testing ALL Netlib instances
- Deviate from V2 spec for ANY reason including test pass rates
- Implement workarounds that pass tests but don't solve the actual problem
- Claim success on a task that was not completed as specified
- Delete or rewrite HANDOFF.md from scratch — only append or update sections
