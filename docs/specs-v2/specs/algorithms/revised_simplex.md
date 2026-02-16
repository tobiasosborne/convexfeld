# Revised Simplex Method

## Published Reference

- **Primary:** Dantzig, G.B. (1963). *Linear Programming and Extensions*. Princeton University Press. Chapters 5--7 (simplex method), Chapter 11 (revised simplex).
- **Computational techniques:** Maros, I. (2003). *Computational Techniques of the Simplex Method*. Springer. Chapters 7--12 (pricing, ratio test, basis update, degeneracy).
- **Steepest edge pricing:** Goldfarb, D. and Reid, J.K. (1977). "A practicable steepest-edge simplex algorithm." *Mathematical Programming*, 12(1):361--371. Forrest, J.J.H. and Goldfarb, D. (1992). "Steepest-edge simplex algorithms for linear programming." *Mathematical Programming*, 57(1):341--374.
- **Devex pricing:** Harris, P.M.J. (1973). "Pivot selection methods of the Devex LP code." *Mathematical Programming*, 5(1):1--28.
- **Harris ratio test:** Harris, P.M.J. (1973), ibid.
- **Bound flipping:** Forrest, J.J.H. and Goldfarb, D. (1992), ibid. See also Maros (2003), Section 9.8.
- **Product form of inverse:** Dantzig, G.B. (1963), Chapter 11. Forrest, J.J.H. and Tomlin, J.A. (1972). "Updated triangular factors of the basis to maintain sparsity in the product form simplex method." *Mathematical Programming*, 2(1):263--278.
- **Perturbation anti-cycling:** Charnes, A. (1952). "Optimality and degeneracy in linear programming." *Econometrica*, 20(2):160--170. See also Gill, P.E., Murray, W., Saunders, M.A., and Wright, M.H. (1989). "A practical anti-cycling procedure for linearly constrained optimization." *Mathematical Programming*, 45(3):437--474.
- **General reference:** Chvatal, V. (1983). *Linear Programming*. W.H. Freeman. Vanderbei, R.J. (2014). *Linear Programming: Foundations and Extensions*. 4th ed. Springer.

The algorithm described here is a standard revised simplex method combining the two-phase method (Dantzig, 1963) with steepest edge pricing (Goldfarb and Reid, 1977; Forrest and Goldfarb, 1992), Harris two-pass ratio test (Harris, 1973), bound flipping ratio test (Forrest and Goldfarb, 1992), product form of the inverse via eta vectors (Dantzig, 1963; Forrest and Tomlin, 1972), and perturbation-based anti-cycling (Charnes, 1952).

---

## Purpose

The revised simplex method solves linear programs of the form:

> minimize c^T x subject to Ax = b, l <= x <= u

where A is an m-by-n constraint matrix, c is the objective vector, b is the right-hand side, and l, u are variable bounds (possibly infinite). The method operates on the LP in standard form with bounded variables, where inequality constraints have been converted to equalities by the addition of slack variables.

The revised simplex method is the core LP solver. It is invoked after optional preprocessing (scaling, bound tightening, redundancy removal) and crash basis computation, and it produces a basic feasible solution at a vertex of the feasible polyhedron. The method supports both Phase I (finding feasibility) and Phase II (optimizing the objective), with automatic transition between phases. It also serves as the cleanup algorithm following barrier (interior-point) crossover.

---

## Inputs

| Input | Type | Description | Preconditions |
|-------|------|-------------|---------------|
| A | Sparse matrix (m x n) | Constraint coefficient matrix, stored in both CSR and CSC formats | No NaN or infinity entries; dimensions consistent |
| c | Dense vector (n) | Objective function coefficients | Finite entries |
| b | Dense vector (m) | Right-hand side values | Finite entries |
| l | Dense vector (n) | Variable lower bounds | l_j <= u_j for all j; may be negative infinity |
| u | Dense vector (n) | Variable upper bounds | u_j >= l_j for all j; may be positive infinity |
| B_0 | Basis (m indices) | Initial basis, either from a crash heuristic, a warm start, or the identity (slack variables) | Must index m linearly independent columns of A |
| tolerances | Parameter set | Optimality tolerance, feasibility tolerance, pivot tolerance | All strictly positive |
| iteration_limit | Integer | Maximum number of simplex iterations before termination | Positive |
| solve_mode | Enum | PRIMAL, DUAL, or AUTO | Valid enum value |

**Additional preconditions:**
- The constraint matrix has been preprocessed (scaled, redundant rows removed) if preprocessing is enabled.
- Working copies of bounds and RHS have been created so that the algorithm may modify them without affecting the original model data.
- The basis factorization (LU decomposition of B_0) is available and numerically valid.

