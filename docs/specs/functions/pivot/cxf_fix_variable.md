# cxf_fix_variable

**Module:** Pivot Execution
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Permanently fixes a variable at a specified constant value, effectively eliminating it from the active optimization problem. Unlike a bound pivot which is reversible, variable fixing is a permanent reduction that removes the variable from the matrix structure entirely. This operation is fundamental to presolve reductions, branch-and-bound node processing, and heuristic search techniques. The function handles the full propagation of the fixing: updating the objective constant, adjusting constraint right-hand sides, linearizing quadratic terms, and maintaining solver data structure consistency.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| env | void* | Convexfeld environment pointer | Valid CxfEnv pointer | Yes |
| state | void* | Solver state structure | Valid SolverState pointer | Yes |
| var | int | Variable index to fix | 0 to numVars-1 | Yes |
| value | double | Value at which to fix the variable | Finite double within [lb, ub] | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | 0 on success, 1001 if out of memory, other error codes propagated from dependencies |

### 2.3 Side Effects

- Modifies variable bounds: sets both lb[var] and ub[var] to the fixed value
- Updates objective constant by adding the linear and quadratic contributions
- Modifies RHS values of all constraints containing the variable
- Sets variable status to FIXED (-4)
- Zeros the variable's objective coefficient
- Marks all column entries as removed from the matrix (-1)
- Sets column count to 0, indicating the variable is eliminated
- Removes variable from basis header if it was basic
- Invalidates and updates pricing data structures
- Increments the fixed variable count statistic
- For quadratic problems: zeros quadratic diagonal term and may update neighbor coefficients

## 3. Contract

### 3.1 Preconditions

- [X] env pointer must be valid
- [X] state pointer must be valid and point to initialized SolverState
- [X] var must be a valid variable index within [0, numVars)
- [X] value should be within or very close to [lb[var], ub[var]] for numerical consistency
- [X] All required array pointers in state must be allocated
- [X] Pricing state must be initialized
- [X] If quadratic objective exists, quadratic adjustment function must be available

### 3.2 Postconditions

- [X] lb[var] = value
- [X] ub[var] = value
- [X] varStatus[var] = -4 (FIXED)
- [X] objCoeffs[var] = 0.0
- [X] colCounts[var] = 0
- [X] Objective constant includes contribution: objConstant' = objConstant + c·value + (1/2)·value²·Q[var,var]
- [X] For each constraint i containing variable: rhs[i]' = rhs[i] - A[i,var]·value
- [X] All column entries marked as removed (colRows[pos] = -1)
- [X] Variable removed from basis header if present
- [X] Fixed variable count incremented
- [X] Pricing state invalidated and updated for the variable

### 3.3 Invariants

- [X] Total number of variables does not change (variable still exists, just fixed)
- [X] Constraint matrix dimensions unchanged (sparsity pattern modified)
- [X] Non-fixed variables' bounds, status, and coefficients remain unchanged
- [X] Basis consistency: if variable was basic, basis is updated to remove it
- [X] Objective constant accumulation is mathematically consistent

## 4. Algorithm

### 4.1 Overview

Variable fixing is a fundamental operation in mixed-integer programming and presolve that permanently assigns a variable to a constant value. This is more aggressive than simply setting a variable to its bound in the simplex method, because it removes the variable from consideration entirely.

The algorithm proceeds in several phases. First, if the problem has quadratic objective terms involving this variable, it invokes the quadratic adjustment routine which linearizes the quadratic relationships and may update neighbor variables' objective coefficients. This step is critical because fixing x_j = v in a quadratic objective f = c'x + (1/2)x'Qx requires updating c_i for all variables i where Q[i,j] is nonzero.

Next, the function updates the objective constant by adding the linear contribution c_j · v. The variable's objective coefficient is then zeroed since it no longer participates in the optimization.

