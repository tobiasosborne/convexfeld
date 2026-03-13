# Spec V2 Audit: Data Model & Structures

Auditor: Claude Opus 4.6
Date: 2026-03-13
Scope: All structures in docs/specs-v2/specs/data-model/

## Files Reviewed

### Spec Files
- docs/specs-v2/specs/data-model/solver_state.md
- docs/specs-v2/specs/data-model/basis_state.md
- docs/specs-v2/specs/data-model/model.md
- docs/specs-v2/specs/data-model/matrix_data.md
- docs/specs-v2/specs/data-model/work_arrays.md
- docs/specs-v2/specs/data-model/supporting_structures.md
- docs/specs-v2/specs/data-model/eta_vector.md
- docs/specs-v2/specs/data-model/environment.md
- docs/specs-v2/specs/data-model/callback_state.md

### Implementation Files
- include/convexfeld/cxf_types.h
- include/convexfeld/cxf_solver.h
- include/convexfeld/cxf_basis.h
- include/convexfeld/cxf_model.h
- include/convexfeld/cxf_matrix.h
- include/convexfeld/cxf_env.h
- include/convexfeld/cxf_solve_state.h
- include/convexfeld/cxf_callback.h
- src/solver_state/init.c
- src/solver_state/extract.c
- src/basis/basis_state.c
- src/basis/basis_internal.h
- src/simplex/simplex_internal.h
- src/memory/internal_types.h

---

## Compliant Structures

### Variable Status Codes
Spec codes (-1=AT_LOWER, -2=AT_UPPER, -3=SUPERBASIC, -4=FIXED, >=0=BASIC) match exactly in cxf_types.h lines 160-172.

### EtaBuffer (MemoryPool)
The spec's BasisState.memoryPool is implemented as `EtaBuffer` in internal_types.h. The bump allocator fields (`bytesUsed`, `currentChunkSize`, `minChunkSize`, chunk chain) are functionally equivalent to the spec's `currentChunkOffset`, `currentChunkCapacity`, and `nextChunkMinSize`.

### VectorContainer
Implemented in internal_types.h. Not directly specified in the data-model specs but serves as internal_vectors from the Model spec.

---

## VIOLATIONS

### SolverState (solver_state.md vs cxf_solver.h)

#### [V1] SolverState.numNonzeros -- type mismatch
- Spec says: `int` type
- Code has: `int64_t num_nonzeros` (cxf_solver.h:55)
- Severity: LOW (code is arguably better; supports >2B nonzeros)

#### [V2] SolverState.numSlacks -- naming only
- Spec says: `numSlacks` determined from constraint senses
- Code has: `num_slacks` always equals `num_constrs` (comment at cxf_solver.h:102)
- Severity: LOW (naming convention difference; semantics appear correct)

#### [V3] SolverState.rowStart -- type mismatch
- Spec says: `array-of-int64` for CSR row pointers
- Code has: `int64_t *csr_row_ptr` -- matches
- NOTE: compliant

#### [V4] SolverState.rowColCount -- MISSING
- Spec says: `rowColCount` array-of-int [numConstrs] giving number of nonzeros per row
- Code has: No equivalent field in SolverState. CSR row pointers exist but no per-row count array.
- File: include/convexfeld/cxf_solver.h
- Severity: MEDIUM (can be derived from csr_row_ptr differences)

#### [V5] SolverState.colRowCount -- MISSING
- Spec says: `colRowCount` array-of-int [numVars] giving number of nonzeros per column
- Code has: No equivalent field. CSC col pointers exist but no per-column count array.
- File: include/convexfeld/cxf_solver.h
- Severity: MEDIUM (can be derived from csc_col_ptr differences)

#### [V6] SolverState.constraintRHS -- field naming
- Spec says: `constraintRHS` array-of-double [numConstrs] for RHS or dual prices
- Code has: `work_rhs` (cxf_solver.h:152) -- semantically equivalent
- Severity: INFO (naming convention difference only)

#### [V7] SolverState.constraintSense -- field naming
- Spec says: `constraintSense` array-of-char [numConstrs]
- Code has: `work_sense` (cxf_solver.h:153) -- semantically equivalent
- Severity: INFO

#### [V8] SolverState.varFlags -- MISSING
- Spec says: `varFlags` array-of-int [numVars] for per-variable type flags (semi-continuous, SOS, PWL, quadratic)
- Code has: No equivalent field in SolverState
- File: include/convexfeld/cxf_solver.h
- Severity: LOW (not needed until MIP/special variable support)

#### [V9] SolverState.steepestEdgeLB -- MISSING
- Spec says: `steepestEdgeLB` array-of-double [numVars]
- Code has: No steepest edge weight arrays
- File: include/convexfeld/cxf_solver.h
- Severity: MEDIUM (required for Devex/steepest edge pricing)

#### [V10] SolverState.steepestEdgeUB -- MISSING
- Spec says: `steepestEdgeUB` array-of-double [numVars]
- Code has: No equivalent
- Severity: MEDIUM

#### [V11] SolverState.dualSteepestLB -- MISSING
- Spec says: `dualSteepestLB` array-of-double [numConstrs]
- Code has: No equivalent
- Severity: MEDIUM (dual simplex not yet implemented)

#### [V12] SolverState.dualSteepestUB -- MISSING
- Spec says: `dualSteepestUB` array-of-double [numConstrs]
- Code has: No equivalent
- Severity: MEDIUM

#### [V13] SolverState.etaPoolMode -- MISSING
- Spec says: `etaPoolMode` int controlling full vs simplified eta tracking
- Code has: No equivalent field
- File: include/convexfeld/cxf_solver.h
- Severity: LOW

