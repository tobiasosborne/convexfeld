# Supporting Structures

This specification covers four auxiliary data structures that serve specialized roles within the LP solver. Each structure is small enough that a dedicated document would be disproportionate, but each is independently implementable and participates in well-defined interactions with the core Model and SolverState structures.

---

## 1. IISState (Irreducible Infeasible Set)

### Purpose

IISState holds the results of an Irreducible Infeasible Subsystem (IIS) computation for an infeasible model. An IIS is a minimal subset of constraints and variable bounds such that (a) the subset is collectively infeasible, and (b) removing any single element from the subset renders the remainder feasible. This minimality property makes the IIS a powerful diagnostic tool: it pinpoints the smallest conflict responsible for infeasibility. The structure stores per-constraint and per-variable-bound membership flags indicating which elements belong to the IIS, along with optional constraint name storage for user-facing reporting.

### Fields

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| numConstrs | int | Number of constraints represented in the constraint IIS status array | >= 0 | Matches the model's constraint count at the time of IIS computation |
| constrIIS | array-of-int [numConstrs] | Per-constraint IIS membership status: indicates whether each constraint participates in the irreducible infeasible set | 0 (not in IIS) or 1 (in IIS) | Array length equals numConstrs |
| varLowerBoundIIS | array-of-int [numVars] | Per-variable lower bound IIS membership status: indicates whether each variable's lower bound participates in the IIS | 0 (not in IIS) or 1 (in IIS) | Array length equals the model's variable count at IIS computation time |
| varUpperBoundIIS | array-of-int [numVars] | Per-variable upper bound IIS membership status: indicates whether each variable's upper bound participates in the IIS | 0 (not in IIS) or 1 (in IIS) | Array length equals the model's variable count at IIS computation time |
| constrNames | array-of-string [numConstrs] | Optional per-constraint name strings, stored for diagnostic output and IIS export to file formats | Null entries for unnamed constraints, or valid string pointers | If non-null, array length equals numConstrs; each entry is independently allocated |

### Relationships

- **Owned by** the Model. The Model holds a single optional reference to an IISState. This reference is null when no IIS has been computed.
- **Borrows** problem dimensions from the Model's matrix data. The IIS arrays are sized according to the constraint and variable counts at the time of computation, not at the time of query.
- **Independent of** SolverState. IIS computation occurs as a separate post-solve analysis, not during optimization.

### Lifecycle

#### Creation
1. The user requests IIS computation on a model that has been determined to be infeasible.
2. The IIS computation algorithm allocates an IISState structure.
3. Arrays are allocated for constraint membership (sized by constraint count), variable lower bound membership, and variable upper bound membership (both sized by variable count).
4. The IIS algorithm populates the membership arrays, marking elements that belong to the minimal infeasible set.
5. If constraint names are available, the name array is allocated and each name string is individually duplicated.
6. The completed IISState is attached to the Model.

#### Mutation
- IISState is **write-once, read-many** after creation. The membership arrays and names are not modified after the IIS computation completes.
- A new IIS computation replaces the existing IISState entirely (the old one is freed first).

#### Destruction
1. Each individually allocated constraint name string is freed.
2. The constraint names array is freed.
3. The variable upper bound IIS array is freed.
4. The constraint IIS array is freed.
5. The variable lower bound IIS array is freed.
6. The IISState structure itself is freed.
7. The Model's reference to the IISState is set to null.

Destruction occurs when:
- The Model is freed.
- Solution data is cleared (e.g., before re-optimization).
- A new IIS computation is requested.
- The model is modified in ways that invalidate the IIS result.

### Invariants

1. **Minimality**: The set of elements marked as "in IIS" (value 1) across all three membership arrays must form an irreducible infeasible set: collectively infeasible and minimal under element removal. This is a semantic guarantee of the IIS algorithm, not enforced by the data structure itself.
2. **Consistent dimensions**: The constraint IIS array and the constraint names array (if present) both have exactly numConstrs entries. The variable bound IIS arrays each have numVars entries corresponding to the model dimensions at computation time.
3. **Binary membership**: All entries in the three membership arrays are either 0 or 1. No other values are valid.
4. **Name ownership**: If the constraint names array is non-null, the IISState owns every name string in it. No name string pointer is shared with the Model's own name storage.

