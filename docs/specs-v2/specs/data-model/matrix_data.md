# MatrixData

## Purpose

MatrixData is the core constraint matrix representation for a linear or mixed-integer programming model. It stores the complete mathematical formulation: the constraint matrix A in sparse format, the objective function coefficients, variable bounds, constraint right-hand sides, constraint senses, and variable types. It also manages auxiliary representations (row-major format), scaling state, and special constraint types (quadratic, piecewise-linear, SOS, and general constraints). A model may maintain two MatrixData instances: a primary (original problem) and a working copy (modified during the solve process).

## Fields

### Dimensions

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| numConstrs | int | Number of linear constraints (rows) in the matrix | >= 0 | Must match lengths of RHS, sense arrays |
| numVars | int | Number of decision variables (columns) in the matrix | >= 0 | Must match lengths of objective, bounds, variable type arrays |
| numNonzeros | int64 | Total number of nonzero entries in the constraint matrix | >= 0 | Equals colStart[numVars] when CSC arrays are populated |

### Sparse Column Storage (CSC)

The primary internal representation uses Compressed Sparse Column (CSC) format, a standard sparse matrix storage scheme (see Saad, *Iterative Methods for Sparse Linear Systems*, 2003, Section 3.4; also known as Compressed Column Storage / CCS in the Harwell-Boeing format). This format is optimal for column-oriented operations, which dominate simplex method computations (e.g., pricing, column generation, basis updates).

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| colStart | array-of-int64, length numVars+1 | Start index in rowIndices/coeffValues for each column | Non-negative, monotonically non-decreasing | colStart[0] == 0; colStart[numVars] == numNonzeros |
| colLen | array-of-int32, length numVars | Number of nonzero entries in each column | >= 0 | colLen[j] == colStart[j+1] - colStart[j] for each column j |
| rowIndices | array-of-int32, length numNonzeros | Row index for each nonzero entry | 0 <= value < numConstrs | Entries within a single column need not be sorted |
| coeffValues | array-of-double, length numNonzeros | Coefficient value for each nonzero entry | Any finite double; zeros may exist but are logically nonzero slots | coeffValues[k] is A[rowIndices[k], j] for the column j containing index k |

**Access pattern:** For variable (column) j, the nonzero entries are at positions colStart[j] through colStart[j+1]-1 in the rowIndices and coeffValues arrays. This provides O(1) access to any column's nonzeros.

### Sparse Row Storage (CSR) -- Lazily Constructed

The row-major representation uses Compressed Sparse Row (CSR) format, the transpose analog of CSC (Saad, 2003, Section 3.4). It is built on demand when row-wise access is first requested and cached for subsequent use.

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| rowStart | array-of-int64, length numConstrs+1 | Start index in rowColIndices/rowCoeffValues for each row | Non-negative, monotonically non-decreasing | rowStart[numConstrs] == numNonzeros (when populated) |
| rowEnd | array-of-int64, length numConstrs | End index (exclusive) for each row's entries | rowStart[i] <= rowEnd[i] <= rowStart[i+1] | Used as a working pointer during CSR construction |
| rowStartCopy | array-of-int64, length numConstrs+1 | Working copy of rowStart used during CSR fill phase | Same as rowStart after construction | Preserves original rowStart values during the two-pass fill |
| rowColIndices | array-of-int32, length numNonzeros | Column index for each nonzero entry (row-major order) | 0 <= value < numVars | -- |
| rowCoeffValues | array-of-double, length numNonzeros | Coefficient value for each nonzero entry (row-major order) | Any finite double | rowCoeffValues[k] is A[i, rowColIndices[k]] for the row i containing index k |

**Access pattern:** For constraint (row) i, the nonzero entries are at positions rowStart[i] through rowEnd[i]-1 in the rowColIndices and rowCoeffValues arrays.

**Laziness:** All CSR fields are NULL until first row-wise access. The construction pipeline is:
1. Allocate CSR arrays
2. Pass 1: Count nonzeros per row from CSC data, compute prefix sum for rowStart
3. Pass 2: Fill rowColIndices and rowCoeffValues by traversing CSC columns
4. Cache the result; subsequent row-access calls reuse the cached CSR

