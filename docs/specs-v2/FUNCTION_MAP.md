# Function-to-Module Mapping

**Purpose:** Authoritative mapping of all 158 analyzed functions to their Layer 3 module assignments.
**Created:** 2026-02-06
**Source:** Cross-referenced from `src/analyzed/lp/` inventory, `data/audit/lp_functions.csv`, and `cleanroom/v2/PLAN.md` Section 6.

---

## Summary

| Metric | Count |
|--------|-------|
| Unique functions | 149 |
| Layer 3 modules | 33 |
| Multi-part functions | 8 |
| Original audit functions | 140 |
| Functions discovered during Phase 3 | 9 |

### Corrections from Plan Section 6

| Change | Detail |
|--------|--------|
| Added `cxf_is_finite` | Assigned to P3.07 (Input Validation). Was in analyzed dir but missing from plan. |
| Removed phantom `LeaveCriticalSection` | Plan listed it separately in P3.12, but no analyzed file exists. The audit's `LeaveCriticalSection` was analyzed as `LeaveCriticalSection_thunk`. |

---

## Module Assignments

### P3.01 - Memory Primitives (4 functions)

| Function | Multi-part | Notes |
|----------|-----------|-------|
| cxf_calloc | | Memory allocation with zeroing |
| cxf_realloc | | Memory reallocation |
| cxf_vector_free | | Array deallocation |
| cxf_model_alloc | | Model-level memory allocation |

**Spec file:** `specs/modules/memory_primitives.md`

---

### P3.02 - Allocation Helpers (3 functions)

| Function | Multi-part | Notes |
|----------|-----------|-------|
| cxf_alloc_eta | | Eta vector allocation |
| cxf_alloc_work_arrays | | Workspace array allocation |
| cxf_setup_resources | | Resource bundle setup |

**Spec file:** `specs/modules/allocation_helpers.md`

---

### P3.03 - State Initialization (3 functions)

| Function | Multi-part | Notes |
|----------|-----------|-------|
| cxf_init_solve_state | | Solver state initialization |
| cxf_free_warmstart_basis | | Basis state setup |
| cxf_free_work_arrays | | Work array configuration |

**Spec file:** `specs/modules/state_initialization.md`

---

### P3.04 - State Cleanup: Solver (6 functions)

| Function | Multi-part | Notes |
|----------|-----------|-------|
| cxf_cleanup_solve_state | | Solver state teardown |
| cxf_free_attribute_table | | Solver state deallocation |
| cxf_free_basis_state | | Basis state deallocation |
| cxf_free_iis_state | | IIS state deallocation |
| cxf_free_warmstart_basis | | Warm-start data deallocation (Phase 3 discovery) |

**Spec file:** `specs/modules/state_cleanup_solver.md`

---

### P3.05 - State Cleanup: Buffers (5 functions)

| Function | Multi-part | Notes |
|----------|-----------|-------|
| cxf_free_callback_state | | Callback state deallocation |
| cxf_free_solution_pool | | Solution pool deallocation |
| cxf_clear_solution | | Solution data reset |
| cxf_clear_pending_buffer | | Pending modification buffer clear |
| cxf_reset_pending_buffer | | Pending modification buffer reset |

**Spec file:** `specs/modules/state_cleanup_buffers.md`

---

### P3.06 - Model Type Checking (5 functions)

| Function | Multi-part | Notes |
|----------|-----------|-------|
| cxf_is_quadratic | | Quadratic model detection |
| cxf_is_socp | | SOCP model detection |
| cxf_is_socp_internal | | Internal SOCP classification |
| cxf_check_model_flags1 | | Model flag validation (set 1) |
| cxf_check_model_flags2 | | Model flag validation (set 2) |

**Spec file:** `specs/modules/model_type_checking.md`

---

### P3.07 - Input Validation (8 functions)

| Function | Multi-part | Notes |
|----------|-----------|-------|
| cxf_check_env | | Environment validity check |
| cxf_check_nan | | NaN detection |
| cxf_check_is_finite | | Finiteness check |
| cxf_is_finite | | Combined NaN/infinity check (Phase 3 discovery) |
| cxf_check_label | | String label validation |
| cxf_check_multiobj_scenario | | Multi-objective scenario validation |
| cxf_check_feasibility | | Solution feasibility check |

