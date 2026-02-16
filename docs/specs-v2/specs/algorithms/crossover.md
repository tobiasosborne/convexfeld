# Barrier-to-Simplex Crossover

## Published Reference

- Megiddo, N. (1991). "On finding primal- and dual-optimal bases." *ORSA Journal on Computing*, 3(1):63-65. Establishes that an optimal basis can be recovered from an optimal primal-dual pair in strongly polynomial time, providing the theoretical foundation for crossover.
- Bixby, R.E. and Saltzman, M.J. (1994). "Recovering an optimal LP basis from an interior point solution." *Operations Research Letters*, 15(4):169-178. Describes a practical crossover procedure that ranks variables by distance to bounds, constructs a candidate basis, and cleans up with simplex.
- Andersen, E.D. and Ye, Y. (1996). "Combining interior-point and pivoting algorithms for linear programming." *Management Science*, 42(12):1719-1731. Introduces the approximate problem approach: construct a nearby LP from an interior-point iterate, solve for a basis of the approximate problem, and prove it is optimal for the original when the iterate is sufficiently close to the optimal face.
- Mehrotra, S. and Ye, Y. (1993). "Finding an interior point in the optimal face of linear programs." *Mathematical Programming*, 62(1-3):497-515. Proves that optimal partition identification can be performed in polynomial time from interior-point iterates.

The crossover algorithm described here follows the general three-phase structure common to modern LP solvers: variable classification, push-to-bound, and simplex cleanup. The specific variant combines a distance-based classification (Bixby and Saltzman, 1994) with optional primal-first or dual-first push ordering (Andersen and Ye, 1996) and supports quadratic objective extensions.

## Purpose

After a barrier (interior-point) method solves a linear program, the resulting solution lies in the interior of the feasible polytope rather than at a vertex. Many downstream operations require a basic feasible solution (vertex solution):

- **Warm-starting**: Subsequent solves (e.g., after adding cuts or changing bounds) are far more efficient when starting from a basis.
- **Sensitivity analysis**: Post-optimality analysis requires knowledge of which constraints are active, which is encoded in the basis.
- **Uniqueness**: Interior-point solutions are not unique in degenerate problems; a basic solution provides a canonical representation.

The crossover procedure bridges the gap between interior-point and simplex representations by systematically pushing variables to their bounds and constructing a valid basis.

## Inputs

- **Primal solution** x: A vector of variable values from the barrier method, satisfying Ax = b (or near-feasibility within barrier convergence tolerance). Each component x_j satisfies l_j <= x_j <= u_j.
- **Dual solution** y, s: Dual multipliers y for constraints and reduced costs s for variables, satisfying A^T y + s = c (or near-feasibility).
- **Constraint matrix** A: Stored in both compressed sparse column (CSC) and compressed sparse row (CSR) formats for efficient row and column access.
- **Bounds** l, u: Lower and upper bounds on each variable. Some variables may be free (l_j = -infinity, u_j = +infinity) or fixed (l_j = u_j).
- **Objective coefficients** c: Linear objective vector. For quadratic problems, the diagonal of the Hessian Q may also be present.
- **Variable types**: Classification of each variable as continuous, binary, or general integer.
- **Tolerances**: Feasibility tolerance (epsilon_feas), optimality tolerance (epsilon_opt), and a snap tolerance (epsilon_snap) for deciding proximity to bounds.
- **Crossover mode**: A configuration parameter selecting the push ordering (primal-first or dual-first) and the cleanup algorithm (primal simplex or dual simplex).

**Preconditions:**

1. The barrier method has converged to an approximate optimal solution within its convergence tolerance.
2. The constraint matrix and its dual representation (CSR/CSC) are consistent and valid.
3. All variable bounds are well-defined, with l_j <= u_j for all j.
4. The solver state has been initialized with working copies of bounds and objective coefficients.

## Outputs

