# Function Checklist

## Memory Management (9 functions)

- [ ] `cxf_malloc` - `docs/specs/functions/memory/cxf_malloc.md`
- [ ] `cxf_calloc` - `docs/specs/functions/memory/cxf_calloc.md`
- [ ] `cxf_realloc` - `docs/specs/functions/memory/cxf_realloc.md`
- [ ] `cxf_free` - `docs/specs/functions/memory/cxf_free.md`
- [ ] `cxf_vector_free` - `docs/specs/functions/memory/cxf_vector_free.md`
- [ ] `cxf_alloc_eta` - `docs/specs/functions/memory/cxf_alloc_eta.md`
- [ ] `cxf_free_solver_state` - `docs/specs/functions/memory/cxf_free_solver_state.md`
- [ ] `cxf_free_basis_state` - `docs/specs/functions/memory/cxf_free_basis_state.md`
- [ ] `cxf_free_callback_state` - `docs/specs/functions/memory/cxf_free_callback_state.md`

## Parameters (4 functions)

- [ ] `cxf_getdblparam` - `docs/specs/functions/parameters/cxf_getdblparam.md`
- [ ] `cxf_get_feasibility_tol` - `docs/specs/functions/parameters/cxf_get_feasibility_tol.md`
- [ ] `cxf_get_optimality_tol` - `docs/specs/functions/parameters/cxf_get_optimality_tol.md`
- [ ] `cxf_get_infinity` - `docs/specs/functions/parameters/cxf_get_infinity.md`

## Validation (2 functions)

- [ ] `cxf_validate_array` - `docs/specs/functions/validation/cxf_validate_array.md`
- [ ] `cxf_validate_vartypes` - `docs/specs/functions/validation/cxf_validate_vartypes.md`

## Error Handling (10 functions)

- [ ] `cxf_error` - `docs/specs/functions/error_logging/cxf_error.md`
- [ ] `cxf_errorlog` - `docs/specs/functions/error_logging/cxf_errorlog.md`
- [ ] `cxf_check_nan` - `docs/specs/functions/validation/cxf_check_nan.md`
- [ ] `cxf_check_nan_or_inf` - `docs/specs/functions/validation/cxf_check_nan_or_inf.md`
- [ ] `cxf_checkenv` - `docs/specs/functions/validation/cxf_checkenv.md`
- [ ] `cxf_pivot_check` - `docs/specs/functions/ratio_test/cxf_pivot_check.md`
- [ ] `cxf_special_check` - `docs/specs/functions/statistics/cxf_special_check.md`
- [ ] `cxf_check_model_flags1` - `docs/specs/functions/validation/cxf_check_model_flags1.md`
- [ ] `cxf_check_model_flags2` - `docs/specs/functions/validation/cxf_check_model_flags2.md`
- [ ] `cxf_check_terminate` - `docs/specs/functions/callbacks/cxf_check_terminate.md`

## Logging (5 functions)

- [ ] `cxf_log_printf` - `docs/specs/functions/error_logging/cxf_log_printf.md`
- [ ] `cxf_log10_wrapper` - `docs/specs/functions/utilities/cxf_log10_wrapper.md`
- [ ] `cxf_snprintf_wrapper` - `docs/specs/functions/utilities/cxf_snprintf_wrapper.md`
- [ ] `cxf_register_log_callback` - `docs/specs/functions/error_logging/cxf_register_log_callback.md`
- [ ] `cxf_get_logical_processors` - `docs/specs/functions/threading/cxf_get_logical_processors.md`

## Threading (7 functions)

- [ ] `cxf_get_threads` - `docs/specs/functions/threading/cxf_get_threads.md`
- [ ] `cxf_set_thread_count` - `docs/specs/functions/threading/cxf_set_thread_count.md`
- [ ] `cxf_get_physical_cores` - `docs/specs/functions/threading/cxf_get_physical_cores.md`
- [ ] `cxf_acquire_solve_lock` - `docs/specs/functions/threading/cxf_acquire_solve_lock.md`
- [ ] `cxf_release_solve_lock` - `docs/specs/functions/threading/cxf_release_solve_lock.md`
- [ ] `cxf_env_acquire_lock` - `docs/specs/functions/threading/cxf_env_acquire_lock.md`
- [ ] `cxf_leave_critical_section` - `docs/specs/functions/threading/cxf_leave_critical_section.md`

## Timing (5 functions)

- [ ] `cxf_get_timestamp` - `docs/specs/functions/timing/cxf_get_timestamp.md`
- [ ] `cxf_timing_start` - `docs/specs/functions/timing/cxf_timing_start.md`
- [ ] `cxf_timing_end` - `docs/specs/functions/timing/cxf_timing_end.md`
- [ ] `cxf_timing_pivot` - `docs/specs/functions/timing/cxf_timing_pivot.md`
- [ ] `cxf_timing_update` - `docs/specs/functions/timing/cxf_timing_update.md`

## Matrix Operations (7 functions)

