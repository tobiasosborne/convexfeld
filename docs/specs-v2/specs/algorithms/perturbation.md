# Perturbation and Anti-Cycling

## Published Reference

- Wolfe, P. (1963). "A technique for resolving degeneracy in linear programming." *SIAM Journal on Applied Mathematics*, 11(2):205-211.
- Bland, R.G. (1977). "New finite pivoting rules for the simplex method." *Mathematics of Operations Research*, 2(2):103-107.
- Gill, P.E., Murray, W., Saunders, M.A., and Wright, M.H. (1989). "A practical anti-cycling procedure for linearly constrained optimization." *Mathematical Programming*, 45(1-3):437-474.
- Harris, P.M.J. (1973). "Pivot selection methods of the Devex LP code." *Mathematical Programming*, 5(1):1-28.
- Maros, I. (2003). *Computational Techniques of the Simplex Method*. Springer. Chapters 6-7, Section 10.3.
- Hall, J.A.J. and McKinnon, K.I.M. (2001). "The simplest examples where the simplex method cycles and conditions where EXPAND fails to prevent cycling." *Mathematical Programming*, 100(1):133-150.

The anti-cycling system described here combines two complementary mechanisms:

1. **Pricing restriction** (Maros, 2003, Section 10.3): An implied-bound analysis that identifies and removes irrecoverably degenerate variables from the pricing candidate set. This addresses *entering-side degeneracy*, where multiple candidates have near-zero reduced costs or are structurally forced into degenerate positions.

2. **EXPAND-style bound perturbation** (Gill et al., 1989): A direct modification of the working bounds of basic variables that sit exactly at their bounds, widening them by a small epsilon to ensure that the ratio test produces strictly positive step lengths. This addresses *leaving-side degeneracy*, where basic variables at bounds produce zero-step ratios regardless of the entering variable selected.

Both mechanisms are necessary because they solve different types of degeneracy that arise in practice.

## Purpose

In the simplex method, **degeneracy** occurs when one or more basic variables sit exactly at a bound, so that a simplex pivot produces a zero step length. When the objective value does not change over many pivots, the algorithm is said to be **stalling**. In the worst case, the same sequence of bases repeats indefinitely, a phenomenon called **cycling**. Although cycling is theoretically possible and has been demonstrated on constructed examples, stalling is the far more common practical concern, since it wastes computational effort without progress toward optimality.

### Two Types of Degeneracy

Understanding the two sources of degeneracy is essential for selecting the correct anti-cycling mechanism:

**Entering-side degeneracy.** Multiple non-basic variables have near-zero or structurally constrained reduced costs. The pricing system selects a candidate, but the candidate is itself degenerate — its implied bound range (given the constraint structure) is zero, meaning it cannot produce a non-degenerate pivot regardless of the leaving variable. This is addressed by *pricing restriction*: analyzing the implied bounds of each pricing candidate and removing those that are irrecoverably degenerate from the candidate set.

**Leaving-side degeneracy.** The entering variable has a strong reduced cost (it is the RIGHT variable to enter), but ALL potential leaving variables are exactly at their bounds. The ratio test computes theta = (x_i - lb_i) / |d_i| = 0 for every basic variable i because x_i = lb_i. The pivot changes the basis but the solution point does not move. This is addressed by *bound perturbation*: temporarily widening the bounds of basic variables at their bounds by a small epsilon, so they are no longer exactly at bounds and the ratio test produces nonzero step lengths.

**Why both mechanisms are needed.** Pricing restriction (entering-side) cannot fix leaving-side degeneracy: removing entering candidates does not change the fact that all leaving candidates have ratio = 0. Conversely, bound perturbation (leaving-side) is unnecessary when the degeneracy is purely on the entering side — it introduces numerical noise without benefit. A complete anti-cycling system must address both types.

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
  - Perturbation counter (cumulative count of variables perturbed or bounds widened)
  - Perturbation active flag (indicates whether EXPAND perturbation is currently in effect)
  - Work counter (performance profiling accumulator)

