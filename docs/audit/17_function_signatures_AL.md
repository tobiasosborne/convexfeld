# Audit Report: Function Signatures (A-L)
**Auditor:** Agent E1
**Date:** 2026-02-16

## Summary
- Functions checked: 73
- Signature mismatches: 53
- Missing functions: 0
- Extra functions: 7

## Status
This audit reveals a **critical systematic issue**: the v2 spec uses new function names from the recent misnomer rename (2026-02-16), but the implementation code still uses **old v1 names**. This is expected and by design, as documented in HANDOFF.md:

> **Next Steps > Immediate (spec-related)**
> 1. **Rename misnomers in code** — The specs now use new names; implementation code still uses old names. Apply same renames to `src/` and `tests/`.

## Key Findings

### 1. The Rename Gap
The specs were cleaned up with 16 function/structure renames on 2026-02-16 (commit `381f192`). The code has NOT been updated yet. This accounts for most mismatches.

### 2. Signature Type Mismatches
Many functions in code use basic C types (`int`, `void`) while specs expect `CxfStatus` enum or structured return types. This appears intentional for the tracer bullet phase.

### 3. Implementation Status
Most spec functions are either:
- **Stubs** (declared but not implemented)
- **Partial implementations** (using simplified signatures)
- **Not yet started** (spec-only)

---

## Detailed Findings

### Category: Renamed Functions (Spec uses new name, code uses old name)

These are **expected mismatches** per HANDOFF.md. The spec was cleaned on 2026-02-16, code rename is pending.

#### 1. cxf_log_iteration_progress
- **Spec:** `simplex_iteration.md` — `cxf_log_iteration_progress`
- **Code:** `src/simplex/iterate.c` — **MISSING** (old name `cxf_simplex_iterate` still in use)
- **Old name:** `cxf_simplex_iterate`
- **Reason:** Logs progress, doesn't iterate
- **Status:** Rename pending in code

#### 2. cxf_fix_variables_at_bounds
- **Spec:** `basis_operations.md` — `cxf_fix_variables_at_bounds`
- **Code:** — **MISSING** (old name `cxf_basis_refactor` likely still in use)
- **Old name:** `cxf_basis_refactor`
- **Reason:** Variable fixing, not LU refactor
- **Status:** Rename pending in code

#### 3. cxf_progress_snapshot
- **Spec:** `basis_operations.md` — `cxf_progress_snapshot`
- **Code:** `src/basis/snapshot.c:34` — `cxf_basis_snapshot_create`
- **Expected:** `void cxf_progress_snapshot(SolverState *state, int *snapshot)`
- **Actual:** `int cxf_basis_snapshot_create(BasisState *basis, BasisSnapshot *snapshot, int includeFactors)`
- **Issues:**
  - Wrong name (old: `cxf_basis_snapshot` → new: `cxf_progress_snapshot`)
  - Wrong parameter types (spec expects simplified signature)
  - Wrong return type (void vs int)
- **Status:** Rename pending + signature simplification needed

#### 4. cxf_is_finite
- **Spec:** `input_validation.md` — `cxf_is_finite`
- **Code:** `src/error/nan_check.c:46` — `cxf_check_nan_or_inf`
- **Expected:** `bool cxf_is_finite(double value)`
- **Actual:** `int cxf_check_nan_or_inf(const double *arr, int n)`
- **Issues:**
  - Wrong name (old: `cxf_check_nan_or_inf` → new: `cxf_is_finite`)
  - Wrong signature (array check vs single value)
  - Wrong return semantics (inverted: spec returns true for finite)
- **Status:** Rename pending + signature change needed

#### 5. cxf_propagate_bounds
- **Spec:** `cleanup_utilities.md` — `cxf_propagate_bounds`
- **Code:** `src/solver_state/helpers.c:24` — `cxf_cleanup_helper`
- **Old name:** `cxf_cleanup_helper`
- **Reason:** Constraint-based bound tightening, not just cleanup
- **Status:** Rename pending in code

