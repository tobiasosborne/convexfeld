# V2 Spec Compliance Remediation Plan

**Created:** 2026-02-16
**Based on:** 20 audit reports (docs/audit/01-20)
**Overall compliance:** ~25-30%

---

## Phase 0: Critical Numerics (1-2 days)
**Impact: Fixes solver failures immediately**

### 0.1 Fix Tolerance Values
- Perturbation constants: 1e-6 → 1e-10 (off by 10,000x)
- Pivot tolerance: 1e-10 → 1e-9 (off by 10x)
- Markowitz threshold: 0.1 → 0.0078125 (off by 13x)
- Add 6 missing constants (Harris pivot, minimum pivot, bound equality, adaptive pricing phases)
- Remove hardcoded magic numbers from solve_lp.c and iterate.c
- **Reports:** 16_tolerances_constants.md

### 0.2 Fix Variable Status Encoding
- Current: enum 0-4
- Spec: row indices (>=0) and negative codes (-1 to -4)
- This breaks the simplex algorithm fundamentally
- **Reports:** 01_core_structs.md (V-86)

---

## Phase 1: Name Alignment (2-3 days)
**Impact: Code/spec can be cross-referenced**

### 1.1 Apply 16 Function/Struct Renames
All 16 old v1 names still present (~155+ occurrences, 29 files).
Mechanical find-replace across src/, tests/, include/:
- cxf_simplex_iterate → cxf_log_iteration_progress
- cxf_basis_refactor → cxf_fix_variables_at_bounds
- cxf_basis_snapshot → cxf_progress_snapshot
- WorkArrays → SolutionData
- (12 more - see 19_old_names.md for full list)
- **Reports:** 19_old_names.md

### 1.2 Rename Core Structures
- SolverContext → SolverState
- PricingContext → PricingState
- EtaFactors → EtaVector
- SparseMatrix → MatrixData
- **Reports:** 01_core_structs.md, 02_algo_structs.md

---

## Phase 2: Data Structure Alignment (5-7 days)
**Impact: Correct foundation for algorithm work**

### 2.1 Environment (CxfEnv) - currently ~25% compliant
- Add parameter table infrastructure
- Add threading fields
- Add child environment management
- Add errorCode field
- Add hardware detection fields
- **Reports:** 01_core_structs.md, 08_api_env.md

### 2.2 Model (CxfModel) - currently ~40% compliant
- Add attribute table
- Separate matrix storage from model
- Add modification tracking
- **Reports:** 01_core_structs.md

### 2.3 SolverState - currently ~35% compliant
- Add dual matrix storage (CSR)
- Add steepest edge pricing arrays (4 arrays)
- Add progress tracking fields
- **Reports:** 01_core_structs.md, 02_algo_structs.md

### 2.4 BasisState / PricingState / EtaVector
- Add LU flat structure (lowerTriangular/upperTriangular pointers)
- Add EtaVector variants (PIVOT, VARIABLE_FIX, WARM_START)
- Add dual queue system to PricingState
- Add memory pool for eta vectors
- Add scaling fields to MatrixData
- **Reports:** 02_algo_structs.md

### 2.5 Missing Structures
- IISState (Irreducible Infeasible Set)
- ModificationTracker (lazy update buffer)
- WarmStartData
- CrossoverState
- **Reports:** 03_support_structs.md

### 2.6 CallbackState
- Rename CallbackContext → CallbackState
- Add mutex for thread safety
- Add parent-child relationship tracking
- Fix callback event type constants
- Remove MIP callback code
- **Reports:** 03_support_structs.md, 15_error_param_callback.md

---

## Phase 3: solve_lp.c Decomposition (3-5 days)
**Impact: Enables modular development of all subsequent phases**

### 3.1 Break Monolith (1262 LOC → ~100 LOC orchestrator)
- Extract 10 hallucinated inline functions into proper modules
- setup_phase_one → delegate to P3.21 (simplex_phases)
- transition_to_phase_two → delegate to P3.21
- compute_reduced_costs → delegate to P3.17-P3.18 (pricing)
- check_obvious_infeasibility → delegate to validation module
- Create two-level iteration loop structure (outer/inner)
- **Reports:** 20_solve_lp_deep.md, 14_optimization_pipeline.md

