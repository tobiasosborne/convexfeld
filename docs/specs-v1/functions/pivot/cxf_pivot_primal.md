# cxf_pivot_primal

**Module:** Pivot Execution
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Executes a primal simplex pivot operation that moves a non-basic variable to a new value (potentially zero) and updates the basis representation accordingly. This function is central to the primal simplex method, handling the mechanics of changing variable values while maintaining feasibility and updating the Product Form of the Inverse (PFI) basis representation. It handles special cases including piecewise linear objectives, quadratic objectives, and combinatorial neighbor relationships.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| env | void* | Environment pointer for memory allocation | Valid CxfEnv pointer | Yes |
| state | void* | Solver state structure containing all problem data | Valid SolverState pointer | Yes |
| var | int | Index of variable to pivot | 0 to numVars-1 | Yes |
| tolerance | double | Numerical tolerance for feasibility checks | Positive value, typically 1e-6 | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | 0 on success, 3 if infeasible, 0x2711 (1001) if out of memory |

### 2.3 Side Effects

- Creates and links a new eta vector (Type 1) to the PFI basis representation
- Updates objective value by contribution from pivoted variable
- Modifies objective coefficients (sets pivot variable to 0, updates neighbors if present)
- Updates RHS values of constraints containing the pivoted variable
- Modifies column sparsity pattern by marking entries as removed (-1)
- Updates variable status flags and basis header
- Updates pricing data structures
- Increments eta count and work counters
- For PWL variables: updates active segment and objective slope
- For quadratic objectives: updates diagonal terms and neighbor coefficients

## 3. Contract

### 3.1 Preconditions

- [X] state pointer must be valid and point to initialized SolverState
- [X] env pointer must be valid
- [X] var must be a valid variable index (0 to numVars-1)
- [X] Variable must be non-basic (not currently in basis)
- [X] All array pointers in state must be allocated and valid
- [X] tolerance must be positive
- [X] Eta allocator at state must be valid
- [X] Pricing state at state must be initialized

### 3.2 Postconditions

- [X] If successful (return 0), an eta vector has been added to the PFI chain OR the variable has been eliminated (for special constraints)
- [X] Objective value reflects contribution from pivoted variable
- [X] Objective coefficient for pivoted variable is set to 0.0
- [X] RHS values updated for all constraints containing the variable
- [X] Column entries marked as removed (set to -1) in active rows
- [X] Variable status appropriately set (AT_LOWER, AT_UPPER, SUPERBASIC, FIXED, or eliminated)
- [X] Neighbor lists updated if variable had quadratic or combinatorial relationships
- [X] Pricing state invalidated and updated for affected variables
- [X] Work counter incremented if present

### 3.3 Invariants

- [X] Eta list remains well-formed (all pointers valid)
- [X] Matrix structure arrays (colStarts, colCounts) remain consistent with marked removals
- [X] Basis header and varStatus arrays remain synchronized
- [X] For variables without special flags: basis representation remains valid

## 4. Algorithm

### 4.1 Overview

This function implements the primal simplex pivot operation using the Product Form of the Inverse (PFI) basis update scheme. The standard revised simplex method maintains a factorization of the basis matrix B through a sequence of eta vectors representing elementary pivot operations.

The algorithm first validates that the pivot is numerically sound by checking if bounds are too tight (indicating infeasibility) and whether the column has numerically unstable coefficients relative to the bound range. If bounds are sufficiently wide, it determines an appropriate target value for the variable based on the objective coefficient direction and bound positions.

For variables with piecewise linear objective terms, the function identifies which linear segment applies at the new value and updates the objective slope accordingly. For quadratic objectives, it delegates to a specialized adjustment routine that maintains the linearization.

The core pivot operation creates an eta vector (Type 1) that records the column being pivoted, the new value, and the original objective coefficient. This eta vector is prepended to a linked list representing the cumulative basis updates. The function then updates the right-hand side of all constraints by subtracting the product of the pivot value and the constraint coefficients, and marks the column entries as logically removed from the active matrix.

