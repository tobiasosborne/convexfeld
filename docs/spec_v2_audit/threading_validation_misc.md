# Spec V2 Audit: Threading, Validation & Misc

## Files Reviewed

### Spec Files
- `docs/specs-v2/specs/modules/threading_sync.md`
- `docs/specs-v2/specs/modules/thread_init_thunks.md`
- `docs/specs-v2/specs/integration/threading_model.md`
- `docs/specs-v2/specs/modules/data_validation.md`
- `docs/specs-v2/specs/modules/input_validation.md`
- `docs/specs-v2/specs/modules/state_cleanup_buffers.md`
- `docs/specs-v2/specs/modules/state_cleanup_solver.md`
- `docs/specs-v2/specs/modules/state_initialization.md`
- `docs/specs-v2/specs/modules/cleanup_utilities.md`
- `docs/specs-v2/specs/modules/model_type_checking.md`
- `docs/specs-v2/specs/algorithms/crash_basis.md`
- `docs/specs-v2/specs/algorithms/bound_propagation.md`

### Implementation Files
- `src/threading/config.c`
- `src/threading/seed.c`
- `src/threading/locks.c`
- `src/threading/cpu.c`
- `src/logging/system.c`
- `src/validation/arrays.c`
- `src/memory/state_cleanup.c`
- `src/analysis/model_type.c`
- `src/analysis/coef_stats.c`
- `src/simplex/crash.c`
- `src/simplex/presolve.c`
- `src/utilities/fix_var.c`
- `src/utilities/math_wrappers.c`
- `src/utilities/helpers.c`
- `src/error/nan_check.c`
- `src/error/model_flags.c`
- `src/error/pivot_check.c`
- `src/solver_state/helpers.c`
- `src/solver_state/init.c`

---

## Compliant Functions

### cxf_simplex_crash (crash.c)
Compliant with `crash_basis.md`. Single-pass row scan, feasibility check for unassigned rows, conditional removal for candidate rows, column nonzero count decrement, work counter tracking, early infeasibility return with problem_row_index set. All algorithmic steps match the spec pseudocode.

### cxf_get_logical_processors (logging/system.c)
Compliant with `threading_sync.md`. Returns the number of logical processors via platform-specific API. Ensures minimum return of 1. Note: spec says it reads from the Environment; implementation is a standalone function (see V3).

### cxf_get_physical_cores (threading/cpu.c)
Partially compliant. Detects physical cores via platform APIs with fallback. Note: spec says it returns `min(logical, physical)` from the Environment; implementation detects directly (see V4).

### cxf_generate_seed (threading/seed.c)
No direct spec function for this. It is an infrastructure utility. Not in violation; it supports per-thread RNG state as described in `thread_init_thunks.md`.

### cxf_validate_pivot_element (error/pivot_check.c)
Not specified in the audited specs. Implementation is a simple utility; no compliance issue.

---

## VIOLATIONS

### [V1] cxf_get_threads -- Stub returning 0 instead of full resolution chain
- **Spec says:** (threading_sync.md, cxf_get_threads) Compute the effective thread count by reconciling: (1) model-level override, (2) auto-detection with cap at ~32, (3) user Threads parameter, (4) license thread limit. Returns the most restrictive of all limits.
- **Code does:** Returns 0 (auto mode) for all non-NULL environments. No resolution chain, no parameter lookup, no cap logic, no license check.
- **File:** `src/threading/config.c:27-35`
- **Severity:** High (entire function is a stub)

### [V2] cxf_validate_thread_count vs cxf_set_thread_count -- Wrong name and wrong behavior
- **Spec says:** (threading_sync.md, cxf_set_thread_count) Validate a thread count against hardware and emit a warning via cxf_log if it exceeds the logical processor count. Takes (environment, thread_count), returns void. Does NOT store the value.
- **Code does:** Named `cxf_validate_thread_count`. Returns int error code (not void). Requires thread_count >= 1 (spec does not mention this requirement). Does not emit any warning via logging. Does not compare against logical processor count.
- **File:** `src/threading/config.c:58-74`
- **Severity:** Medium (name mismatch, no warning emission, wrong return type)