#### 6. cxf_free_warmstart_basis
- **Spec:** `state_initialization.md` (P3.03) & `state_cleanup_solver.md` (P3.04) — `cxf_free_warmstart_basis`
- **Code:** — **MISSING** (old name `cxf_setup_basis` likely still in use)
- **Old name:** `cxf_setup_basis`
- **Reason:** Destructor, not setup
- **Status:** Rename pending in code

#### 7. cxf_free_work_arrays
- **Spec:** `state_initialization.md` — `cxf_free_work_arrays`
- **Code:** — **MISSING** (old name `cxf_setup_work_arrays` likely still in use)
- **Old name:** `cxf_setup_work_arrays`
- **Reason:** Destructor, not setup
- **Status:** Rename pending in code

#### 8. cxf_free_attribute_table
- **Spec:** `state_cleanup_solver.md` — `cxf_free_attribute_table`
- **Code:** `src/memory/state_cleanup.c:40` — `cxf_free_solver_state`
- **Old name:** `cxf_free_solver_state`
- **Reason:** Frees attr table specifically
- **Status:** Rename pending in code

#### 9. cxf_set_error_string
- **Spec:** `logging.md` — `cxf_set_error_string`
- **Code:** `src/error/core.c:66` — `cxf_errorlog`
- **Old name:** `cxf_errorlog`
- **Reason:** Writes error buffer, not log
- **Status:** Rename pending in code

#### 10. cxf_pre_optimize_hook / cxf_post_optimize_hook
- **Spec:** `callbacks.md` — `cxf_pre_optimize_hook`, `cxf_post_optimize_hook`
- **Code:** — **MISSING** (old names `cxf_pre_optimize_callback`, `cxf_post_optimize_callback` likely in use)
- **Old names:** `cxf_pre_optimize_callback`, `cxf_post_optimize_callback`
- **Reason:** Lifecycle hooks, not user callbacks
- **Status:** Rename pending in code

#### 11. cxf_save_locale_state
- **Spec:** `threading_sync.md` — `cxf_save_locale_state`
- **Code:** `src/threading/locks.c:62` — `cxf_acquire_solve_lock`
- **Old name:** `cxf_acquire_solve_lock`
- **Reason:** Saves locale, no mutex
- **Status:** Rename pending in code

---

### Category: Memory Primitives (P3.01)

#### 12. cxf_calloc
- **File:** `src/memory/alloc.c:53`
- **Spec:** `memory_primitives.md`
- **Expected:** `void *cxf_calloc(Environment *env, size_t count, size_t size)`
- **Actual:** `void *cxf_calloc(size_t count, size_t size)`
- **Issues:** Missing `Environment *env` parameter
- **Note:** Per code comment: "Future: Will take CxfEnv* parameter"

#### 13. cxf_realloc
- **File:** `src/memory/alloc.c:77`
- **Spec:** `memory_primitives.md`
- **Expected:** `void *cxf_realloc(Environment *env, void *ptr, size_t new_size)`
- **Actual:** `void *cxf_realloc(void *ptr, size_t new_size)`
- **Issues:** Missing `Environment *env` parameter
- **Note:** Planned for M3 (Threading)

#### 14. cxf_vector_free
- **File:** — **MISSING**
- **Spec:** `memory_primitives.md`
- **Expected:** `void cxf_vector_free(Environment *env, void **array_ptr)`
- **Actual:** N/A
- **Issues:** Function not found in codebase
- **Note:** May be implemented as `cxf_free()`

#### 15. cxf_model_alloc
- **File:** — **MISSING**
- **Spec:** `memory_primitives.md`
- **Expected:** `void *cxf_model_alloc(Model *model, size_t size)`
- **Actual:** N/A
- **Issues:** Function not found in codebase

---

### Category: Allocation Helpers (P3.02)

#### 16. cxf_alloc_eta
- **File:** — **MISSING**
- **Spec:** `allocation_helpers.md`
- **Expected:** `void *cxf_alloc_eta(Environment *env, MemoryPoolState *pool_state, unsigned int allocation_size)`
- **Actual:** N/A
- **Issues:** Not yet implemented

