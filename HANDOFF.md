# Agent Handoff

*Last updated: 2026-02-19*

---

## STATUS: 3 v2 spec issues completed (snwu, 1azn, ypf9)

### Session Summary

1. **Crash basis — convexfeld-snwu — CLOSED**
   - Rewrote `src/simplex/crash.c` with P2.5 row-scanning algorithm
   - Added `row_status`, `col_nz_count`, `num_basic`, `problem_row_index` to SolverState
   - Added `CXF_ROW_BASIC_LOWER/UPPER` status constants to cxf_types.h
   - 11 tests in `tests/unit/test_crash.c`
   - 130 LOC, v2-compliant, not yet called from main solve flow

2. **EXPAND perturbation — convexfeld-1azn — CLOSED**
   - Rewrote `src/simplex/perturbation.c` with P2.6 implied-bound analysis
   - Replaced Wolfe random perturbation with targeted degeneracy removal
   - Added `perturb_count` to SolverState (replaced global flag)
   - Iteration guard: no-op at iteration 0 (stall-triggered per spec)
   - 9 tests in `tests/unit/test_perturbation.c`
   - Updated `test_simplex_edge.c` for new EXPAND semantics

3. **Phase I/II unification — convexfeld-ypf9 — CLOSED**
   - Rewrote `src/simplex/solve_lp.c` with single unified iteration loop
   - Integrated crash basis call before Phase I setup
   - Phase transition managed inline via `cxf_check_phase_one_end`
   - EXPAND perturbation called when `degenerate_count > 50`
   - Added `cxf_check_phase_one_end()` to `phase_loop.c`
   - Old `cxf_run_phase_one/phase_two` now dead code (cleanup issue filed)

### All 39/39 tests pass. No regressions.

---

## V2 Architecture — Current State

```
solve_lp.c (v2):
  cxf_simplex_init()
  cxf_simplex_crash()            ✅ P2.5 (implemented, called)
  cxf_setup_phase_one()          (still needed: sets up artificials)
  cxf_compute_reduced_costs()
  UNIFIED LOOP:                  ✅ P3.25
    cxf_log_iteration_progress()   (single iteration)
    [Phase I optimal?]
      cxf_check_phase_one_end()    ✅ inline transition
    cxf_simplex_perturbation()     ✅ P2.6 EXPAND (stall-triggered)
  cxf_simplex_unperturb()
  cxf_simplex_refine()
  cxf_extract_solution()
```

---

## V2 Dependency Chain Status

```
snwu (crash basis)     ✅ DONE
1azn (EXPAND method)   ✅ DONE
ypf9 (Phase I/II)      ✅ DONE
4gfy (diag_coeff)      ← NEXT (needs activity bounds from setup)
cgjf (Netlib sweep)    ← validate all above
```

---

## Open Cleanup Issues

- `convexfeld-h343`: Refactor perturbation.c to < 200 LOC (currently 311)
- `convexfeld-c0cy`: Remove dead code cxf_run_phase_one/phase_two from phase_loop.c

---

## File Locations

| Item | Path |
|------|------|
| V2 orchestrator (unified loop) | `src/simplex/solve_lp.c` |
| Crash basis (P2.5) | `src/simplex/crash.c` |
| EXPAND perturbation (P2.6) | `src/simplex/perturbation.c` |
| Phase I setup + transition | `src/simplex/phase_one.c` |
| Phase I helpers (check_phase_one_end) | `src/simplex/phase_loop.c` |
| V2 specs | `docs/specs-v2/specs/` |
| Crash tests | `tests/unit/test_crash.c` |
| Perturbation tests | `tests/unit/test_perturbation.c` |
