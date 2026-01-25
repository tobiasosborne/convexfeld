# Master Function Inventory

**Total Functions:** 142 (112 internal + 30 API)
**Total Lines:** 65,863

## Summary by Module

| Module | Count | Simple | Medium | Complex |
|--------|-------|--------|--------|--------|
| Basis Operations | 8 | 0 | 0 | 8 |
| Callbacks | 6 | 0 | 0 | 6 |
| Crossover | 2 | 0 | 0 | 2 |
| Error Handling | 10 | 0 | 1 | 9 |
| Logging | 5 | 0 | 0 | 5 |
| Matrix Operations | 7 | 0 | 1 | 6 |
| Memory Management | 9 | 0 | 0 | 9 |
| Model API | 30 | 0 | 3 | 27 |
| Model Analysis | 6 | 0 | 2 | 4 |
| Parameters | 4 | 0 | 3 | 1 |
| Pricing | 6 | 0 | 2 | 4 |
| Simplex Core | 21 | 0 | 0 | 21 |
| Solver State | 4 | 0 | 0 | 4 |
| Threading | 7 | 0 | 1 | 6 |
| Timing | 5 | 0 | 0 | 5 |
| Utilities | 10 | 0 | 0 | 10 |
| Validation | 2 | 0 | 0 | 2 |

## Model API

**Functions:** 30

| Function | Purpose | Complexity | Calls | Called By |
|----------|---------|------------|-------|----------|
| cxf_addconstr | Add single constraint to model | complex | cxf_addvar, cxf_calloc, cxf_check_nan, cxf_check_nan_or_inf, cxf_error | cxf_addqconstr, cxf_check_nan_or_inf, cxf_updatemodel |
| cxf_addconstrs | Add multiple constraints to model | complex | cxf_addvar, cxf_calloc, cxf_error, cxf_errorlog, cxf_free | - |
| cxf_addgenconstrIndicator | Add indicator constraint | complex | cxf_optimize, cxf_updatemodel, cxf_write | - |
| cxf_addqconstr | Add quadratic constraint | complex | cxf_addconstr, cxf_calloc, cxf_check_nan, cxf_check_nan_or_inf, cxf_error | cxf_check_nan_or_inf |
| cxf_addqpterms | Add quadratic objective terms | complex | cxf_optimize, cxf_updatemodel | - |
| cxf_addvar | Add single variable to model | complex | cxf_addvars, cxf_errorlog, cxf_free, cxf_geterrormsg, cxf_optimize | cxf_addconstr, cxf_addconstrs, cxf_check_nan_or_inf |
| cxf_addvars | Add multiple variables to model | complex | cxf_errorlog, cxf_free | cxf_addvar |
| cxf_chgcoeffs | Change matrix coefficients | complex | cxf_calloc, cxf_error, cxf_errorlog, cxf_free, cxf_malloc | cxf_check_nan_or_inf, cxf_updatemodel |
| cxf_copymodel | Create copy of model | complex | cxf_geterrormsg, cxf_updatemodel | - |
| cxf_delvars | Delete variables from model | complex | cxf_calloc, cxf_error, cxf_optimize, cxf_updatemodel, cxf_write | - |
| cxf_emptyenv | Create empty environment | complex | cxf_freeenv, cxf_geterrormsg, cxf_loadenv, cxf_setintparam | - |
| cxf_freeenv | Free environment resources | complex | - | cxf_checkenv, cxf_emptyenv, cxf_env_acquire_lock |
| cxf_freemodel | Free model resources | complex | cxf_free, cxf_free_basis_state, cxf_free_callback_state, cxf_free_solver_state, cxf_freeenv | cxf_env_acquire_lock, cxf_reset_callback_state |
| cxf_getcoeff | Get single matrix coefficient | complex | - | - |
| cxf_getconstrs | Get constraint matrix data | complex | cxf_build_row_major, cxf_error, cxf_finalize_row_data, cxf_free, cxf_malloc | cxf_build_row_major, cxf_finalize_row_data, cxf_prepare_row_data |
| cxf_getdblattr | Get double attribute value | complex | cxf_error | cxf_extract_solution, cxf_post_optimize_callback |
| cxf_geterrormsg | Get error message | medium | cxf_getintattr | cxf_addvar, cxf_copymodel, cxf_emptyenv |
| cxf_getintattr | Get integer attribute value | complex | cxf_error | cxf_geterrormsg, cxf_is_mip_model, cxf_is_multi_obj |
| cxf_getintparam | Get integer parameter value | complex | cxf_error | cxf_get_threads |
| cxf_loadenv | Load environment | medium | cxf_geterrormsg | cxf_checkenv, cxf_emptyenv, cxf_env_acquire_lock |
| cxf_newmodel | Create new model | complex | cxf_checkenv, cxf_env_acquire_lock, cxf_geterrormsg, cxf_malloc, cxf_validate_array | cxf_env_acquire_lock, cxf_validate_vartypes |
| cxf_optimize | Optimize model | complex | cxf_acquire_solve_lock, cxf_error, cxf_errorlog, cxf_get_logical_processors, cxf_get_physical_cores | cxf_addgenconstrIndicator, cxf_addqpterms, cxf_addvar |
| cxf_optimize_internal | Internal optimization dispatcher | complex | cxf_free, cxf_getintattr, cxf_optimize, cxf_reset_callback_state | cxf_optimize, cxf_post_optimize_callback, cxf_pre_optimize_callback |
| cxf_read | Read model from file | complex | cxf_updatemodel | - |
| cxf_setcallbackfunc | Set callback function | complex | cxf_calloc, cxf_error, cxf_get_timestamp, cxf_init_callback_struct | cxf_free_callback_state, cxf_get_timestamp, cxf_init_callback_struct |
| cxf_setintparam | Set integer parameter | complex | - | cxf_emptyenv, cxf_env_acquire_lock, cxf_leave_critical_section |
| cxf_terminate | Request optimization termination | complex | cxf_callback_terminate | cxf_callback_terminate, cxf_check_terminate, cxf_pre_optimize_callback |
| cxf_updatemodel | Apply pending model changes | complex | cxf_addconstr, cxf_addvar, cxf_chgcoeffs, cxf_geterrormsg, cxf_optimize | cxf_addgenconstrIndicator, cxf_addqpterms, cxf_addvar |
| cxf_version | Get version information | medium | - | - |
| cxf_write | Write model to file | complex | - | cxf_addgenconstrIndicator, cxf_addvar, cxf_delvars |

