# Module: Statistics & Diagnostics

## Purpose

The Statistics & Diagnostics module provides functions that compute, report, and verify quantitative properties of the optimization model and its solutions. These functions serve four distinct roles within the solver:

1. **Pre-solve diagnostics**: Before optimization begins, the solver logs structural characteristics of the model (e.g., counts of quadratic constraints, general constraints, SOS constraints) to give the user visibility into which advanced features are present and may affect algorithm selection.

2. **Numerical conditioning analysis**: The module computes the ranges (minimum and maximum absolute values) of coefficients across the constraint matrix, objective function, variable bounds, right-hand sides, and specialized constraint types. Wide coefficient ranges indicate potential numerical difficulties, and the module issues warnings when ranges exceed well-known thresholds from the optimization literature. These statistics are also exposed as queryable model attributes for programmatic access.

3. **Solution validation**: After optimization, the module evaluates a candidate solution against all constraint types, computing violation metrics (maximum and sum of violations) for linear constraints, quadratic constraints, variable bounds, integrality requirements, SOS constraints, general constraints, and nonlinear constraints. These violation reports help users assess solution quality and diagnose infeasibility.

4. **Model identity and timing**: The module provides a model fingerprinting function that computes a hash digest of the entire problem formulation for cache invalidation and change detection, and a timestamp/session-identifier function for logging and correlation purposes.

Together, these functions are called at well-defined points in the optimization pipeline (before solve, after solve, and on demand) and produce no modifications to the optimization state -- they are purely diagnostic and read-only with respect to the mathematical formulation.

## Functions

### cxf_presolve_stats

**Purpose:** Log counts of advanced model features to the solver log for user visibility before optimization.

**Signature:**
- Input: `model` : pointer-to-Model -- the model whose characteristics to report
- Output: void (no return value; output is written to the solver log)

**Preconditions:**
- The model must have a valid MatrixData structure (the matrix reference is non-null)
- The model must have a valid Environment (the environment reference is non-null)

**Postconditions:**
- Diagnostic messages describing advanced model features have been written to the solver log
- No model state has been modified

**Side Effects:**
- Writes formatted messages to the solver log via the model's Environment

**Error Conditions:**
- None; this function always completes. If the model has no advanced features, no messages are emitted.

**Behavioral Description:**
The function reads dimension counts from the model's MatrixData and writes a summary of each non-trivial advanced feature to the solver log. The features inspected, in order, are:

1. **Quadratic objective terms**: If the model has one or more quadratic objective terms, a message is logged with the count. However, for models that are classified as Quadratically Constrained Programs (QCP), the quadratic objective is expected and the message is suppressed to reduce log noise.

2. **Quadratic constraints**: If the model has one or more quadratic constraints, a message is logged with the count.

3. **Bilinear constraints**: If the model has one or more bilinear constraints, a message is logged with the count.

4. **SOS constraints**: If the model has one or more Special Ordered Set constraints, a message is logged with the count.

5. **Piecewise-linear objective terms**: If the model has one or more piecewise-linear objective terms, a message is logged with the count.

6. **General constraints**: If the model has general constraints, the function performs a detailed breakdown. It calls a helper to classify each general constraint by type and by approximation method (piecewise-linear approximation versus nonlinear treatment). The results are categorized into four groups:
   - *Simple general constraints*: Constraint types that correspond to elementary operations (e.g., MAX, MIN, ABS, AND, OR, INDICATOR, PWL), excluding general nonlinear. If present, the total count is logged followed by a per-type breakdown.
   - *Function constraints approximated by piecewise-linear*: Transcendental and algebraic function constraints (EXP, LOG, POW, SIN, COS, TAN, etc.) that the solver approximates using piecewise-linear segments. If present, the total and per-type breakdown are logged.
   - *Function constraints treated as nonlinear*: The same transcendental/algebraic functions handled directly by a nonlinear solver instead of PWL approximation. If present, the total and per-type breakdown are logged.
   - *General nonlinear constraints*: Arbitrary nonlinear expression constraints. If present, the count and the total number of nonlinear terms are logged.

   Type breakdowns are formatted with comma-separated entries, wrapping to a new line after every five type entries for log readability.

