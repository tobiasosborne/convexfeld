# cxf_pivot_special

**Module:** Ratio Test
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Handles special pivot cases that fall outside the standard primal/dual simplex pivot pattern, including unboundedness detection, free variable handling, and row elimination for variables that appear only in inequality constraints. Determines whether a variable can be moved to a bound or eliminated entirely from the problem, and detects when the problem is unbounded by analyzing objective improvement potential against constraint structure.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| env | void* | Environment pointer | Non-null, valid environment | Yes |
| state | void* | Solver state structure | Non-null, valid state | Yes |
| var | int | Variable index to analyze | 0 to numVars-1 | Yes |
| lb_limit | double | Lower bound limit for unbounded check | Positive value, typically infinity or large threshold | Yes |
| ub_limit | double | Upper bound limit for unbounded check | Positive value, typically infinity or large threshold | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | 0 (SUCCESS) if handled, 5 (UNBOUNDED) if problem unbounded, 0x2711 (OUT_OF_MEMORY) on allocation failure |

### 2.3 Side Effects

May modify solver state extensively depending on operation mode:
- Sets state (problemVar) to var if unbounded
- May fix variable at a bound (modifying bounds, objective, matrix)
- May eliminate rows from the problem (setting varStatus to -1)
- May call cxf_pivot_bound which has extensive side effects
- Updates pricing state and invalidates caches
- May allocate and modify eta vectors

## 3. Contract

### 3.1 Preconditions

- [ ] env and state pointers must be valid
- [ ] var must be a valid non-basic variable index
- [ ] Variable must have objective coefficient potentially beneficial for movement
- [ ] Matrix structure must be consistent
- [ ] Dual pricing arrays must be up-to-date (if used)
- [ ] Basis header and variable status arrays must be consistent

### 3.2 Postconditions

- [ ] If return is UNBOUNDED: state contains var index
- [ ] If return is SUCCESS and variable fixed: lb[var] = ub[var] = fixed value
- [ ] If return is SUCCESS and rows eliminated: varStatus[eliminated rows] = -1
- [ ] If return is SUCCESS and bound flip: cxf_pivot_bound postconditions apply
- [ ] Solver state remains consistent for continued optimization
- [ ] If return is OUT_OF_MEMORY: state may be partially modified (inconsistent)

### 3.3 Invariants

- [ ] Total number of rows and columns unchanged (rows may be marked inactive)
- [ ] Matrix sparsity structure preserved (entries marked -1, not deleted)
- [ ] Basis header consistency maintained

## 4. Algorithm

### 4.1 Overview

This function performs a sophisticated analysis to determine if a variable can be moved to a bound or eliminated entirely, or if doing so would reveal that the problem is unbounded. The function is called during simplex iterations when standard pivot operations are not applicable or when checking for problem unboundedness.

The algorithm operates in three conceptual phases:

1. **Classification:** Based on the variable's objective coefficient, determine if increasing or decreasing the variable would improve the objective. A positive objective coefficient means decreasing the variable improves the objective (for minimization); a negative coefficient means increasing improves it.

2. **Constraint Analysis:** Scan all constraints involving the variable to determine if the beneficial movement direction is constrained. If the variable appears only in inequality constraints that don't block movement, it may be eliminable. If it appears in any equality constraint, elimination is not possible.

3. **Action Selection:** Based on bounds and constraint structure, choose one of four actions:
   - No action (variable is properly constrained)
   - Unbounded (variable can improve objective indefinitely)
   - Fix at bound and eliminate rows (variable appears only in inequalities)
   - Bound flip (move to opposite bound)

The unboundedness detection is the critical feature: if a variable can improve the objective indefinitely without violating constraints, the LP is unbounded.

### 4.2 Detailed Steps

1. **Extract objective coefficient and bounds**
   - Get obj_coeff = objCoeffs[var]
   - Get lower_bound = lb[var]
   - Get upper_bound = ub[var]
   - Define objective threshold (typically 1e-10 for numerical significance)

2. **Determine beneficial movement directions**
   - Set can_decrease = true if obj_coeff > threshold (minimization benefits from decrease)
   - Set can_increase = true if obj_coeff < -threshold (minimization benefits from increase)

3. **Check for special variable flags**
   - If varFlags[var] has special constraint bits set:
     - Call cxf_special_check to verify if pivot is allowed
     - May disable movement if variable participates in SOS, indicator, or other general constraints

4. **Scan column to analyze constraint structure**
   - For each constraint i where var appears:
     - Check if constraint is equality (senses[i] = '=')
     - If equality found: return SUCCESS immediately (cannot eliminate)
     - Analyze coefficient sign to determine constraint implications:
       - Positive coefficient: limits variable increase
       - Negative coefficient: limits variable decrease
     - Update can_increase and can_decrease flags accordingly

