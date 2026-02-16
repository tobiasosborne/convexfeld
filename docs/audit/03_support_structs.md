# Audit Report: Supporting Data Structures (Callback, WorkArrays, Supporting)
**Auditor:** Agent A3
**Date:** 2026-02-16
**Scope:** Headers cxf_callback.h, cxf_solver.h, cxf_timing.h, cxf_utilities.h, cxf_mps.h
**Specs:** data-model/callback_state.md, work_arrays.md, supporting_structures.md

## Summary
- Total violations found: 44
- Critical: 25 / Major: 15 / Minor: 4

## Violations

### V-01: CallbackContext uses wrong structure name
- **Severity:** CRITICAL
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_callback.h:40
- **Spec reference:** callback_state.md, structure name
- **Description:** Structure is named `CallbackContext` in implementation, should be `CallbackState`
- **Expected (from spec):** `struct CallbackState`
- **Actual (in code):** `struct CallbackContext`

### V-02: Missing validationTag1 field
- **Severity:** CRITICAL
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_callback.h:40-60
- **Spec reference:** callback_state.md, Validation section
- **Description:** CallbackContext lacks validationTag1 (int) sentinel
- **Expected (from spec):** `int validationTag1` - Primary sentinel value for detecting memory corruption
- **Actual (in code):** Has `uint32_t magic` instead

### V-03: Missing validationTag2 field
- **Severity:** CRITICAL
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_callback.h:40-60
- **Spec reference:** callback_state.md, Validation section
- **Description:** CallbackContext lacks validationTag2 (int64) sentinel
- **Expected (from spec):** `int64 validationTag2` - Secondary sentinel value for defense-in-depth validation
- **Actual (in code):** Has `uint64_t safety_magic` instead

### V-04: Wrong validation tag types
- **Severity:** MAJOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_callback.h:41-42
- **Spec reference:** callback_state.md, Validation section
- **Description:** Validation tags use uint32_t and uint64_t instead of int and int64
- **Expected (from spec):** `int validationTag1` and `int64 validationTag2`
- **Actual (in code):** `uint32_t magic` and `uint64_t safety_magic`

### V-05: Missing mutex field
- **Severity:** CRITICAL
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_callback.h:40-60
- **Spec reference:** callback_state.md, Synchronization section
- **Description:** CallbackContext has no mutex field for thread synchronization
- **Expected (from spec):** `pointer-to-Mutex mutex` - Mutex protecting concurrent access to callback invocation
- **Actual (in code):** Field missing entirely

### V-06: Missing environment back-reference
- **Severity:** CRITICAL
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_callback.h:40-60
- **Spec reference:** callback_state.md, Back-References section
- **Description:** Missing environment field for back-pointer to owning environment
- **Expected (from spec):** `pointer-to-Environment environment` - Back-pointer to the owning environment
- **Actual (in code):** Field missing entirely

### V-07: Missing primaryModel field
- **Severity:** CRITICAL
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_callback.h:40-60
- **Spec reference:** callback_state.md, Back-References section
- **Description:** Missing primaryModel reference to model that registered callback
- **Expected (from spec):** `pointer-to-Model primaryModel` - Reference to the model that originally registered the callback
- **Actual (in code):** Field missing entirely

### V-08: Missing parentCallbackState field
- **Severity:** MAJOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_callback.h:40-60
- **Spec reference:** callback_state.md, Back-References section
- **Description:** Missing parent link for inherited callback states
- **Expected (from spec):** `pointer-to-CallbackState parentCallbackState` - Link to parent environment's CallbackState
- **Actual (in code):** Field missing entirely

### V-09: Wrong field name for user_data
- **Severity:** MINOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_callback.h:46
- **Spec reference:** callback_state.md, User Callback Registration section
- **Description:** Field is named user_data but spec calls it userData
- **Expected (from spec):** `pointer userData`
- **Actual (in code):** `void *user_data`

### V-10: Missing registrationTimestamp field
- **Severity:** CRITICAL
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_callback.h:40-60
- **Spec reference:** callback_state.md, Timing section
- **Description:** Missing wall-clock timestamp for when callback was registered
- **Expected (from spec):** `int64 registrationTimestamp` - Wall-clock timestamp recorded when callback was first registered
- **Actual (in code):** Has `double start_time` instead

