# Audit Report: Algorithm Data Structures (Basis, Pricing, Eta, Matrix)
**Auditor:** Agent A2
**Date:** 2026-02-16
**Scope:** Headers cxf_basis.h, cxf_matrix.h, cxf_pricing.h
**Specs:** data-model/basis_state.md, eta_vector.md, matrix_data.md, pricing_state.md

## Summary
- Total violations found: 76
- Critical: 38
- Major: 30
- Minor: 8

---

## Violations

### V-01: Wrong structure name for EtaVector
- **Severity:** CRITICAL
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_basis.h:53
- **Spec reference:** eta_vector.md, entire document
- **Description:** Structure is named `EtaFactors` instead of `EtaVector` as specified
- **Expected (from spec):** `EtaVector`
- **Actual (in code):** `struct EtaFactors`

### V-02: Wrong structure name for PricingState
- **Severity:** CRITICAL
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_pricing.h:20
- **Spec reference:** pricing_state.md, entire document
- **Description:** Structure is named `PricingContext` instead of `PricingState` as specified
- **Expected (from spec):** `PricingState`
- **Actual (in code):** `struct PricingContext`

### V-03: Wrong structure name for MatrixData
- **Severity:** CRITICAL
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_matrix.h:26
- **Spec reference:** matrix_data.md, entire document
- **Description:** Structure is named `SparseMatrix` instead of `MatrixData` as specified
- **Expected (from spec):** `MatrixData`
- **Actual (in code):** `struct SparseMatrix`

### V-04: Missing BasisState.numRows field
- **Severity:** CRITICAL
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_basis.h:74
- **Spec reference:** basis_state.md, Core Dimensions section
- **Description:** Field `numRows` is missing from BasisState
- **Expected (from spec):** `int numRows` - Number of rows in the basis matrix (equals the number of constraints)
- **Actual (in code):** Field `m` appears to serve this purpose, but spec requires `numRows`

### V-05: Missing BasisState.numCols field
- **Severity:** CRITICAL
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_basis.h:74
- **Spec reference:** basis_state.md, Core Dimensions section
- **Description:** Field `numCols` is missing from BasisState
- **Expected (from spec):** `int numCols` - Number of columns in the original problem
- **Actual (in code):** Field `n` appears to serve this purpose, but spec requires `numCols`

### V-06: Missing BasisState.lowerTriangular field
- **Severity:** CRITICAL
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_basis.h:74
- **Spec reference:** basis_state.md, LU Factorization Storage section
- **Description:** Field `lowerTriangular` (pointer-to-SparseTriangularMatrix) is missing
- **Expected (from spec):** `pointer-to-SparseTriangularMatrix lowerTriangular` - Lower triangular factor L from LU decomposition
- **Actual (in code):** Has `LUFactors *lu` instead, which is a different structure organization

### V-07: Missing BasisState.upperTriangular field
- **Severity:** CRITICAL
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_basis.h:74
- **Spec reference:** basis_state.md, LU Factorization Storage section
- **Description:** Field `upperTriangular` (pointer-to-SparseTriangularMatrix) is missing
- **Expected (from spec):** `pointer-to-SparseTriangularMatrix upperTriangular` - Upper triangular factor U from LU decomposition
- **Actual (in code):** Has `LUFactors *lu` instead, which is a different structure organization

### V-08: Missing BasisState.pivotOrder field
- **Severity:** CRITICAL
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_basis.h:74
- **Spec reference:** basis_state.md, LU Factorization Storage section
- **Description:** Field `pivotOrder` (array-of-int [numRows]) is missing
- **Expected (from spec):** `array-of-int [numRows] pivotOrder` - Row permutation applied during LU factorization
- **Actual (in code):** LU permutations are inside the nested `LUFactors` structure as `perm_row`

### V-09: Missing BasisState.columnOrder field
- **Severity:** CRITICAL
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_basis.h:74
- **Spec reference:** basis_state.md, LU Factorization Storage section
- **Description:** Field `columnOrder` (array-of-int [numRows]) is missing
- **Expected (from spec):** `array-of-int [numRows] columnOrder` - Column permutation applied during LU factorization
- **Actual (in code):** LU permutations are inside the nested `LUFactors` structure as `perm_col`

