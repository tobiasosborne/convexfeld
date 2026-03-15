################################################################################
# ConvexFeld - Test Suite CMakeLists.txt
################################################################################

################################################################################
# Unity Test Framework
################################################################################

add_library(unity STATIC
    unity/unity.c
)

target_include_directories(unity PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/unity
)

# Unity configuration options
target_compile_definitions(unity PUBLIC
    UNITY_INCLUDE_DOUBLE
    UNITY_DOUBLE_PRECISION=1e-12
)

################################################################################
# Test Helper Function
################################################################################

# Function to create test executables with standard configuration
function(add_cxf_test test_name)
    add_executable(${test_name} ${ARGN})
    target_link_libraries(${test_name}
        PRIVATE
            unity
            convexfeld
    )
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

################################################################################
# Unit Tests
################################################################################

# Smoke test to verify Unity works
add_cxf_test(test_smoke unit/test_smoke.c)

# M2.1.1: Memory management tests
add_cxf_test(test_memory unit/test_memory.c)

# M4.1.1: Matrix operations tests (split from test_matrix.c for 200 LOC limit)
add_cxf_test(test_matrix_create unit/test_matrix_create.c)
target_link_libraries(test_matrix_create PRIVATE m)

add_cxf_test(test_matrix_math unit/test_matrix_math.c)
target_link_libraries(test_matrix_math PRIVATE m)

add_cxf_test(test_matrix_convert unit/test_matrix_convert.c)
target_link_libraries(test_matrix_convert PRIVATE m)

# M5.1.1: Basis operations tests (split from test_basis.c for 200 LOC limit)
add_cxf_test(test_basis_lifecycle unit/test_basis_lifecycle.c)
target_link_libraries(test_basis_lifecycle PRIVATE m)

add_cxf_test(test_basis_ftran unit/test_basis_ftran.c)
target_link_libraries(test_basis_ftran PRIVATE m)

add_cxf_test(test_basis_btran unit/test_basis_btran.c)
target_link_libraries(test_basis_btran PRIVATE m)

add_cxf_test(test_basis_progress unit/test_basis_progress.c)
target_link_libraries(test_basis_progress PRIVATE m)

add_cxf_test(test_basis_snapshot unit/test_basis_snapshot.c)
target_link_libraries(test_basis_snapshot PRIVATE m)

add_cxf_test(test_basis_validate unit/test_basis_validate.c)
target_link_libraries(test_basis_validate PRIVATE m)

add_cxf_test(test_basis_warm unit/test_basis_warm.c)
target_link_libraries(test_basis_warm PRIVATE m)

# LU Factors lifecycle tests
add_cxf_test(test_lu_factors unit/test_lu_factors.c)

# Sparse LU factorization tests
add_cxf_test(test_sparse_lu unit/test_sparse_lu.c)
target_link_libraries(test_sparse_lu PRIVATE m)

# M6.1.1: Pricing operations tests
add_cxf_test(test_pricing unit/test_pricing.c)
target_link_libraries(test_pricing PRIVATE m)  # For math functions

# M8.1.1: API Tests - Environment
add_cxf_test(test_api_env unit/test_api_env.c)

# M8.1.2: API Tests - Model
add_cxf_test(test_api_model unit/test_api_model.c)

# M8.1.3: API Tests - Variables
add_cxf_test(test_api_vars unit/test_api_vars.c)

# M2.1.3: Vector memory management tests
add_cxf_test(test_memory_vectors unit/test_memory_vectors.c)

# M3.1.1: Error handling tests (split into 3 files for < 200 LOC each)
add_cxf_test(test_error_core unit/test_error_core.c)
target_link_libraries(test_error_core PRIVATE m)  # For math functions (NAN)

add_cxf_test(test_error_numeric unit/test_error_numeric.c)
target_link_libraries(test_error_numeric PRIVATE m)  # For math functions (NAN, INFINITY)

add_cxf_test(test_error_reporting unit/test_error_reporting.c)

# P3.09: Fallback error message includes numeric code (convexfeld-gxcp)
add_cxf_test(test_error_fallback unit/test_error_fallback.c)

# P3.09: Zero error code handling (convexfeld-mrz6, convexfeld-tyy6)
add_cxf_test(test_error_zero_code unit/test_error_zero_code.c)

