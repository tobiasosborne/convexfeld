# Audit Report: solve_lp.c Deep Audit
**Auditor:** Agent F2
**Date:** 2026-02-16
**Scope:** src/simplex/solve_lp.c (1262 lines), src/simplex/solve_lp_stub.c (48 lines)
**Specs:** modules/solve_lp_core.md, algorithms/revised_simplex.md, integration/optimization_pipeline.md, modules/simplex_lifecycle.md

## Summary
- **Total violations:** 16 major violations
- **Critical:** 4 / **Major:** 8 / **Minor:** 4
- **Functions in file:** 13
- **Functions matching spec:** 1 (cxf_solve_lp exists but implementation diverges significantly)
- **Hallucinated functions:** 10
- **File size violation:** YES (1262 LOC, limit is 200 LOC)
- **Architecture violation:** SEVERE - implements two-phase simplex inline instead of delegating to specified modules

## Executive Summary

solve_lp.c is the most critically non-compliant file in the codebase. While the high-level function `cxf_solve_lp` is specified in the module specs, **its implementation bears little resemblance to the specification**. The spec describes a sophisticated orchestration function that delegates to well-defined subsystems (P3.20-P3.23) through a two-level iteration loop with multiple phases. Instead, the implementation is a monolithic 1262-line file containing inline Phase I/Phase II simplex logic with manual auxiliary variable setup and extensive hardcoded logic.

**Most Critical Issues:**
1. **Missing orchestration architecture** - No two-level iteration loop, no delegation to P3.20 step functions
2. **Hallucinated Phase I setup** - `setup_phase_one()` implements inline artificial variable logic not specified anywhere
3. **Missing algorithm components** - No crash basis (P3.21), no crossover (P3.23), no perturbation (P2.6), no refactorization triggers
4. **Missing module integration** - Does not use P3.17-P3.19 pricing/pivot subsystems as specified
5. **Incomplete iteration** - Calls `cxf_simplex_iterate()` but doesn't implement the 10-step inner loop pattern

## Function Inventory

| Function | Lines | In Spec? | Matches? | Issues |
|----------|-------|----------|----------|--------|
| cxf_solve_lp | 671-1262 | Yes (P3.25) | **NO** | Missing 2-level loop, crossover, crash, bound-flip ratio test, PWL handling, presolve cycle |
| setup_phase_one | 54-206 | **NO** | N/A | **HALLUCINATED** - Inline Phase I logic not in spec |
| transition_to_phase_two | 220-264 | **NO** | N/A | **HALLUCINATED** - Should be part of cxf_simplex_phase_end (P3.21) |
| get_auxiliary_coeff | 290-307 | **NO** | N/A | **HALLUCINATED** - Auxiliary variable logic should be in P3.22 init |
| compute_reduced_costs | 320-398 | **NO** | N/A | **HALLUCINATED** - Pricing should delegate to P3.17-P3.18 |
| solve_unconstrained | 402-432 | **NO** | Partial | Acceptable special case but missing status mapping |
| get_row_coeffs | 440-464 | **NO** | N/A | **HALLUCINATED** - Matrix access should use P3.16 basis operations |
| rows_parallel | 469-481 | **NO** | N/A | **HALLUCINATED** - Not specified anywhere |
| check_obvious_infeasibility | 497-588 | **NO** | N/A | **HALLUCINATED** - Should be part of P3.21 preprocessing |
| check_obvious_unboundedness | 596-666 | **NO** | N/A | **HALLUCINATED** - Should be part of P3.21 preprocessing |

## Violations

