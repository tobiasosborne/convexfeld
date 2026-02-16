# Perturbation and Anti-Cycling

## Published Reference

- Wolfe, P. (1963). "A technique for resolving degeneracy in linear programming." *SIAM Journal on Applied Mathematics*, 11(2):205-211.
- Bland, R.G. (1977). "New finite pivoting rules for the simplex method." *Mathematics of Operations Research*, 2(2):103-107.
- Gill, P.E., Murray, W., Saunders, M.A., and Wright, M.H. (1989). "A practical anti-cycling procedure for linearly constrained optimization." *Mathematical Programming*, 45(1-3):437-474.
- Harris, P.M.J. (1973). "Pivot selection methods of the Devex LP code." *Mathematical Programming*, 5(1):1-28.
- Maros, I. (2003). *Computational Techniques of the Simplex Method*. Springer. Chapters 6-7.

The algorithm described here is a bound perturbation technique in the family of the EXPAND procedure (Gill et al., 1989). The EXPAND approach maintains a working feasibility tolerance that grows monotonically over iterations, effectively perturbing variable bounds to ensure that every simplex pivot produces a strictly positive step length. This prevents cycling in degenerate linear programs. The implementation combines elements from Wolfe's perturbation method (Wolfe, 1963) with the practical implied-bound analysis described by Maros (2003).

## Purpose

In the simplex method, **degeneracy** occurs when one or more basic variables sit exactly at a bound, so that a simplex pivot produces a zero step length. When the objective value does not change over many pivots, the algorithm is said to be **stalling**. In the worst case, the same sequence of bases repeats indefinitely, a phenomenon called **cycling**. Although cycling is theoretically possible and has been demonstrated on constructed examples, stalling is the far more common practical concern, since it wastes computational effort without progress toward optimality.

Anti-cycling techniques ensure that the simplex algorithm terminates in a finite number of pivots. The perturbation approach described here temporarily modifies the working bounds of variables so that degenerate vertices become non-degenerate. After the perturbed problem reaches optimality, the perturbations are removed and the solution is refined back to feasibility for the original problem.

This algorithm is invoked within the main simplex iteration loop when stalling is detected. It works in conjunction with stalling detection (via basis snapshot comparison) and solution refinement (post-unperturbation cleanup).

## Inputs

- **SolverState**: The current state of the simplex solver, including:
  - Working lower bounds and working upper bounds for all variables
  - Saved (original) lower bounds and saved (original) upper bounds
  - Variable status array (basic, at lower bound, at upper bound, superbasic, fixed)
  - Basis header mapping each constraint row to its basic variable
  - Row-major sparse constraint matrix (CSR format)
  - Reduced cost vector
  - Constraint sense array (equality, less-than-or-equal, greater-than-or-equal)
  - Pricing state (the set of candidate variables for the next pivot)
  - Perturbation counter (cumulative count of variables perturbed)
  - Work counter (performance profiling accumulator)

- **Environment**: Solver parameters, including:
  - Optimality tolerance (typically 1e-6 to 1e-8)
  - Primal feasibility tolerance (typically 1e-6)
  - Verbosity level

- **Preconditions**:
  - The solver is in a valid state with a complete basis (exactly numConstrs basic variables).
  - Saved bounds contain the original unperturbed problem bounds.
  - Working bounds may already reflect prior perturbations.
  - Stalling has been detected by the caller (see Section: Stalling Detection).

## Outputs

- **Modified SolverState**:
  - Some variables removed from the pricing candidate set (no longer eligible for entering the basis).
  - Variable statuses updated for removed variables.
  - Perturbation counter incremented by the number of variables perturbed in this call.
  - Work counter updated to reflect computational effort.

- **Status code**:
  - SUCCESS: Perturbation applied, solver should continue iterating.
  - INFEASIBLE: A bound violation was detected that cannot be resolved by perturbation (lower bound exceeds upper bound at some variable, indicating the original problem is infeasible).

