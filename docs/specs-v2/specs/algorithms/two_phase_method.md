# Two-Phase Simplex Method

## Published Reference

- **Primary:** Dantzig, G.B. (1963). *Linear Programming and Extensions*. Princeton University Press. Chapters 5--7 (two-phase method), Chapter 9 (infeasibility detection).
- **Computational details:** Maros, I. (2003). *Computational Techniques of the Simplex Method*. Springer. Chapter 6 (Phase I), Chapter 10 (degeneracy and cycling in Phase I).
- **Textbook treatment:** Chvatal, V. (1983). *Linear Programming*. W.H. Freeman. Chapter 3 (two-phase method).
- **Modern reference:** Vanderbei, R.J. (2014). *Linear Programming: Foundations and Extensions*. 4th ed. Springer. Chapters 5--6 (Phase I and the two-phase method).
- **Alternative (Big-M) comparison:** Bertsimas, D. and Tsitsiklis, J.N. (1997). *Introduction to Linear Programming*. Athena Scientific. Chapter 2.
- **Crash basis interaction:** Gould, N.I.M. and Reid, J.K. (1989). "New crash procedures for large systems of linear constraints." *Mathematical Programming*, 45:475--501.
- **Phase I pricing considerations:** Forrest, J.J.H. and Goldfarb, D. (1992). "Steepest-edge simplex algorithms for linear programming." *Mathematical Programming*, 57(1):341--374.

---

## Purpose

The two-phase method is the standard technique for solving linear programs that may lack an obvious initial basic feasible solution. It separates the optimization process into two distinct phases:

- **Phase I** finds a basic feasible solution (BFS) by minimizing a measure of total constraint violation.
- **Phase II** minimizes the original objective function c^T x over the feasible region, starting from the BFS found by Phase I.

This specification describes the complete Phase I algorithm: how the solver determines that Phase I is needed, how the Phase I objective is constructed, how Phase I iterations proceed, how Phase I termination and infeasibility detection work, and how the transition to Phase II is orchestrated. It complements the revised simplex method specification (P2.1), which describes the iteration mechanics shared by both phases, and the simplex phases module specification (P3.21), which describes the `phase_end` function that monitors the Phase I → Phase II transition.

---

## Inputs

| Input | Type | Description | Preconditions |
|-------|------|-------------|---------------|
| A | Sparse matrix (m x n) | Constraint coefficient matrix (CSR and CSC) | No NaN or infinity; dimensions consistent |
| c | Dense vector (n) | Original objective function coefficients | Finite entries |
| b | Dense vector (m) | Right-hand side values | Finite entries |
| l | Dense vector (n) | Variable lower bounds | l_j <= u_j for all j; may be -infinity |
| u | Dense vector (n) | Variable upper bounds | u_j >= l_j for all j; may be +infinity |
| B_0 | Basis (m indices) | Initial basis from crash or warm start | Must index m linearly independent columns |
| tolerances | Parameter set | Feasibility tolerance (epsilon_feas), optimality tolerance (epsilon_opt), pivot tolerance (epsilon_piv) | All strictly positive |

**Additional preconditions:**
- The constraint matrix has been converted to standard form: Ax = b, l <= x <= u, where slack variables have been added for inequality constraints.
- A crash basis procedure (P2.5) has been executed, providing B_0 as the initial basis.
- The basis factorization of B_0 is available and numerically valid.

---

## Outputs

| Output | Type | Description |
|--------|------|-------------|
| x_BFS | Dense vector (n) | Basic feasible solution (if Phase I succeeds) |
| B_feas | Basis (m indices) | Feasible basis at the Phase I solution |
| status | Enum | FEASIBLE (Phase I succeeded, transition to Phase II), INFEASIBLE (no feasible solution exists), ITERATION_LIMIT, or NUMERIC_DIFFICULTY |

