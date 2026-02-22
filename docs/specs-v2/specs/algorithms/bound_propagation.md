# Simplex-Integrated Bound Propagation

## Published Reference

- **Primary:** Savelsbergh, M.W.P. (1994). "Preprocessing and probing techniques for mixed integer programming problems." *ORSA Journal on Computing*, 6(4):445-454. Sections 2.1-2.2 describe the bound-tightening procedure for individual constraints based on activity analysis.
- **Foundational:** Brearley, A.L., Mitra, G., and Williams, H.P. (1975). "Analysis of mathematical programming problems prior to applying the simplex algorithm." *Mathematical Programming*, 8(1):54-83. Introduces the concept of deriving variable bounds from constraint structure.
- **Presolve reductions:** Achterberg, T., Bixby, R.E., Gu, Z., Rothberg, E., and Weninger, D. (2020). "Presolve reductions in mixed integer programming." *INFORMS Journal on Computing*, 32(2):473-506. Comprehensive treatment of bound tightening and other presolve techniques in modern LP solvers, including their application during solving.
- **Convergence theory:** Belotti, P., Cafieri, S., Lee, J., and Liberti, L. (2010). "Feasibility-based bounds tightening via fixed points." In *Combinatorial Optimization and Applications (COCOA 2010)*, Lecture Notes in Computer Science, vol. 6508, pp. 65-76, Springer. Characterizes the fixed-point behavior of FBBT on linear constraints and shows that convergence to the limit may not be finite.
- **Bound flipping ratio test:** Forrest, J.J.H. and Goldfarb, D. (1992). "Steepest-edge simplex algorithms for linear programming." *Mathematical Programming*, 57(1):341-374. The bound-flipping technique that motivates per-iteration bound propagation during simplex.
- **Dual simplex bound propagation:** Koberstein, A. (2005). *The dual simplex method, techniques for a fast and stable implementation.* PhD Thesis, University of Paderborn. Discusses integration of bound tightening within the dual simplex iteration loop.
- **Compensated summation:** Kahan, W. (1965). "Pracniques: Further remarks on reducing truncation errors." *Communications of the ACM*, 8(1):40. The error-detection technique used in incremental activity updates.
- **Textbook treatment:** Maros, I. (2003). *Computational Techniques of the Simplex Method*. Springer. Chapters 7-12 discuss pricing, ratio testing, and bound management in practical simplex implementations.

The algorithm described here extends the classical FBBT (Feasibility-Based Bound Tightening) procedure into a simplex-integrated, pricing-driven bound propagation system. Rather than operating as a standalone preprocessing pass, bound propagation runs at every simplex iteration as a bidirectional tightening step, using the pricing subsystem to identify which variables and constraints to examine.

---

## Purpose

Bound propagation tightens variable bounds by analyzing the structure of linear constraints. Given a constraint system Ax <= b (or Ax = b, Ax >= b) and initial variable bounds, the algorithm derives tighter bounds on individual variables by computing what values each variable can take while still satisfying each constraint, given the bounds on all other variables. This serves four purposes within the simplex solver:

1. **Per-iteration domain reduction:** After each simplex pivot, pricing-selected variables and constraints are examined for implied bound improvements. Tighter bounds reduce the feasible region and can accelerate convergence by eliminating regions of the polytope that cannot contain the optimum.

2. **Infeasibility detection:** If propagation produces a lower bound that exceeds the corresponding upper bound (beyond tolerance), the current simplex state is proven infeasible. This provides an early termination signal.

3. **Variable fixing:** When propagation forces the lower and upper bounds of a variable to become equal (within tolerance), the variable is fixed. This reduces the effective problem dimension for subsequent iterations.

4. **Eta chain maintenance:** Bound changes are recorded as lightweight eta records in the Product Form of the Inverse chain (see P1.04, EtaVector). These records enable later basis reconstruction during crossover or warm-start operations.

In the LP solver, bound propagation appears in two modes:

- **Simplex-integrated mode (primary):** Two complementary functions run at every simplex iteration -- one examining variable-side candidates and one examining constraint-side candidates. These are driven by the pricing subsystem and make a single pass per iteration.

- **Cleanup mode (secondary):** A standalone iterative FBBT procedure runs after the simplex algorithm terminates, using a worklist-driven strategy with multiple passes. This is described in Section 6 (Cleanup Bound Propagation).

---

## Inputs

### Common Inputs (Both Modes)

| Input | Type | Description | Preconditions |
|-------|------|-------------|---------------|
| A | sparse matrix (CSR + CSC) | Constraint coefficient matrix with m rows and n columns | Stored in both row-major and column-major sparse format; no NaN or infinity in coefficients |
| l | array of doubles [n] | Current lower bounds on variables | l_j <= u_j for all j; negative infinity permitted |
| u | array of doubles [n] | Current upper bounds on variables | u_j >= l_j for all j; positive infinity permitted |
| s | array of chars [m] | Constraint senses | Each entry is '<' (less-than-or-equal), '=' (equality), or '>' (greater-than-or-equal) |
| L_act | array of doubles [m] | Lower activity bounds for each constraint | See Preliminary Definitions |
| U_act | array of doubles [m] | Upper activity bounds for each constraint | See Preliminary Definitions |
| p_pos | array of ints [m] | Count of positive-coefficient unbounded-above variables per constraint | Non-negative |
| p_neg | array of ints [m] | Count of negative-coefficient unbounded-below variables per constraint | Non-negative |
| epsilon_feas | double | Feasibility tolerance | Positive; typically 1e-9 to 1e-6 |
| M | double | Infinity threshold | Positive; typically 1e+20 |