- **Postconditions**:
  - All remaining pricing candidates have implied bound gaps large enough to permit non-degenerate pivots.
  - Variables removed from pricing will not be selected as entering variables until the pricing state is rebuilt (typically at the next refactorization).

## Algorithm Description

### Overview

The anti-cycling system operates at three levels within the simplex iteration loop:

1. **Stalling detection** determines when the simplex algorithm is making insufficient progress, by comparing basis snapshots taken at intervals.

2. **Bound perturbation** (the core of this specification) processes pricing candidates to remove degenerate variables, effectively perturbing the problem so that ratio tests produce strictly positive step lengths.

3. **Unperturbation and refinement** restores the original bounds after the perturbed problem is solved and performs cleanup iterations to recover a feasible solution to the original problem.

The perturbation strategy used here is an *implied bound analysis* approach: rather than adding explicit random noise to every variable bound (as in classical perturbation), the algorithm examines each pricing candidate and determines whether the constraint matrix structure forces the variable into a degenerate position. Variables that are irretrievably degenerate are removed from the pricing set. This is more targeted than blanket perturbation and avoids introducing unnecessary numerical noise.

### Detailed Steps

#### Phase 1: Stalling Detection (Caller Responsibility)

The main iteration loop detects stalling using a **basis snapshot comparison** protocol:

1. Before each batch of simplex iterations, capture a **basis snapshot** -- a vector of summary statistics about the current solver state, including the counts of removed rows, removed columns, inequality-to-equality conversions, and various iteration counters.

2. After the batch completes, compute a **weighted difference score** between the current state and the snapshot. The score is:

   D = sum over categories of (delta_i * w_i) / N_i

   where delta_i is the change in counter i, w_i is a category weight, and N_i is a normalization factor (such as the number of working columns or working rows). The weighting ensures that structural changes (variable eliminations, constraint tightenings) contribute more to the score than simple iteration counts.

3. If D is below a threshold proportional to the number of iterations elapsed since the last snapshot, the algorithm is considered to be stalling. The threshold formula is:

   threshold = max(0, k - k_0) * tau

   where k is the inner iteration count, k_0 is a grace period (allowing a few iterations before stalling is diagnosed, typically 5), and tau is a base threshold parameter. During the first few iterations, the threshold is zero, meaning any change is considered sufficient progress.

4. When stalling is detected (D <= threshold), the perturbation procedure is invoked.

Additionally, perturbation is applied proactively during the early iterations of the first outer loop pass (before stalling has been specifically diagnosed), as a preventive measure. This is controlled by a configuration parameter that enables or disables proactive perturbation.

#### Phase 2: Candidate Retrieval

1. Query the pricing subsystem for the current list of pricing candidates -- variables that have reduced costs indicating potential objective improvement.

2. Validate the candidate list via a pre-perturbation check that verifies the consistency of the candidate data with the current solver state.

#### Phase 3: Bound Restoration (Verbose Mode)

When operating in a detailed diagnostic mode, copy the saved (original) bounds into the working bound arrays. This ensures that the perturbation analysis is performed against the unperturbed problem, preventing accumulation of perturbation drift. After copying, recompute derived quantities (such as constraint activities) that depend on the bounds.

This step is conditional on a verbosity or diagnostic parameter. In standard mode, the perturbation analysis operates on the current working bounds directly.

#### Phase 4: Candidate Processing

Process each pricing candidate. The behavior depends on the variable's status:

**Case A: Non-basic variable at lower bound (status = AT_LOWER)**

1. Examine the variable's reduced cost d_j.
2. If d_j is strictly less than the negative of the primal feasibility tolerance (d_j < -epsilon_p), this indicates a severe bound violation. If the variable participates in an equality constraint and the reduced cost is large in the opposite direction, report INFEASIBLE.
3. Otherwise, mark the variable as removed from pricing (set status to a non-candidate value) and increment the perturbation counter.

This case handles variables that are sitting at their lower bounds but cannot improve the objective -- they are degenerate candidates that should not be selected for pivoting.

**Case B: Basic variable (status > 0, indicating the basis row index)**