**Spec file:** `specs/modules/input_validation.md`

---

### P3.08 - Data Validation (4 functions)

| Function | Multi-part | Notes |
|----------|-----------|-------|
| cxf_validate_array | | Array data validation |
| cxf_validate_vartypes | | Variable type validation |
| cxf_validate_solution | | Solution vector validation |
| cxf_special_check | | Special-case data checks |

**Spec file:** `specs/modules/data_validation.md`

---

### P3.09 - Error Handling (4 functions)

| Function | Multi-part | Notes |
|----------|-----------|-------|
| cxf_error_env | | Environment-level error reporting |
| cxf_error_model | | Model-level error reporting |
| cxf_set_error_message | | Error message formatting (Phase 3 discovery) |
| cxf_env_set_status | | Environment status update |

**Spec file:** `specs/modules/error_handling.md`

---

### P3.10 - Logging (3 functions)

| Function | Multi-part | Notes |
|----------|-----------|-------|
| cxf_set_error_string | | Error log output |
| cxf_log | | General log output |
| cxf_register_log_callback | | Log callback registration |

**Spec file:** `specs/modules/logging.md`

---

### P3.11 - Threading & Synchronization (7 functions)

| Function | Multi-part | Notes |
|----------|-----------|-------|
| cxf_save_locale_state | | Solve lock acquisition |
| cxf_release_solve_lock | | Solve lock release |
| cxf_env_acquire_lock | | Environment lock acquisition |
| cxf_get_logical_processors | | Logical CPU count query |
| cxf_get_physical_cores | | Physical core count query |
| cxf_get_threads | | Thread count query |
| cxf_validate_thread_count | | Thread count configuration |

**Spec file:** `specs/modules/threading_sync.md`

---

### P3.12 - Thread Init & Thunks (2 functions)

| Function | Multi-part | Notes |
|----------|-----------|-------|
| cxf_init_thread_local | | Thread-local storage initialization |
| LeaveCriticalSection_thunk | | Win32 API thunk (audit name: LeaveCriticalSection) |

**Spec file:** `specs/modules/thread_init_thunks.md`

---

### P3.13 - Callbacks (6 functions)

| Function | Multi-part | Notes |
|----------|-----------|-------|
| cxf_init_callback_struct | | Callback state initialization |
| cxf_callback_terminate | | Callback-driven termination |
| cxf_pre_optimize_hook | | Pre-optimization callback invocation |
| cxf_post_optimize_hook | | Post-optimization callback invocation |
| cxf_getconstrs_callback | | Constraint retrieval via callback |
| cxf_copy_env_callbacks | | Environment callback propagation |

**Spec file:** `specs/modules/callbacks.md`

---

### P3.14 - Matrix Core (4 functions)

| Function | Multi-part | Notes |
|----------|-----------|-------|
| cxf_matrix_setup | | Matrix data structure initialization |
| cxf_prepare_row_data | | Row-major data preparation |
| cxf_build_row_major | | Row-major representation construction |
| cxf_sort_by_values | | Index array sorting |

**Spec file:** `specs/modules/matrix_core.md`

---

### P3.15 - Matrix Finalization (1 function)

| Function | Multi-part | Notes |
|----------|-----------|-------|
| cxf_finalize_row_data | 6 parts | Matrix row data finalization pipeline |

**Spec file:** `specs/modules/matrix_finalization.md`

---

### P3.16 - Basis Operations (5 functions)

| Function | Multi-part | Notes |
|----------|-----------|-------|
| cxf_fix_variables_at_bounds | | Basis refactorization |
| cxf_progress_snapshot | | Basis state snapshot |
| cxf_basis_diff | | Basis difference computation |
| cxf_basis_warm | | Warm-start basis setup |
| cxf_pivot_with_eta | | Pivot operation with eta vector update |

**Spec file:** `specs/modules/basis_operations.md`

---

### P3.17 - Pricing Core (5 functions)

| Function | Multi-part | Notes |
|----------|-----------|-------|
| cxf_pricing_candidates | | Candidate selection for pricing |
| cxf_pricing_update | | Pricing data update |
| cxf_pricing_update_var | | Variable pricing update (Phase 3 discovery) |
| cxf_pricing_update_constr | | Constraint pricing update (Phase 3 discovery) |
| cxf_pricing_invalidate | | Pricing cache invalidation |

