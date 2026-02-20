# Agent Handoff

*Last updated: 2026-02-20*

---

## STATUS: `d1th` DONE — pricing tolerance escalation implemented per v2 P2.3/P3.20

### Session Summary

**Implemented multi-level pricing tolerance escalation** in `cxf_simplex_step()`. This was the #1 item in the critical path for 7 false INFEASIBLE Netlib failures. Previously, when pricing found 0 candidates at the loose tolerance (level 0), the function immediately returned ITERATE_OPTIMAL. Now it escalates through 3 tolerance tiers before declaring optimality.

### Changes This Session

| File | Change | Spec Basis |
|------|--------|------------|
| `step.c` lines 280-359 | Wrap pricing Phases 1+2 in level escalation loop (0→1→2) | P2.3 Phase 5 + P3.20 Phase 1-2 |

**Detail:** The pricing scan in `cxf_simplex_step` now loops over 3 levels with decreasing tolerance thresholds:
- Level 0 (loose): `optimality_tol * 10` — only strongly attractive RCs
- Level 1 (standard): `optimality_tol` — moderately attractive RCs
- Level 2 (tight): `optimality_tol * 0.1` — catches weak RCs near zero

At each failed level, calls `cxf_pricing_end_level()` and increments `level_escalations` counter. Only declares ITERATE_OPTIMAL when ALL 3 levels return 0 candidates. All 3 pricing paths (Bland, pricing subsystem, fallback scan) benefit from escalation.

### Issues Closed

| ID | Title |
|----|-------|
| `d1th` | P2.3: multi-level pricing tolerance escalation incomplete |

### Test Results

- **39/39 unit tests pass**
- No regressions

---

## Next Steps (Strict V2 Order)

### Critical Path for 7 False INFEASIBLE (updated)

1. ~~`d1th` — Pricing tolerance escalation (P2.3)~~ **DONE**

2. **`zr5l` — Proactive perturbation (P2.6)** — Apply perturbation in first 1-2 inner iterations of round 0. Challenge: Mechanism B (nonbasic flipping) must not flip all original variables before meaningful RCs exist. May need to split perturbation into basic-only (proactive) and full (reactive).

3. **`fiyt` — phase_end in Phase I (P3.21)** — Currently phase_end only runs during Phase II. Spec says it runs both phases. Its constraint activity analysis could detect Phase I→II transition earlier.

4. **`x5dj` — LU accuracy (P3.21)** — The correct_basic_variables hack exists because LU gives inaccurate x_B. Better LU (already tracked as `uxae`) would eliminate this workaround and give accurate infeasibility measurement.

### Already Tracked

- `snwu` — Crash basis (P2.5) — better initial basis reduces Phase I iterations
- `1azn` — EXPAND perturbation — tracked separately
- `uxae` — LU factorization performance/accuracy
- `a5vp` — P2.6 perturbation Phase 2 candidates from pricing subsystem
- `9yi2` — P2.6 perturbation Phase 3 bound restoration before analysis

---

## Key Architectural Insight (from previous session)

The 7 false INFEASIBLE failures are caused by **multiple missing spec components working together**: inadequate crash basis, missing proactive perturbation, pricing tolerance escalation (now fixed), phase_end not participating in Phase I, and inaccurate LU factorization. These v2 defense layers must ALL be present for robust Phase I.

---

## File Locations

| Item | Path |
|---|---|
| step.c (pricing + BFRT + **escalation**) | `src/simplex/step.c` |
| perturbation.c (EXPAND, P2.6) | `src/simplex/perturbation.c` |
| phase_loop.c (Phase I/II loops) | `src/simplex/phase_loop.c` |
| solve_lp.c (v2 orchestrator) | `src/simplex/solve_lp.c` |
| candidates.c (pricing candidates) | `src/pricing/candidates.c` |
| queue.c (pricing levels) | `src/pricing/queue.c` |
| reduced_costs.c (RC computation) | `src/simplex/reduced_costs.c` |
| V2 specs | `docs/specs-v2/specs/modules/` |
| V2 algorithm specs | `docs/specs-v2/specs/algorithms/` |
