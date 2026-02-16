# Audit Report: Optimization Pipeline (End-to-End Flow)
**Auditor:** Agent D1
**Date:** 2026-02-16
**Scope:** src/api/optimize_api.c, src/simplex/solve_lp.c, setup.c, iterate.c, cleanup.c, post.c
**Specs:** integration/optimization_pipeline.md, modules/solve_lp_core.md

## Summary

This audit examines the end-to-end optimization pipeline implementation against the v2 integration specification. The implementation shows a **severely simplified pipeline** that skips most of the stages defined in the specification. While the core simplex iteration works, the surrounding orchestration infrastructure is largely absent.

**Critical Findings:**
- 10 major pipeline stages completely missing
- 6 pipeline stages incorrectly implemented or out of order
- No two-level iteration loop (spec's defining architectural feature)
- No PWL constraint processing
- No presolve-solve-uncrush cycle in solve_lp
- No parameter backup/restore in solve_lp
- Pipeline stages executed in wrong order vs spec
- Missing crossover integration
- No basis snapshot/diff convergence detection
- Minimal cleanup and postsolve processing

**Status:** The implementation appears to be an early prototype that implements basic Phase I/Phase II simplex but does not follow the detailed pipeline architecture specified in the v2 specs.

---

## Violations

### CRITICAL VIOLATION 1: Missing Two-Level Iteration Loop
**Spec Location:** integration/optimization_pipeline.md lines 266-340, solve_lp_core.md lines 72-106
**Implementation:** solve_lp.c lines 726-1248
**Severity:** CRITICAL - Core architectural feature missing

**Spec Requires:**
```
OUTER LOOP (round control, max ~5/10/100 rounds depending on mode)
  |
  +-- Take outer basis snapshot (cxf_progress_snapshot, P3.16)
  |
  +-- INNER LOOP (basis stabilization)
  |     |
  |     +--[1] cxf_progress_snapshot (P3.16) - Capture current basis state
  |     +--[2] cxf_log_iteration_progress (P3.20) - Progress logging
  |     +--[3] cxf_simplex_phase_end (P3.21) - Phase transition checks
  |     +--[4] cxf_simplex_perturbation (P3.21) - Anti-cycling
  |     +--[5] cxf_simplex_step (P3.20) - PRIMARY SIMPLEX PIVOT
  |     +--[6] cxf_simplex_step2 (P3.20) - Variable-side bound propagation
  |     +--[7] cxf_simplex_step3 (P3.20) - Constraint-side bound propagation
  |     +--[8] cxf_simplex_phase_end (P3.21) - Post-pivot cleanup
  |     +--[9] cxf_basis_diff (P3.16) - Compare basis to snapshot
  |     +--[10] cxf_simplex_post_iterate (P3.20) - Stall detection
  |
  +-- Check outer basis diff
```

**Implementation Has:**
```c
// Phase I loop - lines 731-1204
while (state->iteration < max_iter) {
    status = cxf_simplex_iterate(state, env);  // Single call per iteration
    if (status == ITERATE_OPTIMAL) { /* feasibility check */ break; }
    // ... error handling ...
}

// Phase II loop - lines 1227-1248
while (state->iteration < max_iter) {
    status = cxf_simplex_iterate(state, env);  // Single call per iteration
    if (status == ITERATE_OPTIMAL) { /* ... */ break; }
    // ... status handling ...
}
```

**What's Wrong:**
1. No outer loop with round control
2. No inner loop with basis stabilization
3. No basis snapshots (cxf_progress_snapshot)
4. No basis diff comparison (cxf_basis_diff)
5. No round limits (5/10/100 depending on mode)
6. Iteration loop is a simple while loop, not the nested structure
7. All the per-iteration functions (step2, step3, phase_end, post_iterate) are NOT called
8. Only cxf_simplex_iterate is called, then control returns to the outer while
9. Perturbation called ONCE before Phase I loop (line 719), not per-iteration
10. No convergence detection via basis diff threshold

**Impact:** The adaptive convergence mechanism that prevents cycling and ensures progress is completely absent. The spec describes this as essential for handling degenerate problems.

---

### CRITICAL VIOLATION 2: Missing Pipeline Phases (10 Stages)
**Spec Location:** integration/optimization_pipeline.md lines 103-509, solve_lp_core.md lines 48-125
**Implementation:** solve_lp.c entire file
**Severity:** CRITICAL - Major orchestration missing

**Spec Defines 9 Phases for cxf_solve_lp:**

| Phase | Spec Requirement | Implementation Status |
|-------|------------------|----------------------|
| Phase 1: Parameter extraction | Read solver params, save for restore | ❌ MISSING - no parameter save/restore |
| Phase 2: Solver state init | cxf_simplex_init | ✅ Present (line 703) |
| Phase 3: Method selection | Priority chain: user > mode > concurrent | ❌ MISSING - no method selection |
| Phase 4: Crash basis | cxf_simplex_crash | ❌ MISSING - setup_phase_one builds artificial basis only |
| Phase 5: Crossover setup | cxf_crossover + cxf_crossover_bounds | ❌ MISSING - no barrier crossover |
| Phase 6: Two-level iteration loop | Outer/inner loops with snapshots | ❌ MISSING (see Violation 1) |
| Phase 7: PWL constraint processing | Breakpoint transitions, coeff updates | ❌ MISSING - no PWL handling |
| Phase 8: Solution extraction | cxf_solution_extract | ✅ Present (line 1258) |
| Phase 9: Cleanup + status mapping | cxf_solver_state_cleanup, param restore | ⚠️ PARTIAL - cleanup yes, no param restore |

**Missing Functions:**
1. `cxf_simplex_crash` - Never called, Phase I uses artificial basis directly
2. `cxf_crossover` - Not called
3. `cxf_crossover_bounds` - Not called
4. `cxf_progress_snapshot` - Not called
5. `cxf_basis_diff` - Not called
6. `cxf_log_iteration_progress` - Not called in loop
7. `cxf_simplex_step2` - Not called
8. `cxf_simplex_step3` - Not called
9. `cxf_simplex_post_iterate` - Not called
10. PWL processing functions - None present

**Parameter Backup/Restore Missing:**
- Spec requires saving ~30 parameters in cxf_solver_dispatch (not in scope here, but mentioned)
- Spec requires saving LP-specific parameters in cxf_solve_lp
- Implementation has ZERO parameter backup/restore code

---

### CRITICAL VIOLATION 3: Missing Presolve-Solve-Uncrush Cycle
**Spec Location:** solve_lp_core.md lines 375-384
**Implementation:** solve_lp.c (entire file)
**Severity:** CRITICAL

**Spec Requires:**
```
1. Presolve: Create reduced model
2. Solve: Solve reduced model
3. Uncrush: Map solution back to original space
4. Verification: Recompute objective
```

**Implementation:**
- No presolve handling in cxf_solve_lp at all
- No presolved model creation
- No uncrushing
- No verification objective recomputation

**Note:** The spec indicates this is managed by cxf_solver_dispatch, but mentions it in solve_lp context for solution handling.

---

### CRITICAL VIOLATION 4: Wrong Pipeline Stage Order
**Spec Location:** integration/optimization_pipeline.md lines 266-367, solve_lp_core.md lines 72-125
**Implementation:** solve_lp.c lines 711-1262
**Severity:** CRITICAL

**Spec Order (Phase I/II loops):**
```
1. Basis snapshot
2. Log iteration progress
3. Phase end checks
4. Perturbation
5. Simplex step (pivot)
6. Step2 (var-side bounds)
7. Step3 (constraint-side bounds)
8. Phase end (post-pivot)
9. Basis diff
10. Post-iterate (stall detect)
```

**Implementation Order (Phase I):**
```
1. setup_phase_one (line 711) - Creates artificial basis
2. cxf_simplex_perturbation (line 719) - ONCE before loop
3. compute_reduced_costs (line 722) - Initial RC
4. while loop:
   4a. cxf_simplex_iterate (line 737) - Single call
   4b. Feasibility check (lines 766-1193) - Massive block
   4c. Status checks
5. transition_to_phase_two (line 1215)
6. compute_reduced_costs (line 1223) - Recompute for Phase II
7. Phase II while loop (similar structure)
```

**Problems:**
1. Perturbation called ONCE before loop, not per-iteration as spec requires
2. No basis snapshots
3. No phase_end calls
4. No step2/step3 calls
5. No post_iterate calls
6. No basis diff
7. Feasibility check is a huge inline block (766-1193) not modularized
8. Phase transition is manual code, not cxf_simplex_phase_end

---

### CRITICAL VIOLATION 5: Missing PWL Constraint Processing
**Spec Location:** solve_lp_core.md lines 90-110
**Implementation:** solve_lp.c (nowhere)
**Severity:** MAJOR

**Spec Requires (Phase 7 of solve_lp):**
```
PWL handling involves:
1. Iterate over active PWL variables
2. Compute breakpoint transitions via ratio test
3. Update objective coefficients when crossing breakpoint
4. Handle segment shifting
5. Range validation, pending deltas, direction determination
6. Column selection, objective update, segment finalization
```

**Implementation:**
- Zero PWL code in solve_lp.c
- No PWL data structures accessed
- No breakpoint processing
- No coefficient updates for PWL

---

### MAJOR VIOLATION 6: Missing Crossover Integration
**Spec Location:** solve_lp_core.md lines 68-69, integration/optimization_pipeline.md lines 256-265
**Implementation:** solve_lp.c (nowhere)
**Severity:** MAJOR

**Spec Requires (Phase 5):**
```
+--[Phase 5: Crossover setup (if barrier solution present)]
|   Delegate to cxf_crossover (P3.23)
|   -> Processes quadratic variables analytically
|   -> Linearizes binary variable quadratic terms
|   Delegate to cxf_crossover_bounds (P3.23)
|   -> Classifies variables by proximity to bounds
|   -> Snaps near-bound variables to exact bounds
|   -> Activates unrepresented constraints into basis
```

**Implementation:**
- No check for barrier solution presence
- No calls to cxf_crossover or cxf_crossover_bounds
- No quadratic variable processing
- No binary variable linearization

**Impact:** Cannot perform barrier-to-simplex crossover, which is a core feature for QP solving.

---

### MAJOR VIOLATION 7: Missing Basis Snapshot/Diff Mechanism
**Spec Location:** integration/optimization_pipeline.md lines 290-331
**Implementation:** solve_lp.c (nowhere)
**Severity:** MAJOR

**Spec Requires:**
```
Inner loop:
  +--[1] cxf_progress_snapshot (P3.16) - Capture current basis state
  ...
  +--[9] cxf_basis_diff (P3.16) - Compare basis to snapshot
          If change below threshold -> exit inner loop

Outer loop:
  +-- Take outer basis snapshot
  +-- Check outer basis diff
      If insufficient progress -> exit outer loop
```

**Implementation:**
- No calls to cxf_progress_snapshot
- No calls to cxf_basis_diff
- No basis snapshots stored
- No convergence detection via basis comparison
- Loop only exits on ITERATE_OPTIMAL or iteration limit

---

### MAJOR VIOLATION 8: Missing Step2/Step3 Bound Propagation
**Spec Location:** integration/optimization_pipeline.md lines 309-321
**Implementation:** solve_lp.c (not called from loops)
**Severity:** MAJOR

**Spec Requires (Inner Loop Steps):**
```
+--[6] cxf_simplex_step2 (P3.20)
|     Variable-side bound propagation
|     -> cxf_pivot_update (P3.19) for activity bounds
|     -> cxf_pivot_bound (P3.19) for fixed variables
|
+--[7] cxf_simplex_step3 (P3.20) [LP only, skipped for QP]
|     Constraint-side bound propagation
|     -> Implied bound derivation (Savelsbergh, 1994)
|     -> cxf_pivot_update (P3.19) for activity bounds
```

**Implementation:**
- These functions exist elsewhere (presumably in simplex modules)
- They are NEVER called from solve_lp.c
- No bound propagation after pivots in the main loop
- Spec says PWL disables step3, but here NOTHING calls step2 or step3

---

### MAJOR VIOLATION 9: Missing Post-Iterate Stall Detection
**Spec Location:** integration/optimization_pipeline.md line 333
**Implementation:** solve_lp.c (not called)
**Severity:** MAJOR

**Spec Requires:**
```
+--[10] cxf_simplex_post_iterate (P3.20)
        Stall detection, termination checks, stagnation
        If termination condition -> exit both loops
```

**Implementation:**
- cxf_simplex_post_iterate exists in post.c (lines 27-48)
- It is NEVER called from solve_lp.c
- No stall detection in the main loops
- No termination checks beyond ITERATE_OPTIMAL

---

### MAJOR VIOLATION 10: Minimal Cleanup and Postsolve
**Spec Location:** integration/optimization_pipeline.md lines 343-367
**Implementation:** solve_lp.c lines 1252-1262
**Severity:** MAJOR

**Spec Requires (Post-Loop Pipeline):**
```
cxf_simplex_refine (P3.21)
    Fix non-basic variables at bounds based on reduced costs
    Recover basic variables near upper bounds
    |
    v
cxf_simplex_final (P3.22)
    Dual-feasibility-based variable fixing
    Complementary slackness analysis
    |
    v
cxf_simplex_postsolve (P3.22)
    Implied bound propagation (FBBT)
    -> Delegates to cxf_propagate_bounds (P3.34)
    Convert tight inequality constraints to equalities
    Free all temporary working arrays
    |
    v
cxf_solution_extract
    Copy solution from SolverState to model
```

**Implementation:**
```c
// Lines 1252-1262
cxf_simplex_unperturb(state, env);  // Remove perturbation
cxf_simplex_refine(state, env);     // Snap near-bound values
if (model->status == CXF_OPTIMAL) cxf_extract_solution(state, model);
cxf_simplex_final(state);           // Cleanup
return model->status;
```

**What's Missing:**
1. No cxf_simplex_postsolve call (implied bound propagation, FBBT)
2. No cxf_propagate_bounds delegation
3. No conversion of tight inequalities to equalities
4. cxf_simplex_final exists but unclear if it does all the spec requires
5. Solution extraction conditional on OPTIMAL only - should extract for SUBOPTIMAL too

---

### MAJOR VIOLATION 11: Missing Phase Transition Function Calls
**Spec Location:** integration/optimization_pipeline.md lines 302-303, 325-326
**Implementation:** solve_lp.c
**Severity:** MAJOR

**Spec Requires (TWO calls per inner loop iteration):**
```
+--[3] cxf_simplex_phase_end (P3.21)
|     Phase transition checks, constraint cleanup
...
+--[8] cxf_simplex_phase_end (P3.21) [second call]
      Post-pivot constraint cleanup
```

**Implementation:**
- cxf_simplex_phase_end exists in post.c (lines 61-99)
- Called ZERO times from solve_lp.c main loops
- Phase transition is manual code in transition_to_phase_two (lines 220-264)
- No pre-pivot constraint cleanup
- No post-pivot constraint cleanup

---

### MAJOR VIOLATION 12: Wrong Perturbation Timing
**Spec Location:** integration/optimization_pipeline.md line 304-306
**Implementation:** solve_lp.c lines 719, 1252-1253
**Severity:** MAJOR

**Spec Requires:**
```
+--[4] cxf_simplex_perturbation (P3.21)
|     Anti-cycling perturbation if stalling detected
|     (EXPAND procedure, Gill et al., 1989)
```
Applied WITHIN the inner loop, conditionally based on stalling.

**Implementation:**
```c
// Line 719 - Before Phase I loop
cxf_simplex_perturbation(state, env);

// Lines 1252-1253 - After Phase II loop
cxf_simplex_unperturb(state, env);
cxf_simplex_refine(state, env);
```

**What's Wrong:**
1. Perturbation applied ONCE before Phase I loop starts
2. NOT applied per-iteration in response to stalling
3. Spec says "if stalling detected" - implementation applies unconditionally
4. Spec says called within inner loop - implementation calls before outer loop
5. Unperturb called after loop, which is correct
6. No conditional logic for when to perturb

---

### MODERATE VIOLATION 13: Missing Algorithm Dispatch from solve_lp
**Spec Location:** solve_lp_core.md lines 188-218 (cxf_solver_dispatch)
**Implementation:** solve_lp.c
**Severity:** MODERATE

**Spec Says:**
cxf_solver_dispatch is the function that:
- Detects model type (LP/QP/SOCP)
- Selects algorithm (simplex/barrier/concurrent/PDHG)
- Dispatches to cxf_solve_lp for simplex path

**Reality:**
- cxf_solve_lp IS the entry point in this codebase
- There's no separate cxf_solver_dispatch visible
- Model type detection, method selection all absent from solve_lp
- solve_lp assumes it's solving LP via simplex

**Note:** The spec structure suggests solve_lp is called BY solver_dispatch, not that it replaces it. This may be a missing layer.

---

### MODERATE VIOLATION 14: Missing Logging and Callbacks
**Spec Location:** integration/optimization_pipeline.md line 299
**Implementation:** solve_lp.c
**Severity:** MODERATE

**Spec Requires:**
```
+--[2] cxf_log_iteration_progress (P3.20)
      Progress logging and callback notification
      (Naming misnomer: does NOT perform iterations)
```

**Implementation:**
- No calls to cxf_log_iteration_progress in the loops
- No iteration progress logging
- No callback notification during solving
- DEBUG_PHASE1 printf debugging exists but is conditional compilation

---

### MODERATE VIOLATION 15: Incomplete Solution Extraction
**Spec Location:** integration/optimization_pipeline.md lines 343-367
**Implementation:** solve_lp.c lines 1258
**Severity:** MODERATE

**Spec Requires:**
```
cxf_solution_extract
    Copy solution from SolverState to model
    Handle presolved model mapping
    Issue solution-found callbacks (P3.13)
```

**Implementation:**
```c
if (model->status == CXF_OPTIMAL) cxf_extract_solution(state, model);
```

**Problems:**
1. Only extracts if OPTIMAL - should also extract for SUBOPTIMAL, ITERATION_LIMIT, TIME_LIMIT
2. No presolved model mapping (but presolve not implemented anyway)
3. No solution-found callbacks
4. Function is called cxf_extract_solution, not cxf_solution_extract

---

### MODERATE VIOLATION 16: Missing Status Code Remapping
**Spec Location:** solve_lp_core.md lines 121-123
**Implementation:** solve_lp.c lines 1236-1250
**Severity:** MODERATE

**Spec Requires:**
```
Map internal status to public status codes. Key mappings include:
- When solving with a working (presolved) matrix, infeasible and
  unbounded statuses may be swapped
- For crossover-origin solves, ambiguous status mapped to
  "infeasible-or-unbounded"
```

**Implementation:**
```c
// Lines 1236-1248 - Direct mapping
if (status == ITERATE_OPTIMAL) {
    model->status = CXF_OPTIMAL;
} else if (status == ITERATE_UNBOUNDED) {
    model->status = CXF_UNBOUNDED;
} else if (status == ITERATE_INFEASIBLE) {
    model->status = CXF_INFEASIBLE;
} else if (status < 0) {
    model->status = status;
}
if (state->iteration >= max_iter) model->status = CXF_ITERATION_LIMIT;
```

**What's Missing:**
1. No presolved matrix detection and status swapping
2. No crossover-origin detection and INF_OR_UNBD mapping
3. No numeric status handling for ill-conditioned bases
4. Direct mapping without transformations

---

### MODERATE VIOLATION 17: Unconstrained LP Handling Not Integrated
**Spec Location:** solve_lp_core.md (general orchestration context)
**Implementation:** solve_lp.c lines 402-432
**Severity:** MODERATE

**Implementation Has:**
```c
static int solve_unconstrained(CxfModel *model) {
    // Lines 402-432: Standalone function that solves unconstrained LP
    // by setting vars at bounds based on objective direction
}
```

Called from cxf_solve_lp line 681:
```c
if (model->num_constrs == 0) return solve_unconstrained(model);
```

**Issues:**
1. Function works correctly for unconstrained case
2. BUT: it bypasses entire pipeline (no state init, no cleanup, etc.)
3. Spec doesn't explicitly address unconstrained LPs
4. Early return means unconstrained LPs don't go through normal pipeline stages
5. This is probably fine for unconstrained, but breaks the architectural uniformity

---

### MINOR VIOLATION 18: Obvious Infeasibility/Unboundedness Checks
**Spec Location:** Not in spec
**Implementation:** solve_lp.c lines 491-666
**Severity:** MINOR (not in spec, but worth noting)

**Implementation Has:**
```c
// Lines 491-588: check_obvious_infeasibility
// Lines 590-666: check_obvious_unboundedness
```

Called at lines 691-700:
```c
if (check_obvious_infeasibility(model)) {
    model->status = CXF_INFEASIBLE;
    return CXF_INFEASIBLE;
}
if (check_obvious_unboundedness(model)) {
    model->status = CXF_UNBOUNDED;
    return CXF_UNBOUNDED;
}
```

**Analysis:**
- These are preprocessing heuristics to detect trivial cases
- NOT mentioned in the spec
- Potentially useful optimizations
- But they bypass the pipeline (early return)
- Spec mentions "obvious" cases in optimization_pipeline.md context but not as explicit functions

---

## Pipeline Stages Missing

### From Spec Phase 6 (Two-Level Iteration Loop):

All 10 inner loop stages are missing:

1. ❌ **cxf_progress_snapshot** (basis snapshot before iteration batch)
2. ❌ **cxf_log_iteration_progress** (progress logging + callbacks)
3. ❌ **cxf_simplex_phase_end** (phase transition checks - FIRST call)
4. ❌ **cxf_simplex_perturbation** (anti-cycling, conditional on stalling)
5. ✅ **cxf_simplex_step** (pivot - called via cxf_simplex_iterate)
6. ❌ **cxf_simplex_step2** (variable-side bound propagation)
7. ❌ **cxf_simplex_step3** (constraint-side bound propagation)
8. ❌ **cxf_simplex_phase_end** (post-pivot constraint cleanup - SECOND call)
9. ❌ **cxf_basis_diff** (basis comparison for convergence)
10. ❌ **cxf_simplex_post_iterate** (stall detection, termination checks)

Outer loop control entirely missing:
- ❌ Outer basis snapshot
- ❌ Outer basis diff check
- ❌ Round limit enforcement (5/10/100)
- ❌ Adaptive convergence threshold

### From Spec Phase 7 (PWL Constraint Processing):

All PWL stages missing:

1. ❌ PWL variable iteration
2. ❌ Breakpoint ratio test
3. ❌ Coefficient delta application
4. ❌ Direction determination
5. ❌ Column selection
6. ❌ Objective update on breakpoint crossing
7. ❌ Segment shifting/finalization

### From Spec Phase 4 (Crash Basis):

1. ❌ **cxf_simplex_crash** - Never called
2. ❌ Primal crash variant
3. ❌ Constraint feasibility evaluation
4. ❌ Sparsity-based basis assignment

### From Spec Phase 5 (Crossover):

1. ❌ **cxf_crossover** - Not called
2. ❌ **cxf_crossover_bounds** - Not called
3. ❌ Quadratic variable processing
4. ❌ Binary variable linearization
5. ❌ Barrier solution presence check

### From Post-Loop Processing:

1. ❌ **cxf_simplex_postsolve** - Not called
2. ❌ **cxf_propagate_bounds** - Not delegated to
3. ❌ Tight inequality conversion
4. ❌ Solution-found callbacks

---

## Pipeline Stages Not In Spec

The following stages are implemented but NOT in the spec:

### 1. Obvious Infeasibility Check
**Location:** solve_lp.c lines 491-588
**Function:** `check_obvious_infeasibility()`
**Purpose:** Detect trivial infeasibility via:
- Single constraint bound propagation (O(m*nnz))
- Parallel constraint contradiction (O(m²*n), only for m ≤ 100)

**Analysis:**
- Not mentioned in spec
- Reasonable preprocessing optimization
- Has O(m²) complexity limit to avoid slowdown on large problems
- Early return bypasses pipeline

### 2. Obvious Unboundedness Check
**Location:** solve_lp.c lines 590-666
**Function:** `check_obvious_unboundedness()`
**Purpose:** Detect unbounded variables via ray analysis

**Analysis:**
- Not mentioned in spec
- Checks if any variable can go to infinity while improving objective
- Early return bypasses pipeline

### 3. Massive Inline Feasibility Correction
**Location:** solve_lp.c lines 766-1193 (428 lines!)
**Purpose:** After Phase I claims optimality, recompute constraint violations and correct basic variable values iteratively

**Analysis:**
- NOT in spec at all
- Huge inline block (should be separate function)
- Iterative correction loop (up to 10 passes)
- Handles numerical drift in Phase I
- Has extensive DEBUG_PHASE1 instrumentation
- This looks like a workaround for numerical issues

### 4. Row-Major Format Building
**Location:** solve_lp.c lines 504-509
**Function:** Calls `cxf_prepare_row_data()` and `cxf_build_row_major()`
**Purpose:** Build CSR format for fast row access

**Analysis:**
- Called from `check_obvious_infeasibility()`
- Not mentioned in spec
- Utility for faster row scans

### 5. Auxiliary Variable Coefficient Computation
**Location:** solve_lp.c lines 290-307, 54-71 (iterate.c)
**Function:** `get_auxiliary_coeff()` and `get_auxiliary_coeff_fallback()`
**Purpose:** Compute +1/-1 coefficient for slack/surplus/artificial variables based on constraint sense and RHS

**Analysis:**
- Implementation detail for Phase I
- Not explicitly in spec (but necessary for standard form conversion)
- Duplicated between solve_lp.c and iterate.c

### 6. Manual Phase Transition Code
**Location:** solve_lp.c lines 220-264
**Function:** `transition_to_phase_two()`
**Purpose:** Phase I → Phase II transition

**Analysis:**
- Should be handled by cxf_simplex_phase_end (per spec)
- Implemented as separate static function
- Restores objective, fixes artificial variables
- Resets anti-cycling state

---

## Positive Findings

Despite the numerous violations, some things ARE correctly implemented:

### ✅ Correct: Basic Simplex Structure
- Phase I with artificial variables works
- Phase II optimizes original objective
- Iteration limits enforced
- Status codes propagated correctly

### ✅ Correct: Phase I Artificial Variable Setup
**Location:** solve_lp.c lines 54-206
**Function:** `setup_phase_one()`

**What It Does Well:**
- Creates slack/surplus/artificial variables correctly
- Handles <=, >=, = constraints with proper coefficients
- Sets Phase I objective to minimize sum of artificials
- Initializes basis with auxiliary variables
- Stores diagonal coefficients (diag_coeff) for standard form

**Matches Spec Expectations:** Yes - this is standard Phase I simplex

### ✅ Correct: Reduced Cost Computation
**Location:** solve_lp.c lines 310-398
**Function:** `compute_reduced_costs()`

**What It Does Well:**
- Computes dual prices π = B^(-T) * c_B via BTRAN
- Computes reduced costs dj = cj - π^T * Aj
- Handles both original and auxiliary variables
- Fallback to simple approximation if BTRAN fails

**Matches Spec Expectations:** Yes - standard pricing computation

### ✅ Correct: Cleanup Sequence (Partial)
**Location:** solve_lp.c lines 1252-1262

**What It Does:**
- cxf_simplex_unperturb - Remove perturbation
- cxf_simplex_refine - Snap near-bound values
- cxf_extract_solution - Copy solution to model
- cxf_simplex_final - Deallocate state

**Missing:** cxf_simplex_postsolve, but the basic cleanup sequence is correct

### ✅ Correct: Anti-Cycling Logic in Iterate
**Location:** iterate.c lines 173-370 (Bland's rule, degenerate count)

While not called correctly from the main pipeline, the iterate.c implementation has:
- Bland's rule activation after 3*m degenerate iterations
- Degenerate pivot detection (stepSize < 1e-8)
- Virtual perturbation to break exact degeneracy
- Candidate filtering in Bland's mode

### ✅ Correct: Model Validation
**Location:** optimize_api.c lines 52-62

The API layer correctly:
- Validates model via cxf_checkmodel
- Checks environment is non-NULL
- Logs optimization start
- Resets termination flag

---

## Recommendations

### Immediate (P0) - Critical Fixes

1. **Implement Two-Level Iteration Loop**
   - Add outer loop with round control (max 5/10/100)
   - Add inner loop with basis stabilization
   - Call cxf_progress_snapshot at outer and inner starts
   - Call cxf_basis_diff to detect convergence
   - Exit inner loop when basis stabilizes
   - Exit outer loop when no progress

2. **Add Missing Per-Iteration Calls**
   - Call cxf_log_iteration_progress within inner loop
   - Call cxf_simplex_phase_end TWICE per iteration (pre/post pivot)
   - Call cxf_simplex_step2 and step3 after pivot
   - Call cxf_simplex_post_iterate for stall detection
   - Move perturbation INTO inner loop as conditional

3. **Add Parameter Backup/Restore**
   - Phase 1 of solve_lp: save LP-specific parameters
   - Phase 9: restore all saved parameters
   - Ensure restore on all exit paths

### Short-Term (P1) - Major Features

4. **Implement Crash Basis**
   - Add cxf_simplex_crash call before Phase I
   - Remove or replace setup_phase_one artificial-only approach
   - Use crash basis to reduce Phase I iterations

5. **Implement Crossover Integration**
   - Check for barrier solution presence
   - Call cxf_crossover and cxf_crossover_bounds
   - Handle QP quadratic variable processing

6. **Complete Post-Solve Pipeline**
   - Call cxf_simplex_postsolve after refine
   - Add cxf_propagate_bounds delegation
   - Add tight inequality conversion
   - Add solution-found callbacks

7. **Add Status Code Remapping**
   - Detect presolved matrix
   - Swap INFEASIBLE/UNBOUNDED if presolved
   - Detect crossover origin
   - Map ambiguous to INF_OR_UNBD

### Medium-Term (P2) - Advanced Features

8. **Implement PWL Constraint Processing**
   - Add PWL data structures to SolverState
   - Implement breakpoint ratio test
   - Add coefficient delta handling
   - Implement segment shifting
   - Disable step3 when PWL active

9. **Add Method Selection Logic**
   - Implement priority chain (user > mode > concurrent)
   - Default to dual simplex for LP
   - Handle primal/dual/auto modes

10. **Refactor Feasibility Correction**
    - Extract lines 766-1193 to separate function
    - Document numerical drift handling
    - Consider if this belongs in postsolve

### Long-Term (P3) - Architecture

11. **Add cxf_solver_dispatch Layer**
    - Implement model type detection (LP/QP/SOCP)
    - Implement algorithm selection heuristic
    - Add presolve-solve-uncrush cycle
    - Add parameter backup/restore (~30 params)
    - Dispatch to solve_lp for simplex path

12. **Add Logging Infrastructure**
    - Implement cxf_log_iteration_progress
    - Add progress callbacks
    - Remove DEBUG_PHASE1 conditional code
    - Use proper logging facility

---

## Impact Assessment

### System Integrity Impact: HIGH

The missing two-level iteration loop and basis convergence detection means:
- No protection against cycling on degenerate problems
- No adaptive convergence thresholds
- May loop forever or terminate prematurely
- Users cannot rely on robust convergence

### Feature Completeness Impact: CRITICAL

Missing features prevent:
- QP solving (no crossover from barrier)
- PWL objective handling (no PWL processing)
- Presolved model solving (no uncrush)
- Warm-start from barrier solutions
- Concurrent algorithm racing

### Performance Impact: HIGH

Missing optimizations:
- No crash basis (more Phase I iterations)
- No bound propagation (step2/step3 missing)
- No stall detection (may waste iterations)
- No round limits (may iterate excessively)

### Correctness Impact: MODERATE

Basic Phase I/Phase II simplex works, but:
- May not converge on degenerate problems
- No numerical safeguards from missing pipeline stages
- Status codes not correctly mapped for presolved models
- Solution extraction incomplete for non-optimal cases

---

## Conclusion

The implementation is a **basic Phase I/Phase II simplex prototype** that implements the core iteration mechanics but **completely omits the sophisticated orchestration infrastructure** specified in the v2 specs.

**The fundamental architectural feature - the two-level iteration loop with basis snapshots and convergence detection - is entirely missing.** This is like implementing a car engine without the transmission, cooling system, and control electronics.

**Spec Compliance: ~20%**
- Core iteration: ✅
- Pipeline orchestration: ❌
- Advanced features: ❌
- Robustness mechanisms: ❌

This is NOT production-ready LP solver code. It's an educational or early-prototype implementation that would need substantial work to meet the spec's architectural requirements.

---

## Files Examined

Implementation files:
- `/home/tobiasosborne/Projects/convexfeld/src/api/optimize_api.c` (108 lines)
- `/home/tobiasosborne/Projects/convexfeld/src/simplex/solve_lp.c` (1262 lines)
- `/home/tobiasosborne/Projects/convexfeld/src/simplex/setup.c` (214 lines)
- `/home/tobiasosborne/Projects/convexfeld/src/simplex/iterate.c` (456 lines)
- `/home/tobiasosborne/Projects/convexfeld/src/simplex/cleanup.c` (76 lines)
- `/home/tobiasosborne/Projects/convexfeld/src/simplex/post.c` (99 lines)

Specification files:
- `/home/tobiasosborne/Projects/convexfeld/docs/specs-v2/specs/integration/optimization_pipeline.md` (775 lines)
- `/home/tobiasosborne/Projects/convexfeld/docs/specs-v2/specs/modules/solve_lp_core.md` (496 lines)

**Total Lines Audited:** 2,215 lines implementation + 1,271 lines specification = 3,486 lines
