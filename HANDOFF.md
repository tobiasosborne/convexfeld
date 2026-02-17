# Agent Handoff

*Last updated: 2026-02-17*

---

## STATUS: 6 issues completed, 1 investigated but NOT changed

### Session Summary

1. **BTRAN + incremental reduced cost update (convexfeld-mpo9) — CLOSED**
   - Replaced O(n*m) full RC recomputation with O(nnz) incremental BTRAN-based update
   - `src/simplex/iterate.c`: BTRAN before pivot, incremental update after

2. **Profiling & stress testing**
   - Callgrind on sc105, share2b, beaconfd — LU factorize is 47-65% of runtime
   - Netlib sweep: 16/56 pass, 40 fail (mostly false INFEASIBLE)
   - Filed 3 bottleneck issues: convexfeld-uxae (LU), convexfeld-cgjf (Netlib), convexfeld-y1ro (presolve)

3. **Closed convexfeld-8vat (refactorization) — already resolved**

4. **FTRAN/BTRAN inner loop optimization (convexfeld-7ahj) — CLOSED**

5. **Steepest edge sqrt removal (convexfeld-z31) — CLOSED**

6. **Code quality cleanup (convexfeld-b6l, convexfeld-8kg) — CLOSED**

7. **Phase I/II investigation (convexfeld-ypf9) — NOT CHANGED, returned to open**
   - Investigated false INFEASIBLE root cause thoroughly (see gotchas.md)
   - Attempted tolerance/stall-recovery patch: improved some, regressed ship04l
   - **Reverted** — patches don't align with v2 spec architecture
   - See "Architecture Gap" section below for full analysis

### Previous sessions (all CLOSED):
   - P1 constraint satisfaction tests + >= solver bug fix
   - P2 decompose solve_lp, P1 struct/function renames, P0 fixes

---

## CRITICAL: Architecture Gap — Current vs V2

The 40/56 Netlib failures are caused by missing v2 infrastructure, not individual bugs.

### Current Architecture (broken for degenerate problems)
```
solve_lp.c:
  setup_phase_one()          ← trivial all-slack basis
  perturbation()             ← Wolfe, called ONCE upfront
  run_phase_one()            ← SEPARATE loop
  transition_to_phase_two()  ← SEPARATE function
  run_phase_two()            ← SEPARATE loop
```

### V2 Target Architecture (from simplex_phases.md)
```
solve_lp.c:
  cxf_simplex_crash          ← triangularity-based initial basis
  cxf_simplex_preprocess     ← fix near-bound variables
  cxf_simplex_setup          ← compute activity bounds
  SINGLE iteration loop:
    cxf_log_iteration_progress
    cxf_simplex_phase_end    ← manages Phase I→II transition inline
    cxf_simplex_perturbation ← EXPAND method, on stall detection
    cxf_simplex_step
    cxf_simplex_phase_end    ← post-pivot cleanup
    cxf_simplex_post_iterate ← stall detection (basis snapshot diff)
  cxf_simplex_refine
```

### Root Cause of False INFEASIBLE
1. Dual degeneracy in Phase I → many RCs cluster at 0
2. Pricing at tolerance 1e-6 skips variables with RC ≈ -1e-8
3. Phase I declares "no improving direction" → false INFEASIBLE
4. Missing crash basis means MORE artificials → MORE degeneracy
5. Missing EXPAND perturbation means no degeneracy recovery

### Recommended Issue Order (v2-aligned)
```
snwu (crash basis)     ← Fewer artificials, less Phase I degeneracy
  ↓
1azn (EXPAND method)   ← Replace Wolfe with EXPAND, stall-triggered
  ↓
ypf9 (Phase I/II)     ← Unify loops, implement cxf_simplex_phase_end
  ↓
4gfy (diag_coeff)      ← Needs activity bounds from setup
  ↓
cgjf (Netlib sweep)    ← Validate all above
```

---

## File Locations

| Item | Path |
|------|------|
| V2 simplex phases spec | `docs/specs-v2/specs/modules/simplex_phases.md` |
| Current Phase I/II | `src/simplex/phase_loop.c`, `src/simplex/phase_one.c` |
| Solve orchestrator | `src/simplex/solve_lp.c` |
| Core iteration | `src/simplex/iterate.c` |
| Perturbation (Wolfe) | `src/simplex/perturbation.c` |
| Context/lifecycle | `src/simplex/context.c` |
| False INFEASIBLE analysis | `docs/learnings/gotchas.md` (bottom) |
| Incremental RC update | `src/simplex/iterate.c` (Steps 5-9) |
| FTRAN/BTRAN | `src/basis/ftran.c`, `src/basis/btran.c` |
| LU factorization | `src/basis/lu_factorize.c` |
| Callgrind profiles | `callgrind_*.out` |
| V2 specs | `docs/specs-v2/specs/` |