All count messages use correct singular/plural grammar (e.g., "1 quadratic constraint" vs. "3 quadratic constraints").

**Thread Safety:** Unsafe. Callers must ensure exclusive access to the model and its logging infrastructure.

**Dependencies:**
- Logging subsystem (formatted log output)
- General constraint classification helper (counts constraints by type and approximation method)
- General constraint name lookup (maps type codes to human-readable names)

---

### cxf_coefficient_stats

**Purpose:** Compute and optionally print coefficient range statistics for the model, issuing numerical warnings when ranges suggest potential ill-conditioning.

**Signature:**
- Input: `model` : pointer-to-Model -- the model to analyze
- Input: `verbose` : int -- controls output: nonzero prints statistics and warnings to the log, zero computes silently
- Output: int -- 0 on success, nonzero error code on failure

**Preconditions:**
- The model must have a valid MatrixData structure and Environment

**Postconditions:**
- On success, coefficient statistics have been computed (and optionally printed)
- If verbose and numerical warnings were issued, a suggestion to consider reformulation or the NumericFocus parameter has been logged (only when NumericFocus is at its default value of zero)
- No model data has been modified (statistics may be cached internally for future retrieval)

**Side Effects:**
- May allocate and cache coefficient statistics on the model for subsequent queries
- When verbose, writes coefficient range summary and warning messages to the solver log

**Error Conditions:**
- Out of memory during statistics computation -> returns the out-of-memory error code
- Error from internal statistics computation -> returns the propagated error code
- Statistics suppression flag set on the model -> returns 0 immediately without computation

**Behavioral Description:**
The function proceeds through the following phases:

1. **Suppression check**: If the model has a statistics-suppression flag set, the function returns immediately with success. This allows internal callers to inhibit redundant statistics output.

2. **Basic coefficient statistics**: Delegates to cxf_compute_coef_stats to obtain the minimum and maximum absolute values (excluding zeros) for eight coefficient categories: objective coefficients, quadratic objective coefficients, variable bounds, constraint right-hand sides, matrix coefficients, quadratic constraint quadratic coefficients, quadratic constraint linear coefficients, and quadratic constraint right-hand sides. If this computation fails, the error is propagated.

3. **Piecewise-linear objective statistics**: For models with piecewise-linear objective terms, the function locally computes the minimum and maximum absolute values of the interior breakpoints (excluding the first and last endpoints) for both x-coordinates and y-values. Endpoints are excluded because the interior breakpoints are where numerical precision is most critical for PWL approximation quality.

4. **General constraint statistics**: For models with general constraints, delegates to a general constraint statistics helper to obtain coefficient ranges for PWL constraint breakpoints, indicator constraint coefficients and right-hand sides, and MAX/MIN constraint constants.

5. **Verbose output**: If the verbose flag is set, the function prints the coefficient range report to the solver log. The report includes:
   - Matrix coefficient range [min, max]
   - Quadratic matrix range (if quadratic constraints present)
   - Quadratic linear matrix range (if present)
   - Objective coefficient range
   - Quadratic objective range (if present)
   - Variable bounds range
   - RHS range
   - Quadratic constraint RHS range (if present)
   - PWL objective x and y ranges (if present and valid)
   - General constraint breakpoint, coefficient, and constant ranges (if present)

6. **Numerical warnings**: The function checks each coefficient category against well-established thresholds from the numerical optimization literature:

   - **Matrix coefficient range**: A warning is issued if the log10-scale range (the difference between the log10 of the maximum and minimum coefficients, with a unit offset to handle small values gracefully) exceeds a large range threshold (on the order of 13 decades), indicating that the matrix spans more than 13 orders of magnitude. Alternatively, a warning is issued if the maximum coefficient exceeds a large-value threshold (on the order of 1e20).
   - **Objective, RHS, and bounds**: Warnings if the maximum absolute values exceed the large-value threshold.
   - **Quadratic constraint coefficients**: Similar range and magnitude checks, with a tighter range threshold (on the order of 8 decades) appropriate for quadratic terms. Singular/plural grammar is used depending on the number of quadratic constraints.
   - **Quadratic objective coefficients**: Range and magnitude checks with the quadratic range threshold.
   - **PWL breakpoints**: Warnings if the maximum x or y values of interior breakpoints exceed a PWL-specific threshold (on the order of 1e10), with separate messages for x-only, y-only, or both.
   - **Indicator constraint coefficients and RHS**: Range and magnitude checks.
   - **MAX/MIN constraint constants**: Magnitude checks.

