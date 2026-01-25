# Module Assignment Document

**Total Functions:** 142 (112 internal + 30 API)
**Total Modules:** 17

---

## Module Dependency Order (for specification extraction)

Modules should be specified in this order to minimize forward references:

```
Level 0 (No dependencies):
  +-- Memory Management (9)
  +-- Parameters (4)
  +-- Validation (2)

Level 1 (Depends on Level 0):
  +-- Error Handling (10)
  +-- Logging (5)
  +-- Threading (7)

Level 2 (Depends on Levels 0-1):
  +-- Matrix Operations (7)
  +-- Timing (5)
  +-- Model Analysis (6)

Level 3 (Depends on Levels 0-2):
  +-- Basis Operations (8)
  +-- Callbacks (6)
  +-- Solver State (4)

Level 4 (Depends on Levels 0-3):
  +-- Pricing (6)

Level 5 (Depends on Levels 0-4):
  +-- Simplex Core (21)
  +-- Crossover (2)

Level 6 (Depends on all internal):
  +-- Utilities (10)
  +-- Model API (30)
```

---

## Model Selection for Subagents

| Module | Functions | Model | Rationale |
|--------|-----------|-------|-----------|
| Memory Management | 9 | **Sonnet** | Standard alloc/free patterns |
| Parameters | 4 | **Sonnet** | Simple accessor functions |
| Validation | 2 | **Sonnet** | Input checking patterns |
| Error Handling | 10 | **Sonnet** | Standard error patterns |
| Logging | 5 | **Sonnet** | Output formatting |
| Threading | 7 | **Sonnet** | Lock acquire/release patterns |
| Matrix Operations | 7 | **Sonnet** | Standard linear algebra |
| Timing | 5 | **Sonnet** | Timer start/stop patterns |
| Model Analysis | 6 | **Sonnet** | Read-only model checks |
| **Basis Operations** | **8** | **Opus** | LU factorization, BTRAN/FTRAN complexity |
| Callbacks | 6 | **Sonnet** | Event handling patterns |
| Solver State | 4 | **Sonnet** | State management |
| **Pricing** | **6** | **Opus** | Steepest edge algorithm nuances |
| **Simplex Core** | **21** | **Opus** | Core algorithm, numerical subtleties |
| Crossover | 2 | **Sonnet** | Straightforward IPM-to-basis |
| Utilities | 10 | **Sonnet** | Miscellaneous helpers |
| Model API | 30 | **Sonnet** | Official docs available |

**Summary:** 101 Sonnet, 35 Opus (Basis + Pricing + Simplex Core)

---

## Module Definitions

### 1. Memory Management (9 functions)

**Purpose:** Core memory allocation, deallocation, and lifecycle management for solver data structures.

**Rationale:** These functions form the foundation layer. All other modules depend on memory management for allocating their data structures.

| Function | Role |
|----------|------|
| cxf_malloc | Primary heap allocation |
| cxf_calloc | Zero-initialized allocation |
| cxf_realloc | Resize existing allocation |
| cxf_free | Deallocate memory |
| cxf_vector_free | Free vector container |
| cxf_alloc_eta | Allocate eta update vector |
| cxf_free_solver_state | Deallocate solver state |
| cxf_free_basis_state | Deallocate basis snapshot |
| cxf_free_callback_state | Deallocate callback structures |

---

### 2. Error Handling (10 functions)

**Purpose:** Error detection, reporting, logging, and recovery.

**Rationale:** Groups all error management together. Includes model validation checks that return error codes.

| Function | Role |
|----------|------|
| cxf_error | Set error code and message |
| cxf_errorlog | Log error to model |
| cxf_check_nan | Detect NaN values |
| cxf_check_nan_or_inf | Detect NaN or Infinity |
| cxf_checkenv | Validate environment pointer |
| cxf_pivot_check | Validate pivot feasibility |
| cxf_special_check | Check special pivot cases |
| cxf_check_model_flags1 | Check MIP features |
| cxf_check_model_flags2 | Check quadratic features |
| cxf_check_terminate | Check termination flag |

---

### 3. Logging (5 functions)

**Purpose:** Output formatting, message routing, and log callback management.

**Rationale:** Separates I/O concerns from error handling. These functions handle the mechanics of producing output.