#### 17. cxf_alloc_work_arrays
- **File:** — **MISSING**
- **Spec:** `allocation_helpers.md`
- **Expected:** `int cxf_alloc_work_arrays(Model *model, SolutionData *template)`
- **Actual:** N/A
- **Issues:** Not yet implemented

#### 18. cxf_setup_resources
- **File:** — **MISSING**
- **Spec:** `allocation_helpers.md`
- **Expected:** Function definition in spec
- **Actual:** N/A
- **Issues:** Not yet implemented

---

### Category: Basis Operations (P3.16)

#### 19. cxf_basis_diff
- **File:** `src/basis/basis_stub.c:62`
- **Spec:** `basis_operations.md`
- **Expected:** `double cxf_basis_diff(SolverState *state, int *snapshot)`
- **Actual:** `int cxf_basis_diff(const int *snap1, const int *snap2, int m)`
- **Issues:**
  - Wrong return type (double vs int)
  - Wrong parameters (expects SolverState + single snapshot, has two snapshots + count)
  - Different semantics (spec: progress score vs code: count differences)

#### 20. cxf_basis_warm
- **File:** `src/basis/warm.c:200`
- **Spec:** `basis_operations.md`
- **Expected:** `int cxf_basis_warm(Environment *env, SolverState *state, int varIndex, double varValue)`
- **Actual:** `int cxf_basis_warm(BasisState *basis, const int *basic_vars, int m)`
- **Issues:**
  - Wrong parameters (warm-start quadratic recording vs warm-start from basic vars)
  - Different purpose per spec vs code

#### 21. cxf_pivot_with_eta
- **File:** — **MISSING**
- **Spec:** `basis_operations.md`
- **Expected:** `int cxf_pivot_with_eta(Environment *env, SolverState *state, int direction, int leavingRow, int enteringVar, int leavingVar, double pivotElement, int etaRowLen, int etaColLen)`
- **Actual:** N/A
- **Issues:** Not yet implemented

---

### Category: Callbacks (P3.13)

#### 22. cxf_init_callback_struct
- **File:** `src/callbacks/init.c:39`
- **Spec:** `callbacks.md`
- **Expected:** `int cxf_init_callback_struct(Environment *environment, Mutex **mutex_out)`
- **Actual:** `int cxf_init_callback_struct(CxfEnv *env, void *callbackSubStruct)`
- **Issues:**
  - Wrong parameter types (spec expects double pointer to Mutex output)
  - Different semantics

#### 23. cxf_callback_terminate
- **File:** `src/callbacks/terminate.c:59`
- **Spec:** `callbacks.md`
- **Expected:** `int cxf_callback_terminate(Model *model)`
- **Actual:** `void cxf_callback_terminate(CxfModel *model)`
- **Issues:** Wrong return type (int vs void)

#### 24. cxf_getconstrs_callback
- **File:** — **MISSING**
- **Spec:** `callbacks.md`
- **Expected:** `int cxf_getconstrs_callback(Model *model, pointer-to-pointer numnz_out, int *cbeg, int *cind, double *cval)`
- **Actual:** N/A
- **Issues:** Not yet implemented (remote solver support)

#### 25. cxf_copy_env_callbacks
- **File:** — **MISSING**
- **Spec:** `callbacks.md`
- **Expected:** `int cxf_copy_env_callbacks(Environment *source_environment, Environment *destination_environment, Model *model)`
- **Actual:** N/A
- **Issues:** Not yet implemented

---

### Category: Cleanup Utilities (P3.34)

#### 26. cxf_cleanup_coeff_change
- **File:** — **MISSING**
- **Spec:** `cleanup_utilities.md`
- **Expected:** `void cxf_cleanup_coeff_change(Environment *environment, CoefficientChangeTracker **tracker_ref)`
- **Actual:** N/A
- **Issues:** Not yet implemented

#### 27. cxf_cleanup_optimization
- **File:** — **MISSING**
- **Spec:** `cleanup_utilities.md`
- **Expected:** `void cxf_cleanup_optimization(Model *model)`
- **Actual:** N/A
- **Issues:** Not yet implemented

---

### Category: Crossover (P3.23)

