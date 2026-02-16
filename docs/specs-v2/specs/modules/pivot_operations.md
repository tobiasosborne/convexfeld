# Module: Pivot Operations

## Purpose

The Pivot Operations module implements the variable fixing, bound flipping, step length computation, unboundedness detection, and incremental activity bound maintenance operations that support simplex pivots. These functions are the concrete mechanisms by which the simplex algorithm modifies the working problem: fixing variables at bounds, computing feasible step lengths for the ratio test, handling special cases such as unboundedness and row elimination, and maintaining constraint activity bounds for efficient infeasibility detection. Together with the eta vector creation functions in the Basis Operations module (P3.16), the Pivot Operations module provides the complete pivot execution infrastructure used by the Simplex Iteration module (P3.20).

## Functions

### cxf_pivot_check

**Purpose:** Compute the maximum and minimum feasible step length bounds for a candidate entering variable by scanning its constraint column, providing the initial bounds for the Harris two-pass minimum ratio test (Harris, 1973).

**Signature:**
- Input: `state` : pointer-to-SolverState - The solver's working state containing the constraint matrix, variable bounds, constraint activity data, and status arrays
- Input: `varIndex` : int - Column index of the entering variable whose step bounds are being computed
- Input: `stepSign` : double - Sign indicator for the step direction (determines whether to compute upper or lower bounding ratios)
- Input: `reserved` : int - Reserved parameter [UNDETERMINED purpose; not used in the computation]
- Output: `maxStep` : pointer-to-double - Output: the upper bound on the step length (may be null if not needed)
- Output: `minStep` : pointer-to-double - Output: the lower bound on the step length (may be null if not needed)
- Return: void

**Preconditions:**
- The solver state must have valid CSC matrix data and consistent constraint status arrays
- The variable index must reference a valid column in the constraint matrix
- Output pointers may be null (results for null pointers are not written)

**Postconditions:**
- The max step and min step output values bound the feasible region for the entering variable's movement, considering all active constraints
- Equality constraints contribute bounds in both directions (since activity must equal the right-hand side)
- Constraints with negligible coefficients (below standard pivot tolerances) are excluded from the computation
- The work counter has been updated proportionally to the number of nonzeros scanned

**Side Effects:**
- Updates the work counter on the solver state (proportional to column length)
- No memory allocation
- No modification of constraint matrix, status arrays, or other solver state

**Error Conditions:**
- None. This is a pure computation with null-safe output pointer writes.

**Behavioral Description:**
This function supports Stage 1 (Candidate Ratio Computation) of the Harris ratio test described in P2.4 (Harris Ratio Test and BFRT). Given a candidate entering variable, it scans the variable's column in the CSC constraint matrix and computes how far the variable can move in each direction before any constraint becomes infeasible.

For each nonzero coefficient in the entering variable's column:

1. **Skip inactive entries.** Constraints that have been eliminated or marked inactive (negative status sentinel) are skipped.

2. **Classify by coefficient sign.** For positive coefficients exceeding a minimum pivot threshold:
   - The constraint limits the maximum step based on the slack between the constraint's current activity and its upper activity bound, divided by the coefficient magnitude.
   - For equality constraints, the constraint also limits the minimum step based on the slack to the lower activity bound.

3. **Negative coefficients** (below a negative threshold): The constraint limits the minimum step via the ratio of the lower activity slack to the coefficient magnitude. For equality constraints, it also limits the maximum step. The threshold for negative coefficients is set very conservatively (much smaller in magnitude than for positive coefficients) to avoid excluding numerically marginal constraints from consideration.

4. **Near-zero coefficients** are skipped entirely to prevent numerical instability from division by near-zero values.

The function tracks the tightest (most restrictive) bound across all active constraints. These bounds are used by the simplex step function to initialize the Harris two-pass ratio test, which then selects the leaving variable with the largest absolute pivot element among candidates within a tolerance band of the tightest bound (see P2.4, Stage 2: Harris Two-Pass Selection).

**Thread Safety:** Not thread-safe. Must be called within a single-threaded simplex iteration context.