## Memory Management

**Functions:** 9

| Function | Purpose | Complexity | Calls | Called By |
|----------|---------|------------|-------|----------|
| cxf_alloc_eta | Allocate eta update vector | complex | cxf_calloc, cxf_free, cxf_malloc | cxf_acquire_solve_lock, cxf_basis_refactor, cxf_pivot_bound |
| cxf_calloc | Zero-initialized allocation | complex | cxf_free, cxf_malloc, cxf_realloc | cxf_acquire_solve_lock, cxf_addconstr, cxf_addconstrs |
| cxf_free | Deallocate memory | complex | cxf_calloc, cxf_freeenv, cxf_loadenv, cxf_malloc, cxf_realloc | cxf_acquire_solve_lock, cxf_addconstr, cxf_addconstrs |
| cxf_free_basis_state | Deallocate basis snapshot | complex | cxf_basis_diff, cxf_basis_snapshot, cxf_basis_warm, cxf_free, cxf_free_solver_state | cxf_cleanup_solve_state, cxf_free_callback_state, cxf_freemodel |
| cxf_free_callback_state | Deallocate callback structures | complex | cxf_free, cxf_free_basis_state, cxf_free_solver_state, cxf_freeenv, cxf_log_printf | cxf_freemodel, cxf_init_callback_struct, cxf_reset_callback_state |
| cxf_free_solver_state | Deallocate solver state | complex | cxf_cleanup_solve_state, cxf_free, cxf_simplex_cleanup, cxf_simplex_final, cxf_simplex_init | cxf_acquire_solve_lock, cxf_cleanup_solve_state, cxf_free_basis_state |
| cxf_malloc | Primary heap allocation | complex | cxf_calloc, cxf_free, cxf_realloc | cxf_acquire_solve_lock, cxf_addconstrs, cxf_alloc_eta |
| cxf_realloc | Resize existing allocation | complex | cxf_calloc, cxf_free, cxf_malloc | cxf_addconstrs, cxf_addqconstr, cxf_calloc |
| cxf_vector_free | Free vector container | complex | cxf_free, cxf_malloc | cxf_freemodel |

