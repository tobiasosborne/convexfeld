# Module: Simplex Phases

## Purpose

The Simplex Phases module contains the functions that manage the non-iterative phases of the simplex method: initial basis construction, bound perturbation for anti-cycling, variable preprocessing to reduce problem size, constraint activity bound computation, phase transition processing, and post-solve solution refinement. These six functions bracket the main simplex iteration loop (P3.20), providing initialization before it begins, conditioning within it, and cleanup after it terminates. They implement the phase management aspects of the revised simplex method described in P2.1 (Revised Simplex Method), the crash basis construction algorithm from P2.5 (Crash Basis Construction), and the perturbation strategy from P2.6 (Perturbation and Anti-Cycling).

## Functions

### cxf_simplex_crash

**Purpose:** Construct an initial basis that is better than the trivial all-slack basis by evaluating constraint feasibility and sparsity, assigning feasible rows to the initial basis.

**Signature:**
- Input: `state` : pointer-to-SolverState - The solver state with constraint matrix (CSR), constraint bounds, constraint sense, variable bounds, and row/column status arrays
- Input: `env` : pointer-to-Environment - Environment containing the feasibility tolerance
- Output: int - Zero on success, or the infeasibility code if a constraint is detected as infeasible

**Preconditions:**
- The solver state must have a valid CSR constraint matrix with correct nonzero counts
- All row status entries must be at their initial (unassigned) values
- Constraint RHS and sense arrays must be populated

**Postconditions:**
- On success: feasible unassigned rows have been assigned BASIC_LOWER status, candidate rows meeting sparsity criteria have been removed and assigned BASIC_UPPER status, column nonzero counts have been decremented for removed rows, and the basis count has been updated
- On infeasibility: the diagnostic row index has been stored in the solver state

**Side Effects:**
- Modifies row status array (assigns BASIC_LOWER or BASIC_UPPER to qualifying rows)
- Decrements column nonzero counts for rows assigned to the basis
- Removes matrix entries for basic rows assigned at upper bound (marks entries as inactive)
- Updates the basis count and work counter

**Error Conditions:**
- A constraint's RHS value violates feasibility tolerance given its sense -> returns the infeasibility code with the diagnostic row index stored

**Behavioral Description:**
This function implements the crash basis construction algorithm described in P2.5 (Crash Basis Construction). It improves on the trivial all-slack basis by identifying constraints that can enter the initial basis without introducing infeasibility, reducing the number of simplex iterations required in Phase I.

**Step 1: Row scan.** The function iterates over all constraints, evaluating each for basis eligibility based on two criteria:

1. **Feasible unassigned rows.** For rows with unassigned status, the function checks whether the constraint's RHS value lies within the feasibility tolerance. If feasible, the row is marked as BASIC_LOWER (basic at its lower bound). This corresponds to the feasibility check in P2.5, which admits rows whose slack is non-negative within tolerance.

2. **Candidate rows for upper-bound assignment.** For rows with candidate status (a positive status value indicating they were pre-classified by an earlier phase), the function evaluates whether to remove them from the active constraint set. If the row qualifies, the function removes all column entries in that row (decrements column nonzero counts and marks entries as inactive) and assigns BASIC_UPPER status. This targets sparse rows whose removal simplifies the basis without affecting the feasible region, following the sparsity-based selection in P2.5.

**Step 2: Basis count update.** The total count of basic rows is updated to reflect all assignments made.

The crash procedure typically reduces Phase I iterations by selecting a substantial fraction of constraints for the initial basis (Gould and Reid, 1989). The quality of the crash basis directly affects overall solve time.

**Thread Safety:** Not thread-safe. Must be called during single-threaded simplex initialization.

**Dependencies:**
- P2.5 (Crash Basis Construction) - implements the crash algorithm
- P1.04 (SolverState) - reads and modifies row status, column nonzero counts, constraint matrix
- P1.05 (BasisState) - basis count tracking

---

### cxf_simplex_perturbation

**Purpose:** Apply anti-cycling measures when the simplex method is detected to be cycling or stalling, using two complementary mechanisms: (1) pricing restriction via implied bound analysis to remove irrecoverably degenerate entering candidates, and (2) EXPAND-style bound widening to break leaving-side degeneracy when basic variables sit exactly at their bounds.

**Signature:**
- Input: `state` : pointer-to-SolverState - The solver state with basis, working bounds, saved bounds, constraint matrix, reduced costs, and pricing state
- Input: `env` : pointer-to-Environment - Environment containing feasibility tolerance and perturbation parameters
- Output: int - Zero on success, or the infeasibility code if a bound violation is detected during perturbation

**Preconditions:**
- Stalling has been detected by the basis snapshot diff mechanism (P3.16, P3.20 post_iterate)
- The solver state must have valid pricing state, working bounds, saved bounds, and constraint matrix
- The pricing subsystem must be initialized

