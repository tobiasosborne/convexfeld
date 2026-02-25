# Agent Handoff

*Last updated: 2026-02-25*

---

## STATUS: 24/35 Netlib pass. 46/46 unit tests. convexfeld-ic80 closed.

### Scorecard

**PASS (24):** afiro, sc50b, sc105, share2b, israel, adlittle, blend, lotfi, beaconfd, stocfor1, ship04l, scfxm1, standata, standgub, standmps, scagr7, sctap1, brandy, scorpion, kb2, stair, e226, recipe, scagr25

**FAIL (11):** etamacro (0.016%), boeing2 (2.4%), bore3d (7.5%), finnis (7.1%), capri (10%), grow7 (12.5%), boeing1 (18%), vtp.base (20%), forplan (43%), scsd1 (TIMEOUT), bandm (TIMEOUT), tuff (TIMEOUT)

### DO NOT run Netlib suite in CI/agent sessions — it is slow. Run manually if needed.

---

## Work Completed This Session (2026-02-25)

### convexfeld-ic80: Phase I→II constraint cleanup — CLOSED

Two bugs fixed in the Phase I→II transition per `two_phase_method.md` Transition Steps 3-4:

#### Bug 1: Inactive constraint slack formula was wrong (post.c)

`cxf_simplex_phase_end` computed constraint slack as `-max_activity`. Our activity bounds represent `sum a_j x_j` range **without RHS subtraction**, so `-max_activity` is NOT `rhs - max(a^T x)`. This made the constraint cleanup a complete no-op — no constraint was ever identified as inactive.

**Fix:** Sense-aware slack computation using RHS:
- `<=` constraints: `slack = rhs - max_activity`
- `>=` constraints: `slack = min_activity - rhs`
- `=` constraints: always active, skip

Also removed `cols_eliminated++` from the cleanup path. This counter feeds stall detection thresholds in `post_iterate`. The old (broken) formula never incremented it, but the corrected formula flags many genuinely inactive constraints, which would have caused false stall detection and premature perturbation.

**Changes:**
- **post.c:138-192**: Rewrote inactive constraint detection with sense-aware, RHS-relative slack computation

#### Bug 2: Stale activity bounds at Phase I→II transition (phase_one.c)

`cxf_transition_to_phase_two` didn't recompute activity bounds after unperturbation and bound restoration. Phase I perturbation (EXPAND widening) and bound propagation leave activity bounds stale. The first Phase II `phase_end` call would operate on inaccurate data.

**Fix:** Added `cxf_compute_activity_bounds(state, 0, NULL)` after the pricing reset in `cxf_transition_to_phase_two`.

**Changes:**
- **phase_one.c:278-287**: Fresh activity bound recomputation at transition

#### Tests: 13 new tests in test_phase_end.c

- `<=` inactive (large slack), active (tight), active (near-tight)
- `>=` inactive (large slack), active (tight)
- Equality never inactive
- Infinite activity skipped
- Basic slack constraint skipped
- Mixed senses — only truly inactive flagged
- Free variable dual infeasibility detection
- Null argument handling

**Changes:**
- **tests/unit/test_phase_end.c**: New file, 13 tests
- **tests/CMakeLists.txt**: Registered new test target

---

## Priority Fix Order (remaining)

| Priority | What | Issues | Impact |
|----------|------|--------|--------|
| P1 | Create internal headers | convexfeld-mxjm | Unblocks 6 issues |
| P1 | Flesh out pivot_special | convexfeld-lmkg | Unblocks BFRT |
| P2 | Sparse LU (M2) | new issue needed | Fixes timeouts + grow7/boeing1 |
| P2 | V1 pricing weight update | convexfeld-l0ca | Pricing quality |
| P2 | Stall detection (post_iterate) | convexfeld-5z94 | Convergence |

---

## Known Issue: Activity Bounds Don't Include RHS

**Discovery during this session:** `cxf_compute_activity_bounds` (setup.c) initializes activity accumulators to 0 and accumulates `a_j * bound_j` products. The spec says accumulators should initialize with `-rhs_i`. This means `min_activity`/`max_activity` represent the range of `a^T x`, NOT `a^T x - b`.

The bound propagation formulas in step2/step3 use `impl_ub = lb - min_act/a`, which derives from assuming activity DOES include `-rhs`. This formula is correct only when `rhs = 0`.

**Impact:** Bound propagation (step2/step3) produces slightly wrong implied bounds for constraints with nonzero RHS. This may contribute to numerical issues but hasn't been root-caused to specific failures yet.

**Fix:** Either subtract RHS during activity initialization in `cxf_compute_activity_bounds`, or adjust the step2/step3 formulas to add `rhs/a`. Filed as separate investigation — don't mix with ic80.

---

## DO NOT
- Enable scaling without testing H1 (Harris tolerance) first
- Change diag_coeff based on RHS sign (causes regressions on israel/stair/e226)
- Re-add RC-based status reassignment in refine.c (corrupts obj)
- Re-add recovery pivots in refine.c Pass 2 (changes basis during post-solve)
- Hack refactorization parameters to fix primal accuracy — needs sparse LU
- Skip reading this file and `docs/learnings/implementation_audit.md`
- Use `cols_eliminated` counter for constraint cleanup — it feeds stall detection