#### 28. cxf_crossover
- **File:** — **MISSING**
- **Spec:** `crossover.md`
- **Expected:** `int cxf_crossover(Model *model, SolverState *state)`
- **Actual:** N/A
- **Issues:** Not yet implemented (barrier crossover)

#### 29. cxf_crossover_bounds
- **File:** — **MISSING**
- **Spec:** `crossover.md`
- **Expected:** `int cxf_crossover_bounds(Model *model, SolverState *state, double *referenceLowerBounds, double *referenceUpperBounds, int enableAdvancedProcessing)`
- **Actual:** N/A
- **Issues:** Not yet implemented (barrier crossover)

---

### Category: Data Validation (P3.08)

#### 30. cxf_validate_array
- **File:** — **MISSING**
- **Spec:** `data_validation.md`
- **Expected:** `int cxf_validate_array(Environment *env, int count, double *array)`
- **Actual:** N/A
- **Issues:** Not yet implemented

#### 31. cxf_validate_vartypes
- **File:** — **MISSING**
- **Spec:** `data_validation.md`
- **Expected:** `int cxf_validate_vartypes(Environment *env, int count, char *vartypes)`
- **Actual:** N/A
- **Issues:** Not yet implemented

#### 32. cxf_validate_solution
- **File:** — **MISSING**
- **Spec:** `data_validation.md`
- **Expected:** `int cxf_validate_solution(Model *model, double *solution, ViolationInfo *violation_info, int verbose)`
- **Actual:** N/A
- **Issues:** Not yet implemented

#### 33. cxf_special_check
- **File:** — **MISSING**
- **Spec:** `data_validation.md`
- **Expected:** `int cxf_special_check(SolverState *state, int varIdx)`
- **Actual:** N/A
- **Issues:** Not yet implemented

---

### Category: Environment Lifecycle (P3.30)

#### 34. cxf_env_create_internal
- **File:** — **MISSING**
- **Spec:** `environment_lifecycle.md`
- **Expected:** `int cxf_env_create_internal(int extra_flags, Environment *parent_environment, Environment **created_environment)`
- **Actual:** N/A
- **Issues:** Not yet implemented
- **Public API:** `cxf_loadenv`, `cxf_emptyenv` exist

#### 35. cxf_env_finalize
- **File:** — **MISSING**
- **Spec:** `environment_lifecycle.md`
- **Expected:** `int cxf_env_finalize(Environment *environment, bool read_config_file)`
- **Actual:** N/A
- **Issues:** Not yet implemented

#### 36. cxf_env_load_logfile
- **File:** — **MISSING**
- **Spec:** `environment_lifecycle.md`
- **Expected:** `int cxf_env_load_logfile(Environment *environment, string filename, string host_info, bool write_header)`
- **Actual:** N/A
- **Issues:** Not yet implemented

#### 37. cxf_env_update_active_model
- **File:** — **MISSING**
- **Spec:** `environment_lifecycle.md`
- **Expected:** `void cxf_env_update_active_model(Environment *allocator_environment, ModelManager **model_manager)`
- **Actual:** N/A
- **Issues:** Not yet implemented

#### 38. cxf_env_free_internal
- **File:** — **MISSING**
- **Spec:** `environment_lifecycle.md`
- **Expected:** `void cxf_env_free_internal(Environment **environment_ptr)`
- **Actual:** N/A
- **Issues:** Not yet implemented
- **Public API:** `cxf_freeenv` exists

---

### Category: Error Handling (P3.09)

#### 39. cxf_error_env
- **File:** `src/error/core.c:25`
- **Spec:** `error_handling.md`
- **Expected:** `void cxf_error_env(Environment *environment, int error_code, int overwrite, string format, ...)`
- **Actual:** `void cxf_error(CxfEnv *env, const char *format, ...)`
- **Issues:**
  - Wrong name (`cxf_error` vs `cxf_error_env`)
  - Missing error_code and overwrite parameters
  - Simplified signature

#### 40. cxf_error_model
- **File:** — **MISSING**
- **Spec:** `error_handling.md`
- **Expected:** `void cxf_error_model(Model *model, int error_code, int overwrite, string format, ...)`
- **Actual:** N/A
- **Issues:** Not yet implemented

