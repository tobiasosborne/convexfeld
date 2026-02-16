# Harris Ratio Test and Bound-Flipping Ratio Test (BFRT)

## Published Reference

- **Harris, P.M.J. (1973).** "Pivot selection methods of the Devex LP code." *Mathematical Programming*, 5(1):1--28. Introduces the relaxed minimum ratio test with tolerance-based candidate selection and largest-pivot tie-breaking.
- **Koberstein, A. (2008).** "Progress in the dual simplex algorithm for solving large scale LP problems: techniques for a fast and stable implementation." *Computational Optimization and Applications*, 41(2):185--204. Describes the integration of Harris' ratio test with bound flipping and cost shifting in the dual simplex.
- **Koberstein, A. (2005).** *The dual simplex method, techniques for a fast and stable implementation.* PhD Thesis, University of Paderborn. Contains detailed treatment of the stabilizing bound flipping ratio test.
- **Pan, P.-Q. (2008).** "A practical method for the bound flipping ratio test." *Optimization Methods and Software*, 23(1):119--131. Presents an efficient implementation strategy for BFRT.
- **Maros, I. (2003).** *Computational Techniques of the Simplex Method.* Springer. Chapter 8: comprehensive treatment of ratio test variants, degeneracy, and numerical stability.
- **Forrest, J.J.H. and Goldfarb, D. (1992).** "Steepest-edge simplex algorithms for linear programming." *Mathematical Programming*, 57(1):341--374. Foundational work on the long-step dual simplex with bound flipping.
- **Dantzig, G.B. (1963).** *Linear Programming and Extensions.* Princeton University Press. Original formulation of the minimum ratio test in the simplex method.

The algorithm specified here is a combination of Harris' two-pass ratio test (Harris, 1973) with the bound-flipping ratio test extension (Forrest and Goldfarb, 1992; Koberstein, 2005). This combination is standard in modern commercial LP solvers.

---

## Purpose

The ratio test determines the **leaving variable** in each iteration of the simplex method. Given an entering variable selected by the pricing procedure, the ratio test computes the maximum step length the solver can take along the entering direction before any basic variable violates its bounds. The leaving variable is the first basic variable to reach a bound.

The standard minimum ratio test can produce numerically poor pivots (very small pivot elements) and is vulnerable to degeneracy cycling. Harris' two-pass ratio test addresses both problems by relaxing the feasibility requirement within a tolerance band and selecting the largest pivot element among near-tied candidates. The bound-flipping ratio test further extends this by allowing variables that reach a bound to "flip" to their opposite bound, enabling longer steps that reduce iteration count on problems with many bounded variables.

This algorithm is invoked during each simplex iteration between the pricing step (which selects the entering variable) and the pivot step (which exchanges basis variables).

---

## Inputs

| Input | Type | Description | Precondition |
|-------|------|-------------|--------------|
| Updated column | sparse vector of doubles | The entering column after FTRAN: d = B^{-1} A_q, where q is the entering variable index | Computed from the current basis factorization |
| Basic variable values | array of doubles | Current values x_B of basic variables | Consistent with the current basis |
| Lower bounds | array of doubles | Lower bound for each variable (may be negative infinity) | Lower bound <= upper bound for every variable |
| Upper bounds | array of doubles | Upper bound for each variable (may be positive infinity) | Upper bound >= lower bound for every variable |
| Variable status | array of status codes | Current status of each variable (basic, at lower bound, at upper bound, superbasic, fixed) | Exactly m variables are basic |
| Feasibility tolerance | positive double | Maximum allowable constraint violation | Typically in the range 1e-9 to 1e-6 |
| Pivot tolerance | positive double | Minimum acceptable absolute value for a pivot element | Typically in the range 1e-10 to 1e-7 |
| Entering variable index | int | Column index of the entering variable selected by pricing | Must reference a valid non-basic variable |
| Entering direction | sign indicator | Whether the entering variable increases (+1) or decreases (-1) from its current bound | Determined by reduced cost sign and current bound status |

---

## Outputs

| Output | Type | Description | Postcondition |
|--------|------|-------------|---------------|
| Leaving variable index | int or sentinel | Row index of the basic variable that limits the step, or a sentinel indicating unbounded/degenerate | If unbounded, the problem has an unbounded ray |
| Step length (theta) | non-negative double | The step size taken along the entering direction | theta >= 0; theta = 0 indicates a degenerate pivot |
| Pivot element | double | The coefficient d_r in the updated column corresponding to the leaving row r | Absolute value exceeds the pivot tolerance |
| Bound-flip set | set of (variable index, new bound value) pairs | Variables that were flipped to their opposite bound during a long step | Each flipped variable was previously at one finite bound and is moved to the other |
| Status | enumeration | One of: NORMAL_PIVOT, DEGENERATE_PIVOT, UNBOUNDED, BOUND_FLIP_ONLY | Indicates what action the caller should take |

