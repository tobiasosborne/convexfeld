# Agent Handoff

*Last updated: 2026-02-05*

---

## STATUS: LU Refactorization Enabled — Accuracy Issues Remain

### Session Summary

Re-enabled periodic LU refactorization in the simplex iteration loop. This was the top recommendation from the previous session's analysis. Perturbation and solution refinement were already wired into solve_lp.c.

#### Changes Made

| File | Changes |
|------|---------|
| `src/simplex/iterate.c` | Changed `REFACTOR_INTERVAL` from 10000 to 100; switched from `cxf_basis_refactor(basis)` to `cxf_solver_refactor(state, env)` for proper LU factorization |

#### Benchmark Results (27 problems tested)

**Before (REFACTOR_INTERVAL=10000, eta-only):**
- PASS: 4 (afiro, sc50b, sc105, blend)
- FAIL: 9 (wrong obj or wrong status)
- TIMEOUT (>10s): 14

**After (REFACTOR_INTERVAL=100, LU refactorization):**
- PASS: 4 (afiro, sc50b, sc105, blend)
- FAIL: 14 (wrong obj or wrong status)
- TIMEOUT (>15s): 9

**Net effect:** 5 fewer timeouts (problems now terminate), but some return wrong answers — suggests LU factorization or LU-based FTRAN/BTRAN has accuracy bugs.

#### Key Findings

1. **Perturbation + refinement already wired in** (lines 714, 1236, 1239 of solve_lp.c) — not the missing piece
2. **LU refactorization infrastructure is complete:** lu_factorize.c (Markowitz), lu_factors.c (lifecycle), ftran.c/btran.c (LU paths)
3. **LU path has correctness issues:** Problems that previously timed out (cycling) now terminate but with wrong answers (kb2: obj=0, share2b: 4.73x error, lotfi: 9.1x error)
4. **FTRAN/BTRAN architecture is correct:** LU replaces diag_coeff when valid; etas layer on top

### Root Cause Hypothesis for LU Accuracy Issues

The LU factorization in `lu_factorize.c` uses dense Markowitz elimination. Possible bugs:
- The L factor row indices store original row indices, but the FTRAN/BTRAN LU apply code may expect step indices (or vice versa)
- Permutation handling (P, Q) in the solve/btran may be inconsistent with how they're stored
- U off-diagonal entries use `U_row_idx[p] = j_step` (step index) — need to verify FTRAN/BTRAN interpret this consistently

### Remaining Bugs

#### P0: Phase I INFEASIBLE for feasible problems
- 6 problems: scorpion, e226, boeing1, boeing2, israel, bnl1, seba
- Root causes: numerical drift (partially fixed), dual degeneracy (unfixed)

#### P1: LU Refactorization Accuracy
- After LU refactor, solution values go wrong
- Need to debug LU factorize + FTRAN/BTRAN LU path consistency
- Consider adding a verification step: after refactor, check that B * B^{-1} * e_i = e_i for a few columns

#### P1: False UNBOUNDED
- 2 problems: bore3d, adlittle (adlittle newly UNBOUNDED after refactor change)
- Issue `convexfeld-o2th`

#### P1: Cycling/Timeouts
- 9 problems still timeout at 15s: brandy, share1b, capri, bandm, recipe, scagr7, grow7, stair, degen2

---

## Next Steps

### Priority 1: Debug LU FTRAN/BTRAN Path
1. Add verification: after `cxf_solver_refactor`, test B * B^{-1} * e_i = e_i for a few columns
2. If verification fails, the bug is in lu_factorize.c or the LU solve routines
3. Focus on permutation handling — most likely source of errors
4. Test with a small problem (afiro) that currently passes, to verify LU path works on simple cases

### Priority 2: Anti-cycling (Bland's Rule)
- Perturbation is applied but may not be sufficient
- Bland's rule (smallest index among improving) is simple and guaranteed to terminate
- Would fix remaining timeouts

### Priority 3: Phase I False INFEASIBLE
- After LU accuracy fixed, re-test Phase I problems
- Dual degeneracy (e226) needs lexicographic pivoting

---

## Quality Gate Status

- **Tests:** 35/36 pass (pre-existing `test_unperturb_sequence` failure)
- **Build:** Clean
- **Netlib:** 4 pass, 14 fail, 9 timeout (was 4/9/14)