### V-01: Missing Two-Level Iteration Loop Architecture
- **Severity:** CRITICAL
- **File:** solve_lp.c:726-1204
- **Spec reference:** P3.25 cxf_solve_lp Phase 6, optimization_pipeline.md lines 283-340
- **Description:** The spec specifies a sophisticated two-level iteration loop with outer loop (round control) and inner loop (basis stabilization) comprising 10 distinct steps per iteration. The implementation has a simple while loop calling cxf_simplex_iterate.
- **Expected:**
  ```
  OUTER LOOP (max ~5/10/100 rounds)
    Take outer basis snapshot
    INNER LOOP (basis stabilization)
      [1] cxf_progress_snapshot
      [2] cxf_log_iteration_progress
      [3] cxf_simplex_phase_end (pre-step)
      [4] cxf_simplex_perturbation
      [5] cxf_simplex_step (pricing + ratio test + pivot)
      [6] cxf_simplex_step2 (variable bound propagation)
      [7] cxf_simplex_step3 (constraint bound propagation)
      [8] cxf_simplex_phase_end (post-step)
      [9] cxf_basis_diff
      [10] cxf_simplex_post_iterate
  ```
- **Actual:** Single while loop with iteration limit check, calls cxf_simplex_iterate() once per iteration (lines 731-1204)

### V-02: Hallucinated Phase I Setup Logic
- **Severity:** CRITICAL
- **File:** solve_lp.c:54-206 (setup_phase_one)
- **Spec reference:** P3.25 Phase 4 (crash basis), P3.22 cxf_simplex_init Phase 7
- **Description:** The implementation contains a 152-line function that manually sets up artificial variables for Phase I. This logic is not specified in any module. The spec indicates artificial variable setup should occur during cxf_simplex_init (P3.22) as part of "special variable processing."
- **Expected:** cxf_simplex_init handles artificial variable allocation and initial basis, then cxf_simplex_crash (P3.21) constructs a smart initial basis
- **Actual:** Manual loop over constraints setting slack/artificial variables with coefficient determination logic

### V-03: Missing Crash Basis Construction
- **Severity:** MAJOR
- **File:** solve_lp.c (entire file)
- **Spec reference:** P3.25 Phase 4, P3.21 cxf_simplex_crash
- **Description:** The spec requires calling cxf_simplex_crash (P3.21) to construct a crash basis, citing Gould and Reid (1989). This reduces Phase I iteration count by providing a better starting point than the all-slack basis.
- **Expected:** After cxf_simplex_init, call `cxf_simplex_crash(state, env)` to build initial basis
- **Actual:** No crash basis construction; uses manual artificial variable setup instead

### V-04: Missing Crossover Support
- **Severity:** MAJOR
- **File:** solve_lp.c (entire file)
- **Spec reference:** P3.25 Phase 5, P3.23 cxf_crossover
- **Description:** The spec specifies comprehensive barrier-to-simplex crossover support (Phase 5 of cxf_solve_lp). The implementation has no crossover logic.
- **Expected:**
  ```c
  if (barrier_solution_exists) {
      copy_barrier_bounds_to_working();
      cxf_crossover(state, env);
      cxf_crossover_bounds(state, env);
  }
  ```
- **Actual:** No crossover support at all

### V-05: Missing Anti-Cycling Perturbation
- **Severity:** MAJOR
- **File:** solve_lp.c:719-720
- **Spec reference:** P3.25 Phase 6 step 4, P2.6 Perturbation, P3.21 cxf_simplex_perturbation
- **Description:** The spec requires calling cxf_simplex_perturbation within the iteration loop based on EXPAND stall detection (Gill et al., 1989). The implementation calls it once before the loop starts.
- **Expected:** `cxf_simplex_perturbation(state, env)` called within inner loop when stalling detected
- **Actual:** Called once at line 719, before any iterations

### V-06: Missing Pricing Subsystem Integration
- **Severity:** MAJOR
- **File:** solve_lp.c:320-398 (compute_reduced_costs)
- **Spec reference:** P3.17-P3.18 Pricing modules, P3.25 Phase 6 step 5
- **Description:** The spec indicates pricing should delegate to P3.17 (cxf_pricing_candidates) and P3.18 (cxf_pricing_cascade_update). The implementation has inline reduced cost computation.
- **Expected:** Pricing delegated to P3.17-P3.18 subsystem with steepest-edge or Devex weights
- **Actual:** Manual loop computing `dj = cj - pi^T * Aj` (lines 367-397)