### Simplex-Integrated Mode Additional Inputs

| Input | Type | Description | Preconditions |
|-------|------|-------------|---------------|
| pricingCandidates | array of ints [k] | Variable or constraint indices selected by pricing | Populated by the pricing subsystem for the current iteration |
| candidateStatus | array of ints [n or m] | Status codes for candidate eligibility | Encodes which candidates the pricing system has flagged for propagation |
| primalValues | array of doubles [m] | Current primal activity for each constraint row | Maintained by the simplex iteration |
| varStatus | array of ints [n] | Variable status in current basis | Non-negative = basic/active, negative = non-basic/fixed |

### Cleanup Mode Additional Inputs

| Input | Type | Description | Preconditions |
|-------|------|-------------|---------------|
| basisHeader | array of ints [m] | Maps each constraint to its basic variable | Non-negative entries indicate active constraints |
| varStatus | array of ints [n] | Basis status of each variable | Standard simplex encoding |

---

## Outputs

| Output | Type | Description | Postconditions |
|--------|------|-------------|----------------|
| l' | array of doubles [n] | Tightened lower bounds (modified in-place) | l'_j >= l_j for all j |
| u' | array of doubles [n] | Tightened upper bounds (modified in-place) | u'_j <= u_j for all j |
| L_act' | array of doubles [m] | Updated lower activity bounds (modified in-place) | Consistent with l' and u' |
| U_act' | array of doubles [m] | Updated upper activity bounds (modified in-place) | Consistent with l' and u' |
| p_pos' | array of ints [m] | Updated unbounded-above counts (modified in-place) | p_pos'_i <= p_pos_i |
| p_neg' | array of ints [m] | Updated unbounded-below counts (modified in-place) | p_neg'_i <= p_neg_i |
| etaRecords | list of eta vectors | Bound-change records appended to the eta chain | Present only in simplex-integrated mode when eta tracking is active |
| status | integer | Return code | 0 = success, INFEASIBLE = bounds crossed, OUT_OF_MEMORY = allocation failure |
| infeasVariable | integer | Index of the variable whose bounds crossed (if INFEASIBLE) | Valid variable index, or unset if status = 0 |
| infeasConstraint | integer | Index of the constraint that caused infeasibility (if INFEASIBLE) | Valid constraint index, or unset if status = 0 |

**Key postcondition:** If status = 0, then l'_j <= u'_j for all j, and every tightened bound is at least as tight as the original bound.

---

## Algorithm Description

### Preliminary Definitions

**Activity bounds** for constraint i are defined as follows. Let N_i denote the set of variables with nonzero coefficient a_{ij} in constraint i.

The **upper activity** U_act_i (also called maximum activity) measures the largest value the left-hand side of constraint i can achieve:

    U_act_i = sum over j in N_i of:
        a_{ij} * u_j   if a_{ij} > 0
        a_{ij} * l_j   if a_{ij} < 0

If any variable contributing to this sum has an infinite bound (u_j = +infinity for a_{ij} > 0, or l_j = -infinity for a_{ij} < 0), the contribution is infinite and is tracked via the unbounded count arrays (p_pos for this case, p_neg for the lower activity case) rather than included in the finite sum.

The **lower activity** L_act_i (also called minimum activity) is defined symmetrically:

    L_act_i = sum over j in N_i of:
        a_{ij} * l_j   if a_{ij} > 0
        a_{ij} * u_j   if a_{ij} < 0

**Unbounded counts:** p_pos_i counts the number of terms in U_act_i that are infinite (variable with positive coefficient unbounded above, or variable with negative coefficient unbounded below). p_neg_i counts infinite terms in L_act_i. The finite sums L_act_i and U_act_i only include contributions from finitely-bounded variables.

**Violation flag encoding.** Both modes of bound propagation classify the propagation outcome for each candidate using a bit-field encoding:

| Value | Name | Meaning |
|-------|------|---------|
| 0 | NONE | No tightening needed; implied value within current bounds |
| 1 | LOWER | Lower bound should be tightened (bit 0) |
| 2 | UPPER | Upper bound should be tightened (bit 1) |
| 3 | BOTH | Both bounds tightened; variable is effectively fixed (bits 0 + 1) |
| 4 | INFEASIBLE | Implied value contradicts existing bounds beyond tolerance |

When the candidate variable is an integer or piecewise-linear variable, an additional flag (value 8) is added to the violation code to signal downstream processing that integrality considerations apply.

### Overview