**Postconditions on FEASIBLE status:**
- All basic variables satisfy their bound constraints: l_{beta_i} <= x_{beta_i} <= u_{beta_i} within epsilon_feas for all i.
- The Phase I objective value is zero within tolerance.
- The basis B_feas is nonsingular and has a valid factorization.
- The solver is ready to begin Phase II with the original objective c.

---

## Algorithm Description

### Overview

The two-phase method operates on the observation that the revised simplex method (P2.1) can minimize any linear objective function over a polyhedral feasible region. Phase I exploits this by constructing a surrogate objective function that measures infeasibility, then applying the simplex method to minimize it. If the minimum of the surrogate objective is zero, a feasible solution has been found and the solver transitions to Phase II. If the minimum is strictly positive, the original problem is infeasible.

The key advantage of the two-phase method over the Big-M method is numerical stability: the two-phase method uses separate objective functions of comparable magnitude for each phase, while the Big-M method combines the feasibility penalty and the original objective into a single function with a very large coefficient M, which can cause catastrophic cancellation and ill-conditioning (Dantzig, 1963, Chapter 7; Maros, 2003, Section 6.3).

### Phase I Determination

After the crash basis procedure (P2.5) constructs the initial basis B_0, the solver computes the initial basic variable values:

> x_B = B_0^{-1} b

Phase I is required when the initial basic variables violate their bound constraints. The determination proceeds as follows:

**Step 1: Bound violation scan.** For each basic variable x_{beta_i}:
- If x_{beta_i} < l_{beta_i} - epsilon_feas: the variable violates its lower bound.
- If x_{beta_i} > u_{beta_i} + epsilon_feas: the variable violates its upper bound.

**Step 2: Phase selection.**
- If no basic variable violates its bounds, the initial basis is primal feasible. The solver enters Phase II directly with the original objective c.
- If one or more basic variables violate their bounds, the solver enters Phase I with a surrogate objective designed to drive these violations to zero.

In practice, the crash basis (P2.5) often produces a feasible or nearly feasible starting point, so Phase I may require only a small number of iterations (or may be skipped entirely). The crash procedure performs early infeasibility detection: if a constraint's RHS is inconsistent with the constraint sense (e.g., an equality constraint with a nonzero RHS in a slack basis), the crash returns an infeasibility code before Phase I even begins.

### Phase I Objective Construction

The Phase I objective measures the total constraint violation of the current basis. It is constructed as a surrogate objective that penalizes basic variables that are outside their bounds.

**Sum-of-infeasibilities formulation.** The Phase I objective is:

> minimize w(x) = sum over all infeasible basic variables: violation_i

where violation_i measures the amount by which basic variable x_{beta_i} exceeds its bound:

- If x_{beta_i} < l_{beta_i}: violation_i = l_{beta_i} - x_{beta_i} (shortfall below lower bound)
- If x_{beta_i} > u_{beta_i}: violation_i = x_{beta_i} - u_{beta_i} (excess above upper bound)
- If l_{beta_i} <= x_{beta_i} <= u_{beta_i}: violation_i = 0 (feasible, no penalty)

This is equivalent to minimizing the L1-norm of the constraint violations. The Phase I objective w(x) is always non-negative, and w(x) = 0 if and only if x is primal feasible.

**Implementation via modified reduced costs.** In practice, the Phase I objective is not stored as a separate objective vector. Instead, the solver modifies the reduced costs used by the pricing system to reflect the Phase I objective rather than the original objective c. The Phase I reduced costs are computed as:

> d_j^{(I)} = w_j - w_B^T B^{-1} a_j

where w_j are the Phase I objective coefficients for non-basic variable j, and w_B are the Phase I objective coefficients for the basic variables. The Phase I objective coefficients are:

- For basic variables at or below their lower bound: w_{beta_i} = -1 (penalizes the shortfall)
- For basic variables at or above their upper bound: w_{beta_i} = +1 (penalizes the excess)
- For feasible basic variables: w_{beta_i} = 0 (no penalty)
- For non-basic variables: w_j = 0 (non-basic variables are at their bounds by definition)