### V-07: Missing Ratio Test and Pivot Delegation
- **Severity:** MAJOR
- **File:** solve_lp.c (no ratio test code)
- **Spec reference:** P3.19 Pivot Operations, P2.4 Ratio Test, P3.25 Phase 6 step 5
- **Description:** The spec requires Harris two-pass ratio test with bound flipping (Forrest and Goldfarb, 1992). The implementation delegates to cxf_simplex_iterate but doesn't show the ratio test integration.
- **Expected:** `cxf_simplex_step()` calls P3.19 ratio test functions and pivot operations
- **Actual:** Hidden inside cxf_simplex_iterate (external function)

### V-08: Missing Bound Propagation Steps
- **Severity:** MAJOR
- **File:** solve_lp.c (entire file)
- **Spec reference:** P3.25 Phase 6 steps 6-7, P3.20 cxf_simplex_step2/step3
- **Description:** The spec specifies two bound propagation passes per iteration: step2 (variable-side) and step3 (constraint-side). The implementation has none.
- **Expected:** After each pivot, call cxf_simplex_step2 and cxf_simplex_step3 for bound tightening
- **Actual:** No bound propagation in iteration loop

### V-09: Missing Piecewise-Linear Constraint Handling
- **Severity:** MAJOR
- **File:** solve_lp.c (entire file)
- **Spec reference:** P3.25 Phase 7, optimization_pipeline.md lines 359-371
- **Description:** The spec dedicates an entire phase (Phase 7) to PWL constraint processing with breakpoint transitions, coefficient updates, and segment shifting (Fourer, 1992). The implementation has no PWL handling.
- **Expected:** Breakpoint ratio test, segment consumption, coefficient batch updates within iteration loop
- **Actual:** No PWL support

### V-10: Missing Basis Refactorization Logic
- **Severity:** MAJOR
- **File:** solve_lp.c (entire file)
- **Spec reference:** P2.1 Revised Simplex Method Step 9, P3.16 Basis Operations
- **Description:** The spec requires periodic basis refactorization triggered by eta vector count or numerical accuracy degradation (Forrest and Tomlin, 1972). The implementation shows no refactorization triggers.
- **Expected:** Check eta_count threshold, call cxf_fix_variables_at_bounds to refactorize
- **Actual:** No refactorization logic visible

### V-11: Missing Post-Solve Pipeline
- **Severity:** MAJOR
- **File:** solve_lp.c:1252-1256
- **Spec reference:** P3.25 Phase 9, P3.21 cxf_simplex_refine, P3.22 cxf_simplex_final/postsolve
- **Description:** The spec requires a three-step post-solve pipeline: refine (P3.21), final (P3.22), postsolve (P3.22). The implementation only calls unperturb and refine.
- **Expected:**
  ```c
  cxf_simplex_unperturb(state, env);
  cxf_simplex_refine(state, env);
  cxf_simplex_final(state, env);  // MISSING
  cxf_simplex_postsolve(state, env);  // MISSING
  ```
- **Actual:** Lines 1253-1256 only unperturb and refine

### V-12: Incorrect Phase Transition Logic
- **Severity:** MAJOR
- **File:** solve_lp.c:220-264 (transition_to_phase_two)
- **Spec reference:** P3.21 cxf_simplex_phase_end, P2.1 Step 7 (Check Termination)
- **Description:** Phase transition should be handled by cxf_simplex_phase_end (P3.21), not a standalone function. The spec indicates phase transitions occur within the iteration loop step 3 and step 8.
- **Expected:** cxf_simplex_phase_end detects Phase I completion and transitions to Phase II
- **Actual:** Manual transition function called at line 1215

### V-13: Missing Parameter Backup/Restore
- **Severity:** MINOR
- **File:** solve_lp.c (entire function)
- **Spec reference:** P3.25 Phase 1, solve_lp_core.md lines 281-285
- **Description:** The spec requires backing up and restoring LP-specific parameters (iteration limits, tolerances, mode flags). The implementation has no parameter backup.
- **Expected:** Save/restore iteration limits, tolerances at function entry/exit
- **Actual:** No parameter backup