The simplex-integrated bound propagation system consists of two complementary single-pass functions that run in sequence at every simplex iteration, after the main pivot step:

1. **Variable-side propagation** examines pricing-selected variable candidates. For each candidate, it computes a ratio of the current primal activity to the pivot coefficient and determines whether the variable's bounds should be tightened based on this ratio's relationship to the variable's current bounds. This is conceptually a bound-flipping analysis: it asks "given the current simplex state, which variables can have their domains narrowed?"

2. **Constraint-side propagation** examines pricing-selected constraint candidates. For each constraint, it computes an implied value for the pivot variable from the constraint's activity and determines whether the variable's bounds can be tightened. This is the classical implied-bound technique from Savelsbergh (1994), applied per-iteration to the pricing-selected subset rather than to all constraints.

Together, these two functions implement a bidirectional propagation pass: variables inform constraints (step 1), and constraints inform variables (step 2). This is a single iteration of what, in a standalone context, would be an iterative FBBT loop. By running one such iteration per simplex step, the solver achieves incremental bound tightening without the overhead of a full propagation pass.

---

### Section 1: Variable-Side Propagation

This function processes variable candidates from the pricing subsystem's secondary queue. These are variables that were identified during the main simplex step as potentially benefiting from bound adjustment but were deferred for separate processing.

#### Step 1.1: Candidate Retrieval

Obtain the list of variable candidates from the pricing subsystem. If the list is empty, return immediately with success.

#### Step 1.2: Per-Candidate Processing

For each candidate variable with eligible pricing status:

**1.2a: Pivot element search.** Scan the candidate's constraint row (in row-major sparse format) to find the first active variable with a sufficiently large coefficient. Skip column entries that are marked as inactive (column index below zero) or whose variable has no active participation (variable count below zero). This search identifies the pivot element for the ratio computation.

**1.2b: Pivot threshold check.** If the absolute value of the pivot coefficient is below a tight numerical threshold (on the order of 1e-13), skip the candidate to avoid numerically unstable division.

**1.2c: Ratio computation.** Compute the ratio:

    ratio = primalActivity / pivotCoefficient

where primalActivity is the current primal value at the candidate's row and pivotCoefficient is the coefficient found in step 1.2a. This ratio represents the implied position of the pivot variable relative to the constraint.

**1.2d: Flip-type classification.** Based on the constraint sense and the sign of the pivot coefficient, classify the action needed:

- **Equality constraint:** Both bound directions are checked independently.
  - If ratio < lowerBound - epsilon_feas: classify as INFEASIBLE.
  - If ratio > upperBound + epsilon_feas: classify as INFEASIBLE.
  - Otherwise, set the LOWER bit if ratio > lowerBound + epsilon_tight, and set the UPPER bit if ratio < upperBound - epsilon_tight. Here epsilon_tight is a tighter tolerance (on the order of 1e-10) that prevents trivially small tightenings.

- **Inequality constraint with non-positive pivot coefficient:** Only the lower bound direction is relevant. If the ratio exceeds the lower bound plus epsilon_tight, check whether it also exceeds the upper bound plus epsilon_feas (INFEASIBLE) or is within range (LOWER flag set).

- **Inequality constraint with positive pivot coefficient:** Only the upper bound direction is relevant. If the ratio is below the upper bound minus epsilon_tight, check whether it also falls below the lower bound minus epsilon_feas (INFEASIBLE) or is within range (UPPER flag set).

**1.2e: Bound clamping.** If the ratio falls outside the variable's current bounds (but not beyond the infeasibility threshold), apply tolerance-based corrections:

- If tightening toward the upper bound and the ratio exceeds it: adjust the primal activity by the feasibility tolerance (in the appropriate direction given the pivot sign), recompute the ratio, and clamp to the upper bound if the adjusted ratio is still within range.

- Symmetrically for tightening toward the lower bound.

This clamping ensures that updated bounds remain within the feasible region, accounting for floating-point imprecision in the ratio computation.

**1.2f: Infeasibility confirmation.** If the classification is INFEASIBLE, apply a two-stage confirmation procedure before returning:

- **Stage 1:** The ratio-based check identified a potential infeasibility.
- **Stage 2:** Cross-check against the constraint's activity bounds (for inequality constraints: verify that the maximum activity confirms the violation; for equality constraints: verify that both activity directions confirm it).

Only if both stages agree is the infeasibility code returned with diagnostic indices. If confirmation fails, the candidate entry is restored and processing continues. This conservative approach prevents false infeasibility alarms caused by accumulated numerical noise.

**1.2g: Bound-change eta record.** If the violation flags are nonzero and eta tracking is active (the solver is in full tracking mode, not simplified mode), create a lightweight bound-change eta record containing:

- The variable index and constraint index
- The violation classification flags (with additional integer/piecewise-linear flag if applicable)
- The pivot coefficient and the computed ratio

This record is prepended to the eta vector chain for later basis reconstruction. The bound-change eta record is structurally distinct from the standard pivot eta records described in P2.2 (Product Form of the Inverse); it is smaller and records a bound tightening rather than a basis exchange. See P1.04 (EtaVector) for the full data model.