### V-10: Missing BasisState.memoryPool field
- **Severity:** CRITICAL
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_basis.h:74
- **Spec reference:** basis_state.md, Memory Pool section
- **Description:** Field `memoryPool` (pointer-to-MemoryPool) is missing
- **Expected (from spec):** `pointer-to-MemoryPool memoryPool` - Bump allocator for eta vector storage
- **Actual (in code):** No memory pool field present

### V-11: Missing BasisState.currentChunkOffset field
- **Severity:** MAJOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_basis.h:74
- **Spec reference:** basis_state.md, Memory Pool section
- **Description:** Field `currentChunkOffset` is missing
- **Expected (from spec):** `int currentChunkOffset` - Byte offset within current chunk for next allocation
- **Actual (in code):** No memory pool management fields present

### V-12: Missing BasisState.currentChunkCapacity field
- **Severity:** MAJOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_basis.h:74
- **Spec reference:** basis_state.md, Memory Pool section
- **Description:** Field `currentChunkCapacity` is missing
- **Expected (from spec):** `int currentChunkCapacity` - Total byte capacity of current memory chunk
- **Actual (in code):** No memory pool management fields present

### V-13: Missing BasisState.nextChunkMinSize field
- **Severity:** MAJOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_basis.h:74
- **Spec reference:** basis_state.md, Memory Pool section
- **Description:** Field `nextChunkMinSize` is missing
- **Expected (from spec):** `int nextChunkMinSize` - Minimum size for next chunk allocation
- **Actual (in code):** No memory pool management fields present

### V-14: Missing BasisState.refactorizationThreshold field
- **Severity:** CRITICAL
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_basis.h:74
- **Spec reference:** basis_state.md, Refactorization Control section
- **Description:** Field `refactorizationThreshold` is missing
- **Expected (from spec):** `int refactorizationThreshold` - Maximum number of eta vectors before refactorization
- **Actual (in code):** Has `refactor_freq` which may be similar but name doesn't match spec

### V-15: Missing BasisState.fillInEstimate field
- **Severity:** MAJOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_basis.h:74
- **Spec reference:** basis_state.md, Refactorization Control section
- **Description:** Field `fillInEstimate` is missing
- **Expected (from spec):** `int fillInEstimate` - Estimated fill-in from most recent LU factorization
- **Actual (in code):** Not present

### V-16: Missing BasisState.numericalStabilityFlag field
- **Severity:** MAJOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_basis.h:74
- **Spec reference:** basis_state.md, Refactorization Control section
- **Description:** Field `numericalStabilityFlag` is missing
- **Expected (from spec):** `bool numericalStabilityFlag` - Indicates numerical instability in recent FTRAN/BTRAN
- **Actual (in code):** Not present

### V-17: Wrong field name: BasisState.basic_vars vs basisHeader
- **Severity:** CRITICAL
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_basis.h:77
- **Spec reference:** basis_state.md, Basis Tracking Arrays section
- **Description:** Field is named `basic_vars` instead of `basisHeader`
- **Expected (from spec):** `array-of-int [numRows] basisHeader` - Maps each basis position to variable index
- **Actual (in code):** `int *basic_vars`

### V-18: Wrong field name: BasisState.var_status vs variableStatus
- **Severity:** CRITICAL
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_basis.h:78
- **Spec reference:** basis_state.md, Basis Tracking Arrays section
- **Description:** Field is named `var_status` instead of `variableStatus`
- **Expected (from spec):** `array-of-int [numCols] variableStatus` - Status of each variable
- **Actual (in code):** `int *var_status`

### V-19: Missing BasisState.progressSnapshot field
- **Severity:** MAJOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_basis.h:74
- **Spec reference:** basis_state.md, Progress Tracking Snapshot section
- **Description:** Field `progressSnapshot` (array-of-int) is missing
- **Expected (from spec):** `array-of-int [SNAPSHOT_SIZE] progressSnapshot` - Buffer holding iteration counter snapshot
- **Actual (in code):** Not present

### V-20: Missing BasisState.snapshotSize field
- **Severity:** MINOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_basis.h:74
- **Spec reference:** basis_state.md, Progress Tracking Snapshot section
- **Description:** Field `snapshotSize` is missing
- **Expected (from spec):** `int snapshotSize` - Number of counters in progress snapshot
- **Actual (in code):** Not present

### V-21: Extra field: BasisState.diag_coeff not in spec
- **Severity:** MAJOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_basis.h:84
- **Spec reference:** basis_state.md (not mentioned)
- **Description:** Field `diag_coeff` is present but not in spec
- **Expected (from spec):** Not specified
- **Actual (in code):** `double *diag_coeff` - Initial basis diagonal [m] (±1 values)