# M4.2.1: Timing tests
add_cxf_test(test_timing unit/test_timing.c)
target_link_libraries(test_timing PRIVATE m)  # For math functions

# V2: cxf_get_timestamp session-ID generator (convexfeld-2v8r)
add_cxf_test(test_timestamp_v2 unit/test_timestamp_v2.c)

# M4.3.1-M4.3.2: Analysis tests
add_cxf_test(test_analysis unit/test_analysis.c)

# M8.1.4: API Tests - Constraints
add_cxf_test(test_api_constrs unit/test_api_constrs.c)

# M8.1.5: API Tests - Optimize
add_cxf_test(test_api_optimize unit/test_api_optimize.c)

# M8.1.6: API Tests - Queries
add_cxf_test(test_api_query unit/test_api_query.c)

# M3.2.1: Logging tests (wrapper functions)
add_cxf_test(test_logging unit/test_logging.c)
target_link_libraries(test_logging PRIVATE m)  # For math functions

# M3.2.1: Logging tests (callbacks and system)
add_cxf_test(test_logging_callback unit/test_logging_callback.c)
target_link_libraries(test_logging_callback PRIVATE m)  # For math functions

# V2: Quiet-mode callback delivery (logging.md §Verbosity, convexfeld-ir53)
add_cxf_test(test_log_quiet_callback unit/test_log_quiet_callback.c)

# M3.1.7: Pivot validation tests
add_cxf_test(test_pivot_check unit/test_pivot_check.c)
target_link_libraries(test_pivot_check PRIVATE m)  # For math functions

# M2.2.1: Parameters tests
add_cxf_test(test_parameters unit/test_parameters.c)

# M7.1.1: Simplex Tests - Basic
add_cxf_test(test_simplex_basic unit/test_simplex_basic.c)

# M5.2.1, M5.2.2: Callbacks tests
add_cxf_test(test_callbacks unit/test_callbacks.c)
target_link_libraries(test_callbacks PRIVATE m)  # For math functions (isinf)

# V2: cxf_callback_terminate spec compliance (convexfeld-7izs)
add_cxf_test(test_callback_terminate_v2 unit/test_callback_terminate_v2.c)

# V2: callback_func ownership on Environment (convexfeld-vd3h)
add_cxf_test(test_callback_ownership unit/test_callback_ownership.c)

# M7.1.2: Simplex Tests - Iteration
add_cxf_test(test_simplex_iteration unit/test_simplex_iteration.c)

# V2 spec rename: cxf_simplex_iterate (was cxf_log_iteration_progress, convexfeld-ge8e)
add_cxf_test(test_simplex_iterate_rename unit/test_simplex_iterate_rename.c)

# M2.3.1: Validation tests
add_cxf_test(test_validation unit/test_validation.c)
target_link_libraries(test_validation PRIVATE m)  # For math functions (NAN, INFINITY)

# P3.08: cxf_validate_vartypes V2 spec compliance (convexfeld-05dr)
add_cxf_test(test_validate_vartypes unit/test_validate_vartypes.c)

# V2: cxf_validate_array diagnostic error messages (convexfeld-vt9j)
add_cxf_test(test_validate_array_diag unit/test_validate_array_diag.c)
target_link_libraries(test_validate_array_diag PRIVATE m)

# M3.3.1: Threading tests
add_cxf_test(test_threading unit/test_threading.c)

# M7.1.3: Simplex Tests - Edge Cases
add_cxf_test(test_simplex_edge unit/test_simplex_edge.c)

# M5.1: Pivot with Eta tests
add_cxf_test(test_pivot_eta unit/test_pivot_eta.c)
target_link_libraries(test_pivot_eta PRIVATE m)  # For math functions

# M7.1.6: Simplex Setup and Preprocess tests
add_cxf_test(test_simplex_setup unit/test_simplex_setup.c)
target_link_libraries(test_simplex_setup PRIVATE m)  # For math functions

# V2 cxf_simplex_preprocess signature compliance (convexfeld-frsq)
add_cxf_test(test_preprocess_sig unit/test_preprocess_sig.c)
target_link_libraries(test_preprocess_sig PRIVATE m)

# M5.3.1: Solver State tests
add_cxf_test(test_solver_state unit/test_solver_state.c)

# M8.1.15: Attribute API tests
add_cxf_test(test_attrs_api unit/test_attrs_api.c)