---

## Outputs

| Output | Type | Description |
|--------|------|-------------|
| x* | Dense vector (n) | Optimal primal solution (variable values) |
| y* | Dense vector (m) | Optimal dual solution (shadow prices) |
| z* | Scalar | Optimal objective value c^T x* |
| basis | Basis (m indices) | Optimal basis identifying the vertex |
| status | Enum | OPTIMAL, INFEASIBLE, UNBOUNDED, ITERATION_LIMIT, or NUMERIC_DIFFICULTY |

**Postconditions on OPTIMAL status:**
- Primal feasibility: Ax* = b within feasibility tolerance, l <= x* <= u within feasibility tolerance.
- Dual feasibility: For each non-basic variable j, the reduced cost d_j satisfies the optimality condition: d_j >= -epsilon_opt if x_j is at its lower bound, and d_j <= epsilon_opt if x_j is at its upper bound, where epsilon_opt is the optimality tolerance.
- The basis matrix B formed by the m basic columns of A is nonsingular.

---

## Algorithm Description

### Overview

The revised simplex method maintains a basis B consisting of m linearly independent columns of A. At each iteration, the algorithm selects a non-basic variable to enter the basis (pricing), computes the direction of change (FTRAN), determines how far to move before another variable hits a bound (ratio test), and updates the basis (pivot). The basis inverse is maintained implicitly via a sequence of elementary matrices (eta vectors) rather than stored explicitly.

The method operates in two phases. Phase I minimizes infeasibility by driving artificial variables out of the basis (or equivalently, minimizing the sum of constraint violations). Phase II minimizes the actual objective function c^T x. The transition from Phase I to Phase II occurs when all artificial variables have left the basis and the current solution is primal feasible.

The complete iteration cycle for the primal revised simplex is:

1. **Price** -- select entering variable with most negative reduced cost
2. **FTRAN** -- compute the representation of the entering column in the current basis
3. **Ratio test** -- determine the leaving variable and step length
4. **Pivot** -- exchange entering and leaving variables in the basis
5. **BTRAN** -- compute the leaving row of the basis inverse (for pricing weight update)
6. **Update pricing weights** -- maintain steepest edge norms incrementally
7. **Check termination** -- test for optimality, infeasibility, unboundedness, or iteration limit

### Detailed Steps

#### Step 0: Initialization

Given an initial basis B_0 with basis header beta = (beta_1, ..., beta_m) listing the basic variable indices:

1. Factorize B_0 = LU (via sparse LU with threshold pivoting).
2. Compute basic variable values: x_B = B^{-1} b.
3. Assign non-basic variables to their bounds: x_j = l_j if at lower bound, x_j = u_j if at upper bound.
4. Compute the objective value: z = c^T x.
5. Compute reduced costs: d = c_N - N^T y, where y = B^{-T} c_B (the simplex multipliers) and N is the matrix of non-basic columns.
6. Initialize steepest edge weights: gamma_j = ||B^{-1} a_j||^2 for each non-basic variable j (or set gamma_j = 1 for initial Devex approximation).
7. Determine initial phase:
   - If x_B satisfies l_B <= x_B <= u_B within tolerance, enter Phase II.
   - Otherwise, enter Phase I with a modified objective that penalizes infeasibility.

#### Step 1: Pricing (Select Entering Variable)

**Design choice: Partial pricing with steepest edge weights.**

The entering variable q is selected to maximize the rate of objective improvement per unit of movement in the variable space:

> q = argmax_{j in N} |d_j| / sqrt(gamma_j) subject to the sign condition

where d_j is the reduced cost of non-basic variable j and gamma_j is its steepest edge weight. The sign condition requires:
- d_j < -epsilon_opt if x_j is at its lower bound (can increase),
- d_j > epsilon_opt if x_j is at its upper bound (can decrease),
- |d_j| > epsilon_opt if x_j is superbasic (can move either way).

**Partial pricing strategy:** Rather than examining all non-basic variables at every iteration (full pricing), the variable index set is partitioned into sections. Only one or a few sections are scanned per iteration, and the best candidate across recently scanned sections is selected. This reduces the per-iteration cost of pricing from O(n) to O(n/p) where p is the number of sections, at the cost of occasionally missing the globally best candidate. Periodic full scans ensure that no variable is neglected indefinitely. See Maros (2003), Chapter 7.