### Thread Safety

IISState is **not thread-safe**. It is a passive data container that is created by a single IIS computation and read by attribute queries. Concurrent reads are safe; concurrent write and read (or concurrent IIS computations on the same Model) require external synchronization.

### Design Rationale

**Separate structure from solution data**: IIS results are conceptually distinct from optimization solutions. An IIS exists only when the model is infeasible (and thus has no feasible solution), making it natural to store IIS data in a dedicated structure rather than within solution storage.

**Independent name storage**: Constraint names are duplicated into the IISState rather than referencing the Model's name arrays. This protects the IIS results from being invalidated by subsequent model name changes and simplifies the ownership model for cleanup.

**Binary membership encoding**: Using integer 0/1 values rather than a bitmask allows direct exposure of the arrays through the public attribute API (IISConstr, IISLB, IISUB) without conversion. This encoding is consistent with the ConvexFeld API documentation.

**Published concept**: The IIS concept was formalized by Gleeson and Ryan (1990) in "Identifying Minimally Infeasible Subsystems of Inequalities," *European Journal of Operational Research*, 46(3):375-381. The algorithm for computing an IIS typically involves iteratively removing constraints and testing feasibility, as described by Chinneck and Dravnieks (1991).

---

## 2. ModificationTracker (Lazy Update Buffer)

### Purpose

ModificationTracker implements the deferred update pattern used in commercial LP solvers. When the user modifies a model through API calls (changing bounds, adding variables, altering coefficients, etc.), changes are not applied to the constraint matrix immediately. Instead, they are recorded in the ModificationTracker as pending operations. A subsequent explicit update call (or an implicit update triggered by optimization) processes all accumulated changes in a single batch operation. This amortizes the cost of internal data structure rebuilds across many modifications, avoiding the O(nnz) matrix reconstruction that would otherwise be required after each individual change.

### Fields

#### Status Flags

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| active | bool | Indicates whether any modifications are pending | true if modifications exist, false otherwise | When false, no other fields contain meaningful data |
| hasVariableModifications | bool | At least one variable-level modification is pending | true or false | true implies active is also true |
| hasConstraintModifications | bool | At least one constraint-level modification is pending | true or false | true implies active is also true |

#### Dimension Tracking

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| previousVariableCount | int | Number of variables in the model before any pending modifications were recorded | >= 0 | Captures the variable count at the time the first modification was recorded |
| previousConstraintCount | int | Number of constraints in the model before any pending modifications were recorded | >= 0 | Captures the constraint count at the time the first modification was recorded |

#### Scaling Changes

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| scalingChanged | bool | Indicates that the model's scaling configuration has been modified | true or false | [UNDETERMINED: interaction with coefficient rescaling] |
| objectiveScaling | double | New objective scaling factor if the objective scale has been changed | Any positive double | Meaningful only when scaling changes are pending |

#### Per-Element Modification Flags

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| variableFlags | array-of-bitmask [numVars] | Per-variable bitmask recording which attributes have been modified | See Variable Modification Bits below | Array length equals the current variable count (including newly added variables) |
| constraintFlags | array-of-bitmask [numConstrs] | Per-constraint bitmask recording which attributes have been modified | See Constraint Modification Bits below | Array length equals the current constraint count (including newly added constraints) |

#### Name Changes

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| modelNameChange | string or null | New model name if it has been changed; null if unchanged | Valid string or null | Owned by the tracker; freed during cleanup |
| variableNameChanges | array-of-string [numVars] or null | New variable names for variables whose names have been modified; null entries for unchanged variables | Valid string entries or null | Non-null only if at least one variable name was changed |
| constraintNameChanges | array-of-string [numConstrs] or null | New constraint names for constraints whose names have been modified; null entries for unchanged constraints | Valid string entries or null | Non-null only if at least one constraint name was changed |

