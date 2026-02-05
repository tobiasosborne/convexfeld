# Agent Handoff

*Last updated: 2026-02-05*

---

## STATUS: Bland's Rule Infrastructure Added — Cycling Root Cause Identified

### Session Summary

Added Bland's rule anti-cycling infrastructure (entering + leaving rules). Diagnosed
the root cause of cycling on capri/grow7/seba but the fix is incomplete — the cycling
is a **degenerate 2-cycle** that Bland's rule alone doesn't resolve.

#### Changes Made

| File | Changes |
|------|---------|
| `include/convexfeld/cxf_solver.h` | Added `use_bland` and `degenerate_count` fields to SolverContext |
| `src/simplex/iterate.c` | Bland's entering rule (first attractive var by index, collects up to 10 candidates); cycling detection via degenerate pivot counter |
| `src/simplex/ratio_test.c` | Bland's leaving rule (smallest variable index among tied ratios when `use_bland` active) |
| `src/simplex/solve_lp.c` | Activates Bland's rule after 3*m iterations per phase; resets anti-cycling state at Phase II transition |

#### Root Cause Analysis: Capri Cycling

Debug showed a **degenerate 2-cycle** in Phase I:
- Iter N: entering=154 (at ub, rc=82.3), leaving=161 at row 240, step=0
- Iter N+1: entering=161 (at ub, rc=82.3), leaving=154 at row 240, step=0
- Variables 154/161 swap in/out of basis row 240 forever with zero progress

This is NOT a standard Bland's rule failure. The issue is:
1. Both variables are at **upper bound** with **identical positive reduced costs**
2. Step is always 0 (fully degenerate — leaving var already at bound)
3. Each degenerate pivot does nothing to the solution but alternates the basis

#### What Didn't Work
- Bland's entering rule alone (still cycles because 2 vars alternate)
- Bland's leaving rule (correctly picks smallest index but both vars are in same row)
- Degenerate step detection with threshold (step was literally -0.0, not just small)

### Remaining Bugs

#### P0: Degenerate Cycling (3 problems — capri, grow7, seba)
Root cause identified. Needs one of:
1. **Bound flip**: When entering var at upper bound would cause step=0, flip it to lower bound WITHOUT basis change — avoids the degenerate pivot entirely
2. **Skip degenerate candidates**: In Bland's mode, if ratio test gives step=0, try next candidate from the collected list (infrastructure for this is partially in place — iterate.c collects up to 10 candidates)
3. **Stronger perturbation**: Perturb RHS (not just bounds) to break exact degeneracy

Option 2 is the simplest next step — the candidate list is already collected.

#### P0: Phase I false INFEASIBLE (8 problems)
- share1b, stair, degen2, boeing1, boeing2, bnl1, e226, scorpion
- Some may also be cycling in Phase I

#### P2: Small numerical errors (5 problems)
- kb2 (0.016%), adlittle (0.12%), recipe (2.4%), scagr7 (1.8%), israel (9.4%)

---

## Next Steps

### Priority 1: Fix Degenerate Cycling
The candidate infrastructure is in place. Next agent should:
1. In iterate.c, after computing stepSize, if `use_bland && stepSize < 1e-12`, loop to try `candidates[1]`, `candidates[2]`, etc. instead of always using `candidates[0]`
2. If ALL candidates give step=0, accept the degenerate pivot with candidates[0] (can't avoid it)
3. This requires moving Steps 2-4 (FTRAN, ratio test, step computation) into a loop over candidates

### Priority 2: Phase I False INFEASIBLE
- 8 problems return INFEASIBLE but are feasible
- After cycling fix, re-test — some may be cycling in Phase I

### Priority 3: Numerical Refinement
- kb2, adlittle very close — tighter pivot tolerance or iterative refinement

---

## Quality Gate Status

- **Tests:** 35/36 pass (pre-existing `test_simplex_edge` failure)
- **Build:** Clean
- **Netlib:** 11 pass, 13 fail, 3 timeout (unchanged — cycling fix incomplete)