**Spec file:** `specs/modules/pricing_core.md`

---

### P3.18 - Pricing Support (8 functions)

| Function | Multi-part | Notes |
|----------|-----------|-------|
| cxf_pricing_mark_dirty | | Mark variable pricing as stale (Phase 3 discovery) |
| cxf_pricing_mark_constr_dirty | | Mark constraint pricing as stale |
| cxf_pricing_cascade_update | | Cascading pricing update |
| cxf_pricing_end_level | | End current pricing level |
| cxf_pricing_set_level | | Set pricing level |
| cxf_pricing_get_var_stats | | Variable pricing statistics |
| cxf_pricing_get_constr_stats | | Constraint pricing statistics |
| cxf_pricing_get_constr_candidates | | Constraint candidate retrieval |

**Spec file:** `specs/modules/pricing_support.md`

---

### P3.19 - Pivot Operations (5 functions)

| Function | Multi-part | Notes |
|----------|-----------|-------|
| cxf_pivot_bound | | Bound pivot operation |
| cxf_pivot_primal | | Primal pivot operation |
| cxf_pivot_special | | Special-case pivot handling |
| cxf_pivot_check | | Pivot validity check |
| cxf_pivot_update | | Post-pivot state update |

**Spec file:** `specs/modules/pivot_operations.md`

---

### P3.20 - Simplex Iteration (5 functions)

| Function | Multi-part | Notes |
|----------|-----------|-------|
| cxf_log_iteration_progress | | Main iteration driver |
| cxf_simplex_step | | Single simplex step (pricing + ratio test) |
| cxf_simplex_step2 | | Step continuation (pivot execution) |
| cxf_simplex_step3 | | Step finalization (basis update) |
| cxf_simplex_post_iterate | | Post-iteration bookkeeping |

**Spec file:** `specs/modules/simplex_iteration.md`

---

### P3.21 - Simplex Phases (6 functions)

| Function | Multi-part | Notes |
|----------|-----------|-------|
| cxf_simplex_crash | | Crash basis construction |
| cxf_simplex_perturbation | | Perturbation for anti-cycling |
| cxf_simplex_preprocess | | Pre-solve preprocessing |
| cxf_simplex_setup | | Simplex method setup |
| cxf_simplex_phase_end | | Phase transition handling |
| cxf_simplex_refine | | Solution refinement |

**Spec file:** `specs/modules/simplex_phases.md`

---

### P3.22 - Simplex Lifecycle (3 functions)

| Function | Multi-part | Notes |
|----------|-----------|-------|
| cxf_simplex_postsolve | | Simplex state cleanup |
| cxf_simplex_final | | Final simplex result processing |
| cxf_simplex_init | 4 parts | Simplex initialization pipeline |

**Spec file:** `specs/modules/simplex_lifecycle.md`

---

### P3.23 - Crossover (2 functions)

| Function | Multi-part | Notes |
|----------|-----------|-------|
| cxf_crossover | | Barrier-to-simplex crossover driver |
| cxf_crossover_bounds | 4 parts | Bound-based crossover operations |

**Spec file:** `specs/modules/crossover.md`

---

### P3.24 - Solve Entry & Dispatch (6 functions)

| Function | Multi-part | Notes |
|----------|-----------|-------|
| cxf_optimize | | Public API entry point |
| cxf_optimize_internal | | Internal optimization dispatch |
| cxf_solve_entry | | Solve chain entry |
| cxf_solve_dispatch | | Solver method dispatch |
| cxf_solve_no_callbacks | | Solve without callback support |
| cxf_solve_with_callbacks | | Solve with callback support |

**Spec file:** `specs/modules/solve_entry.md`

---

### P3.25 - Solve LP Core (2 functions)

| Function | Multi-part | Notes |
|----------|-----------|-------|
| cxf_solve_lp | 6 parts | LP solve pipeline |
| cxf_solver_dispatch | 6 parts | Solver algorithm dispatch |

**Spec file:** `specs/modules/solve_lp_core.md`

---

### P3.26 - Solve Barrier & Concurrent (3 functions)

