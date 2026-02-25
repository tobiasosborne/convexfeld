# Agent Handoff

*Last updated: 2026-02-25*

---

## STATUS: 24/35 Netlib pass. 46/46 unit tests. convexfeld-4nrf closed.

### Scorecard

**PASS (24):** afiro, sc50b, sc105, share2b, israel, adlittle, blend, lotfi, beaconfd, stocfor1, ship04l, scfxm1, standata, standgub, standmps, scagr7, sctap1, brandy, scorpion, kb2, stair, e226, recipe, scagr25

**FAIL (11):** etamacro (0.016%), boeing2 (2.4%), bore3d (7.5%), finnis (7.1%), capri (10%), grow7 (12.5%), boeing1 (18%), vtp.base (20%), forplan (43%), scsd1 (TIMEOUT), bandm (TIMEOUT), tuff (TIMEOUT)

### DO NOT run Netlib suite in CI/agent sessions — it is slow. Run manually if needed.

---

## Work Completed This Session (2026-02-25)

### convexfeld-4nrf: Activity bounds RHS initialization — CLOSED

Root cause fix for bound propagation: `cxf_compute_activity_bounds` (setup.c)
initialized accumulators with `0` instead of `-rhs_i` per the spec. The
step2/step3 implied bound formulas (`lb - min_act/a`, `ub - max_act/a`)
were derived assuming activity represents `a^T x - b`, so they produced
wrong implied bounds whenever `rhs != 0`.

**Changes:**

- **setup.c:36-63**: Initialize activity accumulators with `-rhs_i` instead
  of `0`. Both `do_all` and selective paths now read `state->work_rhs`.
  The final values represent `a^T x - b` (zero = constraint exactly satisfied).

- **post.c:150-163**: Simplified phase_end slack formula. Since RHS is now
  embedded in activity, slack is just `-max_activity` for `<=` and
  `min_activity` for `>=`. No explicit `rhs` variable needed.

- **tests/unit/test_phase_end.c**: Updated all test data to use RHS-inclusive
  activity values (e.g., `max_act = -90` instead of `10` for a constraint with
  `rhs=100, max(a^T x)=10`).

### convexfeld-ic80: Phase I→II constraint cleanup — CLOSED (earlier)

See git log for details.

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

## DO NOT
- Enable scaling without testing H1 (Harris tolerance) first
- Change diag_coeff based on RHS sign (causes regressions on israel/stair/e226)
- Re-add RC-based status reassignment in refine.c (corrupts obj)
- Re-add recovery pivots in refine.c Pass 2 (changes basis during post-solve)
- Hack refactorization parameters to fix primal accuracy — needs sparse LU
- Skip reading this file and `docs/learnings/implementation_audit.md`
- Use `cols_eliminated` counter for constraint cleanup — it feeds stall detection
