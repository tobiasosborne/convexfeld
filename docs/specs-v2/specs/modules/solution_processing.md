# Module: Solution Processing

## Purpose

The Solution Processing module implements the post-solve pipeline that transforms raw solver output into user-accessible result data. After an LP, MIP, or barrier solver method has found a solution (or determined infeasibility/unboundedness), this module performs all steps necessary to make solution information available through the solver's public attribute API: binding result attributes to their storage locations, reversing presolve transformations, computing derived quantities such as optimality gaps and objective values, and managing the MIP solution pool.

The module embodies two key design patterns. First, it uses a **direct-pointer wiring pattern** in which attribute entries are linked to the memory locations where result values reside, enabling attribute queries to read solution data without function-call dispatch overhead. This wiring must be reconfigured after each solve because the storage locations may differ depending on the solve outcome (optimal, infeasible, MIP with multiple solutions, etc.). Second, it implements a **presolve reversal pattern** through which solutions obtained in the reduced (presolved) variable space are mapped back to the original problem's variable space, restoring values for variables that were eliminated, substituted, or aggregated during presolve. This reversal is a standard postsolve operation (Andersen and Andersen, 1995; Gondzio, 1997) that every LP solver with a presolve phase must provide.

The six functions span the full post-solve pipeline: attribute wiring for LP results (cxf_process_lp_solution), attribute wiring for MIP/general results (cxf_wire_result_attributes), presolve reversal (cxf_uncrush_solution), objective value evaluation (cxf_scale_objval), optimality gap computation (cxf_compute_gap), and solution pool management (cxf_copy_solution).

## Functions

### cxf_process_lp_solution

**Purpose:** Bind LP optimization result attributes to their storage locations in the solution information structure, enabling direct-pointer access through the attribute API.

**Signature:**
- Input: `model` : pointer-to-Model - The model whose LP solve has completed
- Output: pointer - Reference to the attribute entries array on success, or an uninitialized marker on early exit

**Preconditions:**
- The model must have completed an LP solve (the optimization status must be set)
- The model's attribute table must be initialized

**Postconditions:**
- Iteration count attributes (simplex, barrier, and first-order method iteration counts) are wired to the solution information structure's corresponding fields
- Node count attributes (node count, open node count, and time-open metric) are all wired to the model's node count storage (these share a single backing location, as LP solves always report zero nodes)
- Solution count and first-solution-node attributes are cleared to indicate zero solutions initially
- Objective value attributes (objective value, objective bound, continuous objective bound) are wired conditionally based on the optimization status:
  - For statuses indicating a valid solution exists (optimal or resource-limit termination): all three objective attributes are wired to the solution information structure's corresponding fields; for LP, the objective bound and continuous objective bound share the same backing field
  - For statuses indicating no valid solution (loaded, infeasible, unbounded, infeasible-or-unbounded): the function attempts to compute an objective value from presolve data under specific conditions; if computation is not possible, all three objective attributes are cleared (set to unavailable)

**Side Effects:**
- Modifies the direct-value pointer field of multiple attribute entries in the model's attribute table
- May compute an objective value and store it in the solution information structure (in the infeasibility diagnostic case)

**Error Conditions:**
- Null or missing attribute table -> returns early with an uninitialized marker
- Null or missing solution information structure -> returns early with an uninitialized marker

**Behavioral Description:**

The function proceeds through four phases:

1. **Validation.** The function verifies that the model has both an attribute table and a solution information structure. If either is absent, it returns an early-exit marker without modifying any state.

2. **Iteration and node count wiring.** The function wires six attributes to their backing storage:
   - Three iteration count attributes (simplex iteration count, barrier iteration count, and first-order method iteration count) are each wired to the corresponding field in the solution information structure.
   - Three node count attributes (node count, open node count, and time-open) are all wired to the same location on the model, since LP solves do not explore branch-and-bound nodes.
   - Two solution count attributes (solution count and first solution node) are set to unavailable (null pointer), indicating that no MIP-style solutions have been recorded.