---

## Algorithm Description

### Overview

The ratio test proceeds in three conceptual stages. First, the **standard ratio computation** identifies, for each basic variable, the maximum step the entering variable can take before that basic variable hits a bound. Second, the **Harris two-pass filter** relaxes this computation with a tolerance band and selects the pivot element that maximizes numerical stability. Third, the **bound-flipping extension** allows the step to continue past blocking variables by flipping them to their opposite bound, collecting multiple bound transitions into a single iteration.

The two-pass Harris procedure works as follows: in Pass 1, find the maximum step theta_max that keeps all basic variables within a feasibility tolerance of their bounds. In Pass 2, among all basic variables whose ratio is within theta_max, select the one with the largest absolute pivot element. This selects numerically stable pivots while making near-optimal progress.

The bound-flipping extension applies when basic variables have finite bounds on both sides. Instead of stopping at the first blocking variable, the algorithm records that variable as "flipped" to its opposite bound, adjusts the step computation, and continues. Only the final blocking variable (one that cannot flip or has infinite bounds) triggers a real basis exchange. The result is a "long step" that may traverse many breakpoints in a single iteration.

### Detailed Steps

#### Stage 1: Candidate Ratio Computation

For each basic variable x_i with updated column element d_i (the i-th component of the entering column d = B^{-1} A_q):

1. **Skip negligible elements.** If |d_i| < pivot_tolerance, variable i cannot be a leaving candidate. Skip it.

2. **Compute the ratio based on entering direction and coefficient sign.** The entering variable moves in a direction delta determined by its reduced cost sign. The step length theta measures how far the entering variable can move before basic variable i hits a bound:

   - If the entering direction is such that x_i will decrease (d_i and the entering direction have matching sign for decrease), compute:
     - theta_i = (x_i - lower_bound_i) / |d_i|

   - If the entering direction is such that x_i will increase (d_i and the entering direction have matching sign for increase), compute:
     - theta_i = (upper_bound_i - x_i) / |d_i|

   More precisely, let s be +1 if the entering variable increases, -1 if it decreases. Then basic variable i is driven toward its lower bound when s * d_i > 0, and toward its upper bound when s * d_i < 0. The blocking ratio is:

   - If s * d_i > 0: theta_i = (x_i - lb_i) / (s * d_i)
   - If s * d_i < 0: theta_i = (ub_i - x_i) / (-s * d_i)

   In both cases, theta_i >= 0 for a feasible basis.

3. **Handle infinite bounds.** If the relevant bound is infinite (the basic variable cannot block in that direction), the variable does not contribute a finite ratio. Skip it as a leaving candidate in that direction.

4. **Handle equality constraints.** For constraints modeled as equalities, both directions of movement are constrained. The variable contributes ratios in both the increasing and decreasing directions.

#### Stage 2: Harris Two-Pass Selection

**Pass 1 -- Relaxed Minimum Ratio (finding theta_max):**

Compute the relaxed minimum ratio:

    theta_max = min over all candidates i { (slack_i + epsilon) / |d_i| }

where:
- slack_i is the distance from basic variable x_i to its blocking bound (as computed in Stage 1)
- epsilon is the feasibility tolerance
- the minimum is taken only over candidates with |d_i| >= pivot_tolerance

The addition of epsilon allows the step to slightly exceed the point where the first variable reaches its bound. This creates a "tolerance band" of candidate leaving variables, all of which are within epsilon of feasibility violation at step length theta_max.

**Pass 2 -- Largest Pivot Selection:**

Among all candidates i satisfying:

    slack_i / |d_i| <= theta_max

select the candidate r that maximizes |d_r|. That is:

    r = argmax over eligible i { |d_i| }

This two-pass procedure was introduced by Harris (1973) and is standard in commercial LP solver implementations. It provides two benefits:

1. **Numerical stability:** By preferring larger pivot elements, the resulting basis update introduces less numerical error. This is critical because small pivots amplify rounding errors in the basis inverse representation.

2. **Anti-degeneracy:** The tolerance band means that several variables may be "tied" for the minimum ratio. By breaking ties on pivot size rather than index order, the algorithm avoids the cycling that can occur with degenerate pivots.