- **Environment**: Solver parameters, including:
  - Optimality tolerance (typically 1e-6 to 1e-8)
  - Primal feasibility tolerance (typically 1e-6)
  - Perturbation magnitude parameter (controls EXPAND epsilon sizing)
  - Verbosity level

- **Preconditions**:
  - The solver is in a valid state with a complete basis (exactly numConstrs basic variables).
  - Saved bounds contain the original unperturbed problem bounds.
  - Working bounds may already reflect prior perturbations.
  - Stalling has been detected by the caller (see Section: Stalling Detection).

## Outputs

- **Modified SolverState**:
  - Some variables removed from the pricing candidate set (entering-side mechanism).
  - Working bounds widened for basic variables at bounds (leaving-side mechanism).
  - Variable statuses updated for removed variables.
  - Perturbation counter incremented.
  - Perturbation active flag set (if EXPAND widening was applied).
  - Work counter updated to reflect computational effort.

- **Status code**:
  - SUCCESS: Perturbation applied, solver should continue iterating.
  - INFEASIBLE: A bound violation was detected that cannot be resolved by perturbation (lower bound exceeds upper bound at some variable, indicating the original problem is infeasible).

- **Postconditions**:
  - All remaining pricing candidates have implied bound gaps large enough to permit non-degenerate pivots (entering-side guarantee).
  - Basic variables that were at bounds have had their working bounds widened, ensuring the ratio test produces nonzero step lengths (leaving-side guarantee).
  - Variables removed from pricing will not be selected as entering variables until the pricing state is rebuilt (typically at the next refactorization).

## Algorithm Description

### Overview

The anti-cycling system operates at three levels within the simplex iteration loop:

1. **Stalling detection** determines when the simplex algorithm is making insufficient progress, by comparing basis snapshots taken at intervals.

2. **Anti-cycling perturbation** (the core of this specification) applies two complementary mechanisms:
   - **Mechanism A (pricing restriction):** Analyze each pricing candidate via implied bound analysis; remove those that are irrecoverably degenerate from the pricing set. This addresses entering-side degeneracy.
   - **Mechanism B (EXPAND bound widening):** When pricing restriction alone is insufficient (i.e., stalling persists after candidate removal, or leaving-side degeneracy is detected), widen the working bounds of basic variables at their bounds by a small epsilon. This addresses leaving-side degeneracy.

3. **Unperturbation and refinement** restores the original bounds after the perturbed problem is solved and performs cleanup iterations to recover a feasible solution to the original problem.

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

#### Phase 3: Bound Restoration (Diagnostic Mode)

When operating in a detailed diagnostic mode, copy the saved (original) bounds into the working bound arrays. This ensures that the perturbation analysis is performed against the unperturbed problem, preventing accumulation of perturbation drift. After copying, recompute derived quantities (such as constraint activities) that depend on the bounds.

This step is conditional on a verbosity or diagnostic parameter. In standard mode, the perturbation analysis operates on the current working bounds directly.

#### Phase 4: Mechanism A — Pricing Restriction (Entering-Side)

Process each pricing candidate. The behavior depends on the variable's status:

**Case A: Non-basic variable at lower bound (status = AT_LOWER)**

1. Examine the variable's reduced cost d_j.
2. If d_j is strictly less than the negative of the primal feasibility tolerance (d_j < -epsilon_p), this indicates a severe bound violation. If the variable participates in an equality constraint and the reduced cost is large in the opposite direction, report INFEASIBLE.
3. Otherwise, mark the variable as removed from pricing (set status to a non-candidate value) and increment the perturbation counter.

This case handles variables that are sitting at their lower bounds but cannot improve the objective -- they are degenerate entering candidates that should not be selected for pivoting.

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

#### Phase 5: Mechanism B — EXPAND Bound Widening (Leaving-Side)

When stalling persists after pricing restriction (detected by a secondary stalling check or by the outer iteration loop re-entering the perturbation procedure), apply EXPAND-style bound perturbation to the working bounds:

