# Agent Handoff

*Last updated: 2026-02-17*

---

## STATUS: Profiling + bottleneck issues filed

### Session Summary

1. **BTRAN + incremental reduced cost update (convexfeld-mpo9) — CLOSED**
   - Replaced O(n*m) full reduced cost recomputation with O(nnz) incremental BTRAN-based update
   - In `src/simplex/iterate.c`: BTRAN before pivot, incremental update after
   - 37/37 tests pass

2. **Profiling & stress testing (callgrind on sc105, share2b, beaconfd)**
   - #1 bottleneck: `cxf_lu_factorize` at 47-65% of runtime
   - #2: `cxf_ftran` 5-12%, `cxf_btran` 4-10% (expected)
   - #3: Phase I overhead (`compute_true_infeasibility`) 4-9%
   - #4: Presolve scanning (`cxf_check_obvious_infeasibility`) 6-8% on small problems

3. **Netlib correctness sweep (114 problems)**
   - Pass: 16, Fail: 40, Skipped: 58 (>500KB or timeout)
   - Main failure mode: false INFEASIBLE on feasible problems

4. **Filed 3 bottleneck issues with dependency chains**
   - convexfeld-uxae (P1): LU factorization 47-65% → depends on 8vat, 4gfy
   - convexfeld-cgjf (P1): 40/56 Netlib fail → depends on uxae, ypf9, 1azn, snwu
   - convexfeld-y1ro (P2): Presolve 6-8% → depends on qrs9

### Previous sessions (all CLOSED):
   - P1 constraint satisfaction tests + >= solver bug fix
   - P2 decompose solve_lp (convexfeld-23p6)
   - P1 struct/function renames (convexfeld-dv0k, convexfeld-b7ow)
   - P0 variable status encoding, tolerance fix

---

## NEXT STEPS

The critical path to Netlib correctness is:
```
8vat (refactorization) + 4gfy (diag_coeff) → uxae (LU perf)  ─┐
ypf9 (Phase I/II logic)                                        ├→ cgjf (Netlib pass)
1azn (EXPAND degeneracy)                                       │
snwu (crash basis)                                            ─┘
```

Run `bd ready` — many P2 issues are unblocked and ready to work.

---

## File Locations

| Item | Path |
|------|------|
| Incremental RC update | `src/simplex/iterate.c` (Steps 5-9) |
| LU factorization | `src/basis/lu_factorize.c` |
| BTRAN/FTRAN | `src/basis/btran.c`, `src/basis/ftran.c` |
| Phase I loop | `src/simplex/phase_loop.c` |
| Presolve | `src/simplex/presolve.c` |
| Callgrind profiles | `callgrind_incr.out`, `callgrind_beaconfd.out`, `callgrind_share2b.out` |
| V2 specs | `docs/specs-v2/specs/` |
| Learnings | `docs/learnings/` |
