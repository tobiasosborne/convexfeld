# Audit Report: Simplex Modules (Setup, Pivot, Lifecycle, Phases)
**Auditor:** Agent C2
**Date:** 2026-02-16
**Scope:** src/simplex/setup.c, cleanup.c, post.c, pivot_primal.c, pivot_special.c, refine.c, quadratic.c
**Specs:** modules/simplex_iteration.md, simplex_lifecycle.md, simplex_phases.md, solution_processing.md, pivot_operations.md

---

## Summary

Audited 7 implementation files against 5 v2 module specifications covering simplex lifecycle, iteration, phases, pivot operations, and solution processing. The implementations are **stubs or partial implementations** that do not match the specifications.

**Key Findings:**
- **CRITICAL**: Core simplex iteration functions (cxf_simplex_step, cxf_simplex_step2, cxf_simplex_step3) are NOT implemented
- **CRITICAL**: Most lifecycle and phase management functions are NOT implemented
- **CRITICAL**: Solution processing functions (uncrushing, attribute wiring) are NOT implemented
- **MAJOR**: Implemented pivot functions are simplified versions missing BFRT bound flip logic
- **MAJOR**: Phase I/II transition logic is incomplete
- **MAJOR**: Missing pricing system integration throughout

The current implementations provide minimal setup/cleanup stubs but lack the core simplex algorithm machinery described in the specifications.

---

## Violations

### 1. CRITICAL: Missing Core Simplex Iteration Functions

**Spec:** `simplex_iteration.md` - Module defines 5 core iteration functions
**Implementation:** NONE of the core iteration functions exist

**Missing Functions:**
1. **cxf_log_iteration_progress** - Progress logging and callback invocation
2. **cxf_simplex_step** - Main simplex pivot with Harris two-pass ratio test and BFRT
3. **cxf_simplex_step2** - Variable-side bound-flipping queue processing
4. **cxf_simplex_step3** - Constraint-side bound propagation
5. **cxf_simplex_post_iterate** - Stall detection, termination checks, objective stagnation

**Impact:** The core revised simplex method iteration loop is completely unimplemented. No pricing, ratio test, pivot selection, or bound propagation logic exists.

---

### 2. CRITICAL: Missing Lifecycle Functions

**Spec:** `simplex_lifecycle.md` - Module defines 3 lifecycle functions
**Implementation:** NONE of the lifecycle functions exist

**Missing Functions:**
1. **cxf_simplex_init** - Allocate and initialize SolverState with 60+ arrays
2. **cxf_simplex_final** - Dual-feasibility-based variable fixing after solve
3. **cxf_simplex_postsolve** - Implied-bound tightening and resource deallocation

**Impact:** No proper solver state initialization or cleanup. The spec describes complex multi-phase initialization including quadratic terms, semi-continuous variables, general constraints, SOS constraints, PWL constraints, and ranged constraints. NONE of this infrastructure exists.

---

### 3. CRITICAL: Missing Phase Management Functions

**Spec:** `simplex_phases.md` - Module defines 6 phase management functions
**Implementation:** Only 2 stub functions exist, 4 are missing entirely

**Missing Functions:**
1. **cxf_simplex_crash** - Construct initial basis (crash algorithm)
2. **cxf_simplex_perturbation** - Anti-cycling bound perturbation (EXPAND algorithm)
3. **cxf_simplex_preprocess** - Variable fixing via bound tightening
4. **cxf_simplex_setup** - Compute constraint activity bounds

**Stub-Only Functions:**
- `cxf_simplex_phase_end` in `post.c` (lines 61-99) - Minimal Phase I->II transition, missing:
  - Constraint candidate processing
  - Free variable dual infeasibility checks
  - Basic constraint cleanup
  - Sparse removal logic
  - Activity bound recomputation
  - Detailed Phase I->II state transformations

- `cxf_simplex_refine` in `refine.c` (lines 27-82) - Only cleans near-zero values and snaps bounds, missing:
  - Non-basic variable cleanup with reduced cost evaluation
  - Basic variable recovery via cxf_pivot_primal
  - Eta record creation for fixings
  - Unboundedness detection

**Impact:** No basis construction, no anti-cycling mechanism, no preprocessing, no activity bounds.

---

### 4. CRITICAL: Missing Solution Processing Functions

**Spec:** `solution_processing.md` - Module defines 6 solution processing functions
**Implementation:** NONE of the solution processing functions exist