For a basic variable x_j with basis row r, compute implied bounds using the saved (original) bounds of the other variables in the constraint:

1. Let the constraint row for variable x_j be:

   sum over i of a_{ri} * x_i = b_r

   Rearranging for x_j:

   x_j = (b_r - sum over i != j of a_{ri} * x_i) / a_{rj}

2. Compute an implied lower bound on x_j by assuming all other variables in the row take their most restrictive values:
   - For each coefficient a_{ri} in the row:
     - If a_{ri} < 0: use the saved lower bound of x_i (if finite) for the implied lower bound, and the saved upper bound for the implied upper bound.
     - If a_{ri} > 0: use the saved upper bound of x_i (if finite) for the implied lower bound, and the saved lower bound for the implied upper bound.
   - Track how many coefficients have unbounded contributions (count_lower_unbounded and count_upper_unbounded).

3. Compute the implied bound gap:

   gap = implied_lower - implied_upper

4. Clamp the gap to lie within a minimum bound range (a small positive constant, on the order of 1e-10) and a maximum perturbation magnitude (on the order of 1e-6):

   perturbation_magnitude = clamp(gap, min_bound_range, max_perturbation)

   The minimum bound range prevents the gap from collapsing to zero (which would reintroduce degeneracy). The maximum perturbation magnitude prevents overly large perturbations that could compromise solution accuracy.

5. **Infeasibility check for equality constraints**: If the variable participates in an equality constraint and the implied bounds prove the constraint cannot be satisfied (the implied upper bound exceeds the perturbation magnitude times the feasibility tolerance, with no unbounded contributions that could relieve it), report INFEASIBLE.

6. **Infeasibility check for inequality constraints**: For inequality constraints, perform a similar test. If the implied lower bound is strictly negative (below -epsilon_p * perturbation_magnitude) and there are no unbounded contributions from below, the variable is degenerate with respect to this constraint.

7. **Degenerate variable removal**: When a variable is identified as irrecoverably degenerate (the implied bound analysis shows no room for a non-zero step):
   - Remove all entries of this variable's column from the pricing candidate set.
   - Decrement the basis positions of the affected constraints.
   - Mark the column indices as inactive.
   - Set the variable status to AT_UPPER (indicating it has been handled by the perturbation mechanism).
   - Increment the perturbation counter.

#### Phase 5: Counter Update

After processing all candidates:
1. Add the count of newly perturbed variables to the cumulative perturbation counter in the solver state.
2. Update the work counter to reflect the number of candidates processed.

### Key Design Choices

- **Implied bound analysis over random perturbation**: Rather than applying random perturbations to all variable bounds (as in the classical method of Wolfe, 1963), this approach uses the constraint structure to identify which variables are truly degenerate. This is more targeted and avoids perturbing variables that are already non-degenerate. The trade-off is higher per-iteration cost in the perturbation step, but fewer total simplex iterations because non-degenerate variables are not artificially disturbed.

- **Saved vs. working bounds**: Maintaining two copies of the bounds (saved/original and working) follows the EXPAND paradigm (Gill et al., 1989). The saved bounds represent the original problem; the working bounds may be temporarily modified. Perturbation analysis uses the saved bounds to avoid compounding perturbation errors across multiple invocations.

- **Selective candidate removal**: Instead of perturbing bounds globally, the algorithm removes individual degenerate candidates from the pricing set. This is equivalent to perturbation from the simplex algorithm's perspective (the removed variables will not be selected as entering variables), but avoids modifying the bound arrays explicitly. The pricing set is rebuilt at the next basis refactorization, at which point previously removed candidates may re-enter.

- **Proactive perturbation in early iterations**: The main iteration loop applies perturbation proactively during the first one or two inner iterations (before stalling is detected), as a preventive measure. This reflects the practical observation that degeneracy is most likely to cause problems at the start of the solve, when the initial basis may contain many degenerate variables.

- **Grace period before stalling diagnosis**: The basis snapshot comparison uses a grace period (typically 5 inner iterations) before declaring stalling. This avoids false positives during the initial phase when the solver is establishing its working basis.