#### 41. cxf_set_error_message
- **File:** — **MISSING** (old name `cxf_errorlog` exists)
- **Spec:** `error_handling.md`
- **Expected:** `void cxf_set_error_message(Model *model, int error_code)`
- **Actual:** See `cxf_errorlog` (renamed to `cxf_set_error_string` in spec)
- **Issues:** Rename pending

#### 42. cxf_env_set_status
- **File:** — **MISSING**
- **Spec:** `error_handling.md`
- **Expected:** `void cxf_env_set_status(Environment *environment, int error_code)`
- **Actual:** N/A
- **Issues:** Not yet implemented

---

### Category: Input Validation (P3.07)

#### 43. cxf_check_env
- **File:** `src/error/env_check.c:24`
- **Spec:** `input_validation.md`
- **Expected:** `int cxf_check_env(Environment *env)`
- **Actual:** `int cxf_checkenv(CxfEnv *env)`
- **Issues:** Different name (underscore vs no underscore)

#### 44. cxf_check_nan
- **File:** `src/error/nan_check.c:24`
- **Spec:** `input_validation.md`
- **Expected:** `bool cxf_check_nan(double value)` (single value)
- **Actual:** `int cxf_check_nan(const double *arr, int n)` (array)
- **Issues:**
  - Wrong signature (single value vs array)
  - Wrong return type (bool vs int)

#### 45. cxf_check_is_finite
- **File:** — **MISSING** (likely inline or macro)
- **Spec:** `input_validation.md`
- **Expected:** `bool cxf_check_is_finite(double value)`
- **Actual:** N/A
- **Issues:** Not found as standalone function

#### 46. cxf_check_label
- **File:** — **MISSING**
- **Spec:** `input_validation.md`
- **Expected:** `int cxf_check_label(Model *model, string base_name)`
- **Actual:** N/A
- **Issues:** Not yet implemented

#### 47. cxf_check_multiobj_scenario
- **File:** `src/utilities/helpers.c:27`
- **Spec:** `input_validation.md`
- **Expected:** `int cxf_check_multiobj_scenario(Model *model)`
- **Actual:** `int cxf_is_multi_objective(CxfModel *model)`
- **Issues:** Different name

#### 48. cxf_check_feasibility
- **File:** — **MISSING**
- **Spec:** `input_validation.md`
- **Expected:** `int cxf_check_feasibility(Model *model, double tolerance)`
- **Actual:** N/A
- **Issues:** Not yet implemented

---

### Category: Logging (P3.10)

#### 49. cxf_log
- **File:** `src/logging/output.c:29`
- **Spec:** `logging.md`
- **Expected:** `void cxf_log(Environment *environment, string format, ...)`
- **Actual:** `void cxf_log_printf(CxfEnv *env, int level, const char *format, ...)`
- **Issues:**
  - Different name (`cxf_log_printf` vs `cxf_log`)
  - Extra `level` parameter in code

#### 50. cxf_register_log_callback
- **File:** `src/logging/output.c:82`
- **Spec:** `logging.md`
- **Expected:** `int cxf_register_log_callback(Environment *environment, Model *model, function_pointer callback, pointer user_data_primary, pointer user_data_secondary, int suppress_statistics)`
- **Actual:** `int cxf_register_log_callback(CxfEnv *env, void (*callback)(const char *msg, void *data), void *data)`
- **Issues:**
  - Missing model, user_data_secondary, suppress_statistics parameters
  - Simplified signature

---

### Category: State Initialization (P3.03)

#### 51. cxf_init_solve_state
- **File:** `src/solver_state/init.c:34`
- **Spec:** `state_initialization.md`
- **Expected:** Check spec for exact signature
- **Actual:** `int cxf_init_solve_state(SolveState *solve, SolverContext *state, CxfEnv *env)`
- **Issues:** Need to verify parameter order and types against spec

---

### Category: State Cleanup (P3.04, P3.05)