The constraint right-hand side (RHS) update phase iterates through all constraints containing the variable. For each constraint A_i · x ≤ b_i, substituting x_j = v yields A'_i · x' ≤ b_i - A[i,j]·v, where A'_i and x' represent the constraint and variable vector with the j-th component removed. The function updates each RHS accordingly and marks the matrix entries as logically deleted.

Finally, the function sets both bounds to the fixed value, marks the variable status as FIXED (-4), and updates the basis header to remove the variable if it was basic (which is unusual but can occur during aggressive presolve). The pricing data structures are invalidated to ensure subsequent pricing operations don't use stale information about this now-fixed variable.

### 4.2 Detailed Steps

1. **Extract Pointers**: Retrieve all necessary array pointers from the solver state structure, including variable status, basis header, matrix data (column starts, counts, rows, coefficients), bounds (lower, upper), objective data (coefficients, RHS), objective constant location, quadratic diagonal array, and pricing state.

2. **Quadratic Handling**: Check if the problem has quadratic objective terms. If yes and the variable has a nonzero quadratic diagonal entry, call cxf_quadratic_adjust to linearize the quadratic relationships. This function updates the objective constant with the quadratic contribution (1/2)·v²·Q[j,j] and propagates the fixing to neighbor variables by updating their objective coefficients: c_i ← c_i + Q[i,j]·v. If this adjustment fails (returns non-zero), propagate the error code.

3. **Linear Objective Update**: Add the variable's linear objective contribution to the objective constant: objConstant ← objConstant + objCoeff[var] · value. This accounts for the removal of the c_j · x_j term from the objective function.

4. **Zero Objective Coefficient**: Set objCoeff[var] to 0.0, since the variable is no longer a decision variable in the optimization.

5. **Constraint RHS Propagation**: Iterate through the variable's column from colStarts[var] to colStarts[var] + colCounts[var]. For each position pos in this range, retrieve the row index and coefficient. If the row is valid (row >= 0, not previously marked as removed), update the RHS: rhs[row] ← rhs[row] - colCoeff[pos] · value. This implements the substitution of x_j = v into the constraint.

6. **Sparsity Update**: For each constraint row processed in step 5, decrement the row's nonzero count (if row count tracking is active) and mark the matrix entry as removed by setting colRows[pos] = -1. This maintains the matrix structure but marks entries as inactive.

7. **Pricing Invalidation**: For each row where the variable appeared and that row corresponds to a basic variable (varStatus[row] >= 0), call cxf_pricing_invalidate to clear any cached pricing information for that variable, ensuring subsequent pricing operations recompute values.

8. **Bound Setting**: Set both lb[var] and ub[var] to the fixed value. This makes the bounds equal, which is the defining characteristic of a fixed variable.

9. **Status Update**: Set varStatus[var] to STATUS_FIXED (-4), marking the variable as permanently fixed.

10. **Column Elimination**: Set colCounts[var] to 0, indicating the variable's column has no active entries.

11. **Basis Header Update**: If the variable was somehow basic (present in the basis header), search through the basis header array for entries equal to var and set them to -1. This is a defensive measure since properly managed solvers should not fix basic variables.

12. **Pricing Update**: Call cxf_pricing_update to refresh the pricing state after the structural change to the problem.

13. **Statistics Update**: Increment the fixed variable counter at state to track how many variables have been fixed.

14. **Success Return**: Return 0 to indicate successful completion.

### 4.3 Pseudocode