## Numerical Considerations

### Tolerances

| Tolerance | Role | Typical Range |
|-----------|------|---------------|
| Optimality tolerance | Determines whether a reduced cost is significant enough to indicate potential improvement | 1e-6 to 1e-8 |
| Primal feasibility tolerance | Threshold for accepting bound violations as tolerable | 1e-6 |
| Minimum bound range | Floor on the implied bound gap, prevents collapse to exact degeneracy | ~1e-10 |
| Maximum perturbation magnitude | Ceiling on the perturbation size, prevents large distortions | ~1e-6 |

### Stability Concerns

- **Perturbation magnitude selection**: The perturbation magnitude is clamped between the minimum bound range and the maximum perturbation. If the implied bound gap is smaller than the minimum bound range, the minimum is used; if larger than the maximum perturbation, the maximum is used. This two-sided clamping ensures that perturbations are always large enough to break degeneracy but small enough to maintain solution quality.

- **Implied bound accumulation errors**: When computing implied bounds by summing products of coefficients and saved bounds across a constraint row, floating-point rounding errors can accumulate, especially for rows with many nonzeros or with coefficients of widely varying magnitude. The algorithm uses standard double-precision arithmetic without compensated summation in this phase, relying on the tolerances to absorb rounding errors.

- **Interaction with saved bounds**: The use of saved (original) bounds for the implied bound computation prevents perturbation drift. Even if the working bounds have been modified by previous perturbation calls, the implied bound analysis always starts from the original problem data.

### Degeneracy Handling

The entire purpose of this algorithm is degeneracy handling. The key insight is that degeneracy at a vertex of the feasible polyhedron means multiple constraints are active simultaneously, causing the simplex algorithm to pivot among equivalent bases without changing the objective. By removing degenerate variables from the pricing set (or, equivalently, by perturbing bounds so that exactly one constraint is active at each variable), the algorithm ensures progress.

## Termination

### Perturbation Step Termination

The perturbation procedure itself terminates in O(C * R) time where C is the number of pricing candidates and R is the average number of nonzeros per row, since it performs a single pass over all candidates.

### Solver-Level Termination

With perturbation active, the simplex algorithm is expected to terminate because:

1. **Each perturbation call removes at least one degenerate variable** from the pricing set (or detects infeasibility). The number of variables is finite, so the number of perturbation calls is bounded.

2. **The basis snapshot comparison** in the outer loop ensures that if the solver makes no progress across an entire outer iteration (including perturbation), the outer loop exits. The outer iteration count is bounded by a configurable limit (default 100 for dual simplex, 10 during crossover, 5 for primal simplex).

3. **Fallback**: If perturbation fails to resolve stalling (the EXPAND approach is not guaranteed to prevent cycling in all cases, as shown by Hall and McKinnon, 2001), the solver relies on the outer iteration limit and may report a suboptimal or numerically difficult status.

For guaranteed finiteness, Bland's rule (Bland, 1977) can serve as a fallback: always choose the variable with the smallest index among candidates with negative reduced costs. This is theoretically guaranteed to prevent cycling but is much slower in practice due to poor pivot selection quality. Practical solvers typically rely on perturbation techniques and only fall back to Bland's rule as a last resort.

## Complexity

### Time Complexity

- **Per-perturbation call**: O(C * nnz_avg), where C is the number of pricing candidates and nnz_avg is the average number of nonzeros per constraint row. In the worst case (dense matrix with all variables as candidates), this is O(n * m) where n is the number of variables and m is the number of constraints.

- **Stalling detection** (basis snapshot + diff): O(1) per call (fixed number of integer comparisons and arithmetic operations on summary statistics).

- **Unperturbation**: O(n) for bound restoration (copying saved bounds to working bounds), plus O(k * n) for refinement iterations, where k is a small constant (typically a handful of simplex iterations to clean up infeasibilities from bound restoration).

### Space Complexity

- O(1) additional space beyond the existing solver state. The perturbation procedure uses the saved bound arrays and pricing candidate list that are already allocated. Basis snapshots require O(1) space (a fixed-size vector of summary statistics).