### [V3] cxf_get_logical_processors -- Standalone function instead of Environment accessor
- **Spec says:** (threading_sync.md) Takes `pointer-to-Environment` as input, returns the logical processor count stored in the environment. The value is detected at environment creation time and is read-only thereafter.
- **Code does:** Takes no arguments (`void`). Detects hardware directly on each call rather than reading from the Environment. Not an accessor -- it performs system calls each time.
- **File:** `src/logging/system.c:29-47`
- **Severity:** Medium (architectural deviation -- should be cached in Environment)

### [V4] cxf_get_physical_cores -- Standalone function instead of Environment accessor
- **Spec says:** (threading_sync.md) Takes `pointer-to-Environment`, returns `min(logicalProcessorCount, physicalCoreCount)` from the environment.
- **Code does:** Takes no arguments. Detects directly via platform APIs. Does NOT compute min(logical, physical) -- returns whatever the OS reports. Falls back to logical count on failure.
- **File:** `src/threading/cpu.c:29-93`
- **Severity:** Medium (no min computation, not an Environment accessor)

### [V5] cxf_validate_array -- Does not write error message to environment error buffer
- **Spec says:** (data_validation.md, cxf_validate_array) On detecting a NaN, writes a diagnostic message to the environment's error buffer identifying the index of the first NaN element, respecting the cascading error pattern (check error buffer lock flag, check empty buffer).
- **Code does:** Casts `env` to `(void)env` and ignores it entirely. No error message is written. Returns error code but provides no diagnostic information about which index contained NaN.
- **File:** `src/validation/arrays.c:24-45`
- **Severity:** Medium (no diagnostics on error)

### [V6] cxf_validate_array -- Uses math.h isnan() instead of cxf_check_nan
- **Spec says:** (data_validation.md) Calls `cxf_check_nan` (from Input Validation module) on each element's bit-level representation.
- **Code does:** Uses `isnan(array[i])` from `<math.h>` directly. While functionally equivalent for NaN detection, it does not use the specified internal function.
- **File:** `src/validation/arrays.c:39`
- **Severity:** Low (functionally equivalent but does not use specified dependency)

### [V7] cxf_validate_vartypes -- Wrong signature
- **Spec says:** (data_validation.md, cxf_validate_vartypes) Signature is `(env, count, vartypes)` -- takes an Environment pointer, a count, and a char array.
- **Code does:** Signature is `(model)` -- takes a CxfModel pointer and reads vtype from model fields directly. Also modifies bounds for binary variables (clamping to [0,1]), which is NOT in the spec.
- **File:** `src/validation/arrays.c:56-114`
- **Severity:** High (completely wrong signature, extra behavior modifying model bounds)

### [V8] cxf_validate_vartypes -- No case normalization
- **Spec says:** Performs case normalization: lowercase ASCII letters are converted to uppercase before comparison.
- **Code does:** Only checks uppercase characters directly (`t != 'C' && t != 'B' && ...`). Lowercase input would be rejected as invalid.
- **File:** `src/validation/arrays.c:80`
- **Severity:** Medium (lowercase variable type codes will be rejected)

### [V9] cxf_validate_vartypes -- No error logging
- **Spec says:** On invalid type, logs an error through the environment's error logging facility, reporting the original non-normalized character.
- **Code does:** Returns CXF_ERROR_INVALID_ARGUMENT with no logging at all.
- **File:** `src/validation/arrays.c:80-81`
- **Severity:** Medium (no diagnostics)

### [V10] cxf_check_nan -- Wrong signature (array instead of single value)
- **Spec says:** (input_validation.md, cxf_check_nan) Takes a single double value (passed as its raw bit representation), returns bool (true if NaN).
- **Code does:** Takes `(const double *arr, int n)` -- an entire array. Returns int (0, 1, or -1). Fundamentally different interface: array-scanning instead of single-value check.
- **File:** `src/error/nan_check.c:24-34`
- **Severity:** High (completely wrong interface)