#### Stage 3: Bound-Flipping Ratio Test (BFRT / Long Step)

The bound-flipping extension applies when the blocking variable r identified in Stage 2 has finite lower and upper bounds (i.e., it can "flip" from one bound to the other). The procedure is:

1. **Initialize.** Set the current step theta to the ratio for the blocking variable r from Stage 2. Initialize the flip set F to empty.

2. **Check if the blocking variable can flip.** Variable r can flip if:
   - It has finite bounds on both sides (lb_r < ub_r, both finite)
   - The bound range (ub_r - lb_r) is not negligibly small

3. **If r can flip:**
   a. Add r to the flip set F with its new bound value (the opposite bound from the one it is approaching).
   b. Adjust the available step: increase theta by the contribution from flipping r. Specifically, the additional step from flipping variable r is:
      - delta_theta = (ub_r - lb_r) / |d_r|
   c. Recompute the next blocking variable among the remaining candidates (those not yet flipped) using the adjusted step.
   d. If a new blocking variable is found, let it become r and repeat from step 2.

4. **If r cannot flip** (one-sided bounds, or the variable is effectively fixed):
   - Variable r is the true leaving variable. Stop.

5. **If no blocking variable remains** (all candidates have been flipped or no finite ratios exist):
   - The step is unbounded in this direction. Either the problem is unbounded (if the objective improves) or the step terminates at the entering variable's opposite bound (if the entering variable itself is bounded).

6. **Apply bound flips.** For each variable in the flip set F:
   a. Set the variable's value to its new bound.
   b. Update the constraint activities to reflect the bound change: for a variable that flips by an amount delta = (new_bound - old_bound), adjust the right-hand side of each constraint containing that variable by subtracting the coefficient times delta.
   c. Negate the relevant row coefficients in the constraint matrix to maintain algebraic consistency when the variable changes its bound direction.
   d. Update the variable's status (from AT_LOWER to AT_UPPER or vice versa).

7. **Record bound flips.** Each bound flip is recorded for later use by the basis reconstruction system. The record includes the variable index, the flip direction, the pivot coefficient, and the adjusted ratio value.

The bound-flipping ratio test was introduced by Forrest and Goldfarb (1992) for the dual simplex and generalized by Koberstein (2005). It can reduce iteration counts by 20--50% on problems with many doubly-bounded variables, because each "long step" iteration accomplishes the work of multiple standard iterations.

### Key Design Choices

- **Tolerance in Pass 1:** The feasibility tolerance epsilon used in the relaxed minimum ratio is the same tolerance used for primal feasibility checking elsewhere in the solver. Using a consistent tolerance avoids situations where the ratio test declares a variable infeasible that the feasibility checker considers feasible, or vice versa. Harris (1973) originally proposed this approach.

- **Pivot size tie-breaking in Pass 2:** Among candidates in the tolerance band, the largest absolute pivot element is preferred. An alternative (used by some solvers) is to prefer the pivot that gives the most progress (smallest ratio). The largest-pivot choice prioritizes numerical stability over greedy objective improvement. Maros (2003, Section 8.3) discusses these trade-offs.

- **Bound-flip ordering:** Variables are flipped in order of increasing breakpoint (ratio value). This corresponds to a natural traversal of the piecewise-linear objective along the entering direction. This ordering is standard in BFRT implementations (Koberstein, 2005).

- **Flip eligibility:** A variable is eligible for flipping only if it has finite bounds on both sides and the bound gap exceeds a small threshold. Variables with infinite bounds or effectively fixed variables (bound gap below a tight tolerance) are never flipped; they become the true leaving variable.

- **Matrix consistency after flip:** When a variable flips bounds, the solver negates the coefficients of the corresponding constraint row and adjusts associated data structures (right-hand side values, dual pricing weights). This maintains the invariant that the constraint matrix encodes the correct relationship between variables and constraints in the current bound configuration.

- **Degenerate step handling:** If the ratio test produces theta = 0 (degenerate pivot), the solver still performs the basis exchange but reports the pivot as degenerate. Accumulated degenerate pivots may trigger perturbation or re-factorization strategies handled at a higher level. The Harris tolerance band reduces (but does not eliminate) the frequency of degenerate pivots.

---

## Numerical Considerations

### Pivot Tolerance

The pivot tolerance is the minimum absolute value of the pivot element |d_r| for a candidate to be considered. This prevents division by near-zero values during the basis update, which would amplify rounding errors. A typical value is 1e-9 to 1e-7. If no candidate meets this threshold, the column is numerically unsuitable for pivoting and should be skipped (the pricing system is asked for an alternative entering variable).

