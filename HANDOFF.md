# Agent Handoff

*Last updated: 2026-02-05*

---

## STATUS: LU Permutation Bug Fixed — 11/27 Netlib Pass

### Session Summary

Fixed two bugs in lu_factorize.c that caused wrong answers after LU refactorization:

1. **L_row_idx permutation bug**: L factor stored original row indices, but FTRAN/BTRAN forward substitution indexed temp[] by step positions (after applying permutation P). Fixed by converting L_row_idx from original rows to step positions using inverse permutation.

2. **L/U buffer overflow**: L_row_idx and U_row_idx arrays were allocated with estimate `m*2`, but dense Markowitz can produce up to `m*(m-1)/2` entries. Added realloc before filling to prevent overflow on large problems.

#### Changes Made

| File | Changes |
|------|---------|
| `src/basis/lu_factorize.c` | Added inverse permutation conversion of L_row_idx (lines 306-324); Added realloc for L arrays (lines 289-303) and U arrays (lines 255-271) |

#### Benchmark Results (27 problems, 15s timeout)

**Before (LU with permutation bug):**
- PASS: 4 (afiro, sc50b, sc105, blend)
- FAIL: 14 (wrong obj or wrong status)
- TIMEOUT: 9

**After (permutation fix + buffer overflow fix):**
- PASS: 11 (afiro, sc50b, sc105, blend, lotfi, share2b, beaconfd, bore3d, ship04l, brandy, bandm)
- FAIL: 13 (wrong obj or wrong status)
- TIMEOUT: 3 (capri, grow7, seba)

**Newly passing:** lotfi, share2b, beaconfd, bore3d, ship04l, brandy, bandm
**Near-miss:** kb2 (err=0.016%), adlittle (err=0.12%)

### Remaining Bugs

#### P0: Phase I false INFEASIBLE (8 problems)
- share1b, stair, degen2, boeing1, boeing2, bnl1, e226, scorpion
- Root cause: numerical drift in Phase I + dual degeneracy
- e226 needs lexicographic pivoting or Bland's rule

#### P1: Cycling/Timeouts (3 problems)
- capri, grow7, seba
- Need anti-cycling: Bland's rule or stronger perturbation

#### P2: Small numerical errors (5 problems)
- kb2 (0.016%), adlittle (0.12%), recipe (2.4%), scagr7 (1.8%), israel (9.4%)
- Solution refinement or iterative refinement could help
- israel may have a Phase I issue contributing to error

---

## Next Steps

### Priority 1: Anti-cycling (Bland's Rule)
- Would fix 3 remaining timeouts (capri, grow7, seba)
- Simple: when multiple candidates have negative reduced cost, choose smallest index
- Implement as fallback when iteration count exceeds threshold

### Priority 2: Phase I False INFEASIBLE
- 8 problems return INFEASIBLE but are feasible
- After cycling fix, re-test — some may be cycling in Phase I
- Dual degeneracy handling needed for e226

### Priority 3: Numerical Refinement
- kb2, adlittle very close — tighter pivot tolerance or iterative refinement
- recipe, scagr7, israel have larger errors — investigate root cause

---

## Quality Gate Status

- **Tests:** 35/36 pass (pre-existing `test_simplex_edge` failure)
- **Build:** Clean
- **Netlib:** 11 pass, 13 fail, 3 timeout (was 4/14/9)
