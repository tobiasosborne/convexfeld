# Cleanroom Specification Plan v2

**Created:** 2026-02-06
**Status:** PLAN READY - Awaiting execution
**Replaces:** cleanroom/specs/ (v1, invalidated by ~36% fabrication in source)

---

## Table of Contents

1. [Objectives](#1-objectives)
2. [Architecture](#2-specification-architecture)
3. [Task Breakdown](#3-complete-task-breakdown)
4. [Dependency Graph](#4-dependency-graph)
5. [Subagent Operating Instructions](#5-subagent-operating-instructions)
6. [Function-to-Module Mapping](#6-function-to-module-mapping)

---

## 1. Objectives

### Non-Negotiable Requirement 1: 100% Clean IP

No one must be able to point to any specification and trace it to original proprietary code.
Every specification must pass the Clean Room Test defined in `cleanroom/v2/IP_RULES.md`.

### Non-Negotiable Requirement 2: Reimplementable to Near-Perfect Copy

Given ONLY these specifications (no access to original software), a competent engineering
team must be able to implement an LP solver that produces identical results on all standard
LP benchmark instances. The specs must document every architectural choice, algorithm
variant, numerical consideration, and edge case.

### How Both Requirements Are Satisfied Simultaneously

- Algorithms are described by citing PUBLISHED TECHNIQUES (clean) with specific CHOICES
  documented as design decisions (reimplementable)
- Data structures are described SEMANTICALLY by field purpose (clean) with complete field
  lists and invariants (reimplementable)
- Behavior is described as CONTRACTS with pre/postconditions (clean) at sufficient detail
  to determine exact behavior (reimplementable)
- Constants are described by ALGORITHMIC ROLE (clean) with standard-practice values
  (reimplementable)

---

## 2. Specification Architecture

### Five Layers (Bottom-Up)

```
Layer 5: Reference (constants, error codes, parameters, defaults)
Layer 4: Integration (pipeline, threading, callbacks, error propagation)
Layer 3: Module Contracts (behavioral specs for all 159 functions)
Layer 2: Algorithms (8 core algorithms with published references)
Layer 1: Data Model (10+ core data structures, semantic descriptions)
```

### Output Structure

```
cleanroom/v2/specs/
├── data-model/          # Layer 1 (10 specs)
├── algorithms/          # Layer 2 (8 specs)
├── modules/             # Layer 3 (35 specs)
├── integration/         # Layer 4 (5 specs)
└── reference/           # Layer 5 (3 specs)
```

### Supporting Documents

- `cleanroom/v2/IP_RULES.md` - IP cleanliness rules (MUST READ before any task)
- `cleanroom/v2/TEMPLATES.md` - Spec templates for each layer
- `cleanroom/v2/PLAN.md` - This document

---

## 3. Complete Task Breakdown

### Phase 0: Framework Setup (3 tasks)

| Task ID | Title | Output File | Description |
|---------|-------|-------------|-------------|
| P0.1 | Create directory structure & templates | `cleanroom/v2/` tree | Create all directories, write IP_RULES.md and TEMPLATES.md |
| P0.2 | Create function-to-module mapping | `cleanroom/v2/FUNCTION_MAP.md` | Map all 159 analyzed functions to their Layer 3 module assignments |
| P0.3 | Archive v1 specs | `cleanroom/v1-archive/` | Move `cleanroom/specs/` to `cleanroom/v1-archive/`, preserve for reference |

**Status:** P0.1 COMPLETE (done in this session). P0.2, P0.3 ready to execute.

---

### Phase 1: Data Model - Layer 1 (10 tasks)

Each task reads the relevant learnings docs + analyzed source files that USE the structure,
then writes a semantic specification following the Layer 1 template.

| Task ID | Title | Output File | Key Input Files |
|---------|-------|-------------|-----------------|
| P1.01 | Environment structure spec | `specs/data-model/environment.md` | `docs/learnings/data-structures/cxf_env_layout.md`, analyzed files using env fields |
| P1.02 | Model structure spec | `specs/data-model/model.md` | `docs/reference/structures/CxfModel.md`, analyzed files using model fields |
| P1.03 | MatrixData structure spec | `specs/data-model/matrix_data.md` | `docs/learnings/data-structures/MatrixData_scaling.md`, `docs/reference/structures/MatrixData.md` |
| P1.04 | SolverState structure spec | `specs/data-model/solver_state.md` | `docs/learnings/data-structures/SolverState_layout.md` |
| P1.05 | BasisState structure spec | `specs/data-model/basis_state.md` | Basis-related analyzed files, learnings on PFI/eta |
| P1.06 | PricingState structure spec | `specs/data-model/pricing_state.md` | `docs/learnings/data-structures/PricingState_layout.md` |
| P1.07 | CallbackState structure spec | `specs/data-model/callback_state.md` | `docs/learnings/functions/cxf_callback_functions_analysis.md` |
| P1.08 | EtaVector structure spec | `specs/data-model/eta_vector.md` | `docs/learnings/functions/cxf_alloc_eta_analysis.md`, basis files |
| P1.09 | SolutionData structure spec | `specs/data-model/work_arrays.md` | `docs/learnings/functions/cxf_alloc_work_arrays_analysis.md` |
| P1.10 | Supporting structures spec | `specs/data-model/supporting_structures.md` | IISState, ModificationTracker, WarmStartData, CrossoverState learnings |

**Per-task instructions for Phase 1:**

```
1. Read IP_RULES.md and TEMPLATES.md (Layer 1 template)
2. Read the listed input files
3. Read 3-5 analyzed source files that heavily use this structure
   (to understand field usage patterns)
4. Write the spec using ONLY semantic descriptions
5. CRITICAL: Convert all byte offsets to semantic field names
6. CRITICAL: Do NOT include any hex values, addresses, or binary layout info
7. DO include: complete field list, types, purposes, valid values, invariants,
   lifecycle, relationships, thread safety
8. Run the IP verification checklist from IP_RULES.md
9. Save learnings to docs/learnings/ if any new discoveries are made
```

---

### Phase 2: Algorithm Specifications - Layer 2 (8 tasks)

Each task writes a standalone algorithm specification citing published literature,
with specific design choices documented.

| Task ID | Title | Output File | Key Input Files | Published References |
|---------|-------|-------------|-----------------|---------------------|
| P2.1 | Revised Simplex Method | `specs/algorithms/revised_simplex.md` | `docs/learnings/algorithms/lp_orchestration.md`, simplex analyzed files | Dantzig (1963), Maros (2003) |
| P2.2 | Product Form of Inverse | `specs/algorithms/product_form_inverse.md` | Basis analyzed files, eta allocation | Dantzig (1963), Forrest & Tomlin (1972) |
| P2.3 | Multi-Level Partial Pricing | `specs/algorithms/partial_pricing.md` | `docs/learnings/data-structures/PricingState_layout.md`, all pricing_* files | Goldfarb & Reid (1977) |
| P2.4 | Harris Ratio Test + BFRT | `specs/algorithms/harris_ratio_test.md` | cxf_simplex_step.c, cxf_simplex_step2.c | Harris (1973), Koberstein (2008) |
| P2.5 | Crash Basis Construction | `specs/algorithms/crash_basis.md` | cxf_simplex_crash.c, learnings | Maros (2003) Ch. 9 |
| P2.6 | Perturbation / Anti-Cycling | `specs/algorithms/perturbation.md` | cxf_simplex_perturbation.c, learnings | Wolfe (1963), Bland (1977) |
| P2.7 | Crossover (Barrier to Simplex) | `specs/algorithms/crossover.md` | `docs/learnings/algorithms/crossover.md`, crossover files | Megiddo (1991) |
| P2.8 | Bound Propagation | `specs/algorithms/bound_propagation.md` | cxf_propagate_bounds.c, cxf_propagate_bounds.c | Savelsbergh (1994) |

**Per-task instructions for Phase 2:**

```
1. Read IP_RULES.md and TEMPLATES.md (Layer 2 template)
2. Read the listed input files (learnings + analyzed source)
3. Web search for the published reference to understand the standard algorithm
4. Write the spec describing:
   a. The STANDARD algorithm (with citation)
   b. The specific VARIANT/CHOICES made (as design decisions)
   c. Numerical considerations and tolerances
   d. Termination conditions
   e. Edge cases
5. CRITICAL: Algorithm description must be derivable from published literature +
   documented design choices. No reverse-engineered code patterns.
6. CRITICAL: Pseudocode uses mathematical notation, NOT C-like syntax
7. Run the IP verification checklist
8. Save learnings
```

---

### Phase 3: Module Behavioral Contracts - Layer 3 (35 tasks)

Each task covers a group of 2-8 related functions. The task reads the analyzed source
for each function, reads relevant Layer 1 (data model) and Layer 2 (algorithm) specs,
then writes behavioral contracts.

#### 3.01-3.05: Memory & State Management

| Task ID | Title | Functions | Output File |
|---------|-------|-----------|-------------|
| P3.01 | Memory Primitives | cxf_calloc, cxf_realloc, cxf_vector_free, cxf_model_alloc | `specs/modules/memory_primitives.md` |
| P3.02 | Allocation Helpers | cxf_alloc_eta, cxf_alloc_work_arrays, cxf_setup_resources | `specs/modules/allocation_helpers.md` |
| P3.03 | State Initialization | cxf_init_solve_state, cxf_free_warmstart_basis, cxf_free_work_arrays | `specs/modules/state_initialization.md` |
| P3.05 | State Cleanup - Buffers | cxf_free_callback_state, cxf_free_solution_pool, cxf_clear_solution, cxf_clear_pending_buffer, cxf_reset_pending_buffer | `specs/modules/state_cleanup_buffers.md` |

#### 3.06-3.08: Validation

| Task ID | Title | Functions | Output File |
|---------|-------|-----------|-------------|
| P3.06 | Model Type Checking | cxf_is_quadratic, cxf_is_socp, cxf_is_socp_internal, cxf_check_model_flags1, cxf_check_model_flags2 | `specs/modules/model_type_checking.md` |
| P3.08 | Data Validation | cxf_validate_array, cxf_validate_vartypes, cxf_validate_solution, cxf_special_check | `specs/modules/data_validation.md` |

#### 3.09-3.10: Error & Logging

| Task ID | Title | Functions | Output File |
|---------|-------|-----------|-------------|
| P3.09 | Error Handling | cxf_error_env, cxf_error_model, cxf_set_error_message, cxf_env_set_status | `specs/modules/error_handling.md` |
| P3.10 | Logging | cxf_set_error_string, cxf_log, cxf_register_log_callback | `specs/modules/logging.md` |

#### 3.11-3.13: Infrastructure

| Task ID | Title | Functions | Output File |
|---------|-------|-----------|-------------|
| P3.11 | Threading & Sync | cxf_save_locale_state, cxf_release_solve_lock, cxf_env_acquire_lock, cxf_get_logical_processors, cxf_get_physical_cores, cxf_get_threads, cxf_validate_thread_count | `specs/modules/threading_sync.md` |
| P3.12 | Thread Init & Thunks | cxf_init_thread_local, LeaveCriticalSection, LeaveCriticalSection_thunk | `specs/modules/thread_init_thunks.md` |
| P3.13 | Callbacks | cxf_init_callback_struct, cxf_callback_terminate, cxf_pre_optimize_hook, cxf_post_optimize_hook, cxf_getconstrs_callback, cxf_copy_env_callbacks | `specs/modules/callbacks.md` |

#### 3.14-3.15: Matrix Operations

| Task ID | Title | Functions | Output File |
|---------|-------|-----------|-------------|
| P3.14 | Matrix Core | cxf_matrix_setup, cxf_prepare_row_data, cxf_build_row_major, cxf_sort_by_values | `specs/modules/matrix_core.md` |
| P3.15 | Matrix Finalization | cxf_finalize_row_data (6 parts) | `specs/modules/matrix_finalization.md` |

#### 3.16: Basis Operations

| Task ID | Title | Functions | Output File |
|---------|-------|-----------|-------------|
| P3.16 | Basis Operations | cxf_fix_variables_at_bounds, cxf_progress_snapshot, cxf_basis_diff, cxf_basis_warm, cxf_pivot_with_eta | `specs/modules/basis_operations.md` |

#### 3.17-3.18: Pricing System

| Task ID | Title | Functions | Output File |
|---------|-------|-----------|-------------|
| P3.17 | Pricing Core | cxf_pricing_candidates, cxf_pricing_update, cxf_pricing_update_var, cxf_pricing_update_constr, cxf_pricing_invalidate | `specs/modules/pricing_core.md` |
| P3.18 | Pricing Support | cxf_pricing_mark_dirty, cxf_pricing_mark_constr_dirty, cxf_pricing_cascade_update, cxf_pricing_end_level, cxf_pricing_set_level, cxf_pricing_get_var_stats, cxf_pricing_get_constr_stats, cxf_pricing_get_constr_candidates | `specs/modules/pricing_support.md` |

#### 3.19: Pivot Operations

| Task ID | Title | Functions | Output File |
|---------|-------|-----------|-------------|
| P3.19 | Pivot Operations | cxf_pivot_bound, cxf_pivot_primal, cxf_pivot_special, cxf_pivot_check, cxf_pivot_update | `specs/modules/pivot_operations.md` |

#### 3.20-3.22: Simplex Core

| Task ID | Title | Functions | Output File |
|---------|-------|-----------|-------------|
| P3.20 | Simplex Iteration | cxf_log_iteration_progress, cxf_simplex_step, cxf_simplex_step2, cxf_simplex_step3, cxf_simplex_post_iterate | `specs/modules/simplex_iteration.md` |
| P3.21 | Simplex Phases | cxf_simplex_crash, cxf_simplex_perturbation, cxf_simplex_preprocess, cxf_simplex_setup, cxf_simplex_phase_end, cxf_simplex_refine | `specs/modules/simplex_phases.md` |
| P3.22 | Simplex Lifecycle | cxf_simplex_postsolve, cxf_simplex_final, cxf_simplex_init (4 parts) | `specs/modules/simplex_lifecycle.md` |

#### 3.23: Crossover

| Task ID | Title | Functions | Output File |
|---------|-------|-----------|-------------|
| P3.23 | Crossover | cxf_crossover, cxf_crossover_bounds (4 parts) | `specs/modules/crossover.md` |

#### 3.24-3.28: Solve Chain

| Task ID | Title | Functions | Output File |
|---------|-------|-----------|-------------|
| P3.24 | Solve Entry & Dispatch | cxf_optimize, cxf_optimize_internal, cxf_solve_entry, cxf_solve_dispatch, cxf_solve_no_callbacks, cxf_solve_with_callbacks | `specs/modules/solve_entry.md` |
| P3.25 | Solve LP Core | cxf_solve_lp (6 parts), cxf_solver_dispatch (6 parts) | `specs/modules/solve_lp_core.md` |
| P3.26 | Solve Barrier & Concurrent | cxf_solve_barrier, cxf_solve_concurrent (6 parts), cxf_solve_concurrent_distributed | `specs/modules/solve_barrier_concurrent.md` |

#### 3.29-3.31: Solution & Lifecycle

| Task ID | Title | Functions | Output File |
|---------|-------|-----------|-------------|
| P3.29 | Solution Processing | cxf_process_lp_solution, cxf_uncrush_solution, cxf_wire_result_attributes, cxf_compute_gap, cxf_scale_objval, cxf_copy_solution | `specs/modules/solution_processing.md` |
| P3.30 | Environment Lifecycle | cxf_env_create_internal, cxf_env_free_internal, cxf_env_finalize (8 parts), cxf_env_load_logfile, cxf_env_update_active_model | `specs/modules/environment_lifecycle.md` |
| P3.31 | Model Lifecycle | cxf_model_create_internal, cxf_env_model_cleanup, cxf_update_model_manager, cxf_model_apply_modifications (4 parts) | `specs/modules/model_lifecycle.md` |

#### 3.32-3.35: Support Functions

| Task ID | Title | Functions | Output File |
|---------|-------|-----------|-------------|
| P3.33 | Statistics & Diagnostics | cxf_presolve_stats, cxf_coefficient_stats, cxf_compute_coef_stats, cxf_gencon_stats, cxf_compute_violations, cxf_compute_fingerprint, cxf_get_timestamp | `specs/modules/statistics_diagnostics.md` |
| P3.34 | Cleanup Utilities | cxf_propagate_bounds, cxf_cleanup_coeff_change, cxf_cleanup_optimization, cxf_propagate_bounds | `specs/modules/cleanup_utilities.md` |
| P3.35 | Query Utilities | cxf_get_genconstr_name, cxf_get_qconstr_data, cxf_count_genconstr_types, cxf_has_history, cxf_fix_variable | `specs/modules/query_utilities.md` |

**Per-task instructions for Phase 3:**

```
1. Read IP_RULES.md and TEMPLATES.md (Layer 3 template)
2. Read relevant Layer 1 specs (data structures these functions use)
3. Read relevant Layer 2 specs (algorithms these functions implement)
4. For EACH function in the group:
   a. Read src/analyzed/lp/{FunctionName}.c
   b. Read docs/learnings/functions/{FunctionName}_analysis.md (if exists)
   c. Write the behavioral contract following the template
5. CRITICAL: Describe WHAT each function does, not HOW (no C code)
6. CRITICAL: Use semantic type names from Layer 1, not raw types
7. CRITICAL: Reference algorithm names from Layer 2, not algorithm details
8. Run the IP verification checklist on the complete module spec
9. Save learnings
```

---

### Phase 4: Integration Specifications - Layer 4 (5 tasks)

| Task ID | Title | Output File | Key Inputs |
|---------|-------|-------------|------------|
| P4.1 | Optimization Pipeline | `specs/integration/optimization_pipeline.md` | Layer 3 solve_entry, solve_lp_core, simplex_iteration, solution_processing |
| P4.2 | Parameter System | `specs/integration/parameter_system.md` | All Layer 3 modules that read parameters |
| P4.3 | Error Propagation | `specs/integration/error_propagation.md` | Layer 3 error_handling, all modules with error returns |
| P4.4 | Callback Protocol | `specs/integration/callback_protocol.md` | Layer 3 callbacks, solve_entry |
| P4.5 | Threading Model | `specs/integration/threading_model.md` | Layer 3 threading_sync, solve_barrier_concurrent |

**Per-task instructions for Phase 4:**

```
1. Read IP_RULES.md and TEMPLATES.md (Layer 4 template)
2. Read ALL relevant Layer 3 module specs (listed in Key Inputs)
3. Read relevant Layer 1 and Layer 2 specs
4. Synthesize the integration specification
5. Focus on HOW MODULES CONNECT, not internal module behavior
6. Include state transition diagrams where appropriate
7. Run IP verification checklist
8. Save learnings
```

---

### Phase 5: Reference Documents - Layer 5 (3 tasks)

| Task ID | Title | Output File | Key Inputs |
|---------|-------|-------------|------------|
| P5.1 | Error & Status Codes | `specs/reference/error_status_codes.md` | `docs/learnings/functions/cxf_error_logging_analysis.md`, all error-handling analyzed files |
| P5.2 | Parameters & Defaults | `specs/reference/parameters_defaults.md` | cxf_env_finalize, parameter-reading functions |
| P5.3 | Tolerances & Constants | `specs/reference/tolerances_constants.md` | Simplex, pricing, pivot analyzed files for tolerance usage |

**Per-task instructions for Phase 5:**

```
1. Read IP_RULES.md and TEMPLATES.md (Layer 5 template)
2. Read the listed input files
3. Compile reference tables with:
   - Semantic names (NOT hex values)
   - Categories
   - Descriptions of purpose
   - Standard/typical values from LP literature (NOT from binary)
4. Run IP verification checklist
5. Save learnings
```

---

### Phase 6: Validation (5 tasks)

| Task ID | Title | Output File | What It Validates |
|---------|-------|-------------|-------------------|
| P6.1 | IP Cleanliness Audit | `validation/ip_audit.md` | Read EVERY spec, check against IP_RULES.md |
| P6.2 | Completeness Audit | `validation/completeness_audit.md` | Verify all 159 functions have contracts, all structures have specs |
| P6.3 | Reimplementability Review | `validation/reimplementability_review.md` | For each module, ask: "Could I implement this from the spec alone?" |
| P6.4 | Cross-Reference Check | `validation/cross_reference.md` | Verify all inter-spec references are consistent |
| P6.5 | Algorithm Accuracy Review | `validation/algorithm_accuracy.md` | Verify algorithm specs match analyzed source behavior |

**Per-task instructions for Phase 6:**

```
1. Read ALL specs in the relevant category
2. Apply the specific validation criteria
3. For EACH finding, document:
   a. File and section with the issue
   b. What the issue is
   c. Recommended fix
4. Assign severity: CRITICAL (blocks release), HIGH (should fix), LOW (nice to fix)
5. The REIMPLEMENTABILITY review (P6.3) is the most important:
   - For each module spec, mentally simulate implementing it
   - Identify any gaps, ambiguities, or missing information
   - Document what additional detail would be needed
```

---

### Phase 7: Final Assembly (2 tasks)

| Task ID | Title | Output File | Description |
|---------|-------|-------------|-------------|
| P7.1 | Fix validation findings | Various spec files | Address all CRITICAL and HIGH findings from Phase 6 |
| P7.2 | Consolidated specification | `output/SPECIFICATION.md` | Single document combining all specs with table of contents |

---

## 4. Dependency Graph

### Phase Dependencies

```
P0 (Setup)
 ├── P1 (Data Model)      -- needs templates
 ├── P2 (Algorithms)       -- needs templates
 └── P5 (Reference)        -- needs templates, independent of P1/P2

P1 (Data Model) ─┐
                  ├── P3 (Module Contracts) -- needs data model + algorithms
P2 (Algorithms) ──┘

P3 (Module Contracts) ── P4 (Integration) -- needs all module contracts

P1 + P2 + P3 + P4 + P5 ── P6 (Validation) -- needs everything

P6 (Validation) ── P7 (Final Assembly)
```

### Within-Phase Parallelism

- **Phase 1:** All 10 tasks are INDEPENDENT (can run in parallel)
- **Phase 2:** All 8 tasks are INDEPENDENT (can run in parallel)
- **Phase 3:** All 35 tasks are INDEPENDENT (can run in parallel)
  - BUT each depends on relevant P1.xx and P2.x tasks
  - Specifically:
    - P3.01-P3.05 (memory/state) depend on P1.01, P1.02, P1.04
    - P3.06-P3.08 (validation) depend on P1.02, P1.03
    - P3.09-P3.10 (error/logging) depend on P1.01
    - P3.11-P3.13 (infra) depend on P1.01, P1.07
    - P3.14-P3.15 (matrix) depend on P1.03
    - P3.16 (basis) depends on P1.05, P1.08, P2.2
    - P3.17-P3.18 (pricing) depend on P1.06, P2.3
    - P3.19 (pivot) depends on P1.05, P2.4
    - P3.20-P3.22 (simplex) depend on P1.04, P1.05, P2.1, P2.4, P2.5, P2.6
    - P3.23 (crossover) depends on P1.04, P2.7
    - P3.24-P3.28 (solve chain) depend on P1.01, P1.02
    - P3.29 (solution) depends on P1.02, P1.04
    - P3.30-P3.31 (lifecycle) depend on P1.01, P1.02
    - P3.32-P3.35 (support) depend on P1.02, P1.03
- **Phase 4:** All 5 tasks can run in parallel (each reads different module specs)
- **Phase 5:** All 3 tasks are INDEPENDENT
- **Phase 6:** P6.1-P6.5 can run in parallel
- **Phase 7:** P7.1 before P7.2

### Critical Path

```
P0 → P1 (parallel) → P3 (parallel, with P2 also ready) → P4 (parallel) → P6 → P7
          ↗ P2 (parallel) ↗                                   ↗ P5 (parallel) ↗
```

Minimum sequential depth: 6 phases (P0 → P1 → P3 → P4 → P6 → P7)
Maximum parallelism: 35 tasks (Phase 3)

---

## 5. Subagent Operating Instructions

### Before Starting Any Task

```
1. Read cleanroom/v2/IP_RULES.md (MANDATORY, non-negotiable)
2. Read cleanroom/v2/TEMPLATES.md (the template for your layer)
3. Read this PLAN.md section for your specific task
4. Check that all dependency tasks are COMPLETE (their output files exist)
5. If a dependency is missing, STOP and report the blocker
```

### During Task Execution

```
1. Read ALL listed input files for your task
2. For Layer 3 tasks: read the actual analyzed source code for each function
3. Write the spec following the template EXACTLY
4. After writing, re-read your spec and run the IP verification checklist
5. Fix any IP violations before saving
```

### After Completing a Task

```
1. Save the spec to the listed output file
2. Save any new learnings to docs/learnings/
3. Update HANDOFF.md with:
   - Task ID completed
   - Any issues or concerns found
   - Suggested improvements for dependent tasks
4. Close the corresponding beads issue: bd close <issue-id>
```

### What To Do If Stuck

```
- If analyzed source is unclear: document the ambiguity in the spec,
  mark the section with [NEEDS VERIFICATION]
- If IP rules conflict with reimplementability: err on the side of
  IP cleanliness, document what detail was omitted and why
- If a function's behavior cannot be determined from source: describe
  what IS known, mark unknowns with [UNDETERMINED]
- NEVER fabricate behavior. Better to have a gap than a hallucination.
```

### Quality Bar

Every spec must satisfy ALL of:
1. Passes IP verification checklist (zero violations)
2. A competent C developer could implement the described behavior
3. No sentences that require access to the original binary to verify
4. All algorithms reference published literature
5. All data structures use semantic field descriptions
6. All functions have complete contracts (pre/post/side effects/errors)

---

## 6. Function-to-Module Mapping

### Complete Assignment (159 functions → 35 modules)

**P3.01 - Memory Primitives (4)**
cxf_calloc, cxf_realloc, cxf_vector_free, cxf_model_alloc

**P3.02 - Allocation Helpers (3)**
cxf_alloc_eta, cxf_alloc_work_arrays, cxf_setup_resources

**P3.03 - State Initialization (3)**
cxf_init_solve_state, cxf_free_warmstart_basis, cxf_free_work_arrays

**P3.04 - State Cleanup Solver (6)**
cxf_cleanup_solve_state, cxf_free_attribute_table, cxf_free_basis_state,

**P3.05 - State Cleanup Buffers (5)**
cxf_free_callback_state, cxf_free_solution_pool, cxf_clear_solution,
cxf_clear_pending_buffer, cxf_reset_pending_buffer

**P3.06 - Model Type Checking (5)**
cxf_is_quadratic, cxf_is_socp, cxf_is_socp_internal,
cxf_check_model_flags1, cxf_check_model_flags2

**P3.07 - Input Validation (7)**
cxf_check_env, cxf_check_nan, cxf_check_is_finite, cxf_check_label,

**P3.08 - Data Validation (4)**
cxf_validate_array, cxf_validate_vartypes, cxf_validate_solution, cxf_special_check

**P3.09 - Error Handling (4)**
cxf_error_env, cxf_error_model, cxf_set_error_message, cxf_env_set_status

**P3.10 - Logging (3)**
cxf_set_error_string, cxf_log, cxf_register_log_callback

**P3.11 - Threading & Sync (7)**
cxf_save_locale_state, cxf_release_solve_lock, cxf_env_acquire_lock,
cxf_get_logical_processors, cxf_get_physical_cores, cxf_get_threads, cxf_validate_thread_count

**P3.12 - Thread Init & Thunks (3)**
cxf_init_thread_local, LeaveCriticalSection, LeaveCriticalSection_thunk

**P3.13 - Callbacks (6)**
cxf_init_callback_struct, cxf_callback_terminate, cxf_pre_optimize_hook,
cxf_post_optimize_hook, cxf_getconstrs_callback, cxf_copy_env_callbacks

**P3.14 - Matrix Core (4)**
cxf_matrix_setup, cxf_prepare_row_data, cxf_build_row_major, cxf_sort_by_values

**P3.15 - Matrix Finalization (1 multi-part)**
cxf_finalize_row_data (6 parts)

**P3.16 - Basis Operations (5)**
cxf_fix_variables_at_bounds, cxf_progress_snapshot, cxf_basis_diff, cxf_basis_warm, cxf_pivot_with_eta

**P3.17 - Pricing Core (5)**
cxf_pricing_candidates, cxf_pricing_update, cxf_pricing_update_var,
cxf_pricing_update_constr, cxf_pricing_invalidate

**P3.18 - Pricing Support (8)**
cxf_pricing_mark_dirty, cxf_pricing_mark_constr_dirty, cxf_pricing_cascade_update,
cxf_pricing_end_level, cxf_pricing_set_level, cxf_pricing_get_var_stats,
cxf_pricing_get_constr_stats, cxf_pricing_get_constr_candidates

**P3.19 - Pivot Operations (5)**
cxf_pivot_bound, cxf_pivot_primal, cxf_pivot_special, cxf_pivot_check, cxf_pivot_update

**P3.20 - Simplex Iteration (5)**
cxf_log_iteration_progress, cxf_simplex_step, cxf_simplex_step2, cxf_simplex_step3,
cxf_simplex_post_iterate

**P3.21 - Simplex Phases (6)**
cxf_simplex_crash, cxf_simplex_perturbation, cxf_simplex_preprocess,
cxf_simplex_setup, cxf_simplex_phase_end, cxf_simplex_refine

**P3.22 - Simplex Lifecycle (3 + 1 multi-part)**
cxf_simplex_postsolve, cxf_simplex_final, cxf_simplex_init (4 parts)

**P3.23 - Crossover (2, includes multi-part)**
cxf_crossover, cxf_crossover_bounds (4 parts)

**P3.24 - Solve Entry & Dispatch (6)**
cxf_optimize, cxf_optimize_internal, cxf_solve_entry, cxf_solve_dispatch,
cxf_solve_no_callbacks, cxf_solve_with_callbacks

**P3.25 - Solve LP Core (2 multi-part)**
cxf_solve_lp (6 parts), cxf_solver_dispatch (6 parts)

**P3.26 - Solve Barrier & Concurrent (3, includes multi-part)**
cxf_solve_barrier, cxf_solve_concurrent (6 parts),
cxf_solve_concurrent_distributed

**P3.29 - Solution Processing (6)**
cxf_process_lp_solution, cxf_uncrush_solution, cxf_wire_result_attributes,
cxf_compute_gap, cxf_scale_objval, cxf_copy_solution

**P3.30 - Environment Lifecycle (5, includes multi-part)**
cxf_env_create_internal, cxf_env_free_internal, cxf_env_finalize (8 parts),
cxf_env_load_logfile, cxf_env_update_active_model

**P3.31 - Model Lifecycle (4, includes multi-part)**
cxf_model_create_internal, cxf_env_model_cleanup, cxf_update_model_manager,
cxf_model_apply_modifications (4 parts)

**P3.32 - Optimization Preparation (3)**

**P3.33 - Statistics & Diagnostics (7)**
cxf_presolve_stats, cxf_coefficient_stats, cxf_compute_coef_stats, cxf_gencon_stats,
cxf_compute_violations, cxf_compute_fingerprint, cxf_get_timestamp

**P3.34 - Cleanup Utilities (4)**
cxf_propagate_bounds, cxf_cleanup_coeff_change, cxf_cleanup_optimization, cxf_propagate_bounds

**P3.35 - Query Utilities (5)**
cxf_get_genconstr_name, cxf_get_qconstr_data, cxf_count_genconstr_types,
cxf_has_history, cxf_fix_variable

### Function Count Verification

Phase 3 total: 4+3+3+6+5+5+7+4+4+3+7+3+6+4+1+5+5+8+5+5+6+4+2+6+2+3+6+5+4+3+7+4+5 = **152 function slots**

Note: Some multi-part functions count as 1 slot. The 152 includes all single-file functions
plus all multi-part functions as single entries.

---

## Summary Statistics

| Phase | Tasks | Depends On | Can Parallelize |
|-------|-------|------------|-----------------|
| P0: Setup | 3 | nothing | yes (all 3) |
| P1: Data Model | 10 | P0 | yes (all 10) |
| P2: Algorithms | 8 | P0 | yes (all 8) |
| P3: Modules | 35 | P1, P2 | yes (all 35) |
| P4: Integration | 5 | P3 | yes (all 5) |
| P5: Reference | 3 | P0 | yes (all 3) |
| P6: Validation | 5 | P1-P5 | yes (all 5) |
| P7: Assembly | 2 | P6 | P7.1 then P7.2 |
| **TOTAL** | **71** | | |

**Minimum sessions to complete (maximum parallelism):** 6
**Realistic sessions (moderate parallelism):** 15-20
**Sequential execution:** 71 sessions