#### Structural Modification Data

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| sosModifications | pointer-to-SOSModData or null | Pending SOS constraint additions or modifications | Null if no SOS changes | [UNDETERMINED: internal layout] |
| indicatorModifications | pointer-to-IndicatorModData or null | Pending indicator constraint additions or modifications | Null if no indicator changes | [UNDETERMINED: internal layout] |
| generalConstraintModifications | pointer-to-GenConstrModData or null | Pending general constraint (min, max, abs, piecewise-linear) additions | Null if no general constraint changes | [UNDETERMINED: internal layout] |
| variableTypeChanges | array-of-char [numVars] or null | New variable types for variables whose types have been modified | Standard variable type codes ('C', 'B', 'I', 'S', 'N') or null for unchanged | Non-null only if at least one variable type was changed |
| rangeModifications | pointer or null | Pending range constraint modifications | Null if no range changes | [UNDETERMINED: internal layout] |

### Variable Modification Bits

The per-variable bitmask uses the following semantic flags:

| Bit | Meaning |
|-----|---------|
| Lower bound changed | The variable's lower bound has been modified |
| Upper bound changed | The variable's upper bound has been modified |
| Objective coefficient changed | The variable's linear objective coefficient has been modified |
| Variable type changed | The variable's type (continuous, binary, integer, etc.) has been modified |
| Variable name changed | The variable's name string has been modified |

Additional bits exist for internal bookkeeping whose exact semantics are [UNDETERMINED].

### Constraint Modification Bits

The per-constraint bitmask uses the following semantic flags:

| Bit | Meaning |
|-----|---------|
| RHS changed | The constraint's right-hand side value has been modified |
| Sense changed | The constraint's sense (<=, >=, =) has been modified |
| Constraint name changed | The constraint's name string has been modified |
| Lazy flag changed | The constraint's lazy evaluation flag has been modified |

Additional bits exist for internal bookkeeping whose exact semantics are [UNDETERMINED].

### Relationships

- **Owned by** the Model. The Model holds a single optional reference to a ModificationTracker. This reference is null when no pending modifications exist (the tracker may be allocated lazily on first modification).
- **References** the Model's MatrixData for dimension information. The previous dimension counts allow detecting structural changes (variables or constraints added/deleted).
- **Consumed by** the update operation, which reads the tracker, applies changes to the Model's matrix data, and then clears or frees the tracker.

### Lifecycle

#### Creation
1. When the first modification API call is made on a model with no pending changes, a ModificationTracker is allocated.
2. The current variable and constraint counts are captured as previous dimensions.
3. Per-element flag arrays are allocated and zero-initialized at the current model dimensions.
4. The active flag is set.

#### Mutation
- Each API modification call (add variable, change bound, change coefficient, etc.) sets the corresponding bit(s) in the appropriate per-element flag array and stores any associated data.
- New variables and constraints are appended, extending the flag arrays as needed.
- Multiple modifications to the same element accumulate: the bitmask is OR'd with new flags.

#### Consumption (Flush)
1. The update operation reads the tracker to determine what has changed.
2. Modifications are classified by type and counted.
3. Each category of modification is applied to the Model's matrix data in a controlled order.
4. The tracker is cleared (all flags reset, temporary data freed) or the tracker is freed entirely.

#### Destruction
- On Model destruction, any pending ModificationTracker is freed without applying changes.
- All owned string data (name changes, model name) is freed.
- All owned sub-structures (SOS, indicator, general constraint modification data) are freed.
- The flag arrays are freed.

### Invariants

1. **Activation consistency**: If active is false, all per-element flag arrays must be empty or null and no modification data pointers may be non-null.
2. **Dimension coherence**: previousVariableCount and previousConstraintCount reflect the model dimensions at the time the tracker was activated. Any variables or constraints added since activation have indices >= these counts.
3. **Flag array sizing**: The variableFlags array has one entry for every variable in the model (both pre-existing and newly added). Similarly for constraintFlags.
4. **Mutual exclusion with optimization**: The tracker must be flushed before optimization begins. No modifications may be recorded while optimization is in progress.
5. **Idempotent flush**: Flushing a tracker with no pending modifications (active is false) is a no-op.

### Thread Safety

ModificationTracker is **not thread-safe**. It is designed for sequential modification from a single thread. The Model's modification-blocked flag prevents concurrent modification during optimization, but concurrent API modification calls on the same Model from different threads require external synchronization.

### Design Rationale

**Lazy update pattern**: This pattern is standard in commercial LP solvers and is explicitly documented in the ConvexFeld API reference. The key insight is that individual API calls like "add variable" or "change coefficient" are O(1) recording operations, while the actual matrix rebuild is batched into a single O(m + n + nnz) operation. This avoids the pathological case of n individual variable additions each triggering an O(nnz) matrix reconstruction.