### [V11] cxf_is_finite vs cxf_check_is_finite / cxf_check_nan_or_inf -- Wrong name and wrong interface
- **Spec says:** (input_validation.md) `cxf_check_is_finite` takes a single double (bit representation), returns bool (true if finite). `cxf_check_nan_or_inf` is an alias for the same behavior.
- **Code does:** Named `cxf_is_finite`. Takes `(const double *arr, int n)` -- an array. Returns int (0 if all finite, 1 if non-finite found, -1 on NULL). Completely different interface.
- **File:** `src/error/nan_check.c:46-56`
- **Severity:** High (wrong name, wrong interface)

### [V12] cxf_check_model_flags1 -- Wrong behavior
- **Spec says:** (model_type_checking.md, cxf_check_model_flags1) Checks whether a model has active optimization state: (1) a non-null concurrent solve state, or (2) a SolverState with an active flag > 0 and a non-null basis factorization pointer.
- **Code does:** Checks whether the model contains MIP features (integer variables, SOS constraints). This is the behavior of `cxf_is_mip_model`, not `cxf_check_model_flags1`.
- **File:** `src/error/model_flags.c:26-55`
- **Severity:** High (completely wrong behavior -- checks MIP features instead of active optimization state)

### [V13] cxf_check_model_flags2 -- Wrong behavior and wrong signature
- **Spec says:** (model_type_checking.md, cxf_check_model_flags2) Takes `(model)`, returns 1 if dual solution data is available: checks non-null SolverState, non-null dual data pointer, status indicates optimal/infeasible/infeasible-or-unbounded, and active flag is positive.
- **Code does:** Takes `(model, flag)` -- extra parameter not in spec. Returns whether the model has quadratic/conic features. This is entirely the wrong behavior; it checks model structure not solve state.
- **File:** `src/error/model_flags.c:73-100`
- **Severity:** High (completely wrong behavior, wrong signature)

### [V14] cxf_is_mip_model -- Scans variable types instead of checking MatrixData flags
- **Spec says:** (model_type_checking.md) Inspects the model's matrix data for multiple MIP-qualifying properties: MIP solve flag, integer variables, binary variables, SOS constraints, indicator constraints, piecewise-linear objective terms, semi-continuous/semi-integer variables, quadratic constraints, multi-objective flag, force-non-convex flag.
- **Code does:** Scans only model->vtype for non-'C' characters. Does not check matrix data for SOS, indicator constraints, PWL, quadratic constraints, or any flags.
- **File:** `src/analysis/model_type.c:24-49`
- **Severity:** Medium (partial implementation -- only checks variable types, misses many MIP indicators)

### [V15] cxf_is_quadratic -- Stub returning 0
- **Spec says:** (model_type_checking.md) Checks for environment force-QP parameter OR (general constraints present AND no disqualifying elements). Returns 1 if model qualifies as pure QP.
- **Code does:** Always returns 0 with a TODO comment.
- **File:** `src/analysis/model_type.c:63-84`
- **Severity:** Medium (unimplemented stub)

### [V16] cxf_is_socp -- Stub returning 0
- **Spec says:** (model_type_checking.md) Checks multiple fields: force-SOCP parameter, quadratic constraints, cone constraints, indicator constraints, multi-scenario, semi-continuous/semi-integer, NLP variables. Returns 1 if any SOCP element present.
- **Code does:** Always returns 0 with a TODO comment.
- **File:** `src/analysis/model_type.c:98-119`
- **Severity:** Medium (unimplemented stub)

### [V17] cxf_special_check -- Wrong signature
- **Spec says:** (data_validation.md, cxf_special_check) Takes `(state, varIdx)` -- a SolverState pointer and variable index. Reads bounds, flags, and quadratic structure from SolverState arrays.
- **Code does:** Takes `(lb, ub, flags, work_accum)` -- individual values passed as arguments. Does not take a SolverState or variable index. Cannot validate quadratic structure (stage 4 of spec).
- **File:** `src/error/pivot_check.c:69-98`
- **Severity:** High (wrong signature, no quadratic validation, no work counter integration via SolverState)