**Postconditions:**
- On success (Mechanism A applied): irrecoverably degenerate variables have been removed from the pricing candidate set, the perturbation counter has been incremented, and the pricing state has been updated. The remaining pricing candidates have implied bound gaps large enough to permit non-degenerate pivots.
- On success (Mechanism B applied): working bounds of basic variables at their bounds have been widened by a small variable-dependent epsilon, the perturbation active flag has been set, the perturbation counter has been incremented, and constraint activities have been updated to reflect the modified bounds.
- On infeasibility: a variable whose lower bound exceeds its upper bound (after implied bound analysis) has been identified, and the diagnostic index has been stored

**Side Effects:**
- Removes degenerate variables from the pricing candidate set (Mechanism A: entering-side)
- Modifies working bounds for basic variables at bounds (Mechanism B: leaving-side, EXPAND widening)
- Notifies the pricing subsystem of all changes via dirty-marking and cascade updates
- Increments the perturbation counter on the solver state
- Sets the perturbation active flag when EXPAND widening is applied
- Updates the work counter

**Error Conditions:**
- Lower bound exceeds upper bound for a candidate variable (detected during implied bound analysis) -> returns the infeasibility code with diagnostic index

**Behavioral Description:**
This function implements the two-mechanism anti-cycling strategy described in P2.6 (Perturbation and Anti-Cycling). It is invoked when the post-iteration stall detection (cxf_simplex_post_iterate, P3.20) determines that the solver is not making sufficient progress.

**Phase 1: Pricing candidate retrieval.** The function obtains the current pricing candidates from the pricing subsystem (cxf_pricing_candidates, P3.17). These are the variables currently eligible for entering the basis.

**Phase 2: Optional bound restoration.** If a previous perturbation cycle is active, the function may first restore saved bounds before applying a new cycle. This prevents perturbation drift from accumulating over multiple invocations.

**Phase 3: Per-candidate implied bound analysis (Mechanism A — entering-side).** For each pricing candidate, the function evaluates whether the variable is irrecoverably degenerate:

1. **Non-basic variables at lower bound.** The function checks whether the variable's bound range is consistent. If the lower bound exceeds the upper bound beyond tolerance, the problem is infeasible. Otherwise, variables that cannot improve the objective are removed from the candidate set.

2. **Basic variables.** For each basic candidate, the function computes the implied bounds from the variable's column in the constraint matrix using saved (original) bounds. The column product (coefficient times saved bound) determines the range of feasible values. If the implied bounds indicate the variable is irrecoverably degenerate (zero feasible range within tolerance), the variable is removed from the pricing set. All column entries for the removed variable are invalidated and neighboring variables' constraint counts are decremented.

3. **Removal from pricing.** Variables identified as irrecoverably degenerate are removed from the pricing candidate set to prevent the simplex method from repeatedly selecting them. This addresses entering-side degeneracy: by shrinking the candidate pool to only variables that can make genuine progress, the method avoids revisiting the same degenerate basis.

**Phase 4: EXPAND bound widening (Mechanism B — leaving-side).** When pricing restriction alone is insufficient to resolve stalling (indicated by persistent stalling after candidate removal, or by a high proportion of zero-step pivots), the function applies EXPAND-style bound perturbation (Gill et al., 1989):

1. For each basic variable at (or within tolerance of) one of its bounds, widen the relevant working bound by a small variable-dependent epsilon, computed from the feasibility tolerance scaled by the variable's bound magnitude and a deterministic hash of the variable index. This ensures distinct perturbations per variable (Wolfe, 1963).

2. Set the perturbation active flag to trigger unperturbation after the perturbed problem reaches optimality.

3. Recompute constraint activity bounds to reflect the modified working bounds.

This mechanism directly addresses leaving-side degeneracy: basic variables are no longer exactly at their bounds, so the ratio test produces strictly positive step lengths. This is particularly important during Phase I (P2.9), where the crash basis often places many slack variables at their lower bounds.

**Phase 5: Pricing notification.** The pricing subsystem is notified of all removals and bound changes via cascading updates (cxf_pricing_cascade_update, P3.18).

**Thread Safety:** Not thread-safe. Must be called within a single-threaded simplex iteration context.

**Dependencies:**
- P2.6 (Perturbation and Anti-Cycling) - implements both mechanisms of the anti-cycling strategy
- P3.17 (Pricing Core) - cxf_pricing_candidates for candidate retrieval
- P3.18 (Pricing Support) - cxf_pricing_mark_dirty, cxf_pricing_cascade_update for notification; cxf_pricing_end_level for removing candidates
- P1.04 (SolverState) - reads/modifies working bounds, saved bounds, pricing state, perturbation counter, perturbation active flag
- P1.05 (BasisState) - basis header for identifying basic variables