## Error Handling

**Functions:** 10

| Function | Purpose | Complexity | Calls | Called By |
|----------|---------|------------|-------|----------|
| cxf_check_model_flags1 | Check if model has MIP features | complex | cxf_count_genconstr_types, cxf_is_mip_model, cxf_is_socp, cxf_solve_lp | - |
| cxf_check_model_flags2 | Check if model has quadratic or conic features | complex | cxf_is_quadratic, cxf_is_socp, cxf_solve_lp | - |
| cxf_check_nan | Detect NaN values | complex | cxf_check_nan_or_inf, cxf_validate_array | cxf_addconstr, cxf_addqconstr, cxf_check_nan_or_inf |
| cxf_check_nan_or_inf | Detect NaN or Infinity | complex | cxf_addconstr, cxf_addqconstr, cxf_addvar, cxf_check_nan, cxf_chgcoeffs | cxf_addconstr, cxf_addqconstr, cxf_check_nan |
| cxf_check_terminate | Check termination flag | complex | cxf_callback_terminate, cxf_set_terminate, cxf_terminate | cxf_callback_terminate, cxf_set_terminate |
| cxf_checkenv | Validate environment pointer | complex | cxf_addvar, cxf_env_acquire_lock, cxf_freeenv, cxf_loadenv | cxf_newmodel, cxf_register_log_callback |
| cxf_error | Set error code and message | complex | cxf_addvar, cxf_errorlog, cxf_free, cxf_geterrormsg, cxf_log_printf | cxf_addconstr, cxf_addconstrs, cxf_addqconstr |
| cxf_errorlog | Log error to model | complex | cxf_error, cxf_geterrormsg, cxf_log_printf, cxf_register_log_callback | cxf_addconstr, cxf_addconstrs, cxf_addqconstr |
| cxf_pivot_check | Validate pivot feasibility | medium | - | - |
| cxf_special_check | Check special pivot cases | complex | cxf_pivot_primal, cxf_pivot_special, cxf_simplex_step | cxf_pivot_special |

## Logging

**Functions:** 5

| Function | Purpose | Complexity | Calls | Called By |
|----------|---------|------------|-------|----------|
| cxf_get_logical_processors | Detect logical processor count | complex | cxf_get_physical_cores, cxf_get_threads, cxf_set_thread_count | cxf_get_physical_cores, cxf_get_threads, cxf_optimize |
| cxf_log10_wrapper | Math helper for log formatting | complex | cxf_coefficient_stats, cxf_floor_ceil_wrapper | - |
| cxf_log_printf | Formatted log output | complex | cxf_error, cxf_errorlog, cxf_geterrormsg, cxf_register_log_callback | cxf_coefficient_stats, cxf_env_acquire_lock, cxf_error |
| cxf_register_log_callback | Set up log callback | complex | cxf_calloc, cxf_checkenv, cxf_errorlog, cxf_init_callback_struct, cxf_loadenv | cxf_errorlog, cxf_log_printf, cxf_misc_utility |
| cxf_snprintf_wrapper | Safe string formatting | complex | cxf_error, cxf_log_printf | - |

## Parameters

**Functions:** 4