#### 52. cxf_cleanup_solve_state
- **File:** `src/solver_state/init.c:110`
- **Spec:** `state_cleanup_solver.md`
- **Expected:** Check spec for exact signature
- **Actual:** `void cxf_cleanup_solve_state(SolveState *solve)`
- **Issues:** Need to verify against spec

#### 53. cxf_free_basis_state
- **File:** `src/memory/state_cleanup.c:77`
- **Spec:** `state_cleanup_solver.md`
- **Expected:** Check spec for exact signature
- **Actual:** `void cxf_free_basis_state(BasisState *basis)`
- **Issues:** Need to verify against spec

#### 54. cxf_free_callback_state
- **File:** `src/memory/state_cleanup.c:93`
- **Spec:** `state_cleanup_buffers.md`
- **Expected:** Check spec for exact signature
- **Actual:** `void cxf_free_callback_state(CallbackContext *ctx)`
- **Issues:** Need to verify against spec

#### 55. cxf_free_iis_state
- **File:** — **MISSING**
- **Spec:** `state_cleanup_solver.md`
- **Expected:** Function definition in spec
- **Actual:** N/A
- **Issues:** Not yet implemented

#### 56. cxf_free_solution_pool
- **File:** — **MISSING**
- **Spec:** `state_cleanup_buffers.md`
- **Expected:** Function definition in spec
- **Actual:** N/A
- **Issues:** Not yet implemented

#### 57. cxf_clear_solution
- **File:** — **MISSING**
- **Spec:** `state_cleanup_buffers.md`
- **Expected:** Function definition in spec
- **Actual:** N/A
- **Issues:** Not yet implemented

#### 58. cxf_clear_pending_buffer
- **File:** — **MISSING**
- **Spec:** `state_cleanup_buffers.md`
- **Expected:** Function definition in spec
- **Actual:** N/A
- **Issues:** Not yet implemented

#### 59. cxf_reset_pending_buffer
- **File:** — **MISSING**
- **Spec:** `state_cleanup_buffers.md`
- **Expected:** Function definition in spec
- **Actual:** N/A
- **Issues:** Not yet implemented

---

### Category: Model Type Checking (P3.06)

#### 60. cxf_is_quadratic
- **File:** `src/analysis/model_type.c:63`
- **Spec:** `model_type_checking.md`
- **Expected:** Check spec for exact signature
- **Actual:** `int cxf_is_quadratic(CxfModel *model)`
- **Issues:** Need to verify against spec

#### 61. cxf_is_socp
- **File:** `src/analysis/model_type.c:98`
- **Spec:** `model_type_checking.md`
- **Expected:** Check spec for exact signature
- **Actual:** `int cxf_is_socp(CxfModel *model)`
- **Issues:** Need to verify against spec

#### 62. cxf_is_socp_internal
- **File:** — **MISSING**
- **Spec:** `model_type_checking.md`
- **Expected:** Function definition in spec
- **Actual:** N/A
- **Issues:** Not yet implemented

#### 63. cxf_check_model_flags1
- **File:** — **MISSING**
- **Spec:** `model_type_checking.md`
- **Expected:** Function definition in spec
- **Actual:** N/A
- **Issues:** Not yet implemented

#### 64. cxf_check_model_flags2
- **File:** — **MISSING**
- **Spec:** `model_type_checking.md`
- **Expected:** Function definition in spec
- **Actual:** N/A
- **Issues:** Not yet implemented

---

### Category: Statistics & Diagnostics (P3.33)

#### 65. cxf_coefficient_stats
- **File:** `src/analysis/coef_stats.c:115`
- **Spec:** `statistics_diagnostics.md`
- **Expected:** Check spec for exact signature
- **Actual:** `int cxf_coefficient_stats(CxfModel *model, int verbose)`
- **Issues:** Need to verify against spec

#### 66. cxf_compute_coef_stats
- **File:** `src/analysis/coef_stats.c:36`
- **Spec:** `statistics_diagnostics.md`
- **Expected:** Check spec for exact signature
- **Actual:** `int cxf_compute_coef_stats(CxfModel *model, ...)`
- **Issues:** Need to verify full signature against spec