| Function | Multi-part | Notes |
|----------|-----------|-------|
| cxf_solve_barrier | | Interior-point method entry |
| cxf_solve_concurrent | 6 parts | Concurrent optimization pipeline |
| cxf_solve_concurrent_distributed | | Distributed concurrent solve |

**Spec file:** `specs/modules/solve_barrier_concurrent.md`

---

### P3.29 - Solution Processing (6 functions)

| Function | Multi-part | Notes |
|----------|-----------|-------|
| cxf_process_lp_solution | | LP solution post-processing |
| cxf_uncrush_solution | | Presolve reverse mapping |
| cxf_wire_result_attributes | | Result attribute population |
| cxf_compute_gap | | Optimality gap computation |
| cxf_scale_objval | | Objective value unscaling |
| cxf_copy_solution | | Solution data copy |

**Spec file:** `specs/modules/solution_processing.md`

---

### P3.30 - Environment Lifecycle (5 functions)

| Function | Multi-part | Notes |
|----------|-----------|-------|
| cxf_env_create_internal | | Environment creation |
| cxf_env_free_internal | | Environment deallocation |
| cxf_env_finalize | 8 parts | Environment finalization pipeline (licensing, etc.) |
| cxf_env_load_logfile | | Log file initialization |
| cxf_env_update_active_model | | Active model tracking |

**Spec file:** `specs/modules/environment_lifecycle.md`

---

### P3.31 - Model Lifecycle (4 functions)

| Function | Multi-part | Notes |
|----------|-----------|-------|
| cxf_model_create_internal | | Model creation |
| cxf_env_model_cleanup | | Model cleanup via environment |
| cxf_update_model_manager | | Model manager update |
| cxf_model_apply_modifications | 4 parts | Lazy modification application pipeline |

**Spec file:** `specs/modules/model_lifecycle.md`

---

### P3.32 - Optimization Preparation (3 functions)

| Function | Multi-part | Notes |
|----------|-----------|-------|
| cxf_prepare_optimization | | Pre-optimization setup |
| cxf_wait_async | | Async result waiting |

**Spec file:** `specs/modules/optimization_preparation.md`

---

### P3.33 - Statistics & Diagnostics (7 functions)

| Function | Multi-part | Notes |
|----------|-----------|-------|
| cxf_presolve_stats | | Presolve statistics |
| cxf_coefficient_stats | | Coefficient statistics |
| cxf_compute_coef_stats | | Detailed coefficient computation |
| cxf_gencon_stats | | General constraint statistics |
| cxf_compute_violations | | Constraint violation computation (Phase 3 discovery) |
| cxf_compute_fingerprint | | Model fingerprint computation |
| cxf_get_timestamp | | Timestamp retrieval |

**Spec file:** `specs/modules/statistics_diagnostics.md`

---

### P3.34 - Cleanup Utilities (4 functions)

| Function | Multi-part | Notes |
|----------|-----------|-------|
| cxf_propagate_bounds | | Cleanup helper utilities (Phase 3 discovery) |
| cxf_cleanup_coeff_change | | Coefficient change cleanup |
| cxf_cleanup_optimization | | Post-optimization cleanup |
| cxf_propagate_bounds | | Bound propagation |

**Spec file:** `specs/modules/cleanup_utilities.md`

---

### P3.35 - Query Utilities (5 functions)

| Function | Multi-part | Notes |
|----------|-----------|-------|
| cxf_get_genconstr_name | | General constraint name query |
| cxf_get_qconstr_data | | Quadratic constraint data query |
| cxf_count_genconstr_types | | General constraint type counting |
| cxf_has_history | | History availability check |
| cxf_fix_variable | | Variable fixing |

**Spec file:** `specs/modules/query_utilities.md`

---

## Multi-Part Functions

These 10 functions were too complex for single-file analysis and were decomposed into logical parts. Each has a directory under `src/analyzed/lp/` containing numbered part files, a `types.h`, and a `README.md`.

| Function | Parts | Module | Description |
|----------|-------|--------|-------------|
| cxf_crossover_bounds | 4 | P3.23 | Bound-based crossover operations |
| cxf_env_finalize | 8 | P3.30 | Environment finalization (licensing pipeline) |
| cxf_finalize_row_data | 6 | P3.15 | Matrix row data finalization |
| cxf_model_apply_modifications | 4 | P3.31 | Lazy modification application |
| cxf_simplex_init | 4 | P3.22 | Simplex initialization |
| cxf_solve_concurrent | 6 | P3.26 | Concurrent optimization |
| cxf_solve_lp | 6 | P3.25 | LP solve pipeline |
| cxf_solver_dispatch | 6 | P3.25 | Solver algorithm dispatch |