| Function | Purpose | Complexity | Calls | Called By |
|----------|---------|------------|-------|----------|
| cxf_get_feasibility_tol | Get primal tolerance | medium | cxf_getdblparam | cxf_ratio_test, cxf_simplex_cleanup, cxf_simplex_perturbation |
| cxf_get_infinity | Get infinity threshold | medium | cxf_getdblparam | cxf_pricing_steepest, cxf_ratio_test, cxf_simplex_perturbation |
| cxf_get_optimality_tol | Get dual tolerance | medium | cxf_getdblparam | cxf_pricing_steepest, cxf_simplex_cleanup, cxf_simplex_phase_end |
| cxf_getdblparam | Get double parameter value | complex | cxf_error | cxf_get_feasibility_tol, cxf_get_infinity, cxf_get_optimality_tol |

## Validation

**Functions:** 2

| Function | Purpose | Complexity | Calls | Called By |
|----------|---------|------------|-------|----------|
| cxf_validate_array | Validate numeric arrays | complex | cxf_check_nan, cxf_check_nan_or_inf, cxf_error, cxf_validate_vartypes | cxf_check_nan, cxf_check_nan_or_inf, cxf_newmodel |
| cxf_validate_vartypes | Validate variable type array | complex | cxf_error, cxf_is_mip_model, cxf_newmodel, cxf_validate_array | cxf_newmodel, cxf_validate_array |

## Simplex Core

**Functions:** 21

| Function | Purpose | Complexity | Calls | Called By |
|----------|---------|------------|-------|----------|
| cxf_pivot_bound | Handle bound pivots, flips, and variable fixing | complex | cxf_alloc_eta, cxf_pricing_invalidate, cxf_pricing_update, cxf_quadratic_adjust | cxf_crossover_bounds, cxf_pivot_special |
| cxf_pivot_primal | Execute standard primal simplex pivot operation | complex | cxf_alloc_eta, cxf_pricing_invalidate, cxf_pricing_update, cxf_quadratic_adjust | cxf_acquire_solve_lock, cxf_dot_product, cxf_matrix_multiply |
| cxf_pivot_special | Handle special pivot cases including unbounded detection | complex | cxf_fix_variable, cxf_pivot_bound, cxf_pricing_invalidate, cxf_special_check | cxf_simplex_step, cxf_special_check |
| cxf_pivot_with_eta | Create Type 2 eta vector for row-based pivot operations | complex | cxf_alloc_eta | cxf_simplex_step |
| cxf_ratio_test | Select leaving variable via ratio test | complex | cxf_basis_refactor, cxf_get_feasibility_tol, cxf_get_infinity, cxf_pivot_primal, cxf_simplex_perturbation | cxf_timing_pivot |
| cxf_simplex_cleanup | Resource cleanup | complex | cxf_get_feasibility_tol, cxf_get_optimality_tol | cxf_cleanup_helper, cxf_crossover, cxf_free_solver_state |
| cxf_simplex_crash | Initial basis construction | complex | cxf_free | cxf_basis_refactor, cxf_basis_warm, cxf_simplex_setup |
| cxf_simplex_final | Finalization and cleanup | complex | cxf_free, cxf_simplex_init | cxf_cleanup_solve_state, cxf_extract_solution, cxf_free_solver_state |
| cxf_simplex_init | Initialize simplex state | complex | cxf_free | cxf_acquire_solve_lock, cxf_basis_warm, cxf_cleanup_solve_state |
| cxf_simplex_iterate | Main iteration loop | complex | - | cxf_floor_ceil_wrapper, cxf_simplex_post_iterate, cxf_solve_lp |
| cxf_simplex_perturbation | Anti-cycling perturbation | complex | cxf_generate_seed, cxf_get_feasibility_tol, cxf_get_infinity, cxf_simplex_unperturb | cxf_generate_seed, cxf_ratio_test, cxf_solve_lp |
| cxf_simplex_phase_end | Phase termination check | complex | cxf_get_feasibility_tol, cxf_get_optimality_tol, cxf_simplex_step, cxf_solve_lp | cxf_simplex_step3, cxf_solve_lp |
| cxf_simplex_post_iterate | Post-iteration processing | complex | cxf_basis_refactor, cxf_simplex_iterate, cxf_simplex_step | cxf_solve_lp, cxf_timing_end |
| cxf_simplex_preprocess | Preprocessing phase | complex | - | cxf_simplex_setup, cxf_solve_lp |
| cxf_simplex_refine | Solution refinement | complex | cxf_ftran, cxf_get_feasibility_tol, cxf_matrix_multiply, cxf_vector_norm | cxf_cleanup_helper, cxf_extract_solution, cxf_ftran |
| cxf_simplex_setup | Pre-iteration setup | complex | cxf_basis_refactor, cxf_pricing_init, cxf_simplex_crash, cxf_simplex_init, cxf_simplex_preprocess | cxf_crossover_bounds, cxf_solve_lp |
| cxf_simplex_step | Single pivot iteration | complex | cxf_alloc_eta, cxf_get_feasibility_tol, cxf_get_infinity, cxf_get_optimality_tol, cxf_pivot_primal | cxf_basis_diff, cxf_basis_equal, cxf_basis_snapshot |
| cxf_simplex_step2 | Phase 1 iteration | complex | cxf_alloc_eta, cxf_get_feasibility_tol, cxf_get_infinity, cxf_simplex_step, cxf_solve_lp | - |
| cxf_simplex_step3 | Optimality check iteration | complex | cxf_alloc_eta, cxf_get_optimality_tol, cxf_simplex_phase_end, cxf_simplex_step, cxf_solve_lp | - |
| cxf_simplex_unperturb | Remove perturbations | complex | - | cxf_simplex_perturbation |
| cxf_timing_pivot | Time pivot operations | complex | cxf_basis_refactor, cxf_pivot_primal, cxf_ratio_test, cxf_simplex_step, cxf_timing_end | cxf_timing_end, cxf_timing_update |