**Multi-level tolerance:** The pricing tolerance is adjusted dynamically across three levels:
- *Initial phase (fast):* A relatively loose tolerance (e.g., 1e-6) accepts candidates with moderately negative reduced costs, enabling rapid early progress.
- *Standard phase:* A tighter tolerance (e.g., 1e-10) is used as the algorithm approaches optimality.
- *Aggressive phase:* A tight tolerance (e.g., 1e-9) is used for final convergence when near-optimal.

If no candidate with sufficiently negative reduced cost is found across all sections, the algorithm declares optimality (Phase II) or feasibility (Phase I).

**Referenced specification:** See P2.3 (Pricing) for the full pricing subsystem specification.

#### Step 2: FTRAN (Forward Transformation)

Compute the representation of the entering column a_q in the current basis:

> eta_q = B^{-1} a_q

This is performed by solving the triangular system B eta_q = a_q using the current LU factors and accumulated eta vectors. The product form of the inverse represents B^{-1} as:

> B^{-1} = E_k * E_{k-1} * ... * E_1 * B_0^{-1}

where each E_i is an elementary matrix (eta matrix) differing from the identity in exactly one column. FTRAN applies these elementary transformations in sequence. See Dantzig (1963), Chapter 11, and the basis update specification (P2.2).

The vector eta_q determines the rate of change of each basic variable when x_q changes: if x_q increases by theta, then x_{beta_i} changes by -theta * eta_{q,i}.

#### Step 3: Ratio Test (Select Leaving Variable)

**Design choice: Harris two-pass ratio test with bound flipping.**

The ratio test determines the maximum step length theta before some basic variable hits a bound, identifying the leaving variable.

**Pass 1 (Harris relaxed):** For each basic variable i where eta_{q,i} is non-negligible (|eta_{q,i}| > epsilon_pivot):

> theta_max = min_i { (x_{beta_i} - l_{beta_i} + epsilon_feas) / eta_{q,i} : eta_{q,i} > 0 } union { (x_{beta_i} - u_{beta_i} - epsilon_feas) / eta_{q,i} : eta_{q,i} < 0 }

This relaxed ratio allows candidates within a tolerance band of their bounds.

**Pass 2 (largest pivot):** Among all candidates i whose ratio is within theta_max, select the one with the largest |eta_{q,i}|:

> r = argmax_{i : ratio_i <= theta_max} |eta_{q,i}|

The Harris two-pass approach (Harris, 1973) improves numerical stability by preferring larger pivot elements while still ensuring a near-minimal step length. This reduces the sensitivity to rounding errors and avoids degenerate pivots with tiny pivot elements.

**Bound flipping (long step):** When the entering variable x_q has finite bounds in both directions, the algorithm may perform a *bound flip* instead of a standard basis exchange. If the step length theta would carry x_q from its current bound to its opposite bound (theta = u_q - l_q or l_q - u_q), the variable is flipped to its other bound without entering the basis. This constitutes a "long step" that can traverse multiple breakpoints in one iteration, reducing the total iteration count on problems with many bounded variables.

During a bound flip:
1. The non-basic variable x_q is moved from one bound to the other.
2. Basic variables are updated: x_{beta_i} -= (u_q - l_q) * eta_{q,i}.
3. The reduced costs of other non-basic variables are updated.
4. No basis exchange occurs; x_q remains non-basic at its new bound.
5. The algorithm returns to Step 1 for a new pricing decision.

Multiple consecutive bound flips may occur before a standard pivot is performed. See Forrest and Goldfarb (1992) and Maros (2003), Section 9.8.

**Special cases:**
- If all ratios are infinite (no basic variable is bounded in the step direction), the problem is unbounded. Return UNBOUNDED status.
- If the maximum step is zero (degenerate pivot), the basis changes but the solution point does not move. This is handled by the anti-cycling mechanism (Step 8).

**Referenced specification:** See P2.4 (Ratio Test) for the full ratio test specification.

#### Step 4: Pivot (Basis Exchange)

Exchange the entering variable q and the leaving variable beta_r:

1. Record the pivot operation as a new eta vector E_{k+1}, which is the identity matrix except in column r, where it contains the vector -eta_q / eta_{q,r} (with 1/eta_{q,r} in position r). Prepend E_{k+1} to the eta vector linked list.
2. Update the basis header: beta_r = q.
3. Update variable statuses: mark q as basic (in row r), mark beta_r as non-basic at the appropriate bound.
4. Update the objective value: z += theta * d_q.
5. Update basic variable values: x_{beta_i} -= theta * eta_{q,i} for all i.
6. Set the entering variable's reduced cost to zero (it is now basic).
7. Increment the eta vector count.

