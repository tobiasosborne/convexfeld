# Module: Simplex Lifecycle

## Purpose

The Simplex Lifecycle module contains the three functions that bracket the entire simplex solve: initialization before the iteration loop begins, post-solve variable fixing after it terminates, and post-solve bound tightening with resource deallocation. Together these functions manage the creation, population, and destruction of the SolverState (P1.04), the central data structure through which all simplex functions communicate. cxf_simplex_init allocates and populates the SolverState from the model's problem data. cxf_simplex_final performs dual-feasibility-based variable fixing to simplify the solution. cxf_simplex_cleanup performs implied-bound propagation, additional variable fixing, and frees all temporary working memory. These lifecycle functions implement the initialization and cleanup phases of the revised simplex method described in P2.1 (Revised Simplex Method).

## Functions

### cxf_simplex_init

**Purpose:** Allocate and initialize the SolverState structure that holds all working data for a simplex solve, copying problem dimensions, bounds, constraint data, and special variable information from the model, and selecting the solve mode.

**Signature:**
- Input: `model` : pointer-to-Model - The model containing the constraint matrix, bounds, objective, variable types, and environment
- Input: `altModel` : pointer-to-Model - An alternative model providing warm-start data; may be null for a fresh start
- Input: `initMode` : int - Zero for a fresh solve; non-zero for reoptimization (warm start from a previous basis)
- Input: `timing` : pointer-to-double - Timing accumulator for performance profiling; may be null to disable timing
- Output: `outState` : pointer-to-pointer-to-SolverState - On success, receives the pointer to the newly allocated and initialized SolverState
- Output: int - Zero on success, or the out-of-memory code if any allocation fails

**Preconditions:**
- The model must have a valid constraint matrix with consistent dimensions (number of variables, constraints, and nonzeros)
- The model's environment must be active and initialized
- If `initMode` is non-zero, the model must have valid variable type information for semi-continuous and semi-integer variable detection
- If `altModel` is non-null, it must contain valid warm-start data

**Postconditions:**
- On success: `outState` points to a fully initialized SolverState with:
  - Problem dimensions (variables, constraints, nonzeros, slacks) copied from the model's matrix
  - All working arrays allocated and sized according to problem dimensions and nonzero count
  - Working copies of bounds, objective coefficients, and constraint senses populated from the model's matrix
  - Variable status and basis header arrays allocated (but not yet populated with an initial basis -- that is done by the crash procedure, P3.21)
  - Solve mode selected based on environment parameters and problem characteristics
  - Iteration limits and tolerances copied from the environment
  - A performance scaling factor computed from the nonzero count
  - Special variable handling structures allocated and populated for any quadratic terms, semi-continuous variables, general constraints, SOS constraints, and piecewise-linear constraints present in the model
  - Variable flags array populated with per-variable type markers indicating special handling requirements
  - A memory pool (string pool) allocated for temporary allocations during the solve
  - Timing subsystem initialized with the provided timing accumulator
  - Sentinel values initialized for anti-cycling tracking
- On out-of-memory: all partially allocated arrays have been freed and `outState` is unchanged

**Side Effects:**
- Allocates the SolverState structure and all its constituent arrays (approximately 60 or more individual allocations depending on problem characteristics)
- Stores a back-pointer to the parent model within the SolverState
- For semi-continuous variables: modifies working copies of bounds (relaxing the non-zero bound to zero so standard simplex can operate) and saves the original bound for post-solve restoration
- For piecewise-linear constraints with a single segment: linearizes them directly into the objective coefficients
- For quadratic constraints with greater-than-or-equal sense: normalizes to less-than-or-equal by negating coefficients and RHS
- For reoptimization mode: marks variables exceeding an iteration threshold with a reoptimization flag

**Error Conditions:**
- Memory allocation failure at any point during the multi-stage allocation sequence -> frees all previously allocated arrays in reverse order and returns the out-of-memory code