### V-22: Extra field: BasisState.pivots_since_refactor not in spec
- **Severity:** MINOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_basis.h:99
- **Spec reference:** basis_state.md (not mentioned)
- **Description:** Field `pivots_since_refactor` is present but not in spec
- **Expected (from spec):** Not specified
- **Actual (in code):** `int pivots_since_refactor`

### V-23: Extra structure: LUFactors not in spec
- **Severity:** CRITICAL
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_basis.h:24
- **Spec reference:** basis_state.md
- **Description:** Structure `LUFactors` exists but is not specified. Spec expects L and U to be stored directly in BasisState as SparseTriangularMatrix pointers
- **Expected (from spec):** lowerTriangular and upperTriangular as separate pointers in BasisState
- **Actual (in code):** Separate `LUFactors` structure containing all LU data

### V-24: Missing EtaVector variant type system
- **Severity:** CRITICAL
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_basis.h:53
- **Spec reference:** eta_vector.md, Eta Vector Variants section
- **Description:** Spec defines three distinct eta vector variants (PIVOT, VARIABLE_FIX, WARM_START), but implementation has only one structure
- **Expected (from spec):** Three variants with different field layouts and a type tag
- **Actual (in code):** Single `EtaFactors` structure with fixed fields

### V-25: Missing EtaVector.selfPtr field
- **Severity:** MAJOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_basis.h:53
- **Spec reference:** eta_vector.md, Variant 1 Fixed Fields
- **Description:** Field `selfPtr` (pointer-to-int) is missing
- **Expected (from spec):** `pointer-to-int selfPtr` - Self-referencing pointer to inline pivot metadata area
- **Actual (in code):** Not present

### V-26: Wrong EtaVector field names and organization
- **Severity:** CRITICAL
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_basis.h:53-64
- **Spec reference:** eta_vector.md, Variant 1: Pivot Transformation
- **Description:** Multiple field name mismatches for pivot variant
- **Expected (from spec):** `pivotRow`, `enteringVar`, `leavingVar`, `pivotElement`, `reducedCost`, `rowNonzeroCount`, `rowIndices`, `rowValues`, `direction`
- **Actual (in code):** Has `pivot_row`, `pivot_var`, `nnz`, `indices`, `values`, `pivot_elem`, `obj_coeff`, `status`

### V-27: Missing EtaVector.enteringVar field
- **Severity:** CRITICAL
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_basis.h:53
- **Spec reference:** eta_vector.md, Variant 1 Fixed Fields
- **Description:** Field `enteringVar` is missing
- **Expected (from spec):** `int enteringVar` - Column index of variable that entered the basis
- **Actual (in code):** Has `pivot_var` but unclear if this is entering or leaving

### V-28: Missing EtaVector.leavingVar field
- **Severity:** CRITICAL
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_basis.h:53
- **Spec reference:** eta_vector.md, Variant 1 Fixed Fields
- **Description:** Field `leavingVar` is missing
- **Expected (from spec):** `int leavingVar` - Column index of variable that left the basis
- **Actual (in code):** Not present

### V-29: Missing EtaVector.reducedCost field
- **Severity:** MAJOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_basis.h:53
- **Spec reference:** eta_vector.md, Variant 1 Fixed Fields
- **Description:** Field `reducedCost` is missing
- **Expected (from spec):** `double reducedCost` - The reduced cost of entering variable at time of pivot
- **Actual (in code):** Has `obj_coeff` which is not the same as reduced cost

### V-30: Missing EtaVector.direction field
- **Severity:** MAJOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_basis.h:53
- **Spec reference:** eta_vector.md, Variant 1 Fixed Fields
- **Description:** Field `direction` is missing
- **Expected (from spec):** `int direction` - Encodes whether entering variable moved from lower or upper bound
- **Actual (in code):** Not present

### V-31: Missing EtaVector.colIndices field
- **Severity:** MAJOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_basis.h:53
- **Spec reference:** eta_vector.md, Optional Column Data Fields
- **Description:** Optional field `colIndices` for dual simplex is missing
- **Expected (from spec):** `pointer-to-array-of-int colIndices` - Row indices of nonzero entries in eta column
- **Actual (in code):** Not present

