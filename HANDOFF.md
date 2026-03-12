# Agent Handoff

*Last updated: 2026-03-13*

---

## STATUS: 58/58 tests pass (19 ratio_test, 11 basis_progress). 7 V2 deviation issues closed this session (23 total closed).

### Session Summary (2026-03-13, continued)

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

### Dependency Graph (unblocked → blocked)

```
READY                                    BLOCKED
─────                                    ───────
convexfeld-cp29  Mechanism A/B seq       (was blocked by k0rk+dm3g, NOW READY)
convexfeld-x9r0  level lifecycle ──────→ convexfeld-ro2u  constraint-side pricing
convexfeld-mjtu  pivot_primal V2 ──────→ convexfeld-exch  tight-bound processing
```

### Next Steps (priority order)

1. **convexfeld-cp29** (P1): EXPAND Mechanism B fires without confirming Mechanism A
   failed. NOW UNBLOCKED by basis diff fix (k0rk) and threshold sync (dm3g).
2. **convexfeld-x9r0** (P2): Fix pricing level lifecycle ordering in queue.c. Unblocks
   C6 (constraint-side V2 pricing).
3. **convexfeld-mjtu** (P2): Rewrite pivot_primal.c to V2 spec. Unblocks H3.
4. **convexfeld-2l8i** (P2): step2.c implied bounds formula — needs investigation of
   activity bounds representation before fixing.
5. **convexfeld-sjio** (P3): Refactor ratio_test.c from 305 LOC to < 200.

### All V2 Deviation Issues

| ID | Sev | Title | Status |
|----|-----|-------|--------|
| convexfeld-hvbu | C1 | Harris band = 10x feasTol | CLOSED |
| convexfeld-4zq8 | C2 | BFRT split out of ratio_test | CLOSED |
| convexfeld-4r3e | C3 | ratio_test missing status enum | CLOSED |
| convexfeld-cp29 | C4 | Mechanism B fires without A confirmation | READY (was blocked by C5+M1) |
| convexfeld-k0rk | C5 | basis diff simple count vs weighted | CLOSED |
| convexfeld-ro2u | C6 | constraint-side V2 pricing missing | BLOCKED by H7 |
| convexfeld-ilr6 | H1 | ratio_test infeasibility pre-check | CLOSED |
| convexfeld-mjtu | H2 | pivot_primal.c stale V1 | OPEN |
| convexfeld-exch | H3 | tight-bound processing skipped | BLOCKED by H2 |
| convexfeld-rr04 | H4 | pre-perturbation consistency check | OPEN |
| convexfeld-9kc5 | H5 | Devex delta_j ignores ref framework | OPEN |
| convexfeld-8p3j | H6 | no periodic SE weight recomputation | OPEN |
| convexfeld-x9r0 | H7 | pricing level lifecycle ordering | OPEN |
| convexfeld-dm3g | M1 | refactor threshold mismatch | CLOSED |
| convexfeld-2l8i | M2 | step2.c implied bounds formula | OPEN |
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