### [V18] cxf_special_check -- Rejects all quadratic variables
- **Spec says:** If the variable has quadratic flag set, validate the Q-matrix structure: check diagonal is non-negative, off-diagonal coefficients are non-negative, neighbor lower bounds are finite.
- **Code does:** If quadratic flag is set, unconditionally returns 0 (rejects).
- **File:** `src/error/pivot_check.c:91-95`
- **Severity:** Medium (correct behavior for LP-only, but violates spec for QP)

### [V19] cxf_init_solve_state -- Different structure and behavior
- **Spec says:** (state_initialization.md, cxf_init_solve_state) Takes `(model, threadLocalData)`. Clears environment termination flag, zeros model timing fields, records timestamp in model manager, initializes MIP callback timing, adjusts environment objective offset tolerance with perturbation, warns about memory limit, initializes thread-local memory.
- **Code does:** Takes `(SolveState, SolverState, CxfEnv)` -- different parameter types entirely. Initializes a SolveState structure (not model state). Sets magic number, status, iterations, phase, captures timestamp, reads config. Does not touch environment termination flag, does not adjust objective offset tolerance, no model manager interaction.
- **File:** `src/solver_state/init.c:34-94`
- **Severity:** High (completely different function operating on different structure)

### [V20] cxf_cleanup_solve_state -- Different structure and behavior
- **Spec says:** (state_cleanup_solver.md, cxf_cleanup_solve_state) Takes `(model, timingData)`. Performs thread-local cleanup, clears environment interrupt flag, restores objective offset tolerance, invokes callback timing, computes solve duration/work rate, invokes environment finalization callback.
- **Code does:** Takes `(SolveState)`. Zeros out all fields of a SolveState structure and invalidates its magic number. No thread cleanup, no environment interaction, no timing computation, no callbacks.
- **File:** `src/solver_state/init.c:110-154`
- **Severity:** High (completely different function)

### [V21] cxf_free_attribute_table vs cxf_free_solver_state -- Name mismatch
- **Spec says:** (state_cleanup_solver.md, cxf_free_solver_state) Frees the model's attribute table structure and its entries array, then nulls the model's reference.
- **Code does:** Named `cxf_free_attribute_table`. Frees an entire SolverState with all its arrays, subcomponents (BasisState, PricingState, TimingState), and the struct itself. This is much broader than spec (which only frees the attribute table).
- **File:** `src/memory/state_cleanup.c:46-99`
- **Severity:** High (name matches intent but frees a completely different structure -- SolverState not attribute table)

### [V22] cxf_free_basis_state -- Wrong behavior
- **Spec says:** (state_cleanup_solver.md, cxf_free_basis_state) Frees the array of concurrent solver environments with reference counting, deferred cleanup, and remote job termination.
- **Code does:** Wraps `cxf_basis_free(basis)` -- frees a BasisState (eta list, basic_vars, var_status). Has nothing to do with concurrent environments.
- **File:** `src/memory/state_cleanup.c:113-115`
- **Severity:** High (completely different behavior)

### [V23] cxf_free_callback_state -- Different behavior from spec
- **Spec says:** (state_cleanup_buffers.md, cxf_free_callback_state) Cleanly disconnects from a remote solver, handles termination with bounded polling, sends disconnect message, frees remote solver connection, zeros callback registration count.
- **Code does:** Clears magic numbers, nulls function pointer and user_data, then frees the struct. No remote solver interaction at all.
- **File:** `src/memory/state_cleanup.c:129-143`
- **Severity:** High (no remote solver cleanup, fundamentally simpler)

### [V24] cxf_propagate_bounds -- Significant structural deviations
- **Spec says:** (cleanup_utilities.md / bound_propagation.md) Uses constraint indices in the worklist queue. Seeds worklist with active constraints from basis header. Uses row-major matrix for constraint traversal and column-major for activity updates. Includes compensated summation heuristic. Records infeasibility in solver_state diagnostic fields.
- **Code does:** Uses variable indices in the worklist, not constraint indices. Seeds worklist based on `var_status < 0` (basic variables), not basis header entries. The iteration pattern conflates row/variable indices (`varIdx >= num_constrs` check suggests confusion). Does not implement compensated summation. Does not record diagnostic constraint index on infeasibility. Uses `uint8_t *constrSenses` where spec uses `array-of-byte [numVars]` described as bound_class.
- **File:** `src/solver_state/helpers.c:45-200`
- **Severity:** High (multiple algorithmic deviations)