#### [V14] SolverState.etaListHead -- MISSING (convenience alias)
- Spec says: SolverState should have `etaListHead` as convenience alias of BasisState.etaListHead
- Code has: No such alias. Code accesses `state->basis->eta_head` instead.
- File: include/convexfeld/cxf_solver.h
- Severity: LOW (functional through basis pointer)

#### [V15] SolverState.etaRowCount -- MISSING (convenience alias)
- Spec says: `etaRowCount` convenience alias of BasisState.etaRowCount
- Code has: No alias. `eta_count` on SolverState is a separate field, not synced.
- File: include/convexfeld/cxf_solver.h:83
- Severity: LOW

#### [V16] SolverState.etaTotalCount -- MISSING (convenience alias)
- Spec says: `etaTotalCount` convenience alias of BasisState.etaTotalCount
- Code has: `eta_count` on SolverState (line 83) but this is not synced with BasisState.eta_count
- Severity: MEDIUM (spec requires both to be in sync)

#### [V17] SolverState.pricingState -- naming only
- Spec says: `pricingState` pointer-to-PricingState
- Code has: `pricing` pointer (cxf_solver.h:75)
- Severity: INFO

#### [V18] SolverState.memoryAllocator -- MISSING
- Spec says: `memoryAllocator` pointer-to-Allocator for temporary buffers
- Code has: No allocator pointer on SolverState (eta pool is on BasisState)
- Severity: LOW

#### [V19] SolverState.workSize1/workSize2/workSize3 -- MISSING
- Spec says: Three int64 work array dimension fields
- Code has: No equivalent fields
- File: include/convexfeld/cxf_solver.h
- Severity: LOW (work arrays are sized implicitly)

#### [V20] SolverState.matrixStatus -- MISSING
- Spec says: `matrixStatus` int copy of matrix solution status
- Code has: No equivalent
- Severity: LOW

#### [V21] SolverState.problemVarIndex -- naming mismatch
- Spec says: `problemVarIndex` int for infeasibility/unboundedness diagnostics
- Code has: `problem_row_index` (line 142) -- different semantics (row vs var)
- File: include/convexfeld/cxf_solver.h:142
- Severity: MEDIUM (spec says variable index, code uses row index)

#### [V22] SolverState.timingArea -- naming only
- Spec says: `timingArea` pointer-to-TimingState
- Code has: `timing` pointer (cxf_solver.h:80)
- Severity: INFO

#### [V23] SolverState.scaleFactor -- compliant
- Spec and code both have `scale_factor` double.

#### [V24] SolverState.removedRows/removedCols -- naming mismatch
- Spec says: `removedRows`, `removedCols` (presolve stats)
- Code has: `rows_eliminated`, `cols_eliminated` (cxf_solver.h:131-132)
- Severity: INFO

#### [V25] SolverState.lastReportTime -- naming mismatch
- Spec says: `lastReportTime` double
- Code has: `last_log_time` (cxf_solver.h:90)
- Severity: INFO

#### [V26] SolverState.timingMode -- MISSING
- Spec says: `timingMode` int selecting wall-clock vs CPU time
- Code has: No equivalent
- Severity: LOW

#### [V27] SolverState.iterLimit -- MISSING
- Spec says: `iterLimit` int secondary iteration limit for sub-procedures
- Code has: Only `max_iterations` (primary). No secondary limit.
- Severity: LOW

#### [V28] SolverState.numBasic -- exists on SolverState but spec says it should be here
- Spec says: `numBasic` int on SolverState
- Code has: `num_basic` on SolverState (line 141) -- compliant
- Severity: NONE (compliant)

#### [V29] SolverState.varStatus/basisHeader -- LOCATION MISMATCH
- Spec says: SolverState owns `varStatus` [numVars] and `basisHeader` [numConstrs]
- Code has: These are on BasisState (`var_status`, `basic_vars`), NOT on SolverState
- File: include/convexfeld/cxf_basis.h:77-78
- Severity: HIGH -- Spec explicitly places these on SolverState. Implementation places them on BasisState. The spec notes BasisState also has copies (variableStatus, basisHeader), so there should be two copies in sync. Code has only one copy (on BasisState).

#### [V30] SolverState.reducedCosts -- naming only
- Spec says: `reducedCosts` array-of-double [numVars]
- Code has: `work_dj` (cxf_solver.h:71) sized [num_vars + num_constrs]
- Severity: INFO (naming); size is larger than spec (includes slacks)

#### [V31] SolverState.lbWorking/ubWorking -- naming only
- Spec says: `lbWorking` [numVars], `ubWorking` [numVars]
- Code has: `work_lb`, `work_ub` sized [num_vars + num_constrs]
- Severity: INFO (naming); size includes slack variables beyond spec

---

### BasisState (basis_state.md vs cxf_basis.h)

#### [V32] BasisState.numRows/numCols -- naming mismatch
- Spec says: `numRows`, `numCols`
- Code has: `m`, `n` (cxf_basis.h:75-76)
- Severity: INFO

#### [V33] BasisState.lowerTriangular/upperTriangular -- structural difference
- Spec says: `lowerTriangular` and `upperTriangular` as pointer-to-SparseTriangularMatrix
- Code has: `LUFactors *lu` (cxf_basis.h:87) which contains both L and U in a single struct
- File: include/convexfeld/cxf_basis.h:24-45
- Severity: LOW (functionally equivalent, just different structure nesting)

#### [V34] BasisState.pivotOrder/columnOrder -- location difference
- Spec says: `pivotOrder` [numRows] and `columnOrder` [numRows] on BasisState
- Code has: `perm_row` and `perm_col` inside LUFactors (cxf_basis.h:39-40)
- Severity: LOW (accessible via basis->lu->perm_row)

