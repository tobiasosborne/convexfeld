# Agent Handoff

*Last updated: 2026-01-28*

---

## STATUS: BTRAN Fix Complete, Slack Variables Still Needed

### What Was Fixed This Session

**BTRAN-based dual price computation implemented.** Added `cxf_btran_vec()` function to compute dual prices properly using BTRAN with arbitrary input vector.

### Files Modified

| File | Change |
|------|--------|
| `src/basis/btran.c` | Added `cxf_btran_vec()` - BTRAN with arbitrary input |
| `include/convexfeld/cxf_basis.h` | Declared `cxf_btran_vec()` |
| `src/simplex/solve_lp.c` | Updated `compute_reduced_costs()` to use BTRAN |
| `src/simplex/iterate.c` | Updated reduced cost computation to use BTRAN |
| `tests/unit/test_basis.c` | Added tests for `cxf_btran_vec()` |

### Verification

**Equality constraints now work correctly:**
```
Problem: min -x s.t. x = 2
Result: x=2, obj=-2 ✓
```

**Inequality constraints still fail** due to missing slack variables:
```
Problem: min -x -y s.t. x+y<=4, x<=2, y<=3
Result: INFEASIBLE (wrong - should be x=2, y=2, obj=-4)
```

---

## ROOT CAUSE: Two Separate Issues

### Issue 1: BTRAN approximation (FIXED ✓)
- **Was**: `pi[i] = work_obj[basic_vars[i]]` (only correct when B = I)
- **Now**: `cxf_btran_vec(basis, cB, work_pi)` (correct for all bases)
- **Issue**: convexfeld-kkmu - **CLOSED**

### Issue 2: No slack variables (NOT FIXED)
- **Problem**: For `<=` constraints, solver uses artificials directly instead of slack variables
- **Effect**: Can't represent feasible solutions for inequality constraints
- **Issue**: convexfeld-nd1r - **STILL OPEN** (P1)

The correct approach for `<=` constraints:
1. Add slack variable s_i >= 0 to convert to equality: Ax + s = b
2. Slack starts basic at value = RHS - (row activity at starting point)
3. Only add artificial if slack would be negative

---

## Next Steps for Next Agent

### BLOCKING ISSUE: convexfeld-nd1r (Slack Variables)

This is the remaining blocker for multi-constraint LPs. The fix requires:

1. **In setup_phase_one():**
   - For `<=` constraints: add slack variable (no artificial needed if slack >= 0)
   - For `>=` constraints: add surplus variable (may need artificial if surplus < 0)
   - For `=` constraints: add artificial (current behavior)

2. **Array sizing:**
   - Need space for n + m_slack + m_artificial variables
   - Currently only allocates n + m

3. **Constraint transformation:**
   - Must convert inequalities to equalities before Phase I

### Test Commands

```bash
# Test equality constraints (should pass)
/tmp/test_equality

# Test inequality constraints (currently fails - needs slack vars)
/tmp/test_btran_fix
```

---

## Issues Status

| ID | Priority | Status | Title |
|----|----------|--------|-------|
| convexfeld-kkmu | P0 | CLOSED | BTRAN not used for dual prices |
| convexfeld-nd1r | P1 | OPEN | No slack variable introduction |
| convexfeld-c251 | P0 | OPEN | step.c artificial variable updates |
| convexfeld-fyi2 | P0 | OPEN | Review simplex specs |
| convexfeld-c4bh | P1 | OPEN | Constraint satisfaction verification |

---

## Quality Gate Status

- Tests: 30/32 pass (same as before - 2 pre-existing failures)
- Build: Clean
- New tests: 5 tests for `cxf_btran_vec()` - all pass