---

### cxf_simplex_preprocess

**Purpose:** Reduce the effective problem size by identifying variables with tight bound ranges and fixing them at their bounds, creating eta records to track the fixings for later recovery.

**Signature:**
- Input: `state` : pointer-to-SolverState - The solver state with bounds, constraint matrix, activity arrays, and eta chain
- Input: `env` : pointer-to-Environment - Environment containing feasibility tolerance and preprocessing parameters
- Output: int - Zero on success, the infeasibility code if fixing a variable creates an infeasible constraint, or the out-of-memory code if eta allocation fails

**Preconditions:**
- The solver state must have valid bounds, constraint matrix (CSR and CSC), and activity arrays
- Activity bounds should have been computed by cxf_simplex_setup before this function is called

**Postconditions:**
- On success: all variables with bound ranges below the tightness threshold have been fixed at their bounds, eta records have been created for each fixing, constraint activities have been updated, and the objective value has been adjusted for the fixed variables' contributions
- On infeasibility: a constraint has been identified where fixing a variable forces infeasibility, and diagnostic information has been stored
- On out-of-memory: the function returns immediately; partial fixings already applied remain in effect

**Side Effects:**
- Allocates variable-fixing eta vectors from the memory pool
- Modifies variable bounds (sets lower bound equal to upper bound for fixed variables)
- Updates constraint activity arrays to reflect fixed variable contributions
- Adjusts the objective value for fixed variable contributions
- Updates the work counter

**Error Conditions:**
- Lower bound exceeds upper bound for a candidate variable -> returns the infeasibility code
- Memory allocation failure for eta vector -> returns the out-of-memory code

**Behavioral Description:**
This function performs a lightweight preprocessing pass that reduces the effective problem size before the main simplex iterations begin. It targets near-fixed variables — those whose lower and upper bounds are so close together that they are effectively constant. Fixing these variables at their bounds removes them from the pricing set, reduces the work per iteration, and can significantly improve solve time on problems with many near-fixed variables.

**Step 1: Candidate identification.** The function scans all variables and collects those whose bound range (upper bound minus lower bound) is below a tightness threshold. The threshold is a multiple of the feasibility tolerance. A minimum candidate count is enforced to prevent degenerate preprocessing on very small problems.

**Step 2: Candidate sorting.** The candidates are sorted by bound width (tightest first) using cxf_sort_indices (P3.14). This ordering ensures that the most constrained variables are fixed first, maximizing the chance that each fixing remains feasible.

**Step 3: Activity initialization.** The constraint activity arrays are cleared (or initialized from current activity bounds) to provide a clean baseline for tracking the cumulative effect of variable fixings.

**Step 4: Per-candidate processing.** For each candidate, in order of increasing bound width:

1. **Feasibility check.** If the variable's lower bound exceeds its upper bound (accounting for tolerance), the problem is infeasible.

2. **Target bound selection.** The variable is fixed at the bound that minimizes disruption: typically the bound closest to the current value, or the lower bound when both are equidistant.

3. **Eta record creation.** A variable-fixing eta record is allocated (cxf_alloc_eta, P3.02) and populated with the variable index and fixing value. This record enables later recovery if the fixing needs to be undone.

4. **Activity update.** The constraint activities are updated to reflect the fixed variable's contribution: for each constraint containing the variable, the activity bound is adjusted by the product of the coefficient and the fixing value.

5. **Objective adjustment.** The objective value is adjusted by the product of the variable's objective coefficient and its fixing value.

**Thread Safety:** Not thread-safe. Must be called during single-threaded simplex initialization.

**Dependencies:**
- P3.02 (Allocation Helpers) - cxf_alloc_eta for eta vector allocation
- P3.14 (Matrix Core) - cxf_sort_indices for candidate sorting
- P1.04 (SolverState) - reads bounds, constraint matrix; modifies activity arrays, objective, eta chain
- P1.05 (BasisState) - eta chain management for fixing records

---

### cxf_simplex_setup

**Purpose:** Compute the constraint activity bounds (minimum and maximum possible left-hand-side values) for each constraint, based on the current variable bounds and constraint matrix coefficients.

**Signature:**
- Input: `state` : pointer-to-SolverState - The solver state with constraint matrix (CSC), variable bounds, and constraint data
- Input: `env` : pointer-to-Environment - Environment containing numerical tolerances
- Input: `count` : int - Number of constraint indices to process; ignored if `indices` is null
- Input: `indices` : pointer-to-int-array - Optional array of constraint indices to process; null means process all constraints
- Output: void (results are written to activity bound arrays in the solver state)