### V-14: Incorrect Tolerance Values
- **Severity:** MINOR
- **File:** solve_lp.c:64, 943
- **Spec reference:** P2.1 Numerical Considerations, Table of Tolerances
- **Description:** The spec provides a table of standard tolerance values. The implementation uses hardcoded tolerance values that may not match spec.
- **Expected:** Tolerances from environment (epsilon_opt: 1e-6 to 1e-8, epsilon_feas: 1e-6 to 1e-8)
- **Actual:** Uses CXF_FEASIBILITY_TOL (unknown value), 0.01 phase1_tol at line 943

### V-15: Missing Basis Snapshot and Diff
- **Severity:** MINOR
- **File:** solve_lp.c (iteration loop)
- **Spec reference:** P3.25 Phase 6 steps 1 and 9, P3.16 cxf_progress_snapshot/cxf_basis_diff
- **Description:** The spec requires basis snapshots before the inner loop and basis diff checks after each iteration to detect convergence. The implementation has no snapshot/diff logic.
- **Expected:** cxf_progress_snapshot at loop start, cxf_basis_diff to detect basis stabilization
- **Actual:** No snapshot or diff functions called

### V-16: File Size Violation
- **Severity:** MINOR
- **File:** solve_lp.c
- **Spec reference:** CLAUDE.md Rule 1 (200 LOC limit)
- **Description:** File is 1262 lines, exceeding the 200 LOC limit by 6.3x
- **Expected:** File should be under 200 lines
- **Actual:** 1262 lines

## Hallucinated Code (not in any spec)

### 1. setup_phase_one (lines 54-206)
**Purpose:** Sets up Phase I artificial variables
**Why hallucinated:** The spec indicates artificial variable setup occurs in cxf_simplex_init (P3.22 Phase 7, "special variable processing"). This 152-line function manually iterates over constraints, determines auxiliary coefficients, sets slack values, and computes Phase I objective. Not specified in any module.

**Key hallucinated logic:**
- Lines 62-67: Manual initialization of original variables at lower bounds
- Lines 78-172: Per-constraint slack/artificial variable setup with coefficient determination
- Lines 180-186: Manual Phase I objective computation

### 2. transition_to_phase_two (lines 220-264)
**Purpose:** Transitions from Phase I to Phase II
**Why hallucinated:** Phase transitions are the responsibility of cxf_simplex_phase_end (P3.21), called within the iteration loop (P3.25 Phase 6 steps 3 and 8). This standalone function is not specified.

**Key hallucinated logic:**
- Lines 226-228: Restore original objective coefficients
- Lines 235-249: Fix artificial variables for equality constraints
- Lines 252-255: Recompute objective value
- Lines 260-261: Reset anti-cycling state

### 3. get_auxiliary_coeff (lines 290-307)
**Purpose:** Determines coefficient for slack/surplus/artificial variables
**Why hallucinated:** This logic should be part of cxf_simplex_init's artificial variable setup (P3.22 Phase 7), not a separate function. The spec doesn't describe coefficient determination as a standalone operation.

### 4. compute_reduced_costs (lines 320-398)
**Purpose:** Computes reduced costs for all variables
**Why hallucinated:** Pricing is specified in P3.17-P3.18. The spec indicates pricing should use cxf_pricing_candidates for candidate selection with steepest-edge or Devex weights (P2.3). This inline computation bypasses the pricing subsystem.

**Key hallucinated logic:**
- Lines 334-363: Dual prices computation via BTRAN
- Lines 367-397: Per-variable reduced cost computation

### 5. solve_unconstrained (lines 402-432)
**Purpose:** Solves unconstrained LP special case
**Why hallucinated:** While this is a reasonable optimization for zero-constraint problems, it's not specified in any module. The spec indicates all problems flow through cxf_solver_dispatch (P3.25).

### 6. get_row_coeffs (lines 440-464)
**Purpose:** Extracts row coefficients as dense array
**Why hallucinated:** Matrix access should use basis operations (P3.16). This function duplicates matrix access logic that should be encapsulated in the basis/matrix subsystem.

### 7. rows_parallel (lines 469-481)
**Purpose:** Checks if two rows are parallel
**Why hallucinated:** Not specified anywhere. Used by check_obvious_infeasibility.