**1.2h: Bound update and propagation.** Compute new bounds based on the violation flags:

- If the LOWER bit is set: the new lower bound is the minimum of the ratio and the current upper bound.
- If the UPPER bit is set: the new upper bound is the maximum of the ratio and the new lower bound.

After computing new bounds, call the incremental activity update function (described in Section 3) to propagate the bound change to all constraints containing this variable. Then write the new bounds to the working bound arrays.

**1.2i: Pricing notification.** Mark the variable as processed in the pricing subsystem and update the variable's status to reflect that it is no longer an active candidate.

**1.2j: Variable fixing.** If both the LOWER and UPPER bits are set (BOTH classification), the variable is effectively fixed. Invoke the variable-fixing procedure (P3.19, cxf_pivot_bound) to fully remove the variable from the active problem. If the fixing procedure returns an error, propagate it immediately.

#### Step 1.3: Counter Update

Increment the solver state's bound-change counter by the number of candidates successfully processed. This counter is used for progress monitoring and stall detection.

---

### Section 2: Constraint-Side Propagation

This function processes constraint candidates from the pricing subsystem's constraint queue. These are constraints that the pricing system has identified as potentially informative for bound tightening -- typically constraints whose activity has changed significantly due to recent pivots.

This function is invoked only for LP problems. For quadratic programs, constraint-side propagation is skipped because nonlinear objective terms invalidate the linear activity-bound inference.

#### Step 2.1: Candidate Retrieval

Obtain the list of constraint candidates from the pricing subsystem's constraint queue. If the list is empty, return immediately with success.

#### Step 2.2: Per-Constraint Processing

For each candidate constraint with eligible propagation status:

**2.2a: Pivot element search.** Scan the constraint's row in the CSR matrix to find the first variable with non-negative status (an active, unfixed variable). This identifies the pivot variable whose bounds may be tightened by the constraint.

**2.2b: Pivot threshold check.** If the absolute value of the constraint coefficient at the pivot position is below a numerical threshold (on the order of 1e-8), skip the constraint.

**2.2c: Implied value computation.** Compute the implied value:

    impliedValue = constraintActivity / pivotCoefficient

where constraintActivity is the current activity (residual) of the constraint and pivotCoefficient is the coefficient found in step 2.2a. This represents what value the pivot variable must take for the constraint to be satisfied, given the current activity from all other variables.

This is a simplified form of the implied bound derivation described in the Preliminary Definitions. Rather than computing the full Savelsbergh (1994) formula with explicit removal of the pivot variable's contribution from the activity, the simplex-integrated version uses the constraint's current activity directly. This is valid because the activity arrays are maintained incrementally by the simplex solver and already reflect the contributions of all variables in their current state.

**2.2d: Violation classification.** Based on the constraint sense and pivot coefficient sign, classify the implication:

- **Equality constraint:** If the implied value falls outside the variable's bounds (below lower - epsilon_feas or above upper + epsilon_feas), classify as INFEASIBLE. Otherwise, set the LOWER bit if implied value exceeds the lower bound by more than epsilon_tight, and set the UPPER bit if implied value is below the upper bound by more than epsilon_tight.

- **Inequality constraint with non-positive coefficient:** For a negative coefficient in a less-than constraint, a decrease in the variable allows the constraint activity to increase. If the implied value exceeds the lower bound by more than epsilon_tight, check for infeasibility (implied value above upper + epsilon_feas) or set the LOWER flag.

- **Inequality constraint with positive coefficient:** For a positive coefficient in a less-than constraint, an increase in the variable increases the activity. If the implied value is below the upper bound by more than epsilon_tight, check for infeasibility (implied value below lower - epsilon_feas) or set the UPPER flag.

**2.2e: Bound clamping.** If the implied value exceeds the variable's current bounds but is not beyond the infeasibility threshold, clamp the implied value to the bound, adjusting for the pivot coefficient sign and feasibility tolerance. This mirrors the clamping procedure in Section 1 (step 1.2e).

**2.2f: Infeasibility confirmation.** Apply the same two-stage confirmation as in the variable-side propagation:

- **Stage 1:** The implied value violates bounds.
- **Stage 2:** Verify against the constraint's minimum and maximum activity bounds. For inequality constraints: the maximum activity must confirm the violation. For equality constraints: both activity directions must agree.

If confirmation fails, restore the candidate entry and continue.

**2.2g: Bound-change eta record.** Create a lightweight eta record (same format as the variable-side records in step 1.2g) storing the variable index, constraint index, violation flags, coefficient, and implied value.

**2.2h: Bound update and propagation.** Compute new bounds:

- If the LOWER bit is set: the new lower bound is the minimum of the implied value and the current upper bound.
- If the UPPER bit is set: the new upper bound is the maximum of the implied value and the new lower bound.

Call the incremental activity update function (Section 3) to propagate the change, then write new bounds.

**2.2i: Pricing notification and variable fixing.** Same as steps 1.2i and 1.2j -- mark the variable as processed, update the status, and invoke the fixing procedure if both bounds are tight.