**Per-element bitmasks**: Rather than storing a list of modified elements, the tracker uses per-element flag arrays that can be scanned in a single pass. This trades memory (one bitmask per element) for simplicity and predictable performance during the flush operation.

**Dimension snapshot**: Recording the previous dimensions allows the flush operation to distinguish between modifications to existing elements and additions of new elements, which require different processing paths (in-place update versus array extension).

**Published pattern**: The deferred update / lazy evaluation pattern in optimization software is described in Maros, *Computational Techniques of the Simplex Method* (Springer, 2003), Section 2.5, in the context of efficient model management for LP solvers.

---

## 3. WarmStartData

### Purpose

WarmStartData stores warm-start information that can accelerate re-optimization after model modifications. When a model is modified and re-solved, the solver can use information from the previous solution to reduce the number of iterations required to reach optimality. WarmStartData supports two complementary warm-start mechanisms: basis-based warm starting (used by the simplex method) and solution-based warm starting (used by the barrier/interior-point method). The structure also tracks validation status to ensure that warm-start data remains compatible with the current model dimensions and structure.

### Fields

#### Validation and Status

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| basisStatus | int | Validation status of the stored basis warm-start data | NOT_PROVIDED (0), PROVIDED (1), VALIDATED (2), INVALIDATED (3) | Transitions to INVALIDATED when structural model changes occur |
| solutionStatus | int | Validation status of the stored solution warm-start data | NOT_PROVIDED (0), PROVIDED (1), VALIDATED (2), INVALIDATED (3), PARTIAL (4) | Transitions to INVALIDATED when structural model changes occur |

#### Basis Warm-Start Data (Simplex)

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| variableBasisStatus | array-of-int [numVars] or null | Per-variable basis status for warm starting the simplex method. Uses the standard simplex basis status encoding: basic, at lower bound, at upper bound, superbasic, or fixed. | Standard basis status codes, or null if no basis warm-start | Null when basisStatus is NOT_PROVIDED |
| constraintBasisStatus | array-of-int [numConstrs] or null | Per-constraint (slack variable) basis status for warm starting the simplex method | Standard basis status codes, or null if no basis warm-start | Null when basisStatus is NOT_PROVIDED; must have exactly numConstrs basic entries to form a valid basis together with variableBasisStatus |

#### Solution Warm-Start Data (Barrier)

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| primalDualValues | array-of-double or null | Primal and/or dual solution values for warm starting the barrier method. The barrier method can use a prior interior-point solution as a starting point. | Finite doubles within reasonable bounds, or null if no solution warm-start | Null when solutionStatus is NOT_PROVIDED |

#### Basis Factorization Cache

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| factorizationData | pointer-to-FactorizationCache or null | Cached basis factorization (LU decomposition) from the previous solve, enabling the simplex method to skip the initial factorization step if the basis has not changed | Null if no cached factorization, or pointer to valid factorization data | Invalidated whenever the basis status arrays change; must be consistent with the stored basis |

The FactorizationCache sub-structure contains:

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| indices | array-of-int or null | Index arrays from the LU factorization | Valid index values | Parallel with values array |
| values | array-of-double or null | Numeric values from the LU factorization | Finite doubles | Parallel with indices array |

### Relationships

- **Owned by** the Model. The Model holds a single optional reference to a WarmStartData structure.
- **References** the Model's problem dimensions implicitly. The array sizes must match the model's current variable and constraint counts for the warm-start to be valid.
- **Consumed by** the solver initialization. When the simplex or barrier method begins, it checks for available warm-start data, validates it, and uses it to initialize the starting point.
- **Invalidated by** the update operation. When structural model changes are applied (variables or constraints added/deleted), the update operation checks whether existing warm-start data remains compatible and discards it if not.

### Lifecycle

#### Creation
1. When the user sets basis status attributes (VBasis/CBasis) or solution start attributes (PStart/DStart) through the API, a WarmStartData structure is allocated if one does not already exist.
2. The corresponding status field (basisStatus or solutionStatus) is set to PROVIDED.
3. Arrays are allocated at current model dimensions and populated with the user-supplied values.