These coefficients are updated dynamically as the basis changes: when a basic variable becomes feasible (crosses back within its bounds during a pivot), its Phase I coefficient changes from +/-1 to 0. When a newly basic variable violates a bound, its coefficient changes from 0 to +/-1.

### Phase I Iteration

Phase I iterations use the same revised simplex machinery described in P2.1 (pricing, FTRAN, ratio test, pivot, BTRAN, weight update). The differences from Phase II are:

**1. Objective function.** The pricing system uses the Phase I reduced costs d^{(I)} rather than the original reduced costs d = c - c_B^T B^{-1} A. This means the entering variable is selected to improve feasibility (reduce the sum of violations) rather than to improve the original objective.

**2. Pricing strategy.** During Phase I, the Devex approximate pricing method (Harris, 1973) may be preferred over exact steepest edge pricing (Forrest and Goldfarb, 1992). The rationale is that the Phase I objective is artificial — it will be discarded at the phase transition — so the extra computational cost of maintaining exact steepest edge weights provides less benefit than in Phase II. The pricing tolerance during Phase I is typically set at the "initial" (loose) level (see P2.3, multi-level tolerance), allowing rapid early progress.

**3. Optimality criterion.** The Phase I optimality criterion is the same as Phase II: no non-basic variable has a reduced cost that would allow it to enter the basis and improve the objective. However, the interpretation differs:
- In Phase II, optimality means the original objective c^T x cannot be improved.
- In Phase I, optimality means the infeasibility measure w(x) cannot be reduced.

**4. Bound propagation.** The bidirectional bound propagation system (cxf_simplex_step2 and cxf_simplex_step3, P3.20) operates during Phase I iterations, tightening variable bounds based on constraint activity. This propagation can detect infeasibility (when implied bounds conflict with existing bounds) and can reduce the effective problem size, accelerating both Phase I and subsequent Phase II iterations.

**5. Anti-cycling.** Degeneracy is common during Phase I because many basic variables may be at their bounds simultaneously (especially early in Phase I, when the crash basis places most slacks at their lower bounds). The perturbation-based anti-cycling mechanism (P2.6) is active during Phase I to prevent cycling.

### Phase I Termination

Phase I terminates in one of three outcomes:

**Outcome 1: Feasibility achieved (w* = 0).** If the Phase I optimal objective value is zero within tolerance (all basic variables satisfy their bound constraints within epsilon_feas), a basic feasible solution has been found. The solver transitions to Phase II.

**Outcome 2: Infeasibility proved (w* > 0).** If the Phase I optimal objective value is strictly positive (the simplex method cannot reduce the infeasibility measure below a positive value), the original problem has no feasible solution. The function returns INFEASIBLE with diagnostic information identifying the conflicting constraints.

Infeasibility determination in Phase I requires careful numerical handling:
- The Phase I objective is compared against epsilon_feas (not zero) to account for floating-point rounding.
- If the Phase I objective is positive but very small (below a multiple of the feasibility tolerance), the solver may attempt additional iterations with tighter tolerances before declaring infeasibility, to distinguish genuine infeasibility from numerical artifact.
- The diagnostic information includes the index of the constraint or variable responsible for the residual infeasibility, enabling the caller to report which constraints are in conflict.

**Outcome 3: Limits reached.** If Phase I reaches an iteration limit or time limit without achieving feasibility, the solver returns the corresponding limit status.

### Infeasibility Detection During Phase I

Infeasibility can be detected at multiple points during Phase I:

**1. Crash-time detection.** The crash basis procedure (P2.5) detects trivial infeasibilities before Phase I iterations begin. For example, an equality constraint with a nonzero RHS (when the slack basis sets all variables to zero) is immediately identified as infeasible if |b_i| >= epsilon_feas. Similarly, an inequality constraint with a negative RHS (b_i < -epsilon_feas) indicates infeasibility in the slack basis.