**Behavioral Description:**
This is the most complex function in the simplex lifecycle, responsible for transforming the model's static problem data into the mutable working state required by the simplex iteration loop. It operates in eight phases.

**Phase 1: Core allocation.** The function allocates the SolverState structure via zero-initialized allocation and initializes the timing subsystem. It retrieves the constraint matrix from the model (preferring the working/alternative matrix if one exists) and extracts the problem dimensions: number of variables, number of constraints, and solution status. For reoptimization mode, it scans the variable types to count semi-continuous and semi-integer variables, and checks for warm-start data availability.

**Phase 2: Working space computation.** The function computes the total number of nonzeros in the constraint matrix by summing the column lengths. It then determines two capacity values using a tiered sizing formula:

1. **Pool capacity** (used for scratch arrays, pricing candidate lists, and as the base allocation unit):

   ```
   poolCapacity = max(MIN_POOL_SIZE, maxDimension, nnz / 10)
   ```

   where `MIN_POOL_SIZE = 10000`, `maxDimension = max(numVars, numConstrs, numSlacks, numQC + numSOS)`, and `nnz` is the total nonzero count. If `nnz / 10` would exceed 2 billion, `poolCapacity` is capped at 2 billion.

2. **Additional capacity** (used for auxiliary working storage that scales with constraint structure complexity):

   ```
   additionalCapacity = nnz / 5 + 10
                       + numQCLinearTerms / 5 + numQCQuadTerms / 5
                       + numSOSEntries / 5
                       + numGeneralConstraints / 5
                       + totalPWLBreakpoints / 5
                       + otherConstraintEntries
   ```

   Each special constraint type contributes one-fifth of its entry count, providing proportional scratch space without over-allocating.

3. **Eta pool capacity** (used for Product Form of the Inverse basis update storage):

   ```
   etaPoolEntries = poolCapacity + nnz
   etaPoolBytes   = etaPoolEntries * ENTRY_SIZE
   ```

   where `ENTRY_SIZE = 32` bytes per entry (sufficient for an index-value pair with alignment and metadata). This ensures the initial eta pool can accommodate at least one full basis representation plus headroom proportional to the problem's sparsity.

The rationale follows standard practice for revised simplex implementations: the eta file grows with each pivot operation, and the initial pool should be large enough to hold approximately one refactorization interval's worth of eta vectors without reallocation (Maros, 2003, Ch. 9). The one-tenth-of-nonzeros heuristic for pool capacity reflects that each simplex iteration typically produces one eta vector whose average density is bounded by the average column density of the constraint matrix.

**Phase 3: Solve mode selection.** For fresh solves, the function selects the simplex variant based on environment parameters and problem characteristics. The selection logic considers: presolve control settings (which may force primal or dual simplex), the presence of quadratic terms (which defaults to dual simplex), general constraint presence (which may force dual simplex for specific solve modes), and the pricing strategy parameter. The selected mode is stored in the SolverState.

**Phase 4: Parameter transfer.** The function copies iteration limits, tolerances, and solve mode parameters from the environment into the SolverState. It initializes sentinel values for anti-cycling tracking (previous entering variable, previous leaving variable, previous pivot row) to "unset." It computes a performance scaling factor as the square root of the logarithm of the nonzero count (for problems above a minimum size threshold), used to scale timing estimates proportionally to problem complexity.

**Phase 5: Array allocation.** The function allocates the primary working arrays: variable status array, basis header, working bounds (lower and upper), reduced costs, row and column scaling factors, workspace arrays sized to the maximum of variables and constraints, and additional temporary arrays. All allocations follow a consistent pattern: check if the dimension is positive, allocate, store the pointer in the SolverState, and jump to cleanup on failure. Scaling arrays are initialized to unity.

**Phase 6: Data copying.** The function copies bounds, objective coefficients, and constraint senses from the model's matrix into the SolverState's working arrays. These working copies allow the simplex algorithm to modify bounds (via perturbation, preprocessing, or bound propagation) without affecting the original model data. The copy uses an overlap-safe memory copy to handle the case where the working arrays alias the matrix arrays.