**Preconditions:**
- The solver state must have valid CSC constraint matrix, variable bounds, and allocated activity bound arrays
- If `indices` is non-null, all entries must be valid constraint indices

**Postconditions:**
- For each processed constraint: the minimum activity bound reflects the smallest possible LHS value given current variable bounds, and the maximum activity bound reflects the largest
- Activity bounds account for the constraint's RHS contribution (subtracted from the initial accumulator)
- Numerical rounding corrections have been applied to prevent false infeasibility from accumulated floating-point error

**Side Effects:**
- Writes to the minimum and maximum activity bound arrays in the solver state
- No memory allocation
- No modification of bounds, basis, or constraint matrix

**Error Conditions:**
- None. This function always succeeds and returns void.

**Behavioral Description:**
This function computes implied activity bounds for constraints, a fundamental operation used by preprocessing (cxf_simplex_preprocess), bound propagation (cxf_simplex_step2/step3, P3.20), phase transition processing (cxf_simplex_phase_end), and infeasibility detection. The technique is the standard activity bound computation from LP presolve theory (Savelsbergh, 1994; Achterberg et al., 2020).

For a constraint of the form sum(a_j * x_j) <= b, the activity bounds are:
- **Minimum activity:** sum over all j of min(a_j * lb_j, a_j * ub_j)
- **Maximum activity:** sum over all j of max(a_j * lb_j, a_j * ub_j)

**Step 1: Initialization.** For each constraint to be processed, the activity accumulators are initialized using the negated constraint bound. This convention means the final activity values represent the surplus or deficit relative to the constraint's RHS.

**Step 2: Coefficient accumulation.** For each nonzero coefficient in the constraint, the function accumulates contributions to both the minimum and maximum activity:
- For positive coefficients: the maximum contribution uses the variable's upper bound, and the minimum contribution uses the lower bound.
- For negative coefficients: the maximum contribution uses the variable's lower bound, and the minimum contribution uses the upper bound.
- For variables with infinite bounds: the corresponding activity bound is set to infinity (or negative infinity), indicating the constraint cannot bound the activity in that direction.

**Step 3: Rounding correction.** After accumulation, the function applies a rounding correction to prevent false infeasibility from accumulated floating-point error. When the minimum and maximum activities are very close (near-zero range), small rounding errors could make the minimum exceed the maximum. The correction detects this case and applies a conservative adjustment.

**Selective computation:** When the `indices` parameter is non-null, only the specified constraints are processed. This enables efficient incremental updates when a small number of constraints are affected by bound changes, avoiding the cost of recomputing all constraints.

**Thread Safety:** Not thread-safe. Must be called within a single-threaded simplex context.

**Dependencies:**
- P1.04 (SolverState) - reads CSC constraint matrix, variable bounds, constraint RHS; writes activity bound arrays
- P2.1 (Revised Simplex Method) - activity bounds support feasibility detection and preprocessing within the simplex framework

---

### cxf_simplex_phase_end

**Purpose:** Process constraints at the end of a simplex phase, identifying variables that can be fixed at bounds based on their reduced costs and cleaning up inactive constraints, then recomputing activity bounds for modified constraints.

**Signature:**
- Input: `state` : pointer-to-SolverState - The solver state with constraint matrix, bounds, reduced costs, activity bounds, and pricing state
- Input: `env` : pointer-to-Environment - Environment containing feasibility and optimality tolerances
- Input: `doScan` : int - Flag controlling detailed coefficient scanning: non-zero enables scanning for small-contribution variable removal
- Output: int - Zero on success, or the infeasibility code if a dual bound violation is detected

**Preconditions:**
- The solver state must have valid pricing state, reduced costs, bounds, and constraint matrix
- Activity bounds must be current (computed by cxf_simplex_setup or maintained by incremental updates)

**Postconditions:**
- On success: free variables with infeasible reduced costs have been identified (if any, infeasibility is returned), basic constraints with activity bounds indicating inactivity have been cleaned up, and activity bounds have been recomputed for all modified constraints
- On infeasibility: a variable with a reduced cost indicating dual infeasibility has been identified

**Side Effects:**
- Removes inactive variables and constraints from the pricing candidate set
- Marks affected variables as dirty in the pricing subsystem
- Commits pricing changes and triggers cascading updates
- Recomputes activity bounds for modified constraints (delegates to cxf_simplex_setup logic)
- Uses sparse entry invalidation (lazy deletion) for removed constraints

**Error Conditions:**
- A free variable's reduced cost indicates dual infeasibility -> returns the infeasibility code with diagnostic index

**Behavioral Description:**
This function manages the transition between simplex phases and performs inter-iteration constraint cleanup. It is called at two points in the iteration loop (P3.20, Module-Level Notes): once before the main pivot step and once after. Its role is to detect opportunities for simplification that arise from the accumulated effect of recent pivots, and to enforce the conditions for transitioning from Phase I (feasibility) to Phase II (optimality).