| Function | Role |
|----------|------|
| cxf_log_printf | Formatted log output |
| cxf_log10_wrapper | Math helper for log formatting |
| cxf_snprintf_wrapper | Safe string formatting |
| cxf_register_log_callback | Set up log callback |
| cxf_get_logical_processors | System info for logging |

---

### 4. Parameters (4 functions)

**Purpose:** Access solver parameters and tolerances.

**Rationale:** Clean separation of parameter retrieval from parameter storage. These are read-only accessors.

| Function | Role |
|----------|------|
| cxf_getdblparam | Get double parameter value |
| cxf_get_feasibility_tol | Get primal tolerance |
| cxf_get_optimality_tol | Get dual tolerance |
| cxf_get_infinity | Get infinity threshold |

---

### 5. Validation (2 functions)

**Purpose:** Input array validation and type checking.

**Rationale:** Pre-condition checking functions that validate user-provided data before processing.

| Function | Role |
|----------|------|
| cxf_validate_array | Validate numeric arrays |
| cxf_validate_vartypes | Validate variable type array |

---

### 6. Simplex Core (21 functions)

**Purpose:** Main simplex algorithm implementation including iteration, phases, and solution extraction.

**Rationale:** This is the heart of the LP solver. Groups all functions directly involved in simplex iteration.

| Function | Role |
|----------|------|
| cxf_solve_lp | Main LP solve entry point |
| cxf_simplex_init | Initialize simplex state |
| cxf_simplex_setup | Pre-iteration setup |
| cxf_simplex_preprocess | Preprocessing phase |
| cxf_simplex_crash | Initial basis construction |
| cxf_simplex_iterate | Main iteration loop |
| cxf_simplex_step | Single pivot iteration |
| cxf_simplex_step2 | Phase 1 iteration |
| cxf_simplex_step3 | Optimality check iteration |
| cxf_simplex_refine | Solution refinement |
| cxf_simplex_perturbation | Anti-cycling perturbation |
| cxf_simplex_unperturb | Remove perturbations |
| cxf_simplex_phase_end | Phase termination check |
| cxf_simplex_post_iterate | Post-iteration processing |
| cxf_simplex_final | Finalization and cleanup |
| cxf_simplex_cleanup | Resource cleanup |
| cxf_pivot_primal | Execute primal pivot |
| cxf_pivot_bound | Execute bound pivot |
| cxf_pivot_special | Handle special pivots |
| cxf_pivot_with_eta | Create eta update |
| cxf_ratio_test | Determine leaving variable |

---

### 7. Basis Operations (8 functions)

**Purpose:** Basis matrix factorization, updates, and linear system solves.

**Rationale:** Groups LU factorization and the BTRAN/FTRAN operations that maintain and use the basis inverse.

| Function | Role |
|----------|------|
| cxf_ftran | Forward transformation (Bx=b) |
| cxf_btran | Backward transformation (y'B=c') |
| cxf_basis_refactor | Full LU refactorization |
| cxf_basis_snapshot | Save basis state |
| cxf_basis_diff | Compare basis states |
| cxf_basis_equal | Test basis equality |
| cxf_basis_validate | Verify basis integrity |
| cxf_basis_warm | Warm start from basis |

---

### 8. Pricing (6 functions)

**Purpose:** Variable selection for entering the basis (pricing).

**Rationale:** Separates the pricing strategy from the main simplex loop. Includes partial pricing and steepest edge.

| Function | Role |
|----------|------|
| cxf_pricing_init | Initialize pricing structures |
| cxf_pricing_candidates | Generate candidate list |
| cxf_pricing_steepest | Steepest edge pricing |
| cxf_pricing_update | Update after pivot |
| cxf_pricing_invalidate | Invalidate pricing cache |
| cxf_pricing_step2 | Phase-specific pricing |

---

### 9. Matrix Operations (7 functions)

**Purpose:** Sparse matrix operations, vector arithmetic, and data format conversion.

**Rationale:** Low-level linear algebra operations used throughout the solver.

| Function | Role |
|----------|------|
| cxf_matrix_multiply | Sparse matrix-vector product |
| cxf_dot_product | Vector inner product |
| cxf_vector_norm | Compute vector norms |
| cxf_build_row_major | Convert to row-major format |
| cxf_prepare_row_data | Prepare row access structures |
| cxf_finalize_row_data | Finalize row structures |
| cxf_sort_indices | Sort sparse indices |