- **Basic feasible solution** x*: A vertex of the feasible polytope that is optimal (or near-optimal within tolerances).
- **Optimal basis** B: A partition of variables into basic and non-basic, where each non-basic variable is at one of its bounds and the basic variables satisfy the constraint system.
- **Variable status array**: For each variable j, one of:
  - BASIC (with the constraint row index in which j is basic)
  - AT_LOWER (non-basic at lower bound)
  - AT_UPPER (non-basic at upper bound)
  - SUPERBASIC (non-basic between bounds; should be zero at completion for LP)
  - FIXED (lower bound equals upper bound)
- **Basis header**: An array mapping each constraint row to the index of its basic variable.

**Postconditions:**

1. The variable status array has exactly m entries with BASIC status, where m is the number of constraints.
2. Every non-basic variable is at one of its bounds (within feasibility tolerance).
3. The basis matrix B is nonsingular.
4. The solution satisfies primal feasibility: |Ax* - b| <= epsilon_feas componentwise.
5. The solution satisfies dual feasibility: reduced costs have the correct sign for their bound status, within epsilon_opt.

## Algorithm Description

### Overview

The crossover proceeds in three main phases. First, variables are classified based on their proximity to bounds in the barrier solution. Second, interior variables are "pushed" to their nearest bound through a sequence of simplex-like pivot operations, creating a partial basis. Third, a cleanup phase of standard simplex iterations resolves any remaining infeasibilities and achieves strict complementarity.

The algorithm supports two orderings: primal-first (push primal variables to bounds, then dual) and dual-first (push dual variables, then primal). The choice of ordering is a configuration parameter. A special sub-phase handles variables with quadratic objective terms, and another handles binary variable simplification.

### Detailed Steps

#### Phase 0: Preprocessing and Special Cases

1. **Quadratic variable processing** (if the problem has a quadratic objective):
   For each variable x_j with a separable diagonal quadratic term Q_jj (and no off-diagonal quadratic coupling):
   - Compute the unconstrained minimizer of the univariate function f_j(x) = c_j x + (1/2) Q_jj x^2, yielding x* = -c_j / Q_jj.
   - Clamp x* to the feasible interval [l_j, u_j].
   - For integer variables, evaluate f_j at floor(x*) and ceil(x*), choosing the integer with lower objective value.
   - Compare f_j at the clamped/rounded x* against f_j(l_j) and f_j(u_j), and push the variable to whichever point achieves the minimum.
   - If the computed target exceeds a large-value threshold (indicating unboundedness), terminate with an error.

2. **Binary variable linearization** (optional, controlled by a configuration parameter):
   For binary variables x_j in {0, 1} with a diagonal quadratic term Q_jj and no off-diagonal coupling, exploit the identity x^2 = x for x in {0, 1}:
   - Replace the linear coefficient: c'_j = c_j + (1/2) Q_jj.
   - Set Q_jj = 0.
   - This converts the quadratic contribution to a linear penalty, simplifying subsequent processing.

3. **Fixed variable identification**:
   Variables with u_j - l_j <= epsilon_snap are classified as FIXED and excluded from crossover processing.

4. **Singleton constraint processing**:
   For equality constraints with a single nonzero coefficient a_ij, the variable value is determined directly: x_j = b_i / a_ij. If this value violates bounds by more than the tolerance, report a crossover error. Otherwise, perform a pivot to make this variable basic in the corresponding row.

#### Phase 1: Variable Classification

For each variable x_j with finite bounds that is not already fixed or classified:

1. Compute the distance to each bound:
   - d_lower(j) = x_j - l_j
   - d_upper(j) = u_j - x_j

2. Classify according to proximity:
   - If d_lower(j) < epsilon_snap: assign status AT_LOWER
   - If d_upper(j) < epsilon_snap: assign status AT_UPPER
   - If both d_lower(j) >= epsilon_snap and d_upper(j) >= epsilon_snap: assign status SUPERBASIC (interior, needs pushing)

3. For free variables (l_j = -infinity or u_j = +infinity), assign status BASIC (they will be placed in the basis).

This classification follows the approach of Bixby and Saltzman (1994), who rank variables by their distance to the nearest bound.

#### Phase 2: Push to Bound

The push phase moves SUPERBASIC variables to one of their bounds. The order of pushing is determined by the crossover mode:

**Primal push** (moves primal variables to bounds):