### V-11: Missing baselineTimestamp field
- **Severity:** MAJOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_callback.h:40-60
- **Spec reference:** callback_state.md, Timing section
- **Description:** Missing secondary timestamp for elapsed time intervals
- **Expected (from spec):** `int64 baselineTimestamp` - Secondary timestamp used as baseline for computing elapsed time intervals
- **Actual (in code):** Field missing entirely

### V-12: Wrong type for start_time
- **Severity:** MAJOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_callback.h:53
- **Spec reference:** callback_state.md, Timing section
- **Description:** start_time is double but spec requires int64 for timestamps
- **Expected (from spec):** `int64 registrationTimestamp`
- **Actual (in code):** `double start_time`

### V-13: Wrong field name for callback invocation count
- **Severity:** MINOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_callback.h:58
- **Spec reference:** callback_state.md, Timing section
- **Description:** Field is callback_calls but spec calls it callbackInvocationCount
- **Expected (from spec):** `double callbackInvocationCount`
- **Actual (in code):** `double callback_calls`

### V-14: Wrong field name for cumulative callback time
- **Severity:** MINOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_callback.h:59
- **Spec reference:** callback_state.md, Timing section
- **Description:** Field is callback_time but spec calls it callbackCumulativeTime
- **Expected (from spec):** `double callbackCumulativeTime`
- **Actual (in code):** `double callback_time`

### V-15: Missing configField1
- **Severity:** MAJOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_callback.h:40-60
- **Spec reference:** callback_state.md, Configuration section
- **Description:** Missing configuration field inherited from parent
- **Expected (from spec):** `int64 configField1` - Configuration parameter inherited from parent
- **Actual (in code):** Field missing entirely

### V-16: Missing configField2
- **Severity:** MAJOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_callback.h:40-60
- **Spec reference:** callback_state.md, Configuration section
- **Description:** Missing secondary configuration field inherited from parent
- **Expected (from spec):** `int64 configField2` - Secondary configuration parameter inherited from parent
- **Actual (in code):** Field missing entirely

### V-17: Missing suppressStatisticsLog field
- **Severity:** MAJOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_callback.h:40-60
- **Spec reference:** callback_state.md, Lifecycle Control section
- **Description:** Missing flag to suppress logging of callback statistics
- **Expected (from spec):** `bool suppressStatisticsLog` - When true, suppresses logging of callback performance statistics
- **Actual (in code):** Field missing entirely

### V-18: Missing sentinel1 and sentinel2 guard fields
- **Severity:** MAJOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_callback.h:40-60
- **Spec reference:** callback_state.md, Sentinel Guards section
- **Description:** Missing guard values for buffer overrun detection
- **Expected (from spec):** `int sentinel1` and `int sentinel2` - Guard values set to -1 for corruption detection
- **Actual (in code):** Fields missing entirely

### V-19: Extra fields not in spec
- **Severity:** MAJOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_callback.h:49-55
- **Spec reference:** callback_state.md
- **Description:** CallbackContext has fields not specified: terminate_requested, enabled, iteration_count, best_obj
- **Expected (from spec):** Only fields in spec should exist
- **Actual (in code):** Has terminate_requested, enabled (correct), iteration_count, best_obj

### V-20: Wrong callback event type constants
- **Severity:** CRITICAL
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_callback.h:19-22
- **Spec reference:** callback_state.md, Callback Event Types section
- **Description:** Callback event types have wrong names and set
- **Expected (from spec):** POLLING, PRESOLVE, SIMPLEX, BARRIER, MESSAGE
- **Actual (in code):** CXF_CB_PRE_SOLVE, CXF_CB_POLLING, CXF_CB_MIP_SOL, CXF_CB_POST_SOLVE (missing SIMPLEX, BARRIER, MESSAGE, has extra MIP_SOL, PRE_SOLVE, POST_SOLVE)