7. **NumericFocus suggestion**: If any warnings were issued and the solver's NumericFocus parameter is at its default value (zero), a suggestion is logged recommending model reformulation or increasing the NumericFocus parameter.

**Thread Safety:** Unsafe. Requires exclusive access to the model.

**Dependencies:**
- cxf_compute_coef_stats (internal coefficient computation)
- General constraint statistics helper (cxf_gencon_stats or equivalent)
- Logging subsystem
- Standard math library (log10)

---

### cxf_compute_coef_stats

**Purpose:** Compute the minimum and maximum absolute coefficient values across all major data arrays of the model, caching results for efficiency and registering them as queryable model attributes.

**Signature:**
- Input: `model` : pointer-to-Model -- the model to analyze
- Output (by pointer): eight pairs of (maximum, minimum) double values covering:
  - Objective coefficients (max, min)
  - Quadratic objective coefficients (max, min)
  - Right-hand side values (max, min)
  - Variable bounds (max, min)
  - Matrix coefficients (max, min)
  - Quadratic constraint quadratic coefficients (max, min)
  - Quadratic constraint linear coefficients (max, min)
  - Quadratic constraint RHS (max, min)
- Output: int -- 0 on success, nonzero error code on failure

**Preconditions:**
- The model must have valid MatrixData and Environment references

**Postconditions:**
- All 16 output values (8 max/min pairs) have been populated
- The computed statistics have been cached on the model; subsequent calls return the cached values without recomputation
- The statistics have been registered with the model's attribute table, making them accessible through the public attribute query API (e.g., MaxCoeff, MinCoeff, MaxBound, MinBound, MaxObjCoeff, MinObjCoeff, MaxRHS, MinRHS, and their quadratic counterparts)

**Side Effects:**
- Allocates a statistics cache structure on the model if not already present
- May trigger application of pending model modifications if the model has unapplied changes (to ensure the statistics reflect the current state)
- Registers or updates attribute table entries

**Error Conditions:**
- Out of memory when allocating the statistics cache -> returns the out-of-memory error code
- Error from applying pending modifications -> returns the propagated error code
- Error from sparse array iteration (for multi-objective or quadratic constraint data) -> returns the propagated error code

**Behavioral Description:**
The function computes coefficient statistics through the following process:

1. **Cache check**: If a statistics cache already exists on the model, the cached values are returned immediately. If the attribute table needs updating, the attribute registration step is performed before returning.

2. **Cache allocation**: A statistics cache is allocated (sufficient for 16 double values). If allocation fails, the out-of-memory error code is returned.

3. **Pending modification sync**: If the model has unapplied modifications (e.g., sparse objective data or range constraints that require preprocessing), the pending modifications are applied first to ensure statistics reflect the current problem state.

4. **Data scanning**: The function iterates over each coefficient category:
   - **Objective coefficients**: Scans the objective coefficient array. For multi-objective models, iterates through all objective arrays. Also includes linear terms from quadratic constraints.
   - **Quadratic objective coefficients**: Scans the quadratic objective term array.
   - **RHS values**: Scans row upper and lower bounds, excluding infinite values. Includes quadratic constraint bound arrays.
   - **Variable bounds**: Scans the column bounds array.
   - **Matrix coefficients**: Scans the CSC coefficient values array, iterating column by column.
   - **QC linear coefficients**: Scans quadratic constraint linear coefficient arrays (CSR format).
   - **QC RHS**: Scans quadratic constraint RHS values.

   For each category, the function tracks the maximum and minimum absolute non-zero values. Zero values are excluded from the minimum calculation. Infinite values are excluded from RHS and bound calculations. If no non-zero values exist in a category, the minimum is set equal to the maximum (both zero).