3. **Status-dependent objective wiring (solution available).** If the optimization status indicates a valid solution exists (any status other than loaded, infeasible, infeasible-or-unbounded, or unbounded), the objective value, objective bound, and continuous objective bound attributes are all wired to their respective fields in the solution information structure. For LP results, the objective bound and continuous objective bound share the same field.

4. **Status-dependent objective wiring (no solution).** If the status indicates no valid solution, the function checks whether an objective value can be computed from presolve data for diagnostic purposes. This diagnostic computation requires all of the following conditions:
   - The infeasibility/unboundedness diagnostic parameter is enabled on the environment
   - The model has no integer variables (pure continuous LP)
   - No reduced (presolved) model copy exists
   - Presolve data is available and indicates an infeasible status
   - Presolve solution arrays are populated
   - The optimization status is either infeasible or infeasible-or-unbounded

   When all conditions are met, the function retrieves a solution vector from the presolve data (preferring a primary solution array, with a secondary array as fallback), prepares the solution, computes an objective value using the solution vector, applies the objective sense scaling (negating for maximization), stores the result in the solution information structure, and wires the objective attributes to the computed values. When the conditions are not met, the three objective attributes are cleared to unavailable.

The attribute wiring pattern used throughout is: look up the attribute by name in the attribute table's hash map, find the corresponding entry in the entries array, and set the entry's direct-value pointer to the target storage location. This allows the attribute retrieval API to return values by simply dereferencing the pointer, without invoking getter functions.

**Thread Safety:** Unsafe. Must be called from a single thread; the model must not be accessed concurrently.

**Dependencies:** Attribute table lookup and name initialization helpers; cxf_scale_objval (for diagnostic objective computation); a helper that checks whether the model contains integer variables.

---

### cxf_uncrush_solution

**Purpose:** Transform a solution vector from the presolved (reduced) variable space back to the original variable space, reversing the presolve transformations.

**Signature:**
- Input: `environment` : pointer-to-Environment - The environment for memory allocation
- Input: `presolve_data` : pointer-to-PresolveData - The presolve mapping information that records how variables were eliminated, substituted, or aggregated
- Input: `crushed_solution` : pointer-to-double-array - The solution vector in the presolved (reduced) variable space, or null if no presolved solution exists
- Input: `output_buffer` : pointer-to-double-array - Buffer to receive the solution in the original variable space
- Input: `copy_count` : int - The number of elements the caller wants written to the output buffer
- Output: int - Zero on success, or out-of-memory error code on allocation failure

**Preconditions:**
- The presolve data must be valid and contain the original variable count and the variable mapping information
- The output buffer must be large enough to hold at least `copy_count` double-precision values
- If `crushed_solution` is non-null, it must contain valid values for all presolved variables

**Postconditions:**
- On success: the first `min(copy_count, original_variable_count)` elements of the output buffer contain the uncrushed solution values
- On error: the output buffer contents are undefined; all temporary memory has been freed

**Side Effects:**
- May allocate and free a temporary buffer through the environment's memory allocator

**Error Conditions:**
- Memory allocation failure (when a temporary buffer is needed) -> returns out-of-memory error code

**Behavioral Description:**

The function reverses presolve transformations to restore solution values for all variables in the original (unreduced) model. During presolve (Andersen and Andersen, 1995), the solver reduces model complexity by eliminating fixed variables, removing redundant constraints, substituting implied bounds, and aggregating duplicate rows or columns. This creates a smaller "presolved" model that solves faster, but the solution must be mapped back to the original variable space before being returned to the user. This postsolve (uncrushing) operation restores eliminated variable values, computes substituted variable values from the remaining variables, and splits aggregated variables back to their original representations.

The function operates in one of two modes depending on the relationship between the copy count and the original variable count:

1. **Direct mode** (copy count >= original variable count): The output buffer is large enough to hold the complete uncrushed solution. The core uncrushing transformation is applied directly into the output buffer without any temporary allocation.