#### [V35] BasisState.basisHeader/variableStatus -- naming
- Spec says: `basisHeader` [numRows], `variableStatus` [numCols]
- Code has: `basic_vars` [m], `var_status` [n] (cxf_basis.h:77-78)
- Severity: INFO

#### [V36] BasisState.progressSnapshot -- MISSING
- Spec says: `progressSnapshot` array-of-int [SNAPSHOT_SIZE] and `snapshotSize` int on BasisState
- Code has: `progress_snapshot` is on SolverState, NOT on BasisState (cxf_solver.h:128)
- File: include/convexfeld/cxf_basis.h (absent)
- Severity: MEDIUM (spec says BasisState owns this; code puts it on SolverState)

#### [V37] BasisState.currentChunkOffset/currentChunkCapacity/nextChunkMinSize -- location difference
- Spec says: These memory pool fields are on BasisState directly
- Code has: These are inside EtaBuffer struct (internal_types.h:48-54), accessed via `basis->eta_pool`
- Severity: LOW (functionally equivalent through pointer indirection)

#### [V38] BasisState.refactorizationThreshold -- naming
- Spec says: `refactorizationThreshold` int
- Code has: `refactor_freq` (cxf_basis.h:107)
- Severity: INFO

#### [V39] BasisState.fillInEstimate -- type mismatch
- Spec says: `fillInEstimate` int
- Code has: `fill_in_estimate` int64_t (cxf_basis.h:100)
- Severity: LOW (code is wider type)

#### [V40] BasisState.numericalStabilityFlag -- type mismatch
- Spec says: `numericalStabilityFlag` bool
- Code has: `numerical_flag` int (cxf_basis.h:99)
- Severity: LOW (int used as bool, functionally equivalent in C)

#### [V41] BasisState.eta_capacity -- EXTRA FIELD (not in spec)
- Spec does not mention `eta_capacity`
- Code has: `eta_capacity` int (cxf_basis.h:91)
- Severity: INFO (extra field, not a compliance issue)

#### [V42] BasisState.diag_coeff -- EXTRA FIELD (not in spec)
- Spec does not mention `diag_coeff` array
- Code has: `diag_coeff` double* [m] (cxf_basis.h:84)
- Severity: INFO (implementation detail for initial basis diagonal)

#### [V43] BasisState.work/work2 -- EXTRA FIELDS (not in spec)
- Spec does not mention working arrays on BasisState
- Code has: `work` and `work2` double* [m] (cxf_basis.h:103-104)
- Severity: INFO (scratch space, not a compliance issue)

#### [V44] BasisState.pivots_since_refactor/iteration -- EXTRA FIELDS
- Spec does not mention these
- Code has: `pivots_since_refactor`, `iteration` (cxf_basis.h:108-109)
- Severity: INFO

---

### Model (model.md vs cxf_model.h)

#### [V45] Model.validity_sentinel -- type mismatch
- Spec says: `validity_sentinel` unsigned 32-bit integer
- Code has: `magic` uint32_t (cxf_model.h:21)
- Severity: INFO (naming only; type matches)

#### [V46] Model.secondary_sentinel -- MISSING
- Spec says: `secondary_sentinel` unsigned 64-bit integer for defense-in-depth
- Code has: No secondary sentinel
- File: include/convexfeld/cxf_model.h
- Severity: MEDIUM (reduced corruption detection)

#### [V47] Model.status_code -- naming
- Spec says: `status_code` int
- Code has: `status` int (cxf_model.h:42)
- Severity: INFO

#### [V48] Model.optimize_in_progress -- MISSING
- Spec says: `optimize_in_progress` int transient flag
- Code has: No equivalent field. `modification_blocked` serves partial role.
- File: include/convexfeld/cxf_model.h
- Severity: LOW

#### [V49] Model.environment_owned -- MISSING
- Spec says: `environment_owned` int indicating if model owns its environment
- Code has: `env_flag` (cxf_model.h:64) -- possibly equivalent but not clearly documented
- Severity: MEDIUM (unclear if env_flag serves same purpose)

#### [V50] Model.concurrent_environments/concurrent_environment_count -- MISSING
- Spec says: pointer-to-array-of-Environment-pointers and count
- Code has: No equivalent
- Severity: LOW (concurrent optimization not implemented)

#### [V51] Model.self_reference -- naming
- Spec says: `self_reference` pointer-to-Model
- Code has: `self_ptr` (cxf_model.h:59)
- Severity: INFO

#### [V52] Model.compute_server_mode -- MISSING
- Spec says: `compute_server_mode` int
- Code has: No equivalent
- Severity: LOW (compute server not implemented)

#### [V53] Model.primary_matrix/working_matrix -- MISSING
- Spec says: `primary_matrix` and `working_matrix` pointer-to-MatrixData (two separate matrix instances)
- Code has: Only `matrix` pointer (cxf_model.h:37). No primary/working separation.
- File: include/convexfeld/cxf_model.h:37
- Severity: HIGH -- Spec requires preserving original problem matrix separate from working copy. Code has only one matrix pointer. The SolverState does copy matrix data into its own CSC/CSR arrays, so the model matrix is not directly modified, but the spec's two-matrix architecture is not implemented at the Model level.

#### [V54] Model.attribute_table -- MISSING
- Spec says: `attribute_table` pointer-to-AttributeTable
- Code has: No attribute table system
- File: include/convexfeld/cxf_model.h
- Severity: MEDIUM (attributes are handled ad-hoc via cxf_getintattr/cxf_getdblattr)

#### [V55] Model.internal_vectors -- MISSING
- Spec says: `internal_vectors` array-of-pointers for scratch space
- Code has: No equivalent
- Severity: LOW