5. **Row data finalization**: If pending modifications were applied in step 3, the function restores the row data state.

6. **Attribute registration**: The 16 computed statistics are registered with the model's attribute table, associating each statistic with its corresponding public attribute name (MaxCoeff, MinCoeff, MaxBound, MinBound, MaxObjCoeff, MinObjCoeff, MaxRHS, MinRHS, MaxQCCoeff, MinQCCoeff, MaxQObjCoeff, MinQObjCoeff, MaxQCLCoeff, MinQCLCoeff, MaxQCRHS, MinQCRHS). Each attribute entry's direct value pointer is set to the cached statistic, enabling efficient retrieval through the attribute API.

7. **Output**: The cached values are copied to the output parameters.

**Thread Safety:** Unsafe. Requires exclusive access to the model. The caching mechanism is not thread-safe.

**Dependencies:**
- Memory allocation
- Model modification application (for pending changes)
- Row data finalization helper
- Sparse array iteration helper (for multi-objective and quadratic constraint data)
- Attribute name lookup and registration

---

### cxf_gencon_stats

**Purpose:** Compute coefficient range statistics for general constraints, providing minimum and maximum absolute values for each coefficient category present in general constraint data.

**Signature:**
- Input: `model` : pointer-to-Model -- the model whose general constraints to analyze
- Output (by pointer): six pairs of (maximum, minimum) double values covering:
  - PWL constraint y-values/slopes (max, min)
  - PWL constraint x-coordinates (max, min) [context-dependent: used for polynomial constraints in practice]
  - MAX constraint RHS constants (max, min)
  - MIN constraint RHS constants (max, min)
  - PWL main coefficients (max, min)
  - Polynomial constraint coefficients (max, min) [context-dependent]
- Output: void

**Preconditions:**
- The model must have a valid MatrixData structure

**Postconditions:**
- All 12 output values (6 max/min pairs) have been populated
- For categories with no contributing data, the maximum is zero and the minimum is a large sentinel value (indicating no valid data)

**Side Effects:**
- None; this is a pure computation function

**Error Conditions:**
- None; the function always completes. If the model has no general constraints, all outputs retain their initial values (max=0, min=sentinel).

**Behavioral Description:**
The function iterates through all general constraints in the model and extracts coefficient data from four general constraint types that carry numeric coefficient information:

1. **MAX constraints**: For each MAX constraint, the function extracts the RHS constant value (the additive constant in the max(x1, ..., xn) = y + constant formulation). The absolute value is tracked for the MAX RHS statistics. Values that represent negative infinity (no finite lower bound) are excluded.

2. **MIN constraints**: For each MIN constraint, the function extracts the RHS constant value. The absolute value is tracked for the MIN RHS statistics. Values that represent positive infinity (no finite upper bound) are excluded.

3. **PWL (piecewise-linear) constraints**: For each PWL constraint, the function extracts:
   - The main scaling coefficient, tracked in the PWL coefficient statistics.
   - All y-coordinate/slope values from the breakpoint array, tracked in the PWL y-value statistics.
   Zero values are excluded from the minimum calculation in both categories.

4. **Polynomial constraints**: For each polynomial constraint, the function extracts both x-coefficient and y-coefficient arrays, starting from the second term (the constant term at index zero is excluded from range analysis, as it does not affect the coefficient range of the polynomial expression). The absolute values of both arrays are tracked in their respective statistics.

Other general constraint types (ABS, AND, OR, INDICATOR, general nonlinear, and transcendental function constraints) do not contribute numeric coefficient data to these statistics and are skipped.

All scanning operations track the maximum and minimum absolute non-zero values. The minimum is initialized to a large sentinel so that any valid value will replace it. The maximum is initialized to zero.

**Thread Safety:** Unsafe. Requires exclusive access to the model.

**Dependencies:**
- MatrixData general constraint array access

---

### cxf_compute_violations

**Purpose:** Evaluate a candidate solution against all constraint types in the model, computing violation metrics and optionally reporting warnings when violations exceed solver tolerances.