### V-32: Missing EtaVector.colValues field
- **Severity:** MAJOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_basis.h:53
- **Spec reference:** eta_vector.md, Optional Column Data Fields
- **Description:** Optional field `colValues` for dual simplex is missing
- **Expected (from spec):** `pointer-to-array-of-double colValues` - Raw coefficients from entering variable's column
- **Actual (in code):** Not present

### V-33: Missing EtaVector VARIABLE_FIX variant
- **Severity:** CRITICAL
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_basis.h:53
- **Spec reference:** eta_vector.md, Variant 2: Variable Fixing
- **Description:** Entire VARIABLE_FIX variant is missing
- **Expected (from spec):** Variant with fields: variableIndex, fixedValue, previousReducedCost, boundStatus, nonzeroCount, columnRowIndices, columnCoefficients
- **Actual (in code):** Not present

### V-34: Missing EtaVector WARM_START variant
- **Severity:** CRITICAL
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_basis.h:53
- **Spec reference:** eta_vector.md, Variant 3: Quadratic Warm-Start
- **Description:** Entire WARM_START variant is missing
- **Expected (from spec):** Variant with fields: variableIndex, boundStatus, entryCount, qIndices, qValues
- **Actual (in code):** Not present

### V-35: Extra field: EtaFactors.obj_coeff not in spec
- **Severity:** MAJOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_basis.h:61
- **Spec reference:** eta_vector.md
- **Description:** Field `obj_coeff` is present but not in spec
- **Expected (from spec):** `reducedCost` for the entering variable
- **Actual (in code):** `double obj_coeff` - Objective coefficient of pivot_var

### V-36: Extra field: EtaFactors.status not in spec
- **Severity:** MAJOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_basis.h:62
- **Spec reference:** eta_vector.md
- **Description:** Field `status` is present but not in spec
- **Expected (from spec):** Not specified
- **Actual (in code):** `int status` - New status of pivot_var

### V-37: Missing MatrixData.numConstrs field
- **Severity:** CRITICAL
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_matrix.h:26
- **Spec reference:** matrix_data.md, Dimensions section
- **Description:** Field `numConstrs` is missing
- **Expected (from spec):** `int numConstrs` - Number of linear constraints (rows) in the matrix
- **Actual (in code):** Has `num_rows` instead

### V-38: Missing MatrixData.numVars field
- **Severity:** CRITICAL
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_matrix.h:26
- **Spec reference:** matrix_data.md, Dimensions section
- **Description:** Field `numVars` is missing
- **Expected (from spec):** `int numVars` - Number of decision variables (columns) in the matrix
- **Actual (in code):** Has `num_cols` instead

### V-39: Missing MatrixData.numNonzeros field
- **Severity:** CRITICAL
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_matrix.h:26
- **Spec reference:** matrix_data.md, Dimensions section
- **Description:** Field `numNonzeros` is missing
- **Expected (from spec):** `int64 numNonzeros` - Total number of nonzero entries
- **Actual (in code):** Has `nnz` instead

### V-40: Wrong field name: SparseMatrix.col_ptr vs colStart
- **Severity:** CRITICAL
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_matrix.h:33
- **Spec reference:** matrix_data.md, Sparse Column Storage (CSC)
- **Description:** Field is named `col_ptr` instead of `colStart`
- **Expected (from spec):** `array-of-int64 colStart` - Start index in rowIndices/coeffValues for each column
- **Actual (in code):** `int64_t *col_ptr`

### V-41: Missing MatrixData.colLen field
- **Severity:** MAJOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_matrix.h:26
- **Spec reference:** matrix_data.md, Sparse Column Storage (CSC)
- **Description:** Field `colLen` is missing
- **Expected (from spec):** `array-of-int32 colLen` - Number of nonzero entries in each column
- **Actual (in code):** Not present (must be computed from col_ptr differences)

### V-42: Wrong field name: SparseMatrix.row_idx vs rowIndices
- **Severity:** CRITICAL
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_matrix.h:34
- **Spec reference:** matrix_data.md, Sparse Column Storage (CSC)
- **Description:** Field is named `row_idx` instead of `rowIndices`
- **Expected (from spec):** `array-of-int32 rowIndices` - Row index for each nonzero entry
- **Actual (in code):** `int *row_idx`