#### [V56] Model.history -- MISSING
- Spec says: `history` pointer-to-HistoryData
- Code has: No equivalent
- Severity: LOW (history tracking not implemented)

#### [V57] Model.name -- type difference
- Spec says: implied pointer or allocated string for model name
- Code has: `name[CXF_MAX_NAME_LEN + 1]` fixed-size char array (cxf_model.h:23)
- Severity: LOW (fixed array vs allocated string; functional for names up to 255 chars)

#### [V58] Model -- missing many fields
- Spec has many fields not in code: `var_capacity` is extra (not in spec).
- Spec fields `num_vars`, `num_constrs` on Model are duplicating matrix dimensions.
- Code has `num_vars`, `num_constrs`, and `var_capacity` on Model directly (cxf_model.h:26-28).
- Severity: INFO (var_capacity is an implementation optimization)

---

### MatrixData (matrix_data.md vs cxf_matrix.h)

#### [V59] MatrixData.numConstrs/numVars -- naming
- Spec says: `numConstrs`, `numVars`
- Code has: `num_rows`, `num_cols` (cxf_matrix.h:28-29)
- Severity: INFO

#### [V60] MatrixData.numNonzeros -- type match
- Spec says: `numNonzeros` int64
- Code has: `nnz` int64_t (cxf_matrix.h:30)
- Severity: INFO (naming only)

#### [V61] MatrixData.colLen -- MISSING
- Spec says: `colLen` array-of-int32 [numVars] giving nonzero count per column
- Code has: No `colLen` array. Only `col_ptr` from which lengths are derivable.
- File: include/convexfeld/cxf_matrix.h
- Severity: LOW (derivable from col_ptr[j+1] - col_ptr[j])

#### [V62] MatrixData.rowEnd -- MISSING
- Spec says: `rowEnd` array-of-int64 [numConstrs] for CSR end pointers
- Code has: No `rowEnd`. Only `row_ptr` is present.
- Severity: LOW (derivable from row_ptr[i+1])

#### [V63] MatrixData.rowStartCopy -- MISSING
- Spec says: `rowStartCopy` array-of-int64 [numConstrs+1] for CSR construction working copy
- Code has: No equivalent
- Severity: LOW (construction detail)

#### [V64] MatrixData.rowMajorReady -- MISSING
- Spec says: `rowMajorReady` int flag
- Code has: No equivalent flag. Laziness inferred from NULL pointers.
- Severity: LOW (NULL check serves same purpose)

#### [V65] MatrixData.objCoeffs -- MISSING from MatrixData
- Spec says: MatrixData should have `objCoeffs` array-of-double [numVars]
- Code has: `obj_coeffs` on CxfModel (cxf_model.h:31), NOT on MatrixData
- File: include/convexfeld/cxf_matrix.h
- Severity: MEDIUM -- Spec locates objective in MatrixData; code locates it on Model

#### [V66] MatrixData.objOffset -- MISSING
- Spec says: `objOffset` double constant term
- Code has: No equivalent anywhere
- Severity: MEDIUM (objective offset not supported)

#### [V67] MatrixData.lb/ub -- MISSING from MatrixData
- Spec says: MatrixData should have `lb` and `ub` arrays
- Code has: `lb` and `ub` on CxfModel (cxf_model.h:33-34), NOT on MatrixData
- Severity: MEDIUM -- Spec locates bounds in MatrixData; code locates them on Model

#### [V68] MatrixData.sense -- naming only
- Spec says: `sense` array-of-char
- Code has: `sense` (cxf_matrix.h:44) -- matches

#### [V69] MatrixData.vtype -- MISSING from MatrixData
- Spec says: MatrixData should have `vtype` array-of-char [numVars]
- Code has: `vtype` on CxfModel (cxf_model.h:34), NOT on MatrixData
- Severity: MEDIUM

#### [V70] MatrixData.modelName/varNames -- MISSING from MatrixData
- Spec says: `modelName` and `varNames` on MatrixData
- Code has: `name` on CxfModel. No varNames anywhere.
- Severity: LOW

#### [V71] MatrixData.rowScaleFactors/colScaleFactors -- MISSING from MatrixData
- Spec says: Scaling arrays on MatrixData
- Code has: `row_scale`/`col_scale` on SolverState (cxf_solver.h:157-158)
- Severity: LOW (different location, same functionality)

#### [V72] MatrixData.globalObjScale/scaleMode/scaleParam/savedScaling/scalingDimension -- MISSING
- Spec says: Multiple scaling fields on MatrixData
- Code has: None of these
- Severity: LOW

#### [V73] MatrixData.numBinaryVars/numIntegerVars/numSemiContVars/numSemiIntVars -- MISSING
- Spec says: Variable type counters on MatrixData
- Code has: No equivalent counters
- Severity: LOW

#### [V74] MatrixData.version/modelType/solStatus/integrality/mipSolveFlag/optimizeFlag/forceNonConvex/numPWLObjs/numMultiScenarios -- MISSING
- Spec says: Multiple status flags on MatrixData
- Code has: None of these on MatrixData
- Severity: LOW (many are MIP-related)

#### [V75] MatrixData -- all quadratic/PWL/SOS/general constraint fields MISSING
- Spec describes extensive quadratic, PWL, SOS, general constraint data on MatrixData
- Code has: None of these (pure LP solver)
- Severity: LOW (not needed for current LP-only scope)

#### [V76] MatrixData.swapData -- MISSING
- Spec says: `swapData` pointer-to-SwapData for row-major conversion
- Code has: No equivalent
- Severity: LOW

#### [V77] MatrixData.range_values -- EXTRA FIELD
- Spec does not mention `range_values` on MatrixData
- Code has: `range_values` double* [num_rows] (cxf_matrix.h:45)
- Severity: INFO (MPS RANGES extension, not in spec)