1. **Identify basic variables at bounds.** For each basic variable x_{beta_i}, check whether its current value is at (or within a tight tolerance of) one of its bounds:
   - At lower bound: x_{beta_i} <= lb_{beta_i} + epsilon_feas
   - At upper bound: x_{beta_i} >= ub_{beta_i} - epsilon_feas

2. **Widen the working bounds.** For each basic variable at a bound, perturb the relevant working bound:
   - At lower bound: lb_work_{beta_i} = lb_saved_{beta_i} - epsilon_i
   - At upper bound: ub_work_{beta_i} = ub_saved_{beta_i} + epsilon_i

   where epsilon_i is a small variable-dependent perturbation. The perturbation magnitude is computed as:

   epsilon_i = epsilon_base * (1 + |bound_value|) * (1 + hash(i))

   where:
   - epsilon_base is typically on the order of 1e-6 to 1e-8 (scaled from the feasibility tolerance)
   - The (1 + |bound_value|) factor provides relative scaling so that variables with large bound values receive proportionally larger perturbations
   - The hash(i) factor (a deterministic function of the variable index, producing values in [0, 1)) ensures that each variable receives a distinct perturbation, preventing the creation of new degenerate ties

   This is the EXPAND procedure of Gill et al. (1989). The use of distinct perturbations per variable is the key insight from Wolfe (1963): if all perturbations are distinct, the perturbed polyhedron is non-degenerate with probability 1.

3. **Set the perturbation active flag.** Record that EXPAND perturbation has been applied, so that unperturbation will be triggered when the perturbed problem reaches optimality.

4. **Update constraint activities.** Recompute constraint activity bounds to reflect the modified working bounds (via the activity bound computation function, P3.21 cxf_simplex_setup).

After bound widening, the ratio test will produce nonzero step lengths because no basic variable is exactly at its bound in the working problem. The simplex method can then make genuine progress toward the (perturbed) optimum.

#### Phase 6: Counter Update

After processing all candidates:
1. Add the count of newly perturbed/removed variables to the cumulative perturbation counter in the solver state.
2. Update the work counter to reflect the number of candidates processed.

### When to Apply Each Mechanism

The two mechanisms are applied in a priority order:

| Condition | Mechanism Applied |
|-----------|-------------------|
| First stalling detection | Mechanism A (pricing restriction) only |
| Stalling persists after pricing restriction | Mechanism A + Mechanism B (EXPAND widening) |
| Proactive early-iteration perturbation | Mechanism A only |
| Phase I with many basic variables at bounds | Mechanism A + Mechanism B |

The rationale for this priority is efficiency: pricing restriction is cheaper (O(C * nnz_avg) vs. O(n) for bound widening) and avoids introducing numerical noise. Bound widening is reserved for cases where pricing restriction alone is insufficient.

### Key Design Choices

- **Two-mechanism approach**: The combination of pricing restriction and EXPAND bound widening addresses both entering-side and leaving-side degeneracy. This is more robust than either mechanism alone. Pricing restriction alone fails on leaving-side degeneracy (as demonstrated by problems like stair, finnis, capri, and tuff from the Netlib test set, where 85%+ of iterations have zero-length steps due to basic variables at bounds). EXPAND alone is overkill for entering-side degeneracy, where targeted candidate removal is more efficient.

- **Implied bound analysis for pricing restriction**: Rather than applying random perturbations to all variable bounds (as in the classical method of Wolfe, 1963), the pricing restriction mechanism uses the constraint structure to identify which variables are truly degenerate. This is more targeted and avoids perturbing variables that are already non-degenerate.

- **Saved vs. working bounds**: Maintaining two copies of the bounds (saved/original and working) is essential for the EXPAND mechanism (Gill et al., 1989). The saved bounds represent the original problem; the working bounds are the (possibly perturbed) bounds used during iteration. Perturbation analysis uses the saved bounds to avoid compounding perturbation errors across multiple invocations.

- **Variable-dependent perturbation magnitudes**: Using distinct perturbations per variable (via the hash function) ensures that the perturbed polyhedron is generically non-degenerate. Equal perturbations could create new degenerate configurations (Wolfe, 1963).