5. **Determine operation mode**
   - If can_decrease = true (beneficial to decrease):
     - If upper_bound < lb_limit (bounded):
       - Mode = 2 (bound flip to upper bound)
     - Else if special_mode = 0 (unboundedness checks enabled):
       - If obj_coeff < -ub_limit (strong objective improvement):
         - Set state = var
         - Return UNBOUNDED
       - Else:
         - Call cxf_fix_variable to fix at lower bound
         - Mode = 1 (row elimination)
   - Else if can_increase = true (beneficial to increase):
     - If lower_bound > -lb_limit (bounded):
       - Mode = 2 (bound flip to lower bound)
     - Else if special_mode = 0:
       - If obj_coeff > ub_limit (strong objective improvement):
         - Set state = var
         - Return UNBOUNDED
       - Else:
         - Call cxf_fix_variable to fix at upper bound
         - Mode = 1 (row elimination)
   - Else:
     - Mode = 0 (no action)

6. **Execute action based on mode**
   - Mode 0 (no action): Return SUCCESS
   - Mode 1 (row elimination):
     - For each row i where var appears:
       - Mark row as eliminated: varStatus[i] = -1
       - Remove row from all columns (mark constraintRows entries as -1)
       - Update basis headers and pricing state
     - Mark variable as eliminated: colCounts[var] = -1, basisHeader[var] = -2
   - Mode 2 (bound flip):
     - Call cxf_pivot_bound(env, state, var, target_value, 0.0, 0)
     - Return its result code

### 4.3 Pseudocode

```
Input: var, lb_limit, ub_limit
Output: SUCCESS, UNBOUNDED, or OUT_OF_MEMORY

c ← obj[var]
l ← lb[var]
u ← ub[var]
θ ← objective significance threshold (e.g., 1e-10)

// Determine beneficial directions
can_decrease ← (c > θ)
can_increase ← (c < -θ)

// Check special constraints
IF varFlags[var] has special bits:
    disabled ← cxf_special_check(state, var)
    IF disabled:
        can_decrease ← FALSE
        can_increase ← FALSE

// Scan constraints
has_equality ← FALSE
FOR each constraint i in column[var]:
    IF senses[i] = '=':
        RETURN SUCCESS  // cannot eliminate with equalities

    a ← A[i,var]
    IF a > 0:
        can_increase ← FALSE  // positive coeff blocks increase
    ELSE IF a < 0:
        can_decrease ← FALSE  // negative coeff blocks decrease

// Determine action
IF can_decrease:
    IF u < lb_limit:  // bounded decrease
        target ← u
        RETURN cxf_pivot_bound(env, state, var, target, 0.0, 0)
    ELSE IF special_mode = 0:  // unbounded check enabled
        IF c < -ub_limit:
            state.problemVar ← var
            RETURN UNBOUNDED
        ELSE:
            cxf_fix_variable(env, state, var, l)
            Eliminate rows containing var
            Mark var as eliminated

ELSE IF can_increase:
    IF l > -lb_limit:  // bounded increase
        target ← l
        RETURN cxf_pivot_bound(env, state, var, target, 0.0, 0)
    ELSE IF special_mode = 0:
        IF c > ub_limit:
            state.problemVar ← var
            RETURN UNBOUNDED
        ELSE:
            cxf_fix_variable(env, state, var, u)
            Eliminate rows containing var
            Mark var as eliminated

RETURN SUCCESS
```

### 4.4 Mathematical Foundation

**Unboundedness Detection:**

For a minimization problem:

    minimize    c^T x
    subject to  Ax ≤ b
                l ≤ x ≤ u

The problem is unbounded if there exists j such that:
- c_j > 0, l_j = -∞, and A[i,j] ≥ 0 for all i (can decrease x_j indefinitely), or
- c_j < 0, u_j = +∞, and A[i,j] ≤ 0 for all i (can increase x_j indefinitely)

**Row Elimination Condition:**

If variable x_j appears only in inequality constraints and can be fixed at a bound without changing the objective significantly, the constraints become:

    ∑_{k≠j} A[i,k] x_k ≤ b_i - A[i,j] v

If this is satisfied for current solution, row i becomes redundant and can be eliminated.

**Special Mode Suppression:**

During Phase I of two-phase simplex, unboundedness checks are disabled (special_mode = 1) because:
- Objective is artificial (minimize sum of infeasibilities)
- Unboundedness in Phase I doesn't imply original problem unbounded
- Phase II will detect true unboundedness if present

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) if can_decrease and can_increase both false
- **Average case:** O(nnz_j) for column scan
- **Worst case:** O(nnz_j + ∑_i nnz_i) for row elimination mode

Where:
- nnz_j = nonzeros in column j
- nnz_i = nonzeros in rows being eliminated

### 5.2 Space Complexity

- **Auxiliary space:** O(1) for mode 0 and 2; O(nnz_j) for mode 1 (eta allocation)
- **Total space:** O(nnz_j) worst case

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| Problem is unbounded | 5 (UNBOUNDED) | Variable can improve objective indefinitely |
| Eta allocation fails | 0x2711 (OUT_OF_MEMORY) | Insufficient memory during row elimination |
| Fix operation fails | 0x2711 (OUT_OF_MEMORY) | cxf_fix_variable allocation failure |

