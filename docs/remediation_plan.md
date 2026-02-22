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
| **RC2:** Phase I conditional diag_coeff | 6 false INFEASIBLE + 2 wrong obj (israel, brandy) | P1 — High | Medium (redesign) |
| **RC3:** Missing OBJSENSE parsing | 3 wrong objective (stand*) | P2 — Medium | Low (add feature) |
| **RC4:** RANGES section silently ignored | 1 wrong objective (forplan) | P2 — Medium | Medium (add feature) |

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

## RC2: Fix Phase I diag_coeff — Use Fixed Sign Per Constraint Sense

### What's Wrong

For ALL constraint types, `phase_one.c` conditionally sets `diag_coeff` based on
whether the initial point satisfies the constraint. When the constraint is violated,
the sign flips — this creates the wrong algebraic representation in the basis matrix.

**<= constraints (lines 84-94):**
- Satisfied (slack >= 0): `diag = +1` (CORRECT)
- Violated (slack < 0): `diag = -1` (WRONG — should still be +1)

**>= constraints (lines 95-106):**
- Satisfied (surplus >= 0): `diag = -1` (CORRECT)
- Violated (surplus < 0): `diag = +1` (WRONG — should still be -1)

### Evidence

**>= violations → false INFEASIBLE:**
- scorpion: 53 violated >= → 53 diag = +1 mismatches → Phase I stalls at obj=0.036

**<= violations → constraint violations → wrong objective:**
- israel: 8 constraints with negative RHS → diag = -1 instead of +1 →
  solver declares OPTIMAL but solution violates 8 constraints by up to 2000
  → objective -981,118 vs reference -896,644 (9.4% error)
- brandy: similar but milder drift (0.037% error)

### Fix

**File: `src/simplex/phase_one.c`**

diag_coeff must be UNCONDITIONAL per constraint sense:
- `<=` → `diag = +1.0` (ALWAYS)
- `>=` → `diag = -1.0` (ALWAYS)
- `=`  → `diag = +1.0` (ALWAYS, sign of auxiliary doesn't depend on RHS)

```c
if (sense == '<' || sense == 'L') {
    diag = 1.0;  // ALWAYS +1 for <= (slack direction)
    if (slack_val >= 0) {
        state->work_x[var_idx] = slack_val;
        state->work_obj[var_idx] = 0.0;
    } else {
        state->work_x[var_idx] = -slack_val;
        state->work_obj[var_idx] = 1.0;
        state->num_artificials++;
    }
} else if (sense == '>' || sense == 'G') {
    diag = -1.0;  // ALWAYS -1 for >= (surplus direction)
    double surplus_val = row_sum - rhs;
    if (surplus_val >= 0) {
        state->work_x[var_idx] = surplus_val;
        state->work_obj[var_idx] = 0.0;
    } else {
        state->work_x[var_idx] = -surplus_val;
        state->work_obj[var_idx] = 1.0;
        state->num_artificials++;
    }
} else { /* = */
    diag = 1.0;  // ALWAYS +1 for = (artificial direction)
    state->work_x[var_idx] = fabs(slack_val);
    state->work_obj[var_idx] = 1.0;
    if (fabs(slack_val) > CXF_FEASIBILITY_TOL)
        state->num_artificials++;
}
```

**CRITICAL:** When `diag` is fixed but `slack_val < 0`, the auxiliary value
`work_x[var_idx] = |slack_val|` is positive. The constraint equation
`a'x + diag * aux = b` must be verified:
- For `<=` with `diag=+1`: `a'x + aux = b` → `aux = b - a'x = slack_val < 0`.
  But we stored `|slack_val|` — this is INCONSISTENT. The fix must also
  account for the sign. The auxiliary value should be `slack_val` (negative),
  and Phase I drives it toward zero (toward feasibility). OR: keep value
  positive and add sign tracking.

**This requires careful algebraic analysis.** The simplest correct approach
may be to always set `work_x[var_idx] = slack_val` (can be negative) and
accept that basic variables can be temporarily infeasible during Phase I.
Phase I's job is to drive them to feasibility.

**Also update Phase I→II transition** (lines 225-228) and remove the
diag_coeff flip logic since diag is now unconditional.

### Risk

Medium-High. Changes the Phase I optimization path fundamentally. The interaction
between diag_coeff sign, auxiliary variable value, and LU factorization initial
diagonal must be algebraically verified. Unit tests for all constraint types
(<=, >=, =) with both satisfied and violated initial points are essential.

### Validation

- scorpion, bandm, bore3d, e226, stair, tuff should change from INFEASIBLE
- israel, brandy constraint violations should disappear
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

| Category | Current | After RC1 | After RC2 | After RC3+RC4 |
|----------|---------|-----------|-----------|---------------|
| PASS | 19 | ~37+ | ~45+ | ~49+ |
| False UNBOUNDED | 18 | ~0 | ~0 | ~0 |
| False INFEASIBLE | 6 | ~6 | ~0 | ~0 |
| Wrong Objective | 6 | ~6 | ~2 | ~0 |
| TIMEOUT | 65 | ~65 | ~65 | ~65 |

**All 30 non-timeout failures now have identified root causes:**
- 18 false UNBOUNDED → RC1 (BFRT row negation)
- 6 false INFEASIBLE → RC2 (conditional diag_coeff for >=)
- 2 wrong objective (israel, brandy) → RC2 (conditional diag_coeff for <=)
- 3 wrong objective (stand*) → RC3 (missing OBJSENSE)
- 1 wrong objective (forplan) → RC4 (missing RANGES)

The timeout instances require performance optimization (sparse LU, etc.) which
is a separate concern from correctness.

---

## Files Modified

| Fix | Files Changed | LOC Impact |
|-----|--------------|------------|
| RC1 | step.c | -50 (delete negate_constraint_row) |
| RC2 | phase_one.c | ~10 (modify >= handling) |
| RC3 | mps_parse.c, cxf_model.h, solve_lp.c | ~30 (add feature) |

| RC4 | mps_parse.c, mps_build.c | ~40 (add RANGES handling) |

Total: ~130 LOC changed. Conservative, targeted fixes.

---

## RC4: Parse RANGES Section in MPS Parser

### What's Wrong

The MPS parser recognizes `RANGES` as a section header (line 44) but has no
handler for it — lines in the RANGES section are silently ignored (no
`case SEC_RANGES:` in the switch at line 167-173).

RANGES converts one-sided constraints into two-sided range constraints:
- For `L` row with RHS b and range r: becomes `b - |r| <= a'x <= b`
- For `G` row with RHS b and range r: becomes `b <= a'x <= b + |r|`
- For `E` row with RHS b and range r: becomes `b <= a'x <= b + |r|` (r > 0)
  or `b + r <= a'x <= b` (r < 0)

### Evidence

- forplan has `RANGES LTSYCT 284990` — constraint LTSYCT should be a range
  constraint but is treated as one-sided → 43% objective error

### Fix

**File: `src/api/mps_parse.c`**
- Add `case SEC_RANGES: status = parse_ranges_line(s, p); break;`
- Implement `parse_ranges_line()` that adjusts constraint bounds

**File: `src/api/mps_build.c`** (or wherever constraints are finalized)
- Convert range constraints into appropriate bound adjustments on slack variables

### Risk

Medium. Requires understanding MPS RANGES semantics (sign-dependent behavior).
Must not break instances without RANGES.

### Validation

- forplan should produce correct objective
- All currently-passing instances unchanged