```
FUNCTION FixVariable(env, state, var, value):
  // Handle quadratic objective
  IF state.hasQuadratic AND quadratic[var] ≠ NULL THEN
    result ← QuadraticAdjust(env, state, var, value)
    IF result ≠ SUCCESS THEN
      RETURN result

  // Update objective constant
  c ← objCoeff[var]
  objConstant ← objConstant + c · value
  objCoeff[var] ← 0

  // Update constraints
  colStart ← colStarts[var]
  colCount ← colCounts[var]

  FOR pos ← colStart TO colStart + colCount - 1 DO
    row ← colRows[pos]
    IF row < 0 THEN CONTINUE  // Already removed

    coeff ← colCoeffs[pos]

    // Substitute x_var = value into constraint
    rhs[row] ← rhs[row] - coeff · value

    // Update sparsity
    IF rowCounts ≠ NULL THEN
      rowCounts[row] ← rowCounts[row] - 1

    colRows[pos] ← -1  // Mark removed

    // Invalidate pricing cache
    IF varStatus[row] ≥ 0 THEN
      InvalidatePricing(pricingState, row)

  // Fix bounds
  lb[var] ← value
  ub[var] ← value

  // Update status
  varStatus[var] ← FIXED

  // Eliminate column
  colCounts[var] ← 0

  // Remove from basis if present
  FOR row ← 0 TO numConstraints - 1 DO
    IF basisHeader[row] = var THEN
      basisHeader[row] ← -1
      BREAK

  // Update pricing
  UpdatePricing(pricingState, state, var)

  // Statistics
  fixedCount ← fixedCount + 1

  RETURN SUCCESS
```

### 4.4 Mathematical Foundation

**Variable Substitution in Linear Programs**

Consider a linear program:
```
minimize    c^T x
subject to  Ax ≤ b
            l ≤ x ≤ u
```

Fixing variable x_j = v̄ is equivalent to substituting into the problem:

```
minimize    c^T x' + c_j · v̄
subject to  A' x' ≤ b - A_·j · v̄
            l' ≤ x' ≤ u'
```

where x' = x with x_j removed, and A' = A with column j removed.

The objective constant becomes: constant' = constant + c_j · v̄

Each constraint i is updated: b'_i = b_i - A_{ij} · v̄

**Quadratic Objective Handling**

For quadratic programs with objective:
```
f(x) = c^T x + (1/2) x^T Q x
```

Fixing x_j = v̄ requires:

1. **Constant term update:**
   constant' = constant + c_j · v̄ + (1/2) v̄² · Q_{jj}

2. **Linear coefficient updates:** For all i ≠ j:
   c'_i = c_i + Q_{ij} · v̄

3. **Quadratic matrix reduction:** Remove row j and column j from Q

This linearization ensures the reduced problem has the same optimal value as the original problem with the fixing constraint x_j = v̄.

**Basis Implications**

In the simplex method, the basis B is a square invertible submatrix of A corresponding to basic variables. A variable being fixed should not be basic; if it is, it must be removed from the basis. The basis header (mapping constraints to basic variables) is updated accordingly.

After fixing, the reduced problem has dimension (n-1) × m where n is the number of variables and m is the number of constraints. The number of constraints remains unchanged, but the effective variable count decreases.

**References:**
- Bertsimas, D., & Tsitsiklis, J. N. (1997). *Introduction to Linear Optimization.* Athena Scientific. (Variable substitution, Section 1.5)
- Nocedal, J., & Wright, S. J. (2006). *Numerical Optimization* (2nd ed.). Springer. (Quadratic programming, Chapter 16)
- Achterberg, T. (2007). *Constraint Integer Programming.* PhD Thesis, TU Berlin. (Variable fixing in presolve, Section 6.2)

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) - Variable has empty column (no constraint entries) and no quadratic terms
- **Average case:** O(nnz_col + nnz_qrow) - Process all column entries and quadratic row entries
- **Worst case:** O(nnz_col + nnz_qrow + m) - If variable is basic, must scan basis header

Where:
- nnz_col = number of nonzero coefficients in the variable's column
- nnz_qrow = number of nonzero entries in variable's row of quadratic matrix Q
- m = number of constraints (only if basis header scan needed)

### 5.2 Space Complexity

- **Auxiliary space:** O(1) - Only local variables, no temporary allocations
- **Total space:** O(1) - All operations modify existing arrays in place

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| Quadratic adjustment failure | 1001 (OUT_OF_MEMORY) | Memory allocation failed in cxf_quadratic_adjust |
| Invalid pointers in state | Undefined | Segmentation fault if required pointers are NULL |