## Edge Cases

### Empty Problem

If the problem has zero variables or zero constraints, the perturbation procedure returns immediately with SUCCESS.

### No Pricing Candidates

If the pricing subsystem returns zero candidates, perturbation has nothing to do and returns SUCCESS. This can occur if the current basis is already optimal (all reduced costs satisfy the optimality conditions).

### All Variables Degenerate

If every pricing candidate is identified as degenerate, the perturbation counter may grow to encompass all candidates. On the next invocation of the pricing subsystem (after refactorization), the candidate set is rebuilt from scratch, potentially re-introducing previously removed variables with updated reduced costs.

### Infeasibility Detection

If the implied bound analysis reveals that a variable's lower bound exceeds its upper bound (even accounting for tolerances), the problem is infeasible. This is a legitimate detection: the implied bounds are derived from the constraint structure, and if they are inconsistent, no feasible solution exists. The perturbation procedure reports INFEASIBLE immediately, and the solver terminates.

### Large Perturbation Counts

If the cumulative perturbation count grows very large relative to the problem size, it may indicate that the problem is severely degenerate or near-infeasible. The outer iteration loop's bounded iteration count provides a safety net in such cases.

### Interaction with Bound Flipping

Bound flipping (a dual simplex technique where a variable's bound status is flipped from lower to upper or vice versa, allowing a long dual step) modifies the constraint sense and row coefficients. Perturbation must be aware that constraint senses may have been altered by previous bound flips. The perturbation procedure reads the current constraint sense array, which reflects any prior bound flips.

## Unperturbation and Solution Refinement

After the perturbed problem reaches an optimal (or satisfactory) solution, the perturbations must be removed and the solution adjusted. This is handled by separate procedures in the simplex solver:

### Bound Restoration

1. Copy the saved lower bounds and saved upper bounds back into the working lower bound and working upper bound arrays, restoring the original problem bounds.

2. This may introduce primal infeasibilities: some basic variables may now violate their restored bounds because the perturbed solution was optimal for the modified bounds, not the originals.

### Solution Refinement

After bound restoration, perform a cleanup pass:

1. **Nonbasic variable cleanup**: For each nonbasic variable with near-zero reduced cost, fix it at the appropriate bound (lower or upper) based on the sign of its reduced cost. This ensures that nonbasic variables satisfy complementary slackness for the original problem.

2. **Basic variable recovery**: For each basic variable that violates its restored bounds, perform a primal pivot to push it back to feasibility. This is a simplified form of Phase I that typically requires only a few iterations.

3. The refinement procedure creates eta vectors for each bound-fixing operation, maintaining the basis inverse in product form.

### Convergence of Refinement

Refinement typically converges within a small number of iterations because:
- The perturbed optimal solution is close to an optimal vertex of the original problem (the perturbations are small).
- Only variables that were at the boundary of their perturbation tolerance need adjustment.
- The basis structure from the perturbed solve provides a warm start that is near-optimal for the original problem.

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

- Bland, R.G. (1977). "New finite pivoting rules for the simplex method." *Mathematics of Operations Research*, 2(2):103-107.
- Dantzig, G.B. (1963). *Linear Programming and Extensions*. Princeton University Press.
- Gill, P.E., Murray, W., Saunders, M.A., and Wright, M.H. (1989). "A practical anti-cycling procedure for linearly constrained optimization." *Mathematical Programming*, 45(1-3):437-474.
- Hall, J.A.J. and McKinnon, K.I.M. (2001). "The simplest examples where the simplex method cycles and conditions where EXPAND fails to prevent cycling." *Mathematical Programming*, 100(1):133-150.
- Harris, P.M.J. (1973). "Pivot selection methods of the Devex LP code." *Mathematical Programming*, 5(1):1-28.
- Maros, I. (2003). *Computational Techniques of the Simplex Method*. Springer.
- Wolfe, P. (1963). "A technique for resolving degeneracy in linear programming." *SIAM Journal on Applied Mathematics*, 11(2):205-211.