**Referenced specification:** See P2.2 (Basis Update / Product Form of Inverse) for the full basis update specification.

#### Step 5: BTRAN (Backward Transformation)

Compute the leaving row of the basis inverse for use in updating steepest edge weights:

> rho = e_r^T B^{-1}

where e_r is the r-th unit vector. This is solved by applying the transposed eta vectors in reverse order. The vector rho is needed for the steepest edge weight update in Step 6.

#### Step 6: Update Pricing Weights

Update the steepest edge weights for all non-basic variables after the pivot. The solver supports two weight maintenance strategies: exact Dantzig-Steepest-Edge (DSE) and the approximate Devex method.

##### Notation

For the pivot in which non-basic variable q enters the basis (replacing basic variable beta_r in row r), define:

- alpha_q = B^{-1} a_q: the FTRAN result (representation of the entering column in the current basis; this is the same vector as eta_q from Steps 2--4, renamed here for clarity in the weight formulas), an m-vector with components alpha_{q,i} for each constraint row i. The pivot element is alpha_{q,r}.
- rho = e_r^T B^{-1}: the BTRAN result (the r-th row of the basis inverse), computed in Step 5.
- tau_j = rho^T a_j: the inner product of the BTRAN vector with column a_j for each non-basic variable j. Note that tau_j equals the r-th component of B^{-1} a_j, i.e., the entry in the leaving row of the current basis representation of column j.
- gamma_j = ||B^{-1} a_j||^2: the steepest edge weight for non-basic variable j, equal to the squared Euclidean norm of the basis representation of column j.

##### Exact DSE Update (Forrest and Goldfarb, 1992)

After the pivot, the new basis inverse satisfies B_new^{-1} = E * B_old^{-1}, where E is the elementary (eta) matrix from Step 4. Applying E to the representation of any non-basic column j gives the updated representation:

> B_new^{-1} a_j = B_old^{-1} a_j - (tau_j / alpha_{q,r}) * (alpha_q - e_r)

Taking the squared norm of both sides and expanding yields the exact steepest edge recurrence (Goldfarb and Reid, 1977; Forrest and Goldfarb, 1992):

> gamma_j' = gamma_j - 2 * (tau_j / alpha_{q,r}) * (sigma_j - tau_j) + (tau_j / alpha_{q,r})^2 * (gamma_q - 2 * alpha_{q,r} + 1)

where sigma_j = (B^{-1} a_j)^T (B^{-1} a_q), the inner product of the basis representations of columns j and q. Computing sigma_j for all non-basic j requires one additional BTRAN solve: let w be the solution of B^T w = alpha_q, then sigma_j = a_j^T w = w^T a_j.

In summary, the exact DSE update per iteration requires:
1. The FTRAN result alpha_q (already computed in Step 2).
2. The BTRAN result rho (computed in Step 5), yielding tau_j = rho^T a_j for all non-basic j.
3. One additional BTRAN solve (B^T w = alpha_q), yielding sigma_j = w^T a_j for all non-basic j.
4. The weight of the entering column, gamma_q = ||alpha_q||^2.

The per-iteration cost is two BTRAN solves plus O(n) inner products. This is O(m) per BTRAN (with the eta vector chain) plus O(nnz(N)) for the column products, where nnz(N) is the total number of nonzeros in non-basic columns.

**Simplified form.** When the additional BTRAN is omitted (setting sigma_j = tau_j, which is exact only when B^{-1} a_j is concentrated in row r), the cross-term vanishes and the formula reduces to:

> gamma_j' = gamma_j + (tau_j / alpha_{q,r})^2 * (gamma_q - 2 * alpha_{q,r} + 1)

This one-BTRAN approximation is cheaper per iteration (eliminating one O(m) solve) but accumulates drift in the weights over many pivots because sigma_j generally differs from tau_j. Periodic exact recomputation of all weights from scratch (e.g., at each basis refactorization) corrects this drift. See Forrest and Goldfarb (1992), Section 3.

**Weight of the entering variable.** After the pivot, the entering variable q is now basic in row r. The new steepest edge weight for the leaving variable (now non-basic) is:

> gamma_p' = 1 / alpha_{q,r}^2 * gamma_q

where p = beta_r is the variable that left the basis. This follows directly from the pivot algebra.

**Numerical safeguard.** After each update, the weight is clamped to a minimum positive value (e.g., gamma_j' = max(epsilon_weight, gamma_j')) to prevent division by zero or negative weights arising from floating-point roundoff in the subtraction terms.

##### Devex Approximate Update (Harris, 1973)