2. **Partial copy mode** (copy count < original variable count): The output buffer is smaller than the full uncrushed result. The function allocates a temporary buffer large enough for the complete uncrushed solution, applies the core uncrushing transformation into the temporary buffer, copies only the first `copy_count` elements to the output buffer, and frees the temporary buffer. This mode supports callers that need only a subset of the original variables (for example, when populating a pre-allocated array that excludes slack variables).

In both modes, the actual variable-by-variable transformation logic (restoring eliminated variables, computing substituted values, expanding aggregated variables) is delegated to a core uncrushing helper function. This wrapper is responsible solely for memory management and partial-copy semantics.

The function handles the edge case of an empty model (zero original variables) by skipping allocation and transformation entirely.

**Thread Safety:** Unsafe. Not thread-safe; depends on environment memory allocator which may not be thread-safe.

**Dependencies:** Core uncrushing transformation function (cxf_uncrush_solution_core); environment memory allocation and deallocation functions.

---

### cxf_wire_result_attributes

**Purpose:** Connect optimization result attributes to their storage locations in the solver result state after MIP or general optimization completes, enabling direct-pointer attribute access.

**Signature:**
- Input: `model` : pointer-to-Model - The model whose optimization has completed
- Output: int64 - Reference to the attribute entries array on success, or an uninitialized marker on validation failure

**Preconditions:**
- The model must have completed optimization (status and result state must be populated)
- The model's attribute table and solver result state must both be initialized

**Postconditions:**
- All standard result attributes are wired to their backing storage locations
- Objective-related attributes are wired differently depending on the solve outcome mode (see behavioral description)
- For the infeasible/unbounded mode, objective bound fields are initialized to a direction-dependent infinity value (positive infinity times the optimization direction, so that minimization gets positive infinity and maximization gets negative infinity)
- For modes with a solution count available, the MIP gap is computed and stored

**Side Effects:**
- Modifies the direct-value pointer (and for array attributes, the size pointer) of multiple attribute entries in the model's attribute table
- May modify objective bound and MIP gap fields in the solver result state
- Calls the MIP gap computation function

**Error Conditions:**
- Null attribute table -> returns uninitialized marker
- Null solver result state -> returns uninitialized marker

**Behavioral Description:**

This function is the MIP/general counterpart to cxf_process_lp_solution. While cxf_process_lp_solution handles the simpler LP case (binding iteration counts and a single objective value), cxf_wire_result_attributes handles the full result set including solution arrays, MIP gap, solution pool bounds, and mode-dependent objective attribute wiring. The function proceeds through seven phases:

1. **Validation.** Checks that the attribute table and solver result state are both present; returns an uninitialized marker if either is missing.

2. **Iteration count wiring.** Wires four iteration count attributes: simplex iteration count, initial iteration count (for reoptimization tracking), barrier iteration count, and first-order method iteration count. Each is wired to its dedicated field in the solver result state.

3. **Node count wiring.** Wires three attributes (node count, open node count, and time-open) to their respective fields in the solver result state. Unlike cxf_process_lp_solution (which shares a single backing location), each node count attribute here has its own field, reflecting the meaningful node tracking that occurs during MIP optimization.

4. **Solution count initialization.** Wires the solution count and first-solution-node attributes to unavailable (null pointer), as these will be populated later during solution pool management.

5. **Solution array wiring.** Wires the primal solution values (X), slack values (Slack), and optionally quadratic constraint slack values (QCSlack) as array attributes. Each array attribute receives both a direct-value pointer (pointing to the array in the solver result state) and a size pointer (pointing to the corresponding dimension in the matrix data: variable count for X, constraint count for Slack, quadratic constraint count for QCSlack). The QCSlack wiring is conditional on the model having quadratic constraints.