### V-21: SolverContext uses wrong structure name
- **Severity:** CRITICAL
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_solver.h:21
- **Spec reference:** work_arrays.md, structure naming (formerly WorkArrays, renamed to SolutionData)
- **Description:** Structure is named SolverContext but spec indicates the work/solution data structure should be SolutionData
- **Expected (from spec):** Work arrays belong in SolutionData (formerly WorkArrays), solver runtime state is SolverState
- **Actual (in code):** Uses SolverContext which conflates solver state and work arrays

### V-22: Missing solveMode field
- **Severity:** CRITICAL
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_solver.h:21-71
- **Spec reference:** work_arrays.md, Solve Mode and Status section
- **Description:** SolverContext has solve_mode but SolutionData should have solveMode
- **Expected (from spec):** `int solveMode` - Records which optimization algorithm produced the solution
- **Actual (in code):** Has `int solve_mode` in SolverContext (close but wrong context)

### V-23: Missing primalValues field
- **Severity:** CRITICAL
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_solver.h:21-71
- **Spec reference:** work_arrays.md, Primal and Dual Solution Arrays section
- **Description:** SolverContext has work_x but SolutionData should have primalValues
- **Expected (from spec):** `pointer-to-array-of-double primalValues` - Primal solution values for all decision variables
- **Actual (in code):** Has `double *work_x` (close but wrong naming convention)

### V-24: Missing dualValues field
- **Severity:** CRITICAL
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_solver.h:21-71
- **Spec reference:** work_arrays.md, Primal and Dual Solution Arrays section
- **Description:** SolverContext has work_pi but SolutionData should have dualValues
- **Expected (from spec):** `pointer-to-array-of-double dualValues` - Dual values (shadow prices) for linear constraints
- **Actual (in code):** Has `double *work_pi` (close but wrong naming convention)

### V-25: Missing rangeDuals field
- **Severity:** MAJOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_solver.h:21-71
- **Spec reference:** work_arrays.md, Primal and Dual Solution Arrays section
- **Description:** No field for range constraint dual values
- **Expected (from spec):** `pointer-to-array-of-double rangeDuals` - Dual values for range constraints
- **Actual (in code):** Field missing entirely

### V-26: Missing sosDuals field
- **Severity:** MAJOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_solver.h:21-71
- **Spec reference:** work_arrays.md, Primal and Dual Solution Arrays section
- **Description:** No field for SOS constraint dual values
- **Expected (from spec):** `pointer-to-array-of-double sosDuals` - Dual values for SOS constraints
- **Actual (in code):** Field missing entirely

### V-27: Missing objectiveValue field
- **Severity:** CRITICAL
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_solver.h:21-71
- **Spec reference:** work_arrays.md, Objective Function Data section
- **Description:** SolverContext has obj_value but SolutionData should have objectiveValue
- **Expected (from spec):** `double objectiveValue` - Best objective function value found during optimization
- **Actual (in code):** Has `double obj_value` (close but wrong naming convention)

### V-28: Missing objectiveBound field
- **Severity:** MAJOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_solver.h:21-71
- **Spec reference:** work_arrays.md, Objective Function Data section
- **Description:** No field for objective bound
- **Expected (from spec):** `double objectiveBound` - Best proven bound on optimal objective value
- **Actual (in code):** Field missing entirely

### V-29: Missing poolObjectiveBound field
- **Severity:** MAJOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_solver.h:21-71
- **Spec reference:** work_arrays.md, Objective Function Data section
- **Description:** No field for solution pool objective bound
- **Expected (from spec):** `double poolObjectiveBound` - Objective bound for solution pool
- **Actual (in code):** Field missing entirely

### V-30: Missing solutionCount field
- **Severity:** MAJOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_solver.h:21-71
- **Spec reference:** work_arrays.md, Iteration Counters section
- **Description:** No field for number of feasible solutions found
- **Expected (from spec):** `int solutionCount` - Number of feasible solutions found during optimization
- **Actual (in code):** Field missing entirely

### V-31: Wrong field name for iterationCount
- **Severity:** MINOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_solver.h:60
- **Spec reference:** work_arrays.md, Iteration Counters section
- **Description:** Field is named iteration but should be iterationCount
- **Expected (from spec):** `double iterationCount` - Total simplex iteration count
- **Actual (in code):** `int iteration` (also wrong type)

