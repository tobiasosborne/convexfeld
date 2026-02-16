# cxf_pivot_bound

**Module:** Ratio Test
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Moves a non-basic variable to a specified bound value and optionally fixes it at that bound, removing it from the active problem. This operation updates the objective value, right-hand sides, dual pricing arrays, and maintains consistency of matrix structures. Used for bound flips during simplex iterations, variable fixing during presolve, and handling piecewise linear or quadratic objective functions.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| env | void* | Environment pointer | Non-null, valid environment | Yes |
| state | void* | Solver state structure | Non-null, valid state | Yes |
| var | int | Variable index to move/fix | 0 to numVars-1 | Yes |
| new_value | double | Target bound value | Any finite value | Yes |
| tolerance | double | Numerical tolerance for comparisons | Typically 1e-6 to 1e-9 | Yes |
| fix_mode | int | 0=move to bound, 1=fix and eliminate | 0 or 1 | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | 0 on success, 0x2711 (OUT_OF_MEMORY) on allocation failure |

### 2.3 Side Effects

Modifies solver state extensively:
- Updates objective value to account for fixed variable contribution
- Sets variable bounds (lb[var] = ub[var] = new_value)
- Updates right-hand sides for all constraints containing the variable
- Updates dual pricing arrays (dualLower, dualUpper)
- Updates row/column pricing flags (counts of unbounded variables)
- Modifies matrix structure by marking column entries as removed
- Creates eta vector for basis update (if in standard mode)
- Updates quadratic neighbor lists (if quadratic objective present)
- Updates piecewise linear segment tracking (if PWL objective present)
- Increments work counter for iteration accounting

## 3. Contract

### 3.1 Preconditions

- [ ] env and state pointers must be valid
- [ ] var must be a valid non-basic variable index
- [ ] new_value must be between lb[var] and ub[var] (or equal to one)
- [ ] Matrix arrays (colStarts, colCounts, colRows, colCoeffs) must be consistent
- [ ] Dual pricing arrays must be up-to-date with current variable bounds
- [ ] If quadratic objective present, neighbor lists must be consistent
- [ ] If piecewise linear objective, PWL arrays must be properly initialized

### 3.2 Postconditions

- [ ] Variable bounds updated: lb[var] = ub[var] = new_value (if fix_mode=0)
- [ ] Objective value updated to include obj[var] * new_value
- [ ] RHS updated: rhs[i] -= A[i,var] * new_value for all constraints i
- [ ] Dual pricing arrays reflect variable at new bound
- [ ] Row/column flags correctly count unbounded variables
- [ ] Matrix column marked as removed (entries set to -1)
- [ ] If fix_mode=1: variable completely eliminated from matrix
- [ ] Eta vector created and linked to eta list (if standard mode)
- [ ] Quadratic neighbor contributions applied to objective
- [ ] PWL segment identified and slope/offset applied

### 3.3 Invariants

- [ ] Matrix structural integrity maintained (no invalid pointers)
- [ ] Total number of rows and columns unchanged
- [ ] Other variable bounds unchanged
- [ ] Basis header consistency maintained

## 4. Algorithm

### 4.1 Overview

This function performs a complex multi-step operation to move or fix a variable at a bound while maintaining solver state consistency. The operation must update numerous interconnected data structures including objective tracking, constraint right-hand sides, dual pricing information, and matrix sparsity patterns.

The algorithm handles three special cases before the standard bound update:

1. **Piecewise Linear Objectives:** When the variable has a PWL objective, identify which piecewise segment the new value falls into, then update the objective with the corresponding slope and offset.

2. **Quadratic Objectives:** When the variable participates in quadratic terms (Q matrix), update neighbor variables' objective coefficients to account for the fixed value.

3. **Eta Vector Creation:** For standard simplex mode, create an eta vector recording the bound change for later basis updates and potential unrolling.

