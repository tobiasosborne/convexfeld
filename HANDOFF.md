# Agent Handoff

*Last updated: 2026-02-05*

---

## STATUS: Cycling Root Cause DEEPER Than Expected — Key Findings

### Session Summary

Implemented candidate-skipping loop in iterate.c and investigated cycling on capri.
**Cycling is NOT a step=0 degeneracy problem** — debug output revealed the real issue.

#### Changes Made (iterate.c only)

| File | Changes |
|------|---------|
| `src/simplex/iterate.c` | Candidate loop: Steps 2-4 (FTRAN, ratio test, step) now loop over Bland candidates, skipping degenerate ones. Degenerate threshold raised to 1e-8. Forced-step for stuck cycles. |

No regressions: 35/36 tests pass (same pre-existing `test_simplex_edge` failure).

#### Key Discovery: Capri Cycling Debug Analysis

Previous session described the cycle as step=0 degenerate pivots. **This was wrong.**
Debug output with the current code shows:

```
Phase 1: Pre-Bland cycling (iter 801-813)
  Vars 247/240 alternate, step=3.04e-10 (NOT zero — above 1e-12 threshold!)
  Bland's mode NOT active, degenerate counter stays at 0

Phase 2: Bland's activates at iter 814 (solve_lp.c 3*m rule)
  Steps become large (1.78, 10.0), cycle breaks

Phase 3: Bland's zig-zagging (iter 814+)
  Vars 306/314 alternate with steps 1.78 and 10.0
  Non-degenerate but oscillating — Bland's rule causes poor pivot selection
  Solver runs 10000+ iterations with no convergence
```

#### Root Causes Identified

1. **Near-zero step threshold too low**: Previous 1e-12 threshold missed steps
   of 3e-10 that are effectively degenerate. Fixed: raised to 1e-8.

2. **Bland's rule is too slow**: Once activated, Bland's causes massive zig-zagging
   on bounded-variable problems. The smallest-index rule makes terrible pivot
   choices. Solver runs 10000+ iterations without converging.

3. **Perturbation is a no-op**: `context.c` has stub implementations of
   `cxf_simplex_perturbation()` and `cxf_simplex_unperturb()` that do nothing.
   The real implementations in `perturbation.c` are NEVER called because the
   linker picks the context.c stubs. **Removing the stubs causes regressions**
   because the perturbation.c implementation has bugs:
   - Wrong direction: shrinks bounds (lb+=eps, ub-=eps) instead of expanding
   - Only perturbs original vars, not auxiliaries
   - Scale too small (1e-12) or too large (causes test failures)

### Remaining Bugs

#### P0: Cycling (3 problems — capri, grow7, seba)
The real fix needs ONE of:
1. **Fix perturbation properly**: Remove context.c stubs, fix perturbation.c to
   expand bounds (lb-=eps, ub+=eps), use correct scale (~1e-8), perturb auxiliaries
   too, AND recompute basic variable values from perturbed RHS after perturbing.
   The key insight: basic vars must be INTERIOR to perturbed bounds.
2. **Temporary Bland's**: Activate Bland for ~200 iterations to escape degenerate
   cycle, then DEACTIVATE and return to normal pricing. Bland's is guaranteed
   finite but practically too slow.
3. **Lexicographic pivoting**: Proper anti-cycling for bounded variables.

#### P0: Phase I false INFEASIBLE (8+ problems)
- brandy, kb2, bandm, share1b, stair, degen2, boeing1/2, bnl1, e226, scorpion

#### P2: Small numerical errors (5 problems)
- adlittle (0.12%), blend (0.005%), recipe, scagr7, israel

---

## Next Steps

### Priority 1: Fix Perturbation (recommended approach for cycling)
1. Remove the no-op stubs from `context.c` (lines 267-293)
2. Fix `perturbation.c`:
   - Expand bounds: `lb -= eps`, `ub += eps`
   - Extend to auxiliaries (j < n+m, not just j < n)
   - Scale: ~1e-8 (feas_tol * 1e-2)
   - After perturbing bounds, recompute initial basic variable values from
     perturbed system (x_B = B^(-1) * b) so they are interior to new bounds
3. Fix `unperturb`: after restoring bounds, recompute objective from original
   costs to eliminate perturbation-induced objective shift
4. Test: capri/grow7/seba should solve; all 11 previously-passing tests must still pass

### Priority 2: Temporary Bland's mode
Instead of keeping Bland's on forever, deactivate after 200 iterations and
trigger refactorization. This avoids the zig-zagging problem.

### Priority 3: Phase I false INFEASIBLE
Many problems fail in Phase I. Some may be cycling, others genuinely stuck.

---

## Quality Gate Status

- **Tests:** 35/36 pass (pre-existing `test_simplex_edge` failure)
- **Build:** Clean (no warnings)
- **Netlib:** 11 pass, 13 fail, 3 timeout (unchanged — cycling fix needs perturbation)
