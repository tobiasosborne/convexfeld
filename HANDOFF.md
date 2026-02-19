# Agent Handoff

*Last updated: 2026-02-19*

---

## STATUS: 3 v2 spec issues completed + Phase II regression fix

### Session Summary

1. **Crash basis — convexfeld-snwu — CLOSED**
   - Rewrote `src/simplex/crash.c` with P2.5 row-scanning algorithm
   - Added `row_status`, `col_nz_count`, `num_basic`, `problem_row_index` to SolverState
   - Added `CXF_ROW_BASIC_LOWER/UPPER` status constants to cxf_types.h
   - 11 tests in `tests/unit/test_crash.c`, 130 LOC

2. **EXPAND perturbation — convexfeld-1azn — CLOSED**
   - Rewrote `src/simplex/perturbation.c` with P2.6 implied-bound analysis
   - Replaced Wolfe random perturbation with targeted degeneracy removal
   - Added `perturb_count` to SolverState (replaced global flag)
   - Iteration guard: no-op at iteration 0 (stall-triggered per spec)
   - 9 tests in `tests/unit/test_perturbation.c`

3. **Phase I/II unification — convexfeld-ypf9 — CLOSED**
   - Rewrote `src/simplex/solve_lp.c` with single unified iteration loop
   - Integrated crash basis call before Phase I setup
   - Phase transition managed inline via `cxf_check_phase_one_end`
   - EXPAND perturbation called when `degenerate_count > 50`
   - Added `cxf_check_phase_one_end()` to `phase_loop.c`

4. **Phase II regression fix**
   - EXPAND marked variables CXF_VAR_FIXED during Phase I stalling
   - These persisted into Phase II, starving pricing of candidates → obj=0
   - Fixed: reset FIXED→AT_LOWER in `cxf_transition_to_phase_two`

### Test Results: 39/39 unit tests pass

---

## Netlib Benchmark Results (2026-02-19)

**19/29 tested problems pass (66%), up from 16/56 (29%) pre-session.**

### Passing (19):
afiro, sc50a, sc50b, sc105, sc205, adlittle, stocfor1, share2b,
kb2, blend, scagr7, lotfi, beaconfd, agg, agg2, agg3, sctap1,
ship04s, ship04l

### New passes this session (4):
kb2, blend, sc205, scagr7

### Still failing (10/29 tested):
| Problem | Status | Notes |
|---------|--------|-------|
| recipe | INFEASIBLE | False infeasible |
| share1b | INFEASIBLE | False infeasible |
| bandm | INFEASIBLE | False infeasible |
| boeing2 | INFEASIBLE | False infeasible |
| brandy | INFEASIBLE | False infeasible |
| scorpion | INFEASIBLE | False infeasible |
| capri | INFEASIBLE | False infeasible |
| e226 | INFEASIBLE | False infeasible |
| bore3d | UNBOUNDED | Wrong status |
| israel | OPTIMAL (9.4% err) | Numerical drift |

### Remaining false INFEASIBLE root causes (hypothesis):
- Phase I degeneracy still present for problems with many equality constraints
- Missing initial LU factorization before first iteration (BTRAN relies on eta+diag fallback)
- Accumulated numerical error in incremental RC updates

---

## V2 Architecture — Current State

```
solve_lp.c (v2 unified loop):
  cxf_simplex_init()
  cxf_simplex_crash()            ✅ P2.5
  cxf_setup_phase_one()
  cxf_compute_reduced_costs()
  UNIFIED LOOP:                  ✅ P3.25
    cxf_log_iteration_progress()
    [Phase I optimal → cxf_check_phase_one_end()]
    [stalling → cxf_simplex_perturbation()]  ✅ P2.6 EXPAND
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
4gfy (diag_coeff)      ⏸ DEFERRED — LU factorize needs ±1 coefficients
cgjf (Netlib sweep)    🔲 OPEN — blocked by uxae (LU performance)
```

---

## Open Issues (prioritized)

### High priority (correctness):
- `convexfeld-cgjf`: 10/29 Netlib failures remain (mostly false INFEASIBLE)
- `convexfeld-4gfy`: Remove diag_coeff — deferred, needs careful analysis

### Medium priority (cleanup):
- `convexfeld-h343`: Refactor perturbation.c to < 200 LOC (currently 311)
- `convexfeld-c0cy`: Remove dead code cxf_run_phase_one/phase_two from phase_loop.c

### Suggested next steps for Netlib correctness:
1. Add initial LU factorization after Phase I setup (v2 spec step 5)
2. Investigate false INFEASIBLE on equality-heavy problems (recipe, e226)
3. Consider adding refactorization at Phase I→II transition boundary

---

## File Locations

| Item | Path |
|------|------|
| V2 orchestrator (unified loop) | `src/simplex/solve_lp.c` |
| Crash basis (P2.5) | `src/simplex/crash.c` |
| EXPAND perturbation (P2.6) | `src/simplex/perturbation.c` |
| Phase I setup + transition | `src/simplex/phase_one.c` |
| Phase I helpers | `src/simplex/phase_loop.c` |
| Core iteration | `src/simplex/iterate.c` |
| Reduced costs | `src/simplex/reduced_costs.c` |
| V2 specs | `docs/specs-v2/specs/` |
| Crash tests | `tests/unit/test_crash.c` |
| Perturbation tests | `tests/unit/test_perturbation.c` |
| Netlib problems | `benchmarks/netlib/feasible/` |
| Netlib reference | `benchmarks/netlib/feasible_reference_1e-8.csv` |