**Missing Functions:**
1. **cxf_process_lp_solution** - Wire LP result attributes
2. **cxf_uncrush_solution** - Reverse presolve transformations
3. **cxf_wire_result_attributes** - Wire general optimization result attributes
4. **cxf_compute_gap** - Compute relative optimality gap
5. **cxf_scale_objval** - Evaluate objective with scaling/quadratic/PWL terms
6. **cxf_copy_solution** - Solution pool management

**Impact:** No solution extraction, no presolve reversal, no attribute wiring, no solution pool.

---

### 5. MAJOR: Pivot Operations Are Incomplete

**Spec:** `pivot_operations.md` - Module defines 5 pivot functions
**Implementation:** 4 functions exist but are simplified versions

**cxf_pivot_primal** (`pivot_primal.c` lines 59-233):

**Present:**
- Bound feasibility checking (lines 97-104)
- Pivot value determination based on objective direction (lines 106-141)
- Objective value update (lines 152-153)
- Variable status update (lines 166-174)
- Constraint RHS update (lines 185-211)

**Missing:**
- Eta vector creation (noted in TODO line 217)
- Piecewise linear objective handling (TODO line 220-222)
- Quadratic objective neighbor updates (TODO line 224-227)
- Pricing state updates (TODO line 228-229)

**cxf_pivot_bound** (`pivot_special.c` lines 54-105):

**Present:**
- Objective value update (line 85)
- Objective coefficient zeroing (line 88)
- Bound update (lines 91-92)
- Variable status update (lines 95-102)

**Missing per spec:**
- Eta vector creation (full form with matrix column data)
- Quadratic objective update (diagonal Q-value)
- Q-neighbor linearization (off-diagonal coupling)
- Pricing notification via cxf_pricing_update_var
- Activity bound propagation via constraint scanning
- Matrix cleanup (CSR representation removal)
- All missing functionality noted in comments lines 42-44

**cxf_pivot_special** (`pivot_special.c` lines 129-191):

**Present:**
- Movement direction determination (lines 154-164)
- Unboundedness detection (lines 172-178, 182-185)
- Bound flip delegation to cxf_pivot_bound (lines 178, 187)

**Missing per spec:**
- Special constraint validation via cxf_special_check
- Equality constraint check
- Row elimination logic (Phase 5c)
- Q-matrix diagonal checks for quadratic variables
- All detailed logic noted in comments lines 115-120

**cxf_pivot_check** - NOT IMPLEMENTED (spec describes ratio test step bound computation)

**cxf_pivot_update** - NOT IMPLEMENTED (spec describes incremental activity bound maintenance)

---

### 6. MAJOR: Wrong Setup Function Signature

**Spec:** `simplex_phases.md` lines 174-224 - cxf_simplex_setup
```
Signature:
- Input: state : pointer-to-SolverState
- Input: env : pointer-to-Environment
- Input: count : int - Number of constraint indices to process
- Input: indices : pointer-to-int-array - Optional array of constraint indices
- Output: void (results written to activity bound arrays)
```

**Implementation:** `setup.c` lines 109-158 - cxf_simplex_setup
```c
int cxf_simplex_setup(SolverContext *state, CxfEnv *env)
```

