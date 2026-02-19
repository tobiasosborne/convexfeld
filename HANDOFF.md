# Agent Handoff

*Last updated: 2026-02-19*

---

## STATUS: V2 spec gap audit complete — 18 implementation issues created with dependency chain

### Session Summary

**Research-only session.** No code changes. Conducted deep audit of current codebase against v2 specs across three parallel threads:
1. iterate.c vs v2 simplex_iteration.md
2. SolverState struct vs v2 state requirements
3. Basis operations, pricing, and pivot specs vs current implementation

### Key Findings

**The core iteration engine is architecturally inverted:**
- `cxf_log_iteration_progress()` in iterate.c (458 lines) IS the iteration engine (pricing, FTRAN, ratio test, pivot, RC update)
- V2 spec says it should be logging-only; actual iteration belongs in `cxf_simplex_step()`
- Our `cxf_simplex_step()` in step.c is a minimal post-pivot helper, not the full engine
- `cxf_simplex_step2()` and `cxf_simplex_step3()` are completely wrong algorithms (extended primal pivot and dual simplex pivot, not bound propagation)

**Missing v2 infrastructure:**
- SolverState: ~15 field groups missing (saved bounds, activity bounds, progress counters, snapshot buffer)
- Pricing subsystem: 11 of 13 v2 functions missing (only cxf_pricing_candidates exists)
- Pivot operations: 4 of 5 functions missing
- Basis snapshot/diff: wrong algorithm (captures var status instead of counters)

---

## V2 Implementation Roadmap — 18 Chained Issues

### Phase A: Iteration Engine Refactor (critical path, P1)

| ID | Title | Deps | Status |
|----|-------|------|--------|
| `h0wk` | A1: Move iteration engine from iterate.c into cxf_simplex_step() | none | **READY** |
| `qh9y` | A2: Make cxf_log_iteration_progress() logging-only | A1 | blocked |
| `6869` | A3: Update solve_lp.c loop to call step/step2/step3/phase_end | A2 | blocked |

### Phase B: SolverState Alignment (parallel with A)

| ID | Title | Deps | Status |
|----|-------|------|--------|
| `aivf` | B1: Add saved_lb/saved_ub to SolverState | none | **READY** |
| `0hzf` | B2: Add activity_lb/activity_ub arrays | none | **READY** |
| `kztb` | B3: Add progress counters + snapshot buffer | none | **READY** |

### Phase C: Activity Bounds + Preprocessing

| ID | Title | Deps | Status |
|----|-------|------|--------|
| `7lbz` | C1: Implement cxf_simplex_setup (activity bounds) | B2 | blocked |
| `nnpw` | C2: Implement cxf_simplex_preprocess | C1 | blocked |
| `wkmv` | C3: Wire setup + preprocess into solve_lp.c | C2, A3 | blocked |

### Phase D: In-Loop Components

| ID | Title | Deps | Status |
|----|-------|------|--------|
| `l8d1` | D1: Implement cxf_simplex_phase_end | A3 | blocked |
| `id19` | D2: Implement cxf_simplex_post_iterate | A3, B3 | blocked |
| `beg0` | D3: Fix cxf_progress_snapshot + cxf_basis_diff | B3 | blocked |

### Phase E: Two-Level Loop + Bound Propagation

| ID | Title | Deps | Status |
|----|-------|------|--------|
| `a5hy` | E1: Implement two-level iteration loop | D1, D2 | blocked |
| `reb8` | E2: Implement cxf_simplex_step2 (variable-side) | E1, F1 | blocked |
| `dz1w` | E3: Implement cxf_simplex_step3 (constraint-side) | E1, F1 | blocked |

### Phase F: Pricing + Pivot Rebuild

| ID | Title | Deps | Status |
|----|-------|------|--------|
| `6er4` | F1: Pricing queue architecture | A3 | blocked |
| `rurr` | F2: Pivot operations (check, bound, update, special) | F1 | blocked |
| `uet6` | F3: Harris two-pass ratio test with BFRT | F2 | blocked |

### Dependency Graph

```
READY NOW:
  h0wk (A1)  aivf (B1)  0hzf (B2)  kztb (B3)

A chain:  h0wk → qh9y → 6869
B→C:      0hzf → 7lbz → nnpw → wkmv (also needs 6869)
B→D:      kztb → beg0, id19 (also needs 6869)
          6869 → l8d1
D→E:      l8d1 + id19 → a5hy → reb8, dz1w (also need 6er4)
F chain:  6869 → 6er4 → rurr → uet6
```

---

## Next Session: Start with A1

**Pick up `convexfeld-h0wk` (A1):** Move the core iteration logic (pricing, FTRAN, ratio test, step-size computation, BTRAN, pivot, incremental RC update, periodic refactorization) from `cxf_log_iteration_progress()` in iterate.c into a proper `cxf_simplex_step()` in step.c. This is a code-move refactor — keep the existing algorithm intact. All 39 tests must still pass after the move.

B1/B2/B3 are independent struct additions that can be done in parallel with or after A1.

---

## Previous Session Results (preserved)

### Completed v2 items:
- Crash basis (P2.5) — `src/simplex/crash.c`
- EXPAND perturbation (P2.6) — `src/simplex/perturbation.c`
- Phase I/II unification (P3.25) — `src/simplex/solve_lp.c`

### Netlib: 19/29 pass (66%)
Still failing: recipe, share1b, bandm, boeing2, brandy, scorpion, capri, e226 (false INFEASIBLE), bore3d (UNBOUNDED), israel (9.4% error). Root cause: v1 iteration engine architecture, not individual bugs.

---

## File Locations

| Item | Path |
|------|------|
| V2 orchestrator (unified loop) | `src/simplex/solve_lp.c` |
| Crash basis (P2.5) | `src/simplex/crash.c` |
| EXPAND perturbation (P2.6) | `src/simplex/perturbation.c` |
| **Iteration engine (TO REFACTOR)** | `src/simplex/iterate.c` |
| Step helper (TO REWRITE) | `src/simplex/step.c` |
| Phase steps (TO REWRITE) | `src/simplex/phase_steps.c` |
| Post-iterate (TO REWRITE) | `src/simplex/post.c` |
| Phase I setup + transition | `src/simplex/phase_one.c` |
| Phase I helpers | `src/simplex/phase_loop.c` |
| Reduced costs | `src/simplex/reduced_costs.c` |
| SolverState header | `include/convexfeld/cxf_solver.h` |
| V2 specs | `docs/specs-v2/specs/modules/` |
| Key v2 specs | `simplex_iteration.md`, `simplex_phases.md`, `solve_lp_core.md` |
