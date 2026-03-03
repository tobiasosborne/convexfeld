# Agent Handoff

*Last updated: 2026-03-03*

---

## STATUS: 58/58 tests pass. 18/22 Netlib. 17 issues closed this session.

### Session Summary (2026-03-03, Sessions 6-7)

**Solver improvements (3 issues):**
1. Relative Markowitz tie-breaking (sparse_elim.c, lu_factorize.c, dense_elim.c) — `av/col_max` per Suhl & Suhl 1990
2. Phase II EXPAND enablement (perturbation.c → expand.c) — removed Phase I guard per spec
3. Fused RC + weight update (step.c) — `compute_tau()` kernel, single-pass loop

**Bug fixes (2 issues):**
4. Reentrancy fix — moved static `last_log_time` into SolverState
5. Introsort — replaced O(n²) insertion sort with O(n log n) introsort in sort.c

**Refactors via 3 parallel waves (12 issues, 15 subagents, worktree isolation):**

| Wave | Files Refactored |
|------|-----------------|
| 1 | test_basis.c(1053→7), test_logging.c(300→2), test_error.c(547→3), lu_factorize.c(318→180+95+78), phase_steps.c(327→191+183) |
| 2 | sparse_elim.c(243→192), perturbation.c(370→190+180), candidates.c(323→171+163), lock naming(4 renames), CMake sanitizers |
| 3 | test_matrix.c(442→3), update.c(212→164+62), model.c(281→187+107), btran.c(311→121+163), sort.c(80→167 introsort) |

### Known regressions (pre-existing)
- scfxm1 + bore3d TIMEOUT: eps_base = feas_tol = 1e-6 too small for degeneracy
- brandy/stair/kb2: sparse LU trajectory + insufficient anti-cycling

### brandy/stair/kb2 root cause (investigated):
- NOT Markowitz tie-breaking. Root cause: Bland's rule (at degenerate_count > 50) resets consecutive counter, preventing EXPAND threshold (100). Fix needs cumulative stalling detector or BFRT.

---

## Priority P2 Bugs (solver correctness)

| Issue | Description |
|-------|-------------|
| convexfeld-3kvi | brandy/stair/kb2 cycling — needs cumulative stall detector or BFRT |
| convexfeld-n9ok | grow7 Phase I cycling |

## Priority P2 Tasks (remaining)

| Issue | Description |
|-------|-------------|
| convexfeld-xa3o | Mixed allocator (raw malloc in matrix/, callbacks/) |
| convexfeld-yzop | API modification stubs silently succeed |
| convexfeld-uqok | Query API stubs return fabricated data |
| convexfeld-ysof | Test infeasibility on 29 Netlib infeasible instances |
| convexfeld-86h | Numerical stability edge case testing |

## Remaining spec deviations (minor)

| Item | Location | Nature |
|------|----------|--------|
| C4 pivot_special | pivot_special.c | ALREADY FIXED |
| pivot_bound Phase 7 | pivot_special.c | Missing CSC/CSR column invalidation for fixed vars |
| simplex_refine Pass 2 | refine.c | Basic recovery via pivot_primal disabled |
| EXPAND eps_base | expand.c | eps_base = feas_tol (spec compliant but ineffective at 1e-6) |
| Stalling detector | step.c / expand.c | Consecutive count resets; needs cumulative |

---

## DO NOT
- Set eps_base outside [1e-8, 1e-6] — SPEC IS THE LAW
- Change Harris band epsilon — spec says use feas_tol directly
- Reference GLPK or other solver implementations (cleanroom)
- Lower EXPAND threshold below 100 without testing ALL Netlib instances (caused regressions at 50)