#### Validation
During the model update operation (when pending modifications are flushed):
1. If the model dimensions have not changed and no structural modifications occurred, the warm-start data remains valid.
2. If bounds, objective coefficients, or RHS values changed but dimensions are unchanged, basis warm-start data typically survives while solution warm-start data may be discarded (barrier starting points are sensitive to these changes).
3. If variables or constraints were added or deleted (structural change), all warm-start data is invalidated and discarded, with a diagnostic warning message.
4. The status fields are updated to reflect the validation outcome.

#### Consumption
During solver initialization:
1. If basisStatus indicates valid data, the simplex method uses the stored basis status arrays to construct its initial basis, bypassing the crash procedure.
2. If solutionStatus indicates valid data, the barrier method uses the stored solution values as its starting point.
3. If factorizationData is available and consistent with the current basis, the solver can reuse the LU factorization.
4. After consumption, the warm-start data may be retained for future re-solves or cleared depending on solver policy.

#### Destruction
1. The factorization cache (if present) is freed: indices array, values array, then the sub-structure itself.
2. The primal/dual values array is freed.
3. The constraint basis status array is freed.
4. The variable basis status array is freed.
5. The WarmStartData structure itself is freed.
6. The Model's reference is set to null.

Destruction occurs when:
- The Model is freed.
- Warm-start data is explicitly invalidated by structural model changes.
- A warm-start reset mode is activated in the environment.
- Solution data is cleared.

### Invariants

1. **Status-array consistency**: If basisStatus is NOT_PROVIDED, both variableBasisStatus and constraintBasisStatus must be null. If basisStatus is PROVIDED or VALIDATED, both must be non-null.
2. **Dimension compatibility**: If warm-start arrays are non-null, their lengths must match the model's current variable and constraint counts. A dimension mismatch triggers invalidation.
3. **Basis completeness**: For a valid basis warm-start, the combined variable and constraint basis status arrays must designate exactly numConstrs basic elements (one per constraint row).
4. **Factorization consistency**: If factorizationData is non-null, basisStatus must be VALIDATED and the factorization must correspond to the current basis. Any change to the basis status arrays invalidates the factorization.
5. **Solution finiteness**: All values in primalDualValues must be finite (no infinities or NaNs). Values outside the solver's infinity threshold are considered invalid.

### Thread Safety

WarmStartData is **not thread-safe**. It is typically written by API attribute-setting calls and read by solver initialization, both of which operate under the Model's single-threaded access model. The Model's modification lock prevents concurrent access during optimization.

### Design Rationale

**Dual warm-start mechanisms**: The simplex and barrier methods require fundamentally different types of starting information. The simplex method needs a basis (combinatorial structure), while the barrier method needs a solution point (continuous values). Supporting both allows efficient re-optimization regardless of the solver algorithm chosen.

**Validation tracking**: Warm-start data can become incompatible with the model after modifications. Rather than immediately discarding warm-start data on every modification, the structure tracks a validation status that is checked at update time. This allows bound and objective changes (which preserve basis validity) to retain the warm-start, while structural changes (which invalidate the basis) trigger cleanup.

**Factorization caching**: Caching the basis LU factorization between solves can save significant computation, as factorization is often the most expensive part of simplex initialization. This optimization is described by Bixby (2002) in "Solving Real-World Linear Programs: A Decade and More of Progress," *Operations Research*, 50(1):3-15.

**Published concept**: Warm starting in LP is a well-established technique described in Yildirim and Wright (2002), "Warm-Start Strategies in Interior-Point Methods for Linear Programming," *SIAM Journal on Optimization*, 12(3):782-810, for barrier methods, and is a natural consequence of the simplex method's basis representation as described in Dantzig (1963).

---

## 4. CrossoverState

### Purpose

CrossoverState is not a standalone data structure but rather a set of additional fields within (or conceptually layered upon) the SolverState that are used exclusively during barrier-to-simplex crossover. After the barrier (interior-point) method produces a solution in the interior of the feasible region, a crossover procedure transforms this interior solution into a basic feasible solution (vertex solution) suitable for the simplex method. The crossover-specific state tracks which variables have been processed, whether they have been snapped to bounds, and accumulates performance metrics for the crossover phases.

### Fields

The following fields are crossover-specific extensions of SolverState, active only during crossover:

#### Crossover Progress Tracking

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| isDualSimplex | int | Selects whether crossover uses primal or dual simplex variable-change operations | 0 (primal) or 1 (dual) | Set at crossover initialization based on the chosen post-crossover simplex mode |
| initMode | int | Crossover initialization mode controlling the algorithm path taken | [UNDETERMINED: full set of modes] | Set before crossover begins; read-only during crossover |

#### Quadratic Objective Handling

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| diagonalQ | array-of-double [numVars] or null | Diagonal entries of the quadratic objective matrix (Q_ii terms). Used during crossover to compute optimal variable placements for quadratic objectives. | Any finite double; null for pure LP | Indexed by variable; modified during binary penalty conversion |
| offDiagonalCounts | array-of-int [numVars] or null | Count of off-diagonal quadratic entries per variable. Variables with nonzero off-diagonal counts require joint optimization and are skipped during crossover's per-variable processing. | >= 0; null for pure LP | Read-only during crossover |
| binaryConversionCount | int | Count of binary variables whose quadratic terms have been converted to linear penalties during crossover | >= 0 | Monotonically non-decreasing during crossover; used for diagnostics |

#### Error Diagnostics

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| errorVariableIndex | int | Index of the variable that caused a crossover error, stored for diagnostic reporting when crossover fails | 0 to numVars-1, or -1 if no error | Set only when a crossover error occurs (return code indicating crossover failure) |

#### Performance Metrics

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| timingWeight | double | Scaling factor for timing accumulation, reflecting the computational cost per operation | > 0 | Set at crossover initialization |
| timingAccumulator | pointer-to-double or null | Pointer to an external timing counter that accumulates weighted operation counts for progress reporting | Null to disable timing; non-null for active timing | Points to a timing counter owned by the caller |

### Relationships

- **Embedded in** SolverState. The crossover-specific fields exist as part of the SolverState structure but are meaningful only during the crossover phase.
- **References** the Model for environment parameters such as infinity thresholds, crossover configuration flags, and quadratic objective presence flags.
- **Uses** the SolverState's standard fields: variable status array, bounds arrays, objective coefficients, constraint matrix, and pricing state. These shared fields are read and modified by crossover operations just as they are by simplex iterations.
- **Interacts with** the PricingState through invalidation calls. When crossover modifies a variable's value or objective coefficient, it invalidates the pricing cache for that variable.

### Lifecycle

#### Activation
1. After the barrier method completes and crossover is requested, the crossover-specific fields in SolverState are initialized.
2. The isDualSimplex flag is set based on the intended post-crossover simplex mode.
3. If the problem has a quadratic objective, the diagonal Q array and off-diagonal counts are populated from the Model's quadratic data.
4. The binary conversion counter is set to zero.
5. The timing accumulator pointer is set from the caller's timing infrastructure.

#### Crossover Execution
The crossover procedure operates in distinct phases:

1. **Quadratic variable processing**: For each variable with a diagonal quadratic term (and no off-diagonal terms), compute the unconstrained optimum of the 1D sub-problem min(c*x + 0.5*Q*x^2) subject to bounds. For integer variables, round to the nearest feasible integer. Push the variable to its computed optimal value. For binary variables with diagonal quadratic terms, convert the quadratic penalty to a linear coefficient adjustment using the identity x^2 = x for x in {0, 1}.

2. **Bound snapping**: For each variable in the interior-point solution, classify it based on its distance to the nearest bound:
   - If close to the lower bound (within tolerance): snap to lower bound, mark as non-basic at lower bound.
   - If close to the upper bound (within tolerance): snap to upper bound, mark as non-basic at upper bound.
   - If interior to both bounds: mark as superbasic (requires later processing by simplex).
   - If both bounds are infinite (free variable): mark as basic.

3. **Feasibility maintenance**: After each variable snap, update the right-hand side values of affected constraints to maintain feasibility: rhs' = rhs - a_ji * (x_new - x_old) for each constraint j containing the snapped variable i.

4. **Simplex cleanup**: The simplex method is invoked to process remaining superbasic variables and achieve a fully basic feasible solution.

#### Deactivation
After crossover completes (or fails), the crossover-specific fields are no longer meaningful. The SolverState transitions to standard simplex mode, and any crossover-specific arrays (if separately allocated) are freed during simplex cleanup.

