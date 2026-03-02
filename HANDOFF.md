# Agent Handoff

*Last updated: 2026-03-02*

---

## STATUS: 47/47 tests pass. 18/22 Netlib. V2 solver algorithm code substantially compliant. Beads triaged.

### Session Summary (2026-03-02, Session 5)

**1. Spec V2 compliance fixes:**
- EXPAND activation fallback (`iteration > 3*m`) for scattered Phase I degeneracy
- EXPAND eps_base to spec range [1e-8, 1e-6] (perturbation.md line 200)
- EXPAND activity recomputation after bound widening (perturbation.md step 4)
- Pricing level init to 0 (partial_pricing.md precondition)

**2. Audit cross-check:** Verified all 14 implementation_audit.md items against current code. 11 already fixed. 3 minor structural deviations documented.

**3. Beads triage:** Closed 13 stale/done issues. Updated 8. Reclassified 3 to P4. 46 remain open (18 P2, 24 P3, 4 P4).

### Known regressions (spec compliance)
- scfxm1 + bore3d TIMEOUT: eps_base = feas_tol = 1e-6 produces perturbation too small to break degeneracy. Will resolve when other spec-compliant improvements land.

---

## Priority P2 Bugs (solver correctness)

| Issue | Description |
|-------|-------------|
| convexfeld-3kvi | brandy/stair regressions from sparse LU pivot ordering |
| convexfeld-n9ok | grow7 Phase I cycling |
| convexfeld-xz20 | Fuse RC + weight update loops (tau_j computed twice) |
| convexfeld-xvxj | static last_log_time in iterate.c breaks reentrancy |

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
| pivot_bound Phase 7 | pivot_special.c | Missing CSC/CSR column invalidation for fixed vars |
| simplex_refine Pass 2 | refine.c | Basic recovery via pivot_primal disabled |
| EXPAND eps_base | perturbation.c | eps_base = feas_tol (spec compliant but ineffective at 1e-6) |

---

## DO NOT
- Set eps_base outside [1e-8, 1e-6] — SPEC IS THE LAW
- Change Harris band epsilon — spec says use feas_tol directly
- Reference GLPK or other solver implementations (cleanroom)