- [ ] `cxf_matrix_multiply` - `docs/specs/functions/matrix/cxf_matrix_multiply.md`
- [ ] `cxf_dot_product` - `docs/specs/functions/matrix/cxf_dot_product.md`
- [ ] `cxf_vector_norm` - `docs/specs/functions/matrix/cxf_vector_norm.md`
- [ ] `cxf_build_row_major` - `docs/specs/functions/matrix/cxf_build_row_major.md`
- [ ] `cxf_prepare_row_data` - `docs/specs/functions/matrix/cxf_prepare_row_data.md`
- [ ] `cxf_finalize_row_data` - `docs/specs/functions/matrix/cxf_finalize_row_data.md`
- [ ] `cxf_sort_indices` - `docs/specs/functions/matrix/cxf_sort_indices.md`

## Model Analysis (6 functions)

- [ ] `cxf_is_mip_model` - `docs/specs/functions/validation/cxf_is_mip_model.md`
- [ ] `cxf_is_quadratic` - `docs/specs/functions/validation/cxf_is_quadratic.md`
- [ ] `cxf_is_socp` - `docs/specs/functions/validation/cxf_is_socp.md`
- [ ] `cxf_coefficient_stats` - `docs/specs/functions/statistics/cxf_coefficient_stats.md`
- [ ] `cxf_compute_coef_stats` - `docs/specs/functions/statistics/cxf_compute_coef_stats.md`
- [ ] `cxf_presolve_stats` - `docs/specs/functions/statistics/cxf_presolve_stats.md`

## Basis Operations (8 functions)

- [ ] `cxf_ftran` - `docs/specs/functions/basis/cxf_ftran.md`
- [ ] `cxf_btran` - `docs/specs/functions/basis/cxf_btran.md`
- [ ] `cxf_basis_refactor` - `docs/specs/functions/basis/cxf_basis_refactor.md`
- [ ] `cxf_basis_snapshot` - `docs/specs/functions/basis/cxf_basis_snapshot.md`
- [ ] `cxf_basis_diff` - `docs/specs/functions/basis/cxf_basis_diff.md`
- [ ] `cxf_basis_equal` - `docs/specs/functions/basis/cxf_basis_equal.md`
- [ ] `cxf_basis_validate` - `docs/specs/functions/basis/cxf_basis_validate.md`
- [ ] `cxf_basis_warm` - `docs/specs/functions/basis/cxf_basis_warm.md`

## Callbacks (6 functions)

- [ ] `cxf_init_callback_struct` - `docs/specs/functions/callbacks/cxf_init_callback_struct.md`
- [ ] `cxf_reset_callback_state` - `docs/specs/functions/callbacks/cxf_reset_callback_state.md`
- [ ] `cxf_pre_optimize_callback` - `docs/specs/functions/callbacks/cxf_pre_optimize_callback.md`
- [ ] `cxf_post_optimize_callback` - `docs/specs/functions/callbacks/cxf_post_optimize_callback.md`
- [ ] `cxf_callback_terminate` - `docs/specs/functions/callbacks/cxf_callback_terminate.md`
- [ ] `cxf_set_terminate` - `docs/specs/functions/callbacks/cxf_set_terminate.md`

## Solver State (4 functions)

- [ ] `cxf_init_solve_state` - `docs/specs/functions/memory/cxf_init_solve_state.md`
- [ ] `cxf_cleanup_solve_state` - `docs/specs/functions/memory/cxf_cleanup_solve_state.md`
- [ ] `cxf_cleanup_helper` - `docs/specs/functions/utilities/cxf_cleanup_helper.md`
- [ ] `cxf_extract_solution` - `docs/specs/functions/utilities/cxf_extract_solution.md`

## Pricing (6 functions)

- [ ] `cxf_pricing_init` - `docs/specs/functions/pricing/cxf_pricing_init.md`
- [ ] `cxf_pricing_candidates` - `docs/specs/functions/pricing/cxf_pricing_candidates.md`
- [ ] `cxf_pricing_steepest` - `docs/specs/functions/pricing/cxf_pricing_steepest.md`
- [ ] `cxf_pricing_update` - `docs/specs/functions/pricing/cxf_pricing_update.md`
- [ ] `cxf_pricing_invalidate` - `docs/specs/functions/pricing/cxf_pricing_invalidate.md`
- [ ] `cxf_pricing_step2` - `docs/specs/functions/pricing/cxf_pricing_step2.md`

## Simplex Core (21 functions)