# M8.1.16: Parameter API tests
add_cxf_test(test_params_api unit/test_params_api.c)

# M8.1.18: Info API tests
add_cxf_test(test_info_api unit/test_info_api.c)

# M7.1.17: Quadratic adjustment tests
add_cxf_test(test_quadratic unit/test_quadratic.c)

# M7.1.7: Crash basis tests (v2 — P2.5)
add_cxf_test(test_crash unit/test_crash.c)
target_link_libraries(test_crash PRIVATE m)

# Crash CSR source + inactive sentinel tests (convexfeld-tx3m, convexfeld-y8ur)
add_cxf_test(test_crash_csr unit/test_crash_csr.c)
target_link_libraries(test_crash_csr PRIVATE m)

# M7.1.13: Perturbation tests (v2 — P2.6 EXPAND)
add_cxf_test(test_perturbation unit/test_perturbation.c)
target_link_libraries(test_perturbation PRIVATE m)

# P4.2-P4.5: V2 Pricing Queue tests
add_cxf_test(test_pricing_v2 unit/test_pricing_v2.c)
target_link_libraries(test_pricing_v2 PRIVATE m)

# P4.5: cxf_pricing_candidates spec name verification (convexfeld-o8ez)
add_cxf_test(test_pricing_candidates_rename unit/test_pricing_candidates_rename.c)
target_link_libraries(test_pricing_candidates_rename PRIVATE m)

# P3.18: V2 Constraint-side pricing tests (convexfeld-ro2u)
add_cxf_test(test_constr_candidates unit/test_constr_candidates.c)
target_link_libraries(test_constr_candidates PRIVATE m)

# MPS Parser tests
add_cxf_test(test_mps_parser unit/test_mps_parser.c)
target_link_libraries(test_mps_parser PRIVATE m)
target_compile_definitions(test_mps_parser PRIVATE SOURCE_DIR="${CMAKE_SOURCE_DIR}")

# Pivot bound (7-phase variable fixing) tests
add_cxf_test(test_pivot_bound unit/test_pivot_bound.c)
target_link_libraries(test_pivot_bound PRIVATE m)

# Pivot special (Phase I unboundedness suppression) tests
add_cxf_test(test_pivot_special unit/test_pivot_special.c)
target_link_libraries(test_pivot_special PRIVATE m)

# Pivot update (cancellation-safe activity bound update) tests
add_cxf_test(test_pivot_update unit/test_pivot_update.c)
target_link_libraries(test_pivot_update PRIVATE m)

# Pivot check direction (V2: harris_ratio_test.md Stage 1 entering direction, convexfeld-xzcx)
add_cxf_test(test_pivot_check_dir unit/test_pivot_check_dir.c)
target_link_libraries(test_pivot_check_dir PRIVATE m)

# Pivot primal (V2 spec compliance: CXF_INFEASIBLE, primal criterion) tests
add_cxf_test(test_pivot_primal unit/test_pivot_primal.c)
target_link_libraries(test_pivot_primal PRIVATE m)

# Pivot primal RHS (V2: work_rhs not model RHS, convexfeld-ir0d) tests
add_cxf_test(test_pivot_primal_rhs unit/test_pivot_primal_rhs.c)
target_link_libraries(test_pivot_primal_rhs PRIVATE m)

# Pivot primal threshold (V2: bound_eq_tol not pricing_tol, convexfeld-o9se) tests
add_cxf_test(test_pivot_primal_threshold unit/test_pivot_primal_threshold.c)
target_link_libraries(test_pivot_primal_threshold PRIVATE m)

# Tight-bound variable processing (V2 simplex_iteration.md Phase 3.2) tests
add_cxf_test(test_tight_bound unit/test_tight_bound.c)
target_link_libraries(test_tight_bound PRIVATE m)

# Ratio test (free variable direction) tests
add_cxf_test(test_ratio_test unit/test_ratio_test.c)
target_link_libraries(test_ratio_test PRIVATE m)

# Perturbation conditional on stalling (convexfeld-9ksl)
add_cxf_test(test_perturb_conditional unit/test_perturb_conditional.c)
target_link_libraries(test_perturb_conditional PRIVATE m)