## Basis Operations

**Functions:** 8

| Function | Purpose | Complexity | Calls | Called By |
|----------|---------|------------|-------|----------|
| cxf_basis_diff | Compare basis states | complex | cxf_basis_equal, cxf_basis_snapshot, cxf_pricing_candidates, cxf_simplex_step | cxf_basis_equal, cxf_basis_snapshot, cxf_free_basis_state |
| cxf_basis_equal | Test basis equality | complex | cxf_basis_diff, cxf_basis_snapshot, cxf_simplex_step | cxf_basis_diff |
| cxf_basis_refactor | Full LU refactorization | complex | cxf_alloc_eta, cxf_basis_snapshot, cxf_pricing_invalidate, cxf_simplex_crash, cxf_timing_refactor | cxf_acquire_solve_lock, cxf_basis_snapshot, cxf_basis_validate |
| cxf_basis_snapshot | Save basis state | complex | cxf_basis_diff, cxf_basis_refactor, cxf_simplex_step | cxf_basis_diff, cxf_basis_equal, cxf_basis_refactor |
| cxf_basis_validate | Verify basis integrity | complex | cxf_basis_refactor, cxf_basis_snapshot, cxf_basis_warm, cxf_error | - |
| cxf_basis_warm | Warm start from basis | complex | cxf_basis_refactor, cxf_basis_snapshot, cxf_optimize, cxf_simplex_crash, cxf_simplex_init | cxf_basis_validate, cxf_free_basis_state |
| cxf_btran | Backward transformation (y'B=c') | complex | cxf_basis_refactor, cxf_pricing_candidates, cxf_simplex_step | cxf_ftran |
| cxf_ftran | Forward transformation (Bx=b) | complex | cxf_basis_refactor, cxf_btran, cxf_simplex_refine, cxf_simplex_step | cxf_simplex_refine |

## Pricing

**Functions:** 6