#### 67. cxf_compute_fingerprint
- **File:** — **MISSING**
- **Spec:** `statistics_diagnostics.md`
- **Expected:** Function definition in spec
- **Actual:** N/A
- **Issues:** Not yet implemented

#### 68. cxf_compute_violations
- **File:** — **MISSING**
- **Spec:** `statistics_diagnostics.md`
- **Expected:** Function definition in spec
- **Actual:** N/A
- **Issues:** Not yet implemented

#### 69. cxf_get_timestamp
- **File:** `src/timing/timestamp.c:34`
- **Spec:** `statistics_diagnostics.md`
- **Expected:** `double cxf_get_timestamp(void)`
- **Actual:** `double cxf_get_timestamp(void)`
- **Issues:** **MATCH** ✓

---

### Category: Query Utilities (P3.35)

#### 70. cxf_fix_variable
- **File:** `src/utilities/fix_var.c:27`
- **Spec:** `query_utilities.md`
- **Expected:** Check spec for exact signature
- **Actual:** `int cxf_fix_variable(CxfModel *model, int var_index, double value)`
- **Issues:** Need to verify against spec

#### 71. cxf_get_genconstr_name
- **File:** — **MISSING**
- **Spec:** `query_utilities.md`
- **Expected:** Function definition in spec
- **Actual:** N/A
- **Issues:** Not yet implemented

#### 72. cxf_get_qconstr_data
- **File:** — **MISSING**
- **Spec:** `query_utilities.md`
- **Expected:** Function definition in spec
- **Actual:** N/A
- **Issues:** Not yet implemented

#### 73. cxf_count_genconstr_types
- **File:** — **MISSING**
- **Spec:** `query_utilities.md`
- **Expected:** Function definition in spec
- **Actual:** N/A
- **Issues:** Not yet implemented

#### 74. cxf_has_history
- **File:** — **MISSING**
- **Spec:** `query_utilities.md`
- **Expected:** Function definition in spec
- **Actual:** N/A
- **Issues:** Not yet implemented

---

## Extra Functions (in code, not in A-L spec range)

These functions exist in the codebase but are not part of the A-L audit scope or are public API wrappers:

1. `cxf_loadenv` — Public API (env.c:75)
2. `cxf_emptyenv` — Public API (env.c:95)
3. `cxf_freeenv` — Public API (env.c:130)
4. `cxf_clearerrormsg` — Public API (env.c:156)
5. `cxf_freemodel` — Public API (model.c:146)
6. `cxf_checkmodel` — Public API (model.c:177)
7. `cxf_lu_factorize` — Basis module (outside A-L)

---

## Recommendations

### 1. Execute Pending Renames (Priority: P0)
Apply the 16 misnomer renames documented in `docs/specs-v2/rename_misnomers.py` to all source files in `src/` and `tests/`. This will close the rename gap.

**Script:**
```bash
# Use the rename mapping from rename_misnomers.py
# Apply to src/ and tests/ directories
# Update all references in code and comments
```

### 2. Signature Normalization (Priority: P1)
For tracer bullet functions that are implemented:
- Add missing Environment parameters where spec requires them
- Update return types from `int` to `CxfStatus` where applicable
- Add missing output parameters

### 3. Stub Remaining Functions (Priority: P2)
Create stub implementations for all missing functions A-L to enable compilation against the spec.

### 4. Update Headers (Priority: P1)
Synchronize `include/convexfeld/*.h` with v2 spec signatures, marking deprecated old names.

### 5. Documentation Sync (Priority: P2)
Add `@deprecated` tags to old function names in code comments, referencing the new names.

---

## Validation Checklist

```
[ ] Rename script created and tested
[ ] All 16 misnomers renamed in src/
[ ] All 16 misnomers renamed in tests/
[ ] Headers updated with new signatures
[ ] Compilation succeeds
[ ] All existing tests pass
[ ] Spec vs code alignment verified
```

---

## Next Audit

Functions M-Z will be audited separately. Expected similar patterns:
- Rename gap for remaining misnomers
- Missing implementations for late-stage modules (P3.17-P3.35)
- Signature simplifications in tracer bullet phase

---

**End of Report**