### 6.2 Error Behavior

On quadratic adjustment failure: The function propagates the error code immediately without completing the fix operation. State may be partially modified (quadratic adjustment may have updated some neighbor coefficients) but the variable itself is not marked as fixed, so the state remains internally consistent for error recovery.

On success: All state modifications are atomic from the caller's perspective. The variable is completely eliminated from the active problem, all constraints are updated, and the basis remains valid.

Defensive basis header update: The function checks if the variable appears in the basis header and removes it if found. This should rarely occur in well-formed solver states (basic variables should be pivoted out before fixing), but provides safety against edge cases.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Empty column | colCounts[var] = 0 | No RHS updates, only bounds and status changed |
| Variable already at value | Current solution has x[var] = value | Objective and RHS still updated (idempotent) |
| All column entries removed | All colRows[pos] = -1 already | Skip RHS updates, only bounds and status modified |
| Variable is basic | varStatus[var] >= 0 or in basisHeader | Remove from basis header, mark as fixed |
| No quadratic terms | quadratic = NULL or quadratic[var] = 0 | Skip quadratic adjustment, only linear update |
| Zero objective coefficient | objCoeff[var] = 0 | Objective constant unchanged except quadratic contribution |
| Fixing at bound | value = lb[var] or value = ub[var] | Proceeds normally, both bounds set to same value |
| Fixing at midpoint | lb[var] < value < ub[var] | Allowed, both bounds set to interior value |
| Fixing outside bounds | value < lb[var] or value > ub[var] | No validation; proceeds with RHS update (may create infeasibility) |
| Multiple fixings | Called repeatedly on different variables | Each fixing is independent, fixedCount accumulates |
| Fixing already fixed | varStatus[var] = -4 already | Re-fixes at new value, updates RHS and objective again |

## 8. Thread Safety

**Thread-safe:** No

This function modifies shared state including bounds arrays, objective constant, RHS array, variable status, column counts, basis header, and pricing structures. All modifications assume exclusive access to the SolverState.

**Synchronization required:** Caller must hold exclusive lock on the SolverState structure. The environment's critical section (if present) should be held throughout the call to prevent concurrent access from other threads.

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_presolve_main | Presolve | Fix variables proven to be at bounds by constraint propagation |
| cxf_branch_apply_bounds | Branch and Bound | Apply branching decision (e.g., x_i = 0 or x_i = 1) at MIP node |
| cxf_heuristic_diving | Heuristics | Iteratively fix variables during diving heuristic |
| cxf_heuristic_rins | Heuristics | Fix variables at values from incumbent solution |
| cxf_fix_variable_at_bound | Pivot | Wrapper that fixes variable at its current bound |
| cxf_simplex_cleanup | Simplex | Fix degenerate variables during cleanup phase |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_fix_variable_at_bound | Wrapper that determines value from current variable status/solution |
| cxf_pivot_primal | Reversible operation that moves variable but maintains basis structure |
| cxf_pivot_bound | Moves variable to bound temporarily (can re-enter basis) |
| cxf_presolve_substitute | More general substitution that can handle linear relationships |
| cxf_unfix_variable | Hypothetical reverse operation (not present; requires state restoration) |
| cxf_quadratic_adjust | Handles quadratic objective linearization when fixing |

## 11. Design Notes

### 11.1 Design Rationale

The decision to make variable fixing a separate, permanent operation rather than a special case of bound pivoting reflects the fundamentally different role these operations play in the solver architecture. Bound pivots are part of the iterative simplex process and must maintain the ability to reverse decisions as the algorithm explores the feasible region. Variable fixing, in contrast, is a reduction operation that shrinks the problem size based on logical deductions or branching decisions that are known to be globally valid.