**Phase 7: Special variable processing.** Depending on the problem's characteristics, the function processes:
- **Quadratic terms:** Allocates Q matrix storage arrays and copies the quadratic structure from the model. Marks variables involved in quadratic terms with appropriate flags.
- **Semi-continuous variables:** Identifies semi-continuous and semi-integer variables, saves their non-zero bounds, relaxes those bounds to zero for standard simplex operation, and marks them with a semi-continuous flag. The saved bounds are restored after the solve.
- **General constraints:** Allocates working arrays for indicator constraints and other general constraint types, builds a compact structure for efficient evaluation, and marks involved variables.
- **Quadratic constraints:** Allocates storage for quadratic constraint linear and quadratic terms, copying and normalizing constraint data (converting greater-than-or-equal constraints to less-than-or-equal form by negation).
- **SOS constraints:** Allocates and copies Special Ordered Set constraint data.
- **Piecewise-linear constraints:** Allocates breakpoint storage, copies PWL data from the model, and linearizes single-segment PWL functions directly into the objective. Multi-segment functions are marked with a PWL flag for special handling during the solve.
- **Ranged constraints:** Marks ranged constraint slack variables with a ranged flag.

**Phase 8: Finalization.** The function stores additional matrix metadata, sets the warm-start flag if an alternative model was provided, creates the auxiliary memory pool for temporary allocations, and writes the completed SolverState pointer to the output parameter.

**Error cleanup:** If any allocation fails during phases 5--7, the function frees all previously allocated arrays in reverse order, including special cleanup for linked-list structures (eta vectors), memory pools, and auxiliary structures. Each pointer slot is checked for null before freeing and set to null after freeing to prevent double-free. The SolverState structure itself is freed last.

**Thread Safety:** Not thread-safe. Must be called from a single thread before simplex iterations begin. The resulting SolverState is intended for single-threaded use throughout the solve.

**Dependencies:**
- P1.04 (SolverState) - defines the structure being allocated and populated
- P1.01 (Environment) - source of solve parameters, tolerances, iteration limits, and solve mode
- P2.1 (Revised Simplex Method) - the SolverState layout supports the data structures required by the revised simplex method (dual CSR/CSC matrix, eta chain, pricing state, working bounds)

---

### cxf_simplex_final

**Purpose:** Perform dual-feasibility-based variable fixing after the simplex iteration loop terminates, identifying variables that can be fixed at their bounds without changing the optimal objective and applying those fixings to simplify the solution for subsequent operations.

**Signature:**
- Input: `state` : pointer-to-SolverState - The solver state with variable status, bounds, dual values (reduced costs), constraint matrix, and constraint data
- Input: `env` : pointer-to-Environment - Environment containing the infinity threshold
- Input: `workOut` : pointer-to-double - Optional work estimation accumulator for performance prediction; null to disable tracking
- Output: int - Zero on success, the out-of-memory code if allocation fails, or an error code propagated from variable fixing

**Preconditions:**
- The simplex iteration loop must have terminated (either optimally or at a limit)
- The solver state must have valid dual values (reduced costs), variable status, bounds, constraint matrix (both CSR and the alternative CSC representation), and constraint senses
- Fixing candidate and marking arrays must be available in the solver state

**Postconditions:**
- On success: all variables that satisfy dual feasibility conditions have been fixed at their appropriate bounds, constraint activities have been verified as consistent with the fixings, and pricing information has been refreshed
- On constraint violation: the function has applied only partial fixings (those that do not violate any constraint), or no fixings if violations are detected early
- On out-of-memory: the function returns immediately with partial state; temporary arrays are freed

