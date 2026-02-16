# Numerical Stability Guide for LP Solver Implementation

## Overview

This document describes the numerical stability practices required for a robust implementation of the revised simplex method. An LP solver operates entirely in IEEE 754 double-precision floating-point arithmetic (approximately 15--16 significant decimal digits, machine epsilon approximately 2.2e-16). Every arithmetic operation introduces a rounding error bounded by machine epsilon relative to the result. Over the course of thousands or millions of simplex iterations, these small errors compound and can degrade solution quality, cause false infeasibility or unboundedness reports, or trigger infinite cycling.

The practices described here are standard in the LP optimization literature and are drawn from published references. They apply to any simplex-based LP solver implementation and complement the tolerance definitions in P5.3 (tolerances_constants.md), the PFI basis update in P2.1 (product_form_inverse.md), the Harris ratio test in P2.4 (harris_ratio_test.md), the simplex iteration module in P3.20 (simplex_iteration.md), and the pivot operations module in P3.19 (pivot_operations.md).

---

## A. Refactorization Strategy

### The Problem

The Product Form of the Inverse (PFI) represents the basis inverse as a product of elementary matrices (eta vectors). Each simplex pivot appends one eta vector. After k pivots, every FTRAN and BTRAN operation applies k elementary transformations sequentially. Each transformation introduces a small rounding error. After many pivots, the accumulated error in the computed basis inverse can become large enough that:

- The primal solution x = B^{-1} b drifts away from the true solution, causing false infeasibility.
- The reduced costs c_bar = c_N - c_B^T B^{-1} N drift, causing incorrect pricing decisions.
- The objective value diverges from the true optimal.

Refactorization -- recomputing the LU decomposition of the current basis matrix from scratch -- resets the accumulated error to the level of a single factorization.

### When to Refactorize

Refactorization should be triggered by any of the following conditions:

**1. Eta count threshold.** When the number of accumulated eta vectors exceeds a threshold, typically in the range 50 to 200, or a fraction of the number of constraints m (e.g., m/4 to m/2). The exact threshold balances the O(m^2)-to-O(m^3) cost of refactorization against the linearly growing per-iteration cost of applying the eta chain. A common practical default is min(100, max(50, m/4)). See P2.1 (product_form_inverse.md), Section "Refactorization Triggers," for the full set of triggering conditions.

**2. Residual monitoring.** After an FTRAN operation computes x = B^{-1} a, the solver should periodically verify accuracy by computing the residual:

    r = a - B * x

where B * x is computed by directly multiplying the current basis matrix (from the constraint matrix columns) by the FTRAN result. The residual norm ||r|| (using the infinity norm, i.e., the maximum absolute component) measures the accuracy of the basis inverse representation. If ||r|| exceeds a threshold -- typically a small multiple of the feasibility tolerance, such as 10 * epsilon_feas (where epsilon_feas is 1e-6; see P5.3, Section 1) -- a refactorization is triggered.

This is the most robust trigger because it directly measures the quantity that matters: the accuracy of the linear system solution. It catches cases where the eta count is within bounds but a sequence of ill-conditioned pivots has degraded accuracy faster than normal.

**3. Pivot element quality.** If a sequence of pivots produces small pivot elements (close to the Harris pivot tolerance of approximately 1e-9; see P5.3, Section 4), the error amplification per eta vector is much larger than normal. After detecting multiple consecutive small pivots, the solver should trigger an early refactorization regardless of the eta count.

**4. Explicit events.** Certain algorithmic transitions should force a refactorization to ensure a clean numerical state:
- Transition from Phase I to Phase II of two-phase simplex.
- Start of barrier-to-simplex crossover.
- Detection of stalling or objective stagnation (see P3.20, simplex_iteration.md, cxf_simplex_post_iterate).
- Recovery from a numerical difficulty (rejected pivot column, NaN detection).

### Practical Guidance