---

### WorkArrays / SolutionData (work_arrays.md vs cxf_solver.h SolutionData)

#### [V78] WorkArrays -- renamed to SolutionData
- Spec says: Structure is called `WorkArrays`
- Code has: Structure called `SolutionData` (cxf_solver.h:21)
- Severity: INFO (naming)

#### [V79] WorkArrays.primalValues -- MISSING
- Spec says: `primalValues` pointer-to-array-of-double wired to "X" attribute
- Code has: No primalValues in SolutionData. Solution stored on Model (`model->solution`).
- Severity: MEDIUM

#### [V80] WorkArrays.dualValues -- MISSING
- Spec says: `dualValues` pointer-to-array-of-double
- Code has: No dualValues in SolutionData. Duals stored on Model (`model->pi`).
- Severity: MEDIUM

#### [V81] WorkArrays.objectiveValue -- naming
- Spec says: `objectiveValue` double
- Code has: `best_obj` (cxf_solver.h:29)
- Severity: INFO

#### [V82] WorkArrays.objectiveBound/objectiveBoundContinuous/poolObjectiveBound/mipGap -- MISSING
- Spec says: Multiple objective bound fields
- Code has: Only `obj_bound` (cxf_solver.h:30)
- Severity: LOW (MIP features)

#### [V83] WorkArrays.nodeCount/openNodeCount/timeOpen -- MISSING
- Spec says: MIP search tree counters
- Code has: None
- Severity: LOW (MIP features)

#### [V84] WorkArrays.solveMode -- MISSING
- Spec says: `solveMode` int on WorkArrays
- Code has: No solveMode in SolutionData
- Severity: LOW

#### [V85] WorkArrays.solutionCount -- MISSING
- Spec says: `solutionCount` int
- Code has: No equivalent
- Severity: LOW

#### [V86] WorkArrays.iterationCount -- type mismatch
- Spec says: `iterationCount` double
- Code has: `total_iterations` int (cxf_solver.h:23)
- Severity: LOW (int vs double; double supports very large iteration counts)

#### [V87] WorkArrays.barrierIterationCount/pdhgIterationCount -- MISSING
- Spec says: Barrier and PDHG iteration counters
- Code has: None
- Severity: LOW

#### [V88] WorkArrays.scaledTolerance/scaleFactor1/scaleFactor2/baseTolerance -- MISSING
- Spec says: Multiple tolerance/scale fields
- Code has: None in SolutionData
- Severity: LOW

#### [V89] WorkArrays.activeFlag -- MISSING
- Spec says: `activeFlag` int
- Code has: No equivalent
- Severity: LOW

#### [V90] WorkArrays.poolSolutionCount/poolVariableValues/poolObjectiveValues/poolObjectiveBounds -- MISSING
- Spec says: Solution pool fields
- Code has: None
- Severity: LOW (solution pool not implemented)

#### [V91] WorkArrays.cutCount/cutVariableValues/cutObjectiveValues -- MISSING
- Spec says: Cut data fields
- Code has: None
- Severity: LOW

#### [V92] WorkArrays.auxiliaryIndices -- MISSING
- Spec says: `auxiliaryIndices` array-of-int [3]
- Code has: No equivalent
- Severity: LOW

---

### EtaVector (eta_vector.md vs cxf_basis.h)

#### [V93] EtaVector -- single variant instead of three
- Spec says: Three distinct variants: PIVOT (type=PIVOT), VARIABLE_FIX, WARM_START
- Code has: Single EtaVector struct with `type` field: 1=refactorization, 2=pivot (cxf_basis.h:54)
- File: include/convexfeld/cxf_basis.h:53-64
- Severity: HIGH -- Spec defines three eta variants with different field layouts. Code has one struct with type values 1 (refactorization, not in spec) and 2 (pivot). VARIABLE_FIX and WARM_START variants are not implemented.

#### [V94] EtaVector.type -- value mismatch
- Spec says: PIVOT constant for pivot transformations
- Code has: `type` with values 1=refactorization, 2=pivot (cxf_basis.h:54)
- Severity: MEDIUM (type=1 "refactorization" is not in the spec at all)

#### [V95] EtaVector.selfPtr -- MISSING
- Spec says: `selfPtr` pointer-to-int self-referencing pointer to inline metadata
- Code has: No self-referencing pointer
- Severity: MEDIUM

#### [V96] EtaVector.enteringVar -- MISSING (for pivot variant)
- Spec says: Pivot variant has `enteringVar` int
- Code has: `pivot_var` (cxf_basis.h:56) but this is single field, not entering+leaving pair
- Severity: MEDIUM -- Spec has separate `enteringVar` and `leavingVar`. Code has only `pivot_var`.

#### [V97] EtaVector.leavingVar -- MISSING
- Spec says: `leavingVar` int for pivot variant
- Code has: No equivalent (only `pivot_var`)
- Severity: MEDIUM

#### [V98] EtaVector.pivotElement -- naming
- Spec says: `pivotElement` double
- Code has: `pivot_elem` (cxf_basis.h:60)
- Severity: INFO

#### [V99] EtaVector.reducedCost -- naming
- Spec says: `reducedCost` double snapshot
- Code has: `obj_coeff` (cxf_basis.h:61) -- different semantics (objective coefficient vs reduced cost)
- Severity: LOW

#### [V100] EtaVector.rowNonzeroCount -- naming
- Spec says: `rowNonzeroCount` int
- Code has: `nnz` (cxf_basis.h:57)
- Severity: INFO