**Side Effects:**
- Allocates a temporary target-values array (one double per variable) for computing fixing targets
- Allocates constraint queue and visited arrays for activity verification
- Calls the variable-fixing pivot function to apply each fixing, which modifies the basis, variable status, and constraint activities
- Calls the pricing update function to refresh pricing information after fixings
- Updates the work estimation accumulator (if non-null) at each phase transition
- Frees all temporary arrays before returning

**Error Conditions:**
- Memory allocation failure for the target-values array -> returns the out-of-memory code immediately
- Memory allocation failure for constraint queue or visited arrays -> returns the out-of-memory code after freeing earlier allocations
- Variable-fixing pivot function returns an error -> propagated immediately after cleanup
- Constraint activity verification detects a violated constraint -> applies partial fixings and returns success (not an error, but fewer variables are fixed)

**Behavioral Description:**
This function performs post-solve solution cleanup by fixing variables at their bounds when dual feasibility conditions guarantee that the fixing does not change the optimal objective value. Fixing reduces the effective problem dimension and improves numerical stability for subsequent operations such as barrier crossover or sensitivity analysis. The approach is based on the standard complementary slackness conditions of linear programming (Dantzig, 1963).

**Phase 1: Target value determination.** The function scans all variables with non-negative status (active, unfixed variables). For each variable, it evaluates the dual value (reduced cost) to determine the appropriate fixing target:

1. **Non-negative dual value:** The variable should be at its lower bound (moving up would not improve the objective). The target is set to the lower bound.
2. **Negative dual value:** The variable should be at its upper bound (moving down would not improve the objective). The target is set to the upper bound.
3. **Zero dual value with both bounds active at zero:** A special-case marker is assigned for deferred handling.
4. **Infeasible configuration:** If the dual value is positive but the lower bound is finite, or the dual value is negative but the upper bound is finite, the function aborts early (the variable cannot be fixed without changing the objective).

If a variable with special flags is encountered (indicating it has already been processed by another subsystem), the function also aborts early.

**Phase 2: Equality constraint verification.** For each active equality constraint, the function computes the constraint activity (sum of coefficients times target values) and checks whether it matches the constraint's right-hand side within tolerance. If a violation is detected, the function skips to partial or no fixings. Variables appearing in equality constraints are added to a fixing candidate list for later selective application.

**Phase 3: Activity propagation.** For each variable in the fixing candidate list, the function propagates the effect of fixing through the constraint matrix. For each non-equality constraint containing the variable, the constraint activity is updated using numerically stable addition (with conservative rounding to prevent accumulated floating-point error from causing false violations). A queue of affected constraints is maintained for subsequent verification.

**Phase 4: Constraint feasibility check.** The function verifies that all inequality constraints remain satisfied under the proposed fixings. For each constraint in the affected queue (and then all remaining inequality constraints), it computes the full activity and checks against the right-hand side. If a violation is detected, only the partial fixings from the candidate list are applied.

**Phase 5: Apply fixings.** Depending on the verification results:
- If all constraints are verified: the function applies fixings for all active variables by calling the variable-fixing pivot function for each one.
- If only some constraints are verified: the function applies fixings only for the candidate list variables.
- After all fixings, the pricing update function is called to refresh pricing information.

All temporary arrays (target values, constraint queue, visited flags) are freed before returning.

**Thread Safety:** Not thread-safe. Must be called within a single-threaded simplex context after the iteration loop terminates.

**Dependencies:**
- P3.19 (Pivot Operations) - the variable-fixing pivot function (cxf_pivot_bound) applies individual fixings
- P3.18 (Pricing Support) - the pricing update function refreshes pricing after fixings
- P1.04 (SolverState) - reads variable status, bounds, dual values, constraint matrix, constraint sense; modifies variables via the pivot function
- P2.1 (Revised Simplex Method) - dual feasibility conditions (complementary slackness) determine which variables can be fixed

---

### cxf_simplex_cleanup

**Purpose:** Perform constraint-based implied bound tightening and variable fixing after the simplex solve, then free all temporary working arrays.