- After refactorization, the solver should recompute all reduced costs from scratch using the fresh LU factors, rather than carrying forward the pre-refactorization values. This eliminates accumulated drift in the pricing data.
- The refactorization interval should be adaptive: if residual monitoring triggers refactorizations frequently, the solver should reduce the eta count threshold for subsequent intervals.
- On highly degenerate problems (where many pivots produce zero-length steps), the refactorization interval may need to be shorter because degenerate pivots still produce eta vectors that contribute to error growth without improving the solution.

### References

- Bartels, R.H. and Golub, G.H. (1969). "The simplex method of linear programming using LU decomposition." *Communications of the ACM*, 12(5):266--268.
- Reid, J.K. (1982). "A sparsity-exploiting variant of the Bartels-Golub decomposition for linear programming bases." *Mathematical Programming*, 24(1):55--69.
- Maros, I. (2003). *Computational Techniques of the Simplex Method.* Springer, Chapters 5 and 9.

---

## B. Cancellation Detection and Mitigation

### The Problem

Catastrophic cancellation occurs when two nearly equal floating-point numbers are subtracted. If both numbers carry absolute error of magnitude epsilon_abs, their difference has the same absolute error, but the relative error can be arbitrarily large when the difference is small. In simplex computations, cancellation arises in several critical locations:

1. **Activity bound updates.** When a variable's bounds change by a small amount, the constraint activity bounds are updated incrementally: `new_activity = old_activity + delta`. If `old_activity` is large and `delta` is small, the addition loses precision. If the result is then used to detect infeasibility (comparing activity bounds against constraint limits), the lost precision can cause false infeasibility or missed infeasibility.

2. **Reduced cost computation.** The reduced cost of a non-basic variable j is c_j - c_B^T B^{-1} A_j. The subtraction c_j - (inner product) can cancel if the inner product is close to c_j, producing a reduced cost with few significant digits. This is particularly dangerous near optimality, where reduced costs are close to zero and their signs determine the optimality declaration.

3. **Step length computation.** In the ratio test, the step length is computed as (slack) / |d_i|, where slack = x_i - lb_i or ub_i - x_i. When a basic variable is near its bound (degenerate or near-degenerate), slack is small and the subtraction x_i - lb_i may lose precision.

### Mitigation Strategies

**Conservative rounding after cancellation detection.** The pivot operations module (P3.19, cxf_pivot_update) implements a cancellation detection test: after computing `result = existing + delta`, it checks whether `(result - delta)` recovers the original value. When cancellation is detected:
- Maximum activity bounds are rounded downward (multiplied by a factor slightly less than 1.0).
- Minimum activity bounds are rounded upward (multiplied by a factor slightly greater than 1.0).

This ensures activity bounds are always "safe" -- they may be wider than exact values but never tighter. Wider bounds prevent false infeasibility detection at the cost of occasionally missing a feasible tightening. This conservative approach is described in Higham (2002, Chapter 4).

**Compensated summation.** When accumulating sums over many terms -- such as constraint activity computations sum(a_j * x_j) over all variables in a row -- Kahan compensated summation can be used to maintain accuracy. Kahan summation keeps a running compensation variable that captures the low-order bits lost in each addition, achieving an error bound of O(epsilon_machine) independent of the number of terms, rather than the O(n * epsilon_machine) bound of naive summation. This is most valuable for:
- Activity bound recomputation during refactorization.
- Objective value computation after many pivots.
- Reduced cost recomputation from scratch.

For incremental updates (adding a single delta to an existing value), compensated summation is less critical because only one addition is involved. The cancellation detection approach described above is sufficient for single-delta updates.

**Avoiding unnecessary subtraction.** Where possible, reformulate computations to avoid subtracting nearly-equal quantities:
- Instead of computing `ub - lb` when both may be large, store the bound range directly if it is frequently needed.
- Instead of computing `x_i - lb_i` for a near-degenerate variable, track the slack directly as a maintained quantity.

### References

- Higham, N.J. (2002). *Accuracy and Stability of Numerical Algorithms.* 2nd ed. SIAM, Chapters 4 and 5.
- Kahan, W. (1965). "Pracniques: further remarks on reducing truncation errors." *Communications of the ACM*, 8(1):40.

