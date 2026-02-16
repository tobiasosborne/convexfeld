# Audit Report: Function Signatures (M-Z)
**Auditor:** Agent (Sonnet 4.5)
**Date:** 2026-02-16
**Status:** PARTIAL - Phase 1 Complete (Discovery)

## Summary
- Total M-Z functions in spec: 75
- Functions found in codebase: 31 (41%)
- Functions missing from codebase: 44 (59%)
- Signature mismatches checked: 0 (pending Phase 2)

## Phase 1: Function Discovery (COMPLETE)

### Found Functions (31)
These functions were located in the codebase and require signature validation:

1. cxf_optimize
2. cxf_optimize_internal
3. cxf_pivot_bound
4. cxf_pivot_primal
5. cxf_pivot_special
6. cxf_pivot_with_eta
7. cxf_prepare_row_data
8. cxf_presolve_stats
9. cxf_pricing_candidates
10. cxf_pricing_invalidate
11. cxf_pricing_update
12. cxf_realloc
13. cxf_register_log_callback
14. cxf_release_solve_lock
15. cxf_simplex_crash
16. cxf_simplex_final
17. cxf_simplex_init
18. cxf_simplex_perturbation
19. cxf_simplex_phase_end
20. cxf_simplex_post_iterate
21. cxf_simplex_preprocess
22. cxf_simplex_refine
23. cxf_simplex_setup
24. cxf_simplex_step
25. cxf_simplex_step2
26. cxf_simplex_step3
27. cxf_solve_lp
28. cxf_special_check
29. cxf_validate_array
30. cxf_validate_vartypes
31. cxf_vector_free

### Missing Functions (44)
These functions are defined in v2 specs but NOT implemented in codebase:

**Memory & Allocation (3):**
- cxf_model_alloc (P3.01 - Memory Primitives)
- cxf_setup_resources (P3.02 - Allocation Helpers)
- cxf_reset_pending_buffer (P3.05 - State Cleanup: Buffers)

**Matrix Operations (2):**
- cxf_matrix_setup (P3.14 - Matrix Core)
- cxf_sort_by_values (P3.14 - Matrix Core)

**Pivot Operations (2):**
- cxf_pivot_check (P3.19 - Pivot Operations)
- cxf_pivot_update (P3.19 - Pivot Operations)

**Pricing (9):**
- cxf_pricing_cascade_update (P3.18 - Pricing Support)
- cxf_pricing_end_level (P3.18 - Pricing Support)
- cxf_pricing_get_constr_candidates (P3.18 - Pricing Support)
- cxf_pricing_get_constr_stats (P3.18 - Pricing Support)
- cxf_pricing_get_var_stats (P3.18 - Pricing Support)
- cxf_pricing_mark_constr_dirty (P3.18 - Pricing Support)
- cxf_pricing_mark_dirty (P3.18 - Pricing Support)
- cxf_pricing_set_level (P3.18 - Pricing Support)
- cxf_pricing_update_constr (P3.17 - Pricing Core)
- cxf_pricing_update_var (P3.17 - Pricing Core)

**Simplex Lifecycle (1):**
- cxf_simplex_postsolve (P3.22 - Simplex Lifecycle)

**Solve Entry & Dispatch (5):**
- cxf_solve_dispatch (P3.24 - Solve Entry & Dispatch)
- cxf_solve_entry (P3.24 - Solve Entry & Dispatch)
- cxf_solve_no_callbacks (P3.24 - Solve Entry & Dispatch)
- cxf_solve_with_callbacks (P3.24 - Solve Entry & Dispatch)
- cxf_solver_dispatch (P3.25 - Solve LP Core)

**Barrier & Concurrent (3):**
- cxf_solve_barrier (P3.26 - Solve Barrier & Concurrent)
- cxf_solve_concurrent (P3.26 - Solve Barrier & Concurrent)
- cxf_solve_concurrent_distributed (P3.26 - Solve Barrier & Concurrent)

**Solution Processing (5):**
- cxf_process_lp_solution (P3.29 - Solution Processing)
- cxf_scale_objval (P3.29 - Solution Processing)
- cxf_uncrush_solution (P3.29 - Solution Processing)
- cxf_wire_result_attributes (P3.29 - Solution Processing)

**Model Lifecycle (2):**
- cxf_model_apply_modifications (P3.31 - Model Lifecycle)
- cxf_model_create_internal (P3.31 - Model Lifecycle)
- cxf_update_model_manager (P3.31 - Model Lifecycle)

**Optimization Preparation (2):**
- cxf_prepare_optimization (P3.32 - Optimization Preparation)
- cxf_wait_async (P3.32 - Optimization Preparation)