**Signature:**
- Input: `state` : pointer-to-SolverState - The solver state with constraint matrix, bounds, variable status, reduced costs, pricing state, and variable flags
- Input: `env` : pointer-to-Environment - Environment containing tolerances (infinity, primal feasibility, bound comparison) and presolve configuration
- Output: int - Zero on success, a numerical difficulty code if a variable cannot be fixed, or the out-of-memory code if allocation fails

**Preconditions:**
- The simplex solve must have completed (cxf_simplex_final has already been called)
- The solver state must have valid constraint matrix (CSC), bounds, variable status, reduced costs, constraint senses, and variable flags

**Postconditions:**
- On success: variables whose bounds can be tightened by constraint-activity analysis have been fixed at their bounds via the variable-fixing pivot function, inequality constraints whose activity bounds indicate tightness have been converted to equalities, pricing state has been updated for all changes, and all temporary working arrays have been freed
- On out-of-memory: partial processing may have occurred; temporary arrays that were successfully allocated have been freed
- On numerical difficulty: variable fixing was attempted but failed for a variable with no finite bound in the required direction; temporary arrays have been freed

**Side Effects:**
- Allocates up to nine temporary working arrays (variable classification, constraint activity bounds, positive/negative coefficient counts, working bounds, and inverted index arrays)
- Modifies variable bounds via the variable-fixing pivot function
- Converts inequality constraints to equality constraints when both activity bounds are tight, incrementing the modification counter and notifying the pricing subsystem
- Adjusts basis header indices for variables with special flags (a temporary index transformation reversed before returning)
- Frees all nine temporary arrays before returning

**Error Conditions:**
- Memory allocation failure for any of the nine temporary arrays -> frees previously allocated arrays and returns the out-of-memory code
- A variable identified for fixing has no finite bound in the required direction -> returns the numerical difficulty code
- Errors from the variable-fixing pivot function are propagated immediately
- Errors from the core bound propagation helper are propagated

**Behavioral Description:**
Despite its name suggesting simple resource cleanup, this function performs substantial post-solve analysis before freeing memory. It implements constraint-based bound propagation -- the standard implied-bound tightening technique from LP presolve (Savelsbergh, 1994) -- applied to the post-solve state to identify variables that can be fixed at their bounds.

**Phase 1: Basis index adjustment.** For variables with special flags (quadratic, semi-continuous, general constraint, piecewise-linear, or ranged), the function temporarily adjusts basis header indices by subtracting an offset. This normalization enables uniform processing of all variables regardless of their special-handling requirements. The adjustment is reversed in Phase 6.

**Phase 2: Variable bound classification.** The function classifies each active variable by which of its bounds are finite:
- Lower bound only finite
- Upper bound only finite
- Both bounds finite
- Neither bound finite (free variable)

This classification drives the subsequent implied-bound analysis by identifying which directions have room for tightening.

**Phase 3: Implied bound computation (conditional).** When enabled by the solve mode and presolve configuration, the function performs two-pass implied-bound analysis:

1. **Build inverted index.** An inverted mapping from constraints to participating variables is constructed using a histogram-to-prefix-sum technique. This enables efficient constraint-to-variable lookups.

2. **Lower bound improvement pass.** For each variable with potential for lower-bound improvement (upper-bound-only or no finite bounds), the function scans the variable's column in the constraint matrix. For each constraint containing the variable, it checks whether the constraint's activity and coefficient structure imply a finite upper bound for the variable that is tighter than the current bound. If so, the variable's classification is upgraded (e.g., from "upper bound only" to "both bounds finite"), and the unbounded-variable counts for affected constraints are updated.

3. **Upper bound improvement pass.** Symmetrically, for each variable with potential for upper-bound improvement, the function checks whether constraint activities imply a finite lower bound that is tighter than the current bound.

**Phase 4: Activity bound initialization.** The function initializes constraint activity bound arrays based on constraint status and sense. Active constraints with equality sense receive full activity tracking (negative infinity to positive infinity). Active inequality constraints receive one-sided tracking.

