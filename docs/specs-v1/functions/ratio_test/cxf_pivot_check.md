# cxf_pivot_check

**Module:** Ratio Test
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Computes the tightest feasible lower and upper bounds for a variable based on constraint implications, used to validate bound flip operations before execution. Analyzes dual pricing arrays and constraint coefficients to determine the maximum range the variable can occupy while maintaining feasibility of all constraints. This pre-validation prevents infeasible bound flips that would violate constraint bounds.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| state | void* | Solver state structure | Non-null, valid state | Yes |
| var | int | Variable index to analyze | 0 to numVars-1 | Yes |
| tolerance | double | Numerical tolerance | Typically 1e-6 to 1e-9 | Yes |
| unused | void* | Reserved parameter (not used) | Any value | No |
| out_lb | double* | Output: tightest feasible lower bound | Can be NULL | No |
| out_ub | double* | Output: tightest feasible upper bound | Can be NULL | No |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | void | No return value |
| *out_lb | double | Tightest lower bound implied by constraints (if out_lb != NULL) |
| *out_ub | double | Tightest upper bound implied by constraints (if out_ub != NULL) |

### 2.3 Side Effects

None. This is a pure computation function that reads solver state but does not modify it.

## 3. Contract

### 3.1 Preconditions

- [ ] state pointer must be valid
- [ ] var must be a valid variable index
- [ ] Column structure must be consistent (colStarts, colCounts, colRows)
- [ ] Dual pricing arrays (dualLower, dualUpper) must be up-to-date
- [ ] Row/column flags must correctly count unbounded variables
- [ ] Bound arrays (lb, ub) must be initialized

### 3.2 Postconditions

- [ ] If out_lb != NULL: *out_lb contains most restrictive lower bound from all constraints
- [ ] If out_ub != NULL: *out_ub contains most restrictive upper bound from all constraints
- [ ] If *out_lb > *out_ub: bound flip to any value in between would be infeasible
- [ ] Bounds account for all active constraints involving the variable
- [ ] Bounds respect sign of constraint coefficients

### 3.3 Invariants

- [ ] Solver state is not modified
- [ ] Input arrays remain unchanged
- [ ] Function is idempotent (can be called multiple times with same result)

## 4. Algorithm

### 4.1 Overview

This function performs constraint propagation to determine implied bounds on a variable. It leverages the dual pricing arrays (dualLower, dualUpper) which maintain cumulative sums of bounded variable contributions to each constraint. By examining each constraint where the variable appears and computing how much the variable can change without violating that constraint, the function determines the tightest feasible range.

The algorithm distinguishes between inequality and equality constraints. For inequality constraints, the variable is bounded in only one direction (depending on coefficient sign). For equality constraints, both directions are constrained.

The row and column flags (rowFlags, colFlags) indicate how many variables in each constraint are unbounded. When all variables are bounded (flag = 0), exact limits can be computed. When exactly one variable is unbounded (flag = 1) and that variable is the one being analyzed, limits can also be computed conditionally.

### 4.2 Detailed Steps

1. **Initialize working bounds**
   - Extract current lower and upper bounds for the variable
   - Set feasible_lb = -tolerance (default most restrictive lower)
   - Set feasible_ub = +tolerance (default most restrictive upper)
   - These defaults are tightened as constraints are analyzed

2. **Scan variable's column entries**
   - For each constraint i where variable appears (iterate colStarts to colStarts + colCounts):
     - Skip if row is inactive (row < 0 or varStatus[row] < 0)
     - Extract coefficient A[i,var]
     - Determine if constraint is equality (check senses[i] = '=')

3. **Analyze positive coefficients (A[i,var] > PIVOT_TOL)**
   - Positive coefficient means variable increase tightens the constraint
   - Compute upper bound limit from dualUpper array:
     - If colFlags[i] = 0 (all variables in constraint bounded):
       - limit = lb[var] - dualUpper[i] / A[i,var]
       - Update feasible_ub = min(feasible_ub, limit)
     - Else if colFlags[i] = 1 and lb[var] <= -tolerance (this var at -infinity):
       - limit = -dualUpper[i] / A[i,var]
       - Update feasible_ub = min(feasible_ub, limit)
   - For equality constraints, symmetric check for lower bound:
     - If rowFlags[i] = 0:
       - limit = ub[var] - dualLower[i] / A[i,var]
       - Update feasible_lb = max(feasible_lb, limit)
     - Else if rowFlags[i] = 1 and ub[var] >= tolerance:
       - limit = -dualLower[i] / A[i,var]
       - Update feasible_lb = max(feasible_lb, limit)

