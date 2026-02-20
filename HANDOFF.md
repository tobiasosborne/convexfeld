# Agent Handoff

*Last updated: 2026-02-20*

---

## STATUS: Strict v2 spec audit — removed bandaids, registered 6 spec-gap issues

### Session Summary

**Deep research round + strict v2 audit.** Investigated the 7 false INFEASIBLE Netlib failures. Attempted multiple fix approaches (recovery loops, direct bound perturbation, proactive perturbation, tolerance tightening) — all were bandaids that don't match the spec. Reverted everything non-compliant. Applied only 2 spec-compliant fixes. Registered 6 issues for the real spec gaps that cause the failures.

### Key Insight

The 7 false INFEASIBLE failures are NOT caused by bugs in perturbation.c or solve_lp.c. Those functions are mostly correct per spec. The failures are caused by **multiple missing spec components working together**: inadequate crash basis, missing proactive perturbation, no pricing tolerance escalation, phase_end not participating in Phase I, and inaccurate LU factorization. Fixing any one component alone won't help — the v2 defense layers must ALL be present.

### Changes This Session

| File | Change | Spec Basis |
|------|--------|------------|
| `phase_loop.c` | Remove 0.01 tolerance floor in Phase I check | P3.21: use feasibility_tol |
| `perturbation.c` | Mark degenerate basic vars dirty in pricing | P2.6 Case B: remove from pricing |

### Issues Created

| ID | Priority | Title |
|----|----------|-------|
| `a5vp` | P2 | P2.6: perturbation Phase 2 should retrieve candidates from pricing subsystem |
| `9yi2` | P2 | P2.6: perturbation Phase 3 bound restoration before analysis |
| `x5dj` | P1 | P3.21: correct_basic_variables should use basis factorization not manual iteration |
| `zr5l` | P1 | P2.6: proactive perturbation in early iterations not implemented |
| `fiyt` | P2 | P3.21: phase_end not participating in Phase I transition detection |
| `d1th` | P1 | P2.3: multi-level pricing tolerance escalation incomplete |

### Test Results

- **39/39 unit tests pass**
- Netlib status unchanged (the 2 changes are correctness fixes, not behavior changes for the failing problems)

---

## Next Steps (Strict V2 Order)

### Critical Path for 7 False INFEASIBLE

The failures need multiple spec components implemented together. Recommended order based on spec dependencies:

1. **`d1th` — Pricing tolerance escalation (P2.3)** — When step() finds 0 candidates at level 0 (loose), try level 1 (standard), then level 2+ (tight). Currently returns ITERATE_OPTIMAL immediately. This alone could unblock some Phase I problems where improving RCs exist but are below the loose tolerance.

2. **`zr5l` — Proactive perturbation (P2.6)** — Apply perturbation in first 1-2 inner iterations of round 0. Challenge: Mechanism B (nonbasic flipping) must not flip all original variables before meaningful RCs exist. May need to split perturbation into basic-only (proactive) and full (reactive).

3. **`fiyt` — phase_end in Phase I (P3.21)** — Currently phase_end only runs during Phase II. Spec says it runs both phases. Its constraint activity analysis could detect Phase I→II transition earlier.

4. **`x5dj` — LU accuracy (P3.21)** — The correct_basic_variables hack exists because LU gives inaccurate x_B. Better LU (already tracked as `uxae`) would eliminate this workaround and give accurate infeasibility measurement.

### Already Tracked

- `snwu` — Crash basis (P2.5) — better initial basis reduces Phase I iterations
- `1azn` — EXPAND perturbation — tracked separately
- `uxae` — LU factorization performance/accuracy

---

## Lessons Learned This Session

### Don't bandaid the orchestrator — fix the components

Spent significant time adding "recovery loops" to solve_lp.c (refactorize → perturbation → Bland → tolerance tightening). None of it is in the spec. The spec says: if Phase I reaches optimality with infeasibility > 0, it's INFEASIBLE. The real issue is that Phase I shouldn't be reaching false optimality in the first place — that's caused by missing components (pricing escalation, proactive perturbation, phase_end participation).

### The spec's perturbation is candidate removal, not bound modification

P2.6 explicitly says: "Instead of perturbing bounds globally, the algorithm removes individual degenerate candidates from the pricing set. This is equivalent to perturbation." Direct bound modification (the "Mechanism A" I tried) is NOT in the spec and caused test regressions.

### The 0.01 tolerance floor was hiding real issues

`fmax(env->feasibility_tol, 0.01)` let problems with residual infeasibility up to 0.01 transition to Phase II. Some currently-passing problems may rely on this. Removing it is correct per spec — the tolerance should be `env->feasibility_tol`.

---

## File Locations

| Item | Path |
|---|---|
| perturbation.c (EXPAND, P2.6) | `src/simplex/perturbation.c` |
| phase_loop.c (Phase I/II loops) | `src/simplex/phase_loop.c` |
| solve_lp.c (v2 orchestrator) | `src/simplex/solve_lp.c` |
| step.c (pricing + BFRT) | `src/simplex/step.c` |
| candidates.c (pricing candidates) | `src/pricing/candidates.c` |
| queue.c (pricing levels) | `src/pricing/queue.c` |
| reduced_costs.c (RC computation) | `src/simplex/reduced_costs.c` |
| V2 specs | `docs/specs-v2/specs/modules/` |
| V2 algorithm specs | `docs/specs-v2/specs/algorithms/` |
