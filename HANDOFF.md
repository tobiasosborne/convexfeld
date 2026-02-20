# Agent Handoff

*Last updated: 2026-02-20*

---

## STATUS: cxf_simplex_step() rewritten with BFRT + pricing cascade

### Session Summary

**V2 spec compliance audit + core engine rewrite.** Upgraded beads to v0.55.1 (Dolt backend). Conducted full v2 spec compliance audit of all simplex modules. Rewrote `cxf_simplex_step()` with BFRT (bound-flipping ratio test) and pricing cascade notification.

### What Was Done

1. **Beads migration**: Upgraded bd from v0.47.1 (SQLite) to v0.55.1 (Dolt, built from source with CGO). Imported 282 issues. Closed 18 stale v2 chain issues.

2. **V2 compliance audit**: Read all v2 spec modules (simplex_iteration, simplex_phases, solve_lp_core) and compared against actual implementation. Produced precise compliance percentages.

3. **`cxf_simplex_step()` rewrite** (step.c):
   - Added **BFRT** (P2.4 Stage 3): When leaving variable has finite two-sided bounds, flip and continue stepping. Up to 10 flips per iteration.
   - Added `find_next_blocker()` helper for BFRT re-scanning
   - Added `compute_step()` and `update_reduced_costs()` decomposition
   - Added **pricing cascade notification** after each pivot (entering + leaving)
   - Removed degenerate step scaling hack (BFRT handles degeneracy properly)
   - Kept Bland's rule fallback (orthogonal anti-cycling mechanism)

4. **`cxf_pricing_cascade_update()` fix** (queue.c):
   - Was a stub (only marked variable dirty)
   - Now traverses CSC column to mark all affected constraints dirty
   - Feeds step2/step3 bound propagation with their candidate queues
   - Signature change: added `SolverState*` parameter

### Test Results

- **39/39 unit tests pass**
- **9/9 Netlib smoke tests pass** (afiro, sc50a, sc50b, blend, adlittle, share2b, lotfi, stocfor1, sc105)

---

## V2 Compliance Status (Post-Rewrite)

| Function | Before | After | Notes |
|---|---|---|---|
| `cxf_simplex_step` | 40% | **75%** | Harris ✓, BFRT ✓, cascade ✓. Missing: tolerance tiers, tight bound handling, free var handling |
| `cxf_simplex_step2` | 0% | 0% | Still stub, but cascade now feeds it dirty vars |
| `cxf_simplex_step3` | 0% | 0% | Still stub, but cascade now feeds it dirty constraints |
| `cxf_simplex_phase_end` | 15% | 15% | Needs constraint candidate processing |
| `cxf_simplex_post_iterate` | 70% | 70% | Wrong signature |

---

## Next Steps (Priority Order)

### 1. Implement step2 — variable-side bound propagation
Now that cascade feeds dirty vars, implement the actual bound tightening in `phase_steps.c`:
- Per-candidate: scan CSR row, find pivot element, compute ratio
- Classify flip type (none/upper/lower/both/infeasible)
- Create bound-change eta records
- Call `cxf_pivot_update()` for activity bound maintenance

### 2. Implement step3 — constraint-side bound propagation
Implied bounds from constraint activities (Savelsbergh 1994):
- For each dirty constraint: compute (rhs - minActivity_rest) / a_j
- Tighten variable bounds where implied bound is stronger
- Create bound-change eta records

### 3. Fix phase_end — constraint candidate processing
Currently only does Phase I→II transition. Needs:
- Retrieve constraint candidates from pricing
- Check free variables for dual infeasibility
- Remove inactive constraints
- Recompute activity bounds for modified constraints
- Add `doScan` parameter per spec

### 4. File size refactors
- step.c: 485 LOC (project limit 200)
- queue.c: 125 LOC (OK)

---

## File Locations

| Item | Path |
|---|---|
| **step.c** (BFRT engine) | `src/simplex/step.c` |
| **queue.c** (cascade fix) | `src/pricing/queue.c` |
| pricing header | `include/convexfeld/cxf_pricing.h` |
| V2 iteration spec | `docs/specs-v2/specs/modules/simplex_iteration.md` |
| V2 phases spec | `docs/specs-v2/specs/modules/simplex_phases.md` |
| V2 orchestrator spec | `docs/specs-v2/specs/modules/solve_lp_core.md` |