**2. Bound violation detection.** During Phase I iterations, the step function (P3.20, cxf_simplex_step) checks for inconsistent bounds (lower bound exceeding upper bound beyond tolerance). If detected, the function returns an infeasibility code.

**3. Dual infeasibility of free variables.** The phase_end function (P3.21, cxf_simplex_phase_end) checks free variables for reduced cost violations. A free variable with a reduced cost indicating dual infeasibility under the Phase I objective signals that the Phase I objective cannot be driven to zero, confirming problem infeasibility.

**4. Bound propagation detection.** The bidirectional bound propagation (cxf_simplex_step2 and cxf_simplex_step3) can detect infeasibility when implied bounds from constraint activities conflict with existing variable bounds. This uses a two-stage confirmation procedure: the initial bound conflict is verified against dual activity bounds before the infeasibility code is returned, preventing false alarms from numerical noise.

**5. Phase I optimality with positive objective.** When the pricing system finds no improving candidate (all Phase I reduced costs satisfy the optimality condition) but the Phase I objective remains positive, the problem is infeasible. This is the definitive infeasibility determination.

### Phase I → Phase II Transition

When Phase I achieves feasibility (w* = 0), the solver performs the following state transformations to prepare for Phase II. These transformations are coordinated between the phase_end function (P3.21) and the orchestration layer (cxf_solve_lp, P3.25).

**Transition Step 1: Objective function swap.** The Phase I surrogate objective (which penalizes infeasibility) is replaced by the original problem objective c. This is the defining characteristic of the two-phase method: the objective function changes at the phase boundary while the basis and variable values are preserved. This clean separation avoids the numerical conditioning problems of the Big-M method (Dantzig, 1963, Chapter 7; Maros, 2003, Section 6.3).

**Transition Step 2: Reduced cost recomputation.** All reduced costs are recomputed from scratch using the original objective:

> d_N = c_N - N^T (B^{-T} c_B)

where B is the current basis matrix, c_B are the objective coefficients for basic variables, and N is the matrix of non-basic columns. This full recomputation is necessary because the Phase I reduced costs are relative to the surrogate objective and are meaningless under the original objective.

**Transition Step 3: Pricing state reset.** The pricing subsystem's candidate sets and tolerance levels are reinitialized to reflect the new objective landscape. Variables that were unattractive under the Phase I objective may now have significant reduced costs under the original objective, and vice versa. The multi-level pricing tolerance (P2.3) typically resets to its initial (loose) level to allow rapid early progress in Phase II.

**Transition Step 4: Constraint cleanup.** Inactive constraints identified during Phase I processing — those whose activity bounds indicate they are not binding at the current solution — are removed from the active set. This sparse removal reduces the effective problem size entering Phase II. The phase_end function (P3.21) handles this cleanup.

**Transition Step 5: Basis preservation.** The basis itself (the set of basic variables and their positions) is carried forward from Phase I to Phase II unchanged. The Phase I solution is a basic feasible solution, and Phase II begins from this vertex of the feasible polyhedron. The basis factorization may be refreshed (via cxf_basis_refactor, P3.16) to ensure numerical accuracy for Phase II iterations, since the objective change can affect the conditioning of subsequent operations.

**Transition Step 6: Tolerance adjustment.** The optimality tolerance used for Phase II termination may differ from the feasibility tolerance used for Phase I. Phase I uses the primal feasibility tolerance to determine when constraint violations are acceptable; Phase II uses the dual feasibility (optimality) tolerance to determine when reduced costs are small enough to declare optimality. These are typically configured as separate environment parameters.

---

## Key Design Choices

### Two-Phase Method vs. Big-M Method