**Phase 1: Candidate constraint processing.** The function retrieves constraint candidates from the pricing subsystem (cxf_pricing_get_constr_candidates, P3.18) and evaluates each:

1. **Free (non-basic) variables.** For variables with free status, the function checks the reduced cost against the optimality tolerance. If the reduced cost indicates that the variable should enter the basis but cannot (because it is at a bound that makes progress impossible), this signals dual infeasibility and the function returns the infeasibility code.

2. **Basic constraints.** For constraints with basic status, the function checks whether the constraint's activity bounds indicate it is inactive (slack exceeds a threshold). Inactive constraints can be removed from the active set because they do not constrain the current solution. If `doScan` is enabled, the function additionally scans the constraint's coefficients to identify variables with very small contributions that can be safely removed without affecting the solution quality.

3. **Sparse removal.** Removed constraints and variables are handled via sparse entry invalidation (marking entries as deleted) rather than array compaction. This lazy deletion strategy avoids the cost of shifting array elements and is cleaned up during the next basis refactorization.

**Phase 2: Activity bound recomputation.** After all constraint modifications, the function recomputes activity bounds for all constraints that were affected by the cleanup. This ensures that subsequent operations (bound propagation, infeasibility detection) work with accurate activity data. The recomputation uses the same technique as cxf_simplex_setup, applied selectively to the modified constraint set.

#### Phase I to Phase II Transition

The two-phase simplex method (Dantzig, 1963; Chvatal, 1983) separates feasibility from optimality. Phase I minimizes a measure of total constraint violation (the sum of infeasibilities) using a surrogate objective function. Phase II minimizes the original objective function over the feasible region. cxf_simplex_phase_end participates in the mechanism that detects when Phase I has succeeded and orchestrates the transition to Phase II.

**Transition conditions.** The Phase I to Phase II transition is triggered when both of the following hold:

1. **Primal feasibility achieved.** All basic variables satisfy their bound constraints within the primal feasibility tolerance. Equivalently, the Phase I objective value (the aggregate infeasibility measure) has been driven to zero within tolerance. This is evaluated through the constraint activity bound analysis performed by this function and the pricing subsystem's reduced cost checks: when no constraint candidate exhibits a bound violation beyond the feasibility tolerance, the current basis is primal feasible.

2. **No dual infeasibility among free variables.** All free (unbounded) variables have reduced costs consistent with dual feasibility. If any free variable has a reduced cost that violates the optimality tolerance (negative for a minimization problem, or positive for an equality constraint), the problem is detected as infeasible and the function returns the infeasibility code rather than transitioning. This check distinguishes between "Phase I succeeded" (feasible solution found) and "Phase I proved infeasibility" (no feasible solution exists).

**Infeasibility determination at Phase I termination.** If Phase I reaches optimality (no improving pivot exists) but the Phase I objective remains strictly positive (i.e., some constraint violations persist beyond tolerance), the original problem is infeasible. In the context of this function, this manifests as the dual infeasibility check on free variables: a free variable with a reduced cost indicating it should improve the infeasibility measure, yet unable to do so because it is already at a bound, signals that the infeasibility cannot be resolved. The function returns the infeasibility code with a diagnostic index identifying the problematic variable or constraint, enabling the caller to report which constraints are in conflict.

**State transformations at transition.** When Phase I succeeds and the transition to Phase II occurs, the following state changes take place (coordinated between this function and the orchestration layer in P3.25):

1. **Objective function swap.** The Phase I surrogate objective (which penalizes infeasibility) is replaced by the original problem objective c. This is the defining characteristic of the two-phase method: the objective function changes at the phase boundary while the basis and variable values are preserved. The two-phase approach avoids the numerical conditioning problems of the Big-M method, which uses a single objective with artificially large penalty coefficients (Dantzig, 1963, Chapter 7; Maros, 2003, Section 6.3).

2. **Reduced cost recomputation.** All reduced costs are recomputed from scratch using the original objective: d_N = c_N - N^T (B^{-T} c_B), where B is the current basis matrix, c_B are the objective coefficients for basic variables, and N is the matrix of non-basic columns. This full recomputation is necessary because the reduced costs from Phase I are relative to the surrogate objective and have no meaning under the original objective.

3. **Pricing state reset.** The pricing subsystem's candidate sets and tolerance levels may be reinitialized to reflect the new objective landscape. Variables that were unattractive under the Phase I objective may now have significant reduced costs under the original objective, and vice versa. The multi-level pricing tolerance (P2.3) typically resets to its initial (loose) level to allow rapid early progress in Phase II.