- **Proactive perturbation in early iterations**: The main iteration loop applies pricing restriction proactively during the first one or two inner iterations (before stalling is detected), as a preventive measure. This reflects the practical observation that degeneracy is most likely to cause problems at the start of the solve, when the initial basis may contain many degenerate variables (especially after crash basis construction).

- **Grace period before stalling diagnosis**: The basis snapshot comparison uses a grace period (typically 5 inner iterations) before declaring stalling. This avoids false positives during the initial phase when the solver is establishing its working basis.

## Numerical Considerations

### Tolerances

| Tolerance | Role | Typical Range |
|-----------|------|---------------|
| Optimality tolerance | Determines whether a reduced cost is significant enough to indicate potential improvement | 1e-6 to 1e-8 |
| Primal feasibility tolerance | Threshold for accepting bound violations as tolerable | 1e-6 |
| Minimum bound range | Floor on the implied bound gap, prevents collapse to exact degeneracy | ~1e-10 |
| Maximum perturbation magnitude | Ceiling on the perturbation size for pricing restriction, prevents large distortions | ~1e-6 |
| EXPAND epsilon base | Base magnitude for EXPAND bound widening | ~1e-8 to 1e-6 |

### Stability Concerns

- **Perturbation magnitude selection (pricing restriction)**: The perturbation magnitude is clamped between the minimum bound range and the maximum perturbation. If the implied bound gap is smaller than the minimum bound range, the minimum is used; if larger than the maximum perturbation, the maximum is used. This two-sided clamping ensures that perturbations are always large enough to break degeneracy but small enough to maintain solution quality.

- **EXPAND epsilon sizing**: The EXPAND perturbation magnitude must be small enough that the perturbed problem's optimal solution is close to the original problem's optimal solution (ensuring rapid convergence of the post-perturbation refinement), but large enough to break all degenerate ties. The relative scaling (1 + |bound_value|) prevents the perturbation from being negligible for variables with large bound values.

- **Implied bound accumulation errors**: When computing implied bounds by summing products of coefficients and saved bounds across a constraint row, floating-point rounding errors can accumulate, especially for rows with many nonzeros or with coefficients of widely varying magnitude. The algorithm uses standard double-precision arithmetic without compensated summation in this phase, relying on the tolerances to absorb rounding errors.

- **Interaction with saved bounds**: The use of saved (original) bounds for the implied bound computation prevents perturbation drift. Even if the working bounds have been modified by previous EXPAND calls, the implied bound analysis always starts from the original problem data.

### Degeneracy Handling

The entire purpose of this algorithm is degeneracy handling. The two mechanisms target different structural causes:

- **Pricing restriction** addresses degeneracy at the vertex level: when multiple constraints are active simultaneously at the current basic feasible solution, some entering candidates are structurally incapable of producing non-degenerate pivots. Removing them narrows the candidate set to variables that can make genuine progress.

- **EXPAND bound widening** addresses degeneracy at the ratio test level: when basic variables sit exactly at their bounds, the ratio test computes theta = 0 regardless of the entering variable. Widening the bounds moves basic variables off their bounds, ensuring theta > 0. This is the mechanism that Gill et al. (1989) actually describes: "the procedure ensures that every nondegenerate subproblem is solved by a sequence of nondegenerate steps."

### Phase I Degeneracy

Phase I (finding an initial feasible solution, see P2.9) is particularly susceptible to leaving-side degeneracy because:

1. The crash basis (P2.5) typically assigns slack variables at their lower bounds (zero), creating many basic variables exactly at bounds.
2. The sum-of-infeasibilities objective can create artificial degeneracy when multiple constraints are violated by equal amounts.
3. The Phase I objective coefficients (+1/-1/0 for basic variables) create many tied reduced costs.

The EXPAND mechanism is especially important during Phase I. Without it, the solver can stall indefinitely on highly degenerate problems where all potential leaving variables have zero ratios. The pricing restriction mechanism alone cannot resolve this because the entering variable has a strong reduced cost (it IS the right variable), but the step is zero because the leaving side is degenerate.

## Termination

