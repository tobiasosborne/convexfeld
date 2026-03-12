# Agent Handoff

*Last updated: 2026-03-13*

---

## STATUS: 61/61 tests pass. 16 V2 deviation issues closed across sessions (32 total closed).

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