### V-32: Missing iterationCount0 field
- **Severity:** MAJOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_solver.h:21-71
- **Spec reference:** work_arrays.md, Iteration Counters section
- **Description:** No field for sub-phase iteration count
- **Expected (from spec):** `double iterationCount0` - Iteration count for specific sub-phase
- **Actual (in code):** Field missing entirely

### V-33: Missing barrierIterationCount field
- **Severity:** MAJOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_solver.h:21-71
- **Spec reference:** work_arrays.md, Iteration Counters section
- **Description:** No field for barrier iteration count
- **Expected (from spec):** `double barrierIterationCount` - Total barrier iteration count
- **Actual (in code):** Field missing entirely

### V-34: Missing pdhgIterationCount field
- **Severity:** MAJOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_solver.h:21-71
- **Spec reference:** work_arrays.md, Iteration Counters section
- **Description:** No field for PDHG iteration count
- **Expected (from spec):** `double pdhgIterationCount` - Total PDHG iteration count
- **Actual (in code):** Field missing entirely

### V-35: Missing all scaling/tolerance fields from SolutionData
- **Severity:** CRITICAL
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_solver.h:21-71
- **Spec reference:** work_arrays.md, Scaling and Tolerance Parameters section
- **Description:** SolutionData should have scaledTolerance, scaleFactor1, scaleFactor2, baseTolerance
- **Expected (from spec):** Four fields for tolerance/scaling parameters
- **Actual (in code):** SolverContext has tolerance (one field) but missing the other scaling fields

### V-36: Missing iteration history tracking fields
- **Severity:** MAJOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_solver.h:21-71
- **Spec reference:** work_arrays.md, Iteration History Tracking section
- **Description:** Missing activeFlag, cycleDetectionFlag, previousEnteringVar, previousLeavingVar, previousPivotRow
- **Expected (from spec):** Five fields for tracking iteration history and anti-cycling
- **Actual (in code):** Fields missing entirely

### V-37: Missing solution pool fields
- **Severity:** MAJOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_solver.h:21-71
- **Spec reference:** work_arrays.md, Solution Pool section
- **Description:** Missing all solution pool fields
- **Expected (from spec):** poolSolutionCount, poolSolutionCount2, poolVariableValues, poolObjectiveValues, poolObjectiveBounds
- **Actual (in code):** Fields missing entirely

### V-38: Missing cut data fields
- **Severity:** MAJOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_solver.h:21-71
- **Spec reference:** work_arrays.md, Cut Data section
- **Description:** Missing cut tracking fields
- **Expected (from spec):** cutCount, cutVariableValues, cutObjectiveValues
- **Actual (in code):** Fields missing entirely

### V-39: Missing thresholds array
- **Severity:** MAJOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_solver.h:21-71
- **Spec reference:** work_arrays.md, Threshold Values section
- **Description:** Missing adaptive threshold array
- **Expected (from spec):** `array-of-double [6] thresholds` - Adaptive threshold values
- **Actual (in code):** Field missing entirely

### V-40: Missing auxiliaryIndices array
- **Severity:** MAJOR
- **File:** /home/tobiasosborne/Projects/convexfeld/include/convexfeld/cxf_solver.h:21-71
- **Spec reference:** work_arrays.md, Auxiliary Indices section
- **Description:** Missing auxiliary indices array
- **Expected (from spec):** `array-of-int [3] auxiliaryIndices` - Auxiliary variable/constraint indices
- **Actual (in code):** Field missing entirely

### V-41: No IISState structure found
- **Severity:** CRITICAL
- **File:** All inspected headers
- **Spec reference:** supporting_structures.md, Section 1: IISState
- **Description:** IISState structure not implemented anywhere
- **Expected (from spec):** Complete IISState structure with numConstrs, constrIIS, varLowerBoundIIS, varUpperBoundIIS, constrNames
- **Actual (in code):** Structure missing entirely

### V-42: No ModificationTracker structure found
- **Severity:** CRITICAL
- **File:** All inspected headers
- **Spec reference:** supporting_structures.md, Section 2: ModificationTracker
- **Description:** ModificationTracker structure not implemented anywhere
- **Expected (from spec):** Complete ModificationTracker structure for lazy updates
- **Actual (in code):** Structure missing entirely