- [ ] `cxf_solve_lp` - `docs/specs/functions/simplex/cxf_solve_lp.md`
- [ ] `cxf_simplex_init` - `docs/specs/functions/simplex/cxf_simplex_init.md`
- [ ] `cxf_simplex_setup` - `docs/specs/functions/simplex/cxf_simplex_setup.md`
- [ ] `cxf_simplex_preprocess` - `docs/specs/functions/simplex/cxf_simplex_preprocess.md`
- [ ] `cxf_simplex_crash` - `docs/specs/functions/simplex/cxf_simplex_crash.md`
- [ ] `cxf_simplex_iterate` - `docs/specs/functions/simplex/cxf_simplex_iterate.md`
- [ ] `cxf_simplex_step` - `docs/specs/functions/simplex/cxf_simplex_step.md`
- [ ] `cxf_simplex_step2` - `docs/specs/functions/simplex/cxf_simplex_step2.md`
- [ ] `cxf_simplex_step3` - `docs/specs/functions/simplex/cxf_simplex_step3.md`
- [ ] `cxf_simplex_refine` - `docs/specs/functions/simplex/cxf_simplex_refine.md`
- [ ] `cxf_simplex_perturbation` - `docs/specs/functions/simplex/cxf_simplex_perturbation.md`
- [ ] `cxf_simplex_unperturb` - `docs/specs/functions/simplex/cxf_simplex_unperturb.md`
- [ ] `cxf_simplex_phase_end` - `docs/specs/functions/simplex/cxf_simplex_phase_end.md`
- [ ] `cxf_simplex_post_iterate` - `docs/specs/functions/simplex/cxf_simplex_post_iterate.md`
- [ ] `cxf_simplex_final` - `docs/specs/functions/simplex/cxf_simplex_final.md`
- [ ] `cxf_simplex_cleanup` - `docs/specs/functions/simplex/cxf_simplex_cleanup.md`
- [ ] `cxf_pivot_primal` - `docs/specs/functions/pivot/cxf_pivot_primal.md`
- [ ] `cxf_pivot_bound` - `docs/specs/functions/ratio_test/cxf_pivot_bound.md`
- [ ] `cxf_pivot_special` - `docs/specs/functions/ratio_test/cxf_pivot_special.md`
- [ ] `cxf_pivot_with_eta` - `docs/specs/functions/basis/cxf_pivot_with_eta.md`
- [ ] `cxf_ratio_test` - `docs/specs/functions/ratio_test/cxf_ratio_test.md`

## Crossover (2 functions)

- [ ] `cxf_crossover` - `docs/specs/functions/crossover/cxf_crossover.md`
- [ ] `cxf_crossover_bounds` - `docs/specs/functions/crossover/cxf_crossover_bounds.md`

## Utilities (10 functions)

- [ ] `cxf_fix_variable` - `docs/specs/functions/pivot/cxf_fix_variable.md`
- [ ] `cxf_quadratic_adjust` - `docs/specs/functions/simplex/cxf_quadratic_adjust.md`
- [ ] `cxf_generate_seed` - `docs/specs/functions/threading/cxf_generate_seed.md`
- [ ] `cxf_floor_ceil_wrapper` - `docs/specs/functions/utilities/cxf_floor_ceil_wrapper.md`
- [ ] `cxf_misc_utility` - `docs/specs/functions/utilities/cxf_misc_utility.md`
- [ ] `cxf_is_multi_obj` - `docs/specs/functions/utilities/cxf_is_multi_obj.md`
- [ ] `cxf_get_genconstr_name` - `docs/specs/functions/utilities/cxf_get_genconstr_name.md`
- [ ] `cxf_get_qconstr_data` - `docs/specs/functions/utilities/cxf_get_qconstr_data.md`
- [ ] `cxf_count_genconstr_types` - `docs/specs/functions/utilities/cxf_count_genconstr_types.md`
- [ ] `cxf_timing_refactor` - `docs/specs/functions/basis/cxf_timing_refactor.md`

## Model API (30 functions)

- [ ] `cxf_loadenv` - Environment loader
- [ ] `cxf_emptyenv` - Empty environment creator
- [ ] `cxf_freeenv` - Environment destructor
- [ ] `cxf_newmodel` - Model constructor
- [ ] `cxf_freemodel` - Model destructor
- [ ] `cxf_copymodel` - Model copy
- [ ] `cxf_updatemodel` - Apply pending changes
- [ ] `cxf_addvar` - Add single variable
- [ ] `cxf_addvars` - Add multiple variables
- [ ] `cxf_delvars` - Delete variables
- [ ] `cxf_addconstr` - Add single constraint
- [ ] `cxf_addconstrs` - Add multiple constraints
- [ ] `cxf_addqpterms` - Add quadratic objective terms
- [ ] `cxf_addqconstr` - Add quadratic constraint
- [ ] `cxf_addgenconstrIndicator` - Add indicator constraint
- [ ] `cxf_chgcoeffs` - Change matrix coefficients
- [ ] `cxf_getconstrs` - Get constraint data
- [ ] `cxf_getcoeff` - Get single coefficient
- [ ] `cxf_optimize` - Optimize model
- [ ] `cxf_optimize_internal` - Internal optimizer
- [ ] `cxf_terminate` - Request termination
- [ ] `cxf_getintattr` - Get integer attribute
- [ ] `cxf_getdblattr` - Get double attribute
- [ ] `cxf_setintparam` - Set integer parameter
- [ ] `cxf_getintparam` - Get integer parameter
- [ ] `cxf_read` - Read model from file
- [ ] `cxf_write` - Write model to file
- [ ] `cxf_version` - Get version info
- [ ] `cxf_geterrormsg` - Get error message
- [ ] `cxf_setcallbackfunc` - Set callback function