6. **Solve mode dispatch.** The function examines the solve mode stored in the solver result state and selects one of three wiring configurations for objective-related attributes:

   **Mode: Optimal / Cutoff / Iteration-Limit** (solve modes 1, 4, or 5):
   - Objective value is wired to the solver result state's objective field if at least one solution exists; otherwise wired to unavailable
   - Objective bound, continuous objective bound, and pool objective bound are all wired to the model's own bound field (a single shared location, as these are identical for non-MIP or trivially-bounded results)

   **Mode: Infeasible/Unbounded** (solve mode 3):
   - Three objective bound fields on the solver result state are initialized to a direction-dependent infinity value (the product of the optimization direction and a large constant representing solver infinity), ensuring that the bound reflects the worst-case value for the given optimization direction
   - Objective value is wired to unavailable (no solution exists)
   - Objective bound, continuous objective bound, and pool objective bound are each wired to their distinct fields in the solver result state
   - MIP gap is wired to the model's bound field

   **Mode: General MIP** (all other solve modes):
   - The MIP gap is computed by calling cxf_compute_gap with the optimization direction, the objective bound, and the objective value from the solver result state; the computed gap is stored in the solver result state
   - Objective value, objective bound, continuous objective bound, and pool objective bound are each wired to their respective fields in the solver result state
   - MIP gap is wired to the computed gap field in the solver result state

**Thread Safety:** Unsafe. Must be called from a single thread after optimization completes.

**Dependencies:** Attribute table lookup and name initialization helpers; cxf_compute_gap (for MIP gap computation in the general MIP mode).

---

### cxf_compute_gap

**Purpose:** Compute the relative optimality gap between an incumbent solution's objective value and the best dual bound.

**Signature:**
- Input: `optimization_direction` : double - The optimization direction: positive for minimization, negative for maximization
- Input: `best_bound` : double - The best dual bound from the branch-and-bound tree (lower bound for minimization, upper bound for maximization)
- Input: `objective_value` : double - The incumbent solution's objective value
- Output: double - The relative optimality gap as a non-negative fraction, or solver infinity if the gap is undefined or unbounded

**Preconditions:**
- None; the function handles all edge cases internally

**Postconditions:**
- Returns a non-negative value representing the relative optimality gap
- Returns zero when the solution is effectively optimal (gap within absolute tolerance)
- Returns solver infinity when the gap cannot be meaningfully computed

**Side Effects:**
- None (pure function with no state modification)

**Error Conditions:**
- No error conditions; all inputs are handled gracefully

**Behavioral Description:**

This function computes the standard MIP optimality gap as documented in the ConvexFeld Reference Manual. The gap measures how far the current best solution (incumbent) is from being provably optimal, expressed as a fraction of the objective value. The formula is:

```
gap = |objective_value - best_bound| / |objective_value|
```

This is the standard relative gap formula used in branch-and-bound algorithms (Wolsey, 1998; Nemhauser and Wolsey, 1988). It provides a scale-invariant measure of solution quality that is comparable across problems of different magnitudes.

The function handles five cases in priority order:

1. **Infinite bound:** If the best bound equals solver infinity (indicating that no valid bound has been established yet, as happens early in MIP optimization before any nodes are processed), the function returns solver infinity.

2. **Infinite objective or bound:** If the absolute value of either the objective value or the best bound is at or above the solver infinity threshold, the function returns solver infinity. This handles cases where the problem is unbounded or the values are numerically unreliable.

3. **Effectively optimal (absolute tolerance check):** The function checks whether the signed gap (adjusted for optimization direction) is within an absolute tolerance. Specifically, it tests whether `direction * objective_value - tolerance < direction * best_bound`. For minimization, this reduces to checking whether `objective_value - best_bound < tolerance`; for maximization, it checks whether `best_bound - objective_value < tolerance`. If the gap is within the absolute tolerance, the function returns zero, indicating that the solution is effectively optimal. This absolute tolerance check prevents near-zero relative gaps from being computed when both the objective and bound are close to each other, which would be numerically unreliable.