**Statistics & Diagnostics (1):**
- cxf_progress_snapshot (P3.16 - Basis Operations)
- cxf_propagate_bounds (P3.34 - Cleanup Utilities)

**Validation (2):**
- cxf_validate_solution (P3.08 - Data Validation)
- cxf_validate_thread_count (P3.11 - Threading & Synchronization)

**Error Handling & Logging (2):**
- cxf_set_error_message (P3.09 - Error Handling)
- cxf_set_error_string (P3.10 - Logging)

**Callbacks (2):**
- cxf_post_optimize_hook (P3.13 - Callbacks)
- cxf_pre_optimize_hook (P3.13 - Callbacks)

**Threading (1):**
- cxf_save_locale_state (P3.11 - Threading & Synchronization)

## Phase 2: Signature Validation (PENDING)

**Next Steps:**
1. For each of the 31 found functions, extract actual signature from source
2. Extract expected signature from corresponding v2 spec
3. Compare:
   - Return type
   - Parameter count
   - Parameter types
   - Parameter names
   - Parameter order
4. Document any mismatches

**Modules requiring detailed signature checks:**
- P3.01 (Memory Primitives): cxf_realloc, cxf_vector_free
- P3.08 (Data Validation): cxf_validate_array, cxf_validate_vartypes, cxf_special_check
- P3.10 (Logging): cxf_register_log_callback
- P3.11 (Threading): cxf_release_solve_lock
- P3.14 (Matrix Core): cxf_prepare_row_data
- P3.16 (Basis Operations): cxf_pivot_with_eta
- P3.17 (Pricing Core): cxf_pricing_candidates, cxf_pricing_update
- P3.19 (Pivot Operations): cxf_pivot_bound, cxf_pivot_primal, cxf_pivot_special
- P3.20 (Simplex Iteration): cxf_simplex_post_iterate
- P3.21 (Simplex Phases): cxf_simplex_crash, cxf_simplex_preprocess, cxf_simplex_refine, cxf_simplex_setup
- P3.22 (Simplex Lifecycle): cxf_simplex_final, cxf_simplex_init
- P3.24 (Solve Entry): cxf_optimize, cxf_optimize_internal
- P3.25 (Solve LP Core): cxf_solve_lp
- P3.33 (Statistics): cxf_presolve_stats

## Analysis

### High Missing Function Rate (59%)
The fact that 44 out of 75 M-Z functions are missing suggests:

1. **Implementation is incomplete** - Many spec'd functions haven't been implemented yet
2. **Possible name mismatches** - Some functions may exist under different names
3. **Stubs may exist** - Some may be declared but not defined

### Critical Missing Subsystems
Several entire subsystems appear unimplemented:

- **Concurrent/Barrier solving** (P3.26): 0/3 functions implemented
- **Solution processing** (P3.29): 0/5 functions implemented
- **Pricing support** (P3.18): 0/8 functions implemented
- **Model lifecycle** (P3.31): 0/3 functions implemented

### Search Methodology
Functions were searched using:
```bash
grep -rn --include=*.c --include=*.h '^[a-zA-Z_*][^;]*FUNC_NAME\s*(' src/ include/
```

This may have missed:
- Functions with unusual formatting
- Static functions (not externally visible)
- Macro-generated functions
- Functions split across multiple lines

## Recommendations

1. **Complete Phase 2**: Validate signatures for all 31 found functions
2. **Investigate missing functions**: Determine if they're:
   - Not yet implemented (create issues)
   - Implemented under different names (document mapping)
   - Intentionally omitted (document rationale)
3. **Create follow-up audit for A-L functions**: This audit only covers M-Z
4. **Update specs**: If functions were renamed, update FUNCTION_MAP.md

## Methodology

### Search Pattern
```bash
find src include -type f \( -name "*.c" -o -name "*.h" \) -exec \
  grep -Hn "^[a-zA-Z_*][a-zA-Z0-9_* ]*cxf_[m-z][a-zA-Z0-9_]*\s*(" {} \;
```

### Spec Sources
- `/home/tobiasosborne/Projects/convexfeld/docs/specs-v2/FUNCTION_MAP.md`
- `/home/tobiasosborne/Projects/convexfeld/docs/specs-v2/specs/modules/*.md`

### Exclusions
- Inline functions
- Comment lines
- Extern declarations (without definitions)

---

**Status:** This audit is INCOMPLETE. Phase 2 (signature validation) remains to be done.

**Estimated remaining work:** 4-6 hours to complete signature comparisons for 31 functions.