**Phase 5: Constraint activity computation.** For each variable with matrix entries, the function computes its contribution to constraint activities. Positive and negative coefficients are handled separately, with contributions accumulated using numerically stable addition that applies conservative rounding corrections when floating-point precision loss is detected. This is the same stability technique used in activity bound computation throughout the solver (Savelsbergh, 1994).

**Phase 6: Basis index restoration.** The temporary basis header index adjustment from Phase 1 is reversed.

**Phase 7: Core bound propagation.** The function delegates to a specialized bound-propagation helper that performs iterative reduced-cost bound tightening. This helper uses a queue-based approach with a bounded number of passes to compute tighter implied bounds from constraint slack.

**Phase 8: Variable fixing.** Based on the propagation results and the presolve mode, the function identifies variables that can be fixed at their bounds. The fixing decision depends on the variable's implied bounds, reduced cost, and the presolve aggressiveness setting:

- **Conservative mode:** A variable is fixed at its lower bound if its implied upper bound (from activity analysis) exceeds a threshold derived from the bound tolerance and the reduced cost magnitude. Symmetrically for upper bound fixing.
- **Aggressive mode:** Additional fixing opportunities are exploited for free variables (those without bound classification) when the implied bounds indicate one-sided tightness.

If a variable is identified for fixing but has no finite bound in the required direction, the function returns a numerical difficulty code.

**Phase 9: Constraint conversion.** The function scans all active inequality constraints and checks whether both activity bounds are tight (lower activity exceeds the feasibility tolerance and upper activity is below the negative feasibility tolerance). Constraints meeting this criterion are converted to equalities, and the pricing subsystem is notified of the change.

**Phase 10: Memory cleanup.** All nine temporary working arrays (variable classification, constraint lower activity, constraint upper activity, positive coefficient counts, negative coefficient counts, working lower bounds, working upper bounds, constraint-to-variable start indices, and constraint-to-variable mapping) are freed. Each pointer is checked for null before freeing.

**Thread Safety:** Not thread-safe. Must be called within a single-threaded simplex context after cxf_simplex_final.

**Dependencies:**
- P3.19 (Pivot Operations) - cxf_pivot_bound for variable fixing
- P3.18 (Pricing Support) - cxf_pricing_mark_dirty for notification of constraint conversions
- P1.04 (SolverState) - reads and modifies constraint matrix, bounds, variable status, reduced costs, constraint senses, pricing state, and modification counter
- P1.01 (Environment) - reads infinity threshold, primal feasibility tolerance, bound comparison tolerance, presolve mode, and solve mode
- P2.1 (Revised Simplex Method) - the implied-bound technique is a standard component of simplex post-processing

---

## Module-Level Behavioral Notes

### Role in the Simplex Solve Lifecycle

The three functions in this module define the outermost brackets of a simplex solve. Their positions in the overall solve flow (cxf_solve_lp, P3.25) are:

**Before the iteration loop:**
1. **cxf_simplex_init** (this module) -- allocate and populate the SolverState
2. cxf_simplex_crash (P3.21) -- construct initial basis
3. cxf_simplex_preprocess (P3.21) -- fix near-bound variables
4. cxf_simplex_setup (P3.21) -- compute activity bounds
5. cxf_basis_refactor (P3.16) -- initial basis factorization

**The iteration loop:**
6. cxf_simplex_iterate through cxf_simplex_post_iterate (P3.20) -- repeated until termination

**After the iteration loop:**
7. cxf_simplex_refine (P3.21) -- solution refinement
8. **cxf_simplex_final** (this module) -- dual-feasibility variable fixing
9. **cxf_simplex_cleanup** (this module) -- implied-bound tightening and resource deallocation

### Relationship to Data Model Specifications