### V-43: Wrong field name: SparseMatrix.values vs coeffValues
- **Severity:** CRITICAL
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_matrix.h:35
- **Spec reference:** matrix_data.md, Sparse Column Storage (CSC)
- **Description:** Field is named `values` instead of `coeffValues`
- **Expected (from spec):** `array-of-double coeffValues` - Coefficient value for each nonzero entry
- **Actual (in code):** `double *values`

### V-44: Wrong field name: SparseMatrix.row_ptr vs rowStart
- **Severity:** CRITICAL
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_matrix.h:38
- **Spec reference:** matrix_data.md, Sparse Row Storage (CSR)
- **Description:** Field is named `row_ptr` instead of `rowStart`
- **Expected (from spec):** `array-of-int64 rowStart` - Start index in rowColIndices/rowCoeffValues for each row
- **Actual (in code):** `int64_t *row_ptr`

### V-45: Missing MatrixData.rowEnd field
- **Severity:** MAJOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_matrix.h:26
- **Spec reference:** matrix_data.md, Sparse Row Storage (CSR)
- **Description:** Field `rowEnd` is missing
- **Expected (from spec):** `array-of-int64 rowEnd` - End index (exclusive) for each row's entries
- **Actual (in code):** Not present

### V-46: Missing MatrixData.rowStartCopy field
- **Severity:** MAJOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_matrix.h:26
- **Spec reference:** matrix_data.md, Sparse Row Storage (CSR)
- **Description:** Field `rowStartCopy` is missing
- **Expected (from spec):** `array-of-int64 rowStartCopy` - Working copy of rowStart during CSR fill phase
- **Actual (in code):** Not present

### V-47: Wrong field name: SparseMatrix.col_idx vs rowColIndices
- **Severity:** CRITICAL
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_matrix.h:39
- **Spec reference:** matrix_data.md, Sparse Row Storage (CSR)
- **Description:** Field is named `col_idx` instead of `rowColIndices`
- **Expected (from spec):** `array-of-int32 rowColIndices` - Column index for each nonzero entry (row-major order)
- **Actual (in code):** `int *col_idx`

### V-48: Wrong field name: SparseMatrix.row_values vs rowCoeffValues
- **Severity:** CRITICAL
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_matrix.h:40
- **Spec reference:** matrix_data.md, Sparse Row Storage (CSR)
- **Description:** Field is named `row_values` instead of `rowCoeffValues`
- **Expected (from spec):** `array-of-double rowCoeffValues` - Coefficient value for each nonzero entry (row-major order)
- **Actual (in code):** `double *row_values`

### V-49: Missing MatrixData.rowMajorReady field
- **Severity:** MAJOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_matrix.h:26
- **Spec reference:** matrix_data.md, Row-Major Ready Flag
- **Description:** Field `rowMajorReady` is missing
- **Expected (from spec):** `int rowMajorReady` - Indicates whether row-major (CSR) data has been constructed
- **Actual (in code):** Not present

### V-50: Missing MatrixData.objCoeffs field
- **Severity:** CRITICAL
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_matrix.h:26
- **Spec reference:** matrix_data.md, Objective Function section
- **Description:** Field `objCoeffs` (array-of-double) is missing
- **Expected (from spec):** `array-of-double objCoeffs` - Objective function coefficient for each variable
- **Actual (in code):** Not present

### V-51: Missing MatrixData.objOffset field
- **Severity:** MAJOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_matrix.h:26
- **Spec reference:** matrix_data.md, Objective Function section
- **Description:** Field `objOffset` is missing
- **Expected (from spec):** `double objOffset` - Constant term added to objective function value
- **Actual (in code):** Not present

### V-52: SparseMatrix.rhs present but should be in MatrixData
- **Severity:** MINOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_matrix.h:43
- **Spec reference:** matrix_data.md, Constraint Right-Hand Sides
- **Description:** RHS field is in the matrix structure (correct) but structure is misnamed
- **Expected (from spec):** In MatrixData structure
- **Actual (in code):** In SparseMatrix structure (which should be MatrixData)

### V-53: SparseMatrix.sense present but should be in MatrixData
- **Severity:** MINOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_matrix.h:44
- **Spec reference:** matrix_data.md, Constraint Senses
- **Description:** Sense field is in the matrix structure (correct) but structure is misnamed
- **Expected (from spec):** In MatrixData structure
- **Actual (in code):** In SparseMatrix structure (which should be MatrixData)