4. **Analyze negative coefficients (A[i,var] < -PIVOT_TOL)**
   - Negative coefficient means variable decrease tightens the constraint
   - Signs are reversed from positive case:
   - Compute lower bound limit from dualUpper array:
     - If colFlags[i] = 0:
       - limit = ub[var] - dualUpper[i] / A[i,var]
       - Update feasible_lb = max(feasible_lb, limit)
     - Else if colFlags[i] = 1 and ub[var] >= tolerance:
       - limit = -dualUpper[i] / A[i,var]
       - Update feasible_lb = max(feasible_lb, limit)
   - For equality constraints:
     - If rowFlags[i] = 0:
       - limit = lb[var] - dualLower[i] / A[i,var]
       - Update feasible_ub = min(feasible_ub, limit)
     - Else if rowFlags[i] = 1 and lb[var] <= -tolerance:
       - limit = -dualLower[i] / A[i,var]
       - Update feasible_ub = min(feasible_ub, limit)

5. **Return results**
   - If out_lb != NULL: write feasible_lb to *out_lb
   - If out_ub != NULL: write feasible_ub to *out_ub

### 4.3 Pseudocode

```
Input: var, tolerance τ
Output: feasible_lb, feasible_ub

l ← lb[var]
u ← ub[var]
feasible_lb ← -τ
feasible_ub ← +τ

FOR each constraint i in column[var]:
    a ← A[i,var]
    IF |a| ≤ PIVOT_TOL:
        CONTINUE  // skip near-zero

    is_eq ← (senses[i] = '=')

    IF a > PIVOT_TOL:  // positive coefficient
        // Upper bound limit
        IF colFlags[i] = 0:
            limit ← l - dualUpper[i] / a
            feasible_ub ← min(feasible_ub, limit)
        ELSE IF colFlags[i] = 1 AND l ≤ -τ:
            limit ← -dualUpper[i] / a
            feasible_ub ← min(feasible_ub, limit)

        // For equality: lower bound limit
        IF is_eq:
            IF rowFlags[i] = 0:
                limit ← u - dualLower[i] / a
                feasible_lb ← max(feasible_lb, limit)
            ELSE IF rowFlags[i] = 1 AND u ≥ τ:
                limit ← -dualLower[i] / a
                feasible_lb ← max(feasible_lb, limit)

    ELSE IF a < -PIVOT_TOL:  // negative coefficient
        // Lower bound limit
        IF colFlags[i] = 0:
            limit ← u - dualUpper[i] / a
            feasible_lb ← max(feasible_lb, limit)
        ELSE IF colFlags[i] = 1 AND u ≥ τ:
            limit ← -dualUpper[i] / a
            feasible_lb ← max(feasible_lb, limit)

        // For equality: upper bound limit
        IF is_eq:
            IF rowFlags[i] = 0:
                limit ← l - dualLower[i] / a
                feasible_ub ← min(feasible_ub, limit)
            ELSE IF rowFlags[i] = 1 AND l ≤ -τ:
                limit ← -dualLower[i] / a
                feasible_ub ← min(feasible_ub, limit)

IF out_lb ≠ NULL:
    *out_lb ← feasible_lb
IF out_ub ≠ NULL:
    *out_ub ← feasible_ub
```

### 4.4 Mathematical Foundation

**Constraint Implication Analysis:**

For constraint i: ∑_j A[i,j] x_j {≤,=,≥} b_i

Rearranging to isolate variable k:

    A[i,k] x_k {≤,=,≥} b_i - ∑_{j≠k} A[i,j] x_j

The right-hand side is precomputed in the dual pricing arrays:
- dualUpper[i] tracks contributions assuming variables at upper bounds
- dualLower[i] tracks contributions assuming variables at lower bounds

**For positive coefficient A[i,k] > 0:**

Upper bound limit:
    x_k ≤ (b_i - ∑_{j≠k} A[i,j] l_j) / A[i,k]  (assuming others at lower bounds)
    x_k ≤ l_k - dualUpper[i] / A[i,k]

Lower bound limit (equality only):
    x_k ≥ (b_i - ∑_{j≠k} A[i,j] u_j) / A[i,k]  (assuming others at upper bounds)
    x_k ≥ u_k - dualLower[i] / A[i,k]