The two-phase method (Dantzig, 1963) is used rather than the Big-M method. In the Big-M method, artificial variables are added with a very large penalty coefficient M in the original objective, converting the problem into a single-phase solve. While simpler to implement, the Big-M method has significant numerical disadvantages:

- The choice of M is critical: too small, and artificial variables may remain in the basis at optimality; too large, and the coefficient range becomes extreme, causing numerical instability.
- With a large M, the LP matrix has a coefficient range of approximately max|c_j| / M, which can cause catastrophic cancellation in floating-point arithmetic.
- The Big-M method conflates feasibility and optimality in a single objective, making it harder to diagnose infeasibility separately from suboptimality.

The two-phase method avoids all of these issues by maintaining separate objectives of comparable magnitude for each phase. The cost is the need for an explicit phase transition, which involves recomputing reduced costs and resetting pricing state — a one-time O(m + nnz) operation that is negligible compared to the total iteration cost.

### Implicit vs. Explicit Artificial Variables

In the classical textbook presentation (Dantzig, 1963; Chvatal, 1983), Phase I introduces explicit artificial variables x_a with the objective "minimize sum(x_a)". In this implementation, Phase I instead works with the existing problem variables and a modified objective that penalizes bound violations. This is equivalent to the classical approach but avoids:

- Expanding the constraint matrix with additional columns for artificial variables
- The memory cost of storing artificial variable data
- The need to remove artificial variables from the basis at the Phase I → Phase II transition

The bound-violation-based approach is standard in modern production simplex solvers (Maros, 2003, Section 6.3). It represents Phase I infeasibility through the existing variable bounds and reduced costs rather than through explicit artificial variables.

### Crash Basis as Phase I Accelerator

The crash basis procedure (P2.5) serves as a Phase I accelerator by:

1. **Detecting trivial infeasibility** before any Phase I iteration, avoiding wasted work.
2. **Constructing a good starting basis** that may already be feasible or nearly feasible, reducing the number of Phase I iterations required.
3. **Removing sparse inactive constraints** from the initial basis, simplifying the problem structure.

On well-conditioned problems with a good crash basis, Phase I may require zero iterations (the crash basis is already feasible). On poorly conditioned or highly constrained problems, Phase I may require up to O(m) iterations in the worst case.

---

## Numerical Considerations

### Tolerances in Phase I

| Tolerance | Role in Phase I |
|-----------|-----------------|
| Feasibility tolerance (epsilon_feas) | Threshold for declaring a basic variable "at its bound" vs. "violating its bound." Determines when Phase I is needed and when it succeeds. |
| Optimality tolerance (epsilon_opt) | Used in pricing to determine when a reduced cost is "small enough" to declare Phase I optimality. |
| Pivot tolerance (epsilon_piv) | Minimum acceptable pivot element magnitude, same as Phase II. |
| Pricing tolerance | Multi-level tolerance (P2.3) controlling pricing candidate acceptance. Typically at the loose (initial) level during Phase I. |

### Numerical Challenges Specific to Phase I

1. **Near-feasible starting points.** When the initial basis is nearly feasible (many variables violating bounds by only a small amount), the Phase I objective is small and may be comparable to floating-point rounding errors. The solver must distinguish genuine infeasibility from numerical noise. The two-stage infeasibility confirmation procedure (cross-checking bound violations against dual activity bounds) prevents false infeasibility declarations.

2. **Phase I objective computation.** The Phase I objective value w(x) is computed as the sum of individual violations. When many small violations are summed, catastrophic cancellation can occur. The solver uses the same numerically stable addition technique (conservative rounding with compensated summation) described in P2.1 for activity bound computation.

3. **Phase transition precision.** At the Phase I → Phase II transition, the Phase I objective should be exactly zero for a truly feasible solution. In practice, floating-point arithmetic means the objective is only approximately zero. The transition is triggered when the Phase I objective is below epsilon_feas, with a small margin to prevent premature transition.