# MPS Solve integration tests
add_cxf_test(test_mps_solve unit/test_mps_solve.c)
target_link_libraries(test_mps_solve PRIVATE m)
target_compile_definitions(test_mps_solve PRIVATE SOURCE_DIR="${CMAKE_SOURCE_DIR}")

# NaN/Inf step length guard (numerical_stability.md §C) tests
add_cxf_test(test_nan_step_guard unit/test_nan_step_guard.c)
target_link_libraries(test_nan_step_guard PRIVATE m)

# Rejected pivot column recovery (numerical_stability.md §A.4) tests
add_cxf_test(test_pivot_recovery unit/test_pivot_recovery.c)
target_link_libraries(test_pivot_recovery PRIVATE m)

# Phase-end constraint cleanup tests (two_phase_method.md Transition Step 4)
add_cxf_test(test_phase_end unit/test_phase_end.c)
target_link_libraries(test_phase_end PRIVATE m)

# Phase-end large constraint count (convexfeld-qxw8: modified[] was fixed 256)
add_cxf_test(test_phase_end_large unit/test_phase_end_large.c)
target_link_libraries(test_phase_end_large PRIVATE m)

# Core algorithm: recompute tests (xB, objective, FTRAN residual)
add_cxf_test(test_recompute unit/test_recompute.c)
target_link_libraries(test_recompute PRIVATE m)

# Numerical stability: near-singular and ill-conditioned matrices
add_cxf_test(test_numerical_singular unit/test_numerical_singular.c)
target_link_libraries(test_numerical_singular PRIVATE m)

# Numerical stability: extreme coefficient ranges and tiny values
add_cxf_test(test_numerical_range unit/test_numerical_range.c)
target_link_libraries(test_numerical_range PRIVATE m)

# FTRAN with eta factors (beyond identity basis)
add_cxf_test(test_ftran_eta unit/test_ftran_eta.c)
target_link_libraries(test_ftran_eta PRIVATE m)

# BTRAN with eta factors (beyond identity basis)
add_cxf_test(test_btran_eta unit/test_btran_eta.c)
target_link_libraries(test_btran_eta PRIVATE m)

# Step3 implied bound formula (Savelsbergh 1994, convexfeld-cm5w)
add_cxf_test(test_step3_implied_bound unit/test_step3_implied_bound.c)
target_link_libraries(test_step3_implied_bound PRIVATE m)

# modification_blocked flag during optimization (solve_entry.md Steps 4/12)
add_cxf_test(test_modification_blocked unit/test_modification_blocked.c)

# Status clear before solve (solve_entry.md Step 4)
add_cxf_test(test_status_clear unit/test_status_clear.c)

# V2 model_type_checking.md: cxf_check_model_flags1/flags2 spec compliance
add_cxf_test(test_model_flags_v2 unit/test_model_flags_v2.c)

# Eta linked list direction (V2 spec: next points to older, convexfeld-pxh0)
add_cxf_test(test_eta_list_direction unit/test_eta_list_direction.c)
target_link_libraries(test_eta_list_direction PRIVATE m)

# V2 cxf_sort_indices: values ascending, indices as satellite (convexfeld-6ygu)
add_cxf_test(test_sort_indices unit/test_sort_indices.c)
target_link_libraries(test_sort_indices PRIVATE m)

# Parameter name case-insensitive lookup (V2 parameter_system.md, convexfeld-djuf)
add_cxf_test(test_param_case unit/test_param_case.c)

# V2 EtaVector fields (enteringVar, leavingVar, direction, reducedCost, type)
add_cxf_test(test_eta_fields unit/test_eta_fields.c)
target_link_libraries(test_eta_fields PRIVATE m)

# Constraint queue L0 status filtering (pricing_support.md, convexfeld-yvbq)
add_cxf_test(test_queue_filter_l0 unit/test_queue_filter_l0.c)
target_link_libraries(test_queue_filter_l0 PRIVATE m)

# V2 input_validation.md: single-value NaN/finite checks (convexfeld-o4n4, convexfeld-roxb)
add_cxf_test(test_nan_check_v2 unit/test_nan_check_v2.c)
target_link_libraries(test_nan_check_v2 PRIVATE m)

# V2: cxf_special_check spec compliance (data_validation.md, convexfeld-6oys)
add_cxf_test(test_special_check_v2 unit/test_special_check_v2.c)
target_link_libraries(test_special_check_v2 PRIVATE m)