### [V25] cxf_propagate_bounds -- Missing work counter update
- **Spec says:** Accumulates computational work to the solver state's work counter for performance profiling.
- **Code does:** No work counter updates at all.
- **File:** `src/solver_state/helpers.c:45-200`
- **Severity:** Low

### [V26] cxf_simplex_crash -- Does not mark column entries as inactive (sentinel -1)
- **Spec says:** (crash_basis.md) "For each column entry in the removed row: mark as inactive by setting `S.rowColIndices[k] := -1`"
- **Code does:** Decrements col_nz_count but does NOT set `mat->col_idx[k] = -1` to mark entries as inactive. The column index entries remain unchanged.
- **File:** `src/simplex/crash.c:98-103`
- **Severity:** Medium (invariant violation -- subsequent code may process stale entries)

### [V27] cxf_simplex_crash -- Uses CSR from MatrixData instead of SolverState arrays
- **Spec says:** (crash_basis.md) The row-major sparse matrix (CSR) is part of the SolverState: `S.rowStart`, `S.rowColCount`, `S.rowColIndices`.
- **Code does:** Reads CSR data from `mat->row_ptr` and `mat->col_idx` (the model's MatrixData), not from SolverState's own CSR arrays (`state->csr_row_ptr`, `state->csr_col_idx`).
- **File:** `src/simplex/crash.c:94-103`
- **Severity:** Medium (uses model matrix instead of solver state's working copy)

---

## Missing Functions

The following spec functions have NO implementation found anywhere in the codebase:

### Threading & Synchronization (threading_sync.md)
1. **cxf_acquire_solve_lock** -- Locale save/switch to "C" before optimization
2. **cxf_release_solve_lock** -- Locale restore after optimization
3. **cxf_env_acquire_lock** -- Clear error buffer state for new API operation

### Thread Init & Thunks (thread_init_thunks.md)
4. **cxf_init_thread_local** -- Per-thread state initialization (independent RNG allocation or shared default)
5. **LeaveCriticalSection_thunk** -- Platform mutex release abstraction

### Input Validation (input_validation.md)
6. **cxf_check_env** -- Environment pointer validation with sentinel checks
7. **cxf_check_label** -- Model label attribute validation
8. **cxf_check_multiobj_scenario** -- Multi-objective scenario count query
9. **cxf_check_feasibility** -- Quick solution feasibility check with caching

### Data Validation (data_validation.md)
10. **cxf_validate_solution** -- Comprehensive solution validation with ViolationInfo output

### State Cleanup - Buffers (state_cleanup_buffers.md)
11. **cxf_free_solution_pool** -- Free concurrent environment references with ref counting
12. **cxf_clear_solution** -- Clear all solution data from model
13. **cxf_clear_pending_buffer** -- Deep free of pending modifications buffer
14. **cxf_reset_pending_buffer** -- Soft reset of pending modifications buffer

### State Cleanup - Solver (state_cleanup_solver.md)
15. **cxf_free_iis_state** -- Free IIS diagnostic data
16. **cxf_free_warmstart_basis** -- Free warm-start basis data

### State Initialization (state_initialization.md)
17. **cxf_setup_basis** -- Free warm-start data (misnomer: actually a destructor)
18. **cxf_setup_work_arrays** -- Free WorkArrays structure

### Cleanup Utilities (cleanup_utilities.md)
19. **cxf_cleanup_coeff_change** -- Free coefficient change tracker
20. **cxf_cleanup_optimization** -- Restore default signal handler after optimization

### Model Type Checking (model_type_checking.md)
21. **cxf_is_socp_internal** -- Two-stage SOCP qualification check (disqualify then confirm)

### Remote Protocol
22. **cxf_disconnect_remote** (unnamed in spec, described in state_cleanup_solver.md) -- Send disconnect message to remote solver

---

## Extra Functions (not in audited specs)

1. **cxf_validate_thread_count** (`src/threading/config.c`) -- Not in spec. Possibly intended as `cxf_set_thread_count` but behavior differs significantly.
2. **cxf_generate_seed** (`src/threading/seed.c`) -- Not in audited specs. Infrastructure utility for RNG seeding.
3. **cxf_env_lock / cxf_env_unlock** (`src/threading/locks.c`) -- Not in spec. Stub mutex functions.
4. **cxf_solver_lock / cxf_solver_unlock** (`src/threading/locks.c`) -- Not in spec. Stub mutex functions.
5. **cxf_compute_coef_stats / cxf_coefficient_stats** (`src/analysis/coef_stats.c`) -- Not in audited specs. Coefficient statistics utility.
6. **cxf_check_obvious_infeasibility** (`src/simplex/presolve.c`) -- Not in audited specs. Presolve infeasibility detection.
7. **cxf_check_obvious_unboundedness** (`src/simplex/presolve.c`) -- Not in audited specs. Presolve unboundedness detection.
8. **cxf_solve_unconstrained** (`src/simplex/presolve.c`) -- Not in audited specs.
9. **cxf_fix_variable** (`src/utilities/fix_var.c`) -- Not in audited specs.
10. **cxf_log10_wrapper, cxf_sqrt_wrapper, cxf_fabs_wrapper, cxf_floor_wrapper, cxf_ceil_wrapper, cxf_pow_wrapper, cxf_exp_wrapper** (`src/utilities/math_wrappers.c`) -- Not in audited specs. Math safety wrappers.
11. **cxf_is_multi_objective** (`src/utilities/helpers.c`) -- Not in audited specs. Stub multi-objective check.
12. **cxf_validate_pivot_element** (`src/error/pivot_check.c`) -- Not in audited specs.

---

## Notes

### Overall Assessment

The audited modules show a pattern of **early-stage stub implementations** that have the right intent but deviate significantly from spec V2 in signatures, behavior, and completeness.

**Key Themes:**

1. **22 missing functions.** The largest compliance gap. Most state cleanup, state initialization, locale management, and advanced validation functions are entirely unimplemented.

2. **Signature mismatches.** Multiple functions have completely different parameter lists from the spec. Most notably: `cxf_check_nan`, `cxf_check_is_finite`, `cxf_validate_vartypes`, `cxf_special_check`, `cxf_init_solve_state`, `cxf_cleanup_solve_state`, `cxf_check_model_flags2`. Several functions that should be Environment accessors are standalone functions.

3. **Wrong behavior in implemented functions.** `cxf_check_model_flags1` checks MIP features instead of active optimization state. `cxf_check_model_flags2` checks quadratic features instead of dual data availability. `cxf_free_basis_state` frees a BasisState instead of concurrent environments. `cxf_free_callback_state` does trivial struct cleanup instead of remote solver disconnect.

4. **Environment caching pattern not adopted.** The spec calls for hardware detection results (logical processors, physical cores) to be cached in the Environment during initialization and accessed via lightweight accessors. The implementation detects hardware on every call and does not use the Environment at all.

5. **Crash basis implementation is close.** `cxf_simplex_crash` is one of the closest implementations to its spec. The two deviations (no sentinel marking of column entries, using MatrixData CSR instead of SolverState CSR) are relatively minor but could affect downstream correctness.

6. **Bound propagation has structural issues.** The worklist seeds on variables (not constraints), the iteration conflates variable and constraint indices, and compensated summation is absent. The algorithm needs rework to match the spec's constraint-centric worklist approach.

### Priority Recommendations

- **Critical:** Implement locale management (cxf_acquire/release_solve_lock) -- required for international deployment correctness.
- **Critical:** Fix cxf_check_model_flags1/flags2 behavior -- these affect solver dispatch routing.
- **High:** Implement state cleanup functions -- memory leaks without them.
- **High:** Fix cxf_check_nan/cxf_check_is_finite signatures -- foundational validation used throughout solver.
- **Medium:** Add Environment-based hardware caching for thread count functions.
- **Medium:** Fix cxf_validate_vartypes signature and behavior.
- **Medium:** Fix cxf_simplex_crash sentinel marking.
- **Low:** Stub functions (cxf_is_quadratic, cxf_is_socp) can remain stubs until QP/SOCP features are needed.