| Function | Purpose | Complexity | Calls | Called By |
|----------|---------|------------|-------|----------|
| cxf_pricing_candidates | Generate candidate list | complex | cxf_pricing_invalidate, cxf_pricing_update, cxf_simplex_step | cxf_basis_diff, cxf_btran, cxf_pricing_init |
| cxf_pricing_init | Initialize pricing structures | complex | cxf_calloc, cxf_free, cxf_pricing_candidates, cxf_pricing_invalidate, cxf_pricing_update | cxf_simplex_setup |
| cxf_pricing_invalidate | Invalidate pricing cache | complex | cxf_basis_refactor, cxf_pivot_primal, cxf_pricing_candidates, cxf_pricing_update | cxf_basis_refactor, cxf_fix_variable, cxf_pivot_bound |
| cxf_pricing_steepest | Steepest edge pricing | complex | cxf_basis_refactor, cxf_get_infinity, cxf_get_optimality_tol, cxf_pricing_candidates, cxf_pricing_update | cxf_crossover, cxf_vector_norm |
| cxf_pricing_step2 | Phase-specific pricing | medium | cxf_pricing_candidates, cxf_pricing_invalidate, cxf_timing_refactor | - |
| cxf_pricing_update | Update after pivot | medium | cxf_pivot_primal, cxf_pricing_candidates, cxf_pricing_invalidate, cxf_simplex_step, cxf_timing_refactor | cxf_fix_variable, cxf_pivot_bound, cxf_pivot_primal |

## Matrix Operations

**Functions:** 7

| Function | Purpose | Complexity | Calls | Called By |
|----------|---------|------------|-------|----------|
| cxf_build_row_major | Convert to row-major format | complex | cxf_finalize_row_data, cxf_free, cxf_getconstrs, cxf_malloc, cxf_prepare_row_data | cxf_finalize_row_data, cxf_getconstrs, cxf_prepare_row_data |
| cxf_dot_product | Vector inner product | complex | cxf_pivot_primal, cxf_simplex_step | - |
| cxf_finalize_row_data | Finalize row structures | complex | cxf_build_row_major, cxf_getconstrs, cxf_optimize, cxf_prepare_row_data, cxf_updatemodel | cxf_build_row_major, cxf_getconstrs, cxf_prepare_row_data |
| cxf_matrix_multiply | Sparse matrix-vector product | medium | cxf_basis_refactor, cxf_crossover, cxf_pivot_primal, cxf_simplex_step | cxf_simplex_refine |
| cxf_prepare_row_data | Prepare row access structures | complex | cxf_build_row_major, cxf_finalize_row_data, cxf_free, cxf_getconstrs, cxf_malloc | cxf_build_row_major, cxf_finalize_row_data, cxf_getconstrs |
| cxf_sort_indices | Sort sparse matrix indices | complex | - | - |
| cxf_vector_norm | Compute various vector norms | complex | cxf_coefficient_stats, cxf_pricing_steepest | cxf_simplex_refine |

## Solver State

**Functions:** 4

| Function | Purpose | Complexity | Calls | Called By |
|----------|---------|------------|-------|----------|
| cxf_cleanup_helper | Bound tightening helper | complex | cxf_free, cxf_malloc, cxf_simplex_cleanup, cxf_simplex_refine | - |
| cxf_cleanup_solve_state | Clean up solve state | complex | cxf_free, cxf_free_basis_state, cxf_free_solver_state, cxf_get_timestamp, cxf_init_solve_state | cxf_free_solver_state, cxf_get_timestamp, cxf_init_solve_state |
| cxf_init_solve_state | Initialize solve state | complex | cxf_cleanup_solve_state, cxf_get_timestamp, cxf_simplex_final, cxf_simplex_init, cxf_solve_lp | cxf_cleanup_solve_state, cxf_get_timestamp |
| cxf_solve_lp | Main LP solve entry point | complex | cxf_basis_diff, cxf_basis_refactor, cxf_basis_snapshot, cxf_crossover, cxf_simplex_cleanup | cxf_check_model_flags1, cxf_check_model_flags2, cxf_cleanup_solve_state |

## Timing

**Functions:** 5