4. **Constraint cleanup.** Inactive constraints identified during Phase I processing (those whose activity bounds indicate they are not binding at the current solution) are removed from the active set. This cleanup, performed by the sparse removal mechanism described above, reduces the effective problem size entering Phase II.

5. **Basis preservation.** The basis itself (the set of basic variables and their positions) is carried forward from Phase I to Phase II unchanged. The Phase I solution is a basic feasible solution, and Phase II begins from this vertex of the feasible polyhedron. The basis factorization may be refreshed (via cxf_basis_refactor, P3.16) to ensure numerical accuracy for Phase II iterations, since the objective change can affect the conditioning of subsequent operations.

6. **Tolerance adjustment.** The optimality tolerance used for Phase II termination may differ from the feasibility tolerance used for Phase I. Phase I uses the primal feasibility tolerance to determine when constraint violations are acceptable; Phase II uses the dual feasibility (optimality) tolerance to determine when reduced costs are small enough to declare optimality. These are typically configured as separate environment parameters.

**Literature context.** The two-phase method is the standard approach for finding an initial basic feasible solution in the simplex method. It was introduced by Dantzig (1963) and is described in detail by Chvatal (1983, Chapter 3), Vanderbei (2014, Chapter 5), and Maros (2003, Chapter 6). The alternative Big-M method embeds the feasibility objective into the original objective using a large penalty coefficient M, avoiding the explicit phase transition but introducing numerical difficulties when M is large relative to the original objective coefficients. The two-phase method is preferred in practice because it provides a clean separation of concerns and avoids the conditioning issues inherent in the Big-M approach.

**Thread Safety:** Not thread-safe. Must be called within a single-threaded simplex iteration context.

**Dependencies:**
- P3.18 (Pricing Support) - cxf_pricing_get_constr_candidates for candidate retrieval, cxf_pricing_mark_dirty and cxf_pricing_cascade_update for notification
- P1.04 (SolverState) - reads reduced costs, bounds, activity bounds, constraint matrix; modifies pricing state and activity bounds
- P2.1 (Revised Simplex Method) - phase transition conditions (Phase I feasibility -> Phase II optimality)

---

### cxf_simplex_refine

**Purpose:** Refine the solution after the main simplex iterations complete by cleaning up non-basic variables with small reduced costs (fixing them at appropriate bounds) and recovering basic variables that have drifted near their upper bounds.

**Signature:**
- Input: `state` : pointer-to-SolverState - The solver state with basis, bounds, reduced costs, variable status, and eta chain
- Input: `env` : pointer-to-Environment - Environment containing the dual feasibility tolerance and solve mode
- Output: int - Zero on success, the infeasibility code if a fixing creates infeasibility, the unbounded code if a variable with significant reduced cost has no finite bound, or the out-of-memory code if eta allocation fails

**Preconditions:**
- The main simplex iteration loop must have terminated (either optimally or at a limit)
- The solver state must have valid basis, reduced costs, bounds, and variable status arrays

**Postconditions:**
- On success: all non-basic variables with reduced costs within the dual tolerance have been fixed at their appropriate bounds with eta records created, and all basic variables near their upper bounds have been processed via cxf_pivot_primal for feasibility-checked fixing
- On infeasibility: a variable fixing triggered an infeasible constraint (propagated from cxf_pivot_primal)
- On unbounded: a variable has a significant reduced cost but no finite bound in the improvement direction

**Side Effects:**
- Allocates variable-fixing eta vectors from the memory pool
- Modifies variable status (non-basic variables may be re-classified)
- Updates the objective value for fixed variable contributions
- Calls cxf_pivot_primal (P3.19) for basic variable recovery, which may modify bounds, activity bounds, and pricing state
- Updates the work counter

**Error Conditions:**
- A non-basic variable has a significant reduced cost (above dual tolerance) but the bound in the improvement direction is infinite -> returns the unbounded code
- Memory allocation failure for eta vector -> returns the out-of-memory code
- cxf_pivot_primal returns infeasibility -> propagated immediately

**Behavioral Description:**
This function performs post-solve cleanup to improve solution quality. It is called after the main simplex iteration loop terminates and before the final solution is extracted. Unlike academic iterative refinement techniques (Gleixner et al., 2016), this function performs bound-based cleanup: it identifies variables that should be at their bounds (based on reduced cost analysis) and forces them there.

**Pass 1: Non-basic variable cleanup.** The function scans all non-basic variables. For each variable with free status and no special flags:

1. **Reduced cost evaluation.** The reduced cost is compared against the dual feasibility tolerance. If the reduced cost is within tolerance (indicating the variable is near-optimal), it is a candidate for fixing.

