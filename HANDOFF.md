# Agent Handoff

*Last updated: 2026-01-29*

---

## STATUS: Full Benchmark Run Completed - BTRAN Bug Fixed

### Session Summary

Ran full Netlib benchmark suite to identify bugs. Found and fixed a critical BTRAN bug.

#### Bug Fixed

**BTRAN diagonal scaling not applied when eta_count=0**
- The initial basis B_0 = diag(coeff), not identity
- BTRAN was returning early without applying diag_coeff scaling
- This caused wrong reduced costs in Phase I for problems with >= or = constraints
- Fix: Apply diag_coeff scaling at the END of BTRAN, outside the eta_count > 0 block

#### Benchmark Results (Post-Fix)

| Category | Count | Examples |
|----------|-------|----------|
| **PASS** | 24 | afiro, sc50a, sc105, blend, brandy, beaconfd, share1b, share2b, lotfi, bandm, bore3d, agg2, agg3, scfxm1, czprob, modszk1, scsd1/6/8, ship04s/l, stocfor1, sc50b, sc205 |
| **INFEASIBLE (bug)** | 9 | e226, scorpion, capri, degen2, bnl1, etamacro, seba, gfrd-pnc, nesm |
| **Wrong obj (>5%)** | 8 | sctap1/2 (100%), standata (86%), wood1p (69%), forplan (43%), scagr25 (14%), agg (13%), israel (9.4%) |
| **Wrong obj (<5%)** | 7 | scrs8 (7.9%), standmps (7.3%), recipe (2.4%), scagr7 (1.8%), ganges (0.7%), adlittle (0.12%), kb2 (0.016%) |

### Files Modified

| File | Changes |
|------|---------|
| `src/basis/btran.c` | Fixed diagonal scaling order - now applied after eta vectors |

### Test Status

- **Unit Tests:** 35/36 pass (97%) - same as before (test_simplex_edge has pre-existing failure)
- **Integration Tests:** 2/2 pass
- **Netlib Benchmarks:** 24 pass, 24 fail (9 INFEASIBLE bugs, 15 numerical issues)

---

## Known Bugs (Prioritized)

### P0: Phase I Returns INFEASIBLE Incorrectly

**Affected:** e226, scorpion, capri, degen2, bnl1, etamacro, seba, gfrd-pnc, nesm

**Symptoms:** Phase I declares infeasibility even though problems are feasible.

**Probable Cause:** Phase I objective doesn't reach zero when it should:
- Reduced cost computation may be wrong for certain constraint types
- Or pricing is not finding improving variables
- Or initial artificial variable values are computed incorrectly

**Debug approach:** Add verbose logging to Phase I for e226 (small, 282 vars, 223 constrs):
- Print initial Phase I objective
- Print which variables have negative reduced costs
- Track if objective decreases each iteration

### P1: Suboptimal Solutions (sctap1, sctap2)

**Symptoms:** Returns obj=0 instead of optimal ~1400-1700

**Probable Cause:** Phase II transition issue or not computing objective correctly

### P2: Numerical Precision Errors

**Ranging from 0.016% (kb2) to 86% (standata)**

Likely causes:
- Eta accumulation without refactorization
- Tolerance issues in ratio test
- Step size computation errors

---

## Next Steps

### Priority 1: Debug Phase I INFEASIBLE Bug

1. Add debug output to solve_lp.c Phase I loop
2. Run e226 and trace:
   - Initial Phase I objective value
   - Reduced costs at each iteration
   - Which variables are selected for entering
   - Phase I objective after each pivot
3. Identify why it terminates with positive objective

### Priority 2: Debug sctap1 obj=0 Bug

1. Trace Phase II transition
2. Verify objective coefficients are restored correctly
3. Check reduced costs in Phase II

---

## Quality Gate Status

- **Tests:** 35/36 unit tests pass (97%)
- **Integration:** 2/2 pass
- **Build:** Clean
- **Netlib:** 24/48 small/medium benchmarks pass (50%)