**Violations:**
- Missing `count` parameter
- Missing `indices` parameter for selective constraint processing
- Returns `int` instead of `void`
- Does not compute constraint activity bounds (spec's primary purpose)
- Only initializes reduced costs, dual values, pricing, phase determination

**Impact:** Cannot perform selective activity bound updates. Does not match spec's behavior at all.

---

### 7. MAJOR: Wrong Preprocess Function Behavior

**Spec:** `simplex_phases.md` lines 114-171 - cxf_simplex_preprocess
- Should fix variables with tight bound ranges
- Should create eta records for fixings
- Should update constraint activities
- Should adjust objective value
- Should sort candidates by bound width

**Implementation:** `setup.c` lines 173-214 - cxf_simplex_preprocess
```c
int cxf_simplex_preprocess(SolverContext *state, CxfEnv *env, int flags) {
    // Only checks for bound violations (lines 195-199)
    // Comments say full preprocessing requires constraint matrix access
    // Returns infeasible or success, does NOTHING else
}
```

**Violations:**
- Does not fix any variables
- Does not create eta records
- Does not update activities or objective
- Does not implement any of the 8 preprocessing phases described
- Comment explicitly defers implementation (lines 201-211)

---

### 8. MAJOR: Cleanup Function Is a Stub

**Spec:** `simplex_lifecycle.md` lines 192-273 - cxf_simplex_postsolve
- 10 phases of post-solve processing
- Implied bound computation
- Variable fixing
- Constraint conversion
- Memory cleanup

**Implementation:** `cleanup.c` lines 37-76 - cxf_simplex_cleanup
```c
int cxf_simplex_cleanup(SolverContext *state, CxfEnv *env) {
    // Validate inputs (lines 39-45)
    // Comments describe future implementation (lines 47-73)
    // Returns success without doing anything (line 75)
}
```

**Violations:**
- Does not unscale primal/dual values
- Does not restore fixed variables
- Does not unscale reduced costs
- Does not perform any of the 10 phases
- Explicit placeholder (comment line 48)

---

### 9. MAJOR: Post-Iterate Function Is a Stub

**Spec:** `simplex_iteration.md` lines 297-383 - cxf_simplex_post_iterate
- Stall detection with dimension-scaled progress formulas
- Solve status verification (time/iteration limits)
- Objective stagnation detection
- User interrupt handling

**Implementation:** `post.c` lines 27-48 - cxf_simplex_post_iterate
```c
int cxf_simplex_post_iterate(SolverContext *state, CxfEnv *env) {
    // Update work counter (lines 33-35)
    // Check refactorization (lines 38-45)
    // Return 0 or 1 (lines 47, 44)
}
```

**Violations:**
- No stall detection (dimension-scaled progress formulas)
- No termination condition checking
- No objective stagnation detection
- No user interrupt handling
- Only checks refactorization interval

---

### 10. MAJOR: Missing Harris Ratio Test and BFRT

**Spec:** `simplex_iteration.md` lines 96-138 - cxf_simplex_step Phase 5
- Harris two-pass ratio test (passes 1-2)
- Bound-flipping ratio test (BFRT, pass 3)
- Multiple bound flips may occur in a single step
- Row coefficient negation for flip consistency
- Auxiliary data array swapping

**Implementation:** NONE of this exists
- No ratio test implementation
- No BFRT bound flip logic
- No coefficient negation
- No auxiliary data handling

**Impact:** The core pivot selection mechanism is completely absent. Cannot select leaving variables correctly.

---

### 11. MAJOR: Missing Bidirectional Bound Propagation

**Spec:** `simplex_iteration.md` lines 140-293
- cxf_simplex_step2: Variable-side bound flipping with flip-type classification
- cxf_simplex_step3: Constraint-side implied bound propagation
- Two-stage infeasibility detection
- Bound-change eta record creation
- Pricing notification and activity bound updates

**Implementation:** NONE

**Impact:** No bound tightening during solve. Cannot exploit constraint structure for faster convergence.

---

### 12. MAJOR: Quadratic Adjust Function Is a Stub

**Spec:** Should update reduced costs for quadratic programming
- Compute q_j = sum_k Q[j,k] * x[k]
- Update reducedCosts[j] += q_j
- Handle single variable or all nonbasic variables

**Implementation:** `quadratic.c` lines 44-100
```c
int cxf_quadratic_adjust(SolverContext *state, int varIndex) {
    // Validate inputs (lines 46-58)
    // TODO comments (lines 60-96)
    // Return success without doing anything (line 99)
}
```

**Violations:**
- Explicit stub with TODO (comment line 61)
- No Q matrix access
- No reduced cost updates

---

### 13. MODERATE: Missing Pricing System Integration

**Spec:** Throughout all modules
- cxf_pricing_candidates retrieval
- cxf_pricing_mark_dirty notifications
- cxf_pricing_cascade_update after pivots
- cxf_pricing_update_var for fixed variables
- Multi-level pricing tolerance selection

**Implementation:** Partial in setup.c
- `init_pricing` (lines 76-97) creates pricing context
- No candidate retrieval
- No dirty marking
- No cascade updates
- No variable updates
- No tolerance tier selection

**Impact:** Pricing state is initialized but never used. Cannot select entering variables.

---

### 14. MODERATE: Missing Special Variable Handling

**Spec:** `simplex_lifecycle.md` Phase 7
- Quadratic terms (Q matrix storage)
- Semi-continuous variables (bound relaxation)
- General constraints (indicator constraints)
- Quadratic constraints (normalization)
- SOS constraints
- Piecewise-linear constraints (breakpoint arrays, linearization)
- Ranged constraints (flag marking)

**Implementation:** `setup.c` only mentions semi-continuous in comments
- No Q matrix handling
- No semi-continuous bound relaxation
- No general constraint processing
- No SOS/PWL infrastructure

---

### 15. MODERATE: Wrong Phase Transition Logic

**Spec:** `simplex_phases.md` lines 229-296 - cxf_simplex_phase_end
- Detailed Phase I->II transition with 6 state transformations
- Objective function swap
- Reduced cost recomputation
- Pricing state reset
- Constraint cleanup
- Basis preservation
- Tolerance adjustment

**Implementation:** `post.c` lines 61-99 - cxf_simplex_phase_end
```c
int cxf_simplex_phase_end(SolverContext *state, CxfEnv *env) {
    // Only process Phase I (lines 67-69)
    // Check feasibility (lines 72-74)
    // Restore objective (lines 77-84)
    // Recompute objective value (lines 87-93)
    // Transition to Phase II (line 96)
}
```

**Violations:**
- No reduced cost recomputation (critical for Phase II)
- No pricing state reset
- No constraint cleanup
- No basis preservation/refactorization
- Manually recomputes objective instead of proper reduced cost update

---

### 16. MINOR: Function Name Mismatch

**Spec:** `simplex_lifecycle.md` line 227 - Function renamed from cxf_simplex_cleanup to cxf_simplex_postsolve

**Implementation:** `cleanup.c` line 37 - Still named cxf_simplex_cleanup

**Impact:** Low - naming inconsistency only

---

### 17. MINOR: Missing Work Counter Patterns

**Spec:** Throughout specs - Work counter updates with per-operation cost estimates
- Updated after column scans
- Updated after constraint processing
- Updated proportionally to operations performed

**Implementation:** Sporadic work counter usage
- `post.c` line 34: Updates work counter in post_iterate
- Most functions do not update work counter

---

## Spec Functions Not Implemented

### Simplex Iteration Module (5/5 missing):
1. cxf_log_iteration_progress
2. cxf_simplex_step
3. cxf_simplex_step2
4. cxf_simplex_step3
5. cxf_simplex_post_iterate (stub only)

### Simplex Lifecycle Module (3/3 missing):
1. cxf_simplex_init
2. cxf_simplex_final
3. cxf_simplex_postsolve (stub named cleanup exists)

### Simplex Phases Module (4/6 missing):
1. cxf_simplex_crash
2. cxf_simplex_perturbation
3. cxf_simplex_preprocess (stub exists)
4. cxf_simplex_setup (wrong signature/behavior)
5. cxf_simplex_phase_end (minimal stub exists)
6. cxf_simplex_refine (stub exists)

### Solution Processing Module (6/6 missing):
1. cxf_process_lp_solution
2. cxf_uncrush_solution
3. cxf_wire_result_attributes
4. cxf_compute_gap
5. cxf_scale_objval
6. cxf_copy_solution

### Pivot Operations Module (3/5 missing):
1. cxf_pivot_check
2. cxf_pivot_bound (simplified version exists)
3. cxf_pivot_primal (simplified version exists)
4. cxf_pivot_special (simplified version exists)
5. cxf_pivot_update

**Total Missing: 21/25 functions (84%)**

---

## Code Functions Not In Spec

### In setup.c:
1. `has_bound_violation` (lines 43-51) - Helper not in spec
2. `init_reduced_costs` (lines 56-61) - Helper not in spec
3. `init_dual_values` (lines 66-71) - Helper not in spec
4. `init_pricing` (lines 76-97) - Helper not in spec

### In cleanup.c:
None - single stub function

### In post.c:
None - two stub functions

### In pivot_primal.c:
None - single simplified function

### In pivot_special.c:
None - two simplified functions

### In refine.c:
None - single stub function

### In quadratic.c:
None - single stub function

**Total Extra: 4 helper functions (all reasonable internal helpers)**

---

## Recommendations

### IMMEDIATE (Blockers):

1. **Implement core simplex iteration loop** (cxf_simplex_step, step2, step3)
   - Harris two-pass ratio test with tolerance bands
   - Bound-flipping ratio test (BFRT) logic
   - Pricing candidate retrieval and processing
   - Eta vector creation for all pivot types
   - Constraint coefficient negation for bound flips

2. **Implement pricing system integration**
   - cxf_pricing_candidates for entering variable selection
   - Multi-level tolerance selection
   - Dirty marking and cascade updates
   - Variable and constraint candidate queues

3. **Implement lifecycle management**
   - cxf_simplex_init with full 8-phase initialization
   - Special variable handling (quadratic, semi-continuous, SOS, PWL)
   - cxf_simplex_final with dual-feasibility fixing
   - cxf_simplex_postsolve with 10-phase cleanup

4. **Implement phase management**
   - cxf_simplex_crash for initial basis construction
   - cxf_simplex_setup for activity bound computation
   - cxf_simplex_preprocess for variable fixing
   - cxf_simplex_perturbation for anti-cycling

### HIGH PRIORITY:

5. **Complete pivot operations**
   - Add eta vector creation to cxf_pivot_primal
   - Add quadratic and PWL handling
   - Add pricing notifications
   - Implement cxf_pivot_check for ratio test bounds
   - Implement cxf_pivot_update for incremental activity maintenance
   - Add full activity bound propagation to cxf_pivot_bound

6. **Fix Phase I/II transition**
   - Add reduced cost recomputation after objective swap
   - Add pricing state reset
   - Add constraint cleanup with sparse removal
   - Add basis refactorization trigger

7. **Implement post-iteration monitoring**
   - Dimension-scaled stall detection formulas
   - Termination condition checking
   - Objective stagnation detection
   - User interrupt handling

### MEDIUM PRIORITY:

8. **Implement solution processing**
   - cxf_uncrush_solution for presolve reversal
   - cxf_scale_objval for objective evaluation
   - cxf_wire_result_attributes for attribute binding
   - cxf_compute_gap for optimality gap
   - cxf_copy_solution for solution pool

9. **Add special variable infrastructure**
   - Q matrix storage and operations
   - Semi-continuous variable bound relaxation/restoration
   - SOS constraint handling
   - PWL breakpoint processing and linearization
   - General constraint processing

10. **Fix function signatures**
    - Add count/indices parameters to cxf_simplex_setup
    - Change return type to void where specified

### TESTING:

11. **Test critical paths first**
    - Pure LP simplex solve (Phase II only)
    - Two-phase simplex (infeasible start)
    - Unbounded detection
    - Bound flip execution
    - Phase transition

12. **Test special cases**
    - Quadratic objectives
    - PWL objectives
    - Semi-continuous variables
    - Tight bound ranges
    - Degenerate problems requiring perturbation

---

## Architecture Notes

The specifications describe a sophisticated revised simplex implementation with:
- **Product Form of the Inverse (PFI)** basis representation with eta vectors
- **Harris two-pass ratio test** with numerical tolerance bands
- **Bound-flipping ratio test (BFRT)** for multiple bound flips per pivot
- **Bidirectional bound propagation** (variable-side and constraint-side)
- **Multi-level pricing** with tolerance tiers and steepest edge
- **Anti-cycling perturbation** (EXPAND algorithm)
- **Crash basis construction** for warm starts
- **Two-phase simplex** with proper objective swapping
- **Dual/primal switching** based on problem characteristics
- **Activity bound maintenance** for fast infeasibility detection
- **Quadratic/PWL objective support** with linearization
- **Solution pool management** with gap-based pruning

The current implementation provides only:
- Basic solver state structure
- Minimal setup/cleanup stubs
- Simplified pivot operations without eta vectors or pricing
- No ratio test
- No bound propagation
- No pricing system
- No special variable handling

**Gap: ~90% of specified functionality is missing.**

---

## Conclusion

The simplex module implementations are **NOT compliant** with the v2 specifications. The code provides skeletal stubs and simplified pivot operations but lacks the core simplex algorithm machinery. This is consistent with a project in early development or a placeholder for future implementation.

**Priority:** CRITICAL - Core solver functionality is absent. Cannot solve LP problems without implementing the missing iteration loop, pricing, ratio test, and phase management.

**Estimated Effort:** Large - The specifications describe approximately 5,000+ lines of complex algorithmic code across 25 functions. Current implementation is ~1,000 lines of stubs.

**Next Steps:**
1. Implement cxf_simplex_step with full Harris ratio test and BFRT
2. Implement pricing candidate retrieval and selection
3. Implement cxf_simplex_init/final/postsolve lifecycle management
4. Add eta vector creation throughout pivot operations
5. Implement activity bound computation and maintenance
6. Add Phase I/II transition logic with reduced cost recomputation