For each SUPERBASIC variable x_j, ordered by some priority criterion (e.g., smallest distance to nearest bound first):

1. Determine the target bound: if d_lower(j) <= d_upper(j), push to l_j; otherwise push to u_j.
2. Compute the displacement: delta = target - x_j.
3. Update the right-hand side of each constraint involving x_j:
   For each constraint i with coefficient a_ij: b'_i = b_i - a_ij * delta.
4. Set x_j to the target value and update its status to AT_LOWER or AT_UPPER.
5. If the resulting system requires a basis exchange (a basic variable becomes infeasible due to the RHS change), perform a simplex pivot to restore feasibility.
6. Invalidate any cached pricing data for the affected variable.

**Dual push** (moves dual variables / reduced costs to bounds):

Analogous to the primal push but operates on the dual problem. Constraint dual multipliers are pushed to satisfy complementary slackness:

1. For each constraint i where the dual y_i is not at a bound implied by the constraint sense:
   - For a less-than-or-equal constraint: y_i should be non-positive at optimality.
   - For a greater-than-or-equal constraint: y_i should be non-negative.
   - For an equality constraint: y_i is unrestricted.
2. Push y_i to zero (or the appropriate bound) using a dual simplex pivot.
3. Update reduced costs accordingly.

The push ordering follows the recommendation of Andersen and Ye (1996): performing the dual push first tends to be more efficient because dual pivots are typically cheaper, but the choice is problem-dependent.

#### Phase 3: Basis Construction and Activation

After the push phases, some constraints may not yet be represented in the basis. These are activated:

1. **Count activatable constraints**: Scan all pending constraints. A constraint is activatable if it has column-space activity (nonzero coefficients among unpushed variables) but no row-space activity (no basic variable assigned to it yet).
2. **Activate each constraint**:
   - Add the constraint row to the basis representation.
   - Copy the right-hand side value and constraint sense into the working dual arrays.
   - For each nonzero coefficient in the constraint, perform a forward transformation (FTRAN) to express the column in terms of the current basis.
   - Update the basis factorization.
   - Invalidate pricing for the newly added row.
3. **Mark processed variables**: Set processing flags on variables that participated in activation, so they are not re-examined in subsequent passes.

#### Phase 4: Simplex Cleanup

The partial basis from Phases 1-3 may have residual primal or dual infeasibilities. The cleanup phase resolves these using standard simplex iterations:

1. Initialize the simplex algorithm from the partial basis as a warm start.
2. Select either primal simplex or dual simplex based on the crossover mode configuration:
   - If the basis is primal-feasible but dual-infeasible, use primal simplex.
   - If the basis is dual-feasible but primal-infeasible, use dual simplex.
   - The configuration parameter may override this automatic selection.
3. Iterate until the standard optimality conditions are satisfied (reduced cost complementarity within epsilon_opt) or an iteration limit is reached.
4. Because the push phases have placed most variables at their correct bounds, the cleanup phase typically requires far fewer iterations than solving from scratch -- often only a small fraction of the total variable count.

### Key Design Choices