2. **Target bound selection.** The fixing direction depends on the sign of the reduced cost:
   - Positive reduced cost: fix at lower bound (moving up would worsen the objective)
   - Negative reduced cost: fix at upper bound (moving down would worsen the objective)
   - Near-zero reduced cost: fix at the bound closest to the current value

3. **Unboundedness check.** If the target bound is infinite, the variable cannot be fixed and the problem is unbounded in that direction. The function returns the unbounded code.

4. **Eta record creation.** A variable-fixing eta record is allocated and populated with the variable index and fixing value, enabling later recovery.

5. **Objective adjustment.** The objective value is updated by the contribution of the fixed variable.

**Pass 2: Basic variable recovery.** The function scans basic variables to identify those that have drifted near their upper bounds during the iteration process. For each such variable:

1. **Proximity check.** If the variable's current value is close to its upper bound (within the feasibility tolerance), it is a candidate for recovery.

2. **Feasibility-checked fixing.** The function delegates to cxf_pivot_primal (P3.19) to fix the variable at its upper bound with full feasibility checking. This ensures that the fixing does not create constraint violations.

**Pass 3: Finalization.** The work counter is updated to reflect the total refinement work performed.

**Thread Safety:** Not thread-safe. Must be called within a single-threaded simplex context.

**Dependencies:**
- P3.19 (Pivot Operations) - cxf_pivot_primal for feasibility-checked variable fixing during basic variable recovery
- P3.02 (Allocation Helpers) - eta vector allocation for variable-fixing records
- P1.04 (SolverState) - reads reduced costs, bounds, variable status; modifies objective, eta chain, variable status
- P1.05 (BasisState) - eta chain management for fixing records

---

## Module-Level Behavioral Notes

### Role in the Simplex Solve Lifecycle

The six functions in this module bracket and condition the main simplex iteration loop. Their positions in the overall solve flow (cxf_solve_lp, P3.25) are:

**Pre-iteration setup:**
1. cxf_simplex_init (P3.22) — allocate solver state
2. **cxf_simplex_crash** (this module) — construct initial basis
3. **cxf_simplex_preprocess** (this module) — fix near-bound variables
4. **cxf_simplex_setup** (this module) — compute activity bounds
5. cxf_basis_refactor (P3.16) — initial basis factorization

**Within the iteration loop:**
6. cxf_simplex_iterate (P3.20) — progress logging
7. **cxf_simplex_phase_end** (this module) — phase transition and constraint cleanup
8. **cxf_simplex_perturbation** (this module) — anti-cycling when stalling detected
9. cxf_simplex_step/step2/step3 (P3.20) — main pivot and bound propagation
10. **cxf_simplex_phase_end** (this module) — post-pivot constraint cleanup
11. cxf_simplex_post_iterate (P3.20) — stall detection, termination checks

**Post-iteration cleanup:**
12. **cxf_simplex_refine** (this module) — solution refinement
13. cxf_simplex_final (P3.22) — solution extraction
14. cxf_simplex_cleanup (P3.22) — resource deallocation

### Relationship to Algorithm Specifications

| Function | Primary Algorithm Spec | Role |
|----------|----------------------|------|
| cxf_simplex_crash | P2.5 (Crash Basis Construction) | Implements the crash algorithm |
| cxf_simplex_perturbation | P2.6 (Perturbation and Anti-Cycling) | Implements pricing restriction (entering-side) and EXPAND bound widening (leaving-side) |
| cxf_simplex_preprocess | P2.1 (Revised Simplex Method) | Preprocessing reduction within the simplex framework |
| cxf_simplex_setup | P2.1 (Revised Simplex Method) | Activity bound computation for feasibility and propagation |
| cxf_simplex_phase_end | P2.1 (Revised Simplex Method) | Phase I/II transition management |
| cxf_simplex_refine | P2.1 (Revised Simplex Method) | Post-optimality bound cleanup |

### Eta Vector Types Created by This Module

| Function | Eta Type | Description |
|----------|----------|-------------|
| cxf_simplex_preprocess | VARIABLE_FIX (Variant 2) | Records variable fixings at bounds for later recovery |
| cxf_simplex_refine | VARIABLE_FIX (Variant 2) | Records post-solve variable fixings for solution reconstruction |

cxf_simplex_crash, cxf_simplex_perturbation, cxf_simplex_setup, and cxf_simplex_phase_end do not create eta records directly (they modify status arrays and pricing state instead).

### Activity Bound Computation Pattern

cxf_simplex_setup is the foundational activity bound computation function, but the same logic appears in reduced form within cxf_simplex_phase_end (for selective recomputation after constraint cleanup). The standard activity bound formula for constraint i is:

- activityMin_i = sum over j: min(a_ij * lb_j, a_ij * ub_j) - b_i
- activityMax_i = sum over j: max(a_ij * lb_j, a_ij * ub_j) - b_i