For variables with neighbor relationships (common in quadratic or combinatorial problems), the function propagates the pivot value to update neighbors' objective coefficients and removes the pivoted variable from all neighbor lists.

### 4.2 Detailed Steps

1. **Infeasibility Check**: Compute the bound range (upper - lower). If the absolute bound range is less than twice the tolerance, the bounds are too tight to admit a feasible pivot. Store the problematic variable index and return infeasibility code (3).

2. **Numeric Stability Check**: If bounds are sufficiently wide (range > WIDE_BOUNDS threshold), scan the variable's column for the maximum absolute coefficient value among active (basic) rows. If the product of this maximum coefficient and the bound range exceeds the tolerance, the pivot would be numerically unstable; skip the operation and return success without modifying state.

3. **Pivot Value Determination**: Determine where to pivot the variable based on the objective coefficient and bound positions. If the product of the objective coefficient and bound range has significant magnitude (above TINY_OBJ_THRESHOLD * tolerance), move the variable to the favorable bound: upper bound if objective coefficient is non-positive (minimization), lower bound if positive. Otherwise, if both bounds have the same sign, use their midpoint; if bounds straddle zero, pivot to zero.

4. **Piecewise Linear Handling**: If the variable has the PWL flag (0x80) set and no other special flags, locate the appropriate piecewise linear segment by finding the first breakpoint greater than or equal to the pivot value. Update the variable's objective coefficient to the slope of that segment, add the segment's offset to the objective constant, clear the PWL count for this variable, and decrement the total PWL constraint count.

5. **Quadratic Objective Handling**: If in standard mode (not fast mode or special mode) and quadratic terms exist, invoke the quadratic adjustment function to linearize the quadratic contribution. This may add neighbor relationships or update the neighbor coefficient lists.

6. **Eta Vector Creation**: In standard mode, allocate an eta vector of Type 1 with sufficient space for the header (0x50 bytes), row indices (aligned), and coefficient values. Initialize the eta header with type=1, link to the existing eta list head, store the pivot variable index, pivot value, and original objective coefficient. Determine the variable's post-pivot status (FIXED if bounds are within TINY_TOL, SUPERBASIC if strictly between bounds, AT_UPPER if at or above upper bound, AT_LOWER otherwise). Copy all column entries corresponding to basic rows into the eta vector's index and value arrays, incrementing work counters. In special mode, create a minimal eta vector (0x28 bytes) with just the basic header and pivot information.

7. **Objective Update**: Clear any special flags on the variable (if creating eta). Add the product of the objective coefficient and pivot value to the current objective value. Set the variable's objective coefficient to zero.

8. **Quadratic Contribution**: If quadratic diagonal terms exist for this variable, add one-half times the square of the pivot value times the quadratic diagonal coefficient to the objective value. Clear the quadratic diagonal entry.

9. **Neighbor List Processing**: If the variable has neighbor relationships, iterate through each neighbor in the list. For each neighbor, invalidate its pricing cache, add the product of the pivot value and the neighbor coefficient to the neighbor's objective coefficient. Find the variable's entry in the neighbor's reverse list and remove it by decrementing the neighbor's count and shifting remaining entries down.

10. **Pricing State Update**: Invoke the pricing update function to refresh any cached pricing data structures based on the variable being pivoted.

11. **RHS and Sparsity Update**: Iterate through all entries in the variable's column. For each entry in a basic row (varStatus >= 0), subtract the product of the pivot value and the coefficient from the row's RHS value, decrement the row's nonzero count, and mark the column entry as removed (set row index to -1). If the variable has special flags or skipped eta creation, mark affected rows in a temporary list for constraint removal.

12. **Variable Status Finalization**: If no special flags are present, mark the variable as AT_UPPER in the basis header and set its column count to -1 (indicating non-basic). If special flags exist, mark the variable as eliminated (basis header = 0, column count = 0) and remove it from all constraint row structures by scanning the temporary row list and clearing matching entries in the row-oriented constraint matrices.

### 4.3 Pseudocode