The integration of quadratic adjustment directly into the fixing operation ensures that the complex interdependencies between variables in quadratic programs are handled atomically. Separating these concerns would risk inconsistent state if the adjustment succeeded but the fixing failed, or vice versa.

The choice to mark matrix entries as removed (-1) rather than physically deleting them preserves the original matrix structure, which simplifies memory management and allows potential restoration in backtracking scenarios (though the function itself doesn't provide unfixing). This is a classic space-time tradeoff: we use more memory to maintain the original allocation but gain simpler code and faster access patterns.

The defensive basis header update handles an edge case that shouldn't occur in normal operation but provides robustness. During aggressive presolve or parallel processing, there might be brief windows where a variable is marked basic but has been proven to require fixing. The function handles this gracefully rather than asserting or failing.

### 11.2 Performance Considerations

Variable fixing is typically very fast (O(column nnz)) and is heavily used during presolve and branch-and-bound. The performance of an entire MIP solve can be dominated by the quality of variable fixing strategies rather than simplex iterations on the reduced problem.

The RHS update loop is the primary computational cost, requiring one pass through the variable's column. For dense columns (many constraint entries), this can be expensive, but such columns are rare in practice. The function doesn't attempt to optimize this loop beyond straightforward iteration because the memory access pattern is already cache-friendly (sequential access to colRows and colCoeffs).

The quadratic adjustment call can be expensive for variables with many quadratic neighbors, but this is unavoidable for correctness. The cost is amortized over the solve because proper fixing during presolve reduces the problem size significantly.

Work counter increments are intentionally omitted from this function (unlike cxf_pivot_primal) because fixing is considered structural work rather than iteration work. The distinction helps separate presolve/branching overhead from solving overhead in performance analysis.

### 11.3 Future Considerations

The function could be extended to support batch fixing of multiple variables simultaneously. This would allow better cache utilization when processing RHS updates and could enable vectorization of the update operations. However, it would complicate error handling (what if some fixes succeed and others fail?) and would require more complex memory management.

Currently, the function doesn't validate that the fixed value is within the variable's bounds. Adding a tolerance-based check (value ∈ [lb - tol, ub + tol]) could catch bugs in calling code and provide better error messages to users.

The basis header scan is linear in the number of constraints, which could be expensive for very large models. An inverse mapping (variable -> constraint where basic) would reduce this to O(1) but would require maintaining the inverse mapping as the basis changes.

For problems where many variables are fixed, it may be worthwhile to rebuild the matrix data structures periodically to reclaim memory and improve cache locality. The current design leaves "holes" in the matrix arrays marked by -1 entries, which accumulate over many fixings.

## 12. References

- Achterberg, T. (2007). *Constraint Integer Programming.* PhD Thesis, TU Berlin. (Chapter 6: Presolve and variable fixing techniques)
- Bixby, R. E., & Rothberg, E. (2007). Progress in computational mixed integer programming—A look back from the other side of the tipping point. *Annals of Operations Research*, 149(1), 37-41. (Role of presolve in modern MIP solvers)
- Savelsbergh, M. W. P. (1994). Preprocessing and probing techniques for mixed integer programming problems. *ORSA Journal on Computing*, 6(4), 445-454. (Variable fixing in presolve)
- Bertsimas, D., & Tsitsiklis, J. N. (1997). *Introduction to Linear Optimization.* Athena Scientific. (Variable substitution in linear programs, Section 1.5)
- Nocedal, J., & Wright, S. J. (2006). *Numerical Optimization* (2nd ed.). Springer. (Quadratic programming objective handling, Chapter 16)

## 13. Validation Checklist

Before finalizing this spec, verify:

- [X] No code copied from implementation
- [X] Algorithm description is implementation-agnostic
- [X] All parameters documented
- [X] All error conditions listed
- [X] Complexity analysis complete
- [X] Edge cases identified
- [X] A competent developer could implement from this spec alone

---

*Reviewed by: Pending*

---
*Convexfeld LP Solver Specification*
*Based on published optimization literature*