where a_ij are matrix coefficients, lb_j/ub_j are variable bounds, and b_i is the constraint RHS. This is the standard computation from LP presolve theory (Savelsbergh, 1994).

### Numerical Considerations

- **Crash tolerance:** Feasibility decisions in cxf_simplex_crash use the primal feasibility tolerance from the environment. Rows that are infeasible by more than this tolerance are rejected.
- **Perturbation stability:** cxf_simplex_perturbation uses implied bound analysis with the same feasibility tolerance to avoid removing variables that are only marginally degenerate.
- **Preprocessing threshold:** cxf_simplex_preprocess uses a bound-width threshold (a multiple of the feasibility tolerance) to identify near-fixed variables. Variables with bound ranges at or below this threshold are candidates for fixing.
- **Activity rounding:** cxf_simplex_setup applies rounding corrections when the accumulated minimum and maximum activities are very close, preventing false infeasibility from floating-point error.
- **Refinement tolerance:** cxf_simplex_refine uses the dual feasibility tolerance to classify reduced costs as "small enough to fix" versus "significant enough to indicate unboundedness."

### Parameter Structure Distinction

The six functions in this module use two parameter patterns:

- **cxf_simplex_setup** accepts `state`, `env`, `count`, and `indices` — enabling selective constraint processing for incremental updates.
- **All other functions** accept `state` and `env` only, operating on the full problem.

### Return Code Conventions

| Code | Meaning | Functions |
|------|---------|-----------|
| Success (zero) | Operation completed normally | All six functions |
| Infeasibility code | Bound violation or dual infeasibility detected | crash, perturbation, preprocess, phase_end, refine |
| Unbounded code | Variable with significant reduced cost has no finite bound | refine only |
| Out-of-memory code | Eta vector allocation failed | preprocess, refine |

### Thread Safety Summary

| Function | Thread Safety | Notes |
|----------|---------------|-------|
| cxf_simplex_crash | Not thread-safe | Modifies row status, column counts |
| cxf_simplex_perturbation | Not thread-safe | Modifies bounds, pricing state |
| cxf_simplex_preprocess | Not thread-safe | Modifies bounds, activities, eta chain |
| cxf_simplex_setup | Not thread-safe | Writes to activity bound arrays |
| cxf_simplex_phase_end | Not thread-safe | Modifies pricing state, activity bounds |
| cxf_simplex_refine | Not thread-safe | Modifies bounds, objective, eta chain |

All functions operate within a single-threaded simplex solve. Thread safety for concurrent solves is achieved at the model level by creating independent solver instances.

---

## Verification Checklist

```
[x] No hex addresses or offsets
[x] No Ghidra artifacts (param_, local_, iVar, DAT_, FUN_)
[x] No binary-specific constants or magic numbers
[x] No copied code fragments from analyzed source
[x] All descriptions are behavioral, not implementational
[x] All data structures described semantically using Layer 1/2 references
[x] Explicit cross-references to P1.04, P1.05, P2.1, P2.5, P2.6 (algorithm specs) and P3.02, P3.14, P3.17-P3.20 (module specs)
[x] All algorithms cite published sources (Dantzig, Chvatal, Vanderbei, Maros, Gould & Reid, Gill et al., Savelsbergh, Achterberg et al.)
[x] Passes the Clean Room Test: could be written without seeing the binary
```

## References

- Achterberg, T., Bixby, R.E., Gu, Z., Rothberg, E., and Weninger, D. (2020). "Presolve Reductions in Mixed Integer Programming." *INFORMS Journal on Computing*, 32(2):473-506.
- Chvatal, V. (1983). *Linear Programming*. W.H. Freeman.
- Dantzig, G.B. (1963). *Linear Programming and Extensions*. Princeton University Press.
- Gill, P.E., Murray, W., Saunders, M.A., and Wright, M.H. (1989). "A practical anti-cycling procedure for linearly constrained optimization." *Mathematical Programming*, 45(1-3):437-474.
- Gleixner, A.M., Steffy, D.E., and Wolter, K. (2016). "Iterative refinement for linear programming." *INFORMS Journal on Computing*, 28(3):449-464.
- Gould, N.I.M. and Reid, J.K. (1989). "New crash procedures for large systems of linear constraints." *Mathematical Programming*, 45(1-3):475-501.
- Maros, I. (2003). *Computational Techniques of the Simplex Method*. Springer. International Series in Operations Research and Management Science, Vol. 61.
- Savelsbergh, M.W.P. (1994). "Preprocessing and Probing Techniques for Mixed Integer Programming Problems." *ORSA Journal on Computing*, 6(4):445-454.
- Vanderbei, R.J. (2014). *Linear Programming: Foundations and Extensions*. 4th ed. Springer.