#### [V101] EtaVector.rowIndices/rowValues -- naming
- Spec says: `rowIndices`, `rowValues`
- Code has: `indices`, `values` (cxf_basis.h:58-59)
- Severity: INFO

#### [V102] EtaVector.direction -- MISSING
- Spec says: `direction` int encoding entering variable's bound direction
- Code has: `status` int (cxf_basis.h:62) -- different semantics (new status vs direction)
- Severity: MEDIUM

#### [V103] EtaVector.colIndices/colValues -- MISSING
- Spec says: Optional column data fields for dual simplex
- Code has: No column data fields
- Severity: LOW (dual simplex not implemented)

#### [V104] EtaVector.next -- direction difference
- Spec says: `next` links to next OLDER eta vector
- Code has: `next` comment says "Link to next eta (newer)" (cxf_basis.h:63)
- Severity: HIGH -- Spec says prepend (newest at head, next points to older). Code comment says "newer", which contradicts spec's linked list direction. If the actual runtime behavior matches the spec (prepend), the comment is just wrong. If the list is truly ordered newest-first with next pointing to newer, FTRAN/BTRAN traversal order would be wrong.

---

### Environment (environment.md vs cxf_env.h)

#### [V105] CxfEnv.validationTag -- naming + type
- Spec says: `validationTag` int
- Code has: `magic` uint32_t (cxf_env.h:21)
- Severity: INFO

#### [V106] CxfEnv.secondaryTag -- MISSING
- Spec says: `secondaryTag` int64 for defense-in-depth
- Code has: No secondary validation tag
- Severity: MEDIUM

#### [V107] CxfEnv.activationState -- simplified
- Spec says: `activationState` int with INACTIVE=0, INITIALIZING=1, ACTIVE=2
- Code has: `active` int with 0/1 only (cxf_env.h:22)
- Severity: LOW (no INITIALIZING state)

#### [V108] CxfEnv.deploymentType/licenseMode/licensedFlag/licenseChecksum/licenseCode/licenseFilePath -- MISSING
- Spec says: Multiple licensing fields
- Code has: None (no licensing system)
- Severity: LOW (licensing not implemented)

#### [V109] CxfEnv.versionCode -- MISSING
- Spec says: `versionCode` int encoded version number
- Code has: No equivalent
- Severity: LOW

#### [V110] CxfEnv.parameterTable -- MISSING
- Spec says: Full parameter table system with metadata
- Code has: Individual parameter fields (feasibility_tol, optimality_tol, etc.) -- no table-driven system
- File: include/convexfeld/cxf_env.h:27-29
- Severity: MEDIUM -- Spec requires a table-driven parameter system. Code uses direct struct fields.

#### [V111] CxfEnv.parameterFlags/parameterStorage1/parameterStorage2/stringParameterPointers etc. -- MISSING
- Spec says: Full parameter management infrastructure
- Code has: None
- Severity: MEDIUM

#### [V112] CxfEnv.logFileHandle/logFileName -- MISSING
- Spec says: Log file handle and name
- Code has: No log file support (only log callback)
- Severity: LOW

#### [V113] CxfEnv.criticalSection -- MISSING
- Spec says: `criticalSection` pointer-to-mutex for thread safety
- Code has: No mutex
- Severity: MEDIUM (thread safety not implemented)

#### [V114] CxfEnv.threadPool1/threadPool2 and all threading fields -- MISSING
- Spec says: Multiple threading fields
- Code has: None
- Severity: LOW (single-threaded implementation)

#### [V115] CxfEnv.computeServerAddress and all server fields -- MISSING
- Spec says: Multiple compute server, token server, cloud, cluster manager fields
- Code has: None
- Severity: LOW (remote solving not implemented)

#### [V116] CxfEnv.wlsConnection and all WLS fields -- MISSING
- Spec says: ~15 WLS fields
- Code has: None
- Severity: LOW (WLS not implemented)

#### [V117] CxfEnv.isvVendorName and all ISV fields -- MISSING
- Spec says: Multiple ISV fields
- Code has: None
- Severity: LOW (ISV not implemented)

#### [V118] CxfEnv.childEnvironmentArray/childEnvironmentCount -- MISSING
- Spec says: Child environment tracking
- Code has: No child management (only `master_env` pointer, cxf_env.h:63)
- Severity: LOW

#### [V119] CxfEnv.modelArray/modelCount/modelCapacity -- MISSING
- Spec says: Model tracking arrays
- Code has: No model tracking on environment
- Severity: MEDIUM

#### [V120] CxfEnv.rootEnvironment -- naming
- Spec says: `rootEnvironment` pointer-to-Environment
- Code has: `master_env` (cxf_env.h:63)
- Severity: INFO

#### [V121] CxfEnv.relatedEnvironment -- MISSING
- Spec says: `relatedEnvironment` pointer
- Code has: No equivalent
- Severity: LOW

#### [V122] CxfEnv.recordingEnabled/recordingData -- MISSING
- Spec says: API recording fields
- Code has: No equivalent
- Severity: LOW

#### [V123] CxfEnv.sessionReferenceCounter/sessionIdentifier -- partial
- Spec says: `sessionReferenceCounter` int64, `sessionIdentifier` int64
- Code has: `session_ref` int, `session_id` uint64_t (cxf_env.h:49-50)
- Severity: LOW (session_ref is int not int64)

#### [V124] CxfEnv.versionCounter -- naming
- Spec says: `versionCounter` int
- Code has: `version` int (cxf_env.h:46)
- Severity: INFO

#### [V125] CxfEnv.asyncState -- MISSING
- Spec says: `asyncState` pointer-to-AsyncState
- Code has: No equivalent (terminate_flag/terminate_flag_ptr serve partial role)
- Severity: LOW