### V-43: No WarmStartData structure found
- **Severity:** CRITICAL
- **File:** All inspected headers
- **Spec reference:** supporting_structures.md, Section 3: WarmStartData
- **Description:** WarmStartData structure not implemented anywhere
- **Expected (from spec):** Complete WarmStartData structure with basis and solution warm-start fields
- **Actual (in code):** Structure missing entirely

### V-44: No CrossoverState structure found
- **Severity:** MAJOR
- **File:** All inspected headers
- **Spec reference:** supporting_structures.md, Section 4: CrossoverState
- **Description:** CrossoverState fields not found in SolverContext
- **Expected (from spec):** Crossover-specific fields embedded in SolverState: isDualSimplex, initMode, diagonalQ, offDiagonalCounts, binaryConversionCount, errorVariableIndex, timingWeight, timingAccumulator
- **Actual (in code):** Fields missing entirely

## Files Not Covered by Spec

1. **cxf_timing.h**: The TimingState structure (lines 20-32) is implemented but not covered by the supporting_structures.md spec. The spec mentions timing but doesn't provide a complete TimingState structure definition.

2. **cxf_utilities.h**: Math wrapper functions and utility functions are implemented but not covered by supporting_structures.md.

3. **cxf_mps.h**: MPS file format parser declarations not covered by the data model specs (this is expected as it's I/O functionality, not a data structure).

## Spec Items Not Implemented

1. **CallbackState** (callback_state.md): Only partially implemented as CallbackContext with ~60% of fields missing or wrong.

2. **SolutionData** (work_arrays.md, formerly WorkArrays): Not implemented as a separate structure. The implementation conflates solver runtime state (SolverState concept) with solution data container (SolutionData concept) in a single SolverContext structure. Missing ~70% of specified fields.

3. **IISState** (supporting_structures.md, Section 1): Not implemented at all.

4. **ModificationTracker** (supporting_structures.md, Section 2): Not implemented at all.

5. **WarmStartData** (supporting_structures.md, Section 3): Not implemented at all.

6. **CrossoverState** (supporting_structures.md, Section 4): Not implemented at all.

## Analysis

### Critical Architectural Issue: SolverContext vs SolverState vs SolutionData

The implementation has a fundamental architectural confusion:

- **Spec architecture**: Two separate structures:
  - **SolverState**: Runtime solver state (basis, pricing, eta vectors, working arrays FOR iteration)
  - **SolutionData**: Model-level output container (primal values, dual values, objective, iteration counts, solution pool)

- **Implementation architecture**: Single structure:
  - **SolverContext**: Conflates both concepts into one structure

The spec explicitly states (work_arrays.md, Purpose section):
> "Naming history: Formerly `WorkArrays`; renamed to better reflect its actual role as a solution data container rather than a scratch buffer structure. **This structure stores the results of an optimization call** ... **It does not hold scratch buffers for simplex iterations -- that role belongs to SolverState.**"

The implementation's SolverContext contains:
- Solver runtime state fields: phase, solve_mode, basis, pricing, eta_count, work_counter, etc. (belongs in SolverState)
- Solution output fields: work_x, work_pi, obj_value, iteration (belongs in SolutionData)

This conflation violates the separation of concerns specified in the v2 specs.

### CallbackContext Issues

The CallbackContext structure is missing critical thread-safety infrastructure (mutex), parent-child relationship tracking (parentCallbackState), and environment integration (environment, primaryModel). The callback event types don't match the spec at all.

### Missing Supporting Structures

Four entire structures from supporting_structures.md are completely missing: IISState, ModificationTracker, WarmStartData, and CrossoverState. These are not optional features - they are core LP solver functionality.

## Recommendations

1. **Immediate**: Split SolverContext into SolverState and SolutionData per spec architecture.

2. **High Priority**: Implement missing supporting structures (IISState, ModificationTracker, WarmStartData, CrossoverState).

3. **High Priority**: Rename and fix CallbackContext to match CallbackState spec, add missing fields.

4. **Medium Priority**: Standardize field naming (camelCase per spec vs snake_case in implementation).

5. **Document TimingState**: Either add TimingState to specs or remove it from implementation if redundant.