### Invariants

1. **Mode consistency**: isDualSimplex must agree with the SolverState's solve mode. Primal crossover (isDualSimplex = 0) uses primal variable-change operations; dual crossover (isDualSimplex = 1) uses dual variable-change operations.
2. **Quadratic array consistency**: If diagonalQ is non-null, it must have numVars entries. If offDiagonalCounts is non-null, it must also have numVars entries. Both must be null simultaneously for pure LP (non-quadratic) problems.
3. **Binary conversion validity**: Binary penalty conversion (c' = c + 0.5*Q_ii, Q_ii = 0) may only be applied to binary variables with zero off-diagonal quadratic counts.
4. **Error variable validity**: errorVariableIndex is meaningful only when crossover returns an error code. It must be a valid variable index (0 to numVars-1).
5. **Variable classification completeness**: After bound snapping completes, every variable must be classified as either non-basic at a bound, superbasic, or basic. No variable may remain unclassified.

### Thread Safety

CrossoverState inherits SolverState's thread safety model: **not thread-safe**. Crossover operates within a single-threaded simplex solve context. Each concurrent solver thread has its own SolverState with its own crossover-specific fields.

### Design Rationale

**Embedding in SolverState rather than a separate structure**: Crossover is a transitional phase between barrier and simplex that uses the same working arrays (bounds, status, matrix) as the simplex method. Allocating a separate crossover structure would require either duplicating these arrays or adding indirection. Embedding the few crossover-specific fields directly in SolverState avoids both costs.

**Per-variable processing for diagonal Q**: Variables with purely diagonal quadratic terms (no cross-product terms Q_ij with i != j) can be optimized independently in O(1) per variable. This is a standard separability observation for diagonal QP problems. Variables with off-diagonal terms require joint optimization and are deferred to the simplex phase.

**Binary penalty conversion**: The identity x^2 = x for x in {0, 1} is a well-known reformulation technique in mixed-integer quadratic programming. Converting diagonal quadratic terms to linear penalties for binary variables reduces the effective problem to a MILP for those variables, enabling more effective presolve and simplex processing. This technique is described in Sherali and Adams, *A Reformulation-Linearization Technique for Solving Discrete and Continuous Nonconvex Problems* (Springer, 1999).

**Bound-snapping heuristic**: The crossover bound-snapping procedure follows the approach described by Megiddo (1991) in "On Finding Primal- and Dual-Optimal Bases," *ORSA Journal on Computing*, 3(1):63-65, and elaborated by Andersen and Ye (1996) in "A Computational Study of the Homogeneous Algorithm for Large-Scale Convex Optimization," *Computational Optimization and Applications*, 5(3):227-247. The core idea is to identify variables whose interior-point values are close to bounds and fix them, progressively reducing the problem to one where the remaining superbasic variables can be resolved by the simplex method.

---

## Combined References

- Andersen, E.D. and Ye, Y. (1996). "A Computational Study of the Homogeneous Algorithm for Large-Scale Convex Optimization." *Computational Optimization and Applications*, 5(3):227-247.
- Bixby, R.E. (2002). "Solving Real-World Linear Programs: A Decade and More of Progress." *Operations Research*, 50(1):3-15.
- Chinneck, J.W. and Dravnieks, E.W. (1991). "Locating Minimal Infeasible Constraint Sets in Linear Programs." *ORSA Journal on Computing*, 3(2):157-168.
- Dantzig, G.B. (1963). *Linear Programming and Extensions*. Princeton University Press.
- Gleeson, J. and Ryan, J. (1990). "Identifying Minimally Infeasible Subsystems of Inequalities." *European Journal of Operational Research*, 46(3):375-381.
- Maros, I. (2003). *Computational Techniques of the Simplex Method*. Springer.
- Megiddo, N. (1991). "On Finding Primal- and Dual-Optimal Bases." *ORSA Journal on Computing*, 3(1):63-65.
- Sherali, H.D. and Adams, W.P. (1999). *A Reformulation-Linearization Technique for Solving Discrete and Continuous Nonconvex Problems*. Springer.
- Yildirim, E.A. and Wright, S.J. (2002). "Warm-Start Strategies in Interior-Point Methods for Linear Programming." *SIAM Journal on Optimization*, 12(3):782-810.

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