**Dependencies:**
- P1.04 (SolverState) - reads CSC matrix data, constraint status arrays, and activity/bound data
- P2.4 (Harris Ratio Test) - this function provides the initial bounds for Stage 1 of the Harris two-pass procedure

---

### cxf_pivot_bound

**Purpose:** Fix a variable at a specific bound value during simplex iterations, updating the objective function, constraint activities, activity bounds, and the PFI eta chain to reflect the variable's removal from the active problem.

**Signature:**
- Input: `env` : pointer-to-Environment - Environment for memory allocation services
- Input: `state` : pointer-to-SolverState - The solver's working state
- Input: `varIndex` : int - Index of the variable to fix
- Input: `fixedValue` : double - The value at which to fix the variable
- Input: `upperBound` : double - The variable's current upper bound (used for bound contribution removal)
- Input: `mode` : int - Operation mode: zero for unconditional fixing; nonzero to check variable flags before proceeding
- Output: int - Zero on success, or the out-of-memory error code if eta allocation fails

**Preconditions:**
- The solver state must be fully initialized with valid constraint matrix data in both CSR and CSC formats
- The variable index must be valid and the variable must not already be fixed or inactive
- The environment must provide memory allocation services for eta vector creation

**Postconditions:**
- The variable has been fixed at the specified value and removed from the active problem
- The objective function has been updated to reflect the fixed variable's linear and quadratic contributions
- An eta record has been created in the PFI chain for basis reconstruction support
- All constraints containing the variable have had their activity bounds updated incrementally
- The pricing subsystem has been notified of the variable status change
- The variable's reduced cost has been set to zero
- The variable's lower and upper bounds have been set equal to the fixed value
- The variable's column has been marked inactive in the constraint matrix

**Side Effects:**
- Allocates an eta vector from the memory pool (see P2.01 for PFI memory management)
- Updates the objective value accumulator
- Modifies constraint activity bounds and unbounded variable counts for each affected constraint
- Notifies the pricing subsystem via variable update and dirty-marking calls
- Marks the variable's column as inactive in the constraint matrix
- For quadratic objectives: linearizes off-diagonal Q-matrix contributions into neighboring variables' reduced costs and removes Q adjacency entries
- For piecewise-linear variables: performs breakpoint segment lookup before fixing
- Updates the work counter proportional to the number of matrix entries processed

**Error Conditions:**
- Memory allocation failure during eta vector creation -> returns the out-of-memory error code; partial state updates may have occurred

**Behavioral Description:**
This is the foundational variable-fixing operation used throughout the simplex algorithm. It is called directly by other pivot functions (cxf_pivot_primal, cxf_pivot_special) and by simplex finalization routines. The function executes in several behavioral phases:

**Phase 1: Flag evaluation and eta record creation.** If mode is nonzero, the function first checks the variable's flag array for special handling requirements. Variables with piecewise-linear (PWL) flags require breakpoint segment lookup to determine the correct fixing value within the active segment. An eta record is then created and prepended to the PFI chain. Two forms of eta records are used:
- A compact form when the solver is in simplified eta tracking mode, storing only the variable index and fixed value.
- A full form that additionally stores the variable's constraint matrix column data (filtered to active rows), the previous reduced cost, and the bound status. This full form supports later basis reconstruction during crossover or warm-start scenarios.

See P2.01 (Product Form of the Inverse) for the eta record format and P1.08 (EtaVector) Variant 2 for the variable-fixing record definition.

**Phase 2: Linear objective update.** The objective value is incremented by the product of the variable's reduced cost and its fixed value. The variable's reduced cost is then set to zero.

**Phase 3: Quadratic objective update.** If the variable has a nonzero diagonal Q-matrix entry, the quadratic contribution is added to the objective: one-half times the diagonal Q-value times the fixed value squared. The diagonal entry is then zeroed.

**Phase 4: Q-neighbor linearization.** For each off-diagonal Q-matrix neighbor k of the fixed variable, the quadratic coupling is linearized into the neighbor's reduced cost by adding Q[varIndex, k] times the fixed value. The neighbor's Q-adjacency entry for the fixed variable is removed. If the variable has quadratic contributions and the solver requires warm-start recording, cxf_basis_warm (P3.16) may be called to create a warm-start eta vector before the fixing record. This linearization technique is standard for reducing QP subproblems during variable elimination (Maros, 2003, Section 14.3).