### V-54: Missing MatrixData.lb field
- **Severity:** CRITICAL
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_matrix.h:26
- **Spec reference:** matrix_data.md, Variable Bounds section
- **Description:** Field `lb` (array-of-double) is missing
- **Expected (from spec):** `array-of-double lb` - Lower bound for each variable
- **Actual (in code):** Not present in SparseMatrix

### V-55: Missing MatrixData.ub field
- **Severity:** CRITICAL
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_matrix.h:26
- **Spec reference:** matrix_data.md, Variable Bounds section
- **Description:** Field `ub` (array-of-double) is missing
- **Expected (from spec):** `array-of-double ub` - Upper bound for each variable
- **Actual (in code):** Not present in SparseMatrix

### V-56: Missing MatrixData.vtype field
- **Severity:** CRITICAL
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_matrix.h:26
- **Spec reference:** matrix_data.md, Variable Types section
- **Description:** Field `vtype` (array-of-char) is missing
- **Expected (from spec):** `array-of-char vtype` - Type code for each variable
- **Actual (in code):** Not present

### V-57: Missing MatrixData.modelName field
- **Severity:** MINOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_matrix.h:26
- **Spec reference:** matrix_data.md, Names section
- **Description:** Field `modelName` is missing
- **Expected (from spec):** `pointer-to-char modelName` - Name string for the model
- **Actual (in code):** Not present

### V-58: Missing MatrixData.varNames field
- **Severity:** MINOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_matrix.h:26
- **Spec reference:** matrix_data.md, Names section
- **Description:** Field `varNames` is missing
- **Expected (from spec):** `array-of-pointer-to-char varNames` - Name string for each variable
- **Actual (in code):** Not present

### V-59: Missing all MatrixData scaling fields
- **Severity:** CRITICAL
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_matrix.h:26
- **Spec reference:** matrix_data.md, Scaling System section
- **Description:** All scaling fields are missing: rowScaleFactors, colScaleFactors, globalObjScale, scaleMode, scaleParam, savedScaling, scalingDimension
- **Expected (from spec):** Seven fields for scaling system
- **Actual (in code):** None present

### V-60: Missing all MatrixData special constraint type fields
- **Severity:** MAJOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_matrix.h:26
- **Spec reference:** matrix_data.md, Special Constraint Types sections
- **Description:** All special constraint fields are missing: quadratic constraints, PWL, SOS, general constraints, etc.
- **Expected (from spec):** Numerous fields for QC, PWL, SOS, general constraints
- **Actual (in code):** None present

### V-61: Missing PricingState.levelActive field
- **Severity:** MAJOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_pricing.h:20
- **Spec reference:** pricing_state.md, Level Management section
- **Description:** Field `levelActive` (array-of-bool [MAX_LEVELS]) is missing
- **Expected (from spec):** `array-of-bool [MAX_LEVELS] levelActive` - Per-level flag for phase activation
- **Actual (in code):** Not present

### V-62: Wrong field name: PricingContext.num_vars vs context dimension
- **Severity:** MAJOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_pricing.h:25
- **Spec reference:** pricing_state.md, Constraint Queue System
- **Description:** Field is named `num_vars` but spec expects separate problem dimension tracking
- **Expected (from spec):** Problem dimensions tracked via SolverState reference
- **Actual (in code):** `int num_vars` stored directly

### V-63: Missing all PricingState constraint queue fields
- **Severity:** CRITICAL
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_pricing.h:20
- **Spec reference:** pricing_state.md, Constraint Queue System section
- **Description:** All constraint queue fields are missing: constrFlags, constrQueueCommitted, constrQueueTotal, constrQueue, cachedConstrCount (x3), constrOutputBuffer
- **Expected (from spec):** Complete constraint queue system with per-level arrays
- **Actual (in code):** No constraint queue system present

### V-64: Missing all PricingState variable queue fields
- **Severity:** CRITICAL
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_pricing.h:20
- **Spec reference:** pricing_state.md, Variable Queue System section
- **Description:** All variable queue fields are missing: varFlags, varQueueCommitted, varQueueTotal, varQueue, cachedVarCount (x3), varOutputBuffer
- **Expected (from spec):** Complete variable queue system with per-level arrays
- **Actual (in code):** No variable queue system present

### V-65: Extra field: PricingContext.candidate_counts not in spec
- **Severity:** MAJOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_pricing.h:29
- **Spec reference:** pricing_state.md
- **Description:** Field `candidate_counts` present but not in spec
- **Expected (from spec):** Separate constraint/variable queue systems with committed/total counts
- **Actual (in code):** `int *candidate_counts` - Candidates at each level [max_levels]