---

### 10. Solver State (4 functions)

**Purpose:** High-level solver state initialization and cleanup.

**Rationale:** Manages the overall SolverContext structure that coordinates all solver components.

| Function | Role |
|----------|------|
| cxf_init_solve_state | Initialize solve state |
| cxf_cleanup_solve_state | Clean up solve state |
| cxf_cleanup_helper | Bound tightening helper |
| cxf_extract_solution | Extract solution values |

---

### 11. Timing (5 functions)

**Purpose:** Performance measurement and profiling.

**Rationale:** Isolated timing functionality for measuring solver phases.

| Function | Role |
|----------|------|
| cxf_get_timestamp | Get high-resolution time |
| cxf_timing_start | Start timing section |
| cxf_timing_end | End timing section |
| cxf_timing_pivot | Time pivot operations |
| cxf_timing_update | Update timing statistics |
| cxf_timing_refactor | Time refactorization |

**Note:** cxf_timing_refactor grouped here (6 total) based on primary purpose.

---

### 12. Threading (7 functions)

**Purpose:** Thread management, synchronization, and CPU detection.

**Rationale:** All concurrency-related functionality in one module.

| Function | Role |
|----------|------|
| cxf_get_threads | Get configured thread count |
| cxf_set_thread_count | Set active thread count |
| cxf_get_physical_cores | Detect physical cores |
| cxf_acquire_solve_lock | Acquire solve-level lock |
| cxf_release_solve_lock | Release solve-level lock |
| cxf_env_acquire_lock | Acquire environment lock |
| cxf_leave_critical_section | Release critical section |

---

### 13. Callbacks (6 functions)

**Purpose:** User callback infrastructure and termination handling.

**Rationale:** Groups callback registration, invocation, and termination checking.

| Function | Role |
|----------|------|
| cxf_init_callback_struct | Initialize callback state |
| cxf_reset_callback_state | Reset to initial state |
| cxf_pre_optimize_callback | Pre-optimization callback |
| cxf_post_optimize_callback | Post-optimization callback |
| cxf_callback_terminate | Request termination |
| cxf_set_terminate | Set termination flag |

---

### 14. Model Analysis (6 functions)

**Purpose:** Analyze model characteristics and compute statistics.

**Rationale:** Functions that examine model structure without modifying it.

| Function | Role |
|----------|------|
| cxf_is_mip_model | Check for integer variables |
| cxf_is_quadratic | Check for QP objective |
| cxf_is_socp | Check for conic constraints |
| cxf_coefficient_stats | Compute coefficient statistics |
| cxf_compute_coef_stats | Compute detailed stats |
| cxf_presolve_stats | Log presolve statistics |

---

### 15. Crossover (2 functions)

**Purpose:** Convert interior-point solution to basic solution.

**Rationale:** Specialized functionality for barrier-to-simplex transition.

| Function | Role |
|----------|------|
| cxf_crossover | Main crossover algorithm |
| cxf_crossover_bounds | Bound snapping phase |

---

### 16. Utilities (10 functions)

**Purpose:** Miscellaneous helper functions.

**Rationale:** Functions that don't fit cleanly into other modules.

| Function | Role |
|----------|------|
| cxf_fix_variable | Fix variable to bound |
| cxf_quadratic_adjust | Adjust quadratic terms |
| cxf_generate_seed | Generate random seed |
| cxf_floor_ceil_wrapper | Floor/ceiling wrapper |
| cxf_misc_utility | Miscellaneous utility |
| cxf_is_multi_obj | Check multi-objective |
| cxf_get_genconstr_name | Get constraint name |
| cxf_get_qconstr_data | Get quadratic data |
| cxf_count_genconstr_types | Count constraint types |
| cxf_extract_solution | Extract solution values |

**Note:** cxf_extract_solution appears in both Solver State and Utilities based on dual role.

---

### 17. Model API (30 functions)

**Purpose:** Public API functions exposed to users.

**Rationale:** All externally-visible functions that form the Convexfeld C API.