4. **Degenerate Phase I bases.** Phase I is particularly susceptible to degeneracy because the initial crash basis often has many variables exactly at their bounds (zero-valued slack variables). The perturbation-based anti-cycling mechanism (P2.6) is critical during Phase I to prevent infinite cycling through degenerate bases.

---

## Complexity

### Time Complexity

Phase I uses the same per-iteration operations as Phase II (see P2.1, Complexity section). The total Phase I cost is:

| Scenario | Phase I Iterations | Notes |
|----------|-------------------|-------|
| Feasible crash basis | 0 | Phase I skipped entirely |
| Nearly feasible crash | O(1) to O(sqrt(m)) | Few violations to resolve |
| General case | O(m) | Comparable to Phase II |
| Pathological | O(2^n) | Theoretical worst case (Klee-Minty) |

In practice, Phase I iterations constitute a small fraction of total iterations for most problems. The crash basis procedure (P2.5) eliminates Phase I entirely on many practical instances.

### Space Complexity

Phase I requires no additional memory beyond what is already allocated by the SolverState initialization (P3.22, cxf_simplex_init). The Phase I objective coefficients are computed on-the-fly from the variable status and bound data already present in the solver state. The Phase I reduced costs reuse the same storage as the Phase II reduced costs.

---

## Interaction with Other Components

### Pricing System (P2.3, P3.17, P3.18)

During Phase I, the pricing system operates with the Phase I reduced costs. The pricing candidate selection, partial pricing partitioning, and multi-level tolerance adjustment all function identically to Phase II — only the reduced cost values change. The pricing subsystem is not aware of which phase is active; the phase distinction is handled entirely through the objective function swap and reduced cost recomputation.

### Ratio Test (P2.4)

The Harris two-pass ratio test and bound-flipping ratio test operate identically during Phase I and Phase II. The ratio test determines the step length and leaving variable based on the current bounds and the FTRAN direction vector, which are independent of the objective function.

### Basis Update (P2.2, P3.16)

The eta vector chain and basis refactorization mechanism are shared between Phase I and Phase II. A refactorization may be triggered at the Phase I → Phase II transition to ensure numerical accuracy for the new objective.

### Bound Propagation (P2.8, P3.20)

The bidirectional bound propagation (cxf_simplex_step2 and cxf_simplex_step3) operates during Phase I. Bound tightening during Phase I can:
- Detect infeasibility early (before Phase I optimality is reached)
- Fix variables at bounds, reducing the effective problem size for both Phase I and Phase II
- Improve the starting point for Phase II by tightening bounds during Phase I

### Perturbation (P2.6, P3.21)

The perturbation-based anti-cycling mechanism is active during Phase I. Because Phase I bases are often highly degenerate (many variables at bounds), perturbation is frequently needed to ensure Phase I progress.

### Phase End Function (P3.21)

The cxf_simplex_phase_end function monitors the Phase I → Phase II transition. It is called within the main iteration loop (P3.25, cxf_solve_lp, Phase 6) and performs:
1. Checking free variables for dual feasibility violations (infeasibility detection)
2. Removing inactive constraints from the active set (problem size reduction)
3. Recomputing activity bounds for modified constraints

The phase_end function does not directly trigger the transition; it detects the conditions under which the transition should occur and signals this to the orchestration layer. The actual objective swap and reduced cost recomputation are performed by the solve driver (P3.25).

### Solve Driver (P3.25)

The cxf_solve_lp function (P3.25) orchestrates the Phase I → Phase II transition. After crash basis construction, it evaluates whether Phase I is needed, runs the iteration loop (which handles both phases), and manages the state transformations at the phase boundary. The two-level iteration loop structure provides convergence control for both Phase I and Phase II iterations.

---

## Edge Cases

### Empty Problem (m = 0 or n = 0)

If the problem has no constraints, Phase I is trivially satisfied (the empty solution is feasible). If the problem has no variables, feasibility reduces to checking b = 0.

### All-Equality Constraints