### V-66: Extra field: PricingContext.candidate_arrays not in spec
- **Severity:** MAJOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_pricing.h:30
- **Spec reference:** pricing_state.md
- **Description:** Field `candidate_arrays` present but not in spec structure
- **Expected (from spec):** Separate constrQueue and varQueue arrays
- **Actual (in code):** `int **candidate_arrays` - Variable indices per level [max_levels]

### V-67: Extra field: PricingContext.candidate_sizes not in spec
- **Severity:** MAJOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_pricing.h:31
- **Spec reference:** pricing_state.md
- **Description:** Field `candidate_sizes` present but not in spec
- **Expected (from spec):** Pre-allocated queue arrays sized by problem dimensions
- **Actual (in code):** `int *candidate_sizes` - Allocated size per level

### V-68: Extra field: PricingContext.weights not fully specified
- **Severity:** MINOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_pricing.h:34
- **Spec reference:** pricing_state.md
- **Description:** Field `weights` is present and matches spec description but spec doesn't formally list it
- **Expected (from spec):** Not formally specified in field tables
- **Actual (in code):** `double *weights` - SE/Devex weights

### V-69: Wrong field name: PricingContext.cached_counts vs cachedConstrCount/cachedVarCount
- **Severity:** MAJOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_pricing.h:37
- **Spec reference:** pricing_state.md, Per-Level Candidate Cache sections
- **Description:** Single `cached_counts` array instead of separate constraint/variable cache arrays
- **Expected (from spec):** Separate cachedConstrCount[MAX_LEVELS] and cachedVarCount[MAX_LEVELS] arrays (plus secondary and tertiary)
- **Actual (in code):** `int *cached_counts` - Single array for cached result count

### V-70: Missing PricingState secondary and tertiary cache fields
- **Severity:** MAJOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_pricing.h:20
- **Spec reference:** pricing_state.md, Per-Level Candidate Cache sections
- **Description:** Secondary and tertiary cache slots missing
- **Expected (from spec):** cachedConstrCount2, cachedConstrCount3, cachedVarCount2, cachedVarCount3
- **Actual (in code):** Only single cached_counts array

### V-71: Extra field: PricingContext.last_pivot_iteration not in spec
- **Severity:** MINOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_pricing.h:40
- **Spec reference:** pricing_state.md
- **Description:** Field `last_pivot_iteration` present but not in spec
- **Expected (from spec):** Not specified
- **Actual (in code):** `int last_pivot_iteration` - Iteration of last pivot

### V-72: Extra field: PricingContext.total_candidates_scanned not in spec
- **Severity:** MINOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_pricing.h:41
- **Spec reference:** pricing_state.md
- **Description:** Field `total_candidates_scanned` present but not in spec
- **Expected (from spec):** Not specified
- **Actual (in code):** `int64_t total_candidates_scanned` - Cumulative candidates evaluated

### V-73: Extra field: PricingContext.level_escalations not in spec
- **Severity:** MINOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_pricing.h:42
- **Spec reference:** pricing_state.md
- **Description:** Field `level_escalations` present but not in spec
- **Expected (from spec):** Not specified
- **Actual (in code):** `int level_escalations` - Count of level increases

### V-74: Wrong BasisSnapshot field names (camelCase vs spec)
- **Severity:** MAJOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_basis.h:109
- **Spec reference:** basis_state.md (BasisSnapshot not fully specified, inferred from BasisState)
- **Description:** BasisSnapshot uses different naming convention (numVars vs n, numConstrs vs m, basisHeader vs basic_vars)
- **Expected (from spec):** Should mirror BasisState naming (though BasisSnapshot is not fully specified)
- **Actual (in code):** Uses camelCase: numVars, numConstrs, basisHeader, varStatus

### V-75: Architecture violation: LU factors organization
- **Severity:** CRITICAL
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_basis.h:24
- **Spec reference:** basis_state.md, LU Factorization Storage
- **Description:** LU factorization organized as separate nested structure instead of direct fields in BasisState
- **Expected (from spec):** lowerTriangular, upperTriangular, pivotOrder, columnOrder as direct BasisState fields
- **Actual (in code):** All LU data grouped in separate LUFactors structure, referenced by pointer from BasisState