The Devex method (Harris, 1973; see also Goldfarb and Forrest, 1992) avoids the expensive BTRAN-derived sigma_j products by maintaining approximate weights that track the steepest edge norms loosely. The Devex pricing rule selects the entering variable as:

> q = argmax_{j in N} |d_j| / sqrt(gamma_j)

using the same formula as exact steepest edge, but with approximate weights gamma_j.

**Reference framework.** Devex maintains a *reference framework* R, which is the set of non-basic variable indices at the time the weights were last initialized. When the reference framework is established, all weights are set to 1:

> gamma_j = 1 for all j in R

After each pivot, the Devex weight update for non-basic variable j is:

> gamma_j' = max(epsilon_devex * gamma_j, (tau_j / alpha_{q,r})^2 + delta_j)

where:
- tau_j / alpha_{q,r} is the ratio of the leaving-row component of B^{-1} a_j to the pivot element (same as in the DSE formula),
- delta_j = 1 if variable j is in the current reference framework R, and delta_j = 0 otherwise,
- epsilon_devex is a decay factor (typically 0.99) that prevents the weight from decreasing too rapidly when the second term is small.

The reference framework term delta_j approximates the contribution of the identity-like component in the basis representation, providing a lower bound on the true steepest edge weight for variables that have remained non-basic since the reference framework was established. When most variables have entered and left the basis (making the reference framework stale), the weights are reinitialized: a new reference framework R' is set to the current non-basic set and all weights are reset to 1.

**Comparison with exact DSE.** The Devex update avoids the additional BTRAN solve required by exact DSE (Step 3 in the DSE procedure above), reducing the per-iteration cost of weight maintenance from two BTRAN solves plus O(nnz(N)) products to zero BTRAN solves plus O(nnz(N)) products. On large-scale problems, this can reduce the per-iteration time significantly. However, the approximate weights may cause the pricing rule to select suboptimal entering variables, increasing the total iteration count. Empirically, Devex typically requires 10--30% more iterations than exact steepest edge but with 30--50% less time per iteration, making it competitive or superior on large sparse problems (Harris, 1973; Forrest and Goldfarb, 1992; Maros, 2003, Section 7.4).

**Design choice:** The solver supports both exact steepest edge and Devex pricing, selectable by parameter. Steepest edge is the default for most problems; Devex is selected automatically for very large problems where the per-iteration BTRAN cost dominates. During Phase I (feasibility), Devex may be preferred because the objective function is artificial and the extra accuracy of exact steepest edge provides less benefit.

**Referenced specification:** See P2.3 (Pricing) for the full pricing weight update specification.

#### Step 7: Check Termination

After each pivot (or batch of bound flips), evaluate termination conditions:

1. **Optimality (Phase II):** If no non-basic variable has a reduced cost violating the optimality tolerance, the current solution is optimal. Return OPTIMAL.
2. **Feasibility achieved (Phase I):** If all artificial variables have left the basis (or the Phase I objective is zero within tolerance), transition to Phase II by restoring the original objective coefficients and recomputing reduced costs.
3. **Infeasibility (Phase I):** If the Phase I objective is strictly positive at optimality, the original problem has no feasible solution. Return INFEASIBLE.
4. **Unboundedness:** If the ratio test finds no blocking variable, the objective is unbounded. Return UNBOUNDED.
5. **Iteration limit:** If the iteration count exceeds the configured limit, return ITERATION_LIMIT.
6. **Numeric difficulty:** If the basis factorization fails or pivot elements are below minimum thresholds, attempt recovery by refactorizing the basis. If recovery fails, return NUMERIC_DIFFICULTY.

#### Step 8: Anti-Cycling via Perturbation

**Design choice: Bound perturbation for anti-cycling.**

Degeneracy occurs when one or more basic variables are at a bound, causing zero-length steps that do not change the solution point. If the same sequence of bases repeats, the algorithm cycles infinitely.

**Detection:** Periodically, the algorithm takes a snapshot of the current basis and compares it to a later basis. If the basis has not changed significantly (measured by the fraction of basic variables that differ) after a configurable number of iterations, cycling is suspected.

**Response:** When cycling is detected, the algorithm applies perturbation to the variable bounds:
1. For each basic variable x_{beta_i}, perturb its bounds:
   - l'_{beta_i} = l_{beta_i} - epsilon_i
   - u'_{beta_i} = u_{beta_i} + epsilon_i
   where epsilon_i is a small random perturbation proportional to the bound range and a function of the variable index (to ensure distinct perturbations).