#### [V126] CxfEnv.batchMode/batchSizeLimit -- MISSING
- Spec says: Batch operation fields
- Code has: None
- Severity: LOW

#### [V127] CxfEnv.fingerprintMode -- MISSING
- Spec says: `fingerprintMode` int
- Code has: No equivalent
- Severity: LOW

#### [V128] CxfEnv.cpuInfoBuffer/platformInfoBuffer/hostnameBuffer/distributionInfoBuffer -- MISSING
- Spec says: System information buffers
- Code has: None
- Severity: LOW

#### [V129] CxfEnv.memoryLimit/softMemoryLimit -- MISSING
- Spec says: Memory limit fields
- Code has: None
- Severity: LOW

#### [V130] CxfEnv.infinityThreshold -- naming
- Spec says: `infinityThreshold` double
- Code has: `infinity` double (cxf_env.h:29)
- Severity: INFO

#### [V131] CxfEnv -- EXTRA FIELDS not in spec
- `max_eta_count`, `max_eta_memory`, `refactor_interval` (cxf_env.h:40-42) -- refactorization parameters
- `terminate_flag_ptr`, `terminate_flag` (cxf_env.h:36-37) -- termination handling
- `log_callback`, `log_callback_data` (cxf_env.h:58-59)
- Severity: INFO (implementation-specific extensions)

---

### CallbackState (callback_state.md vs cxf_callback.h)

#### [V132] CallbackState -- renamed to CallbackContext
- Spec says: Structure named `CallbackState`
- Code has: `CallbackContext` (cxf_callback.h:40)
- Severity: INFO (naming)

#### [V133] CallbackContext.validationTag1 -- naming
- Spec says: `validationTag1` int
- Code has: `magic` uint32_t (cxf_callback.h:41)
- Severity: INFO

#### [V134] CallbackContext.validationTag2 -- naming
- Spec says: `validationTag2` int64
- Code has: `safety_magic` uint64_t (cxf_callback.h:42)
- Severity: INFO (types match semantically)

#### [V135] CallbackContext.mutex -- MISSING
- Spec says: `mutex` pointer-to-Mutex for thread-safe callback invocation
- Code has: No mutex field
- File: include/convexfeld/cxf_callback.h
- Severity: MEDIUM (thread safety for callbacks not implemented)

#### [V136] CallbackContext.environment -- MISSING
- Spec says: `environment` back-pointer to Environment
- Code has: No environment back-pointer
- Severity: MEDIUM

#### [V137] CallbackContext.primaryModel -- MISSING
- Spec says: `primaryModel` pointer-to-Model
- Code has: No model reference
- Severity: MEDIUM

#### [V138] CallbackContext.parentCallbackState -- MISSING
- Spec says: `parentCallbackState` pointer for inherited callbacks
- Code has: No equivalent
- Severity: LOW

#### [V139] CallbackContext.registrationTimestamp/baselineTimestamp -- partial
- Spec says: `registrationTimestamp` int64, `baselineTimestamp` int64
- Code has: `start_time` double (cxf_callback.h:53) -- single field, different type
- Severity: LOW

#### [V140] CallbackContext.callbackInvocationCount -- type mismatch
- Spec says: `callbackInvocationCount` double
- Code has: `callback_calls` double (cxf_callback.h:58) -- matches type
- Severity: INFO (naming only)

#### [V141] CallbackContext.callbackCumulativeTime -- naming
- Spec says: `callbackCumulativeTime` double
- Code has: `callback_time` double (cxf_callback.h:59)
- Severity: INFO

#### [V142] CallbackContext.configField1/configField2 -- MISSING
- Spec says: Two int64 configuration fields
- Code has: No equivalent
- Severity: LOW

#### [V143] CallbackContext.suppressStatisticsLog -- MISSING
- Spec says: `suppressStatisticsLog` bool
- Code has: No equivalent
- Severity: LOW

#### [V144] CallbackContext.sentinel1/sentinel2 -- MISSING
- Spec says: Guard sentinel values set to -1
- Code has: No guard sentinels
- Severity: LOW

#### [V145] CallbackContext -- EXTRA FIELDS
- `callback_func` (cxf_callback.h:45) -- spec says function pointer is on Environment, not CallbackState
- `terminate_requested` (cxf_callback.h:49) -- spec says termination uses Environment's asyncState
- `iteration_count` (cxf_callback.h:54) -- not in spec
- `best_obj` (cxf_callback.h:55) -- not in spec
- Severity: MEDIUM -- Spec explicitly says callback function pointer resides on Environment, not CallbackState. Code places it on CallbackContext.

---

### Supporting Structures (supporting_structures.md)

#### [V146] IISState -- NOT IMPLEMENTED
- Spec defines complete IISState structure with constrIIS, varLowerBoundIIS, varUpperBoundIIS, constrNames
- Code has: No IISState implementation found
- Severity: LOW (IIS computation not implemented)

#### [V147] ModificationTracker -- NOT IMPLEMENTED
- Spec defines full lazy update buffer with per-element bitmasks
- Code has: `pending_buffer` void* on Model (cxf_model.h:52) but no ModificationTracker struct
- Severity: MEDIUM (pending buffer exists as opaque pointer but no structure definition found)

#### [V148] WarmStartData -- NOT IMPLEMENTED
- Spec defines warm start data with basis status, solution values, factorization cache
- Code has: No WarmStartData structure
- Severity: LOW (warm starting not implemented)

#### [V149] CrossoverState -- NOT IMPLEMENTED
- Spec defines crossover-specific fields (isDualSimplex, diagonalQ, offDiagonalCounts, etc.)
- Code has: No crossover state
- Severity: LOW (barrier-to-simplex crossover not implemented)

---

### BasisSnapshot (in code but spec doesn't describe as standalone)