```
FUNCTION PivotPrimal(env, state, var, tol):
  // Feasibility check
  Δ ← ub[var] - lb[var]
  IF |Δ| < 2·tol THEN
    state.problemVar ← var
    RETURN INFEASIBLE

  // Numeric stability
  IF Δ > WIDE_BOUNDS_THRESHOLD THEN
    maxCoeff ← max{|a_ij| : row i basic, a_ij ∈ column var}
    IF maxCoeff · |Δ| < tol THEN
      RETURN SUCCESS  // Skip unstable pivot

  // Choose pivot value
  c ← objCoeff[var]
  IF |c · Δ| > TINY_THRESHOLD · tol THEN
    value ← (c ≤ 0) ? ub[var] : lb[var]  // Move to favorable bound
  ELSE IF lb[var] > 0 OR ub[var] < 0 THEN
    value ← (lb[var] + ub[var]) / 2      // Midpoint
  ELSE
    value ← 0                             // Zero crossing

  // Handle PWL objectives
  IF var has PWL flag THEN
    seg ← FindSegment(pwlBreakpoints[var], value)
    objCoeff[var] ← pwlSlopes[seg]
    objConstant ← objConstant + pwlOffsets[seg]

  // Handle quadratic objectives
  IF hasQuadratic AND quadratic[var] ≠ 0 THEN
    QuadraticAdjust(env, state, var, value)

  // Create eta vector (if not in special mode)
  IF NOT specialMode THEN
    η ← AllocateEta(column_nnz + header_size)
    η.type ← 1
    η.pivotVar ← var
    η.pivotValue ← value
    η.objCoeff ← c
    η.status ← DetermineStatus(lb[var], ub[var], value)
    η.indices[] ← {i : a_i,var ≠ 0, row i basic}
    η.values[] ← {a_i,var : a_i,var ≠ 0, row i basic}
    PrependToEtaList(η)

  // Update objective
  objConstant ← objConstant + c · value
  objCoeff[var] ← 0
  IF quadratic[var] ≠ 0 THEN
    objConstant ← objConstant + 0.5 · value² · quadratic[var]
    quadratic[var] ← 0

  // Update neighbor objectives (QP/combinatorial)
  FOR EACH neighbor j of var DO
    objCoeff[j] ← objCoeff[j] + value · Q[var,j]
    RemoveFromNeighborList(j, var)

  // Update RHS and remove from matrix
  FOR EACH row i where a_i,var ≠ 0 DO
    rhs[i] ← rhs[i] - value · a_i,var
    MarkRowEntryRemoved(i, var)

  // Finalize variable status
  IF specialFlags THEN
    EliminateVariable(var)  // Remove from constraints
  ELSE
    MarkNonBasic(var)

  UpdatePricingState(var)

  RETURN SUCCESS
```

### 4.4 Mathematical Foundation

**Revised Simplex Method with PFI**

The revised simplex method maintains a factorization of the basis matrix B. When a variable x_j (non-basic) enters the basis or changes value without entering, we update the representation using eta vectors.

Let B^(k) be the basis at iteration k. After a pivot operation on variable j with column vector a_j, we create an eta matrix:

E = I + (η - e_p)e_p^T

where:
- e_p is the p-th unit vector (p is the row leaving the basis, or special marker)
- η is the column vector a_j scaled and transformed

The updated basis inverse is:
B^(k+1)^(-1) = E^(-1) · B^(k)^(-1)

For a primal pivot that doesn't exchange basis variables (just moves a non-basic to a new value), we record the column data as an eta vector to enable efficient FTRAN/BTRAN operations later.

**Objective Update**

Linear objective: f = c^T x + constant

After fixing x_j = v:
- constant' = constant + c_j · v
- c_j' = 0

Quadratic objective: f = c^T x + (1/2) x^T Q x + constant

After fixing x_j = v:
- constant' = constant + c_j · v + (1/2) v^2 · Q_{jj}
- c_i' = c_i + Q_{ij} · v  for i ≠ j
- Q_{jj}' = 0

**Constraint RHS Update**

Constraint: Ax ≤ b

After fixing x_j = v, substitute into constraints:
- b_i' = b_i - A_{ij} · v  for all rows i