| Function | Purpose | Complexity | Calls | Called By |
|----------|---------|------------|-------|----------|
| cxf_get_timestamp | Get high-resolution time | complex | cxf_cleanup_solve_state, cxf_init_solve_state, cxf_setcallbackfunc, cxf_timing_end, cxf_timing_start | cxf_cleanup_solve_state, cxf_generate_seed, cxf_init_solve_state |
| cxf_timing_end | End timing section | complex | cxf_basis_snapshot, cxf_simplex_iterate, cxf_simplex_post_iterate, cxf_solve_lp, cxf_timing_pivot | cxf_get_timestamp, cxf_solve_lp, cxf_timing_pivot |
| cxf_timing_refactor | Time refactorization | complex | cxf_basis_refactor, cxf_pricing_candidates, cxf_pricing_invalidate, cxf_pricing_update, cxf_simplex_step | cxf_basis_refactor, cxf_pricing_step2, cxf_pricing_update |
| cxf_timing_start | Start timing section | complex | cxf_timing_end, cxf_timing_refactor, cxf_timing_update | cxf_get_timestamp, cxf_solve_lp, cxf_timing_end |
| cxf_timing_update | Update timing statistics | complex | cxf_basis_refactor, cxf_pivot_primal, cxf_timing_end, cxf_timing_pivot, cxf_timing_start | cxf_timing_end, cxf_timing_pivot, cxf_timing_start |

## Threading

**Functions:** 7

| Function | Purpose | Complexity | Calls | Called By |
|----------|---------|------------|-------|----------|
| cxf_acquire_solve_lock | Acquire solve-level lock | complex | cxf_alloc_eta, cxf_basis_refactor, cxf_calloc, cxf_env_acquire_lock, cxf_free | cxf_env_acquire_lock, cxf_leave_critical_section, cxf_optimize |
| cxf_env_acquire_lock | Acquire environment lock | complex | cxf_acquire_solve_lock, cxf_freeenv, cxf_freemodel, cxf_loadenv, cxf_log_printf | cxf_acquire_solve_lock, cxf_checkenv, cxf_leave_critical_section |
| cxf_get_physical_cores | Detect physical CPU cores | complex | cxf_get_logical_processors, cxf_get_threads, cxf_set_thread_count | cxf_get_logical_processors, cxf_get_threads, cxf_optimize |
| cxf_get_threads | Get configured thread count | medium | cxf_get_logical_processors, cxf_get_physical_cores, cxf_getintparam, cxf_set_thread_count | cxf_get_logical_processors, cxf_get_physical_cores, cxf_optimize |
| cxf_leave_critical_section | Release critical section | complex | cxf_acquire_solve_lock, cxf_env_acquire_lock, cxf_release_solve_lock, cxf_setintparam | - |
| cxf_release_solve_lock | Release solve-level lock | complex | cxf_acquire_solve_lock, cxf_env_acquire_lock, cxf_solve_lp | cxf_acquire_solve_lock, cxf_env_acquire_lock, cxf_leave_critical_section |
| cxf_set_thread_count | Set active thread count | complex | cxf_acquire_solve_lock, cxf_env_acquire_lock, cxf_free, cxf_get_logical_processors, cxf_get_physical_cores | cxf_get_logical_processors, cxf_get_physical_cores, cxf_get_threads |

## Callbacks

**Functions:** 6

| Function | Purpose | Complexity | Calls | Called By |
|----------|---------|------------|-------|----------|
| cxf_callback_terminate | Request termination | complex | cxf_check_terminate, cxf_set_terminate, cxf_terminate | cxf_check_terminate, cxf_set_terminate, cxf_terminate |
| cxf_init_callback_struct | Initialize callback state | complex | cxf_free_callback_state, cxf_post_optimize_callback, cxf_pre_optimize_callback, cxf_reset_callback_state, cxf_setcallbackfunc | cxf_register_log_callback, cxf_reset_callback_state, cxf_setcallbackfunc |
| cxf_post_optimize_callback | Post-optimization callback | complex | cxf_get_timestamp, cxf_getdblattr, cxf_getintattr, cxf_optimize, cxf_optimize_internal | cxf_init_callback_struct, cxf_optimize, cxf_pre_optimize_callback |
| cxf_pre_optimize_callback | Pre-optimization callback | complex | cxf_get_timestamp, cxf_getintattr, cxf_optimize, cxf_optimize_internal, cxf_post_optimize_callback | cxf_init_callback_struct, cxf_optimize, cxf_post_optimize_callback |
| cxf_reset_callback_state | Reset to initial state | complex | cxf_free_callback_state, cxf_freeenv, cxf_freemodel, cxf_get_timestamp, cxf_init_callback_struct | cxf_free_callback_state, cxf_init_callback_struct, cxf_optimize_internal |
| cxf_set_terminate | Set termination flag | complex | cxf_callback_terminate, cxf_check_terminate, cxf_error, cxf_terminate | cxf_callback_terminate, cxf_check_terminate |