---

## C. Infinity and Extreme Value Handling

### Representing Infinite Bounds

LP models frequently contain variables with one-sided or no bounds (free variables). The solver represents infinite bounds using a large finite sentinel value. The standard choice is 1e100, consistent with the solver infinity documented in P5.3 (tolerances_constants.md, Section 6). Any bound value at or above 1e100 in absolute value is treated as infinite.

A secondary threshold (the "large bound marker," approximately 1e20; see P5.3, Section 12) is used for heuristic decisions: variables with bound ranges at or above 1e20 are treated as "effectively infinite" for purposes such as refactorization eligibility, perturbation magnitude computation, and activity bound tracking.

### Fixed Variables (lb == ub)

When a variable's lower bound equals its upper bound within the bound equality tolerance (approximately 1e-10; see P5.3, Section 9), the variable is treated as fixed. Fixed variables:
- Do not participate in the ratio test as standard candidates (they have zero slack and would always produce degenerate pivots).
- Are handled by a separate tight-bound routine (cxf_pivot_primal, P3.19) that checks whether the variable can be safely eliminated.
- Should be fixed at the midpoint (lb + ub) / 2 when the gap is nonzero but within tolerance, to minimize the constraint perturbation from the fixing operation.

### NaN and Infinity Detection

IEEE 754 arithmetic can produce NaN (Not a Number) from operations such as 0/0 or infinity - infinity, and infinity from overflow or division by zero. These values propagate silently through subsequent computations and corrupt results.

The solver should include NaN/Inf detection at the following checkpoints:
- After FTRAN and BTRAN results, before using the transformed vector for pricing or ratio test decisions.
- After objective value updates, before comparing with convergence criteria.
- After step length computation in the ratio test, before applying the pivot.
- On input data during model loading, to reject models with NaN or Inf coefficients.

When NaN or Inf is detected in an intermediate computation, the recommended response is:
1. Trigger an immediate basis refactorization to attempt recovery.
2. If the NaN/Inf persists after refactorization, report a numerical difficulty status and terminate.

### Ratio Test Clamping

The ratio test can produce very large step lengths when the blocking variable has a large bound range or when the updated column element is very small. Unclamped large steps can cause:
- Overflow in the primal value update (x_B = x_B - theta * d, where theta * d overflows).
- Loss of all significant digits in the updated basic variable values.

The step length should be clamped to prevent these issues. A practical clamping strategy:
- If the step length theta exceeds a large threshold (e.g., 1e15), the pivot is numerically suspect. The solver should either reject the entering column and request an alternative from pricing, or accept the step but trigger an immediate refactorization.
- After applying the step, basic variable values should be checked against their bounds and projected back to the nearest bound if they have overshot.

### References

- Maros, I. (2003). *Computational Techniques of the Simplex Method.* Springer, Section 8.6.
- ConvexFeld Optimizer Reference Manual, "Guidelines for Numerical Issues."

---

## D. Pivot Element Quality

### Minimum Pivot Threshold

The pivot element alpha_{pq} in the simplex method is the denominator in the eta vector computation: eta_i = -d_i / alpha for i != p, and eta_p = 1 / alpha. A small pivot element amplifies all entries in the eta vector, directly amplifying rounding errors in subsequent FTRAN and BTRAN operations.

The Harris pivot tolerance (approximately 1e-9; see P5.3, Section 4) sets the minimum absolute value of an updated column element for a basic variable to be considered as a leaving candidate. Elements below this threshold are treated as zero in the ratio test. This prevents the selection of pivots that would severely degrade numerical accuracy.

A secondary, more conservative threshold (approximately 1e-13; see P5.3, Section 4) is used in bound propagation steps (cxf_simplex_step2 and cxf_simplex_step3; see P3.20) where the consequences of a bad pivot are less severe because bound-change operations are cheaper to reverse.

