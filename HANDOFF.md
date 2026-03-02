# Agent Handoff

*Last updated: 2026-03-02*

---

## STATUS: 47/47 tests pass. 18/22 Netlib (scfxm1+bore3d regressed from spec compliance). V2 solver algorithm code substantially compliant.

### Session Summary (2026-03-02, Session 5)

**Spec V2 compliance audit + fixes:**

1. **EXPAND Mechanism B activation** (perturbation.c): Added `iteration > 3*m` fallback for Phase I stalling. Old threshold `degenerate_count > 100` was unreachable for scattered degeneracy.

2. **EXPAND eps_base** (perturbation.c): Fixed to spec range [1e-8, 1e-6] per perturbation.md line 200. Known regressions: scfxm1 + bore3d (perturbation too small with feas_tol = 1e-6).

3. **EXPAND activity recomputation** (perturbation.c): Added `cxf_simplex_setup(state, env, 0, NULL)` after EXPAND bound widening per spec step 4.

4. **Pricing level init** (init.c, context.c): Fixed `current_level = 1` to `current_level = 0` per partial_pricing.md precondition.

5. **Audit cross-check**: Verified all 14 audit items against current code. Most are fixed since the Feb 22 audit: C1+C3 (pivot_update API), C4 (Phase I UNBOUNDED suppression), H2 (two-stage infeasibility), H3 (convergence formula), H4 (Phase I w-coefficients), M2 (sparse LU), M6 (refine eta records).

### Remaining spec deviations (known)

| Item | Location | Nature | Impact |
|------|----------|--------|--------|
| pivot_bound Phase 7 | pivot_special.c | Missing CSC/CSR column invalidation for fixed vars | Low — fixed vars stay in matrix, processed as zero-slack |
| simplex_refine Pass 2 | refine.c | Basic recovery via pivot_primal disabled | Low — CS fix in solve_lp.c covers the behavior |
| EXPAND eps_base | perturbation.c | eps_base = feas_tol = 1e-6 (spec compliant but ineffective) | Medium — scfxm1+bore3d TIMEOUT. Fix: tighten feas_tol |

---

## Open Issues

| Issue | Priority | Notes |
|-------|----------|-------|
| convexfeld-3kvi | P2 | brandy/stair regressions (sparse LU trajectory) |
| convexfeld-n9ok | P2 | grow7 Phase I cycling |

---

## DO NOT
- Set eps_base outside [1e-8, 1e-6] — SPEC IS THE LAW
- Change Harris band epsilon — spec says use feas_tol directly
- Reference GLPK or other solver implementations (cleanroom)