---

## Reverse Index (Alphabetical)

| Function | Module | Category |
|----------|--------|----------|
| cxf_save_locale_state | P3.11 | Threading & Sync |
| cxf_alloc_eta | P3.02 | Allocation Helpers |
| cxf_alloc_work_arrays | P3.02 | Allocation Helpers |
| cxf_basis_diff | P3.16 | Basis Operations |
| cxf_fix_variables_at_bounds | P3.16 | Basis Operations |
| cxf_progress_snapshot | P3.16 | Basis Operations |
| cxf_basis_warm | P3.16 | Basis Operations |
| cxf_build_row_major | P3.14 | Matrix Core |
| cxf_callback_terminate | P3.13 | Callbacks |
| cxf_calloc | P3.01 | Memory Primitives |
| cxf_check_env | P3.07 | Input Validation |
| cxf_check_feasibility | P3.07 | Input Validation |
| cxf_check_is_finite | P3.07 | Input Validation |
| cxf_check_label | P3.07 | Input Validation |
| cxf_check_model_flags1 | P3.06 | Model Type Checking |
| cxf_check_model_flags2 | P3.06 | Model Type Checking |
| cxf_check_multiobj_scenario | P3.07 | Input Validation |
| cxf_check_nan | P3.07 | Input Validation |
| cxf_is_finite | P3.07 | Input Validation |
| cxf_cleanup_coeff_change | P3.34 | Cleanup Utilities |
| cxf_propagate_bounds | P3.34 | Cleanup Utilities |
| cxf_cleanup_optimization | P3.34 | Cleanup Utilities |
| cxf_cleanup_solve_state | P3.04 | State Cleanup: Solver |
| cxf_clear_pending_buffer | P3.05 | State Cleanup: Buffers |
| cxf_clear_solution | P3.05 | State Cleanup: Buffers |
| cxf_coefficient_stats | P3.33 | Statistics & Diagnostics |
| cxf_compute_coef_stats | P3.33 | Statistics & Diagnostics |
| cxf_compute_fingerprint | P3.33 | Statistics & Diagnostics |
| cxf_compute_gap | P3.29 | Solution Processing |
| cxf_compute_violations | P3.33 | Statistics & Diagnostics |
| cxf_copy_env_callbacks | P3.13 | Callbacks |
| cxf_copy_solution | P3.29 | Solution Processing |
| cxf_count_genconstr_types | P3.35 | Query Utilities |
| cxf_crossover | P3.23 | Crossover |
| cxf_crossover_bounds | P3.23 | Crossover |
| cxf_env_acquire_lock | P3.11 | Threading & Sync |
| cxf_env_create_internal | P3.30 | Environment Lifecycle |
| cxf_env_finalize | P3.30 | Environment Lifecycle |
| cxf_env_free_internal | P3.30 | Environment Lifecycle |
| cxf_env_load_logfile | P3.30 | Environment Lifecycle |
| cxf_env_model_cleanup | P3.31 | Model Lifecycle |
| cxf_env_set_status | P3.09 | Error Handling |
| cxf_env_update_active_model | P3.30 | Environment Lifecycle |
| cxf_error_env | P3.09 | Error Handling |
| cxf_error_model | P3.09 | Error Handling |
| cxf_set_error_string | P3.10 | Logging |
| cxf_finalize_row_data | P3.15 | Matrix Finalization |
| cxf_fix_variable | P3.35 | Query Utilities |
| cxf_free_basis_state | P3.04 | State Cleanup: Solver |
| cxf_free_callback_state | P3.05 | State Cleanup: Buffers |
| cxf_free_iis_state | P3.04 | State Cleanup: Solver |
| cxf_free_solution_pool | P3.05 | State Cleanup: Buffers |
| cxf_free_attribute_table | P3.04 | State Cleanup: Solver |
| cxf_free_warmstart_basis | P3.04 | State Cleanup: Solver |
| cxf_gencon_stats | P3.33 | Statistics & Diagnostics |
| cxf_get_genconstr_name | P3.35 | Query Utilities |
| cxf_get_logical_processors | P3.11 | Threading & Sync |
| cxf_get_physical_cores | P3.11 | Threading & Sync |
| cxf_get_qconstr_data | P3.35 | Query Utilities |
| cxf_get_threads | P3.11 | Threading & Sync |
| cxf_get_timestamp | P3.33 | Statistics & Diagnostics |
| cxf_getconstrs_callback | P3.13 | Callbacks |
| cxf_has_history | P3.35 | Query Utilities |
| cxf_init_callback_struct | P3.13 | Callbacks |
| cxf_init_solve_state | P3.03 | State Initialization |
| cxf_init_thread_local | P3.12 | Thread Init & Thunks |
| cxf_is_quadratic | P3.06 | Model Type Checking |
| cxf_is_socp | P3.06 | Model Type Checking |
| cxf_is_socp_internal | P3.06 | Model Type Checking |
| cxf_log | P3.10 | Logging |
| cxf_matrix_setup | P3.14 | Matrix Core |
| cxf_model_alloc | P3.01 | Memory Primitives |
| cxf_model_apply_modifications | P3.31 | Model Lifecycle |
| cxf_model_create_internal | P3.31 | Model Lifecycle |
| cxf_optimize | P3.24 | Solve Entry & Dispatch |
| cxf_optimize_internal | P3.24 | Solve Entry & Dispatch |
| cxf_pivot_bound | P3.19 | Pivot Operations |
| cxf_pivot_check | P3.19 | Pivot Operations |
| cxf_pivot_primal | P3.19 | Pivot Operations |
| cxf_pivot_special | P3.19 | Pivot Operations |
| cxf_pivot_update | P3.19 | Pivot Operations |
| cxf_pivot_with_eta | P3.16 | Basis Operations |
| cxf_post_optimize_hook | P3.13 | Callbacks |
| cxf_pre_optimize_hook | P3.13 | Callbacks |
| cxf_prepare_optimization | P3.32 | Optimization Preparation |
| cxf_prepare_row_data | P3.14 | Matrix Core |
| cxf_presolve_stats | P3.33 | Statistics & Diagnostics |
| cxf_pricing_candidates | P3.17 | Pricing Core |
| cxf_pricing_cascade_update | P3.18 | Pricing Support |
| cxf_pricing_end_level | P3.18 | Pricing Support |
| cxf_pricing_get_constr_candidates | P3.18 | Pricing Support |
| cxf_pricing_get_constr_stats | P3.18 | Pricing Support |
| cxf_pricing_get_var_stats | P3.18 | Pricing Support |
| cxf_pricing_invalidate | P3.17 | Pricing Core |
| cxf_pricing_mark_constr_dirty | P3.18 | Pricing Support |
| cxf_pricing_mark_dirty | P3.18 | Pricing Support |
| cxf_pricing_set_level | P3.18 | Pricing Support |
| cxf_pricing_update | P3.17 | Pricing Core |
| cxf_pricing_update_constr | P3.17 | Pricing Core |
| cxf_pricing_update_var | P3.17 | Pricing Core |
| cxf_process_lp_solution | P3.29 | Solution Processing |
| cxf_propagate_bounds | P3.34 | Cleanup Utilities |
| cxf_realloc | P3.01 | Memory Primitives |
| cxf_register_log_callback | P3.10 | Logging |
| cxf_release_solve_lock | P3.11 | Threading & Sync |
| cxf_reset_pending_buffer | P3.05 | State Cleanup: Buffers |
| cxf_scale_objval | P3.29 | Solution Processing |
| cxf_set_error_message | P3.09 | Error Handling |
| cxf_validate_thread_count | P3.11 | Threading & Sync |
| cxf_free_warmstart_basis | P3.03 | State Initialization |
| cxf_setup_resources | P3.02 | Allocation Helpers |
| cxf_free_work_arrays | P3.03 | State Initialization |
| cxf_simplex_postsolve | P3.22 | Simplex Lifecycle |
| cxf_simplex_crash | P3.21 | Simplex Phases |
| cxf_simplex_final | P3.22 | Simplex Lifecycle |
| cxf_simplex_init | P3.22 | Simplex Lifecycle |
| cxf_log_iteration_progress | P3.20 | Simplex Iteration |
| cxf_simplex_perturbation | P3.21 | Simplex Phases |
| cxf_simplex_phase_end | P3.21 | Simplex Phases |
| cxf_simplex_post_iterate | P3.20 | Simplex Iteration |
| cxf_simplex_preprocess | P3.21 | Simplex Phases |
| cxf_simplex_refine | P3.21 | Simplex Phases |
| cxf_simplex_setup | P3.21 | Simplex Phases |
| cxf_simplex_step | P3.20 | Simplex Iteration |
| cxf_simplex_step2 | P3.20 | Simplex Iteration |
| cxf_simplex_step3 | P3.20 | Simplex Iteration |
| cxf_solve_barrier | P3.26 | Solve Barrier & Concurrent |
| cxf_solve_concurrent | P3.26 | Solve Barrier & Concurrent |
| cxf_solve_concurrent_distributed | P3.26 | Solve Barrier & Concurrent |
| cxf_solve_dispatch | P3.24 | Solve Entry & Dispatch |
| cxf_solve_entry | P3.24 | Solve Entry & Dispatch |
| cxf_solve_lp | P3.25 | Solve LP Core |
| cxf_solve_no_callbacks | P3.24 | Solve Entry & Dispatch |
| cxf_solve_with_callbacks | P3.24 | Solve Entry & Dispatch |
| cxf_solver_dispatch | P3.25 | Solve LP Core |
| cxf_sort_by_values | P3.14 | Matrix Core |
| cxf_special_check | P3.08 | Data Validation |
| cxf_uncrush_solution | P3.29 | Solution Processing |
| cxf_update_model_manager | P3.31 | Model Lifecycle |
| cxf_validate_array | P3.08 | Data Validation |
| cxf_validate_solution | P3.08 | Data Validation |
| cxf_validate_vartypes | P3.08 | Data Validation |
| cxf_vector_free | P3.01 | Memory Primitives |
| cxf_wait_async | P3.32 | Optimization Preparation |
| cxf_wire_result_attributes | P3.29 | Solution Processing |
| LeaveCriticalSection_thunk | P3.12 | Thread Init & Thunks |