**Phase 5: Pricing notification.** The pricing subsystem is notified of the variable change via cxf_pricing_update_var (P3.17), which cascades dirty-marking to structurally adjacent constraints. The variable itself is also marked dirty via cxf_pricing_mark_dirty (P3.18).

**Phase 6: Activity bound propagation.** For each constraint containing the fixed variable (scanning the variable's CSC column), the function:
- Updates the constraint's activity by removing the variable's contribution
- Adjusts the constraint's minimum and maximum activity bounds based on the coefficient sign and the removed variable's bound contributions
- Updates the per-constraint unbounded variable counts when bounds cross the infinity threshold

**Phase 7: Matrix cleanup.** The variable's column length is set to an invalid sentinel to mark it inactive. The variable's entries are removed from the CSR representation of affected constraints. The variable's lower and upper bounds are both set to the fixed value.

**Thread Safety:** Not thread-safe. Must be called within a single-threaded simplex iteration context.

**Dependencies:**
- P1.04 (SolverState) - reads and modifies working arrays, constraint matrix (CSC and CSR), bounds, reduced costs, objective value, and eta management fields
- P1.08 (EtaVector) - creates variable-fixing eta records (Variant 2: compact or full)
- P2.01 (Product Form of the Inverse) - eta chain management and memory pool allocation
- P3.16 (Basis Operations) - cxf_basis_warm may be called for variables with quadratic objective contributions
- P3.17 (Pricing Core) - cxf_pricing_update_var notified after variable fixing
- P3.18 (Pricing Support) - cxf_pricing_mark_dirty called for the fixed variable

---

### cxf_pivot_primal

**Purpose:** Perform a primal simplex pivot that evaluates whether a variable can be safely fixed, computes the optimal fixing value using the primal criterion, and executes the fixing operation with feasibility checking.

**Signature:**
- Input: `env` : pointer-to-Environment - Environment for memory allocation services
- Input: `state` : pointer-to-SolverState - The solver's working state
- Input: `varIndex` : int - Index of the variable to evaluate for fixing
- Input: `tolerance` : double - Feasibility tolerance for range and coefficient impact checks
- Output: int - Zero on success (including no-op when fixing is not beneficial), the infeasibility code if the variable's range is too narrow, or the out-of-memory error code

**Preconditions:**
- The solver state must have valid constraint matrix data, variable bounds, and reduced costs
- The tolerance must be positive
- The variable must be eligible for primal pivot evaluation (not already fixed or inactive)

**Postconditions:**
- If the variable's bound range is too narrow (less than twice the tolerance), the function returns the infeasibility code and records the problematic variable index for diagnostic reporting
- If the variable's maximum constraint coefficient times its bound range exceeds the tolerance, the variable has too much constraint impact to fix safely; the function returns success without taking action
- Otherwise, the variable has been fixed at the computed target value with all the postconditions of cxf_pivot_bound

**Side Effects:**
- Same as cxf_pivot_bound when the variable is actually fixed
- Records the problematic variable index in the solver state when returning the infeasibility code
- No side effects when returning with no action taken (coefficient impact too large)

**Error Conditions:**
- Variable bound range too narrow -> returns the infeasibility error code with diagnostic variable index stored in the solver state
- Memory allocation failure during eta creation -> returns the out-of-memory error code
- Variable has too much constraint impact -> returns success (no-op, not an error)

**Behavioral Description:**
This function extends cxf_pivot_bound with feasibility checks and dynamic target value computation using the primal simplex criterion. It is used during simplex presolve and bound-tightening phases where variables with small ranges or negligible impact can be eliminated.

**Phase 1: Bound range feasibility.** The function checks whether the variable's bound range (upper bound minus lower bound) is large enough relative to the tolerance. If twice the tolerance meets or exceeds the range, the variable's bounds are effectively contradictory at the current numerical precision. The function records the variable index for diagnostic purposes and returns the infeasibility code.

**Phase 2: Coefficient impact scan.** The function scans the variable's CSC column to find the maximum absolute coefficient (using a two-element-per-iteration scan for efficiency). If the product of this maximum coefficient and the variable's bound range exceeds the tolerance, fixing the variable would meaningfully perturb constraint activities, and the function returns without action. This check prevents fixing variables that would cause significant feasibility disturbance.

**Phase 3: Target value selection (primal criterion).** The fixing value is determined by the variable's reduced cost magnitude:
- If the product of the reduced cost and the bound range is significant relative to a scaled tolerance threshold: the variable is fixed at its lower bound (when the reduced cost is positive, meaning decreasing the variable improves the objective for minimization) or at its upper bound (when the reduced cost is negative). This is the classical primal simplex criterion (Dantzig, 1963, Chapter 5).
- If the reduced cost is small (the variable's objective contribution is insensitive to its position within the range): the variable is fixed at the midpoint of its bounds, or at zero if zero lies within the bound range. Midpoint fixing minimizes the maximum constraint perturbation.

**Phase 4: Variable fixing.** The function executes the same fix-and-update sequence as cxf_pivot_bound: eta record creation, objective updates (linear and quadratic), Q-neighbor linearization, pricing notification, activity bound propagation, and matrix cleanup.

**Thread Safety:** Not thread-safe. Must be called within a single-threaded simplex iteration context.

**Dependencies:**
- All dependencies of cxf_pivot_bound
- P1.04 (SolverState) - additionally reads reduced costs and bounds for the primal criterion computation

---

### cxf_pivot_special

**Purpose:** Handle non-standard pivot cases during simplex iterations, including unboundedness detection, constraint row elimination for singleton or dominated variables, and bound flipping to the opposite bound.

**Signature:**
- Input: `env` : pointer-to-Environment - Environment for memory allocation services
- Input: `state` : pointer-to-SolverState - The solver's working state
- Input: `varIndex` : int - Index of the variable to evaluate
- Input: `lowerLimit` : double - Lower bound threshold for unboundedness detection
- Input: `upperLimit` : double - Upper bound threshold for unboundedness detection
- Output: int - Zero on success, the unbounded code if the problem is detected as unbounded, or the out-of-memory error code

**Preconditions:**
- The solver state must have valid constraint matrix data, variable bounds, reduced costs, and variable flag arrays
- The variable must be eligible for special pivot evaluation

**Postconditions:**
- If the variable triggers unboundedness detection: the function returns the unbounded code and records the unbounded variable index for diagnostic reporting
- If the variable is eliminated via row elimination: all inequality constraints containing only this variable as the sole eligible variable have been marked inactive, an elimination eta record has been created, and all affected neighboring variables have been notified via pricing dirty-marking
- If the variable is bound-flipped: the variable has been fixed at its opposite bound via cxf_pivot_bound with all its attendant postconditions
- If no action is applicable: the function returns success with no state modification

**Side Effects:**
- May create eta records (via cxf_pivot_bound for bound flips, or via the variable-fixing utility for row elimination)
- May mark constraints as inactive (lazy deletion via status sentinel)
- May update neighboring variables' constraint counts and pricing dirty flags during row elimination
- Records the unbounded variable index in the solver state when returning the unbounded code

**Error Conditions:**
- Problem is unbounded in the beneficial direction -> returns the unbounded error code
- Memory allocation failure during eta or elimination record creation -> returns the out-of-memory error code

**Behavioral Description:**
This function handles the three special cases that fall outside the standard primal/dual simplex pivot pattern. It is called during simplex presolve and simplification phases for variables that meet special criteria.

**Phase 1: Movement direction determination.** The function examines the variable's reduced cost to determine which direction of movement would improve the objective. For minimization, a positive reduced cost favors decreasing the variable (fixing at the lower bound); a negative reduced cost favors increasing the variable (fixing at the upper bound).

**Phase 2: Special constraint validation.** If the variable participates in special constraint types -- SOS constraints, indicator constraints, or other structured constraints identified by variable flags -- the function delegates to cxf_special_check (P3.08) to determine whether the pivot is permitted. If the validation rejects the pivot, the function returns with no action. For variables with quadratic objective terms, the validation additionally checks positive-semidefiniteness conditions on the Q-matrix diagonal and scans Q-neighbors for compatibility.

**Phase 3: Equality constraint check.** The function scans the variable's column to determine whether it appears in any equality constraint. If so, the variable cannot be eliminated by this function, because fixing a variable that appears in an equality constraint requires careful algebraic handling beyond simple row elimination. The function returns with no action.

**Phase 4: Action determination.** Based on the beneficial movement direction, the finiteness of the variable's bounds, and the magnitude of the reduced cost, the function selects one of three actions:

| Movement Direction | Bound in Direction | Reduced Cost Magnitude | Action |
|----|----|----|------|
| Decrease | Finite lower bound | Any | Bound flip to lower bound |
| Decrease | Infinite lower bound | Large (exceeds limit) | Unbounded detection |
| Decrease | Infinite lower bound | Small | Row elimination and variable fixing |
| Increase | Finite upper bound | Any | Bound flip to upper bound |
| Increase | Infinite upper bound | Large (exceeds limit) | Unbounded detection |
| Increase | Infinite upper bound | Small | Row elimination and variable fixing |

A special mode flag on the solver state can disable unboundedness detection; this is used during Phase I of two-phase simplex, where unboundedness of the auxiliary problem does not imply unboundedness of the original problem.

**Phase 5a: Bound flip execution.** When a finite bound exists in the beneficial direction, the function delegates to cxf_pivot_bound (this module) to fix the variable at that bound. This performs all the standard fixing operations (eta record, objective update, pricing notification, activity bound propagation, matrix cleanup).

**Phase 5b: Unboundedness reporting.** When the variable has an infinite bound in the beneficial direction and its reduced cost exceeds the threshold parameters (lowerLimit, upperLimit), the function records the variable index for diagnostic reporting and returns the unbounded code.

**Phase 5c: Row elimination.** When the variable has an infinite bound but a small reduced cost (meaning the objective is insensitive to this variable's value), the function eliminates all inequality constraints in which the variable participates:
1. For each constraint containing the variable, the constraint is marked inactive via a status sentinel (lazy deletion).
2. The CSR representation of the constraint is cleaned: all variable entries in the eliminated rows have their column indices invalidated.
3. Neighboring variables in the eliminated rows have their per-variable constraint counts decremented and are marked dirty for repricing via cxf_pricing_mark_dirty (P3.18).
4. A specialized elimination eta record is created via cxf_fix_variable (P3.35) to support later reconstruction if needed.
5. The variable itself is marked as inactive in the constraint matrix.

This row elimination technique is a standard simplex presolve reduction: if a variable appears only in inequality constraints and its objective contribution is negligible, those constraints can be removed without affecting the optimal solution (Andersen and Andersen, 1995; Maros, 2003, Section 10.2).

**Thread Safety:** Not thread-safe. Must be called within a single-threaded simplex iteration context.

**Dependencies:**
- cxf_pivot_bound (this module) - called for bound flip execution
- P3.08 (Data Validation) - cxf_special_check validates special constraint compatibility
- P3.35 (Query Utilities) - cxf_fix_variable creates elimination eta records during row elimination
- P3.18 (Pricing Support) - cxf_pricing_mark_dirty called for affected variables during row elimination
- P1.04 (SolverState) - reads and modifies constraint status, variable flags, reduced costs, and matrix representations

---

### cxf_pivot_update

**Purpose:** Incrementally update constraint activity bounds when a variable's lower or upper bounds change, avoiding full activity recomputation by applying only the delta from the bound changes.

**Signature:**
- Input: `state` : pointer-to-SolverState - The solver's working state (passed by value rather than by reference, unlike the other functions in this module)
- Input: `colIndex` : int - Index of the variable whose bounds changed
- Input: `oldLowerBound` : double - Previous lower bound value
- Input: `newLowerBound` : double - Updated lower bound value
- Input: `oldUpperBound` : double - Previous upper bound value
- Input: `newUpperBound` : double - Updated upper bound value
- Input: `infinityThreshold` : double - Threshold beyond which a bound is treated as infinite
- Output: void

**Preconditions:**
- The solver state must have valid activity bound arrays and constraint status information
- The column index must reference a valid variable in the constraint matrix
- The old bound values must be consistent with the previous state of the activity bounds

**Postconditions:**
- All constraint activity bounds (minimum and maximum) affected by the variable's bound change have been updated incrementally
- The per-constraint unbounded variable counts have been adjusted for any bounds that cross the infinity threshold
- The updates are numerically conservative: when floating-point cancellation is detected, activity bounds are rounded in the safe direction (widened rather than tightened)
- The work counter has been updated proportionally to the number of nonzeros in the variable's column

**Side Effects:**
- Modifies constraint activity bound arrays (minimum and maximum activity)
- Modifies per-constraint unbounded variable counts when bounds cross the infinity threshold
- Updates the work counter
- No memory allocation

**Error Conditions:**
- None. Inactive constraints and slack variable indicators are skipped silently.

**Behavioral Description:**
This function is a performance-critical incremental update mechanism. When a variable's bounds change (due to bound tightening, preprocessing, or pivot operations), the constraint activity bounds must be updated accordingly. A naive approach would recompute each affected constraint's activity bounds from scratch at O(nnz per row) cost. This function instead performs the update incrementally at O(nnz in column) total cost, updating only the delta from the old bounds to the new bounds.

**Case dispatch.** The function first determines which bounds changed by comparing old and new values:
- **Both bounds changed:** The most complex path; for each affected constraint, both the minimum and maximum activity bounds are updated.
- **Lower bound only changed:** For each affected constraint, one activity bound is updated (which one depends on the coefficient sign in the constraint).
- **Upper bound only changed:** Symmetric to the lower-bound-only case.
- **Neither bound changed:** Early return with no work.

**Per-constraint update logic.** For each nonzero coefficient in the variable's column (scanning the CSC representation):

1. **Skip conditions.** Slack variable entries and inactive or eliminated constraints are skipped.

2. **Coefficient sign determines the bound-to-activity mapping.** This follows the standard constraint propagation rules:
   - **Positive coefficient:** The variable's lower bound contributes to the constraint's minimum activity; the upper bound contributes to the maximum activity.
   - **Negative coefficient:** The mapping is reversed -- the lower bound (multiplied by the negative coefficient) contributes to the maximum activity, and the upper bound contributes to the minimum activity.

3. **Delta application.** The difference between the old and new bound contributions is applied incrementally to the appropriate activity bound.

4. **Infinity transitions.** When a bound crosses the infinity threshold, three sub-cases arise:
   - Finite-to-finite: normal arithmetic delta.
   - Finite-to-infinite: increment the per-constraint unbounded variable count, remove the old finite contribution from the activity bound.
   - Infinite-to-finite: decrement the unbounded variable count, add the new finite contribution to the activity bound.

**Numerical stability (cancellation detection).** After each incremental update, the function checks for floating-point cancellation -- a condition where adding a small delta to a large existing value loses significant precision. The detection tests whether the addition is reversible: after computing `result = existing + delta`, the function verifies that `(result - delta)` recovers the original value. When cancellation is detected, the function applies conservative rounding:
- For maximum activity bounds: the result is multiplied by a factor slightly less than one, producing a safe underestimate.
- For minimum activity bounds: the result is multiplied by a factor slightly greater than one, producing a safe overestimate.

This conservative rounding ensures that activity bounds are always "safe" -- they may be slightly wider than the exact values, but never tighter. This prevents false infeasibility detection while maintaining the bounds' usefulness for constraint propagation and implied bound tightening (Higham, 2002, Chapter 4; see also Savelsbergh, 1994, for the general technique of activity-based constraint propagation in LP solvers).

**Thread Safety:** Not thread-safe. Must be called within a single-threaded simplex iteration context.

**Dependencies:**
- P1.04 (SolverState) - reads CSC matrix data, constraint status arrays, and modifies activity bound arrays and unbounded variable counts

---

## Module-Level Behavioral Notes

### Function Hierarchy

The five functions form a clear hierarchy of increasing abstraction:

1. **cxf_pivot_check** is a pure computation function that determines feasible step length bounds. It has no side effects beyond work counter updates. It requires no environment pointer and performs no memory allocation.

2. **cxf_pivot_update** is an incremental maintenance function for activity bounds. It modifies activity data but does not create eta records or interact with the pricing subsystem. It also requires no environment pointer.

3. **cxf_pivot_bound** is the core variable-fixing operation. It creates eta records, updates the objective (linear and quadratic), propagates activity changes, and notifies pricing. All other fixing functions ultimately use its logic.

4. **cxf_pivot_primal** adds feasibility checking and the primal criterion target selection on top of cxf_pivot_bound's fixing logic. It is the entry point for primal simplex variable elimination with safety guards.

5. **cxf_pivot_special** handles the three non-standard cases (unboundedness, row elimination, bound flip) and may delegate to cxf_pivot_bound for the bound-flip case.

### Relationship to the Simplex Iteration Loop

These functions are called from the simplex iteration module (P3.20) and related solver phases:
- **cxf_pivot_check** is called during the ratio test phase to compute initial step length bounds for the Harris two-pass procedure
- **cxf_pivot_bound** is called during simplex presolve, variable elimination, simplex finalization, and bound-flip execution
- **cxf_pivot_primal** is called during simplex presolve and bound-tightening phases for candidate variable elimination
- **cxf_pivot_special** is called during simplex simplification phases for variables that may be unbounded, singleton, or dominated
- **cxf_pivot_update** is called whenever bound tightening occurs during preprocessing or bound propagation

The standard simplex pivot (basis exchange of entering and leaving variables) is handled by cxf_pivot_with_eta in the Basis Operations module (P3.16), which creates the pivot eta record. The functions in this module handle the variable-fixing, bound-flipping, and preprocessing aspects of pivot operations, which are distinct from (but complementary to) the basis exchange mechanism.

### Eta Vector Types Created by This Module

| Function | Eta Type | Variant (P1.08) | Purpose |
|----------|----------|------------------|---------|
| cxf_pivot_bound | VARIABLE_FIX | Variant 2 (compact or full) | Records variable fixed at bound |
| cxf_pivot_primal | VARIABLE_FIX | Variant 2 (via pivot_bound logic) | Records variable fixed via primal criterion |
| cxf_pivot_special | VARIABLE_FIX or ELIMINATION | Variant 2 (via pivot_bound) or specialized record (via cxf_fix_variable) | Records bound flip or row elimination |

Note that the standard simplex pivot eta (Variant 1, type = PIVOT) is created by cxf_pivot_with_eta in P3.16, not by functions in this module. All eta records are allocated from the SolverState's memory pool via bump allocation and are never individually freed. The entire pool is released in bulk during basis refactorization or simplex cleanup (see P2.01).

### Dual Matrix Representation

The pivot operations in this module maintain both column-major (CSC) and row-major (CSR) representations of the constraint matrix simultaneously. When a variable is fixed:
- The CSC representation is used for scanning the variable's column (to find affected constraints and compute activity deltas)
- The CSR representation is used for cleaning up variable entries from constraint rows
- Both representations must remain consistent after each operation

This dual representation enables efficient column scanning (for ratio tests and activity updates) and row scanning (for eta extraction and constraint processing) without requiring transposition, which is standard practice in production simplex implementations (Maros, 2003, Section 4.2).

### Lazy Deletion Pattern

Variables and constraints are not physically removed from the matrix during pivot operations. Instead:
- Constraints are marked inactive by setting their status to a negative sentinel value
- Variables are marked inactive by setting their column length to an invalid sentinel
- Column entries in the CSR representation are invalidated by setting column indices to negative sentinels

All functions in this module check for these sentinels and skip inactive entries. Physical cleanup of the matrix occurs during basis refactorization, when the sparse data structures are rebuilt from the active entries. This lazy deletion pattern avoids the cost of compacting sparse data structures during the inner simplex loop.

### Activity Bound Propagation

Four arrays maintained on the SolverState track constraint activity bounds:
- **Minimum activity:** The smallest possible constraint activity given the current variable bounds
- **Maximum activity:** The largest possible constraint activity given the current variable bounds
- **Negative unbounded count:** Number of variables contributing negatively infinite activity to the constraint
- **Positive unbounded count:** Number of variables contributing positively infinite activity to the constraint

These bounds enable O(1) detection of whether a constraint is certainly feasible (activity range is within constraint bounds), certainly infeasible (activity range is entirely outside constraint bounds), or requires further investigation. cxf_pivot_bound and cxf_pivot_update maintain these bounds incrementally as variables are fixed or have their bounds tightened. This is a standard technique for efficient constraint propagation in LP solvers (Savelsbergh, 1994; Achterberg et al., 2020).

### Quadratic and Piecewise-Linear Support

The pivot operations are not limited to pure linear programming:
- **Quadratic objectives:** When a variable with Q-matrix entries is fixed, the quadratic coupling is linearized into neighboring variables' reduced costs. This is handled within cxf_pivot_bound (Phases 3-4) and inherited by cxf_pivot_primal.
- **Piecewise-linear variables:** Variables flagged as PWL require breakpoint segment lookup before fixing, to determine the correct linearization within the active segment.
- **Special constraints:** SOS constraints, indicator constraints, and other structured constraints are validated by cxf_special_check (P3.08) before allowing pivot operations (cxf_pivot_special, Phase 2).

### Return Code Conventions

| Code | Meaning | Functions |
|------|---------|-----------|
| Success (zero) | Operation completed successfully, or no-op when no action is needed | All five functions |
| Infeasibility code | Variable bound range too narrow for the given tolerance | cxf_pivot_primal only |
| Unbounded code | Problem detected as unbounded in the beneficial direction | cxf_pivot_special only |
| Out-of-memory code | Eta vector or elimination record allocation failed | cxf_pivot_bound, cxf_pivot_primal, cxf_pivot_special |

### Thread Safety Summary

| Function | Thread Safety | Notes |
|----------|---------------|-------|
| cxf_pivot_check | Not thread-safe | Reads solver state, updates work counter |
| cxf_pivot_bound | Not thread-safe | Modifies solver state, eta chain, pricing state, constraint matrix |
| cxf_pivot_primal | Not thread-safe | Same as cxf_pivot_bound plus diagnostic state |
| cxf_pivot_special | Not thread-safe | May modify constraint status, matrix representations, and eta chain |
| cxf_pivot_update | Not thread-safe | Modifies activity bound arrays and unbounded variable counts |

All functions operate within a single-threaded simplex solve. Thread safety for concurrent solves is achieved at the model level by creating independent solver instances, each with its own SolverState.

---

## Verification Checklist

```
[x] No hex addresses or offsets
[x] No Ghidra artifacts (param_, local_, iVar, DAT_, FUN_)
[x] No binary-specific constants or magic numbers
[x] No copied code fragments from analyzed source
[x] All descriptions are behavioral, not implementational
[x] All data structures described semantically using Layer 1/2 references
[x] Explicit cross-references to P1.04 (SolverState), P1.08 (EtaVector), P2.01 (PFI), P2.4 (Harris Ratio Test)
[x] [UNDETERMINED] used for unknowns (reserved parameter in cxf_pivot_check)
[x] Passes the Clean Room Test: could be written without seeing the binary
```

## References

- Achterberg, T., Bixby, R.E., Gu, Z., Rothberg, E., and Weninger, D. (2020). "Presolve Reductions in Linear Programming." *INFORMS Journal on Computing*, 32(2):473-506.
- Andersen, E.D. and Andersen, K.D. (1995). "Presolving in Linear Programming." *Mathematical Programming*, 71(2):221-245.
- Dantzig, G.B. (1963). *Linear Programming and Extensions*. Princeton University Press.
- Forrest, J.J.H. and Goldfarb, D. (1992). "Steepest-edge simplex algorithms for linear programming." *Mathematical Programming*, 57(1):341-374.
- Harris, P.M.J. (1973). "Pivot selection methods of the Devex LP code." *Mathematical Programming*, 5(1):1-28.
- Higham, N.J. (2002). *Accuracy and Stability of Numerical Algorithms*. Second Edition. SIAM.
- Maros, I. (2003). *Computational Techniques of the Simplex Method*. Springer. International Series in Operations Research and Management Science, Vol. 61.
- Savelsbergh, M.W.P. (1994). "Preprocessing and Probing Techniques for Linear Programming Problems." *ORSA Journal on Computing*, 6(4):445-454.