The CSR cache is invalidated (all CSR arrays freed and set to NULL) whenever the model is modified.

### Row-Major Ready Flag

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| rowMajorReady | int | Indicates whether row-major (CSR) data has been constructed | Flag value: not-ready or ready | Set to ready after successful CSR construction; reset on model modification |

### Objective Function

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| objCoeffs | array-of-double, length numVars | Objective function coefficient for each variable | Any finite double | objCoeffs[j] is the cost coefficient c_j in min c^T x |
| objOffset | double | Constant term added to the objective function value | Any finite double | Does not affect optimization; added to reported objective |

### Constraint Right-Hand Sides

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| rhs | array-of-double, length numConstrs | Right-hand side value for each constraint | Any finite double | Stored in internal form (see Internal Storage Convention below) |

### Variable Bounds

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| lb | array-of-double, length numVars | Lower bound for each variable | Any double (negative infinity for unbounded below) | lb[j] <= ub[j] for all j |
| ub | array-of-double, length numVars | Upper bound for each variable | Any double (positive infinity for unbounded above) | ub[j] >= lb[j] for all j |

### Constraint Senses

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| sense | array-of-char, length numConstrs | Sense of each constraint as provided by the user | '<' (less-than-or-equal), '>' (greater-than-or-equal), '=' (equality) | Preserves user's original constraint direction |

### Variable Types

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| vtype | array-of-char, length numVars | Type code for each variable | 'C' (continuous), 'B' (binary), 'I' (integer), 'S' (semi-continuous), 'N' (semi-integer) | Determines whether the problem is an LP or MIP |

### Names

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| modelName | pointer-to-char | Name string for the model | NULL or valid NUL-terminated string | Optional; may be NULL |
| varNames | array-of-pointer-to-char, length numVars | Name string for each variable | Each entry NULL or valid NUL-terminated string | Optional; individual entries may be NULL |

### Scaling System

Scaling transforms the constraint matrix to improve numerical conditioning before solving. Row and column scaling factors are applied so the effective matrix is D_r * A * D_c, where D_r and D_c are diagonal scaling matrices. This is standard practice in LP solvers (see Tomlin, 1975; Curtis and Reid, 1972).

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| rowScaleFactors | array-of-double, length numConstrs | Diagonal scaling factor for each constraint row | Positive doubles | NULL when scaling is not active |
| colScaleFactors | array-of-double, length numVars | Diagonal scaling factor for each variable column | Positive doubles | NULL when scaling is not active |
| globalObjScale | double | Global multiplicative scale factor applied to the objective function | Positive double | Typically 1.0 when scaling is inactive |
| scaleMode | int | Selects which scaling algorithm to apply | Non-negative integer; multiple modes available | Controls the scaling strategy (e.g., geometric mean, equilibration) |
| scaleParam | double | Tuning parameter for the scaling algorithm | [UNDETERMINED] | Interpretation depends on scaleMode |
| savedScaling | pointer | Saved scaling state for restore after solve | NULL or valid pointer | Used to restore original scaling after modification |
| scalingDimension | int | Expected dimension for scaling factor validation | [UNDETERMINED] | Used for consistency checks during scaling operations |

### Special Constraint Types

#### Variable Type Counters

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| numBinaryVars | int | Count of binary variables in the model | >= 0 | Sum with other type counts consistent with numVars |
| numIntegerVars | int | Count of general integer variables in the model | >= 0 | -- |
| numSemiContVars | int | Count of semi-continuous variables | >= 0 | -- |
| numSemiIntVars | int | Count of semi-integer variables | >= 0 | -- |