After special case handling, the function updates the right-hand sides and dual pricing arrays by incorporating the variable's contribution to each constraint. Finally, it either fixes the variable (eliminating it from the problem) or simply sets it to the target bound.

### 4.2 Detailed Steps

1. **Extract current bounds and check special flags**
   - Save original lb[var] and ub[var] for later calculations
   - Check varFlags[var] to determine if variable has special properties
   - Determine if eta vector creation is needed based on fix_mode and flags

2. **Handle piecewise linear objective (if applicable)**
   - If variable has PWL flag set and not being fixed:
     - Scan PWL breakpoints to find which segment new_value falls into
     - Update objective coefficient to slope of that segment
     - Add segment offset to objective value
     - Clear PWL count to mark as resolved
     - Update work counter

3. **Handle quadratic objective (if applicable)**
   - If quadratic array exists and variable has quadratic term:
     - Call cxf_quadratic_adjust to update neighbor objective coefficients
     - This propagates the fixed value through Q matrix terms
     - Add quadratic contribution (0.5 * new_value^2 * Q[var,var]) to objective
     - Clear quadratic[var] to mark as resolved

4. **Create eta vector (if needed)**
   - If fix_mode=1 or variable has no special flags:
     - Determine eta size based on column structure and mode
     - Allocate eta vector from eta allocator
     - Initialize eta header with variable index, new value, old objective coefficient
     - Determine status code (FIXED, SUPERBASIC, AT_LOWER, AT_UPPER)
     - Copy column indices and coefficients for active rows
     - Link eta to eta list for later processing
     - Clear varFlags[var] to mark as processed

5. **Update objective value**
   - Add obj[var] * new_value to objective
   - Set obj[var] = 0 (variable no longer contributes linearly)
   - If quadratic term exists, add 0.5 * new_value^2 * Q[var,var]
   - Set quadratic[var] = 0

6. **Update neighbor lists (if quadratic objective)**
   - For each neighbor j in neighbor list of var:
     - Add new_value * Q[var,j] to obj[j]
     - Remove var from neighbor j's neighbor list (update indices and coefficients)
     - Decrement neighbor count
     - Invalidate pricing for neighbor j
   - Clear neighbor count for var

7. **Update pricing state**
   - Call cxf_pricing_update to invalidate cached pricing information

8. **Update RHS and dual pricing arrays**
   - For each constraint i where var appears:
     - Compute delta = new_value * A[i,var]
     - Update rhs[i] -= delta
     - Update dualLower[i] and dualUpper[i] based on coefficient sign
     - If coefficient positive:
       - dualLower affected by upper bound changes
       - dualUpper affected by lower bound changes
     - If coefficient negative: reverse relationship
     - Update rowFlags and colFlags to track unbounded variable counts
     - Mark column entry as removed: colRows[pos] = -1
     - Decrement varStatus[row] (count of active variables in row)
     - Track affected rows in temporary array for later processing

9. **Finalize variable bounds and status**
   - If fixing (fix_mode=1 and standard variable):
     - Set lb[var] = ub[var] = new_value
     - Set colCounts[var] = 0 (no active entries)
     - Clear basisHeader[var] if positive
     - Remove var from all constraint rows (mark as -1)
   - If elimination mode (fix_mode=1 and special):
     - Set colCounts[var] = -1 (eliminated marker)
     - Set basisHeader[var] = -2 (AT_UPPER status)

### 4.3 Pseudocode

