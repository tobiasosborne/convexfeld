# Module: Crossover

## Purpose

The Crossover module implements the barrier-to-simplex crossover procedure, which transforms an interior-point (barrier) solution into a basic feasible solution (vertex solution) suitable for the simplex method. Interior-point methods produce solutions that lie strictly in the interior of the feasible polytope, while the simplex method requires vertex solutions where non-basic variables sit exactly at their bounds. This module bridges that gap through two complementary functions: cxf_crossover handles quadratic objective variable processing and binary variable linearization as a preliminary phase, while cxf_crossover_bounds performs the main crossover operations including variable classification, bound snapping, constraint activation, and basis construction. Together they implement the three-phase crossover structure common to modern LP solvers (Megiddo, 1991; Bixby and Saltzman, 1994; Andersen and Ye, 1996), as described in P2.7 (Barrier-to-Simplex Crossover).

## Functions

### cxf_crossover

**Purpose:** Process variables with separable quadratic objective terms during crossover by computing their optimal placement within bounds and, optionally, linearize binary variable quadratic terms using the identity x^2 = x for x in {0, 1}.

**Signature:**
- Input: `model` : pointer-to-Model - The optimization model containing environment parameters, infinity threshold, and crossover configuration flags
- Input: `state` : pointer-to-SolverState - The solver state with variable arrays, bounds, objective coefficients, quadratic objective data, pricing state, and timing infrastructure
- Output: int - Zero on success, the unbounded error code if a computed target value exceeds the solver's infinity threshold, or an error code propagated from variable change operations

**Preconditions:**
- The solver state must have valid working bound arrays, objective coefficient array, and variable status arrays
- If the problem has a quadratic objective, the diagonal Q array and off-diagonal count array must be populated in the CrossoverState fields of the SolverState (see P1.10, CrossoverState)
- The pricing state must be initialized
- The crossover mode (primal or dual) must be set in the SolverState's isDualSimplex field

**Postconditions:**
- On success: all eligible variables with separable diagonal quadratic terms have been pushed to their computed optimal values, pricing caches have been invalidated for all processed variables, the timing accumulator has been updated, and (if binary linearization was performed) binary variables with diagonal Q terms have had their quadratic contributions absorbed into their linear objective coefficients
- On unbounded: a variable's computed optimal target exceeded half the solver's infinity threshold, indicating potential unboundedness or numerical overflow
- On propagated error: a variable change operation (primal or dual) failed during the push phase

**Side Effects:**
- Modifies variable values via primal or dual variable change operations (dispatched based on the isDualSimplex crossover mode flag)
- Invalidates pricing cache entries for every processed variable
- Clears pricing validity flags on processed variables
- For binary linearization: modifies linear objective coefficients (adds half the diagonal Q value), zeroes diagonal Q values, and increments the binary conversion counter on the CrossoverState
- Updates the timing accumulator with weighted operation counts for both phases

**Error Conditions:**
- Computed target value for a quadratic variable exceeds half the infinity threshold -> returns the unbounded error code immediately
- Primal or dual variable change operation returns an error -> propagated immediately without further processing
- No error conditions for memory allocation (this function does not allocate memory)

**Behavioral Description:**
This function implements Phase 0 (Preprocessing and Special Cases) of the crossover algorithm described in P2.7 (Barrier-to-Simplex Crossover). It handles variables with quadratic objective terms before the main bound-snapping phase, because the optimal placement of separable quadratic variables can be computed analytically.

**Phase 1: Quadratic variable processing.** The function iterates over all variables and processes those with nonzero diagonal quadratic objective terms (Q_jj) that have no off-diagonal coupling (the off-diagonal count is zero). Variables with off-diagonal Q terms require simultaneous optimization and are deferred to the simplex cleanup phase. For each eligible variable:

1. **Skip conditions.** The variable is skipped if: (a) it has off-diagonal quadratic coupling, (b) it is already classified (non-zero basis status), (c) its bound range is below the feasibility tolerance (effectively fixed), or (d) its variable flags indicate special handling requirements beyond pricing.

2. **No diagonal Q term (pricing cleanup).** If the variable has a zero diagonal Q but has a stale pricing cache entry (indicated by a pricing validity flag), the flag is cleared and the pricing cache is invalidated. No value change is performed.