---

## Module Size Distribution

| Size | Modules | IDs |
|------|---------|-----|
| 1-2 functions | 4 | P3.12, P3.15, P3.23, P3.25 |
| 3 functions | 6 | P3.02, P3.03, P3.10, P3.22, P3.26, P3.32 |
| 4 functions | 6 | P3.01, P3.08, P3.09, P3.14, P3.31, P3.34 |
| 5 functions | 8 | P3.05, P3.06, P3.16, P3.17, P3.19, P3.20, P3.30, P3.35 |
| 6 functions | 5 | P3.04, P3.13, P3.21, P3.24, P3.29 |
| 7 functions | 2 | P3.11, P3.33 |
| 8 functions | 2 | P3.07, P3.18 |

---

## Provenance Notes

### Functions Discovered During Phase 3 (9 beyond original audit)

These functions were not in the original 149-function audit but were identified during Phase 3 analysis of related functions:

| Function | Module | Discovery Context |
|----------|--------|-------------------|
| cxf_is_finite | P3.07 | IEEE 754 helper, related to cxf_check_nan/cxf_check_is_finite |
| cxf_propagate_bounds | P3.34 | Helper discovered during cleanup analysis |
| cxf_compute_violations | P3.33 | Discovered during solution processing analysis |
| cxf_free_warmstart_basis | P3.04 | Discovered during state cleanup analysis |
| cxf_pricing_mark_dirty | P3.18 | Discovered during pricing system analysis |
| cxf_pricing_update_constr | P3.17 | Discovered during pricing system analysis |
| cxf_pricing_update_var | P3.17 | Discovered during pricing system analysis |
| cxf_set_error_message | P3.09 | Discovered during error handling analysis |
| cxf_solve_multiscenario | P3.28 | Discovered during multi-objective analysis |

### Audit Name Corrections

| Audit Name | Analyzed Name | Reason |
|------------|---------------|--------|
| LeaveCriticalSection | LeaveCriticalSection_thunk | Clarified as IAT thunk during analysis |