#### Quadratic Constraints

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| numQuadConstrs | int | Number of quadratic constraints | >= 0 | Determines whether QC-related arrays are allocated |
| qcLinStart | array-of-int32 | Start indices for linear terms of each QC | Non-negative, non-decreasing | Length numQuadConstrs+1 |
| qcLinVarIdx | array-of-int32 | Variable indices for QC linear terms | 0 <= value < numVars | -- |
| qcLinCoeffs | array-of-double | Coefficients for QC linear terms | Finite doubles | -- |
| qcQuadStart | array-of-int32 | Start indices for quadratic terms of each QC | Non-negative, non-decreasing | Length numQuadConstrs+1 |
| qcQuadRow | array-of-int32 | Row variable index (i) for each Q[i,j] term | 0 <= value < numVars | -- |
| qcQuadCol | array-of-int32 | Column variable index (j) for each Q[i,j] term | 0 <= value < numVars | -- |
| qcQuadCoeffs | array-of-double | Coefficient for each Q[i,j] term | Finite doubles | -- |

#### QC Row-Major Index (Lazily Constructed)

Built alongside the linear CSR to support efficient per-variable lookup of all quadratic constraint terms involving a given variable.

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| qcRowStart | array-of-int32, length numVars+1 | Per-variable start indices into QC CSR arrays | Non-negative, non-decreasing | -- |
| qcRowConstrIdx | array-of-int32 | Constraint index for each QC CSR entry | 0 <= value < numQuadConstrs | -- |
| qcRowPairIdx | array-of-int32 | Paired variable index for each QC CSR entry | 0 <= value < numVars, or sentinel for linear terms | -- |
| qcRowCoeffs | array-of-double | Coefficient for each QC CSR entry | Finite doubles | -- |
| qcSingleStart | array-of-int32, length numVars+1 | Per-variable start for single-variable QC terms | Non-negative, non-decreasing | Only allocated when single-variable QC terms exist |
| qcSingleConstrIdx | array-of-int32 | Constraint index for single-variable QC entries | -- | -- |
| qcSinglePairIdx | array-of-int32 | Paired variable for single-variable QC entries | -- | -- |
| qcSingleCoeffs | array-of-double | Coefficient for single-variable QC entries | -- | -- |

#### Piecewise-Linear (PWL) Constraints

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| numPWL | int | Number of piecewise-linear constraints | >= 0 | -- |
| pwlStart | array-of-int64 | Start index into breakpoint arrays for each PWL constraint | Non-negative, non-decreasing | Length numPWL+1 |
| pwlBreakpointCount | array-of-int64 | Number of breakpoints per PWL constraint | >= 2 per constraint | -- |
| pwlXCoords | array-of-double | X-coordinate breakpoints | Strictly increasing within each constraint | -- |
| pwlYCoords | array-of-double | Y-coordinate breakpoints | Finite doubles | -- |
| pwlSlopes | array-of-double | Slopes for each PWL segment | Finite doubles | -- |

#### SOS Constraints

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| numSOS | int | Number of SOS (Special Ordered Set) constraints | >= 0 | -- |
| sosData | pointer | SOS constraint data (member variables, weights, types) | NULL or valid pointer | Non-NULL when numSOS > 0 |
| sosRowStart | array-of-int64 | CSR start pointers for SOS row-major index | -- | Lazily constructed alongside linear CSR |
| sosRowEnd | array-of-int64 | CSR end pointers for SOS row-major index | -- | -- |
| sosRowIndices | array-of-int32 | CSR column indices for SOS row-major index | -- | -- |
| sosRowCoeffs | array-of-double | CSR coefficients for SOS row-major index | -- | -- |

#### General Constraints

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| numGenConstrs | int | Number of general constraints (abs, min, max, indicator, etc.) | >= 0 | -- |
| numIndicators | int | Number of indicator constraints | >= 0 | -- |

### Sparse Row Mapping (for General Constraints)

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| sparseRowFlags | array-of-int32, length numGenConstrs | Activity flag for each general constraint | Flag values | NULL when no general constraints |
| sparseRowStart | array-of-int32 | Start indices for variable references per general constraint | Non-negative | -- |
| sparseRowIndices | array-of-int32 | Variable indices referenced by general constraints | 0 <= value < numVars | -- |
| varToConstrStart | array-of-int32, length numVars+1 | Per-variable start into reverse general-constraint index | Non-negative, non-decreasing | Lazily constructed |
| varToConstrIdx | array-of-int32 | General constraint indices per variable | -- | Lazily constructed |