### 8. check_obvious_infeasibility (lines 497-588)
**Purpose:** Detects infeasibility via bound propagation and parallel constraint analysis
**Why hallucinated:** Infeasibility detection should occur during preprocessing (P3.21 cxf_simplex_preprocess) or within the simplex iteration (returning INFEASIBLE status). This 91-line function implements O(m²) parallel constraint checking not specified anywhere.

**Key hallucinated logic:**
- Lines 516-544: Single constraint infeasibility via bound propagation
- Lines 547-584: Parallel constraint contradiction check (O(m²))

### 9. check_obvious_unboundedness (lines 596-666)
**Purpose:** Detects unboundedness via ray analysis
**Why hallucinated:** Unboundedness detection should occur during the ratio test (P2.4, P3.19), when no blocking variable is found. This 70-line function implements a separate ray analysis not specified anywhere.

### 10. Inline Phase I/Phase II Logic (lines 708-1210)
**Purpose:** Implements two-phase simplex with manual Phase I loop and Phase II loop
**Why hallucinated:** The spec describes delegating to a modular architecture with well-defined iteration steps. The implementation has inline logic with hardcoded Phase I objective checks, artificial variable counting, and manual Phase II transition.

**Key hallucinated patterns:**
- Lines 731-1204: Phase I iteration loop with inline optimality checks
- Lines 766-1191: Manual Phase I feasibility verification with correction iterations
- Lines 1215-1220: Manual Phase II setup
- Lines 1227-1248: Phase II iteration loop

## Missing Code (in spec but not implemented)

### 1. Two-Level Iteration Loop (P3.25 Phase 6)
**Spec reference:** optimization_pipeline.md lines 283-340
**Description:** The spec describes a sophisticated two-level loop with outer loop (round control) and inner loop (basis stabilization). The inner loop comprises 10 steps including snapshot, logging, phase transitions, perturbation, pivot, bound propagation, and convergence detection.
**Status:** NOT IMPLEMENTED - replaced with simple while loop calling cxf_simplex_iterate

### 2. Crash Basis Construction (P3.25 Phase 4, P3.21)
**Spec reference:** solve_lp_core.md lines 255-256, P3.21 cxf_simplex_crash
**Description:** "Delegate to cxf_simplex_crash (P3.21) to construct a crash basis" citing Gould and Reid (1989)
**Status:** NOT IMPLEMENTED

### 3. Crossover Setup (P3.25 Phase 5, P3.23)
**Spec reference:** solve_lp_core.md lines 258-269
**Description:** Complete barrier-to-simplex crossover with quadratic variable processing, binary variable linearization, and variable classification
**Status:** NOT IMPLEMENTED

### 4. Pricing Subsystem Delegation (P3.17-P3.18)
**Spec reference:** optimization_pipeline.md line 310
**Description:** Delegate to cxf_pricing_candidates (P3.17) for entering variable selection
**Status:** NOT IMPLEMENTED - uses inline reduced cost computation

### 5. Harris Two-Pass Ratio Test with Bound Flipping (P2.4, P3.19)
**Spec reference:** revised_simplex.md lines 142-174
**Description:** Harris two-pass ratio test with relaxed tolerance and largest pivot selection, plus bound flipping for long steps
**Status:** UNKNOWN - hidden inside cxf_simplex_iterate

### 6. Bound Propagation Steps (P3.20 step2/step3)
**Spec reference:** optimization_pipeline.md lines 315-323
**Description:** Variable-side bound propagation (step2) and constraint-side bound propagation (step3) after each pivot
**Status:** NOT IMPLEMENTED

### 7. Piecewise-Linear Constraint Processing (P3.25 Phase 7)
**Spec reference:** solve_lp_core.md lines 90-110, optimization_pipeline.md lines 359-371
**Description:** Breakpoint transitions, coefficient updates, segment consumption, finalization
**Status:** NOT IMPLEMENTED

### 8. Basis Refactorization Triggers (P2.1 Step 9, P3.16)
**Spec reference:** revised_simplex.md lines 306-327
**Description:** Periodic refactorization based on eta vector count, numerical accuracy, or phase transition
**Status:** NOT IMPLEMENTED - no refactorization logic visible