### V-76: Missing entire SparseTriangularMatrix type
- **Severity:** CRITICAL
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_basis.h
- **Spec reference:** basis_state.md, LU Factorization Storage
- **Description:** Spec references SparseTriangularMatrix type for L and U factors, but this type is not defined
- **Expected (from spec):** `pointer-to-SparseTriangularMatrix lowerTriangular` and `pointer-to-SparseTriangularMatrix upperTriangular`
- **Actual (in code):** LUFactors structure with inline CSC arrays for L and U

---

## Files Not Covered by Spec

None. All implementation files examined (cxf_basis.h, cxf_matrix.h, cxf_pricing.h) correspond to spec files.

---

## Spec Items Not Implemented

### From basis_state.md:
- Memory pool management (memoryPool, currentChunkOffset, currentChunkCapacity, nextChunkMinSize)
- Progress tracking snapshot (progressSnapshot, snapshotSize)
- Numerical stability tracking (numericalStabilityFlag, fillInEstimate)
- SparseTriangularMatrix type for L and U factors

### From eta_vector.md:
- Three-variant type system (PIVOT, VARIABLE_FIX, WARM_START)
- VARIABLE_FIX variant (entire variant missing)
- WARM_START variant (entire variant missing)
- Self-referencing pointer (selfPtr)
- Multiple fields for PIVOT variant (enteringVar, leavingVar, reducedCost, direction, optional column data)

### From matrix_data.md:
- Objective function data (objCoeffs, objOffset)
- Variable bounds (lb, ub)
- Variable types (vtype)
- Names (modelName, varNames)
- Complete scaling system (7 fields)
- All special constraint types (quadratic, PWL, SOS, general constraints)
- CSR auxiliary fields (rowEnd, rowStartCopy, rowMajorReady)
- CSC auxiliary field (colLen)

### From pricing_state.md:
- Complete constraint queue system (8+ fields)
- Complete variable queue system (8+ fields)
- Level activation tracking (levelActive)
- Multiple cache slots (secondary and tertiary)
- Flag-based membership tracking (constrFlags, varFlags)

---

## Severity Definitions

- **CRITICAL**: Missing/wrong core structure or field that breaks spec compliance. Requires immediate attention.
- **MAJOR**: Missing/wrong important field that impacts functionality but system may partially work.
- **MINOR**: Missing/extra field that has low impact or is optional/debugging-related.

---

## Recommendations

1. **URGENT**: Rename structures to match spec exactly:
   - `EtaFactors` → `EtaVector`
   - `PricingContext` → `PricingState`
   - `SparseMatrix` → `MatrixData`

2. **URGENT**: Rename all snake_case fields to camelCase to match spec:
   - BasisState: `basic_vars` → `basisHeader`, `var_status` → `variableStatus`
   - MatrixData: `num_rows` → `numConstrs`, `num_cols` → `numVars`, `col_ptr` → `colStart`, etc.

3. **URGENT**: Restructure LU factorization storage to match spec:
   - Remove nested `LUFactors` structure
   - Add direct fields to BasisState: `lowerTriangular`, `upperTriangular`, `pivotOrder`, `columnOrder`
   - Define `SparseTriangularMatrix` type

4. **URGENT**: Implement three-variant EtaVector system with type tags

5. **HIGH**: Add all missing core fields to BasisState (memory pool, refactorization control, progress tracking)

6. **HIGH**: Add all missing core fields to MatrixData (objective, bounds, vtype, scaling)

7. **HIGH**: Restructure PricingState with separate constraint/variable queue systems

8. **MEDIUM**: Add special constraint type support to MatrixData (QC, PWL, SOS)

9. **MEDIUM**: Document or remove extra fields not in spec (diag_coeff, obj_coeff, status, etc.)

---

## Conclusion

The implementation was built against a seriously hallucinated v1 specification. The v2 specs reveal fundamental mismatches in:
- **Naming conventions**: Implementation uses snake_case; spec uses camelCase
- **Structure names**: All three main algorithm structures are misnamed
- **Architectural organization**: LU factors nested vs flat, single eta type vs three variants, unified pricing vs dual-queue system
- **Completeness**: Major subsystems missing (memory pool, scaling, special constraints, dual queue system)

This represents a **complete rebuild requirement** for full spec compliance. The implementation captures some core concepts (basis tracking, sparse matrix storage, pricing context) but the execution deviates significantly from the v2 specification ground truth.
