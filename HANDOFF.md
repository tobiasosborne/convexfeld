# Agent Handoff

*Last updated: 2026-02-26*

---

## STATUS: REGRESSION from 4nrf. Unit tests 45/45 pass but Netlib regressions.

### Latest Change (2026-02-26): Removed dead V1 pricing function

**convexfeld-l0ca closed.** Removed `cxf_pricing_update` (V1) from `update.c` — a dead function containing a broken SE weight-update stub that suppressed all nonbasic weight updates with `(void)gamma_entering`. The function was:
- Not declared in `cxf_pricing.h` (not part of public API)
- Never called from any production code in `src/`
- Only tested by unit tests that verified incorrect behavior

The correct, active implementation is `cxf_pricing_update_weights` in `weight_update.c` (V2, P4.9), which properly implements simplified one-BTRAN DSE (Forrest & Goldfarb 1992). Called from `step.c` Phase 7b.

Files changed: `src/pricing/update.c`, `src/pricing/pricing_stub.c`, `tests/unit/test_pricing.c`. 45/45 tests pass. No Netlib behavior change (confirmed share2b OPTIMAL).

### Spot Check Results (post-4nrf)

| Instance | Before | After | Notes |
|----------|--------|-------|-------|
| afiro | PASS | PASS | No change |
| sc50b | PASS (2.4e-4) | PASS (2e-16) | **Improved** |
| blend | PASS | FAIL (39% off) | **REGRESSION** |
| brandy | PASS | ITER_LIMIT | **REGRESSION** |
| scorpion | PASS | INFEASIBLE | **REGRESSION** |
| e226 | PASS | INFEASIBLE | **REGRESSION** |
| etamacro | FAIL (0.016%) | INFEASIBLE | was FAIL |
| boeing2 | FAIL (2.4%) | INFEASIBLE | was FAIL |

### Root Cause

convexfeld-4nrf initialized activity bounds with `-rhs` instead of `0` per spec. Mathematically correct — the step2/step3 formulas (`lb - min_act/a`) were derived for RHS-inclusive activity. However, the changed activity values cause bound propagation to produce different implied bounds that cascade into false infeasibility or wrong pivoting paths. The solver's other subsystems were implicitly calibrated around zero-init activity.

### What's Currently Deployed

1. **convexfeld-ic80** (Phase I→II constraint cleanup) — `post.c` slack uses `rhs - max_a`, activity recomputed at transition. Tests pass.
2. **convexfeld-4nrf** (Activity -rhs init) — `setup.c` inits with `-rhs`, `post.c` slack simplified to `-max_a`. **Causes regressions.**

### Next Steps (for next agent)

1. **Investigate** why the spec-correct `-rhs` init causes regressions. Key suspects:
   - step3 infeasibility check (`min_act > tol`) fires more aggressively with RHS offset
   - Bound propagation produces tighter implied bounds that the numerical engine can't sustain
   - Phase I w-coefficient interaction with shifted activity values
2. May need to make the rest of the solver robust enough before enabling `-rhs` init
3. Or revert 4nrf and fix step2/step3 formulas to explicitly add `rhs/a` instead

---

## DO NOT
- Revert without understanding root cause — the fix IS mathematically correct
- Enable scaling without testing H1 (Harris tolerance) first
- Change diag_coeff based on RHS sign (causes regressions on israel/stair/e226)
- Re-add RC-based status reassignment in refine.c (corrupts obj)
- Hack refactorization parameters to fix primal accuracy — needs sparse LU
- Use `cols_eliminated` counter for constraint cleanup — it feeds stall detection