### 3.2 Implement Spec's Pipeline Architecture
- 9-phase pipeline orchestration
- Parameter backup/restore around optimization
- Crash basis construction hook
- Presolve-solve-uncrush cycle
- Crossover integration point
- **Reports:** 14_optimization_pipeline.md

---

## Phase 4: Core Algorithm Completion (7-10 days)
**Impact: Solver actually works correctly**

### 4.1 Two-Level Iteration Loop
- Outer loop with round control
- Inner loop with basis stabilization
- cxf_progress_snapshot + cxf_basis_diff for convergence
- Stall detection via cxf_simplex_post_iterate
- **Reports:** 04_revised_simplex.md, 14_optimization_pipeline.md

### 4.2 BTRAN Step + Reduced Cost Update
- Add Step 5 (BTRAN) to iteration loop - currently completely absent
- Switch from O(n*m) full recomputation to O(n) incremental update
- ~2x performance improvement
- **Reports:** 04_revised_simplex.md (V-04, V-05)

### 4.3 Phase I/II Proper Logic
- Phase determination
- Phase transition with reduced cost recomputation
- Infeasibility detection
- **Reports:** 04_revised_simplex.md (V-01, V-02, V-03)

### 4.4 BFRT Stage 3 (Bound-Flipping)
- Harris ratio test two-pass is implemented (Stages 1-2)
- Stage 3 (long steps with bound flips) completely missing
- Need bound-flip outputs, application logic (negate matrix rows, adjust RHS)
- 20-50% fewer iterations on bounded-variable problems
- **Reports:** 05_ratio_perturbation.md

### 4.5 EXPAND Perturbation Method
- Current: Wolfe 1963 random bound perturbation (WRONG)
- Spec: EXPAND method (Gill 1989) with implied bound analysis
- Need stalling detection infrastructure (basis snapshot comparison)
- Need degenerate variable removal from pricing candidate set
- **Reports:** 05_ratio_perturbation.md

### 4.6 Steepest Edge Weight Maintenance
- All four pricing weight arrays missing from SolverState
- Weight update formula is stub code
- Required for production-quality pricing
- **Reports:** 04_revised_simplex.md, 07_pricing_crash.md

---

## Phase 5: Pricing System Rewrite (7-10 days)
**Impact: Production-quality variable selection**

### 5.1 Multi-Level Neighbor-Expansion Partial Pricing
- Current: classical Dantzig sectional pricing (0% compliance)
- Spec: sophisticated multi-level system with:
  - Dirty marking after pivots (structural adjacency)
  - Dual queue system (constraint + variable queues)
  - Committed/pending split mechanism
  - 1-hop and 2-hop neighbor expansion
  - Adaptive threshold for full scan vs partial expansion
- Complete ground-up rewrite required
- **Reports:** 07_pricing_crash.md, 12_pricing_callbacks_threading.md

### 5.2 Crash Basis
- Current: trivially all-slack
- Spec: structural variable selection with triangularity heuristic
- **Reports:** 07_pricing_crash.md

---

## Phase 6: Infrastructure Systems (8-12 days)
**Impact: Production-quality supporting systems**

### 6.1 Table-Driven Parameter System (3-4 days)
- Replace hardcoded parameter fields with static definition tables
- Add string parameter support
- Add double parameter setter
- Add parameter backup/restore mechanism
- Add child environment inheritance
- Expand from 7 to ~30 parameters
- **Reports:** 15_error_param_callback.md

### 6.2 Error Propagation Model (2-3 days)
- Implement 4 core error reporting functions
- Add predefined error message table
- Add first-error-wins pattern with overwrite control
- Add 21 missing error codes (only 5 of 26 exist)
- Add 9 missing optimization status codes
- **Reports:** 15_error_param_callback.md, 11_memory_error_validation.md, 16_tolerances_constants.md

### 6.3 Thread-Safe Callbacks (2-3 days)
- Add mutex to CallbackContext
- Implement 5 missing WHERE codes
- Implement all 7 callback invocation points (only 2 exist)
- Add callback propagation for model clones
- **Reports:** 15_error_param_callback.md, 12_pricing_callbacks_threading.md