### Adaptive Pivot Tolerance

The solver uses a multi-phase adaptive pivot tolerance strategy (see P5.3, Section 4, "Adaptive Pivot Tolerance"):

| Phase | Approximate Tolerance | Purpose |
|-------|----------------------|---------|
| Fast (initial) | ~1e-6 | Loose tolerance for rapid early progress |
| Standard (fallback) | ~1e-10 | Very tight tolerance for problematic iterations |
| Aggressive (near optimality) | ~1e-9 | Tight tolerance for final convergence accuracy |

The rationale: in early iterations, the solver is far from optimal, and a loose tolerance accepts more pivot candidates, enabling faster progress even if individual pivots are slightly less accurate. Near optimality, reduced costs are small and their signs are critical, so tight tolerances ensure that the final solution is accurate. If the standard tolerance phase is entered (because the solver is encountering difficulties), the very tight tolerance reduces the risk of accepting a numerically damaging pivot.

### Markowitz Pivot Selection for LU Factorization

During basis factorization (LU decomposition), the choice of pivot elements affects both sparsity (how much fill-in the factorization produces) and numerical stability (how large the entries in L and U become). The Markowitz criterion selects the pivot that minimizes fill-in:

    Markowitz count = (r_i - 1) * (c_j - 1)

where r_i is the number of nonzeros in row i and c_j is the number of nonzeros in column j. Among candidates with acceptable Markowitz counts, threshold pivoting requires:

    |a_{ij}| >= u * max_k |a_{kj}|

where u is the Markowitz pivot tolerance (approximately 7.8e-3, i.e., 1/128; see P5.3, Section 4). This ensures the pivot element is at least a fraction u of the largest element in the pivot column. The value 1/128 is a power-of-two fraction, which avoids introducing rounding error in the threshold comparison itself.

### Growth Factor Monitoring

The growth factor of an LU factorization measures how much the matrix entries grow during elimination:

    rho = max_{ij} |U_{ij}| / max_{ij} |A_{ij}|

A large growth factor (rho >> 1) indicates that the factorization has amplified matrix entries, which degrades the accuracy of subsequent FTRAN/BTRAN operations. For a well-conditioned basis with threshold pivoting, the growth factor is typically moderate (single digits to low hundreds). A growth factor exceeding approximately 1e8 to 1e10 signals potential numerical problems.

Monitoring the growth factor after each refactorization provides an early warning of ill-conditioning. If the growth factor is unacceptably large, the solver may:
- Increase the Markowitz pivot tolerance u (accepting more fill-in for better stability).
- Apply additional scaling to the constraint matrix.
- Report a numerical difficulty warning to the user.

### References

- Markowitz, H.M. (1957). "The elimination form of the inverse and its application to linear programming." *Management Science*, 3(3):255--269.
- Suhl, U.H. and Suhl, L.M. (1990). "Computing sparse LU factorizations for large-scale linear programming bases." *ORSA Journal on Computing*, 2(4):325--335.
- Harris, P.M.J. (1973). "Pivot selection methods of the Devex LP code." *Mathematical Programming*, 5(1):1--28.
- Maros, I. (2003). *Computational Techniques of the Simplex Method.* Springer, Section 9.3.

---

## E. Tolerance Management

### The Tolerance Hierarchy

The LP solver uses a hierarchy of tolerances that serve different purposes. It is critical that these tolerances are consistent with each other; inconsistencies can cause the solver to oscillate between declaring a solution feasible and infeasible. The full tolerance catalog is in P5.3 (tolerances_constants.md). Here we describe the relationships and consistency requirements.

**Feasibility tolerance (epsilon_feas, typically 1e-6).** A constraint a'x <= b is satisfied if a'x - b <= epsilon_feas. This is the primary tolerance for determining solution quality. It is used in the Harris ratio test (P2.4, harris_ratio_test.md) as the relaxation parameter in Pass 1, ensuring that the ratio test and the feasibility checker use the same standard. All other tolerances must be set in relation to this one.