### 9. Post-Solve Pipeline (P3.21 refine, P3.22 final/postsolve)
**Spec reference:** solve_lp_core.md lines 344-365, simplex_lifecycle.md lines 123-273
**Description:** Three-step post-solve: refine (snap near-bound values), final (dual-feasibility fixing), postsolve (implied-bound propagation + cleanup)
**Status:** PARTIALLY IMPLEMENTED - only refine called, missing final and postsolve

### 10. Parameter Backup/Restore (P3.25 Phase 1/9)
**Spec reference:** solve_lp_core.md lines 281-285
**Description:** Save and restore LP-specific parameters (iteration limits, tolerances, mode flags) on all exit paths
**Status:** NOT IMPLEMENTED

### 11. Basis Snapshot and Diff (P3.16)
**Spec reference:** optimization_pipeline.md lines 294, 329
**Description:** cxf_progress_snapshot captures basis state, cxf_basis_diff detects convergence
**Status:** NOT IMPLEMENTED

### 12. Steepest-Edge or Devex Pricing Weights (P2.3, P3.17-P3.18)
**Spec reference:** revised_simplex.md lines 199-272
**Description:** Maintain steepest-edge weights via exact DSE update or Devex approximation (Goldfarb and Reid, 1977; Harris, 1973)
**Status:** NOT IMPLEMENTED - uses simple reduced cost pricing

## Structural Issues (file organization)

### 1. Monolithic Implementation
**Issue:** The entire LP solve pipeline is implemented in a single 1262-line file instead of delegating to the specified modular architecture.
**Impact:** Violates the layered architecture principle, makes testing and maintenance difficult, prevents code reuse.
**Recommendation:** Refactor to delegate to P3.20-P3.23 subsystems as specified.

### 2. Inline Algorithm Logic
**Issue:** Phase I setup, Phase II transition, reduced cost computation, and infeasibility/unboundedness detection are implemented inline instead of using dedicated modules.
**Impact:** Duplicates logic that should be shared across the solver, prevents algorithmic improvements from propagating, makes the code harder to understand.
**Recommendation:** Move Phase I setup to P3.22 (simplex_init), phase transitions to P3.21 (simplex_phase_end), pricing to P3.17-P3.18.

### 3. Missing Error Handling
**Issue:** The function has basic error propagation but doesn't implement the comprehensive cleanup pattern described in the spec (parameter restore on all paths, resource deallocation).
**Impact:** Potential resource leaks, environment parameter corruption on error paths.
**Recommendation:** Add parameter backup/restore, ensure all exit paths clean up properly.

### 4. Debugging Code Remains
**Issue:** Lines 190-203, 728-1186 contain extensive #ifdef DEBUG_PHASE1 blocks totaling ~450 lines.
**Impact:** Inflates file size, makes the logic harder to follow, suggests the code is still in development.
**Recommendation:** Remove debug code or move to separate debug-only functions.

### 5. Magic Numbers
**Issue:** Hardcoded constants throughout (line 943: 0.01 phase1_tol, line 64: CXF_INFINITY comparison)
**Impact:** Makes tolerance tuning difficult, violates the spec's requirement to use environment-provided tolerances.
**Recommendation:** Use environment parameters for all tolerances.

### 6. No Unit Tests
**Issue:** A 1262-line monolithic function cannot be effectively unit tested.
**Impact:** Regression risk, difficulty verifying correctness of individual components.
**Recommendation:** Refactor into smaller, testable functions per spec.

## Architecture Compliance Summary