The pivot tolerance may be adaptive: in early iterations a relatively loose tolerance accelerates convergence, while near optimality a tighter tolerance improves solution precision. Maros (2003, Section 8.4) recommends a phased approach:

- **Early phase:** Larger tolerance (e.g., 1e-6), accepting more candidates and enabling faster progress.
- **Middle phase:** Standard tolerance (e.g., 1e-9), balancing speed and accuracy.
- **Late phase (near optimality):** Tight tolerance (e.g., 1e-9 to 1e-10), ensuring final solution accuracy.

### Feasibility Tolerance in Harris Test

The relaxation parameter epsilon in Pass 1 determines how much temporary infeasibility is tolerated. If epsilon is too large, the solver may accept pivots that cause significant infeasibility in basic variables, requiring later correction. If epsilon is too small, the tolerance band collapses and the Harris test degenerates to the standard minimum ratio test, losing its anti-degeneracy and stability benefits.

A practical choice is to use the same epsilon as the solver's primal feasibility tolerance (typically 1e-6 to 1e-9). This ensures that variables declared "feasible" by the ratio test are within the solver's feasibility standard.

### Degeneracy

A degenerate pivot occurs when a basic variable is already at (or very near) its bound, producing a step length theta near zero. Degenerate pivots make no objective progress and can cause the simplex method to cycle indefinitely in exact arithmetic. In floating-point arithmetic, degeneracy manifests as very slow convergence with many near-zero steps.

The Harris tolerance band mitigates degeneracy by allowing the solver to "look past" degenerate candidates and select a pivot with better numerical properties. However, it cannot eliminate degeneracy entirely. Additional techniques applied at higher levels include:

- **Bound perturbation** (Maros, 2003, Section 8.6): slightly perturbing bounds to break exact ties.
- **Bland's rule** (Bland, 1977): selecting the lowest-index candidate as a cycling safeguard.
- **Objective perturbation** (Koberstein, 2005): adding small random perturbations to the cost vector.

### Bound-Flip Numerical Updates

When a variable flips bounds during the BFRT, the constraint matrix row corresponding to the flipping variable's constraint is negated (all coefficients and the right-hand side value have their signs reversed). This is a numerically exact operation (sign change does not introduce rounding error) but it alters the dual pricing weights, which must be swapped and negated as well.

After a sequence of bound flips, the accumulated changes to the right-hand side and pricing weights may introduce numerical drift. Periodic basis refactorization resets this drift by recomputing the basis factorization from scratch.

### Infeasibility Detection

During the ratio test, if a variable's lower bound exceeds its upper bound (plus tolerance), the problem is infeasible at that variable. The solver stores the problematic variable index for diagnostic reporting and terminates the iteration with an infeasibility status.

For bound-flipping, if the adjusted ratio places a variable outside the range [lower_bound - tolerance, upper_bound + tolerance], the flip is classified as infeasible. The solver checks whether this is confirmed by constraint activity bounds (dual information) before declaring infeasibility, since temporary numerical violations may resolve themselves after basis refactorization.

---

## Termination

The ratio test within a single simplex iteration terminates in one of the following ways:

1. **Normal pivot:** A leaving variable r is identified with a finite, positive step length theta and an acceptable pivot element |d_r| >= pivot_tolerance. The simplex iteration proceeds with the basis exchange.

2. **Degenerate pivot:** A leaving variable is identified but the step length theta is zero (or negligibly small). The basis exchange occurs but the objective does not improve. The iteration count increments.

3. **Unbounded detection:** No basic variable blocks the step in the improving direction (all ratios are infinite or no candidates meet the pivot tolerance). The entering variable can improve the objective without limit. The solver reports the problem as unbounded.

4. **Bound-flip only:** All blocking variables have been flipped, and the entering variable reaches its opposite bound without any remaining blocker. No basis exchange occurs, but the objective improves via the bound flips. This is a valid iteration that reduces the iteration count compared to doing the flips one at a time.

5. **No candidates:** If the updated column has no elements exceeding the pivot tolerance (the column is numerically zero), the entering variable is rejected. Control returns to the pricing system to select a different entering variable. This is not a termination of the solver, only of this ratio test attempt.

The simplex method as a whole terminates when no improving entering variable can be found (optimality), when the ratio test detects unboundedness, or when the iteration limit is exceeded.

---

## Complexity

### Per-Iteration Ratio Test