**Optimality tolerance (epsilon_opt, typically 1e-6).** A reduced cost is considered zero if its absolute value is at most epsilon_opt. This determines when the solver declares optimality. Setting epsilon_opt > epsilon_feas can cause the solver to accept solutions that are feasible but not truly optimal; setting epsilon_opt << epsilon_feas is wasteful because the reduced cost accuracy is limited by the feasibility of the basis.

**Pivot tolerance (epsilon_piv, typically 1e-9).** The minimum acceptable pivot element magnitude. This must be significantly smaller than the feasibility tolerance: if epsilon_piv were close to epsilon_feas, the error introduced by a single marginal pivot could immediately violate feasibility. A ratio of epsilon_feas / epsilon_piv of approximately 1e3 provides a safety margin of about three orders of magnitude.

**Bound equality tolerance (epsilon_bnd, typically 1e-10).** Variables with ub - lb <= epsilon_bnd are treated as fixed. This must be smaller than the pivot tolerance to avoid treating variables as non-fixed in the ratio test while their slack is below the pivot tolerance.

**Minimum pivot threshold (approximately 1e-13).** Used in bound propagation. This is much smaller than the Harris pivot tolerance because bound propagation operations are conservative (they widen rather than tighten bounds on failure) and do not commit to a basis exchange.

The ordering epsilon_bnd < epsilon_piv < epsilon_feas = epsilon_opt ensures consistency:
- A variable declared non-fixed (ub - lb > epsilon_bnd) has enough slack to produce a meaningful ratio in the ratio test.
- A pivot accepted by the ratio test (|d_r| >= epsilon_piv) is large enough that the resulting eta vector does not amplify errors beyond the feasibility tolerance.
- A reduced cost declared non-zero (|c_bar_j| > epsilon_opt) represents a genuine improvement opportunity, not noise.

### Multi-Level Tolerance Strategy for Pricing

The pricing subsystem uses an adaptive tolerance that varies across the solve (see P5.3, Section 4, and P3.20, simplex_iteration.md, cxf_simplex_step Phase 1). The three pricing phases -- fast, standard, and aggressive -- use progressively different reduced cost thresholds:

- **Fast phase (early iterations):** A loose threshold (~1e-6) accepts more entering candidates, enabling rapid progress. Numerical precision is less critical because the solution is far from optimal and minor pricing errors are self-correcting.
- **Standard phase (fallback):** A very tight threshold (~1e-10) is used when the solver encounters difficulties. This prevents false candidates caused by reduced cost drift.
- **Aggressive phase (near optimality):** A tight threshold (~1e-9) ensures that the final optimality declaration is accurate. Only genuinely improving variables are selected.

This multi-level approach follows the recommendation of Maros (2003, Section 7.5): use loose tolerances for speed when accuracy is not critical, and tighten when approaching the optimal solution.

### User-Adjustable Tolerances

The feasibility and optimality tolerances are typically user-adjustable within a documented range (e.g., [1e-9, 1e-2] for both). When the user modifies these tolerances, the implementation must propagate the change to all dependent internal thresholds:
- The Harris ratio test relaxation parameter must match the feasibility tolerance.
- The pricing threshold must be scaled relative to the optimality tolerance.
- The stagnation detection threshold (see P3.20, cxf_simplex_post_iterate) must use the optimality tolerance.

Failure to propagate tolerance changes leads to inconsistent behavior where, for example, the ratio test accepts steps that the feasibility checker rejects.

### References

- Maros, I. (2003). *Computational Techniques of the Simplex Method.* Springer, Chapters 6, 7, and 8.
- ConvexFeld Optimizer Reference Manual, "Tolerances and User-Scaling."

---

## F. Common Numerical Pitfalls

### F.1 Problem Scaling

Extreme coefficient ranges in the constraint matrix are the single most common cause of numerical difficulties in LP solving. A well-scaled model has all nonzero coefficients, right-hand sides, bounds, and objective coefficients within a moderate range (e.g., [1e-3, 1e3]). When the range exceeds approximately six orders of magnitude (e.g., coefficients ranging from 1e-6 to 1e6), the solver's fixed tolerances (which are absolute, not relative) become inappropriate for some part of the problem.

