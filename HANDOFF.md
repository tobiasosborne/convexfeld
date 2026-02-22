# Agent Handoff

*Last updated: 2026-02-22*

---

## STATUS: RC1 done, RC2 in progress (separate artificials 90% complete). 39/40 tests pass.

### What Was Done This Session

1. **Built Component Interface Contract Map** (`docs/architecture_contract_map.md`)
2. **Built diagnostic tools** (`tools/diagnose.c`, `tools/trace_bfrt.c`)
3. **Found all 30/30 Netlib failure root causes** (`docs/remediation_plan.md`)
4. **Confirmed v2 spec bug** in BFRT row negation (P3.5)

5. **Implemented RC1: Disabled BFRT**
   - Deleted `negate_constraint_row` (spec bug: dual technique in primal context)
   - Found second BFRT bug: `find_next_blocker` skipped variables between old/extended step
   - Disabled BFRT entirely as simplest correct fix
   - Result: +1 PASS (etamacro), 4 reclassified UNBOUNDED→INFEASIBLE

6. **Implemented RC2 partial: Phase I→II transition fix for <= constraints**
   - Added missing diag_coeff flip from -1 to +1 for <= at transition
   - Result: +1 PASS (israel)

7. **Implementing RC2 complete: Separate artificial variables** (IN PROGRESS)
   - Added `art_coeff[m]` to SolverState
   - Expanded all arrays to `n + 2*m` (14 files changed)
   - Rewrote `cxf_setup_phase_one` with separate slack/artificial
   - Updated `extract_column_ext`, `reduced_costs`, `ratio_test`, `lu_factorize`, `refactor`
   - 39/40 tests pass. ONE failure remains: `test_geq_constraints`

### Current Bug: test_geq_constraints (obj=0, expected=5)

**Problem:** `min x0+x1 s.t. x0>=2, x1>=3`. Phase I enters SURPLUS variables
(not structurals) to drive artificials out. After transition, surpluses are
basic, structurals at lb=0. Phase II declares OPTIMAL at obj=0.

**Root cause:** Phase II reduced costs for structurals are wrong (dj=+1, should
be negative to attract them into basis). The dual prices after transition
aren't correctly reflecting the >= constraint values. The surplus variables
absorbed the constraints without the structurals moving.

**Investigation needed:** Check `cxf_compute_reduced_costs` after transition.
The dual prices pi should be computed from B^T * pi = c_B. With surpluses
basic (obj=0), pi = 0, so structural reduced costs = c_j - 0 = c_j > 0.
This is "correct" in the sense that at x=0 with surplus=surplus_val, the
basis IS optimal for the current basis — but the basis should have structurals,
not surpluses.

**Possible fixes:**
1. In Phase I, prefer structurals over surpluses when entering (modify pricing)
2. In transition, pivot surpluses out and structurals in (like artificial pivot-out)
3. Accept this as correct Phase I behavior — Phase II should eventually find better basis

Fix 3 is the right answer IF Phase II reduced costs are computed correctly.
The issue may be that surpluses for >= constraints should NOT be nonbasic at 0
after Phase I — they should track the constraint activity.

---

## Next Steps

1. **Debug test_geq_constraints**: trace reduced costs and dual prices after
   Phase I→II transition for `min x+y s.t. x>=2, y>=3`
2. **Run Netlib key instances** after fixing: scorpion, boeing1, bandm
3. **Then RC3 (OBJSENSE) and RC4 (RANGES)**

## Key Files Modified This Session

| File | Change |
|------|--------|
| `src/simplex/step.c` | BFRT disabled, 3-range column extraction, n+2m totals |
| `src/simplex/phase_one.c` | Separate artificials, transition simplified |
| `src/simplex/context.c` | n+2m array allocation, art_coeff |
| `src/simplex/reduced_costs.c` | 3-range RC computation |
| `src/simplex/ratio_test.c` | n+2m bounds in both passes |
| `src/simplex/phase_loop.c` | Phase I obj over artificial range |
| `src/basis/lu_factorize.c` | 3-range basis extraction |
| `src/basis/refactor.c` | Diagonal fast-path for artificials |
| `include/convexfeld/cxf_solver.h` | art_coeff field |
| `docs/architecture_contract_map.md` | Contract map + post-RC1 landscape |
| `docs/remediation_plan.md` | Full plan with spec bug analysis |
| `tools/diagnose.c`, `tools/trace_bfrt.c` | Diagnostic tools |

## DO NOT
- Run full Netlib suite (use targeted --filter)
- Skip reading docs/remediation_plan.md
- Modify solver without understanding the architecture contract map