- **Stage 1 (ratio computation):** O(nnz_q) where nnz_q is the number of nonzeros in the entering column after FTRAN. This is a single pass over the nonzero entries.

- **Stage 2 (Harris two-pass):** O(nnz_q) for each pass. Two passes over the candidates, so O(nnz_q) total.

- **Stage 3 (bound flipping):** O(f * nnz_q) in the worst case, where f is the number of variables that flip. In practice, f is typically small relative to the number of constraints. The inner loop rescans the candidates after each flip, but optimized implementations maintain a sorted breakpoint list to avoid rescanning:
  - With sorting: O(nnz_q * log(nnz_q)) to sort breakpoints, then O(f) to traverse.
  - Without sorting: O(f * nnz_q) worst case.

- **Bound-flip updates:** O(nnz_j) per flipped variable j, where nnz_j is the number of nonzeros in column j (the number of constraints affected by the flip).

### Space

- **Working storage:** O(nnz_q) for the breakpoint list and candidate tracking.
- **Flip records:** O(f) for the set of bound-flip records in the current iteration.

---

## Edge Cases

### No Finite Ratios (Unbounded)

If every nonzero in the entering column either falls below the pivot tolerance or corresponds to a basic variable with infinite bounds in the blocking direction, no finite ratio exists. The solver declares the problem unbounded and reports the entering variable as the unbounded ray.

### All Zero Column (Numerically Singular)

If the updated column d = B^{-1} A_q has no entries exceeding the pivot tolerance, the column is numerically zero. This indicates that the entering variable does not appear in the current basis representation. The column is rejected and the pricing system selects an alternative. If this occurs repeatedly, it may indicate numerical difficulties requiring basis refactorization.

### Degenerate Pivot (theta = 0)

A zero step length occurs when a basic variable is already at its bound. The Harris tolerance band helps by creating a small window of candidates around theta = 0, allowing the solver to select the pivot with the largest element. This reduces the probability of choosing a degenerate pivot when a numerically better option is available within the tolerance.

### Single Candidate

If exactly one basic variable qualifies as a leaving candidate (only one nonzero in the column meets the pivot tolerance), it is selected regardless of pivot size. The two-pass procedure degenerates to the standard minimum ratio test. No tie-breaking is needed.

### Nearly Equal Bounds (Tight Bounds)

If a variable's upper bound minus lower bound is smaller than the pivot tolerance, the variable is effectively fixed. Such variables are handled by a separate tight-bound routine rather than the general ratio test. They do not participate as leaving candidates because their "slack" is smaller than numerical precision allows.

### All Variables Flip (Complete Long Step)

In the bound-flipping extension, it is possible that every blocking variable has finite bounds on both sides and every one flips. In this case, no basis exchange occurs. The entering variable proceeds to its opposite bound, and the iteration consists entirely of bound flips. This is valid and may advance the objective significantly.

### Entering Variable Is Free (Unbounded in Both Directions)

If the entering variable has infinite bounds in both directions (a free variable), the ratio test must find a leaving variable regardless of the entering direction. If no leaving variable exists, the problem is unbounded. Free variables cannot participate in bound flipping.

### Infeasible Bounds Detected

If, during the ratio test, the solver encounters a variable whose lower bound exceeds its upper bound by more than the feasibility tolerance, the problem is infeasible at the current state. The solver records the variable for diagnostic purposes and returns an infeasibility status. This check is performed before the ratio computation for each candidate.

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

---

## References

- Bland, R.G. (1977). "New finite pivoting rules for the simplex method." *Mathematics of Operations Research*, 2(2):103--107.
- Dantzig, G.B. (1963). *Linear Programming and Extensions.* Princeton University Press.
- Forrest, J.J.H. and Goldfarb, D. (1992). "Steepest-edge simplex algorithms for linear programming." *Mathematical Programming*, 57(1):341--374.
- Harris, P.M.J. (1973). "Pivot selection methods of the Devex LP code." *Mathematical Programming*, 5(1):1--28.
- Koberstein, A. (2005). *The dual simplex method, techniques for a fast and stable implementation.* PhD Thesis, University of Paderborn.
- Koberstein, A. (2008). "Progress in the dual simplex algorithm for solving large scale LP problems: techniques for a fast and stable implementation." *Computational Optimization and Applications*, 41(2):185--204.
- Maros, I. (2003). *Computational Techniques of the Simplex Method.* Springer. International Series in Operations Research and Management Science, Vol. 61.
- Pan, P.-Q. (2008). "A practical method for the bound flipping ratio test." *Optimization Methods and Software*, 23(1):119--131.
