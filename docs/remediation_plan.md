# Remediation Plan — Netlib Failure Investigation

*Date: 2026-02-22*
*Based on: docs/architecture_contract_map.md (Component Interface Contract Map)*

---

## Summary

Three root causes explain 27 of 30 non-timeout Netlib failures.
Remaining 3 (brandy, forplan, israel) likely resolve after fixing #1.

| Root Cause | Failures Explained | Priority | Estimated Complexity |
|-----------|-------------------|----------|---------------------|
| **RC1:** BFRT `negate_constraint_row` | 18 false UNBOUNDED | P0 — Critical | Low (delete code) |
| **RC2:** Phase I >= diag_coeff | 6 false INFEASIBLE | P1 — High | Medium (redesign) |
| **RC3:** Missing OBJSENSE parsing | 3 wrong objective (stand*) | P2 — Medium | Low (add feature) |

---

## RC1: Remove `negate_constraint_row` from BFRT Path

### What's Wrong

After BFRT flips a basic variable, `negate_constraint_row()` negates the CSR/CSC
row coefficients, RHS, and diag_coeff. But the LU factorization (eta vectors)
is NOT updated. This makes all subsequent FTRAN/BTRAN results wrong, causing
cumulative bound violations until the ratio test can't find valid candidates.

### Evidence

- recipe: 12 BFRT flips → 12 basic vars past bounds → UNBOUNDED at iter 152
- grow7: 8 BFRT flips → worst infeasibility 3.58e8 → UNBOUNDED at iter 347
- boeing2: 262 BFRT flips → worst infeasibility 1.45e5 → UNBOUNDED at iter 156

### V2 Spec Note

P3.5 references "harris_ratio_test.md Stage 3 Step 6c" for row negation.
**The spec may be incorrect here.** Standard BFRT (Koberstein 2005, Maros 2003)
does NOT negate constraint rows. The BFRT procedure is:

1. Find leaving variable (hits bound)
2. If leaving var has both finite bounds: flip to opposite bound, extend step
3. Find next blocker
4. Repeat until no more flips or max flips reached
5. Pivot the final leaving variable out