# V2 cxf_get_threads: thread count resolution chain (convexfeld-gx34)
add_cxf_test(test_get_threads unit/test_get_threads.c)

# V2 basis_diff scoring formula (convexfeld-3xyi)
add_cxf_test(test_basis_diff_scoring unit/test_basis_diff_scoring.c)
target_link_libraries(test_basis_diff_scoring PRIVATE m)

# V2 basis_diff/snapshot signature compliance (convexfeld-67bt)
add_cxf_test(test_basis_diff_sig unit/test_basis_diff_sig.c)
target_link_libraries(test_basis_diff_sig PRIVATE m)

# Locale safety: save/restore LC_NUMERIC around cxf_optimize (solve_entry.md Steps 3/12)
add_cxf_test(test_locale_safety unit/test_locale_safety.c)

# Adaptive Markowitz tolerance on high growth factor (numerical_stability.md §D)
add_cxf_test(test_markowitz_adaptive unit/test_markowitz_adaptive.c)
target_link_libraries(test_markowitz_adaptive PRIVATE m)

# V2 cache invalidation must not occur at level 0 (pricing_core.md, convexfeld-l0w2)
add_cxf_test(test_cache_level0 unit/test_cache_level0.c)
target_link_libraries(test_cache_level0 PRIVATE m)

# Work counter uses pre-compaction counts (pricing_support.md, convexfeld-xlam)
add_cxf_test(test_work_counter unit/test_work_counter.c)
target_link_libraries(test_work_counter PRIVATE m)

# V2 spec rename: cxf_pricing_update (was cxf_pricing_update_queues, convexfeld-5ox8)
add_cxf_test(test_pricing_update_rename unit/test_pricing_update_rename.c)
target_link_libraries(test_pricing_update_rename PRIVATE m)

# Cancellation detection in pivot_update (numerical_stability.md §B)
add_cxf_test(test_cancellation_detect unit/test_cancellation_detect.c)
target_link_libraries(test_cancellation_detect PRIVATE m)

# NaN/Inf input data rejection at API entry points (numerical_stability.md §C)
add_cxf_test(test_input_validation unit/test_input_validation.c)
target_link_libraries(test_input_validation PRIVATE m)

# SolverState.eta_count sync with BasisState.eta_count (convexfeld-0ev4)
add_cxf_test(test_eta_count_sync unit/test_eta_count_sync.c)
target_link_libraries(test_eta_count_sync PRIVATE m)

# Phase I setup uses env feasibility_tol (two_phase_method.md, convexfeld-mh2o)
add_cxf_test(test_phase_one_tol unit/test_phase_one_tol.c)
target_link_libraries(test_phase_one_tol PRIVATE m)

# Snapshot naming: verify lightweight (spec) vs heavy (extension) coexist
add_cxf_test(test_snapshot_naming unit/test_snapshot_naming.c)
target_link_libraries(test_snapshot_naming PRIVATE m)

# Bound propagation tolerance: CXF_MIN_PIVOT not CXF_PIVOT_TOL (convexfeld-2tl9)
add_cxf_test(test_bound_prop_tol unit/test_bound_prop_tol.c)
target_link_libraries(test_bound_prop_tol PRIVATE m)

# Phase I->II transition refactorization + xB recomputation (numerical_stability.md §A.4)
add_cxf_test(test_phase_transition_refactor unit/test_phase_transition_refactor.c)
target_link_libraries(test_phase_transition_refactor PRIVATE m)

# Phase I end: env tolerance never mutated (two_phase_method.md, convexfeld-eph4)
add_cxf_test(test_phase_end_tol unit/test_phase_end_tol.c)
target_link_libraries(test_phase_end_tol PRIVATE m)

################################################################################
# Integration Tests
################################################################################

# M1.0: Tracer Bullet Test - proves end-to-end architecture
add_cxf_test(test_tracer_bullet integration/test_tracer_bullet.c)

# Constrained LP integration test
add_cxf_test(test_constrained_lp integration/test_constrained_lp.c)

# Constraint satisfaction verification (mixed senses, multi-constraint)
add_cxf_test(test_constraint_satisfaction integration/test_constraint_satisfaction.c)

# End-to-end LP solving: optimal, equality, infeasible, unbounded
add_cxf_test(test_e2e_solve integration/test_e2e_solve.c)