2. Continue simplex iterations on the perturbed problem. The perturbation breaks the degeneracy by making all basic variables strictly interior to their bounds.
3. After the perturbed problem reaches optimality, remove the perturbation (restore original bounds) and perform cleanup iterations to resolve any remaining infeasibility introduced by the restoration.

This approach is based on Charnes (1952) and is the standard perturbation technique described in Maros (2003), Section 10.3. It is preferred over Bland's rule (Bland, 1977) because Bland's rule, while guaranteeing finite termination, often leads to poor pivot choices and substantially increased iteration counts.

**Referenced specification:** See P2.6 (Perturbation) for the full anti-cycling specification.

#### Step 9: Periodic Basis Refactorization

The product form of the inverse accumulates eta vectors over successive pivots. As the eta list grows:
- FTRAN and BTRAN operations become slower (each must traverse the full list).
- Numerical errors accumulate from successive transformations.

**Trigger conditions for refactorization:**
1. The eta vector count exceeds a threshold (typically a fraction of the basis dimension m, e.g., m/4 or a fixed count such as 100).
2. The estimated numerical accuracy has degraded below a threshold.
3. The solver has performed a phase transition.

**Refactorization procedure:**
1. Extract the current basis matrix B from the column indices in the basis header.
2. Perform a sparse LU factorization: B = LU with threshold partial pivoting.
3. Discard all accumulated eta vectors and reset the eta count to zero.
4. Recompute basic variable values x_B = B^{-1} b to correct accumulated roundoff.
5. Recompute reduced costs d_N to correct accumulated drift.

The Forrest-Tomlin update scheme (Forrest and Tomlin, 1972) maintains the LU factors in updated triangular form between refactorizations, providing a more efficient alternative to the pure product form for individual updates. However, periodic complete refactorization remains necessary for numerical hygiene.

**Referenced specification:** See P2.2 (Basis Update / Product Form of Inverse) for the full specification.

### Key Design Choices

- **Two-phase method vs. Big-M:** The two-phase method (Dantzig, 1963) is used rather than the Big-M method. The two-phase method avoids the numerical difficulties associated with choosing a sufficiently large M and provides a clean separation between feasibility (Phase I) and optimality (Phase II). The Big-M method can cause conditioning problems when M is large relative to other coefficients.

- **Steepest edge with Devex fallback vs. Dantzig pricing:** Steepest edge pricing (Goldfarb and Reid, 1977) is the default pricing strategy rather than the original Dantzig pricing rule (most negative reduced cost). Steepest edge typically reduces the iteration count by 30--50% on large problems by selecting the direction of steepest descent in the original variable space, not just the coordinate direction with the most negative gradient. The Devex approximation (Harris, 1973) provides a computationally cheaper alternative when exact steepest edge is too expensive.

- **Partial pricing vs. full pricing:** Partial pricing scans a subset of non-basic variables at each iteration, reducing per-iteration cost at the potential expense of missing the globally best pivot. This is a standard trade-off in large-scale LP solvers. See Maros (2003), Chapter 7.

- **Harris ratio test vs. standard minimum ratio:** The Harris two-pass ratio test (Harris, 1973) replaces the textbook minimum ratio test. By introducing a tolerance band and preferring larger pivots, it improves numerical stability and reduces the incidence of degenerate pivots.

- **Bound flipping vs. standard pivoting:** For bounded variables, bound flipping (Forrest and Goldfarb, 1992) allows the algorithm to take "long steps" past breakpoints where variables hit bounds, flipping them to their opposite bound without performing a basis exchange. This can dramatically reduce iteration counts on highly constrained problems with many bounded variables.

- **Product form of inverse (PFI) vs. explicit inverse:** The basis inverse is maintained as a product of elementary matrices (eta vectors) rather than computed and stored explicitly. This is the classical approach (Dantzig, 1963) that exploits sparsity and avoids the O(m^2) storage cost of an explicit inverse. Periodic refactorization resets the product to maintain accuracy and performance.

- **Perturbation vs. Bland's rule for anti-cycling:** Perturbation (Charnes, 1952) is preferred over Bland's rule (Bland, 1977). While Bland's rule guarantees finite termination with any pivoting strategy, it does not consider objective improvement and often leads to very poor pivot choices. Perturbation preserves the freedom to use steepest edge pricing and Harris ratio test while breaking degeneracy.

- **Primal vs. dual simplex:** The solver supports both primal and dual simplex variants. The dual simplex maintains dual feasibility (all reduced costs satisfy optimality conditions) and drives toward primal feasibility, while the primal simplex maintains primal feasibility and drives toward dual feasibility (optimality). The choice is governed by a solve mode parameter; AUTO mode selects based on problem characteristics (e.g., dual simplex is often preferred for reoptimization after bound changes, while primal simplex may be preferred for problems with a good crash basis).