### 6.2 Error Behavior

**On UNBOUNDED:** Sets state to problem variable index for error reporting. State remains consistent and can be queried for unbounded ray information.

**On OUT_OF_MEMORY:** State may be partially modified and inconsistent. Caller should abort solve or restore from checkpoint.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Fixed variable | lb[var] = ub[var] | Returns SUCCESS immediately (no movement possible) |
| Free variable | lb = -inf, ub = +inf | May detect unboundedness if objective nonzero |
| Zero objective | obj[var] = 0 | Both can_decrease and can_increase false, returns SUCCESS |
| Equality constraints | Any row has senses[i] = '=' | Returns SUCCESS (cannot eliminate) |
| Special mode active | state != 0 | Unboundedness detection disabled |
| General constraint | varFlags has special bits | Calls cxf_special_check for validation |
| Small objective | \|obj[var]\| < threshold | Treated as zero (no beneficial movement) |

## 8. Thread Safety

**Thread-safe:** No

This function modifies solver state extensively (fixes variables, eliminates rows, updates matrices). Not safe for concurrent execution even on different variables of the same model.

**Synchronization required:** Caller must hold exclusive lock on solver state

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_pivot_bound | Ratio Test | Execute bound flip operation |
| cxf_fix_variable | Simplex Core | Fix variable at specific value |
| cxf_special_check | Validation | Validate pivot allowed for general constraints |
| cxf_pricing_invalidate | Pricing | Invalidate pricing cache for affected variables |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_simplex_presolve | Simplex Core | Check for variables that can be eliminated |
| cxf_dual_iterate | Simplex Core | Dual simplex special handling |
| cxf_phase1_cleanup | Simplex Core | Phase I to Phase II transition |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_ratio_test | Standard complement: handles normal pivot case |
| cxf_pivot_check | Pre-validation: checks bound feasibility |
| cxf_unbounded_ray | Downstream: extracts unbounded ray if UNBOUNDED returned |
| cxf_presolve_singleton | Similar: eliminates singleton rows/columns |

## 11. Design Notes

### 11.1 Design Rationale

This function consolidates several related "special case" operations that don't fit the standard pivot pattern:

1. **Unboundedness detection** must be integrated with bound operations because it requires the same constraint analysis
2. **Row elimination** is performed here rather than in presolve because it can arise dynamically during simplex (as variables reach bounds, constraints may become redundant)
3. **Bound flipping** for non-standard cases (not arising from ratio test) needs the same bound movement infrastructure

Combining these operations reduces code duplication and ensures consistent handling of edge cases. The alternative would be multiple specialized functions with overlapping constraint analysis logic.

The special_mode flag provides a critical safety mechanism: during Phase I or other specialized simplex phases, unboundedness in the auxiliary objective doesn't imply unboundedness in the original problem. Disabling checks prevents false positives.

### 11.2 Performance Considerations

**Early return on equality:** Checking for equality constraints first enables early termination, avoiding expensive unboundedness analysis when elimination is impossible.

**Flag-based checks:** The varFlags check for general constraints prevents wasted work on variables that cannot be pivoted due to SOS, indicator, or other special structure.

**Lazy deletion:** Rows are marked as eliminated (varStatus = -1) rather than physically removed, avoiding expensive array compaction.

**Work tracking:** All operations update the work counter for accurate iteration accounting and refactorization timing.

### 11.3 Future Considerations

**Unbounded ray construction:** Could immediately construct unbounded ray (certificate of unboundedness) rather than just detecting.

**Batch elimination:** Could identify multiple eliminable variables simultaneously and batch the row removal operations.

**Conflict learning:** When unboundedness is detected, could record which constraints failed to bound the variable for conflict analysis in MIP.

## 12. References

- Dantzig, G.B. (1963). *Linear Programming and Extensions*. Princeton University Press. (Unboundedness detection)
- Andersen, E.D. and Andersen, K.D. (1995). "Presolving in linear programming." *Mathematical Programming*, 71(2), 221-245. (Row and column elimination)
- Bixby, R.E. (2002). "Solving real-world linear programs: a decade and more of progress." *Operations Research*, 50(1), 3-15. (Modern simplex special cases)
- Maros, I. (2003). *Computational Techniques of the Simplex Method*. Kluwer Academic Publishers. (Chapter 8: Handling degeneracy and special cases)

## 13. Validation Checklist

Before finalizing this spec, verify:

- [x] No code copied from implementation
- [x] Algorithm description is implementation-agnostic
- [x] All parameters documented
- [x] All error conditions listed
- [x] Complexity analysis complete
- [x] Edge cases identified
- [x] A competent developer could implement from this spec alone

---

*Reviewed by: Pending*

---
*Convexfeld LP Solver Specification*
*Based on published optimization literature*