### 6.4 Environment-Scoped Memory (2 days)
- Replace direct stdlib wrappers with env-based tracking
- Add Environment *env parameter to allocation functions
- **Reports:** 11_memory_error_validation.md

### 6.5 Threading (2 days)
- All lock functions currently no-op stubs
- Add locale safety
- Add thread-local state initialization
- **Reports:** 12_pricing_callbacks_threading.md

---

## Phase 7: Missing Modules (10-15 days)
**Impact: Feature completeness**

### 7.1 Matrix Scaling (3-4 days)
- Current: 13-line no-op stub
- Spec: 251-line spec with 4 strategies (Ruiz, Curtis-Reid, etc.)
- **Reports:** 10_basis_matrix.md

### 7.2 Bound Propagation (3-4 days)
- cxf_simplex_step2, step3 completely missing
- Constraint-based bound tightening
- ~1,500 LOC per HANDOFF.md
- **Reports:** 09_simplex_modules.md

### 7.3 Solution Processing (2-3 days)
- 0/5 functions implemented
- Uncrushing, attribute wiring, gap computation, objective evaluation
- **Reports:** 09_simplex_modules.md, 18_function_signatures_MZ.md

### 7.4 Remaining Module Functions
- 44 of 75 M-Z functions missing (59%)
- 21 of 25 simplex module functions missing (84%)
- Query utilities at 0% implementation
- Logging infrastructure missing
- **Reports:** 18_function_signatures_MZ.md, 09_simplex_modules.md, 13_remaining_modules.md

### 7.5 Basis Operations (PFI module functions)
- 0/5 spec PFI functions implemented
- Eta vector creation, variable fixing, cycling detection
- (Note: underlying LU math is ~70% correct - best in codebase)
- **Reports:** 10_basis_matrix.md, 06_basis_lu.md

---

## Phase 8: Function Signature Normalization (2-3 days)
**Impact: API correctness**

### 8.1 Fix 53+ Signature Mismatches
- Add missing Environment *env parameters
- Fix return types (int vs CxfStatus)
- Fix parameter types, counts, ordering
- **Reports:** 17_function_signatures_AL.md, 18_function_signatures_MZ.md

### 8.2 API Function Renames
- cxf_loadenv → cxf_env_create_internal
- cxf_startenv → cxf_env_finalize
- cxf_freeenv → cxf_env_free_internal
- cxf_newmodel → cxf_model_create_internal
- cxf_updatemodel → cxf_model_apply_modifications
- **Reports:** 08_api_env.md

---

## Dependency Graph

```
Phase 0 (numerics)     ─┐
Phase 1 (names)        ─┼→ Phase 2 (structs) → Phase 3 (decompose solve_lp)
                        │                              │
                        │               ┌──────────────┼──────────────┐
                        │               ▼              ▼              ▼
                        │         Phase 4          Phase 5        Phase 6
                        │       (algorithms)      (pricing)    (infrastructure)
                        │               │              │              │
                        │               └──────────────┼──────────────┘
                        │                              ▼
                        │                        Phase 7 (modules)
                        │                              │
                        └──────────────────────────────▼
                                                 Phase 8 (signatures)
```

## Estimated Total Effort

| Phase | Days | LOC (est.) |
|-------|------|-----------|
| 0: Critical numerics | 1-2 | 100 |
| 1: Name alignment | 2-3 | 0 (renames) |
| 2: Data structures | 5-7 | 800 |
| 3: solve_lp decompose | 3-5 | 500 |
| 4: Core algorithms | 7-10 | 2,000 |
| 5: Pricing rewrite | 7-10 | 2,000 |
| 6: Infrastructure | 8-12 | 1,500 |
| 7: Missing modules | 10-15 | 3,000 |
| 8: Signatures | 2-3 | 200 |
| **TOTAL** | **45-67 days** | **~10,100 LOC** |

## What to Keep (Do Not Delete)

- CSC/CSR sparse matrix format and SpMV operations
- Basis/LU factorization core math (~70% compliant)
- Basic eta vector mechanics (algebraically equivalent)
- MPS parser infrastructure
- Test framework and existing passing tests
- Build system (CMake)
