# Agent Handoff

*Last updated: 2026-03-15*

---

## STATUS: 124/124 tests pass. 65 V2 spec compliance issues closed this session. ~249 remain. Master clean and pushed.

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