#### Step 2.3: Counter Update

Increment the propagation counter on the solver state.

---

### Section 3: Incremental Activity Update

When a variable's bounds change (in either propagation direction), all constraints containing that variable must have their activity bounds updated. This is a shared utility used by both the variable-side and constraint-side propagation functions. It operates using the column-major (CSC) sparse representation of the constraint matrix.

The function receives the variable index, the old bounds, and the new bounds. It iterates over all nonzero entries in the variable's column and updates the affected constraint's activity bounds based on the coefficient sign and the nature of the bound change.

#### Step 3.1: Case Dispatch

The function first determines which bounds changed:
- If both lower and upper bounds changed: process both (Case A).
- If only the lower bound changed: process lower only (Case B).
- If only the upper bound changed: process upper only (Case C).
- If neither changed: return immediately (Case D).

#### Step 3.2: Activity Update Rules

For each constraint i containing the variable j with coefficient a_{ij}, and for each changed bound:

**Lower bound change (l_old -> l_new):**

- If a_{ij} > 0: The lower bound contributes to the *minimum* activity.
  - Remove old contribution: L_act_i -= a_{ij} * l_old (if l_old was finite).
  - Add new contribution: L_act_i += a_{ij} * l_new (if l_new is finite).
  - If l_old was infinite (below -M): decrement p_pos_i.
  - If l_new is infinite (below -M): increment p_pos_i.

- If a_{ij} < 0: The lower bound contributes to the *maximum* activity.
  - Remove old contribution: U_act_i -= a_{ij} * l_old (if l_old was finite).
  - Add new contribution: U_act_i += a_{ij} * l_new (if l_new is finite).
  - If l_old was infinite: decrement p_neg_i.
  - If l_new is infinite: increment p_neg_i.

**Upper bound change (u_old -> u_new):** Symmetric rules with the roles of minimum/maximum activity exchanged.

- If a_{ij} > 0: The upper bound contributes to the *maximum* activity. Update U_act_i accordingly, using p_neg_i for the unbounded count.

- If a_{ij} < 0: The upper bound contributes to the *minimum* activity. Update L_act_i accordingly, using p_pos_i for the unbounded count.