```
Input: var, new_value, tolerance τ, fix_mode
Output: SUCCESS or OUT_OF_MEMORY

// Extract original bounds
l_orig ← lb[var]
u_orig ← ub[var]

// Handle piecewise linear
IF varFlags[var] has PWL flag AND fix_mode = 0:
    Find segment s where breakpoint[s-1] ≤ new_value < breakpoint[s]
    obj[var] ← slope[s]
    objective ← objective + offset[s]

// Handle quadratic
IF quadratic present AND NOT fixing:
    FOR each neighbor j of var:
        obj[j] ← obj[j] + new_value * Q[var,j]
        Remove var from neighbor[j]'s list
    objective ← objective + 0.5 * new_value^2 * Q[var,var]

// Create eta vector
IF fix_mode = 1 OR no special flags:
    Allocate eta vector
    eta.var ← var
    eta.value ← new_value
    eta.oldObj ← obj[var]
    eta.status ← determine_status(new_value, l_orig, u_orig, τ)
    Copy active column entries to eta
    Link eta to eta list

// Update objective
objective ← objective + obj[var] * new_value
obj[var] ← 0

// Update constraints
FOR each row i in column[var]:
    δ ← new_value * A[i,var]
    rhs[i] ← rhs[i] - δ

    // Update dual pricing based on coefficient sign
    dualLower[i] ← dualLower[i] + (adjustments based on A[i,var] sign)
    dualUpper[i] ← dualUpper[i] + (adjustments based on A[i,var] sign)

    // Update unbounded variable counts
    Update rowFlags[i] and colFlags[i]

    // Mark as removed
    colRows[pos] ← -1
    varStatus[row]--

// Finalize
IF fix_mode = 1 AND standard variable:
    lb[var] ← ub[var] ← new_value
    colCounts[var] ← 0
    Remove var from all constraint rows
ELSE IF fix_mode = 1:
    colCounts[var] ← -1
    basisHeader[var] ← -2

RETURN SUCCESS
```

### 4.4 Mathematical Foundation

**Bound Update in Linear Program:**
When fixing variable x_j = v, the LP transforms from:

    minimize    c^T x
    subject to  Ax ≤ b
                l ≤ x ≤ u

to:

    minimize    c^T x' + c_j v
    subject to  A' x' ≤ b - A_j v
                l' ≤ x' ≤ u'

where A_j is column j of A, and A' is A with column j removed.

**Dual Pricing Array Updates:**
The dual pricing arrays track cumulative contributions for efficient dual ratio tests:

    dualUpper[i] = -∑_{j at lower} A[i,j] * l_j - ∑_{j at upper} A[i,j] * u_j

When fixing x_j = v:
- If A[i,j] > 0: update affects dualUpper based on upper bound, dualLower based on lower bound
- If A[i,j] < 0: relationships reverse

**Quadratic Objective Propagation:**
For quadratic term x^T Q x, fixing x_j = v affects neighbors:

    ∂/∂x_k (x^T Q x) = 2 Q[k,j] x_j

Linear term becomes: c_k' = c_k + 2 Q[k,j] v

Constant term increases by: v^2 Q[j,j]

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(nnz_j + n_nbr) where nnz_j = nonzeros in column j, n_nbr = quadratic neighbors
- **Average case:** O(nnz_j + n_nbr + ∑_i nnz_i) for constraint row updates
- **Worst case:** O(nnz_j * m) when fixing requires scanning all constraint rows

Where:
- nnz_j = number of nonzeros in column j (typically 0.1% to 5% of m)
- n_nbr = number of quadratic neighbors (typically 0 to 20)
- m = number of constraint rows
- n = number of variables

### 5.2 Space Complexity

- **Auxiliary space:** O(nnz_j) for eta vector allocation
- **Total space:** O(nnz_j)

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| Eta allocation fails | 0x2711 (OUT_OF_MEMORY) | Insufficient memory for eta vector |
| Quadratic adjustment fails | 0x2711 (OUT_OF_MEMORY) | Insufficient memory during quadratic update |

### 6.2 Error Behavior

On memory allocation failure, function returns OUT_OF_MEMORY immediately. State may be partially modified (objective value, some RHS values updated) before failure. Caller should treat state as inconsistent and either:
- Restore from checkpoint, or
- Abort solve

