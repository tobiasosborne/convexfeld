# Agent Handoff

*Last updated: 2026-01-29*

---

## STATUS: Phase II Objective Bug Partially Fixed

### Session Summary

Made significant progress on bug **convexfeld-p7wr** (Phase II returns objective = 0):

#### Root Cause Analysis
Found TWO bugs causing Phase II to return obj=0:

1. **Artificial variables re-entering basis in Phase II**
   - Auxiliary variables for = constraints (artificials) had obj coeff = 0 in Phase II
   - Their reduced cost was dj = 0 - π[row] * coeff
   - Large dual prices made artificials attractive for entering
   - When they re-entered with positive values, solution became infeasible
   - **Fix:** In `transition_to_phase_two()`, set ub=0 for auxiliaries of = constraints

2. **Step size not considering entering variable bounds**
   - iterate.c computed step size based only on LEAVING variable bounds
   - For bounded entering variables (like fixed artificials), should also limit by entering bound
   - **Fix:** Added code in iterate.c to limit stepSize by `(ub - current)` for entering variable

#### Results
- **ship04l:** obj=1.86e+06 (was 0, ref=1.79e+06) - 3.5% error vs 100% error before
- **afiro, sc50b, sc105:** Still PASS
- **kb2:** Still fails (returns 0) - may need additional investigation
- Some benchmarks now closer to correct but not yet within tolerance

### Test Status

- **Unit Tests:** 35/36 pass (97%) - same as before
- **Build:** Clean, no warnings
- **Netlib:** Partial improvement on objective=0 issue

---

## Immediate Next Steps

### Bug Resolution (P1) - In Progress
```
convexfeld-p7wr: PARTIAL - Some benchmarks fixed, others still fail
  - ship04l improved (3.5% error)
  - kb2, woodw still need work
  - May need to investigate why some = constraints still allow artificial re-entry
```

### Remaining Cleanup (P1)
```
convexfeld-4gfy: Remove diag_coeff hack after LU implementation
convexfeld-8vat: Re-enable periodic refactorization (REFACTOR_INTERVAL)
convexfeld-1x14: BUG: Phase I stuck with all positive reduced costs
```

---

## Files Modified This Session

| File | Changes |
|------|---------|
| `src/simplex/solve_lp.c` | Fixed artificial variable bounds in transition_to_phase_two() |
| `src/simplex/iterate.c` | Added step size limiting for bounded entering variables |

---

## Key Learning: Bounded Variable Simplex

For LPs with BOUNDED variables (especially fixed variables with lb=ub):

1. **Pricing:** Variables fixed at a bound shouldn't be selected for entering
2. **Step size:** Must consider BOTH leaving variable bound AND entering variable bound
3. **Artificials:** Must be FIXED at zero in Phase II, not just have obj coeff = 0

Standard simplex assumes unbounded variables. For bounded variables:
```
stepSize = min(
    ratio_from_leaving_variable,
    max_step_for_entering_variable
)
```

For a variable at lower bound trying to increase:
```
max_step_for_entering = ub - current_value
```

If ub = lb = 0 (fixed), then max_step = 0, so pivot is degenerate.

---

## Quality Gate Status

- **Tests:** 35/36 pass (97%)
- **Build:** Clean
- **Netlib:** Partial improvement, ongoing investigation

---

## Open Issues Requiring Follow-up

1. **kb2 returns 0:** 41x43 problem, takes 7+ seconds (should be instant), returns obj=0
2. **ship04l 3.5% error:** Close but not passing - may need tolerance adjustment or other fix
3. **Some benchmarks still UNBOUNDED:** adlittle reported UNBOUNDED in earlier test (needs investigation)