### Swap Data (for Row-Major Conversion)

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| swapData | pointer-to-SwapData | Temporary array swap state used during lazy CSR construction | NULL when no conversion is in progress | Manages partitioning of active vs. removed constraints during row-major preparation |

The SwapData structure supports the row-major conversion pipeline by:
- Tracking which constraints are active vs. removed (e.g., by presolve)
- Partitioning each column's entries so active constraints appear first
- Temporarily swapping array pointers to allow operations on the active subset

### Status Flags

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| version | int | Internal version number for the matrix data format | Positive integer | Incremented when the internal format changes |
| modelType | int | Identifies the overall model type (LP, QP, etc.) | Non-negative integer | Derived from the combination of variable types and constraint types |
| solStatus | int | Solution status from the most recent optimization | Negative when unsolved; non-negative solver-specific status codes | Reset when model is modified |
| integrality | int | Flag indicating the model contains integer variables | 0 (continuous only) or nonzero (has integer/binary/etc.) | Nonzero if any of numBinaryVars, numIntegerVars, numSemiContVars, numSemiIntVars > 0 |
| mipSolveFlag | int | Flag set when the solver is actively processing | 0 or 1 | Set to 1 during MIP solve, cleared afterward |
| optimizeFlag | int | Controls whether optimization should proceed | 0 (do not optimize) or positive (proceed) | Also used for multi-objective state |
| forceNonConvex | int | Forces non-convex problem handling | 0 or nonzero | Overrides default convexity checks |
| numPWLObjs | int | Count of piecewise-linear objective terms | >= 0 | -- |
| numMultiScenarios | int | Count of multi-scenario configurations | >= 0 | -- |

## Relationships

- **Owned by CxfModel:** The model holds two pointers to MatrixData instances: the primary matrix (original problem as provided by the user) and a working matrix (a copy that may be modified during the solve process). The model owns both instances.
- **References CxfEnv:** MatrixData itself does not contain an environment pointer. All memory allocation and error reporting is done through the CxfEnv obtained from the owning model.
- **Referenced by solver subsystems:** The simplex solver, barrier solver, solver, and presolve routines all read from and (in the case of the working copy) write to MatrixData fields.
- **Contains SwapData (conditional):** During row-major conversion, a temporary SwapData structure is allocated and pointed to from MatrixData. This is owned by the conversion pipeline and freed after use.
- **Related to PendingBuffer:** Model modifications are batched in a PendingBuffer and applied to MatrixData in bulk. The PendingBuffer is a separate structure on the model.

## Lifecycle

### Creation

1. MatrixData is allocated and zero-initialized when a new model is created.
2. Dimensions (numConstrs, numVars, numNonzeros) are set.
3. CSC arrays (colStart, colLen, rowIndices, coeffValues) are allocated and populated.
4. Objective, RHS, bounds, sense, and vtype arrays are allocated and populated.
5. CSR arrays are left as NULL (lazy construction).
6. Scaling arrays are left as NULL (applied later if scaling is enabled).
7. The version field is set.

### Mutation

- **Model modification:** Adding/removing variables or constraints invalidates the CSR cache (all CSR pointers set to NULL) and updates the CSC arrays. Changes may be batched through PendingBuffer.
- **Scaling:** When scaling is activated, rowScaleFactors, colScaleFactors, and globalObjScale are computed and stored. The scaled values are used during the solve; original values are recoverable.
- **Working copy creation:** Before solving, a working copy of the primary MatrixData may be created. The solver modifies only the working copy, leaving the primary intact.
- **CSR construction:** On first row-wise access, the CSR arrays and all related row-major indices (QC, SOS, general constraint) are constructed from the CSC data.
- **Row-major conversion with active constraint filtering:** When constraints have been removed (e.g., by presolve), the conversion pipeline partitions each column's entries so that active constraints appear first, then builds CSR from only the active subset.

### Destruction

1. All dynamically allocated arrays are freed (CSC arrays, CSR arrays, QC arrays, SOS arrays, PWL arrays, scaling arrays, name strings).
2. SwapData is freed if present.
3. The MatrixData structure itself is freed.
4. The working copy (if any) is freed before the primary copy.

