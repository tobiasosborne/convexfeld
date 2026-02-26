# Agent Handoff

*Last updated: 2026-02-26*

---

## STATUS: REGRESSION from 4nrf. Unit tests 45/45 pass but Netlib regressions.

### Latest Changes (2026-02-26)

**convexfeld-36qh closed.** Rewrote basis snapshot/diff system per spec (basis_operations.md, perturbation.md):
- Snapshot expanded 8→10 slots: wired `ftran_count` (slot 2, was placeholder), added `degenerate_count` (slot 8), `perturb_count` (slot 9)
- `cxf_basis_diff` rewritten with 4 weighted categories: structural (w=4.0, colDenom-normalized), iteration (w=0.25, colDenom-normalized), propagation (w=1.0, rowDenom-normalized), work (w=0.5, rowDenom-normalized). Deltas clamped >=0.
- Threshold formula fixed: `CONVERGENCE_BASE/(1+round)` → `max(0, k-5)*CONVERGENCE_BASE` per spec. Grace period now integral to formula, not separate guard.
- Files: `cxf_solver.h`, `basis_stub.c`, `solve_lp.c`, `test_basis.c`. 45/45 tests pass. share2b+afiro OPTIMAL.

**convexfeld-s9am closed.** Re-enabled BFRT long-step ratio test using standard clamping (Koberstein 2005). No row negation (the previous approach that corrupted LU/eta). Flipped basic vars stay in basis, clamped to exact opposite bounds. Only the final leaving variable creates an eta vector. Loop collects up to 10 flippable blockers, extending step by `(ub-lb)/|d_i|`. Disabled under Bland's rule.
- Files: `step.c` (Phase 3 loop, ~25 lines). All dead-code infrastructure was correct.

**convexfeld-lmkg closed.** Added equality constraint column scan (Phase 3) to `cxf_pivot_special` per `pivot_operations.md`. Variables in equalities return CXF_OK.
- Files: `pivot_special.c`, `test_pivot_special.c`.

**convexfeld-l0ca closed.** Removed dead V1 `cxf_pricing_update` (broken SE weight stub, never called). V2 `cxf_pricing_update_weights` is correct and active.
- Files: `update.c`, `pricing_stub.c`, `test_pricing.c`.

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
