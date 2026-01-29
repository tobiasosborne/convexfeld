# Agent Handoff

*Last updated: 2026-01-29*

---

## STATUS: Partial Fix for False INFEASIBLE - More Work Needed

### What Was Done This Session

**Fixed:** Major Phase I bug where auxiliary variable coefficients were computed incorrectly.

**Root Cause Found:**
1. The auxiliary coefficient for slack/surplus/artificial variables depends on BOTH constraint sense AND initial feasibility
2. For constraints violated at initial point (x at lower bounds), the coefficient sign was wrong
3. The product form of inverse (FTRAN/BTRAN) wasn't applying the initial diagonal correctly

**Key Changes:**

1. **BasisState** (`include/convexfeld/cxf_basis.h`):
   - Added `double *diag_coeff` field to store initial basis diagonal coefficients

2. **Basis state management** (`src/basis/basis_state.c`):
   - Allocate and free `diag_coeff` array
   - Initialize to identity (+1) by default

3. **FTRAN** (`src/basis/ftran.c`):
   - Apply `diag_coeff` BEFORE eta transformations

4. **BTRAN** (`src/basis/btran.c`):
   - Apply `diag_coeff` AFTER eta transformations

5. **Phase I setup** (`src/simplex/solve_lp.c`):
   - Compute `diag_coeff[i]` based on constraint feasibility at initial point
   - For violated constraints, coefficient = -1 to make auxiliary positive

6. **Iterate** (`src/simplex/iterate.c`):
   - Use `basis->diag_coeff` for auxiliary columns in FTRAN
   - Use `basis->diag_coeff` for reduced cost updates

7. **Refactorization disabled** (`src/simplex/iterate.c`):
   - `REFACTOR_INTERVAL` set to 10000 (effectively disabled)
   - Reason: `cxf_basis_refactor` resets eta chain but doesn't have proper LU factorization
   - After refactor, FTRAN would use wrong diagonal, causing UNBOUNDED errors

---

## Benchmark Results After Fix

**Passing (3 tested):** afiro, sc50a, sc50b
**Failing with small error:** blend (0.03% objective error - may be tolerance issue)
**Still INFEASIBLE:** boeing2 and likely others

---

## CRITICAL: Next Steps

### Priority 1: Complete False INFEASIBLE Fix (convexfeld-zim5)

The fix is partial. Some benchmarks still report INFEASIBLE. Possible causes:

1. **Objective sense mismatch**: Some Netlib problems may be maximization, but we only minimize
2. **Remaining coefficient bugs**: Edge cases in the coefficient calculation
3. **Refactorization limitation**: Without proper LU refactor, long-running solves accumulate error

**Debug approach:**
1. Check if failing problems are maximization vs minimization
2. Add objective sense handling to MPS parser
3. Trace Phase I on a specific failing problem to find remaining bugs

### Priority 2: Implement Proper LU Refactorization (convexfeld-w9to)

The `cxf_basis_refactor` function is a stub. It clears etas but doesn't compute new factorization. This causes:
- Wrong FTRAN results after refactorization
- Currently worked around by setting `REFACTOR_INTERVAL=10000`
- Will cause numerical instability on large problems

### Priority 3: Wrong Objective Values (convexfeld-8rt5)

After fixing INFEASIBLE, some benchmarks solve but with wrong objective. This may be:
- Objective sense (min vs max)
- Solution extraction bug
- Numerical precision issues

---

## Technical Details

### Auxiliary Coefficient Logic

The coefficient for auxiliary variable in row `i` depends on:

```
For <= constraint with initial slack = rhs - Ax(lb):
  - If slack >= 0: coeff = +1 (normal slack)
  - If slack < 0:  coeff = -1 (artificial, violated)

For >= constraint with initial surplus = Ax(lb) - rhs:
  - If surplus >= 0: coeff = -1 (normal surplus)
  - If surplus < 0:  coeff = +1 (artificial, violated)

For = constraint with slack = rhs - Ax(lb):
  - If slack >= 0: coeff = +1
  - If slack < 0:  coeff = -1
```

This ensures the auxiliary variable value is always non-negative.

### FTRAN/BTRAN with Diagonal

With initial basis B_0 = D (diagonal), product form gives:
```
B = D * E_1 * E_2 * ... * E_k
B^(-1) = E_k^(-1) * ... * E_1^(-1) * D

FTRAN(a) = D * (apply etas oldest-to-newest) * a
BTRAN(c) = (apply etas newest-to-oldest) * D * c
```

---

## Quality Gate Status

- **Tests:** 34/35 pass (97%)
- **Build:** Clean
- **Netlib:** Improved from 21% to ~60% (estimate based on tested samples)

---

## Files Modified This Session

- `include/convexfeld/cxf_basis.h` - Added diag_coeff field
- `src/basis/basis_state.c` - Allocate/free diag_coeff
- `src/basis/ftran.c` - Apply diagonal before etas
- `src/basis/btran.c` - Apply diagonal after etas
- `src/basis/refactor.c` - Reset diag_coeff on refactor (temporary fix)
- `src/simplex/solve_lp.c` - Compute diag_coeff in Phase I setup
- `src/simplex/iterate.c` - Use diag_coeff for auxiliary columns and reduced costs