### Perturbation Step Termination

The perturbation procedure itself terminates in O(C * R) time where C is the number of pricing candidates and R is the average number of nonzeros per row, plus O(m) for EXPAND bound widening when applied.

### Solver-Level Termination

With both anti-cycling mechanisms active, the simplex algorithm is expected to terminate because:

1. **Pricing restriction** removes at least one degenerate variable from the pricing set per invocation (or detects infeasibility). The number of variables is finite, so the number of pricing restriction calls is bounded.

2. **EXPAND bound widening** ensures that all subsequent ratio tests produce strictly positive step lengths (assuming exact arithmetic). Each pivot strictly improves the perturbed objective, and since the number of bases is finite, the perturbed problem terminates.

3. **The basis snapshot comparison** in the outer loop ensures that if the solver makes no progress across an entire outer iteration (including both perturbation mechanisms), the outer loop exits. The outer iteration count is bounded by a configurable limit (default 100 for dual simplex, 10 during crossover, 5 for primal simplex).

4. **Fallback**: If perturbation fails to resolve stalling (the EXPAND approach is not guaranteed to prevent cycling in all cases, as shown by Hall and McKinnon, 2001), the solver relies on the outer iteration limit and may report a suboptimal or numerically difficult status.

For guaranteed finiteness, Bland's rule (Bland, 1977) can serve as a fallback: always choose the variable with the smallest index among candidates with negative reduced costs. This is theoretically guaranteed to prevent cycling but is much slower in practice due to poor pivot selection quality. Practical solvers typically rely on perturbation techniques and only fall back to Bland's rule as a last resort.

## Complexity

### Time Complexity

- **Pricing restriction (Mechanism A)**: O(C * nnz_avg), where C is the number of pricing candidates and nnz_avg is the average number of nonzeros per constraint row. In the worst case (dense matrix with all variables as candidates), this is O(n * m) where n is the number of variables and m is the number of constraints.

- **EXPAND bound widening (Mechanism B)**: O(m) per invocation (one pass over all basic variables to identify those at bounds and widen their bounds). Activity bound recomputation adds O(nnz) in the worst case.

- **Stalling detection** (basis snapshot + diff): O(1) per call (fixed number of integer comparisons and arithmetic operations on summary statistics).

- **Unperturbation**: O(n) for bound restoration (copying saved bounds to working bounds), plus O(k * per_iteration_cost) for refinement iterations, where k is a small constant (typically a handful of simplex iterations to clean up infeasibilities from bound restoration).

### Space Complexity

- O(1) additional space beyond the existing solver state. The perturbation procedure uses the saved bound arrays and pricing candidate list that are already allocated. Basis snapshots require O(1) space (a fixed-size vector of summary statistics). The EXPAND perturbation modifies working bounds in place and requires no additional arrays.

## Edge Cases

### Empty Problem

If the problem has zero variables or zero constraints, the perturbation procedure returns immediately with SUCCESS.

### No Pricing Candidates

If the pricing subsystem returns zero candidates, perturbation has nothing to do and returns SUCCESS. This can occur if the current basis is already optimal (all reduced costs satisfy the optimality conditions).

### All Variables Degenerate (Entering-Side)

If every pricing candidate is identified as degenerate by Mechanism A, the perturbation counter may grow to encompass all candidates. On the next invocation of the pricing subsystem (after refactorization), the candidate set is rebuilt from scratch, potentially re-introducing previously removed variables with updated reduced costs. If stalling persists after candidate set rebuilding, Mechanism B (EXPAND) is triggered.

### All Basic Variables at Bounds (Leaving-Side)

This is the case where Mechanism B is essential. When every basic variable is exactly at one of its bounds, every ratio test produces theta = 0. EXPAND bound widening moves all basic variables off their bounds, breaking the universal leaving-side degeneracy. This situation is common during Phase I and on highly constrained problems.

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

2. Clear the perturbation active flag.

3. This may introduce primal infeasibilities: some basic variables may now violate their restored bounds because the perturbed solution was optimal for the modified bounds, not the originals.

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

---

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
