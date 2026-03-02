# Agent Handoff

*Last updated: 2026-03-02*

---

## STATUS: 47/47 tests pass. 18/22 Netlib pass. Spec V2 fully compliant.

### Session Summary (2026-03-02, Session 5)

**Spec V2 compliance fixes to EXPAND Mechanism B (perturbation.c)**

Three changes, all spec-mandated:

1. **EXPAND activation fallback** (perturbation.c): Added `iteration > 3*m` fallback alongside the existing `degenerate_count > 100` threshold. The old fallback (`degenerate_count > 3*m`) was unreachable because `degenerate_count` resets on any non-degenerate pivot. The new fallback fires when Phase I has stalled for many iterations with current degeneracy. Spec: perturbation.md "Phase I with many basic variables at bounds → Mechanism A + Mechanism B".

2. **eps_base spec compliance** (perturbation.c): Changed from `feas_tol * 1000` (= 1e-3, clamped to 1e-4) to `feas_tol` (= 1e-6, clamped to [1e-8, 1e-6]). Spec: "epsilon_base is typically on the order of 1e-6 to 1e-8."

3. **Activity recomputation after EXPAND** (perturbation.c): Added `cxf_simplex_setup(state, env, 0, NULL)` after EXPAND bound widening. Spec step 4: "Update constraint activities. Recompute constraint activity bounds."

**Known regressions (spec compliance):**
- scfxm1: TIMEOUT (was PASS at 0.09s). EXPAND perturbation too small with eps_base = 1e-6 = feas_tol.
- bore3d: TIMEOUT (was PASS at 0.013s with non-compliant eps_base). Same root cause.
- **Fix path:** Tighten feas_tol to 1e-8 (within spec range [1e-9, 1e-2]). This gives eps_base = 1e-6 = 100*feas_tol, making EXPAND effective while staying spec-compliant.

### Previous Sessions
- Session 4: EXPAND activation fallback (bore3d solved — then reverted by eps_base compliance)
- Session 3: FTRAN error recovery, step clamping, optimality verification
- Session 1: Unit tests, ratio test refactoring
- Feb 27: Sparse LU implementation

---

## Open Issues

| Issue | Priority | Notes |
|-------|----------|-------|
| convexfeld-3kvi | P2 | Investigate brandy/stair regressions with sparse LU |
| convexfeld-n9ok | P2 | Phase I cycling: grow7 still fails |
| convexfeld-nt3i | P3 | Refactor sparse_elim.c to < 200 LOC |
| convexfeld-0x54 | P3 | Refactor lu_factorize.c to < 200 LOC |

---

## Audit Items Still Open (implementation_audit.md)

| Priority | Item | Description | Effort |
|----------|------|-------------|--------|
| 1 | C1+C3 | Redesign cxf_pivot_update API + Kahan-stable addition | Medium |
| 2 | C2 | Implement full cxf_pivot_bound (eta, activity, matrix cleanup) | Medium |
| 3 | C4 | Phase I UNBOUNDED suppression in cxf_pivot_special | Low |
| 4 | H2 | Two-stage infeasibility confirmation in step2/step3 | Low |

---

## DO NOT
- Set eps_base outside spec range [1e-8, 1e-6] — SPEC IS THE LAW
- Change Harris band epsilon to 10*feas_tol — spec says use feas_tol directly
- Reference GLPK or other solver implementations (cleanroom project)