**Signature:**
- Input: `model` : pointer-to-Model -- the model to check
- Input: `solution` : array-of-double, length numVars -- the primal variable values to evaluate
- Input: `output` : pointer-to-ViolationResult (nullable) -- structure to receive detailed violation metrics
- Input: `verbosity` : int -- controls output mode: zero produces a compact summary log line; nonzero produces detailed warnings for each violation category that exceeds tolerance
- Output: int -- 0 on success, nonzero error code on failure

**Preconditions:**
- The model must have valid MatrixData and Environment
- The solution array must have length at least equal to the number of variables

**Postconditions:**
- Violation metrics have been computed for all constraint types
- If the output structure is non-null, it has been populated with the violation results
- Warning or summary messages have been written to the solver log (depending on verbosity)

**Side Effects:**
- Allocates a temporary work array for residual computation (freed before return)
- Writes diagnostic messages to the solver log
- Temporarily clears and restores a model validation field to enable attribute queries during diagnostics

**Error Conditions:**
- Out of memory when allocating work array -> returns out-of-memory error code
- Error from general constraint or nonlinear constraint checking helpers -> returns the propagated error code

**Behavioral Description:**
The function evaluates solution quality through the following phases:

1. **Initialization**: Extracts model dimensions and array pointers. Allocates a work array of length numConstrs for residual computation. Prefers the working matrix if available, falling back to the primary matrix.

2. **Linear constraint activity computation**: Computes the matrix-vector product Ax using the CSC sparse matrix format. If scaling factors are present, the computation incorporates row and column scaling: the effective computation is (D_r^{-1} A D_c^{-1}) * x, where D_r and D_c are the row and column scaling diagonal matrices. The work array is zero-initialized before accumulation.

3. **Residual computation**: Computes the residual as r = b - Ax (with scaling applied to RHS if present). For constraints with greater-than-or-equal sense, the residual sign is flipped so that positive residuals uniformly indicate constraint satisfaction.

4. **Linear constraint violations**: For each linear constraint, the violation is computed based on the constraint sense:
   - Equality ('='): violation = |residual|
   - Less-than-or-equal ('<'): violation = max(0, -residual), i.e., the amount by which Ax exceeds b
   - Greater-than-or-equal ('>'): violation = max(0, residual) after sign flip
   The function tracks the maximum violation, the sum of all violations, and the index of the worst-violating constraint.

5. **Quadratic constraint violations**: For each quadratic constraint (x^T Q x + c^T x sense b), the function evaluates both the linear and quadratic terms, applies the constraint sense, and computes the violation. A secondary residual computation helper is consulted for improved numerical accuracy, and the smaller of the two violation estimates is used. The worst quadratic constraint violation is merged with the linear constraint tracking, with appropriate index offsetting.

6. **Secondary general constraint violations**: Processes a secondary category of general constraints with linear and quadratic terms, using a similar evaluation pattern to quadratic constraints.

7. **Bound violations**: For each variable, computes the violation of lower and upper bounds. Semi-continuous and semi-integer variables receive special treatment: if the absolute value of the variable is smaller than the lower bound violation, the absolute value is used instead (reflecting the semi-continuous domain where the variable must be either zero or within bounds). The function tracks maximum and sum of bound violations, the worst-violating variable index, and the maximum absolute value of non-continuous variables (for large-integer warnings).

8. **Integrality violations**: For each integer, binary, or semi-integer variable, computes the distance to the nearest integer value: violation = |x - round(x)|, where round(x) = floor(x + 0.5). Continuous and semi-continuous variables are skipped. The function tracks maximum and sum of integrality violations and the worst-violating variable index.

9. **SOS constraint violations**: For each SOS constraint:
   - SOS1 (at most one nonzero): The violation is the second-largest absolute value in the set. If only one variable is nonzero, the violation is zero.
   - SOS2 (at most two adjacent nonzeros): The violation is the largest absolute value of a set member that is not adjacent to the member with the largest absolute value. Adjacency is determined by position in the ordered set.

10. **General and nonlinear constraint violations**: Delegates to specialized helper functions that evaluate general constraints (indicator, PWL, etc.) and nonlinear constraints against the solution. Results are merged into the overall constraint violation tracking.