**Why scaling matters.** The feasibility tolerance epsilon_feas = 1e-6 is absolute. For a constraint with coefficients of magnitude 1e6, a violation of 1e-6 represents a relative error of 1e-12, which is excellent. But for a constraint with coefficients of magnitude 1e-6, the same violation of 1e-6 represents a relative error of 100%, which is meaningless.

**Scaling strategies.** The solver should apply equilibration scaling to the constraint matrix before optimization (see P5.3, Section 8, for scaling constants):
- **Ruiz equilibration:** Iteratively scales rows and columns to have unit infinity-norm. Typically 9--10 iterations suffice (Ruiz, 2001).
- **Geometric mean scaling:** Scales each row and column by the geometric mean of its absolute nonzero entries (Curtis and Reid, 1972).
- **Power-of-two rounding:** After computing scaling factors, round them to the nearest power of 2. IEEE 754 multiplication by a power of 2 is exact (introduces zero rounding error), so the scaling operation itself is numerically lossless.

**Scaling factor bounds.** Scaling factors should be clamped to [1e-6, 1e6] (see P5.3, Section 8) to prevent overly aggressive scaling from amplifying originally moderate coefficients.

### F.2 Degeneracy and Stalling

A degenerate LP has one or more basic variables at their bounds, so the simplex step length is zero. Degeneracy causes:
- **Stalling:** The solver performs many iterations without objective improvement.
- **Cycling (in exact arithmetic):** The solver may revisit the same basis repeatedly, looping indefinitely.
- **Wasted computation:** Each degenerate pivot produces an eta vector and degrades the basis inverse without making progress.

**Perturbation.** The standard mitigation is to perturb variable bounds slightly to break exact degeneracy. The EXPAND procedure (Gill, Murray, Saunders, and Wright, 1989) and the implied-bound perturbation approach (see P5.3, Section 7, and the perturbation module P3.21) apply small perturbations clamped to [1e-10, 1e-6]. After the solver converges on the perturbed problem, a cleanup phase (simplex_cleanup, P3.21) solves the original (unperturbed) problem starting from the perturbed solution. This typically requires only a few additional iterations.

**Stall detection.** The simplex iteration module (P3.20, cxf_simplex_post_iterate) monitors progress by comparing the number of variable and constraint eliminations against dimension-scaled thresholds. If insufficient progress is made over a refactorization interval, the solver switches strategies (e.g., from primal to dual simplex, or activates perturbation). See P3.20 for the threshold formulas.

### F.3 Dense Columns

Variables that appear in many constraints (high-degree columns) cause problems for the LU factorization:
- **Fill-in:** Pivoting on a dense column can turn a sparse factorization dense, dramatically increasing memory use and per-iteration cost.
- **Eta vector bloat:** The eta vector for a pivot involving a dense column has many nonzero entries, making subsequent FTRAN and BTRAN operations expensive.

**Mitigation.** Production simplex implementations handle dense columns by:
- Avoiding them as pivot candidates when sparser alternatives exist (the Markowitz criterion naturally favors sparser pivots).
- Treating columns with more than a threshold number of nonzeros (e.g., sqrt(m) or m/10) as "dense" and handling them separately in the factorization.
- Triggering refactorization sooner when dense pivots occur, because the error amplification per eta vector is larger.

### F.4 Long Eta Chains

Even with well-conditioned pivots, the accumulated effect of applying many eta vectors introduces numerical drift. After k eta vectors:

- The computational cost of FTRAN and BTRAN grows as O(k * average eta density).
- The accumulated rounding error grows roughly as O(k * epsilon_machine * condition(B)), where condition(B) is the condition number of the current basis.
- The effective sparsity of B^{-1} may decrease (fill-in in the implicit inverse) even when individual eta vectors are sparse.