No matrix modification is required. The pivot column doesn't change during BFRT
(it's precomputed by FTRAN). Only the step size accumulates.

### Fix

**File: `src/simplex/step.c`**

1. **Delete** the `negate_constraint_row()` function (lines 96-140)
2. **Delete** the call at lines 576-577:
   ```c
   for (int f = 0; f < num_flips; f++)
       negate_constraint_row(state, flipped_rows[f]);
   ```
3. **Verify** that the BFRT clamping logic (lines 606-613) still works correctly
   without negation — it should, since it only sets `work_x[bv]` to opposite bound
4. **Run** all 18 false UNBOUNDED instances to confirm fix
5. **Run** full unit test suite (40 tests) to confirm no regression

### Risk

Low. Removing code that actively corrupts the matrix. The BFRT flip logic
(clamp variable to opposite bound) remains intact. Standard BFRT doesn't need
row negation.

### Validation

- All 18 false UNBOUNDED should change status (OPTIMAL, or different failure)
- Unit tests should pass
- Instances without BFRT (0 flips) should be unchanged

---

## RC2: Fix Phase I >= Constraint Formulation

### What's Wrong

For >= constraints with violated initial point, `phase_one.c:102` sets
`diag_coeff = +1.0`. This means the constraint is formulated as `a'x + aux = b`
(artificial direction) instead of `a'x - aux = b` (surplus direction).

Without a surplus variable, the solver has fewer pivot options. Phase I can
get permanently stuck at a non-zero objective, declaring feasible problems
as infeasible.

### Evidence

- scorpion: 53 violated >= → 53 diag_coeff = +1.0 mismatches → Phase I stalls
  at obj = 0.036 → false INFEASIBLE
- All 6 false INFEASIBLE instances have >= constraints

### Fix

**File: `src/simplex/phase_one.c`**

For >= constraints (lines 95-106), change to always use `diag = -1.0`:

```c
} else if (sense == '>' || sense == 'G') {
    double surplus_val = row_sum - rhs;  // surplus = a'x - b
    diag = -1.0;  // ALWAYS -1 for >= (surplus direction)
    if (surplus_val >= 0) {
        // Constraint satisfied: surplus is non-negative
        state->work_x[var_idx] = surplus_val;
        state->work_obj[var_idx] = 0.0;
    } else {
        // Constraint violated: surplus is negative
        // Set value to |surplus| and use artificial objective
        state->work_x[var_idx] = -surplus_val;
        state->work_obj[var_idx] = 1.0;
        state->num_artificials++;
    }
}
```

**IMPORTANT:** With this fix, when the surplus value is negative (constraint
violated), `work_x[var_idx] = -surplus_val > 0` but the constraint equation
`a'x + diag*aux = b` gives `a'x - aux = b` → `aux = a'x - b = surplus < 0`.
The value in `work_x` should be `|surplus|` which represents the infeasibility.
Phase I will minimize this (obj_coeff = 1.0), driving it toward zero.

**Also update Phase I→II transition** (lines 225-228): The transition code
currently checks `diag_coeff[i] > 0` and flips to -1.0. After this fix,
diag is already -1.0, so the transition code can be simplified/removed.

### Risk

Medium. Changes the Phase I optimization path. Must verify:
- Phase I objective = sum of infeasibility values is computed correctly
- LU factorization initial diagonal uses correct diag_coeff
- Phase I→II transition logic still works
- Problems that currently pass still pass

### Validation

- All 6 false INFEASIBLE should change status
- Full unit test suite passes
- Full Netlib suite retested

---

## RC3: Add OBJSENSE Parsing to MPS Parser

### What's Wrong

The MPS parser (`src/api/mps_parse.c`) doesn't recognize the OBJSENSE section.
Maximization problems are solved as minimizations, producing sign-inverted
objectives. CxfModel has no `obj_sense` field.

### Evidence

- standata, standgub, standmps: objective sign is inverted (negative vs positive)

### Fix

**File: `include/convexfeld/cxf_model.h`**
- Add field: `int obj_sense;  /* 1=minimize (default), -1=maximize */`

**File: `src/api/mps_parse.c`**
- Add OBJSENSE section recognition in `get_section()`
- Parse "MAX" / "MAXIMIZE" → set `model->obj_sense = -1`
- Parse "MIN" / "MINIMIZE" → set `model->obj_sense = 1` (default)

**File: `src/simplex/solve_lp.c`**
- If `model->obj_sense == -1`: negate all objective coefficients before solving
- After solving: negate the reported objective value

### Risk

Low. Additive feature. Default behavior (minimize) unchanged.

### Validation

- standata, standgub, standmps should produce correct sign
- All currently-passing instances unchanged

---

## Execution Order

```
RC1 (BFRT) ──► Retest Netlib ──► RC2 (Phase I >=) ──► Retest ──► RC3 (OBJSENSE) ──► Final retest
```

RC1 first because:
1. It's the highest-impact fix (18 instances)
2. It's the simplest fix (delete code)
3. It may fix some of the "wrong objective" cases too (brandy, forplan, israel)
4. It validates the diagnostic tool and approach

After RC1, re-run all Netlib to get updated baseline before RC2.

---

## Post-Fix Expected Results

| Category | Current | After RC1 | After RC2 | After RC3 |
|----------|---------|-----------|-----------|-----------|
| PASS | 19 | ~37+ | ~43+ | ~46+ |
| False UNBOUNDED | 18 | ~0 | ~0 | ~0 |
| False INFEASIBLE | 6 | ~6 | ~0 | ~0 |
| Wrong Objective | 6 | ~3 | ~3 | ~0 |
| TIMEOUT | 65 | ~65 | ~65 | ~65 |

The timeout instances require performance optimization (sparse LU, etc.) which
is a separate concern from correctness.

---

## Files Modified

| Fix | Files Changed | LOC Impact |
|-----|--------------|------------|
| RC1 | step.c | -50 (delete negate_constraint_row) |
| RC2 | phase_one.c | ~10 (modify >= handling) |
| RC3 | mps_parse.c, cxf_model.h, solve_lp.c | ~30 (add feature) |

Total: ~90 LOC changed. Conservative, targeted fixes.