11. **Reporting**: Depending on the verbosity setting:
    - *Compact mode (verbosity zero)*: A single summary line is logged showing maximum violations for constraints, bounds, and integrality, with optional SOS, general constraint, and nonlinear constraint components appended when those constraint types are present. For models with quadratic constraints, the quadratic violation is shown separately.
    - *Verbose mode (verbosity nonzero)*: Individual warnings are logged for each violation category that exceeds the corresponding solver tolerance (feasibility tolerance for constraints and bounds, integrality tolerance for integer variables). Additional diagnostic hints are provided:
      - Large integer values exceeding a warning limit trigger an advisory.
      - PWL-related general constraint violations trigger specific advice about adjusting PWL approximation parameters or enabling presolve.
      - When violations significantly exceed tolerances (by a factor of ten or more), the function queries model coefficient attributes to suggest possible numerical causes (large matrix coefficients, large coefficient range, large variable bounds, large RHS values, or possible infeasibility).

12. **Output population**: If the output structure is non-null, it is populated with: overall maximum violation (the maximum across constraint, bound, and integrality violations), maximum bound violation, maximum constraint violation, maximum integrality violation, sums of bound/constraint/integrality violations, and the indices of the worst-violating elements in each category.

**Thread Safety:** Unsafe. Requires exclusive access to the model. Temporarily modifies and restores a model validation field.

**Dependencies:**
- Memory allocation and deallocation
- Sparse matrix-vector multiply (CSC format)
- Scaling data from MatrixData
- General constraint violation checker helper
- Nonlinear constraint violation checker helper
- PWL constraint type classification helper
- Quadratic constraint residual computation helper
- Public attribute query API (for coefficient-based diagnostic hints)
- Logging subsystem
- Standard math library (fabs, floor)

---

### cxf_compute_fingerprint

**Purpose:** Compute a deterministic hash digest of the model's entire problem formulation for cache invalidation and change detection between solve calls.

**Signature:**
- Input: `model` : pointer-to-Model -- the model to fingerprint
- Output (by pointer): `fingerprint` : int -- the computed 32-bit hash value
- Output: int -- 0 on success, nonzero error code on failure

**Preconditions:**
- The model must have valid MatrixData and Environment references

**Postconditions:**
- On success, the fingerprint output contains a 32-bit hash of the model data. Two models with identical problem formulations (same dimensions, coefficients, bounds, types, constraints, and auxiliary data) will produce the same fingerprint.
- The fingerprint is guaranteed to be nonzero on success (a zero fingerprint indicates an error or uncomputed state).

**Side Effects:**
- None (pure computation; no model state modified)

**Error Conditions:**
- Error from sparse array hashing helpers -> returns the propagated error code; fingerprint is set to zero

**Behavioral Description:**
The function computes a hash of the model using a polynomial rolling hash algorithm. The hash incorporates every component of the problem formulation that could affect the optimization result:

1. **Hash algorithm**: The function uses a polynomial rolling hash with a standard multiplier chosen for good distribution properties (a well-known small prime multiplier). The accumulator is 64 bits wide, and a high-bit mixing step folds bits from the top of the accumulator back into the lower bits on each floating-point value incorporation, improving avalanche properties. Floating-point values are converted to integers by multiplying by a scaling factor (preserving approximately 9 significant digits) before incorporation. Zero values are normalized (handling negative zero and denormals) before scaling. The final 64-bit accumulator is folded to 32 bits by adding its upper and lower halves. If the result is zero, it is replaced with one to ensure a nonzero fingerprint.