Problems with all equality constraints are more likely to require Phase I, since the crash basis (which assigns slacks at their lower bounds) may produce basic variables that violate the equality RHS. The crash procedure detects immediate infeasibility when |b_i| >= epsilon_feas for any equality constraint i.

### Infeasible Equality Constraints

An equality constraint a^T x = b where b is nonzero and no feasible assignment of x can satisfy it is detected either:
- During crash (if |b_i| >= epsilon_feas with the slack basis), or
- During Phase I (when the Phase I objective cannot be driven to zero).

### Warm Start from Previous Solve

When a warm-start basis is available from a previous solve, it may already be feasible. In this case, Phase I is skipped entirely and the solver proceeds directly to Phase II. If the warm-start basis is infeasible (due to model modifications between solves), Phase I is invoked to restore feasibility.

### Crossover from Barrier Solution

When the simplex solver is invoked to clean up a barrier (interior-point) solution, the starting point is an interior point that is primal feasible but not at a vertex. The crossover procedure (P2.7, P3.23) pushes variables to their bounds to construct a basic solution. If the resulting basis is feasible, Phase I is skipped. If crossover produces an infeasible basis (which can happen due to numerical issues in the interior-point solution), Phase I is invoked.

---

## Termination

### Convergence Guarantee

Phase I inherits the finite convergence guarantee of the revised simplex method (P2.1, Termination). With anti-cycling (perturbation or Bland's rule), Phase I visits at most C(n, m) distinct bases and therefore terminates finitely.

### Termination Conditions

Phase I terminates when any of the following holds:

1. **Feasibility achieved:** The Phase I objective w(x) is zero within epsilon_feas. All basic variables satisfy their bound constraints. The solver transitions to Phase II.

2. **Infeasibility proved:** The Phase I objective w(x) is at its minimum (no improving pivot exists) but the minimum is strictly positive. The original problem has no feasible solution.

3. **Iteration limit reached:** The iteration counter exceeds the configured maximum.

4. **Time limit reached:** The elapsed solve time exceeds the configured maximum.

5. **User termination:** An external termination signal has been received.

6. **Numerical failure:** Basis refactorization fails and recovery attempts are exhausted.

---

## Verification Checklist

```
[x] No hex addresses or offsets
[x] No Ghidra artifacts (param_, local_, iVar, DAT_, FUN_)
[x] No binary-specific constants or magic numbers
[x] No copied code fragments from analyzed source
[x] All descriptions are behavioral, not implementational
[x] All data structures described semantically using Layer 1/2 references
[x] Explicit cross-references to P2.1-P2.8 (algorithm specs) and P3.16-P3.25 (module specs)
[x] All algorithms cite published sources (Dantzig, Chvatal, Vanderbei, Maros, Bertsimas & Tsitsiklis)
[x] Passes the Clean Room Test: could be written without seeing the binary
```

## References

- Bertsimas, D. and Tsitsiklis, J.N. (1997). *Introduction to Linear Programming*. Athena Scientific.
- Chvatal, V. (1983). *Linear Programming*. W.H. Freeman.
- Dantzig, G.B. (1963). *Linear Programming and Extensions*. Princeton University Press.
- Forrest, J.J.H. and Goldfarb, D. (1992). "Steepest-edge simplex algorithms for linear programming." *Mathematical Programming*, 57(1):341--374.
- Gould, N.I.M. and Reid, J.K. (1989). "New crash procedures for large systems of linear constraints." *Mathematical Programming*, 45:475--501.
- Harris, P.M.J. (1973). "Pivot selection methods of the Devex LP code." *Mathematical Programming*, 5(1):1--28.
- Maros, I. (2003). *Computational Techniques of the Simplex Method*. Springer. International Series in Operations Research and Management Science, Vol. 61.
- Vanderbei, R.J. (2014). *Linear Programming: Foundations and Extensions*. 4th ed. Springer.