3. **Univariate quadratic optimization.** For variables with nonzero diagonal Q:
   - The unconstrained minimizer is computed as x* = -c_j / Q_jj, where c_j is the linear objective coefficient (see P2.7, Phase 0, Step 1).
   - The objective function f(x) = c_j * x + 0.5 * Q_jj * x^2 is evaluated at the lower bound, upper bound, and (if x* lies in the interior of [lb, ub]) at the unconstrained minimizer.
   - For integer variables, the continuous optimum is rounded: both the floor and ceiling of x* are evaluated, and the integer point with the best objective value is selected.
   - The target is set to whichever evaluation point achieves the minimum objective value.

4. **Unboundedness check.** If the absolute value of the target exceeds half the solver's infinity threshold, the function returns the unbounded error code. This catches cases where a concave quadratic term (Q_jj < 0) causes divergence.

5. **Variable push.** The variable is pushed to the computed target value using either the primal or dual variable change operation, selected by the isDualSimplex flag on the CrossoverState. This dispatching follows the recommendation in P2.7 (Key Design Choice: Push ordering is configurable).

**Phase 2: Binary variable linearization.** If the problem contains binary variables and the binary crossover configuration parameter is enabled, the function performs a second pass over all variables. For each binary variable (type 'B') with a nonzero diagonal Q term and no off-diagonal coupling:

1. **Penalty conversion.** The identity x^2 = x for x in {0, 1} is exploited (Sherali and Adams, 1999) to convert the quadratic contribution to a linear penalty: the linear objective coefficient is incremented by half the diagonal Q value (c'_j = c_j + 0.5 * Q_jj), and the diagonal Q value is set to zero.

2. **Pricing invalidation.** The variable's pricing validity flag is cleared and its pricing cache entry is invalidated, since the effective objective coefficient has changed.

3. **Counter update.** The binary conversion counter on the CrossoverState is incremented for diagnostic tracking.

**Thread Safety:** Not thread-safe. Must be called within a single-threaded crossover context.

**Dependencies:**
- P2.7 (Barrier-to-Simplex Crossover) - implements Phase 0 (quadratic preprocessing and binary linearization)
- P1.04 (SolverState) - reads variable status, bounds, objective coefficients; dispatches variable change operations
- P1.10 (CrossoverState) - reads/writes diagonal Q array, off-diagonal counts, isDualSimplex flag, binary conversion counter, timing accumulator
- P1.01 (Environment) - reads the infinity threshold and binary crossover configuration parameter from the model's environment
- P3.17 (Pricing Core) - pricing cache invalidation for processed variables

---

### cxf_crossover_bounds

**Purpose:** Perform the main barrier-to-simplex crossover by classifying variables based on proximity to bounds, snapping near-bound variables to exact bound values, processing special constraint structures (singletons, SOS patterns), activating unrepresented constraints into the basis, and finalizing the partial basis for simplex cleanup.

**Signature:**
- Input: `model` : pointer-to-Model - The optimization model containing environment parameters, crossover configuration flags, quadratic objective flag, and tolerance values
- Input: `state` : pointer-to-SolverState - The solver state with constraint matrix (CSR and CSC), variable arrays, bounds, basis tracking, pricing state, and timing infrastructure
- Input: `referenceLowerBounds` : pointer-to-double (nullable) - An optional reference copy of the variable lower bounds array, used during the initial crossover pass when the solver maintains working copies of bounds that may differ from the solver state's current bounds (see Behavioral Description, Reference Bounds). When null, the function uses the solver state's own lower bounds directly. Provided by the caller from the CrossoverState's working lower bounds field when bound copies were made prior to crossover.
- Input: `referenceUpperBounds` : pointer-to-double (nullable) - An optional reference copy of the variable upper bounds array, paired with `referenceLowerBounds`. When null, the function uses the solver state's own upper bounds directly. Provided by the caller from the CrossoverState's working upper bounds field when bound copies were made prior to crossover.
- Input: `enableAdvancedProcessing` : int - A flag controlling whether a late-stage advanced constraint processing phase is executed. When zero, the advanced phase is skipped regardless of model configuration. When nonzero, and the model's advanced crossover configuration flag is also set, an additional constraint processing pass is performed after the main crossover phases. In the standard crossover calling sequence, this flag is passed as zero; it exists to support alternative crossover invocations that require the extended processing.
- Output: int - Zero on success, the crossover error code if a variable cannot satisfy its bound constraints, or the out-of-memory error code if work array allocation fails

**Preconditions:**
- The barrier method must have converged to an approximate optimal solution (see P2.7, Precondition 1)
- The solver state must have valid constraint matrix data in both CSR and CSC formats
- Variable bounds, status arrays, and objective coefficients must be populated
- The pricing state must be initialized
- The crossover mode (primal or dual) must be set in the SolverState's isDualSimplex field
- If the problem has a quadratic objective, cxf_crossover should have been called first to process separable quadratic variables
- If reference bound arrays are provided (non-null), they must have the same dimension as the solver state's variable count and must contain valid finite bound values

**Postconditions:**
- On success: all eligible variables have been classified and pushed to bounds, unrepresented constraints have been activated into the basis via forward transformation, the basis representation has been updated, processed variables have been marked with processing flags, timing accumulators have been updated, and the partial basis is ready for simplex cleanup
- On crossover error: a specific variable could not be snapped to any bound without violating feasibility constraints; the problematic variable index has been stored in the solver state's error diagnostic field
- On out-of-memory: a work array allocation failed; partial processing may have occurred

**Side Effects:**
- Allocates and frees temporary classification work arrays
- Modifies variable status, bounds, and constraint RHS values during bound snapping
- Creates eta records for variable fixings and constraint activations via pivot operations
- Updates the basis representation through forward transformation and basis update operations
- Invalidates pricing cache entries for snapped and activated variables
- Marks processed variables with row-processed and column-processed flags
- Updates work counters and timing accumulators throughout processing
- May delegate to specialized sub-procedures for binary handling, SOS constraint processing, dual crossover, and advanced crossover operations

**Error Conditions:**
- Variable bound violation during singleton constraint processing -> returns the crossover error code with the problematic variable index stored
- Variable bound violation during general bound snapping -> returns the crossover error code with diagnostic information
- Memory allocation failure for classification array -> returns the out-of-memory error code
- Error propagated from sub-procedures (initial setup, binary handling, SOS processing, pivot operations, basis operations, forward transformation) -> propagated immediately

**Behavioral Description:**
This function implements the core crossover algorithm described in P2.7 (Barrier-to-Simplex Crossover), Phases 1 through 4. It is a large multi-phase function (the largest in the crossover module) that orchestrates variable classification, bound snapping, constraint activation, and basis construction. The function follows different code paths depending on whether the problem has a quadratic objective and the crossover configuration flags.

**Reference Bounds.** The `referenceLowerBounds` and `referenceUpperBounds` parameters provide an optional mechanism for the caller to supply separate copies of the variable bounds that were snapshotted before crossover modifications began. During the standard crossover calling sequence, the caller copies the solver state's current bounds into the CrossoverState's working bound fields before invoking this function. When these reference bound pointers are non-null, the function uses them during extended constraint processing phases (including SOS pattern detection) to access the original bound values, since the solver state's own bounds may be modified by earlier phases of this function. When null (as in iterative re-invocations of crossover during the simplex iteration loop), the function operates entirely on the solver state's current bounds. The null/non-null status of the reference lower bounds pointer also serves as a condition for entering certain specialized code paths: SOS constraint handling and some bound-checking phases are only entered when the reference bounds are null (indicating a re-invocation context where the initial crossover pass has already been performed).

**Phase 1: Quadratic path dispatch.** If the problem has a quadratic objective, the function enters a specialized path that allocates a classification work array and initializes it based on variable eligibility. Variables are excluded from crossover processing if they are already classified (non-zero basis status), effectively fixed (bound range below a tight tolerance), or have special handling flags set. All other variables are marked as unclassified. After classification, the function delegates to specialized crossover sub-procedures and frees the work array before continuing.

**Phase 2: Singleton equality constraint processing.** For problems without a quadratic objective (or after the quadratic path), and when the crossover is enabled and not in re-initialization mode, the function processes singleton equality constraints -- constraints with a single nonzero coefficient. For each such constraint:

1. **Target computation.** The variable value is determined directly from the constraint: x_j = b_i / a_ij, where b_i is the constraint RHS and a_ij is the sole nonzero coefficient (see P2.7, Phase 0, Step 4: Singleton constraint processing).

2. **Bound checking.** The computed target is checked against the variable's bounds. If the target exceeds the upper bound or falls below the lower bound, a tolerance-adjusted target is computed. If the adjusted target still violates bounds beyond the tolerance, the crossover error code is returned with the problematic variable index stored for diagnostics.

3. **Pivot execution.** A pivot operation fixes the variable at the validated target value, maintaining constraint feasibility through the standard pivot mechanism (P3.19).

**Phase 3: Initial crossover and binary handling.** The function calls an initial crossover setup sub-procedure to prepare the constraint matrix structures and work arrays. If the problem has binary variables and the appropriate configuration flags are set, a binary crossover handler is invoked to process binary variables with special structure. If the problem contains constraints matching SOS (Special Ordered Set) patterns -- typically constraints with exactly three nonzeros involving binary variables with unit coefficients -- these are detected and processed by introducing auxiliary variables and constraints to facilitate basis construction. The SOS pattern detection examines constraint structure to identify cases of the form x_binary - x_continuous + x_binary <= 0 and reformulates them using equality constraints with auxiliary variables that are incorporated into the basis via forward transformation and basis update operations.

**Phase 4: Bound checking and variable snapping.** After the initial setup phases, the function processes remaining constraints through bound-checking and pivot operations. For each pending constraint:

1. **Constraint classification.** Constraints are classified by their sense (equality, less-than, greater-than) and structure (single-column, multi-column).

2. **Target computation and bound validation.** For each variable involved in a constraint, the function computes a target value based on the constraint's RHS and coefficients, validates the target against variable bounds with tolerance adjustments, and determines whether a pivot or a direct bound snap is needed.

3. **Pivot dispatch.** Depending on the crossover mode (primal or dual), the function selects between primal and dual pivot operations. Primal crossover uses primal simplex pivots; dual crossover uses dual simplex pivots. This dispatching follows the configurable push ordering described in P2.7 (Key Design Choice: Push ordering is configurable).

4. **Integer handling.** For integer and binary variables, continuous target values are rounded to the nearest feasible integer before bound validation.

5. **Pricing invalidation.** After each variable snap, the pricing cache is invalidated for the affected variable to ensure subsequent simplex iterations use correct reduced costs.

**Phase 5: Constraint activation.** After the variable-pushing phases, some constraints may not yet be represented in the basis. The function counts activatable constraints -- those with column-space activity (nonzero coefficients among unpushed variables) but no row-space activity (no basic variable assigned). This counting uses a cache-efficient scanning pattern for performance on large problems. For each activatable constraint:

1. **Basis extension.** The constraint is added to the basis representation.
2. **RHS and sense setup.** The constraint's RHS value and sense are copied into the working dual arrays.
3. **Forward transformation.** For each nonzero coefficient in the constraint, a forward transformation (FTRAN) operation expresses the column in terms of the current basis (see P2.7, Phase 3, Step 2).
4. **Basis update.** The basis factorization is updated to incorporate the new constraint.
5. **Pricing invalidation.** The pricing cache is invalidated for the newly added row.

**Phase 6: Continuous variable filtering and flag marking.** For inequality constraints containing continuous variables, the function identifies constraints where continuous variable participation may require special treatment. Processed variables are then marked with row-processed and column-processed flags to prevent redundant processing in subsequent passes.

**Phase 7: Finalization.** The function calls a final crossover sub-procedure that validates the partial basis and performs any remaining adjustments. Fully-processed constraints (those with no remaining column or row activity) are compacted out of the pending constraint list by updating the pending count. If a special post-processing flag is set on the model, an additional specialized crossover sub-procedure is invoked. If the `enableAdvancedProcessing` flag is nonzero and the model's advanced crossover configuration flag is also set, an advanced crossover handler is called to perform an additional constraint processing pass with extended scanning and variable classification. This advanced phase is skipped in the standard crossover calling sequence (where `enableAdvancedProcessing` is zero) and exists to support alternative invocation patterns that require deeper constraint analysis.

**Thread Safety:** Not thread-safe. Must be called within a single-threaded crossover context.

**Dependencies:**
- P2.7 (Barrier-to-Simplex Crossover) - implements Phases 1 through 4 of the crossover algorithm
- P1.04 (SolverState) - reads and modifies constraint matrix (CSR and CSC), variable status, bounds, RHS, and basis tracking arrays
- P1.05 (BasisState) - basis header updates and forward transformation support
- P1.10 (CrossoverState) - reads isDualSimplex flag, error variable index, timing fields
- P1.01 (Environment) - reads crossover configuration parameters, tolerances, and feature flags from the model's environment
- P3.19 (Pivot Operations) - cxf_pivot_bound for variable fixing with full basis and activity maintenance; cxf_pivot_primal for feasibility-checked primal fixing
- P3.16 (Basis Operations) - basis update and forward transformation operations during constraint activation
- P3.17 (Pricing Core) - pricing cache invalidation for snapped and activated variables

---

## Module-Level Behavioral Notes

### Role in the Solver Pipeline

The Crossover module occupies a specific position in the overall solve flow. After the barrier (interior-point) method converges to an approximate optimal solution, the crossover procedure transforms that solution into a basic feasible solution before handing control to the simplex method for final cleanup. The calling sequence is:

1. **Barrier solve** (P3.26) - produces an interior-point solution
2. **cxf_crossover** (this module) - processes quadratic variables and binary linearization
3. **cxf_crossover_bounds** (this module) - main crossover: classification, snapping, activation
4. **Simplex cleanup** (P3.20/P3.22) - resolves remaining infeasibilities from a warm-started basis

The crossover module is invoked only when the barrier method is the primary solver and crossover has not been disabled by the user's crossover parameter setting. When crossover is disabled, the barrier solution is returned directly without basis construction.

### Primal vs. Dual Crossover Modes

The CrossoverState's isDualSimplex field controls whether the crossover uses primal or dual simplex operations for variable pushing. This follows the configurable push ordering described in P2.7 and summarized in the crossover parameter documentation:

- **Primal crossover (isDualSimplex = 0):** Variables are pushed to bounds using primal variable change operations that maintain primal feasibility. The subsequent simplex cleanup uses primal simplex.
- **Dual crossover (isDualSimplex = 1):** Variables are pushed using dual variable change operations that maintain dual feasibility. The subsequent simplex cleanup uses dual simplex.

The choice between primal and dual modes is problem-dependent. Andersen and Ye (1996) observe that dual-first push ordering tends to be more efficient for many problem classes, but the automatic mode selects based on problem characteristics.

### Quadratic Objective Support

The crossover module provides first-class support for problems with quadratic objectives through two mechanisms:

1. **Separable quadratic processing (cxf_crossover).** Variables with purely diagonal Q terms (no off-diagonal coupling) are handled analytically by solving the univariate sub-problem min(c*x + 0.5*Q*x^2) within bounds. This is an O(1) computation per variable that avoids the need for iterative optimization.

2. **Binary linearization (cxf_crossover).** For binary variables, the identity x^2 = x converts diagonal quadratic terms to linear penalties, simplifying the problem for subsequent simplex processing. This is gated by a configuration parameter.

Variables with off-diagonal quadratic coupling (Q_ij with i != j) cannot be optimized independently and are deferred to the simplex cleanup phase, where they are handled through standard pivoting.

### SOS Constraint Handling

cxf_crossover_bounds includes detection logic for Special Ordered Set (SOS) constraint patterns. When a constraint matches the pattern x_binary - x_continuous + x_binary <= 0 with three nonzero entries, the function introduces auxiliary variables and equality constraints to reformulate the constraint in a way that facilitates basis construction. This reformulation adds three new equality constraints and auxiliary variables that are incorporated into the basis through forward transformation, enabling the crossover to proceed without stalling on the SOS structure.

### Performance Characteristics

The crossover module is designed for efficiency on large-scale problems:

- **Classification and pushing phases** are O(n) in the number of variables, with constant work per variable for the quadratic processing in cxf_crossover.
- **Constraint activation** is O(nnz) in the number of nonzeros, with forward transformation cost per activated constraint.
- **Counting loops** in cxf_crossover_bounds use cache-efficient scanning with block-level unrolling for performance on modern processor architectures.
- **Timing accumulators** track weighted operation counts throughout all phases, enabling progress reporting during long crossover operations.

In practice, the crossover typically completes in O(n + m + nnz) time plus a small number of simplex cleanup iterations, as described in P2.7 (Complexity section).

### Relationship to Algorithm Specifications

| Function | Primary Algorithm Spec | Role |
|----------|----------------------|------|
| cxf_crossover | P2.7, Phase 0 (Preprocessing and Special Cases) | Quadratic variable placement and binary linearization |
| cxf_crossover_bounds | P2.7, Phases 1-4 (Classification, Push, Activation, Cleanup) | Main crossover operations |

### Crossover Sub-Procedures

cxf_crossover_bounds delegates to several internal sub-procedures for specialized tasks. These sub-procedures are not part of the public module interface but are called internally:

| Sub-procedure | Role |
|---------------|------|
| Crossover initial setup | Prepare constraint matrix structures and work arrays |
| Crossover binary handler | Process binary variables with special structure |
| Crossover SOS handler | Detect and reformulate SOS constraint patterns |
| Crossover dual handler | Execute dual-mode crossover operations |
| Crossover final | Validate and adjust the partial basis |
| Crossover special | Handle model-specific post-processing |
| Crossover advanced | Perform problem-specific optimizations |
| Crossover cleanup | Free work arrays and reset crossover state |

### Numerical Considerations

- **Snap tolerance:** The threshold for deciding whether a variable is "at a bound" versus "interior" is critical for crossover quality. Setting it too large causes premature snapping and potential infeasibility; setting it too small leaves many superbasic variables for the simplex cleanup to resolve (see P2.7, Numerical Considerations).
- **Singleton constraint tolerance:** When processing singleton equality constraints, the tolerance is used for both the direct target computation and the fallback tolerance-adjusted computation. Coefficients with opposite signs require opposite tolerance adjustments.
- **Feasibility maintenance:** After each variable snap, constraint RHS values must be updated to maintain feasibility. Accumulated floating-point error from many snaps can degrade feasibility, which is resolved during the simplex cleanup phase.
- **Integer rounding:** For integer and binary variables, continuous target values are rounded to the nearest feasible integer. Both floor and ceiling values may be evaluated to find the objective-minimizing integer point.

### Return Code Conventions

| Code | Meaning | Functions |
|------|---------|-----------|
| Success (zero) | Operation completed successfully | Both functions |
| Unbounded code | Variable target exceeds infinity threshold | cxf_crossover only |
| Crossover error code | Variable cannot satisfy bound constraints | cxf_crossover_bounds only |
| Out-of-memory code | Work array allocation failed | cxf_crossover_bounds only |
| Propagated error | Error from sub-procedure or pivot operation | Both functions |

### Thread Safety Summary

| Function | Thread Safety | Notes |
|----------|---------------|-------|
| cxf_crossover | Not thread-safe | Modifies variable values, objective coefficients, Q-matrix, pricing state |
| cxf_crossover_bounds | Not thread-safe | Modifies constraint matrix, basis, bounds, status, pricing state, allocates work arrays |

Both functions operate within a single-threaded crossover context. Thread safety for concurrent solves is achieved at the model level by creating independent solver instances, each with its own SolverState and CrossoverState fields.

---

## Verification Checklist

```
[x] No hex addresses or offsets
[x] No Ghidra artifacts (param_, local_, iVar, DAT_, FUN_)
[x] No binary-specific constants or magic numbers
[x] No copied code fragments from analyzed source
[x] All descriptions are behavioral, not implementational
[x] All data structures described semantically using Layer 1/2 references
[x] Explicit cross-references to P1.01, P1.04, P1.05, P1.10, P2.7, P3.16, P3.17, P3.19, P3.20, P3.22
[x] All algorithms cite published sources (Megiddo, Bixby & Saltzman, Andersen & Ye, Sherali & Adams)
[x] Previously-UNDETERMINED parameters (data1, data2, mode) resolved to referenceLowerBounds, referenceUpperBounds, enableAdvancedProcessing
[x] Passes the Clean Room Test: could be written without seeing the binary
```

## References

- Andersen, E.D. and Ye, Y. (1996). "Combining interior-point and pivoting algorithms for linear programming." *Management Science*, 42(12):1719-1731.
- Bixby, R.E. and Saltzman, M.J. (1994). "Recovering an optimal LP basis from an interior point solution." *Operations Research Letters*, 15(4):169-178.
- Dantzig, G.B. (1963). *Linear Programming and Extensions*. Princeton University Press.
- Maros, I. (2003). *Computational Techniques of the Simplex Method*. Springer. International Series in Operations Research and Management Science, Vol. 61.
- Megiddo, N. (1991). "On finding primal- and dual-optimal bases." *ORSA Journal on Computing*, 3(1):63-65.
- Mehrotra, S. and Ye, Y. (1993). "Finding an interior point in the optimal face of linear programs." *Mathematical Programming*, 62(1-3):497-515.
- Sherali, H.D. and Adams, W.P. (1999). *A Reformulation-Linearization Technique for Solving Discrete and Continuous Nonconvex Problems*. Springer.
- Vanderbei, R.J. (2014). *Linear Programming: Foundations and Extensions*. 4th edition. Springer.