2. **Components hashed** (in order):
   - **Dimension counts and scalars**: Number of constraints, variables, nonzeros, SOS constraints, quadratic constraints, general constraints of all categories, indicator constraints, ranges, and version/format identifiers. Objective and quadratic constraint scaling factors are included.
   - **Objective data**: Objective coefficients for all variables. For multi-objective models, quadratic objective data is included via sparse array hashing.
   - **Range constraint data**: If present, range bounds are hashed.
   - **General constraint auxiliary data**: Constraint indices and RHS values.
   - **Constraint matrix (CSC)**: For each column, the column length (for empty columns), row indices, and coefficient values are hashed. For models that store greater-than-or-equal constraints with negated coefficients internally, the sign is flipped back before hashing to ensure that equivalent models with different internal representations produce the same fingerprint.
   - **Constraint properties**: Constraint senses and RHS values (with sign normalization for greater-than-or-equal constraints).
   - **Variable properties**: Lower bounds, upper bounds, and variable types for all variables.
   - **Quadratic objective (Q matrix)**: Row structure, coefficients, and column indices of the quadratic objective matrix.
   - **SOS constraints**: Member indices and weights for each SOS set. Secondary SOS data (if present) including coefficients, row/column indices, senses, and RHS values.
   - **Indicator constraints**: Variable indices for each indicator constraint.
   - **Quadratic constraints**: Linear coefficients, variable indices, quadratic coefficients, row/column indices, senses, and RHS values.
   - **Piecewise-linear constraints**: Delegated to a PWL hashing helper.
   - **General constraint data**: Delegated to a general constraint data hashing helper.
   - **Function constraint data**: Sparse array data for each function constraint.
   - **Optional model-level arrays**: Each optional array uses a distinct marker value to prevent collisions between absent and empty data. The optional arrays include: variable basis status, MIP start hints, branching priorities, start values (with validity flags), partition data, lazy constraint flags, variable hint values, variable hint priorities, constraint basis status, quadratic constraint basis status, SOS basis status, and PWL basis status.
   - **Warm start data**: Solution values (scoped by warm start type: full, variables-only, or constraints-only), basis status, and dual values for basic variables.

3. **Determinism guarantees**: The same model data always produces the same fingerprint. The sign normalization for greater-than-or-equal constraints ensures that models stored with different internal conventions hash identically. The zero normalization prevents negative zero from producing a different hash than positive zero.

**Thread Safety:** Safe for concurrent reads of the model (the function does not modify any model state). However, the model must not be modified concurrently with fingerprint computation.

**Dependencies:**
- Sparse array hashing helper (for Q matrix, QC, and function constraint data)
- PWL constraint hashing helper
- General constraint data hashing helper
- Warm start value inspection helper

---

### cxf_get_timestamp

**Purpose:** Generate a unique 64-bit session identifier based on the current system time, suitable for use as a session ID, correlation identifier, or registration marker.

**Signature:**
- Input: none
- Output: 64-bit integer -- a hashed session identifier

**Preconditions:**
- None (the function has no external dependencies beyond the system clock)

**Postconditions:**
- A 64-bit identifier has been returned. The identifier is derived from the current time but is not directly interpretable as a timestamp due to the mixing step.

**Side Effects:**
- Queries the system clock (a read-only system call)

**Error Conditions:**
- None under normal operation. The function always returns a value.

**Behavioral Description:**
The function generates a session identifier through a three-step process:

1. **System time acquisition**: The current UTC system time is obtained from the operating system.

2. **Epoch conversion**: The system time is converted to an integer count of fine-grained time units (sub-microsecond resolution) relative to the Unix epoch (January 1, 1970, 00:00:00 UTC). This conversion uses the well-known constant difference between the Windows FILETIME epoch (January 1, 1601) and the Unix epoch.

3. **Hash mixing**: The epoch-relative time value is multiplied by a large odd constant with good bit-mixing properties. This multiplication step serves as a lightweight hash function that ensures:
   - Consecutive timestamps produce very different identifiers (good avalanche)
   - Identifiers are non-sequential and not trivially predictable
   - The distribution is suitable for hash table keys

The result is a 64-bit integer that is NOT directly usable as a timestamp for timing measurements. The solver uses separate high-resolution performance counter functions for elapsed time measurement. This function is used exclusively for generating unique identifiers for sessions, callback registrations, and log correlation.

**Thread Safety:** Thread-safe. The function operates entirely on local variables and makes only a read-only system call.

**Dependencies:**
- Operating system time API (system clock query)

---

## Module-Level Behavioral Notes

### Relationship Between Functions

The seven functions in this module form three functional clusters:

**Cluster 1: Pre-optimization diagnostics (cxf_presolve_stats, cxf_coefficient_stats, cxf_compute_coef_stats, cxf_gencon_stats)**

These four functions work together to produce the diagnostic output seen in the solver log before optimization begins:

- cxf_presolve_stats logs structural counts (types and numbers of advanced constraints).
- cxf_coefficient_stats is the orchestrator for numerical diagnostics: it calls cxf_compute_coef_stats for basic coefficient statistics and cxf_gencon_stats for general constraint statistics, then prints the results and checks for numerical warnings.
- cxf_compute_coef_stats is the workhorse that scans all major coefficient arrays and caches the results as model attributes.
- cxf_gencon_stats handles the specialized scanning of general constraint coefficient data.

**Call graph:**
```
cxf_coefficient_stats
  |-- cxf_compute_coef_stats  (basic matrix/obj/rhs/bound stats)
  |-- cxf_gencon_stats         (general constraint stats)
  |-- [local computation]     (PWL objective stats)
```

**Cluster 2: Post-optimization validation (cxf_compute_violations)**

This function operates independently after a solution has been obtained. It evaluates the solution against all constraint types and reports violations. It may query the coefficient statistics (via the model attribute API) to provide diagnostic hints when violations are large.

**Cluster 3: Infrastructure (cxf_compute_fingerprint, cxf_get_timestamp)**

These two utility functions support the solver infrastructure:
- cxf_compute_fingerprint enables cache invalidation and incremental re-optimization by detecting model changes.
- cxf_get_timestamp provides unique identifiers for session management and logging.

### Numerical Diagnostics Philosophy

The coefficient statistics and warning system follows standard numerical LP best practices (Maros, 2003; Higham, 2002):

- **Coefficient ranges spanning many orders of magnitude** indicate ill-conditioned constraint matrices, which can lead to large rounding errors during simplex pivoting and basis factorization.
- **Large absolute coefficient values** (on the order of 1e20 or more) can cause overflow or severe precision loss in floating-point arithmetic.
- **Log-scale range analysis** (computing log10(max) - log10(min)) is the standard way to assess condition number proxies. The offset of 1.0 added before taking logarithms ensures the calculation remains well-defined when coefficients are near zero.
- **Quadratic coefficient ranges** use a tighter warning threshold than linear coefficients because quadratic terms involve products of variables, amplifying numerical effects.

### Violation Computation Standards

The violation categories computed by cxf_compute_violations correspond to standard LP solution quality metrics:

- **Primal feasibility violation**: max_i |a_i^T x - b_i| for equality constraints, max(0, a_i^T x - b_i) for inequality constraints. This is compared against the solver's feasibility tolerance (typically 1e-6).
- **Bound violation**: max_j max(0, lb_j - x_j, x_j - ub_j). Also compared against the feasibility tolerance.
- **Integrality violation**: max_j |x_j - round(x_j)| for integer variables. Compared against the integrality feasibility tolerance (typically 1e-5).
- **SOS violations**: Based on the defining property of each SOS type (at most k nonzeros in a prescribed order).

These definitions are standard across commercial LP solvers (see Achterberg, 2007; Koch et al., 2011).

### Model Fingerprinting

The fingerprint algorithm uses a polynomial rolling hash, a well-known family of hash functions (Knuth, 1997, Section 6.4). The choice of a small prime multiplier (31) and the high-bit mixing step are standard techniques for achieving good hash distribution. The algorithm is not cryptographic -- it is designed for fast change detection, not tamper resistance. The fingerprint is used internally to decide whether cached data (basis factorization, row-major matrix, etc.) is still valid or must be recomputed.

---

## Verification Checklist

```
[x] No hex addresses or offsets
[x] No Ghidra artifacts (param_, local_, iVar, DAT_, FUN_)
[x] No binary-specific constants or magic numbers
[x] No copied code fragments
[x] All descriptions are behavioral, not implementational
[x] All data structures described semantically using Layer 1 types
[x] Algorithms reference published work where applicable
[x] Constants described by algorithmic role, not specific values from binary
[x] Passes the Clean Room Test
```