- **Push ordering is configurable**: The solver supports both primal-first and dual-first orderings, with an automatic mode that selects based on problem characteristics. This follows the observation by Andersen and Ye (1996) that the best ordering is problem-dependent.
- **Quadratic terms handled before linear crossover**: Variables with separable quadratic objectives are processed in a separate preliminary phase, because their optimal placement within bounds can be computed analytically from the univariate quadratic formula. Off-diagonal quadratic coupling is deferred to the simplex cleanup phase.
- **Binary linearization is optional**: The conversion of x^2 to x for binary variables is gated by a configuration parameter, as it modifies the objective and may not always be beneficial.
- **Pricing cache invalidation**: Each variable push invalidates the pricing cache for that variable, ensuring that subsequent simplex pivots use correct reduced costs. This is critical for numerical accuracy in the cleanup phase.
- **Distance-based classification**: The snap tolerance for deciding whether a variable is "at a bound" is a separate parameter from the feasibility tolerance used in simplex. A variable within epsilon_snap of a bound is snapped directly; this avoids unnecessary pivots for nearly-bound variables.
- **Dual representation of the constraint matrix**: Both CSR and CSC formats are used. CSC is needed for column-oriented operations during push (computing constraint violations from a column's coefficients), while CSR is needed for row-oriented operations during basis activation and pivot selection.

## Numerical Considerations

### Tolerances

| Tolerance | Typical Range | Role |
|-----------|---------------|------|
| Snap tolerance (epsilon_snap) | 1e-6 to 1e-9 | Threshold for classifying a variable as "at bound" vs "interior" |
| Feasibility tolerance (epsilon_feas) | 1e-6 to 1e-8 | Maximum allowable constraint violation in the final solution |
| Optimality tolerance (epsilon_opt) | 1e-6 to 1e-8 | Maximum allowable reduced cost violation at optimality |
| Barrier convergence tolerance | Problem-dependent | Accuracy of the interior-point solution provided as input |

### Stability Concerns

1. **Ill-conditioned basis matrices**: When many variables are simultaneously near their bounds (common for barrier solutions close to convergence), the candidate basis may be ill-conditioned. The basis refactorization performed during cleanup helps restore numerical stability, but severely ill-conditioned problems may require perturbation or iterative refinement.

2. **Coefficient range issues**: If the constraint matrix has coefficients spanning many orders of magnitude, the push phase may introduce large cancellation errors when updating right-hand sides. Scaling the problem before crossover (a standard preprocessing step) mitigates this.

3. **Snap tolerance sensitivity**: Setting epsilon_snap too large causes variables that should remain interior to be prematurely snapped, leading to infeasibility in the cleanup phase. Setting it too small leaves many variables as SUPERBASIC, increasing the number of pivots required. The tolerance should be set in relation to the barrier convergence tolerance: snapping is only meaningful if the barrier solution is accurate enough to distinguish "at bound" from "interior."

4. **Degeneracy**: If multiple optimal bases exist (common in degenerate problems), the crossover procedure may find any one of them. The specific basis obtained depends on the push ordering and tie-breaking rules. This is not a numerical error but may affect warm-starting if the user expects a particular basis structure.

5. **Unboundedness detection**: During the push of quadratic variables, if a computed target value exceeds a large threshold (typically half the solver's infinity value), the problem is flagged as unbounded. This catches cases where a concave quadratic objective leads to divergence.

## Termination

The crossover algorithm terminates when one of the following conditions is met:

1. **Successful completion**: All SUPERBASIC variables have been pushed to bounds, all constraints have been activated, and the simplex cleanup phase has achieved primal and dual feasibility within tolerances. The resulting solution is a basic feasible solution that is optimal.

2. **Infeasibility during push**: A variable cannot be pushed to any bound without violating constraint feasibility beyond the tolerance. The crossover reports an error identifying the problematic variable.

3. **Memory allocation failure**: Work arrays for classification or constraint activation cannot be allocated. The crossover returns an out-of-memory error.

4. **Iteration limit in cleanup**: The simplex cleanup phase exceeds its iteration limit without achieving optimality. The partial basis is returned with a sub-optimal status.

5. **Unbounded detection**: A variable's target value during quadratic processing exceeds the solver's infinity threshold.

**Convergence guarantee**: For a well-posed LP where the barrier method has converged to a sufficiently accurate solution, the crossover is guaranteed to find an optimal basis. Megiddo (1991) proved that this can be done in strongly polynomial time. In practice, the number of cleanup simplex iterations is typically small -- often O(m) or less -- because the push phases have already placed most variables at their correct optimal bounds.

## Complexity

### Time Complexity

| Phase | Best Case | Typical Case | Worst Case |
|-------|-----------|--------------|------------|
| Phase 0 (quadratic preprocessing) | O(n) | O(n) | O(n) |
| Phase 1 (classification) | O(n) | O(n) | O(n) |
| Phase 2 (push to bound) | O(n) per push, O(n) pushes | O(n * k) where k << n | O(n^2 * m) if every push triggers a refactorization |
| Phase 3 (basis activation) | O(nnz) | O(nnz) | O(nnz * m) with refactorization |
| Phase 4 (simplex cleanup) | O(1) if basis is already optimal | O(m) iterations | O(2^m) (simplex worst case, extremely rare) |

Where n = number of variables, m = number of constraints, nnz = number of nonzeros in the constraint matrix, and k = number of superbasic variables requiring pushes.

In practice, the entire crossover typically completes in O(n + m + nnz) time plus a small number of simplex iterations, making it a minor fraction of the total barrier solve time for well-conditioned problems.

### Space Complexity

- O(n) for the classification array and variable status arrays.
- O(n + m) for working copies of bounds, reduced costs, and dual values.
- O(nnz) for the constraint matrix (already allocated by the solver).
- Temporary work arrays for sorting and pivot selection: O(n).

Total additional space beyond the existing solver state: O(n + m).

## Edge Cases

1. **All variables at bounds**: If the barrier solution places every variable within epsilon_snap of a bound, classification is trivial and no pushes are needed. The cleanup phase verifies feasibility and may perform zero iterations.

2. **Free variables** (l_j = -infinity, u_j = +infinity): Free variables have no bound to push to. They are placed in the basis as SUPERBASIC and handled entirely by the cleanup simplex phase. The push phase skips them.

3. **Fixed variables** (l_j = u_j): These are classified as FIXED and excluded from crossover processing. Their values are set to the common bound.

4. **Empty problem** (n = 0 or m = 0): The crossover returns immediately with success. No classification or push is needed.

5. **Singleton constraints**: Equality constraints with a single nonzero coefficient uniquely determine the variable value. These are handled in the preprocessing phase by direct assignment.

6. **Pure quadratic with no linear term**: When c_j = 0 and Q_jj > 0, the unconstrained minimizer is x* = 0. If 0 is feasible, the variable is pushed to 0 (which may or may not coincide with a bound).

7. **Variables with off-diagonal quadratic coupling**: These cannot be optimized independently and are skipped during the quadratic preprocessing phase. They are handled by the simplex cleanup phase, which can optimize coupled variables simultaneously through pivoting.

8. **SOS (Special Ordered Set) constraints**: Constraints involving binary and continuous variables in SOS1 or SOS2 patterns receive special handling. The crossover detects these patterns (typically constraints with exactly three nonzeros involving binary variables with unit coefficients) and may introduce auxiliary variables and constraints to facilitate basis construction.

9. **Very large problems**: For problems with millions of variables, the classification and push phases use cache-efficient scanning patterns. Constraint counting loops may be unrolled for performance. The dominant cost is typically the cleanup simplex phase.

10. **Barrier did not converge accurately**: If the barrier method terminated early or with loose tolerances, many variables will be classified as SUPERBASIC, increasing the number of pushes and cleanup iterations. In extreme cases, the crossover degenerates to a full simplex solve from a poor starting point.

## Verification Checklist

```
[x] No hex addresses or offsets
[x] No Ghidra artifacts (param_, local_, iVar, DAT_, FUN_)
[x] No binary-specific constants
[x] No copied code fragments
[x] All algorithms cite published sources
[x] All data structures described semantically
[x] Passes the Clean Room Test (Rule 10)
```

## References

- Andersen, E.D. and Ye, Y. (1996). "Combining interior-point and pivoting algorithms for linear programming." *Management Science*, 42(12):1719-1731.
- Bixby, R.E. and Saltzman, M.J. (1994). "Recovering an optimal LP basis from an interior point solution." *Operations Research Letters*, 15(4):169-178.
- Dantzig, G.B. (1963). *Linear Programming and Extensions*. Princeton University Press.
- Maros, I. (2003). *Computational Techniques of the Simplex Method*. Springer.
- Megiddo, N. (1991). "On finding primal- and dual-optimal bases." *ORSA Journal on Computing*, 3(1):63-65.
- Mehrotra, S. and Ye, Y. (1993). "Finding an interior point in the optimal face of linear programs." *Mathematical Programming*, 62(1-3):497-515.
- Vanderbei, R.J. (2014). *Linear Programming: Foundations and Extensions*. 4th edition. Springer. Chapter 19.5.
