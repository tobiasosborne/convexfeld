# Agent Handoff

*Last updated: 2026-02-17*

---

## STATUS: 6 issues completed this session

### Session Summary

1. **BTRAN + incremental reduced cost update (convexfeld-mpo9) — CLOSED**
   - Replaced O(n*m) full RC recomputation with O(nnz) incremental BTRAN-based update
   - `src/simplex/iterate.c`: BTRAN before pivot, incremental update after

2. **Profiling & stress testing**
   - Callgrind on sc105, share2b, beaconfd — LU factorize is 47-65% of runtime
   - Netlib sweep: 16/56 pass, 40 fail (mostly false INFEASIBLE)
   - Filed 3 bottleneck issues: convexfeld-uxae (LU), convexfeld-cgjf (Netlib), convexfeld-y1ro (presolve)

3. **Closed convexfeld-8vat (refactorization) — already resolved**
   - REFACTOR_INTERVAL was already 100, profiling confirms LU runs regularly

4. **FTRAN/BTRAN inner loop optimization (convexfeld-7ahj) — CLOSED**
   - Removed redundant bounds checks from hot inner loops
   - FTRAN -40%, BTRAN -52% instruction count, -4.3% total on beaconfd
   - 37/37 tests pass

5. **Steepest edge sqrt removal (convexfeld-z31) — CLOSED**
   - Changed `abs_rc / sqrt(weight)` to `(abs_rc * abs_rc) / weight`
   - Avoids sqrt in hot pricing loop, equivalent comparison via squared ratios

6. **Code quality cleanup (convexfeld-b6l, convexfeld-8kg) — CLOSED**
   - Removed unused `INSERTION_THRESHOLD` define from `src/matrix/sort.c`
   - Deleted no-op `cxf_finalize_row_data` from `src/matrix/row_major.c`
   - Updated tests to remove calls to deleted function

### Previous sessions (all CLOSED):
   - P1 constraint satisfaction tests + >= solver bug fix
   - P2 decompose solve_lp, P1 struct/function renames, P0 fixes

---

## NEXT STEPS

Critical path to Netlib correctness:
```
4gfy (diag_coeff) → uxae (LU perf)  ────────────────┐
ypf9 (Phase I/II logic)                              ├→ cgjf (40/56 Netlib)
1azn (EXPAND degeneracy)                             │
snwu (crash basis)                                  ─┘
```

Run `bd ready` for available work.

---

## File Locations

| Item | Path |
|------|------|
| Incremental RC update | `src/simplex/iterate.c` (Steps 5-9) |
| FTRAN (optimized) | `src/basis/ftran.c` |
| BTRAN (optimized) | `src/basis/btran.c` |
| Eta creation | `src/basis/pivot_eta.c` |
| LU factorization | `src/basis/lu_factorize.c` |
| Phase I loop | `src/simplex/phase_loop.c` |
| Callgrind profiles | `callgrind_*.out` |
| V2 specs | `docs/specs-v2/specs/` |
| Learnings | `docs/learnings/` |