This maintains constraint satisfaction: A' x' ≤ b' where x' is x with x_j removed.

**Piecewise Linear Objectives**

PWL objective for variable j: f_j(x_j) = max_k {m_k · x_j + b_k}

At value v, determine active segment s where:
- s = max{k : breakpoint_k ≤ v}

Then:
- c_j = m_s (slope of segment s)
- constant = constant + b_s (offset of segment s)

**References:**
- Dantzig, G. B. (1963). Linear Programming and Extensions. Princeton University Press.
- Maros, I. (2003). Computational Techniques of the Simplex Method. Kluwer Academic Publishers.
- Forrest, J. J., & Goldfarb, D. (1992). Steepest-edge simplex algorithms for linear programming. Mathematical Programming, 57(1-3), 341-374.

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) - Variable has no column entries, no special features
- **Average case:** O(nnz_col + nnz_neighbors) - Must process all column coefficients and neighbor relationships
- **Worst case:** O(nnz_col · m + nnz_neighbors · m) - If special flags require constraint removal, must scan constraint rows

Where:
- nnz_col = number of nonzero coefficients in the variable's column
- nnz_neighbors = number of neighbor relationships for the variable (quadratic/combinatorial)
- m = number of constraints

### 5.2 Space Complexity

- **Auxiliary space:** O(nnz_col) - Allocates one eta vector with indices and coefficients for basic rows
- **Total space:** O(nnz_col) - No additional temporary allocations beyond the eta vector

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| Bounds too tight | 3 (INFEASIBLE) | Upper bound - lower bound < 2 * tolerance, cannot determine feasible value |
| Eta allocation failure | 0x2711 (1001) | Memory allocation failed for eta vector |
| Quadratic adjustment failure | 0x2711 (1001) | cxf_quadratic_adjust returned out of memory |

### 6.2 Error Behavior

On infeasibility (code 3): Sets state to the problematic variable index, allowing caller to identify which variable caused the issue. No state modifications occur.

On out of memory (code 0x2711): Returns immediately without completing the pivot. State may be partially modified (e.g., PWL segment updated, objective partially updated). Caller should treat this as a fatal error requiring cleanup.

On success (code 0): All state modifications are complete and consistent. The variable is effectively removed from active optimization and the eta representation is ready for subsequent FTRAN/BTRAN operations.

Numeric stability bypass: If the column's maximum coefficient times bound range is below tolerance, the function returns success (0) without performing the pivot. This is not an error but a safeguard against numerical instability.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Bounds essentially equal | ub[var] - lb[var] < TINY_TOL | Variable marked as FIXED status, bounds treated as effectively equal |
| Zero objective coefficient | abs(objCoeff * boundRange) < TINY_THRESHOLD * tol | Pivot to midpoint if bounds same sign, otherwise pivot to zero |
| Unbounded variable | lb = -infinity or ub = +infinity | Not typically called on unbounded variables; would pivot to extreme bound |
| Variable already fixed | lb[var] == ub[var] exactly | Pivot proceeds, value determined from equal bounds |
| Empty column | colCounts[var] = 0 | No RHS updates, eta created with zero nonzeros |
| All column entries non-basic | All rows have varStatus < 0 | Eta vector created but with zero effective entries |
| PWL at boundary | value exactly equals breakpoint | Segment selection uses strict < comparison, takes higher segment |
| PWL at upper segment | value exceeds all breakpoints | Clamps to highest segment (segment = pwl_end - 1) |
| Quadratic with no diagonal | quadratic[var] = 0 but neighbors exist | Only neighbor updates performed, no diagonal contribution |
| Special mode enabled | state != 0 | Creates minimal eta (0x28 bytes) with no coefficient data |
| Fast mode enabled | state != 0 | Skips eta creation entirely, only updates objective and bounds |

## 8. Thread Safety

**Thread-safe:** No

This function modifies extensive shared state including the eta list, objective value, RHS array, column data structures, neighbor lists, and pricing caches. It must be called from a single thread with exclusive access to the SolverState.