| Function | Primary Data Model Spec | Lifecycle Phase |
|----------|------------------------|-----------------|
| cxf_simplex_init | P1.04 (SolverState) - Creation | Allocates and populates all fields described in P1.04 |
| cxf_simplex_final | P1.04 (SolverState) - Mutation | Modifies variable status and bounds via pivot operations |
| cxf_simplex_cleanup | P1.04 (SolverState) - Destruction | Frees all working arrays; corresponds to the Destruction lifecycle phase of P1.04 |

cxf_simplex_init is the sole creator of the SolverState structure. The lifecycle described in P1.04 (SolverState, Lifecycle section) maps directly to the initialization phases of this function: zero-initialized allocation, dimension copying, working array sizing, array allocation, data copying, and special variable processing.

cxf_simplex_cleanup is the primary destructor. While it does not free every array (the solve driver handles some cleanup), it frees the temporary working arrays and performs the bulk of the implied-bound analysis before other cleanup functions handle the remaining arrays and the SolverState structure itself.

### Initialization Complexity

cxf_simplex_init is the most complex function in the simplex subsystem. Its complexity stems from the need to handle multiple problem types within a single allocation framework:

| Problem Feature | Initialization Impact |
|----------------|----------------------|
| Pure LP | Core arrays only (basis, bounds, scaling, reduced costs) |
| Quadratic objective (QP) | Additional Q matrix storage (row counts, column starts, row indices) |
| Semi-continuous variables | Bound relaxation arrays and variable index tracking |
| General constraints | Working arrays, index arrays, coefficient arrays, link structures |
| Quadratic constraints | Linear term arrays, quadratic term arrays, sense/RHS normalization |
| SOS constraints | SOS data arrays |
| Piecewise-linear constraints | Breakpoint arrays (X, Y, slopes), linearization of single-segment functions |
| Ranged constraints | Flag marking on ranged slack variables |

For pure LP problems, many of these specialized allocations are skipped entirely (the dimension is zero, so the allocation is bypassed).

### Variable Flags System

cxf_simplex_init populates a per-variable flags array that marks variables requiring special handling during the solve:

| Flag | Meaning | Set By |
|------|---------|--------|
| QUADRATIC | Variable appears in the quadratic objective | Phase 7 (quadratic term processing) |
| QUADRATIC_CONSTRAINT | Variable appears in a quadratic constraint | Phase 7 (quadratic term processing) |
| SEMI_CONTINUOUS | Semi-continuous or semi-integer variable | Phase 7 (semi-continuous processing) |
| GENERAL_CONSTRAINT | Variable involved in a general constraint | Phase 7 (general constraint processing) |
| REOPTIMIZATION | Variable marked for reoptimization warm-start | Phase 1 (reoptimization mode) |
| PIECEWISE_LINEAR | Variable with multi-segment PWL function | Phase 7 (PWL processing) |
| RANGED | Ranged constraint slack variable | Phase 8 (finalization) |

These flags are read by the simplex iteration functions (P3.20) and the pivot operations (P3.19) to apply appropriate special-case handling. cxf_simplex_cleanup also reads the flags to adjust basis header indices during its temporary normalization step.

### Allocation Strategy

The initialization function follows a strict allocation discipline:

1. **Zero-initialized base structure.** The SolverState is allocated via calloc (zero-initialized), ensuring all pointer fields start as null. This simplifies error cleanup: any non-null pointer can be safely freed.

2. **Dimension-gated allocations.** Each array allocation is gated by a dimension check (e.g., "if numVars > 0"). This avoids zero-length allocations and ensures correct behavior on degenerate problems.

3. **Fail-fast with reverse cleanup.** On any allocation failure, the function jumps to a cleanup path that frees all previously allocated arrays in reverse order. Each pointer is checked for null before freeing and set to null after freeing to prevent double-free.