| Spec Component | Status | Notes |
|---------------|--------|-------|
| Two-level iteration loop | ❌ NOT IMPLEMENTED | Replaced with simple while loop |
| Crash basis construction | ❌ NOT IMPLEMENTED | Uses manual artificial variable setup |
| Crossover setup | ❌ NOT IMPLEMENTED | No barrier-to-simplex support |
| Pricing delegation | ❌ NOT IMPLEMENTED | Inline reduced cost computation |
| Ratio test delegation | ⚠️ UNKNOWN | Hidden in cxf_simplex_iterate |
| Bound propagation | ❌ NOT IMPLEMENTED | No step2/step3 calls |
| PWL handling | ❌ NOT IMPLEMENTED | No Phase 7 logic |
| Refactorization triggers | ❌ NOT IMPLEMENTED | No eta count checks |
| Post-solve pipeline | ⚠️ PARTIAL | Only refine, missing final/postsolve |
| Parameter backup | ❌ NOT IMPLEMENTED | No save/restore |
| Basis snapshot/diff | ❌ NOT IMPLEMENTED | No convergence detection |
| Phase transitions | ⚠️ PARTIAL | Manual transition instead of phase_end |

**Overall Compliance:** ~10% (1 of 12 major components fully implemented)

## Recommendations

### Priority 1 (Critical - Required for Spec Compliance)
1. **Implement two-level iteration loop** (P3.25 Phase 6) - This is the core algorithmic structure
2. **Delegate to P3.20 step functions** - Replace inline iteration with step/step2/step3 calls
3. **Add crash basis construction** (P3.21) - Replace manual Phase I setup
4. **Integrate pricing subsystem** (P3.17-P3.18) - Remove inline reduced cost computation

### Priority 2 (Major - Needed for Full Functionality)
5. **Add crossover support** (P3.23) - Barrier-to-simplex conversion
6. **Implement bound propagation** (P3.20 step2/step3) - Tighten bounds during iteration
7. **Add refactorization triggers** (P3.16) - Maintain numerical stability
8. **Complete post-solve pipeline** (P3.22 final/postsolve) - Add dual fixing and bound propagation

### Priority 3 (Important - Spec Compliance)
9. **Add parameter backup/restore** - Protect environment from internal modifications
10. **Implement basis snapshot/diff** (P3.16) - Detect convergence
11. **Add PWL handling** (P3.25 Phase 7) - Support piecewise-linear constraints
12. **Move phase transitions to phase_end** (P3.21) - Use specified module

### Priority 4 (Cleanup)
13. **Refactor to < 200 LOC** - Break into multiple files per module spec
14. **Remove hallucinated functions** - Move logic to appropriate modules
15. **Remove debug code** - Clean up #ifdef blocks
16. **Use environment tolerances** - Remove magic numbers

## Verification Against Clean Room Test

**Question:** Could this file be implemented from the specification alone, without seeing the binary?

**Answer:** **NO.** The specification describes a modular architecture with delegation to well-defined subsystems (P3.20-P3.23, P3.16-P3.19). The implementation is a monolithic file with inline Phase I/Phase II logic that bears little resemblance to the specified architecture. A clean-room implementation from the spec would produce a completely different file structure with ~100-150 lines of orchestration code delegating to the specified modules.

The only way to produce this implementation would be to:
1. Ignore the two-level iteration loop specification
2. Ignore the module delegation specifications (P3.20-P3.23)
3. Ignore the pricing/pivot/basis subsystem specifications (P3.16-P3.19)
4. Implement a traditional textbook two-phase simplex from first principles
5. Add extensive debug logging

This suggests the file was written before the modular architecture was designed, or the architecture specs were written after the implementation and don't match reality.

## Conclusion

solve_lp.c is **fundamentally non-compliant** with the specifications. While it implements a working two-phase simplex method, the architecture diverges completely from the specified modular design. The file should be considered a **prototype or proof-of-concept** rather than a conforming implementation.

To achieve spec compliance, the file requires a **complete rewrite** following the specified architecture:
- Replace the 1262-line monolith with ~100-150 lines of orchestration
- Delegate Phase I setup to P3.22 (simplex_init)
- Delegate crash basis to P3.21 (simplex_crash)
- Delegate crossover to P3.23
- Delegate iteration to P3.20 (step/step2/step3) within a two-level loop
- Delegate pricing to P3.17-P3.18
- Delegate ratio test and pivot to P3.19
- Delegate phase transitions to P3.21 (phase_end)
- Delegate post-solve to P3.21 (refine) and P3.22 (final/postsolve)

**Estimated Refactoring Effort:** 2-3 weeks for a senior developer familiar with the codebase and specifications.