---

## Numerical Considerations

### Tolerances

| Tolerance | Typical Range | Role |
|-----------|---------------|------|
| Optimality tolerance (epsilon_opt) | 1e-6 to 1e-8 | Reduced cost threshold for declaring optimality |
| Feasibility tolerance (epsilon_feas) | 1e-6 to 1e-8 | Constraint violation threshold |
| Pivot tolerance (epsilon_piv) | 1e-9 to 1e-13 | Minimum acceptable pivot element magnitude |
| Bound tolerance (epsilon_bnd) | 1e-10 to 1e-12 | Threshold for treating bounds as equal (variable is fixed) |
| Pricing tolerance | 1e-6 to 1e-10 | Threshold for accepting a pricing candidate (varies by phase) |

### Stability Concerns

1. **Catastrophic cancellation in activity bounds:** When computing constraint activity bounds by summing products of coefficients and variable bounds, the summation can suffer from catastrophic cancellation when large positive and negative terms nearly cancel. A safe rounding technique is applied: after each addition, if the result cannot be verified by reverse subtraction (i.e., (a + b) - b != a due to floating-point precision), the result is nudged away from zero by a factor of (1 + 2*epsilon_machine) or (1 - 2*epsilon_machine). This ensures conservative activity bounds.

2. **Small pivot elements:** Pivot elements below the pivot tolerance degrade the accuracy of the basis inverse representation. The Harris ratio test mitigates this by preferring larger pivots in its second pass. If the best available pivot is still below the tolerance, the iteration is skipped and a different entering variable is selected.

3. **Eta vector accumulation:** Each pivot appends an eta vector to the linked list. After many pivots without refactorization, FTRAN/BTRAN operations traverse a long chain, accumulating roundoff at each step. Periodic refactorization (Step 9) resets this chain.

4. **Reduced cost drift:** Reduced costs are updated incrementally at each iteration rather than recomputed from scratch. Small errors accumulate over many iterations. Refactorization includes a full recomputation of reduced costs to correct this drift.

5. **Near-degenerate bounds:** When a variable's lower and upper bounds differ by less than the bound tolerance, the variable is treated as fixed. This avoids pivoting on nearly fixed variables, which produces tiny step lengths and degrades the solution.

### Degeneracy Handling

Degeneracy (zero-length simplex steps) is ubiquitous in practical LP problems, particularly those with many equality constraints or tight bounds. The algorithm handles degeneracy through:

1. **Perturbation (primary):** As described in Step 8, bound perturbation breaks degeneracy by making all basic variables strictly between their bounds.
2. **Harris ratio test (secondary):** The tolerance band in the Harris ratio test allows small steps past degenerate breakpoints, reducing (but not eliminating) the incidence of exact zero steps.
3. **Bound flipping (tertiary):** Bound flips can bypass degenerate configurations by moving non-basic variables to opposite bounds without requiring a basis change.

---

## Termination

### Convergence Guarantee