## Invariants

1. **CSC consistency:** colStart has length numVars+1, with colStart[0] == 0 and colStart[numVars] == numNonzeros. For every column j, colLen[j] == colStart[j+1] - colStart[j].
2. **Row index validity:** Every entry in rowIndices is in the range [0, numConstrs).
3. **Bounds ordering:** For every variable j, lb[j] <= ub[j].
4. **Array co-sizing:** objCoeffs, lb, ub, and vtype all have length numVars. rhs and sense both have length numConstrs.
5. **CSR cache coherence:** If any CSR array is non-NULL, all CSR arrays are non-NULL and consistent with the current CSC data. If the model has been modified since the last CSR construction, all CSR arrays must be NULL.
6. **Internal storage convention for '>=' constraints:** Constraints with sense '>' have their coefficients and RHS negated in storage so that the matrix uniformly represents '<=' constraints internally. On retrieval, the sign of coefficients for '>' constraints is flipped back. This negation uses an IEEE 754 sign-bit flip, which is exact (no floating-point rounding), handles infinity and NaN correctly, and is computationally cheaper than multiplication.
7. **Scaling factor positivity:** When scaling is active, all row and column scaling factors are strictly positive.
8. **Working copy independence:** Modifications to the working copy do not affect the primary copy.

## Thread Safety

- **Read access:** Multiple threads may read from a single MatrixData instance concurrently, provided no thread is writing.
- **Write access:** All modifications to MatrixData must be serialized. The model-level critical section (on the environment) must be held during any write operation.
- **Lazy CSR construction:** The first row-wise access triggers CSR construction, which is a write operation. Concurrent first-access calls must be serialized. After construction, the cached CSR data is safe for concurrent reads.
- **SwapData operations:** The swap data reference count is not atomically incremented, so concurrent row-major conversion operations on the same MatrixData are unsafe without external synchronization.

## Design Rationale

### CSC as Primary Format

The simplex method is column-oriented: pricing selects a column, and basis updates operate on columns. Storing the matrix in CSC format makes these operations cache-friendly and avoids transposition overhead. This is the standard choice in production LP solvers (Maros, *Computational Techniques of the Simplex Method*, 2003, Chapter 8).

### Lazy CSR Construction

Row-wise access (e.g., retrieving a constraint's coefficients, dual simplex pricing) is less frequent than column-wise access. Constructing CSR eagerly would waste memory and time for models that never need row access. The lazy approach defers the O(nnz) conversion cost to the first request and amortizes it over all subsequent row-access calls.

The two-pass CSC-to-CSR conversion algorithm is the standard approach:
1. Count nonzeros per row, then compute a prefix sum to obtain row start pointers.
2. Traverse all CSC columns, placing each entry at the appropriate position in the CSR arrays.

This runs in O(nnz) time and O(numConstrs) auxiliary space.

### Internal '<=' Normal Form

Normalizing all constraints to '<=' form simplifies the simplex algorithm, which only needs to handle one constraint direction. The sense array preserves the user's original direction for correct retrieval and reporting. The sign-bit flip technique for negation is a well-known IEEE 754 optimization that avoids floating-point multiplication.

### Dual Matrix Instances (Primary/Working)

Keeping the original problem intact while allowing the solver to modify a working copy enables:
- Warm-starting from the original after parameter changes
- Correct reporting of original problem data during callbacks
- Clean recovery if the solve is interrupted or fails

### Scaling as Optional Overlay

Scaling factors are stored separately from the matrix data, allowing the solver to work with scaled values during optimization while preserving the ability to unscale results for reporting. The scaling arrays are NULL when scaling is inactive, avoiding any overhead for unscaled solves.

### Active Constraint Partitioning

During presolve or row-major conversion, removed constraints are partitioned to the end of each column's entries using a two-pointer partition (similar to Hoare's partition scheme from quicksort). This allows the solver to operate on only the active subset without allocating a new matrix, then restore the original layout afterward by swapping array pointers back.