**For negative coefficient A[i,k] < 0:**
All inequality directions reverse.

**Flag Interpretation:**
- colFlags[i] = 0: All variables in constraint i are at bounds (exact computation possible)
- colFlags[i] = 1: One variable unbounded (computation valid if that variable is x_k)
- colFlags[i] ≥ 2: Multiple unbounded (cannot compute tight limit)

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(nnz_j) where nnz_j = nonzeros in column j
- **Average case:** O(nnz_j)
- **Worst case:** O(nnz_j)

Where:
- nnz_j = number of nonzeros in column j (typically 0.1% to 5% of m)

The algorithm scans the column once, performing constant-time work per entry.

### 5.2 Space Complexity

- **Auxiliary space:** O(1) - only scalar working variables
- **Total space:** O(1)

## 6. Error Handling

### 6.1 Error Conditions

None. Function always succeeds (void return type). Invalid inputs may produce meaningless bounds but no error is signaled.

### 6.2 Error Behavior

No error conditions. If state is corrupted, results will be incorrect but no crash occurs.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| No constraints | colCounts[var] = 0 | Returns feasible_lb = -tol, feasible_ub = +tol (default) |
| All equality constraints | All senses[i] = '=' | Both lb and ub are tightly constrained |
| All inactive rows | All varStatus[row] < 0 | Returns default bounds (no constraints active) |
| Multiple unbounded | colFlags[i] ≥ 2 for all rows | Returns default bounds (cannot compute limits) |
| Infeasible bounds | Computed lb > ub | Caller must detect this condition |
| Free variable | lb[var] = -inf, ub[var] = +inf | May compute finite implied bounds from constraints |

## 8. Thread Safety

**Thread-safe:** Yes (read-only)

This function only reads from solver state and does not modify any shared data. Multiple threads can safely call this function concurrently on the same or different variables, as long as no thread is modifying the solver state.

**Synchronization required:** None for read access; caller must ensure state is not being modified concurrently

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| fabs | Math | Compute absolute value for coefficient comparison |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_pivot_special | Ratio Test | Before executing bound flip, verify new bound is valid |
| cxf_bound_flip | Simplex Core | Dual simplex bound flip validation |
| cxf_presolve_bounds | Presolve | Tighten variable bounds during presolve |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_pivot_bound | Downstream: executes bound change after check validates it |
| cxf_bound_propagate | Similar: propagates bounds through constraints |
| cxf_dual_pricing_update | Maintains dual pricing arrays used by this function |

## 11. Design Notes

### 11.1 Design Rationale

This function implements constraint-based bound tightening, a key component of LP presolve and dual simplex. Rather than attempt a bound flip and detect infeasibility afterward, the check provides a priori validation.

The design leverages the dual pricing arrays which maintain cumulative sums of bounded variable contributions. This enables O(nnz_j) computation rather than O(nnz_j * nnz_i) that would be required if recalculating constraint slacks from scratch.

The flag-based approach (rowFlags, colFlags) provides an efficient test for whether a constraint can yield a tight bound. When multiple variables are unbounded in a constraint, that constraint cannot provide a meaningful limit, so it is skipped.

### 11.2 Performance Considerations

**Cache efficiency:** Sequential scan of column entries provides good cache locality.

**Early termination:** Could add early termination if bounds become infeasible (lb > ub), though this is rare in practice.

**Sparse iteration:** Already efficient - only examines constraints where variable appears.

**Flag checks:** The flag-based filtering eliminates many unnecessary limit computations. In typical LP problems, 80-90% of constraints have multiple unbounded variables and can be skipped.

### 11.3 Future Considerations

**Bound propagation:** Could be extended to propagate tightened bounds to neighboring variables.

**Conflict analysis:** Could track which constraints contribute to infeasibility when lb > ub.

**Vectorization:** Limit computations are independent and could be vectorized.

## 12. References

- Andersen, E.D. and Andersen, K.D. (1995). "Presolving in linear programming." *Mathematical Programming*, 71(2), 221-245. (Bound tightening techniques)
- Savelsbergh, M.W.P. (1994). "Preprocessing and probing techniques for mixed integer programming problems." *INFORMS Journal on Computing*, 6(4), 445-454. (Constraint propagation)
- Achterberg, T. (2007). *Constraint Integer Programming*. PhD thesis, TU Berlin. (Bound analysis algorithms)

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