The solution is periodic refactorization, as described in Section A. The refactorization interval should be set so that the accumulated error remains well below the feasibility tolerance between refactorizations.

### F.5 Ill-Conditioned Bases

The condition number kappa(B) of the basis matrix measures how sensitive the solution of Bx = a is to perturbations in B or a. A basis with kappa(B) close to 1 is well-conditioned; a basis with kappa(B) exceeding approximately 1e12 to 1e14 is ill-conditioned for double-precision arithmetic (because the solution accuracy is roughly epsilon_machine * kappa(B)).

Ill-conditioned bases arise from:
- Poor scaling (see F.1).
- Near-parallel constraints (two constraints that are almost identical but not quite).
- Variables with vastly different bound ranges in the same basis.

The solver detects ill-conditioning indirectly through:
- Large residuals after FTRAN (Section A).
- Small pivot elements in the ratio test (Section D).
- Large growth factors in the LU factorization (Section D).
- Inconsistent optimality declarations (a variable appears optimal, but after refactorization, it no longer is).

When ill-conditioning is detected, the solver should:
1. Refactorize the basis.
2. Recompute all reduced costs from scratch.
3. If the problem persists, tighten pivot tolerances (accepting more fill-in for better stability).
4. As a last resort, report a numerical difficulty status to the user, advising them to improve model scaling.

### F.6 Catastrophic Cancellation in Specific Operations

Beyond the general discussion in Section B, certain operations are particularly vulnerable:

**Objective value tracking.** The objective value is updated incrementally at each pivot: z_new = z_old + theta * delta_c. After thousands of pivots, the accumulated objective may have lost many significant digits. The solver should periodically recompute the objective from scratch: z = c_B^T * x_B, using the fresh FTRAN result after refactorization.

**Reduced cost updates.** After a pivot, reduced costs are updated via: c_bar_j_new = c_bar_j_old - (c_bar_q / alpha) * d_j, where c_bar_q is the entering variable's reduced cost, alpha is the pivot element, and d_j is the updated column entry for variable j. When c_bar_j_old and the update term are nearly equal, the new reduced cost may have few significant digits. The solver should monitor for reduced costs that change sign after refactorization (indicating that the pre-refactorization value was numerically unreliable) and consider these as potential sources of incorrect pricing decisions.

### References

- Curtis, A.R. and Reid, J.K. (1972). "On the automatic scaling of matrices for Gaussian elimination." *IMA Journal of Applied Mathematics*, 10(1):118--124.
- Gill, P.E., Murray, W., Saunders, M.A., and Wright, M.H. (1989). "A practical anti-cycling procedure for linearly constrained optimization." *Mathematical Programming*, 45(1--3):437--474.
- Gill, P.E., Murray, W., Saunders, M.A., and Wright, M.H. (1991). "Numerical Linear Algebra and Optimization." Vol. 1. Addison-Wesley.
- Higham, N.J. (2002). *Accuracy and Stability of Numerical Algorithms.* 2nd ed. SIAM.
- Maros, I. (2003). *Computational Techniques of the Simplex Method.* Springer, Chapters 6, 8, and 9.
- Ruiz, D. (2001). "A scaling algorithm to equilibrate both rows and columns norms in matrices." Technical Report RAL-TR-2001-034, Rutherford Appleton Laboratory.

---

## Summary: Numerical Stability Checklist for Implementers

The following checklist summarizes the key practices. An implementation that follows all of these practices should achieve solution accuracy comparable to production LP solvers.