**Synchronization required:** Caller must hold exclusive lock on the entire SolverState structure before invoking this function. The environment's critical section (if present) should be held by the calling thread.

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_simplex_iterate | Simplex Core | Main iteration loop after ratio test determines degenerate pivot |
| cxf_simplex_phase1 | Simplex Core | Phase 1 to move artificial variables to zero |
| cxf_presolve_reduce | Presolve | When variable proven to be at bound |
| cxf_crash_basis | Initialization | During crash basis construction for zero-valued variables |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_pivot_bound | Similar operation but for moving variable to bound without basis change |
| cxf_pivot_dual | Dual simplex counterpart, handles dual pivots |
| cxf_fix_variable | Permanent variable elimination (similar objective updates but no eta) |
| cxf_ftran | Uses eta vectors created by this function for forward transformation |
| cxf_btran | Uses eta vectors created by this function for backward transformation |
| cxf_eta_refactor | May be called when eta list becomes too long |

## 11. Design Notes

### 11.1 Design Rationale

The function combines several related operations (pivot, objective update, RHS adjustment, neighbor handling) into a single routine to minimize state access overhead and maintain consistency. The PFI representation is chosen over other basis update schemes (LU refactorization, Bartels-Golub update) because it's simple to implement, requires no complex matrix operations during the pivot itself, and defers the computational cost to FTRAN/BTRAN operations which are already highly optimized.

The infeasibility check at the start serves as an early detection mechanism for numerical issues that would otherwise manifest as convergence failures or incorrect solutions later in the solve. By detecting bound violations immediately, the solver can either tighten tolerances, switch to dual simplex, or report the infeasibility to the user.

The special handling for PWL and quadratic objectives reflects Convexfeld's architecture where these non-linear features are linearized incrementally during the simplex solve rather than being fully linearized in a preprocessing step. This allows the solver to exploit the structure of these problems more effectively.

The neighbor list mechanism is a sophisticated optimization for problems with quadratic objectives or indicator constraints where variables have direct relationships. By maintaining explicit neighbor lists, the solver can update only affected variables' objective coefficients rather than scanning the entire quadratic matrix or constraint set.

### 11.2 Performance Considerations

The two-mode design (full eta vs minimal eta) allows the solver to trade memory for speed. In fast mode or when handling general constraints, the solver skips eta creation or creates minimal placeholders, reducing memory allocation overhead at the cost of less accurate basis tracking.


Work counter increments provide runtime visibility into the computational cost of each pivot. These can be used for adaptive strategy switching (e.g., switching from primal to dual, triggering refactorization) based on observed work patterns.

The temporary row marking mechanism (using tempValues as a set membership flag) avoids costly set operations when processing rows affected by general constraint elimination. This is a classic space-time tradeoff: use O(m) temporary space to achieve O(1) membership tests.

### 11.3 Future Considerations

The current implementation processes neighbor lists sequentially, which could be parallelized for variables with many neighbors (common in large quadratic programs). However, this requires careful synchronization to avoid race conditions when multiple neighbors try to remove the same variable.

The eta vector allocation could be optimized by pre-allocating a pool of common sizes (e.g., small columns with < 10 entries) to reduce allocator overhead. The current design allocates variable-sized blocks which may cause fragmentation over long solves.

The infeasibility detection could be enhanced to provide more context (e.g., which bound is problematic, what the bound values are) to help with debugging and user error messages.

## 12. References

- Dantzig, G. B. (1963). *Linear Programming and Extensions.* Princeton University Press. (Revised simplex method foundation)
- Maros, I. (2003). *Computational Techniques of the Simplex Method.* Kluwer Academic Publishers. (PFI and eta vectors, Chapter 5)
- Forrest, J. J., & Goldfarb, D. (1992). Steepest-edge simplex algorithms for linear programming. *Mathematical Programming*, 57(1-3), 341-374. (Pricing strategies)
- Bixby, R. E. (2002). Solving real-world linear programs: A decade and more of progress. *Operations Research*, 50(1), 3-15. (Modern simplex implementation practices)
- Achterberg, T. (2007). Constraint Integer Programming. PhD Thesis, TU Berlin. (Variable fixing in MIP context)

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