## Model Analysis

**Functions:** 6

| Function | Purpose | Complexity | Calls | Called By |
|----------|---------|------------|-------|----------|
| cxf_coefficient_stats | Compute coefficient statistics | complex | cxf_compute_coef_stats, cxf_log_printf | cxf_compute_coef_stats, cxf_log10_wrapper, cxf_presolve_stats |
| cxf_compute_coef_stats | Compute detailed stats | complex | cxf_coefficient_stats, cxf_get_qconstr_data, cxf_presolve_stats | cxf_coefficient_stats |
| cxf_is_mip_model | Check for integer variables | medium | cxf_getintattr | cxf_check_model_flags1, cxf_validate_vartypes |
| cxf_is_quadratic | Check for QP objective | medium | - | cxf_check_model_flags2, cxf_is_socp |
| cxf_is_socp | Check for conic constraints | complex | cxf_is_quadratic | cxf_check_model_flags1, cxf_check_model_flags2 |
| cxf_presolve_stats | Log presolve statistics | complex | cxf_coefficient_stats, cxf_count_genconstr_types, cxf_get_genconstr_name, cxf_log_printf | cxf_compute_coef_stats, cxf_count_genconstr_types, cxf_get_genconstr_name |

## Crossover

**Functions:** 2

| Function | Purpose | Complexity | Calls | Called By |
|----------|---------|------------|-------|----------|
| cxf_crossover | Convert interior-point to basic solution | complex | cxf_basis_refactor, cxf_crossover_bounds, cxf_pricing_steepest, cxf_simplex_cleanup | cxf_crossover_bounds, cxf_matrix_multiply, cxf_solve_lp |
| cxf_crossover_bounds | Bound snapping during crossover | complex | cxf_crossover, cxf_pivot_bound, cxf_simplex_setup | cxf_crossover |

## Utilities

**Functions:** 10

| Function | Purpose | Complexity | Calls | Called By |
|----------|---------|------------|-------|----------|
| cxf_count_genconstr_types | Count general constraints by type | complex | cxf_get_genconstr_name, cxf_presolve_stats | cxf_check_model_flags1, cxf_get_genconstr_name, cxf_presolve_stats |
| cxf_extract_solution | Extract solution values | complex | cxf_calloc, cxf_free, cxf_getdblattr, cxf_simplex_final, cxf_simplex_refine | - |
| cxf_fix_variable | Fix variable to bound | complex | cxf_pricing_invalidate, cxf_pricing_update, cxf_quadratic_adjust | cxf_pivot_special |
| cxf_floor_ceil_wrapper | Floor/ceiling wrapper | complex | cxf_simplex_iterate | cxf_log10_wrapper |
| cxf_generate_seed | Generate random seed | complex | cxf_get_timestamp, cxf_simplex_perturbation | cxf_simplex_perturbation |
| cxf_get_genconstr_name | Get human-readable constraint name | complex | cxf_count_genconstr_types, cxf_log_printf, cxf_presolve_stats | cxf_count_genconstr_types, cxf_presolve_stats |
| cxf_get_qconstr_data | Retrieve quadratic constraint data | complex | - | cxf_compute_coef_stats |
| cxf_is_multi_obj | Check if model has multiple objectives | complex | cxf_getintattr, cxf_optimize | - |
| cxf_misc_utility | Miscellaneous utility | complex | cxf_register_log_callback | - |
| cxf_quadratic_adjust | Adjust quadratic terms | complex | cxf_pricing_invalidate | cxf_fix_variable, cxf_pivot_bound, cxf_pivot_primal |

---
*Convexfeld LP Solver Specification*
*Based on published optimization literature*