#### [V150] BasisSnapshot -- EXTRA STRUCTURE
- Spec describes `progressSnapshot` as a simple int array on BasisState
- Code has: Full `BasisSnapshot` struct with numVars, numConstrs, basisHeader, varStatus, etc. (cxf_basis.h:118-129)
- Severity: INFO (code's BasisSnapshot is richer than spec's snapshot concept)

---

### SolveState (in code, not in spec)

#### [V151] SolveState -- EXTRA STRUCTURE
- Spec does not define a `SolveState` structure
- Code has: Full `SolveState` struct (cxf_solve_state.h:41-61) as lightweight solve control wrapper
- Severity: INFO (implementation detail for solve orchestration)

---

### LUFactors (in code, spec describes differently)

#### [V152] LUFactors -- structural difference from spec
- Spec says: BasisState has `lowerTriangular` and `upperTriangular` as pointer-to-SparseTriangularMatrix
- Code has: `LUFactors` struct (cxf_basis.h:24-45) combining L and U with additional fields: `U_diag`, `L_nnz`, `U_nnz`, `valid` flag
- Severity: LOW (functionally equivalent, more practical implementation)

#### [V153] LUFactors.U_diag -- EXTRA FIELD
- Spec says: U has "no zero diagonal entries" but doesn't mention separate diagonal storage
- Code has: `U_diag` double* [m] (cxf_basis.h:35) -- separate diagonal storage
- Severity: INFO (common implementation optimization)

---

## Missing Structures/Fields Summary

### Critical (HIGH severity)
1. **SolverState.varStatus/basisHeader** (V29) -- Spec says SolverState owns these; code puts them only on BasisState
2. **Model.primary_matrix/working_matrix** (V53) -- Two-matrix architecture not implemented at Model level
3. **EtaVector variants** (V93) -- Only one variant (pivot) implemented; VARIABLE_FIX and WARM_START missing
4. **EtaVector.next direction** (V104) -- Code comment says "newer" but spec says "older"

### Moderate (MEDIUM severity)
5. SolverState: rowColCount, colRowCount, steepest edge arrays (4 fields), etaTotalCount sync, problemVarIndex semantics
6. Model: secondary_sentinel, environment_owned, attribute_table
7. MatrixData: objCoeffs/objOffset/lb/ub/vtype location (on Model instead of MatrixData)
8. WorkArrays/SolutionData: primalValues, dualValues location
9. Environment: secondaryTag, parameterTable, criticalSection, modelArray
10. CallbackContext: mutex, environment backpointer, primaryModel, callback_func location
11. Supporting: ModificationTracker not defined
12. BasisState: progressSnapshot location (on SolverState instead)

## Extra Structures/Fields (in code but not in spec)

1. `SolveState` (cxf_solve_state.h) -- lightweight solve wrapper
2. `BasisSnapshot` (cxf_basis.h) -- richer than spec's snapshot
3. `LUFactors` (cxf_basis.h) -- combined L+U struct
4. `SparseWork` / `SparseCol` (basis_internal.h) -- LU working matrix
5. BasisState: `diag_coeff`, `work`, `work2`, `eta_capacity`, `pivots_since_refactor`, `iteration`
6. SolverState: `num_artificials`, `work_x`, `work_pi`, `work_column`, `work_cB`, `use_bland`, `degenerate_count`, `saved_lb`, `saved_ub`, `perturb_*`, `min_activity`, `max_activity`, `negUnbdCount`, `posUnbdCount`, `row_status`, `col_nz_count`, `row_scale`, `col_scale`, `obj_at_last_refactor`, `iteration_mode`, `bounds_propagated`, `flip_count`, `eta_memory`, `total_ftran_time`, `ftran_count`, `baseline_ftran`, `iteration`, `last_refactor_iter`
7. MatrixData: `range_values`
8. CxfEnv: `max_eta_count`, `max_eta_memory`, `refactor_interval`, `terminate_flag_ptr`, `terminate_flag`, `log_callback`, `log_callback_data`
9. CallbackContext: `callback_func`, `terminate_requested`, `iteration_count`, `best_obj`

## Notes

1. **Naming convention**: The spec uses camelCase (numVars, basisHeader); the code uses snake_case (num_vars, basic_vars). This is a systematic difference, not individual violations. All naming-only findings are marked INFO severity.

2. **Array sizing**: The spec sizes SolverState working arrays at [numVars]. The code consistently sizes them at [num_vars + num_constrs] to include slack variables. This is not a spec violation per se -- the spec's description of slacks may imply they are included in numVars. However, if the spec intends numVars to exclude slacks, the code arrays are oversized (not undersized), which is safe.

3. **Data location pattern**: The biggest structural pattern deviation is that the spec distributes data across Model, MatrixData, SolverState, and BasisState differently than the code. In particular:
   - Spec puts obj, bounds, vtype on MatrixData; code puts them on Model
   - Spec puts varStatus/basisHeader on both SolverState and BasisState; code puts them only on BasisState
   - Spec puts progressSnapshot on BasisState; code puts it on SolverState
   - Spec puts solution data (primalValues, dualValues) on WorkArrays; code puts them on Model

4. **Unimplemented subsystems**: Many missing fields relate to subsystems not yet built: MIP (node counts, solution pool, cuts), barrier (PDHG counts), QP (quadratic data), dual simplex (dual steepest edge), steepest edge pricing, IIS, warm starting, crossover, compute server, WLS, ISV licensing, threading, recording. These are expected gaps for a current LP-only implementation.

5. **EtaVector is the most structurally divergent**: The spec defines three distinct variants with rich field sets. The code has a single simple struct with only basic pivot fields. This will need significant work to become spec-compliant.