Function does not provide rollback capability.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Fixed variable | lb[var] = ub[var] initially | Creates FIXED status eta, eliminates from matrix |
| Free variable | lb[var] = -inf, ub[var] = +inf | Can move to any value, updates only objective |
| Variable at bound | new_value = lb[var] or ub[var] | No movement needed, but updates still applied |
| Zero column | colCounts[var] = 0 | Only updates objective, no RHS changes |
| PWL at breakpoint | new_value exactly on breakpoint | Selects lower segment index |
| Large quadratic neighborhood | Many neighbors | Updates all neighbor objectives sequentially |
| Special mode active | state != 0 | Creates minimal eta (only header, no indices) |

## 8. Thread Safety

**Thread-safe:** No

This function extensively modifies solver state and allocates from shared eta allocator. Not safe for concurrent calls even on different variables of the same model.

**Synchronization required:** Caller must hold exclusive lock on solver state before calling

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_alloc_eta | Memory | Allocate eta vector for basis update history |
| cxf_pricing_invalidate | Pricing | Invalidate cached pricing for affected variables |
| cxf_pricing_update | Pricing | Update pricing state after bound change |
| cxf_quadratic_adjust | Simplex Core | Adjust quadratic neighbor objectives |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_pivot_special | Ratio Test | After determining bound flip is beneficial |
| cxf_presolve_fix | Presolve | Fixing variables during presolve |
| cxf_bound_flip | Simplex Core | Dual simplex bound flip operations |
| cxf_simplex_phase1 | Simplex Core | Phase I variable fixing |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_pivot_check | Pre-validation: computes feasible bound range before this operation |
| cxf_pivot_primal | Alternative: performs standard basis exchange pivot |
| cxf_fix_variable | Simpler variant: fixes variable without eta creation |
| cxf_bound_update | Lower-level: updates bounds without matrix modification |

## 11. Design Notes

### 11.1 Design Rationale

The function combines multiple related operations (bound update, objective update, RHS update, matrix update) into a single atomic operation to maintain state consistency. This prevents partially-updated states that could violate invariants.

The two-mode design (fix_mode parameter) allows the same function to handle both temporary bound moves during simplex iterations and permanent variable elimination during presolve or row reduction. This code reuse reduces duplication and ensures consistent behavior.

Eta vector creation provides a history mechanism for:
- Basis reinversion after multiple pivots
- Rollback during infeasibility detection
- Warm-start preservation

The dual pricing array updates are particularly subtle: they must account for coefficient signs and bound types to maintain the invariant that dualLower/dualUpper track cumulative bounded variable contributions. This enables efficient dual ratio tests without recomputing constraint slacks.

### 11.2 Performance Considerations

**Work counter tracking:** The function meticulously tracks operation counts in the work counter for iteration accounting. This enables accurate pricing strategies and refactorization timing.

**Sparse updates:** Only active (non-removed) matrix entries are processed. Removed entries are marked with -1 rather than physically deleted, enabling O(1) marking.

**Neighbor list surgery:** Removing variable from neighbor lists requires shifting arrays. For variables with many neighbors, this can be expensive. Alternative: mark as inactive rather than removing.

**Special mode optimization:** In special mode (state != 0), creates minimal eta vectors without index arrays, reducing allocation overhead during Phase I or other specialized phases.

### 11.3 Future Considerations

**Batch fixing:** Could optimize for fixing multiple variables simultaneously by deferring RHS updates until all variables processed.

**Lazy deletion:** Instead of removing from neighbor lists immediately, could mark as inactive and defer compaction.

**Vectorization:** RHS and dual array updates are independent across rows and could be vectorized with SIMD instructions.

## 12. References

- Forrest, J.J. and Tomlin, J.A. (1972). "Updated triangular factors of the basis to maintain sparsity in the product form simplex method." *Mathematical Programming*, 2, 263-278. (Eta vector technique)
- Koberstein, A. (2005). *The dual simplex method, techniques for a fast and stable implementation*. PhD thesis, Universität Paderborn. (Bound flipping)
- Bixby, R.E. (2002). "Solving real-world linear programs: a decade and more of progress." *Operations Research*, 50(1), 3-15. (Modern LP solver techniques)

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
