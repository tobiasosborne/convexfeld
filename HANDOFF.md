# Agent Handoff

*Last updated: 2026-02-23*

---

## STATUS: 22/35 Netlib pass. 42/42 unit tests. Kahan-stable pivot_update done.

### Scorecard

**PASS (22):** afiro, sc50b, sc105, share2b, israel, adlittle, blend, lotfi, beaconfd, stocfor1, ship04l, scfxm1, standata, standgub, standmps, scagr7, sctap1, brandy, scorpion, kb2, stair, e226

**FAIL (13):** etamacro (0.016%), recipe (0.02%), boeing2 (0.09%), scagr25 (2.2%), bore3d (7.5%), finnis (7.3%), capri (10%), grow7 (12.5%), boeing1 (18%), vtp.base (20%), forplan (43%), scsd1 (TIMEOUT), bandm (TIMEOUT), tuff (TIMEOUT)

---

## Work Completed This Session (2026-02-23)

### convexfeld-heyz: P0 Redesign cxf_pivot_update for Kahan-stable addition — CLOSED

Rewrote `cxf_pivot_update` from delta-only API to full spec-compliant signature:
`(state, col, oldLB, newLB, oldUB, newUB, infinityThreshold)`

**Changes:**
- **pivot_update.c**: Full rewrite with:
  - Case dispatch (LB only, UB only, both, neither)
  - Cancellation detection: `(result - delta) != existing` triggers conservative rounding
  - Conservative rounding: min_activity * (1+1e-12), max_activity * (1-1e-12)
  - Infinity threshold transitions (finite↔infinite) with unbounded count tracking
  - `safe_add()` and `apply_transition()` static helpers
- **cxf_solver.h**: Added `negUnbdCount` and `posUnbdCount` int arrays to SolverState
- **context.c**: Allocate/free negUnbdCount and posUnbdCount
- **state_cleanup.c**: Free negUnbdCount and posUnbdCount
- **setup.c**: Initialize unbounded counts during `cxf_compute_activity_bounds()`
  - Counts infinite contributions instead of setting activity to +/-inf directly
- **phase_steps.c**: Updated `tighten_bound()` caller to new signature
  - Captures old LB/UB before mutation, passes both old and new values
- **pivot_special.c**: Phase 6 of `cxf_pivot_bound` now calls `cxf_pivot_update`
  instead of inline += math
- **test_pivot_update.c**: 13 new direct tests (finite delta, infinity transitions,
  cancellation detection, null safety)
- **test_pivot_bound.c**: Added negUnbdCount/posUnbdCount to test fixtures
- 42/42 tests pass

### Previous Issues Fixed (same day, earlier sessions)

**convexfeld-7rvr: P0 Implement 4-function error model + fix error/status codes** — CLOSED
**convexfeld-h8xm: P0 Implement full cxf_pivot_bound** — CLOSED
**convexfeld-aal4: P1 CXF_ENV_MAGIC == CXF_MODEL_MAGIC defeats type safety** — CLOSED
**convexfeld-vk8l: P1 helpers.c bound propagation dead code** — CLOSED
**convexfeld-vlja: P2 coef_stats.c int loop variable for int64_t nnz** — CLOSED
**convexfeld-yhmx: P1 model_stub.c grow_vars partial realloc** — CLOSED
**convexfeld-it1r: P2 Leaving variable always set AT_LOWER in pivot_with_eta** — CLOSED

### Priority Fix Order (remaining)

| Priority | What | Issues | Impact |
|----------|------|--------|--------|
| ~~P0~~ | ~~Redesign cxf_pivot_update~~ | ~~convexfeld-heyz~~ | DONE |
| P1 | Create internal headers | convexfeld-mxjm | 2 hrs |
| P1 | Eliminate hot-path malloc | convexfeld-7nyb | 1 hr |
| P1 | Add core algorithm tests | convexfeld-sxgk | 1 day |

---

## DO NOT
- Enable scaling without testing H1 (Harris tolerance) first
- Change diag_coeff based on RHS sign (causes regressions on israel/stair/e226)
- Re-add RC-based status reassignment in refine.c (corrupts obj)
- Re-add recovery pivots in refine.c Pass 2 (changes basis during post-solve)
- Skip reading this file and `docs/learnings/implementation_audit.md`