| Practice | Section | Priority |
|----------|---------|----------|
| Periodic basis refactorization with adaptive interval | A | Critical |
| Residual monitoring after FTRAN to detect drift | A | Critical |
| Conservative rounding on activity bound updates | B | High |
| NaN/Inf detection at key checkpoints | C | High |
| Minimum pivot element threshold in ratio test | D | Critical |
| Markowitz threshold pivoting in LU factorization | D | Critical |
| Consistent tolerance hierarchy (epsilon_bnd < epsilon_piv < epsilon_feas) | E | Critical |
| Adaptive pricing tolerance across solve phases | E | High |
| Matrix equilibration scaling before optimization | F.1 | High |
| Bound perturbation for anti-degeneracy | F.2 | High |
| Stall detection with dimension-scaled thresholds | F.2 | Medium |
| Objective and reduced cost recomputation after refactorization | F.6 | High |
| Growth factor monitoring after LU factorization | D | Medium |
| Compensated summation for long accumulations | B | Medium |
| Step length clamping in ratio test | C | Medium |

---

## Cross-References to Other Specification Documents

| Topic | Document | Section |
|-------|----------|---------|
| Tolerance values and ranges | P5.3 tolerances_constants.md | All sections |
| PFI algorithm and refactorization | P2.1 product_form_inverse.md | Steps 5--6, Numerical Considerations |
| Harris ratio test and pivot selection | P2.4 harris_ratio_test.md | Numerical Considerations |
| Stall and stagnation detection | P3.20 simplex_iteration.md | cxf_simplex_post_iterate |
| Cancellation detection in activity updates | P3.19 pivot_operations.md | cxf_pivot_update |
| Perturbation for anti-cycling | P3.21 simplex_phases.md | cxf_simplex_perturbation |
| Scaling constants and modes | P5.3 tolerances_constants.md | Section 8 |

---

## References

- Bartels, R.H. and Golub, G.H. (1969). "The simplex method of linear programming using LU decomposition." *Communications of the ACM*, 12(5):266--268.
- Curtis, A.R. and Reid, J.K. (1972). "On the automatic scaling of matrices for Gaussian elimination." *IMA Journal of Applied Mathematics*, 10(1):118--124.
- Dantzig, G.B. (1963). *Linear Programming and Extensions.* Princeton University Press.
- Gill, P.E., Murray, W., Saunders, M.A., and Wright, M.H. (1989). "A practical anti-cycling procedure for linearly constrained optimization." *Mathematical Programming*, 45(1--3):437--474.
- Gill, P.E., Murray, W., Saunders, M.A., and Wright, M.H. (1991). *Numerical Linear Algebra and Optimization.* Vol. 1. Addison-Wesley.
- Harris, P.M.J. (1973). "Pivot selection methods of the Devex LP code." *Mathematical Programming*, 5(1):1--28.
- Higham, N.J. (2002). *Accuracy and Stability of Numerical Algorithms.* 2nd ed. SIAM.
- Kahan, W. (1965). "Pracniques: further remarks on reducing truncation errors." *Communications of the ACM*, 8(1):40.
- Maros, I. (2003). *Computational Techniques of the Simplex Method.* Springer. International Series in Operations Research and Management Science, Vol. 61.
- Markowitz, H.M. (1957). "The elimination form of the inverse and its application to linear programming." *Management Science*, 3(3):255--269.
- Reid, J.K. (1982). "A sparsity-exploiting variant of the Bartels-Golub decomposition for linear programming bases." *Mathematical Programming*, 24(1):55--69.
- Ruiz, D. (2001). "A scaling algorithm to equilibrate both rows and columns norms in matrices." Technical Report RAL-TR-2001-034, Rutherford Appleton Laboratory.
- Suhl, U.H. and Suhl, L.M. (1990). "Computing sparse LU factorizations for large-scale linear programming bases." *ORSA Journal on Computing*, 2(4):325--335.
- [ConvexFeld Optimizer Reference Manual -- Tolerances and User-Scaling](https://docs.convexfeld.com/projects/optimizer/en/current/concepts/numericguide/tolerances_scaling.html)

---

## Verification Checklist

```
[x] No hex addresses or offsets
[x] No Ghidra artifacts (param_, local_, iVar, DAT_, FUN_)
[x] No binary-specific constants
[x] No copied code fragments
[x] All content references published literature or standard practices
[x] All tolerance values reference P5.3 or published sources
[x] All data structures described semantically
[x] Passes the Clean Room Test (Rule 10)
```