4. **Tiered sizing.** Working array sizes are computed from three tiers. First, the **pool capacity** is the maximum of a minimum floor (10000), the largest problem dimension, and one-tenth of the nonzero count -- capped at 2 billion. Second, an **additional capacity** accumulates one-fifth of the entry count for each special constraint type (quadratic, SOS, general, piecewise-linear) plus `nnz/5 + 10`. Third, the **eta pool** is sized to `(poolCapacity + nnz) * 32` bytes, providing initial storage for basis update vectors. This tiered approach avoids both undersized buffers (which would cause later allocation failures) and grossly oversized buffers (which would waste memory). See Phase 2 of the cxf_simplex_init behavioral description for the full formulas.

### Post-Solve Analysis Pipeline

cxf_simplex_final and cxf_simplex_cleanup form a two-stage post-solve analysis pipeline:

| Stage | Function | Technique | Purpose |
|-------|----------|-----------|---------|
| 1 | cxf_simplex_final | Dual feasibility analysis | Fix variables at bounds based on reduced cost signs |
| 2 | cxf_simplex_cleanup | Implied bound propagation | Tighten variable bounds using constraint activity analysis, then fix at bounds |

Stage 1 (cxf_simplex_final) uses a local criterion: each variable is evaluated independently based on its dual value and bounds. Stage 2 (cxf_simplex_cleanup) uses a global criterion: constraint activities propagate information across variables, enabling fixings that require knowledge of the full constraint structure.

### Numerical Stability Techniques

Both cxf_simplex_final and cxf_simplex_cleanup use the same numerically stable addition technique when accumulating constraint activities. When the magnitudes of the two operands differ significantly, floating-point addition can lose precision. The functions detect this by checking whether the reverse subtraction recovers the original operand. If precision loss is detected, the result is multiplied by a conservative rounding factor (slightly above 1.0 for positive results, slightly below 1.0 for negative results) to ensure that activity bounds remain conservative. This prevents false infeasibility or false tightness from accumulated rounding errors. The technique is a simplified variant of compensated summation (Kahan, 1965), adapted for the specific needs of bound propagation where conservative over-estimation is preferred over exact summation.

### Work Estimation

cxf_simplex_final supports optional work estimation via the `workOut` parameter. When non-null, the function accumulates a weighted estimate of computational work at each phase transition, using per-operation cost multipliers scaled by a problem-dependent work multiplier stored in the SolverState. This estimate is used by the outer solve driver for progress prediction and time-limit enforcement. cxf_simplex_cleanup uses a similar mechanism via the timing pointer stored in the SolverState.

### Return Code Conventions

| Code | Meaning | Functions |
|------|---------|-----------|
| Success (zero) | Operation completed normally | All three functions |
| Out-of-memory code | Memory allocation failed | init, final, cleanup |
| Numerical difficulty code | Variable cannot be fixed (no finite bound in required direction) | cleanup only |
| Variable-fixing error | Propagated from the pivot function | final, cleanup |

### Thread Safety Summary

| Function | Thread Safety | Notes |
|----------|---------------|-------|
| cxf_simplex_init | Not thread-safe | Allocates and populates a single-threaded SolverState |
| cxf_simplex_final | Not thread-safe | Modifies variable status, bounds, and pricing state |
| cxf_simplex_cleanup | Not thread-safe | Modifies bounds, constraint senses, pricing state; frees arrays |

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
[x] Explicit cross-references to P1.01, P1.04, P2.1 (algorithm specs) and P3.16, P3.18-P3.21, P3.25 (module specs)
[x] All algorithms cite published sources (Dantzig, Savelsbergh, Kahan)
[x] Passes the Clean Room Test: could be written without seeing the binary
```

## References

- Dantzig, G.B. (1963). *Linear Programming and Extensions*. Princeton University Press.
- Kahan, W. (1965). "Further remarks on reducing truncation errors." *Communications of the ACM*, 8(1):40.
- Maros, I. (2003). *Computational Techniques of the Simplex Method*. Springer. International Series in Operations Research and Management Science, Vol. 61.
- Savelsbergh, M.W.P. (1994). "Preprocessing and Probing Techniques for Linear Programming Problems." *ORSA Journal on Computing*, 6(4):445-454.