4. **Denominator too small:** If the absolute value of the objective value is below a minimum threshold (a small positive constant on the order of the solver's numerical tolerance), the function returns solver infinity. This prevents division by near-zero, which would produce a meaningless or infinite relative gap.

5. **Normal computation:** The function computes `(objective_value - best_bound) / objective_value` and returns the absolute value of the result. Taking the absolute value ensures a non-negative result regardless of optimization direction. For minimization, the incumbent is typically greater than the lower bound, so the raw quotient is positive; for maximization, the incumbent is typically less than the upper bound, so the raw quotient is negative, and the absolute value corrects this.

The absolute tolerance and minimum denominator threshold are small positive constants comparable in magnitude to the solver's standard numerical tolerances.

**Thread Safety:** Safe. This is a pure function with no state access.

**Dependencies:** None (leaf function; uses only arithmetic operations and absolute-value computation).

---

### cxf_scale_objval

**Purpose:** Evaluate the objective function for a given solution vector, accounting for linear terms, quadratic terms, piecewise-linear objectives, column scaling, global objective scaling, and the objective constant.

**Signature:**
- Input: `model` : pointer-to-Model - The model containing the objective function definition
- Input: `solution` : pointer-to-double-array - A solution vector containing one value per variable
- Output: double - The computed objective function value in the original (unscaled) problem space, including the objective constant

**Preconditions:**
- The model must have valid matrix data with objective coefficients populated
- The solution array must contain at least as many elements as the model has variables
- If quadratic terms are present, the quadratic term arrays must be consistent

**Postconditions:**
- Returns the correct objective value as if computed by: `constant + (1/globalScale) * [c'x + 0.5 * x'Qx + PWL(x)]` where x may be column-scaled

**Side Effects:**
- None (pure computation)

**Error Conditions:**
- No explicit error handling; assumes valid inputs

**Behavioral Description:**

This function evaluates the full objective function for a given solution vector, handling all objective types supported by the solver. The evaluation accounts for the solver's internal scaling transformations and produces a result in the user's original problem space. The computation proceeds through up to four stages, depending on the problem type:

1. **Matrix selection.** The function retrieves the model's matrix data, preferring the working (potentially scaled) matrix if one exists, falling back to the primary (original) matrix. This ensures that the evaluation uses the correct coefficient values for the scaling state of the solution.

2. **Objective coefficient selection.** The function uses the standard linear objective coefficients, but if an alternative objective coefficient array is present (used during multi-objective optimization), the alternative coefficients take precedence.

3. **Linear term evaluation.** The linear objective contribution is computed as the dot product of the objective coefficient vector and the solution vector: `sum(c[j] * x[j])` for all variables j. If column scaling is active, each term is divided by the corresponding column scale factor: `sum(c[j] * x[j] / s[j])`. The implementation uses loop unrolling for computational efficiency.

4. **Piecewise-linear term evaluation.** If the model has piecewise-linear (PWL) objective terms (Beale and Tomlin, 1970; Padberg, 2000), the function evaluates the PWL contribution for each variable that has a piecewise-linear cost function defined. For each such variable:
   - The variable's solution value is retrieved (and divided by the column scale factor if scaling is active)
   - A linear search through the breakpoint array finds the segment containing the solution value
   - The contribution is computed as `slope * x + intercept` for the identified segment
   - At segment boundaries (within a small numerical tolerance), the function evaluates the adjacent segment(s) and selects the contribution that yields the better (lower for minimization) objective value, providing numerical stability at breakpoints
   - Variables without PWL terms use their standard linear coefficient instead

5. **Quadratic term evaluation.** If the model has quadratic objective terms (as in QP: `min c'x + 0.5 * x'Qx`), the quadratic contribution is computed. The quadratic objective is stored in coordinate format with row index pairs and coefficient values. Each term contributes `0.5 * Q[k] * x[i] * x[j]` where i and j are the variable indices for term k and Q[k] is the coefficient. The standard factor of one-half follows the convention that Q represents the full Hessian matrix (Nocedal and Wright, 2006). If column scaling is active, each variable value is divided by its column scale factor before the product. The implementation uses loop unrolling for the common unscaled case.

6. **Global objective unscaling.** If the solver applied a global objective scaling factor for numerical conditioning, the accumulated objective value is divided by this factor to reverse the scaling.

7. **Objective constant addition.** The objective constant (a fixed offset specified by the user as part of the objective function) is added to produce the final result.

The overall formula is:

```
result = constant + (1/globalScale) * [linearTerms + quadraticTerms + pwlTerms]
```

where each component accounts for column scaling as appropriate.

**Thread Safety:** Safe if the model is not concurrently modified. The function performs read-only access to model data.

**Dependencies:** None (leaf function; purely computational using model matrix data).

---

### cxf_copy_solution

**Purpose:** Add a new feasible solution to the MIP solution pool, maintaining solutions in sorted order by objective value with deterministic tie-breaking, and enforcing pool size and gap limits.

**Signature:**
- Input: `model` : pointer-to-Model - The model whose solution pool is being managed
- Input: `solution` : pointer-to-double-array - The solution vector to add (one value per variable)
- Input: `solution_added` : pointer-to-int (nullable) - Output flag set to 1 if the solution was added, 0 if rejected as a duplicate
- Output: int - Zero on success, or out-of-memory error code on allocation failure

**Preconditions:**
- The model must have valid matrix data with variable counts
- The solution array must contain valid values for all variables
- The model's solution pool structure must be accessible

**Postconditions:**
- On success: the solution has been inserted into the pool in the correct sorted position (or rejected as a duplicate), pool size limits have been enforced, gap-based pruning has been applied, the solution count has been updated, and the output flag reflects whether the solution was added
- On out-of-memory: partial state may have been modified; the final solution count is still updated before return

**Side Effects:**
- May initialize the solution pool on first use (allocating all pool arrays)
- Allocates memory for a copy of the solution vector
- May reallocate pool arrays when capacity is exceeded
- May free solution vectors that are pruned by size or gap limits
- Updates the pool's best objective metric for pruning decisions
- May update integer solution tracking arrays

**Error Conditions:**
- Memory allocation failure during pool initialization -> returns out-of-memory error code
- Memory allocation failure during capacity growth -> returns out-of-memory error code
- Memory allocation failure for solution copy -> returns out-of-memory error code

**Behavioral Description:**

This function manages the MIP solution pool, a sorted collection of feasible solutions discovered during branch-and-bound search. The pool supports the ConvexFeld PoolSolutions, PoolGap, and PoolGapAbs parameters, enabling users to collect multiple high-quality solutions rather than just the single best. Solutions are maintained in ascending objective order (for minimization) to enable efficient pruning of suboptimal solutions. The function implements the solution pool management pattern described in the ConvexFeld documentation for the PoolSolutions parameter.

The function proceeds through eleven phases:

1. **Pool initialization (lazy).** If the pool's allocated capacity is zero (indicating first use), the function performs full initialization: frees any stale pool data from a previous solve, allocates fresh arrays for solution vector pointers, weighted metrics, and objective values with a default initial capacity, and optionally allocates integer solution tracking arrays if the model has integer variables. Integer solution objective values are initialized with a sentinel marker indicating they have not yet been populated.

2. **Objective computation.** The solution's objective value is computed by calling cxf_scale_objval, which evaluates the full objective function (linear, quadratic, piecewise-linear terms with scaling).

3. **Solution fingerprint computation.** A weighted position metric is computed for the solution as a deterministic fingerprint: `metric = sum(x[j] / (j + 1))` for all variables j. This metric provides a unique, position-dependent signature for each solution vector, ensuring that solutions with identical objective values can be ordered deterministically. The weighting by inverse position gives higher influence to earlier variables.

4. **Insertion position search.** The function searches the existing sorted pool to find the correct insertion position for the new solution. Solutions are sorted primarily by objective value (ascending for minimization). When objectives are equal, the weighted metric is used as a secondary sort key (ascending). When both objective and metric are equal, a lexicographic comparison of the full solution vectors determines ordering. If the lexicographic comparison finds an exact duplicate (all variable values identical), the solution is rejected and the function skips to the finalization phase.

5. **Capacity growth.** If the pool has reached its allocated capacity, the arrays are reallocated with a growth factor (approximately 1.5 times the current capacity plus a small constant), following a standard amortized growth strategy.

6. **Solution storage.** A fresh copy of the solution vector is allocated and the solution values are copied into it. This ensures the pool owns its solution data independently of the caller.

7. **Insertion.** Existing entries at and after the insertion position are shifted to make room. The new solution vector pointer, metric, and objective are inserted at the determined position. The solution count is incremented.

8. **Size limit enforcement.** If the solution count exceeds the PoolSolutions parameter, excess solutions (those with the worst objective values, at the end of the sorted array) are removed and their memory is freed.

9. **Best objective tracking.** The pool's best objective metric is updated. The best metric represents the best (most optimal) objective found so far, adjusted for optimization direction, and is used as the reference point for gap-based pruning.

10. **Gap-based pruning.** When the PoolGap or PoolGapAbs parameter is set to a finite value, the function computes a cutoff threshold based on the best objective value and the gap parameters. Solutions whose objective values exceed the cutoff are removed from the pool. The cutoff computation accounts for the optimization direction and combines relative and absolute gap tolerances. This pruning is triggered only when a new best solution is found (insertion at position zero) or when a solution is added at the end of the pool, avoiding unnecessary pruning overhead on interior insertions.

11. **Integer solution tracking.** If the model has integer variables and the pool has integer tracking arrays, the function checks whether the new solution represents a better objective for its integer variable configuration. If so, the solution is recorded (or updated) in the integer tracking arrays, supporting solution pool diversity across different integer assignments.

Throughout the function, the `solution_added` output flag is set to 1 only if the solution was actually inserted (not rejected as a duplicate). Before returning, the pool's final count field is synchronized with the current solution count.

**Thread Safety:** Unsafe. Must be called from a single thread; solution pool state is not protected by synchronization.

**Dependencies:** cxf_scale_objval (for objective computation); environment memory allocation functions (calloc, malloc, realloc, free); memory copy.

---

## Module-Level Behavioral Notes

### Relationship Between the Six Functions

The six functions form a pipeline that can be organized into three functional groups:

**Result attribute wiring** (cxf_process_lp_solution, cxf_wire_result_attributes):
These two functions serve the same purpose -- connecting result attributes to their storage locations -- but for different solve contexts. cxf_process_lp_solution handles the LP case, wiring a smaller set of attributes (iteration counts, node counts, objective values) using the LP-specific solution information structure. cxf_wire_result_attributes handles the general case (MIP, concurrent, multi-scenario), wiring a broader set of attributes including solution arrays (X, Slack, QCSlack), MIP gap, solution pool bounds, and multiple iteration counters, using the general solver result state. Both functions use the identical attribute wiring pattern: look up the attribute by name, find the entry, set the direct-value pointer.

**Solution transformation** (cxf_uncrush_solution, cxf_scale_objval):
These functions transform solution data from the solver's internal representation back to the user's problem space. cxf_uncrush_solution reverses presolve transformations on the variable values. cxf_scale_objval reverses scaling transformations on the objective function. Together they ensure that solution data returned to the user accurately reflects the original problem formulation, even though the solver may have heavily transformed the problem for numerical stability and efficiency.

**Derived computation** (cxf_compute_gap, cxf_copy_solution):
cxf_compute_gap is a pure mathematical function that derives the optimality gap from the incumbent objective and dual bound. cxf_copy_solution manages the solution pool, computing objective values via cxf_scale_objval and using them to maintain a sorted, pruned collection of solutions.

### Call Graph

```
cxf_solve_lp / cxf_solver_dispatch (callers)
    |
    +-> cxf_uncrush_solution  (reverse presolve on solution vector)
    |
    +-> cxf_process_lp_solution  (wire LP attributes)
    |       |
    |       +-> cxf_scale_objval  (compute objective for infeasibility diagnostic)
    |
    +-> cxf_wire_result_attributes  (wire MIP/general attributes)
            |
            +-> cxf_compute_gap  (compute MIP optimality gap)

cxf_solve_mip (caller)
    |
    +-> cxf_copy_solution  (add solution to pool)
            |
            +-> cxf_scale_objval  (compute objective value for pool)
```

### Attribute Wiring Pattern

Both cxf_process_lp_solution and cxf_wire_result_attributes use a consistent pattern for binding attributes:

1. Look up the attribute by name in the attribute table's hash map
2. Retrieve the attribute entry from the entries array at the found index
3. Set the entry's direct-value pointer to the address of the value to expose

For scalar result attributes (e.g., ObjVal, MIPGap), the direct-value pointer points to a double in the solution state. For array result attributes (e.g., X, Slack), the direct-value pointer points to an array, and an additional size pointer is set to point to the dimension count (enabling bounds checking on array access). Setting the direct-value pointer to null marks the attribute as unavailable, causing queries to return a "data not available" error.

### Solve Mode Classification

cxf_wire_result_attributes classifies solve outcomes into three categories that determine how objective-related attributes are wired:

| Category | Modes | Characteristics |
|----------|-------|-----------------|
| Optimal/Limited | Optimal, Cutoff, Iteration Limit | Solution may exist; objective bound from model |
| Infeasible/Unbounded | Infeasible or Unbounded | No solution; bounds initialized to infinity |
| General MIP | All others | Solution exists; MIP gap computed; full bound set |

### Solution Pool Ordering

Solutions in the pool managed by cxf_copy_solution are ordered by a three-level sorting key designed for deterministic behavior:

1. **Primary key:** Objective value (ascending for minimization)
2. **Secondary key:** Weighted position metric -- `sum(x[j] / (j + 1))` -- providing a deterministic fingerprint
3. **Tertiary key:** Lexicographic comparison of the full solution vector

This three-level ordering ensures that the pool order is identical across runs (given identical inputs), supporting the solver's determinism guarantee. Exact duplicate solutions (matching on all three keys) are rejected.

### Presolve Reversal Context

cxf_uncrush_solution sits in the postsolve pipeline between the internal solver (which operates on the reduced problem) and the result reporting functions (which operate on the original problem). The typical call sequence is:

1. Solver finds solution in presolved space
2. cxf_uncrush_solution maps the solution to original space
3. cxf_scale_objval computes the objective in original space
4. cxf_process_lp_solution or cxf_wire_result_attributes wires results to the attribute table
5. User queries attributes through the public API

If the objective value changes significantly during uncrushing (due to accumulated numerical error in the variable restoration process), the calling code may issue a warning and trigger additional simplex iterations on the original model to clean up any infeasibilities introduced by the uncrushing process.

### References

- Andersen, E.D. and Andersen, K.D. (1995). "Presolving in Linear Programming." *Mathematical Programming*, 71(2):221-245.
- Beale, E.M.L. and Tomlin, J.A. (1970). "Special Facilities in a General Mathematical Programming System for Non-Convex Problems Using Ordered Sets of Variables." In *Proceedings of the Fifth International Conference on Operations Research*, 447-454.
- Gondzio, J. (1997). "Presolve Analysis of Linear Programs Prior to Applying an Interior Point Method." *INFORMS Journal on Computing*, 9(1):73-91.
- Nemhauser, G.L. and Wolsey, L.A. (1988). *Integer and Combinatorial Optimization*. Wiley.
- Nocedal, J. and Wright, S.J. (2006). *Numerical Optimization*, 2nd edition. Springer.
- Padberg, M. (2000). "Approximating Separable Nonlinear Functions Via Mixed Zero-One Programs." *Operations Research Letters*, 27(1):1-5.
- Wolsey, L.A. (1998). *Integer Programming*. Wiley.

---

## Verification Checklist

```
[x] No hex addresses or offsets
[x] No Ghidra artifacts (param_, local_, iVar, DAT_, FUN_)
[x] No binary-specific constants or magic numbers
[x] No copied code fragments
[x] All descriptions are behavioral, not implementational
[x] All data structures described semantically using Layer 1 types
[x] All algorithms cite published sources where applicable
[x] Passes the Clean Room Test
```