In exact arithmetic, the revised simplex method terminates finitely because:
1. There are at most C(n, m) = n! / (m!(n-m)!) distinct bases.
2. With anti-cycling (perturbation or Bland's rule), no basis is visited twice.
3. Therefore, the method terminates in at most C(n, m) iterations.

In practice, the iteration count is typically O(m) to O(m * log(n/m)) for well-conditioned problems (Vanderbei, 2014, Chapter 7), though worst-case exponential examples exist (Klee and Minty, 1972).

### Termination Conditions

The algorithm terminates when any of the following holds:

1. **Optimal:** No eligible entering variable exists (all reduced costs satisfy optimality within tolerance).
2. **Infeasible:** Phase I terminates with a positive optimal value (infeasibility cannot be eliminated).
3. **Unbounded:** The ratio test finds no blocking variable (improving direction is a ray).
4. **Iteration limit reached:** The iteration counter exceeds the user-specified maximum.
5. **User termination:** An external termination signal (e.g., callback) has been received.
6. **Numerical failure:** Basis refactorization fails and recovery attempts are exhausted.

### Phase Transition

The transition from Phase I to Phase II is triggered when:
1. All infeasibilities have been eliminated (Phase I objective is zero within tolerance), AND
2. The current basis is primal feasible.

At transition:
1. The Phase I objective is replaced by the original objective c.
2. Reduced costs are recomputed using the original c: d = c_N - N^T (B^{-T} c_B).
3. The basis refactorization counter may be reset.
4. Pricing weights are reinitialized or carried forward (implementation choice).

---

## Complexity

### Time Complexity

| Operation | Per-Iteration Cost | Notes |
|-----------|-------------------|-------|
| Pricing (partial) | O(n/p) | p = number of partitions |
| Pricing (full) | O(n) | When full scan is triggered |
| FTRAN | O(m + eta_count * sparsity) | Depends on eta list length and fill-in |
| Ratio test | O(m) worst case | O(nnz(a_q)) typical for sparse columns |
| BTRAN | O(m + eta_count * sparsity) | Same structure as FTRAN |
| Weight update (DSE) | O(m + nnz(N)) | Two BTRAN solves + column products |
| Weight update (Devex) | O(nnz(N)) | No BTRAN; column products only |
| Basis update (eta) | O(m) | Append one eta vector |
| Refactorization | O(m^2) to O(m^3) | Depends on fill-in; sparse LU is much cheaper in practice |

**Total per iteration:** Dominated by FTRAN/BTRAN (O(m) to O(m * eta_count)) and pricing (O(n/p) to O(n)).

**Total iterations:** Typically O(m) to O(3m) on practical problems, though worst-case is exponential.

### Space Complexity

| Component | Space | Notes |
|-----------|-------|-------|
| Constraint matrix (CSR + CSC) | O(nnz) each | Dual storage for row and column access |
| Working bounds and costs | O(n) | Copies of l, u, c, d |
| Basis header and status | O(m + n) | Basis tracking arrays |
| Eta vector list | O(m * k) | k = number of pivots since last refactorization |
| LU factors | O(nnz(L) + nnz(U)) | Depends on fill-in |
| Steepest edge weights | O(n) | One weight per non-basic variable |
| Activity bounds | O(m) | Min/max constraint activity for bound propagation |

**Total:** O(nnz + m * n/p + m * k), dominated by the constraint matrix storage.

---

## Edge Cases

### Empty or Trivial Problems

- **Zero constraints (m = 0):** All variables are unconstrained by A. If all variables have finite bounds, the optimal solution sets each x_j to the bound that minimizes c_j * x_j. If any variable is unbounded in the improving direction, return UNBOUNDED.
- **Zero variables (n = 0):** The problem is trivially feasible (or infeasible if b != 0). Return OPTIMAL with z = 0 or INFEASIBLE.
- **All variables fixed (l = u):** The unique feasible point is x = l. Check feasibility of Ax = b and return OPTIMAL or INFEASIBLE.

### Degenerate Cases

- **Highly degenerate basis:** Many basic variables at bounds. Perturbation (Step 8) is essential to prevent cycling. If perturbation is already active and cycling persists, increase the perturbation magnitude.
- **Dual degeneracy:** Multiple non-basic variables are tied for the most negative reduced cost. The pricing rule (steepest edge) resolves ties by favoring the steepest descent direction. This does not cause cycling but may affect the basis reached at optimality.

### Numerical Boundary Conditions

- **Near-parallel constraints:** When two constraints are nearly parallel, the basis matrix is ill-conditioned. Refactorization with threshold pivoting is required; if conditioning does not improve, report NUMERIC_DIFFICULTY.
- **Very large coefficient range:** If max|a_ij| / min|a_ij| exceeds approximately 1e10, numerical difficulties are likely. Scaling (applied before the simplex method) mitigates this.
- **Free variables (l_j = -infinity, u_j = +infinity):** Free variables may enter or leave the basis without bound constraints. Special handling in the ratio test is needed: a free variable can increase or decrease without limit, so it never blocks a step. If a free variable has a non-zero reduced cost and no basic variable blocks the step, the problem is unbounded.
- **Fixed variables (l_j = u_j):** Variables with equal bounds are marked as fixed and excluded from pricing. They do not participate in basis exchanges.

### Constraint-Based Bound Propagation

During simplex iterations, implied bounds can be derived from constraint activity analysis. For a constraint sum(a_ij * x_j) <= b_i, if bounds on all variables except x_k are known, then:

> If a_ik > 0: x_k <= (b_i - MinActivity_{without k}) / a_ik
> If a_ik < 0: x_k >= (b_i - MaxActivity_{without k}) / a_ik

where MinActivity and MaxActivity are the minimum and maximum possible values of the left-hand side excluding x_k's contribution. When these implied bounds are tighter than the current bounds, the working bounds are updated. This is a form of preprocessing that can be applied during simplex iterations, not just as a pre-solve step.

See Savelsbergh (1994) and Achterberg (2007) for the theory of bound propagation in LP and MIP solvers.

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