Only constraints with active basic variables (non-negative status for the constraint's row) are updated; slack constraints are skipped.

#### Step 3.3: Compensated Activity Update

When updating an activity sum S := S + delta, floating-point round-off can accumulate. The algorithm applies an error-detection heuristic inspired by Kahan (1965): after computing the sum, it checks whether the result is algebraically consistent with the inputs.

    S_new := S + delta
    if |S| < |delta|:
        if S_new - delta != S:
            S_new := S_new * rho
    else:
        if S_new - S != delta:
            S_new := S_new * rho

The correction factor rho is chosen based on the sign of S_new and whether the update is to the minimum or maximum activity:

- For **maximum activity** (U_act): rho is chosen so that the corrected value is nudged *away from zero* (upward for positive sums, toward zero for negative sums). This ensures the upper activity is never underestimated.

- For **minimum activity** (L_act): rho is chosen in the opposite direction, ensuring the lower activity is never overestimated.

The specific correction factors are slightly below 1 (for rounding down) and slightly above 1 (for rounding up). This is a lightweight alternative to full Kahan summation: rather than maintaining a separate compensation variable per constraint, it applies a one-time multiplicative correction when cancellation error is detected.

This conservative rounding direction ensures that all implied bounds derived from the corrected activities remain valid, at the cost of being slightly weaker than the true mathematical bound.

---

### Section 4: Integration with the Simplex Iteration Loop

The two propagation functions are called in a fixed sequence within each simplex iteration, after the main pivot step and before the post-iteration monitoring checks. The typical calling order within the solve driver is:

1. **cxf_simplex_step** (P3.20) -- the primary simplex pivot, including pricing candidate retrieval, Harris two-pass ratio test, bound flipping, and eta record creation.

2. **Variable-side propagation** (this algorithm, Section 1) -- processes the variable candidates deferred from the main step. Creates bound-change eta records.

3. **Constraint-side propagation** (this algorithm, Section 2) -- processes constraint candidates identified by the pricing system. Creates bound-change eta records. Skipped for quadratic programs.

This decomposition means that bound propagation is *pricing-driven*, not worklist-driven. The candidates examined in each iteration are determined by the multi-level partial pricing system (P2.3), not by a dedicated worklist that tracks which constraints have had their variables' bounds change. This is a fundamental structural difference from standalone FBBT:

| Aspect | Simplex-Integrated (Sections 1-2) | Standalone FBBT (Section 6) |
|--------|-----------------------------------|-----------------------------|
| Candidate selection | Pricing-driven | Worklist-driven (all active constraints) |
| Passes per invocation | Single pass | Up to K_max iterative passes |
| Invocation frequency | Every simplex iteration | Post-solve cleanup only |
| Scope | Pricing-selected subset | All active constraints |
| Eta record creation | Yes (bound-change records) | No |

The simplex-integrated approach sacrifices the thoroughness of iterative FBBT (which converges toward a fixed point) in exchange for low per-iteration overhead. Since bound propagation runs at every iteration, the cumulative effect over many iterations can approach or exceed the tightening achieved by a single standalone pass, while distributing the computational cost evenly across the solve.

---

### Section 5: Implied Bound Mathematics

This section documents the mathematical foundation underlying both modes of bound propagation.

#### The Implied Bound Derivation (Savelsbergh, 1994)

For a constraint sum_j(a_{ij} * x_j) <= b_i with known bounds on all variables, the implied bound on variable x_k is derived by isolating x_k:

    a_{ik} * x_k <= b_i - sum_{j != k}(a_{ij} * x_j)

The tightest bound on x_k comes from bounding the right-hand side as tightly as possible:

**If a_{ik} > 0 (upper bound derivation):**

    x_k <= (b_i - MinActivity_{excluding k}) / a_{ik}

where MinActivity_{excluding k} is the minimum value of sum_{j != k}(a_{ij} * x_j). Using the maintained activity arrays:

    MinActivity_{excluding k} = L_act_i - a_{ik} * l_k   (if a_{ik} > 0 and l_k is finite)

So the implied upper bound becomes:

    x_k <= (b_i - L_act_i + a_{ik} * l_k) / a_{ik}

This simplifies to:

    x_k <= l_k + (b_i - L_act_i) / a_{ik}

Since the activity arrays track the excess activity relative to the right-hand side (i.e., U_act_i represents the excess of the maximum activity above b_i, or equivalently, b_i - L_act_i represents the slack in the minimum activity), the implied bound can be computed efficiently from the maintained arrays.

**If a_{ik} < 0 (lower bound derivation):**

    x_k >= (b_i - MaxActivity_{excluding k}) / a_{ik}

The negative coefficient reverses the inequality direction.

**For equality constraints:** Both directions apply, yielding both an implied upper bound and an implied lower bound. The intersection of these with the current bounds gives the tightest result.

**Single-unbounded variable case:** When the unbounded count for a constraint is exactly 1 and the single unbounded variable is the one being examined, the activity sum over all other variables is finite, allowing a bound to be derived even though the variable itself is currently unbounded.

**Filtering tiny coefficients:** If |a_{ik}| is below a very small threshold (on the order of 1e-40), the variable is skipped for that constraint to avoid division by near-zero values producing meaninglessly large or small implied bounds.

#### Simplified Form in Simplex-Integrated Mode

In the simplex-integrated mode (Sections 1 and 2), the implied value computation is simplified:

    impliedValue = constraintActivity / pivotCoefficient

This is valid because the constraintActivity array, maintained by the simplex solver, already incorporates the contributions of all variables. The ratio impliedValue represents the value the pivot variable must take for the constraint to be satisfied at its current activity level. The classification into violation flags then determines whether this implied value is tighter than the current bounds.

---

### Section 6: Cleanup Bound Propagation

After the simplex algorithm terminates, a standalone iterative FBBT procedure runs as part of post-solve cleanup. This mode uses the classical worklist-driven approach described by Savelsbergh (1994), rather than the pricing-driven approach of the simplex-integrated mode.

#### Step 6.1: Initialize the Worklist

Allocate a circular queue Q of capacity n (number of variables) and a boolean membership array.

    for each constraint i from 0 to m-1:
        if constraint i is active (has a non-negative basis header entry):
            enqueue i into Q
            mark i as in-queue

#### Step 6.2: Main Propagation Loop

    iterationCount := 0
    epochEnd := tail of Q

    while Q is not empty:
        dequeue constraint i from Q
        mark i as not-in-queue

        // Constraint-level infeasibility check
        if constraint i has sense '<' or '=':
            if p_neg_i = 0 and U_act_i > epsilon_feas:
                return INFEASIBLE

        if constraint i has sense '>' or '=':
            if p_pos_i = 0 and L_act_i < -epsilon_feas:
                return INFEASIBLE

        // Derive implied bounds for each variable in constraint i
        for each nonzero coefficient a_{ij} in row i:
            if variable j is inactive (varStatus[j] < 0): skip
            if |a_{ij}| < epsilon_tiny: skip

            compute candidate bounds using the formulas in Section 5

            // Apply lower bound tightening
            if newLB_j > l_j + delta_min:
                if newLB_j > u_j + epsilon_feas:
                    return INFEASIBLE with variable j
                update l_j := newLB_j
                propagate to all constraints containing j (Section 3)
                enqueue affected constraints

            // Apply upper bound tightening
            if newUB_j < u_j - delta_min:
                if newUB_j < l_j - epsilon_feas:
                    return INFEASIBLE with variable j
                update u_j := newUB_j
                propagate to all constraints containing j (Section 3)
                enqueue affected constraints

        // Epoch tracking
        if current position reaches epochEnd:
            iterationCount := iterationCount + 1
            epochEnd := current tail of Q
            if iterationCount > K_max:
                break

Here delta_min is the minimum improvement threshold, computed as epsilon_prop * epsilon_feas, where epsilon_prop is a small constant (on the order of 1e-6). K_max is the maximum number of full passes (typically 10).

#### Step 6.3: Post-Propagation Variable Fixing

After the worklist drains or the iteration cap is reached, the cleanup procedure scans all variables and fixes those whose bounds have converged to within the feasibility tolerance. This is done via the variable-fixing procedure (P3.19), which creates variable-fixing eta records and updates the basis.

---

## Key Design Choices

- **Pricing-driven candidate selection (simplex-integrated mode):** Rather than maintaining a dedicated worklist for bound propagation, the solver reuses the pricing subsystem's candidate queues. This eliminates the memory overhead of a separate worklist and naturally focuses propagation effort on variables and constraints that are currently relevant to the optimization.

- **Single pass per iteration:** Each simplex iteration performs at most one pass over the pricing-selected candidates. This contrasts with standalone FBBT, which iterates until convergence. The rationale is that the simplex solver calls propagation at every iteration, so the cumulative effect of many single-pass invocations approaches the fixed point over the course of the solve.

- **Bidirectional decomposition:** Variable-side and constraint-side propagation are separated into two functions. This is not merely an implementation detail; the two directions use different candidate sources (variable queue vs. constraint queue) and different pricing status checks. The separation also allows constraint-side propagation to be disabled for quadratic programs, where the linear activity-bound inference is invalid.

- **Two-stage infeasibility detection:** Both propagation functions use a conservative two-stage infeasibility check. The first stage detects a potential violation via the ratio or implied value computation. The second stage cross-checks against activity bounds or dual information. Only when both stages agree is infeasibility reported. This prevents the numerical noise inherent in incremental activity maintenance from triggering false infeasibility alarms.

- **Lightweight eta records:** Bound-change eta records are smaller than standard pivot eta records. They store only the variable index, constraint index, violation classification, coefficient, and ratio/implied value. This keeps the memory overhead of per-iteration propagation low while still enabling basis reconstruction during crossover.

- **Circular queue with epoch tracking (cleanup mode):** The standalone FBBT uses a circular buffer with an epoch marker to count full passes. This avoids re-allocating the worklist between passes and provides a simple mechanism for the iteration cap. Belotti et al. (2010) showed that FBBT may not converge finitely; the cap prevents excessive computation.

- **Maximum iteration count K_max = 10 (cleanup mode):** A hard iteration cap prevents unbounded computation in the standalone mode. In practice, most problems converge within 2-3 passes.

- **Only active constraints and variables are processed:** Constraints not in the basis and variables that are non-basic or fixed are skipped. This reflects the fact that propagation is most useful for the active portion of the problem where bounds are not yet determined by the basis.

---

## Numerical Considerations

1. **Tolerance hierarchy:** Three tolerances interact:
   - epsilon_feas (feasibility tolerance): used for infeasibility detection. A bound is crossed only if the violation exceeds this tolerance.
   - epsilon_tight (tight tolerance): used for tightening decisions. A bound is tightened only if the improvement exceeds this threshold. This is a tighter value (on the order of 1e-10) that prevents trivially small tightenings.
   - epsilon_tiny (tiny coefficient threshold): coefficients with absolute value below this threshold (on the order of 1e-40 in cleanup mode, 1e-13 in simplex-integrated mode) are treated as zero to avoid division-by-near-zero.

2. **Compensated summation in activity updates:** Incremental activity updates accumulate round-off error over many bound changes. The error-detection heuristic in Section 3 applies a conservative multiplicative correction when the computed sum deviates from the algebraically expected value. This is essential for the simplex-integrated mode, where activity arrays are updated at every iteration over potentially thousands of iterations.

3. **Conservative rounding direction:** When a correction factor is applied to an activity bound, the direction is chosen to keep the bound conservative (upper activity nudged upward, lower activity nudged downward). This ensures that all implied bounds derived from corrected activities are valid, at the cost of being slightly weaker than the true mathematical bound.

4. **Infinity representation:** Bounds exceeding the infinity threshold M are treated as unbounded and tracked via the count arrays (p_pos, p_neg) rather than contributing to the finite activity sums. This avoids arithmetic with very large numbers and the catastrophic cancellation that results from subtracting two large numbers.

5. **Activity recomputation:** Over many incremental updates, accumulated activity values may drift from their true values. In the cleanup mode, the limited number of propagation passes bounds the drift. In the simplex-integrated mode, periodic basis refactorization (P2.2) provides an opportunity to recompute activities from scratch, resetting accumulated error.

6. **Pivot threshold separation:** The variable-side function uses a tighter pivot threshold (order 1e-13) than the constraint-side function (order 1e-8). This reflects the different numerical characteristics of the two computations: the variable-side ratio divides a primal value by a row coefficient, while the constraint-side implied value divides a constraint activity by a column coefficient. The constraint-side computation is more sensitive to small denominators because the activity values can be much larger than primal values.

---

## Termination

### Simplex-Integrated Mode

Each invocation of the variable-side or constraint-side function terminates after a single pass through the pricing-selected candidates. The three possible outcomes are:

1. **Success (all candidates processed):** Return 0. The simplex iteration loop continues.
2. **Infeasibility detected and confirmed:** Return the infeasibility code. The simplex iteration loop terminates.
3. **Memory allocation failure:** Return the out-of-memory code. The simplex iteration loop terminates.

There is no convergence or iteration-limit concept in the simplex-integrated mode; each invocation is a single pass by design.

### Cleanup Mode

The standalone FBBT terminates under one of three conditions:

1. **Empty worklist (fixed-point):** No more constraints have pending bound changes. All implied bounds have been derived and are consistent. This is the normal successful termination.

2. **Maximum iterations exceeded:** The iteration counter exceeds K_max (typically 10). The algorithm assumes convergence is sufficiently close and exits with status = 0 (success). Any remaining potential tightening is negligible.

3. **Infeasibility detected:** Either a constraint-level check fails or a variable's new implied bound crosses its opposite bound. Exit immediately with status = INFEASIBLE.

**Convergence guarantee (cleanup mode):** Belotti et al. (2010) proved that for linear constraints, the FBBT operator is monotone and its iterates converge to a unique fixed point from any starting point (given consistent initial bounds). However, convergence may require infinitely many iterations. The practical consequence is that the algorithm may exit via condition 2 with bounds that are tighter than the input but not at the true fixed point.

**Monotonicity (both modes):** Each bound update strictly tightens the variable's domain (lower bounds can only increase; upper bounds can only decrease). Activity bounds are updated correspondingly. This monotonicity ensures that neither mode can cycle.

---

## Complexity

### Simplex-Integrated Mode (Per Invocation)

| Operation | Complexity | Notes |
|-----------|-----------|-------|
| Variable-side propagation | O(k * r_avg) | k candidates, r_avg average nonzeros per row |
| Constraint-side propagation | O(k * r_avg) | k constraint candidates |
| Activity update per bound change | O(c_avg) | c_avg average nonzeros per column |

The total per-iteration cost is proportional to the number of pricing candidates, which is typically a small fraction of the total problem size. This is the key efficiency advantage over standalone FBBT: the per-iteration cost is independent of the full problem dimensions m and n.

### Cleanup Mode (Per Invocation)

| Case | Complexity | Description |
|------|-----------|-------------|
| Best | O(m + n) | Single pass, no bound changes |
| Typical | O(K_avg * nnz) | K_avg passes (typically 2-3) |
| Worst | O(K_max * nnz) | All K_max passes executed |

**Space complexity (cleanup mode):** O(n) auxiliary space for the circular queue and membership array.

---

## Edge Cases

1. **Empty problem (n = 0 or m = 0):** Both modes return immediately with status = 0.

2. **No candidates (simplex-integrated):** If the pricing system returns zero candidates, both propagation functions return immediately with no changes.

3. **No active constraints (cleanup):** If all constraints have negative basis header entries, the worklist is empty and cleanup returns immediately.

4. **All variables already fixed:** No further tightening is possible. Propagation functions return immediately after scanning candidates and finding no actionable items.

5. **Unbounded variables (l_j = -infinity or u_j = +infinity):** Tracked via unbounded counts. In cleanup mode, the single-unbounded-variable case in the implied bound derivation can produce finite bounds from previously unbounded variables. In simplex-integrated mode, unbounded variables are handled through the standard ratio/implied-value computation.

6. **Free variables (l_j = -infinity and u_j = +infinity):** Both counts are incremented. A bound can only be derived when the free variable is the sole remaining unbounded contributor in a constraint.

7. **Dense constraints:** Handled correctly but produce weaker implied bounds because the "room" for each variable is divided among more terms.

8. **Memory allocation failure (eta record):** If the memory pool cannot allocate a bound-change eta record, the function returns OUT_OF_MEMORY immediately. The simplex solve terminates.

9. **Memory allocation failure (cleanup queue):** If the circular queue cannot be allocated, cleanup returns OUT_OF_MEMORY without modifying any bounds or activities.

10. **Quadratic programs:** Constraint-side propagation (Section 2) is skipped entirely. Variable-side propagation (Section 1) still operates. This is because the quadratic objective introduces nonlinear dependencies that invalidate the linear activity-bound inference used by constraint-side propagation.

11. **Integer and piecewise-linear variables:** These receive an additional flag in the violation classification stored in the eta record. This flag does not affect the propagation logic itself but enables downstream MIP or piecewise-linear processing to identify bound changes on variables requiring special integrality or breakpoint handling.

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
[x] Mathematical derivation complete (Section 5)
[x] Convergence behavior documented (both modes)
[x] Infeasibility detection specified (two-stage procedure)
[x] Simplex-integrated architecture accurately described (pricing-driven, single-pass, bidirectional)
[x] Cleanup mode accurately described (worklist-driven, iterative, epoch-tracked)
[x] Activity update with compensated summation documented (Section 3)
[x] Eta record creation documented with cross-reference to P1.04
[x] Integration point in simplex loop documented with cross-reference to P3.20
[x] An implementer could build working bound propagation from this spec
```
