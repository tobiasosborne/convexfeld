# Agent Handoff

*Last updated: 2026-03-03*

---

## STATUS: 56/56 tests pass. 18/22 Netlib. 12 issues closed this session.

### Session Summary (2026-03-03, Session 7)

**4. Fused RC + weight update** (step.c, convexfeld-xz20 CLOSED):
- Factored `compute_tau(state, j, rho)` — CSC dot product appears once.
- `update_rc_and_weights()` fuses both loops into single pass over nonbasic vars.
- Phase II + BTRAN + weights active → fused path. Otherwise separate paths.
- Zero new allocations, zero new data structures.
- Spec basis: revised_simplex.md Step 6 — both formulas share tau_j.

**5. Reentrancy fix** (iterate.c, cxf_solver.h, convexfeld-xvxj CLOSED):
- Moved static `last_log_time` into SolverState. Thread-safe, no stale timing.

**6-10. Five parallel refactors** (5 subagents, worktree isolation, zero conflicts):
- convexfeld-4vl9 CLOSED: test_basis.c 1053→7 files (90-198 LOC each)
- convexfeld-5w6 CLOSED: test_logging.c 300→2 files (154+180)
- convexfeld-afb CLOSED: test_error.c 547→3 files (191+185+157)
- convexfeld-0x54 CLOSED: lu_factorize.c 318→180 + dense_elim.c(95) + lu_output.c(78)
- convexfeld-rlll CLOSED: phase_steps.c 327→step2.c(191) + step3.c(183)

**11-15. Five more parallel refactors** (5 subagents, worktree isolation):
- convexfeld-nt3i CLOSED: sparse_elim.c 243→192 (moved sparse_to_dense to sparse_work.c)
- convexfeld-h343 CLOSED: perturbation.c 370→190 + expand.c(180)
- convexfeld-p3sl CLOSED: candidates.c 323→171 + candidates_v1.c(163)
- convexfeld-ccrf CLOSED: lock naming fix (4 functions renamed to consistent pairs)
- convexfeld-yyo6 CLOSED: CMake sanitizer support (-DSANITIZER=address|undefined|thread|memory)

### Session Summary (2026-03-03, Session 6)

**1. Relative Markowitz tie-breaking** (sparse_elim.c, lu_factorize.c):
- Changed tie-breaking from absolute magnitude (`av > best_abs`) to relative
  stability (`av/col_max > best_rel`), per Suhl & Suhl (1990).
- Scale-independent: a 0.01 pivot in a col with max 0.01 (ratio 1.0) now beats
  a 100.0 pivot in a col with max 100000 (ratio 0.001).
- Same change in both sparse-phase and dense-phase pivot selection.
- Updated test_sparse_lu.c test expectations accordingly.

**2. Phase II EXPAND enablement** (perturbation.c):
- Removed `state->phase == 1` guard from EXPAND activation.
- Spec (perturbation.md §Phase 5) says "stalling persists → A + B" without
  phase restriction. Phase II uses existing unperturb + refine at optimality.
- Kept original conservative thresholds (degenerate_count > 100 primary,
  iteration > 3*m fallback) — lower thresholds caused regressions.

**3. Investigation: brandy/stair/kb2 cycling root cause:**
- Cycling is NOT caused by Markowitz tie-breaking.
- Root cause: Bland's rule (activated at degenerate_count > 50) produces
  occasional non-degenerate steps that reset the consecutive counter,
  preventing it from reaching EXPAND's threshold (100).
- The fallback (iteration > 3*m) also fails when the most recent step is
  non-degenerate (degenerate_count = 0).
- Fix requires: cumulative stalling detector that doesn't reset on
  individual non-degenerate steps, or BFRT implementation.

### Known regressions (pre-existing)
- scfxm1 + bore3d TIMEOUT: eps_base = feas_tol = 1e-6 too small for degeneracy
- brandy/stair/kb2: sparse LU trajectory + insufficient anti-cycling

---

## Priority P2 Bugs (solver correctness)

| Issue | Description |
|-------|-------------|
| convexfeld-3kvi | brandy/stair/kb2 cycling — needs cumulative stall detector or BFRT |
| convexfeld-n9ok | grow7 Phase I cycling |
| ~~convexfeld-xz20~~ | ~~Fuse RC + weight update loops~~ **CLOSED** |
| ~~convexfeld-xvxj~~ | ~~static last_log_time breaks reentrancy~~ **CLOSED** |

## Priority P2 Tasks (quality/testing)

| Issue | Description |
|-------|-------------|
| convexfeld-xa3o | Mixed allocator (raw malloc in matrix/, callbacks/) |
| convexfeld-yzop | API modification stubs silently succeed |
| convexfeld-uqok | Query API stubs return fabricated data |
| convexfeld-yyo6 | CMake sanitizer support (ASan/UBSan/TSan) |
| convexfeld-ysof | Test infeasibility on 29 Netlib infeasible instances |
| convexfeld-4vl9 | Refactor test_basis.c (947 LOC) |
| convexfeld-86h | Numerical stability edge case testing |

## Remaining spec deviations (minor)

| Item | Location | Nature |
|------|----------|--------|
| C4 pivot_special | pivot_special.c | ALREADY FIXED: Phase I suppression + equality scan present |
| pivot_bound Phase 7 | pivot_special.c | Missing CSC/CSR column invalidation for fixed vars |
| simplex_refine Pass 2 | refine.c | Basic recovery via pivot_primal disabled |
| EXPAND eps_base | perturbation.c | eps_base = feas_tol (spec compliant but ineffective at 1e-6) |
| Stalling detector | step.c / perturbation.c | Consecutive count resets on non-degenerate steps; needs cumulative |

---

## DO NOT
- Set eps_base outside [1e-8, 1e-6] — SPEC IS THE LAW
- Change Harris band epsilon — spec says use feas_tol directly
- Reference GLPK or other solver implementations (cleanroom)
- Lower EXPAND threshold below 100 without testing ALL Netlib instances (caused regressions at 50)