| Function | Category |
|----------|----------|
| cxf_loadenv | Environment |
| cxf_emptyenv | Environment |
| cxf_freeenv | Environment |
| cxf_newmodel | Model |
| cxf_freemodel | Model |
| cxf_copymodel | Model |
| cxf_updatemodel | Model |
| cxf_addvar | Variables |
| cxf_addvars | Variables |
| cxf_delvars | Variables |
| cxf_addconstr | Constraints |
| cxf_addconstrs | Constraints |
| cxf_addqpterms | Quadratic |
| cxf_addqconstr | Quadratic |
| cxf_addgenconstrIndicator | General |
| cxf_chgcoeffs | Modification |
| cxf_getconstrs | Query |
| cxf_getcoeff | Query |
| cxf_optimize | Solving |
| cxf_optimize_internal | Solving |
| cxf_terminate | Control |
| cxf_getintattr | Attributes |
| cxf_getdblattr | Attributes |
| cxf_setintparam | Parameters |
| cxf_getintparam | Parameters |
| cxf_read | I/O |
| cxf_write | I/O |
| cxf_version | Info |
| cxf_geterrormsg | Error |
| cxf_setcallbackfunc | Callbacks |

---

## Reassignment Notes

The following functions were reassigned from the original proposal:

| Function | Original | Final | Justification |
|----------|----------|-------|---------------|
| cxf_check_nan | Validation | Error Handling | Returns error status; fits error detection pattern |
| cxf_check_nan_or_inf | Validation | Error Handling | Returns error status; fits error detection pattern |
| cxf_pivot_check | Ratio Test | Error Handling | Validation function, not ratio computation |
| cxf_special_check | Statistics | Error Handling | Returns boolean check result |
| cxf_check_model_flags1 | - | Error Handling | Validation function |
| cxf_check_model_flags2 | - | Error Handling | Validation function |
| cxf_pivot_primal | Pivot | Simplex Core | Core simplex operation |
| cxf_pivot_bound | Ratio Test | Simplex Core | Core simplex operation |
| cxf_pivot_special | Ratio Test | Simplex Core | Core simplex operation |
| cxf_pivot_with_eta | Basis | Simplex Core | Part of pivot execution |
| cxf_ratio_test | Ratio Test | Simplex Core | Core simplex operation |
| cxf_timing_refactor | Basis | Timing | Primary purpose is timing |
| cxf_get_logical_processors | Threading | Logging | Used for system info output |
| cxf_extract_solution | Simplex | Utilities | Also used outside simplex |

---

## Module Statistics Summary

| Module | Functions | Complexity Distribution |
|--------|-----------|------------------------|
| Memory Management | 9 | 9 complex |
| Error Handling | 10 | 1 medium, 9 complex |
| Logging | 5 | 5 complex |
| Parameters | 4 | 3 medium, 1 complex |
| Validation | 2 | 2 complex |
| Simplex Core | 21 | 21 complex |
| Basis Operations | 8 | 8 complex |
| Pricing | 6 | 2 medium, 4 complex |
| Matrix Operations | 7 | 1 medium, 6 complex |
| Solver State | 4 | 4 complex |
| Timing | 5 | 5 complex |
| Threading | 7 | 1 medium, 6 complex |
| Callbacks | 6 | 6 complex |
| Model Analysis | 6 | 2 medium, 4 complex |
| Crossover | 2 | 2 complex |
| Utilities | 10 | 10 complex |
| Model API | 30 | 3 medium, 27 complex |
| **Total** | **142** | **13 medium, 129 complex** |

---

## Specification Order Recommendation

For Phase B (function specifications), process modules in this order:

1. **Memory Management** - Foundation layer, no internal deps
2. **Parameters** - Simple accessors
3. **Validation** - Input checking
4. **Error Handling** - Depends on memory only
5. **Logging** - Depends on error handling
6. **Threading** - Standalone concurrency
7. **Timing** - Performance measurement
8. **Matrix Operations** - Linear algebra primitives
9. **Model Analysis** - Read-only analysis
10. **Basis Operations** - LU factorization
11. **Callbacks** - User interaction
12. **Solver State** - State management
13. **Pricing** - Variable selection
14. **Simplex Core** - Main algorithm (largest)
15. **Crossover** - IPM transition
16. **Utilities** - Miscellaneous
17. **Model API** - Public interface (last, depends on all)

---
*Convexfeld LP Solver Specification*
*Based on published optimization literature*
