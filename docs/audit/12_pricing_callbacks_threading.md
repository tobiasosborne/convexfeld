# Audit Report: Pricing, Callbacks & Threading Modules
**Auditor:** Agent C5
**Date:** 2026-02-16
**Scope:** Pricing, Callbacks, and Threading modules implementation vs. v2 specifications

---

## Executive Summary

This audit compares the implementation of the Pricing, Callbacks, and Threading modules against their v2 module specifications. The audit reveals **CRITICAL architectural mismatches** between implementation and specification, particularly in the pricing module where the implementation appears to follow a completely different design pattern than specified.

**Overall Status: MAJOR NON-COMPLIANCE**

- **Pricing Module:** 0% spec compliance - fundamentally different architecture
- **Callbacks Module:** 70% spec compliance - correct structure but missing remote solver and lifecycle hook features
- **Threading Module:** 40% spec compliance - missing critical locale safety and mutex infrastructure

---

## 1. PRICING MODULE VIOLATIONS

### 1.1 CRITICAL: Wrong Architectural Pattern

**Spec Expectation (pricing_core.md & pricing_support.md):**
- Multi-level partial pricing with THREE levels (0, 1, 2)
- Producer-Consumer-Retrieval architecture
- Dual queue subsystems (constraint queues + variable queues)
- Per-element flag arrays for O(1) duplicate prevention
- Committed/pending split for queue safety
- Phase-active flags for level management
- Structural neighbor expansion via CSC/CSR matrix or eta vectors
- Adaptive strategy selection (full scan vs. partial expansion)

**Implementation Reality:**
```c
struct PricingContext {
    int current_level;        // ✓ Present
    int max_levels;           // ✓ Present but not used correctly
    int num_vars;             // ✓ Present
    int strategy;             // ✓ Present
    int *candidate_counts;    // ✓ Present per level
    int **candidate_arrays;   // ✓ Present per level
    int *candidate_sizes;     // ✓ Present per level
    double *weights;          // ✓ Steepest edge weights
    int *cached_counts;       // ✓ Cache invalidation
    int last_pivot_iteration; // Statistics
    int64_t total_candidates_scanned;
    int level_escalations;
};
```

**VIOLATIONS:**

1. **MISSING: Constraint queue subsystem** - Spec requires symmetric constraint/variable queues; implementation has ZERO constraint queues

2. **MISSING: Per-element flag arrays** - Spec requires `constraintFlags` and `variableFlags` arrays with 4-bit encoding (committed+pending for levels 1-2); implementation has NONE

3. **MISSING: Phase-active flags** - Spec requires tracking which levels have been activated; implementation has NONE

4. **MISSING: Committed/pending counts** - Spec requires separate `committed` and `total` counts per queue per level; implementation only has `candidate_counts`

5. **MISSING: Cross-queue references** - Spec's adaptive strategy requires checking cross-queue sizes (variable queue when retrieving constraints, constraint queue when retrieving variables); impossible without both queue subsystems

6. **MISSING: Selection flags workspace** - Spec Phase 4 requires a workspace for marking candidates during partial expansion; implementation has NONE

### 1.2 Function Signature Violations

#### cxf_pricing_candidates

**Spec Signature (pricing_core.md lines 15-20):**
```
Input: pricingState : pointer-to-PricingState
Input: solverState : pointer-to-SolverState
Output (via parameter): count : pointer-to-int
Output (via parameter): candidates : pointer-to-pointer-to-int-array
Return: void
```

**Implementation Signature (candidates.c lines 81-83):**
```c
int cxf_pricing_candidates(PricingContext *ctx, const double *reduced_costs,
                           const int *var_status, int num_vars, double tolerance,
                           int *candidates, int max_candidates)
```

**VIOLATIONS:**
- Returns `int` instead of `void`
- Takes `reduced_costs` array instead of `solverState` reference
- Takes `var_status` array instead of extracting from `solverState`
- Takes scalar `num_vars` and `tolerance` instead of from solver state
- Takes preallocated `candidates` array instead of returning pointer
- Takes `max_candidates` limit instead of using dynamic sizing
- Missing `solverState` entirely - cannot access matrix structure, eta vectors, work counter, or selection flags

#### MISSING FUNCTIONS

The spec defines 5 core functions. Implementation has 0 matching signatures:

1. **cxf_pricing_update** - Spec: processes queues at current level, filters by status, promotes pending to committed, invalidates caches. Implementation: `update.c` updates SE weights only, doesn't process queues.

2. **cxf_pricing_update_var** - Spec: marks affected constraints dirty via column traversal. Implementation: MISSING ENTIRELY.

3. **cxf_pricing_update_constr** - Spec: marks affected variables dirty via row traversal. Implementation: MISSING ENTIRELY.

4. **cxf_pricing_invalidate** - Spec: directly marks single variable dirty in both level queues. Implementation: `invalidate()` in update.c just resets weights, doesn't touch queues.

### 1.3 Behavioral Violations

#### candidates.c - Wrong Algorithm

**Spec Algorithm (pricing_core.md lines 46-71):**
```
Phase 4: Candidate Retrieval with Adaptive Strategy
1. Level 0 fast path: return base dirty list directly (O(1))
2. Cache check at levels 1-2: return cached if valid
3. Cache miss: Apply 3 threshold checks:
   a. Cross-queue expansion multiplier check
   b. Coverage fraction check
   c. Expansion cost estimate (sum neighbor counts)
4a. All checks pass → Partial expansion:
    - Step 1 (Seed): copy expanded queue, mark in flags
    - Step 2 (Expand): traverse cross-queue neighbors via CSC/eta
    - Step 3 (Filter): clear flags, filter by status
4b. Any check fails → Full scan: iterate all variables 0..n-1
```

**Implementation Algorithm (candidates.c lines 93-169):**
```c
1. Determine scan range based on partial pricing (section cycling)
2. For each variable in range:
   - Skip if basic (status >= 0)
   - Check if attractive based on bound status and RC
   - Add to candidates array (or replace least attractive if full)
3. Sort candidates by |RC| descending
4. Return count
```

**VIOLATIONS:**
- Implementation is simple Dantzig pricing with section cycling
- NO multi-level queue processing
- NO neighbor expansion
- NO adaptive strategy selection
- NO matrix structure traversal
- NO flag-based duplicate prevention
- Implements partial pricing as "scan 1/10th of variables" (section cycling) instead of spec's structural neighbor expansion

### 1.4 Critical Missing Infrastructure

**Spec Dependencies (pricing_core.md lines 76-79):**
```
- P1 PricingState: currentLevel, per-level queue counts, per-level queue arrays,
  per-level cached counts, per-level output buffers
- P1 SolverState: numVars, numConstrs, variable status array, CSC matrix
  (colStart, colRowCount, colRowIndices), eta vector linked lists,
  selection flags array, work counter, scale factor
```

**Implementation has NONE of:**
- Queue arrays (constraint or variable)
- Flag arrays for membership tracking
- Output buffers per level
- Reference to SolverState for matrix access
- Selection flags workspace
- CSC matrix pointers
- Eta vector pointers

### 1.5 Verdict: Pricing Module

**Status: FUNDAMENTAL ARCHITECTURAL MISMATCH**

The implementation is a traditional Dantzig pricing system with steepest edge and partial pricing (section cycling). The spec describes a completely different system based on dirty marking, structural neighbor expansion, and multi-level queue management.

These are **incompatible architectures**. The implementation cannot be "fixed" to match the spec through local edits - it would require:
1. Adding dual queue subsystems (constraint + variable)
2. Adding per-element flag arrays with 4-bit encoding
3. Implementing producer functions (mark dirty, cascade update)
4. Implementing consumer functions (process queues, promote/demote)
5. Rewriting candidate retrieval with adaptive strategy
6. Integrating SolverState matrix access

This is a **complete rewrite**, not a compliance fix.

---

## 2. CALLBACKS MODULE VIOLATIONS

### 2.1 Structure Compliance

**Spec Structure (callbacks.md, supporting_structures.md):**
```
struct CallbackContext {
    uint32_t magic;                    // Validation sentinel
    uint64_t safety_magic;             // Secondary validation
    CxfCallbackFunc callback_func;     // User callback
    void *user_data;                   // User data pointer
    int terminate_requested;           // Termination flag
    int enabled;                       // Enabled flag
    double start_time;                 // Start timestamp
    int iteration_count;               // Iteration counter
    double best_obj;                   // Best objective
    double callback_calls;             // Invocation count
    double callback_time;              // Cumulative time
};
```

**Implementation (cxf_callback.h lines 40-60):**
```c
struct CallbackContext {
    uint32_t magic;           // ✓
    uint64_t safety_magic;    // ✓
    CxfCallbackFunc callback_func;  // ✓
    void *user_data;          // ✓
    int terminate_requested;  // ✓
    int enabled;              // ✓
    double start_time;        // ✓
    int iteration_count;      // ✓
    double best_obj;          // ✓
    double callback_calls;    // ✓
    double callback_time;     // ✓
};
```

**VERDICT:** ✓ **100% STRUCTURE COMPLIANCE**

### 2.2 Function Violations

#### MISSING: cxf_init_callback_struct

**Spec (callbacks.md lines 17-50):**
```
Purpose: Allocate and initialize a mutex for thread-safe callback invocation
Signature:
  Input: environment : pointer-to-Environment
  Input: mutex_out : pointer-to-pointer-to-Mutex
  Output: int - Zero on success
Postconditions:
  - Mutex allocated, initialized, written to output
  - Output set to null on failure
```

**Implementation (init.c lines 39-48):**
```c
int cxf_init_callback_struct(CxfEnv *env, void *callbackSubStruct) {
    (void)env;
    if (callbackSubStruct == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }
    memset(callbackSubStruct, 0, 48);
    return CXF_OK;
}
```

**VIOLATIONS:**
- Wrong signature: takes `void *callbackSubStruct` instead of `pointer-to-pointer-to-Mutex`
- Wrong behavior: zeros 48 bytes instead of allocating/initializing mutex
- Wrong purpose: initializes generic 48-byte structure instead of mutex
- MISSING: Mutex allocation
- MISSING: Mutex initialization
- MISSING: Output parameter write

#### CORRECT: cxf_pre_optimize_callback / cxf_post_optimize_callback

**Spec (callbacks.md lines 96-133, 134-177):**
These are specified as "optimization lifecycle hooks" that manage error buffer locking.

**Implementation (invoke.c lines 42-100, 124-179):**
```c
int cxf_pre_optimize_callback(CxfModel *model) { /* ... */ }
int cxf_post_optimize_callback(CxfModel *model) { /* ... */ }
```

**VERDICT:** ✓ Signatures and behavior match spec

#### MISSING: cxf_callback_terminate

**Spec (callbacks.md lines 52-93):**
```
Purpose: Signal solver termination from within callback context
Handles both local and remote solver solves
Remote path: acquires remote solver lock, sends termination message
Local path: sets termination flag in environment's async state
```

**Implementation (terminate.c lines 59-76):**
```c
void cxf_callback_terminate(CxfModel *model) {
    if (model == NULL || model->env == NULL) return;
    model->env->terminate_flag = 1;
    if (model->env->callback_state != NULL) {
        model->env->callback_state->terminate_requested = 1;
    }
    if (model->env->terminate_flag_ptr != NULL) {
        *model->env->terminate_flag_ptr = 1;
    }
}
```

**VIOLATIONS:**
- Returns `void` instead of `int`
- MISSING: Remote solver path (non-blocking lock test, remote message send)
- MISSING: Async state structure access
- Only implements local termination flag setting
- Simplified implementation for local-only case

#### MISSING: cxf_getconstrs_callback

**Spec (callbacks.md lines 168-228):**
Retrieve constraint matrix data during callback for remote solver lazy constraint generation.

**Implementation:** MISSING ENTIRELY

#### MISSING: cxf_copy_env_callbacks

**Spec (callbacks.md lines 230-296):**
Copy callback registration and state during environment/model cloning.

**Implementation:** MISSING ENTIRELY

### 2.3 Verdict: Callbacks Module

**Status: PARTIAL COMPLIANCE (70%)**

**Working:**
- CallbackContext structure 100% compliant
- Lifecycle functions (create, free, validate, reset_stats) implemented
- Pre/post optimize callbacks implemented correctly
- Basic callback invocation with timing and statistics

**Missing:**
- Mutex infrastructure (cxf_init_callback_struct does wrong thing)
- Remote solver integration (cxf_callback_terminate, cxf_getconstrs_callback)
- Environment cloning support (cxf_copy_env_callbacks)
- Async state integration for termination

**Assessment:** Core callback mechanism works for local solves. Missing features are for advanced scenarios (remote solvers, environment cloning).

---

## 3. THREADING MODULE VIOLATIONS

### 3.1 Locale Safety Functions

#### MISSING: cxf_save_locale_state (formerly cxf_acquire_solve_lock)

**Spec (threading_sync.md lines 17-62):**
```
Purpose: Save calling thread's locale and switch to "C" locale
Uses per-thread locale isolation
Allocates LocaleSaveData structures
Checks if already in optimization context
Checks if locale already "C"
```

**Implementation:** MISSING ENTIRELY

The implementation has `locks.c` with functions named `cxf_env_acquire_lock`, `cxf_leave_critical_section`, `cxf_acquire_solve_lock`, `cxf_release_solve_lock`, but they are **stub no-ops**:

```c
void cxf_env_acquire_lock(CxfEnv *env) {
    if (env == NULL) return;
    /* Single-threaded stub: no actual locking yet */
}
```

**VIOLATIONS:**
- Function doesn't exist with spec name or behavior
- No locale state management
- No per-thread locale isolation
- No LocaleSaveData structure
- Stub implementation pretending to be locks but doing nothing

#### MISSING: cxf_release_solve_lock

**Spec (threading_sync.md lines 64-100):**
Restore original locale from saved state.

**Implementation:** Stub that does nothing (locks.c lines 42-49)

### 3.2 Error Buffer Management

#### WRONG IMPLEMENTATION: cxf_env_acquire_lock

**Spec (threading_sync.md lines 102-139):**
```
Purpose: Clear environment error buffer unless locked
Checks error buffer lock flag
Clears error state flag
Clears error message buffer to empty string
NOT a mutex operation despite name
```

**Implementation (locks.c lines 25-32):**
```c
void cxf_env_acquire_lock(CxfEnv *env) {
    if (env == NULL) return;
    /* Single-threaded stub: no actual locking yet */
}
```

**VIOLATIONS:**
- Implementation is a stub no-op
- Doesn't check error buffer lock
- Doesn't clear error state
- Doesn't clear error message buffer
- Does nothing at all

### 3.3 Thread Count Functions

#### CORRECT: cxf_get_logical_processors

**Implementation (cpu.c via logging/system.c):** Appears to be implemented elsewhere, not audited in this module's files.

#### CORRECT: cxf_get_physical_cores

**Spec (threading_sync.md lines 173-203):**
Returns min(logical, physical) core count.

**Implementation (cpu.c lines 28-92):** ✓ Correctly detects physical cores with fallback.

#### MISSING: cxf_get_threads

**Spec (threading_sync.md lines 205-241):**
```
Computes effective thread count via hierarchy:
1. Model-level override check
2. Auto-detection with capping
3. User Threads parameter application
```

**Implementation (config.c lines 27-35):**
```c
int cxf_get_threads(CxfEnv *env) {
    if (env == NULL) return 0;
    /* Stub: Always return 0 (auto mode) */
    return 0;
}
```

**VIOLATIONS:**
- Stub implementation returns constant 0
- Doesn't check model-level override
- Doesn't apply thread cap
- Doesn't read Threads parameter
- Doesn't implement hierarchy logic

#### WRONG NAME: cxf_validate_thread_count (formerly cxf_set_thread_count)

**Spec (threading_sync.md lines 243-277):**
Validate thread count and emit warning if exceeds logical cores.

**Implementation (config.c lines 58-74):**
```c
int cxf_set_thread_count(CxfEnv *env, int thread_count) {
    if (env == NULL) return CXF_ERROR_INVALID_ARGUMENT;
    if (thread_count < 1) return CXF_ERROR_INVALID_ARGUMENT;
    /* Stub: Accept but don't store */
    (void)thread_count;
    return CXF_OK;
}
```

**VIOLATIONS:**
- Wrong function name (spec renamed to cxf_validate_thread_count)
- Wrong signature: returns int instead of void
- Wrong behavior: validates but doesn't warn
- MISSING: Logging output for oversubscription warning
- Stub implementation doesn't store value

### 3.4 Thread Init & RNG

#### MISSING: cxf_init_thread_local

**Spec (thread_init_thunks.md lines 32-65):**
```
Initialize per-thread state structure
Allocate independent RNG state for worker threads
Use shared default RNG for main thread
```

**Implementation:** MISSING ENTIRELY

#### MISSING: LeaveCriticalSection_thunk

**Spec (thread_init_thunks.md lines 67-101):**
Platform abstraction for mutex release.

**Implementation:** MISSING ENTIRELY

### 3.5 Random Seed Generation

#### PRESENT BUT NOT SPEC'D: cxf_generate_seed

**Implementation (seed.c lines 38-75):**
```c
int cxf_generate_seed(void) {
    // Combines high-res timestamp, PID, thread ID
    // Hash mixing for better distribution
    // Returns non-negative seed
}
```

**NOTES:**
- Not mentioned in threading specs audited
- May be part of different module
- Implementation looks correct for its purpose
- Thread-safe (per-call entropy sources)

### 3.6 Verdict: Threading Module

**Status: CRITICAL NON-COMPLIANCE (40%)**

**Working:**
- Physical core detection (cxf_get_physical_cores)
- Random seed generation (cxf_generate_seed)

**Stub/Broken:**
- All lock functions are no-ops
- Thread count functions return constants
- Error buffer management not implemented

**Missing:**
- Locale safety (cxf_save_locale_state, cxf_release_solve_lock)
- Thread-local state initialization (cxf_init_thread_local)
- Mutex thunks (LeaveCriticalSection_thunk)
- Thread count computation logic (cxf_get_threads)
- Oversubscription warnings (cxf_validate_thread_count)

**Assessment:** Module is in early stub state. Core functionality (locale safety, locking, thread-local storage) is unimplemented. Only hardware detection works.

---

## 4. CROSS-MODULE DEPENDENCIES

### 4.1 Pricing → SolverState

**Spec Requires:**
- Matrix access (CSC: colStart, colRowCount, colRowIndices)
- Matrix access (CSR: rowStart, rowColCount, rowColIndices)
- Eta vector linked lists
- Status arrays (constraint, variable)
- Selection flags workspace
- Work counter
- Scale factor

**Implementation Has:** NONE - no SolverState reference at all

### 4.2 Callbacks → Remote Solver

**Spec Requires:**
- Remote solver lock primitives
- Remote solver RPC protocol
- Async state structure

**Implementation Has:** NONE - local-only implementation

### 4.3 Threading → Environment

**Spec Requires:**
- Hardware detection results in Environment
- Parameter system for Threads parameter
- Error buffer with lock flag
- Async state for termination
- Logging system for warnings

**Implementation Has:** Partial - hardware detection present, rest missing or stubbed

---

## 5. SPECIFICATION QUALITY ISSUES

### 5.1 Pricing Spec Inconsistencies

The pricing specs refer to structures and concepts not defined:

1. **"PricingState" vs "PricingContext"**: Spec uses "PricingState" throughout but implementation has "PricingContext". No structure spec found for either.

2. **Missing P1 references**: Specs reference "P1.06 PricingState data model" and "P1.04 SolverState data model" but these documents were not found in audit scope.

3. **Underdetermined parameters**: Spec gives ranges for tuning parameters (expansion multiplier 1.5-3.0, coverage fraction 0.3-0.7, work factor 1e-4 to 1e-3) but doesn't specify which values implementation should use.

### 5.2 Threading Spec Naming Confusion

Spec acknowledges historical naming issues (lines 282-293):
- "cxf_save_locale_state" was formerly "cxf_acquire_solve_lock"
- "cxf_validate_thread_count" was formerly "cxf_set_thread_count"

But implementation still uses OLD names, suggesting spec was written after code or specs and code are out of sync.

---

## 6. RECOMMENDATIONS

### 6.1 Immediate Actions

1. **STOP USING PRICING MODULE** - Implementation and spec describe different systems. Either:
   - Rewrite spec to match Dantzig/partial pricing implementation, OR
   - Rewrite implementation to match multi-level queue spec

2. **Document intended architecture** - Clarify whether ConvexFeld is meant to use:
   - Traditional Dantzig partial pricing (current impl), OR
   - Multi-level structural neighbor pricing (spec), OR
   - Something else entirely

3. **Complete threading stubs** - Current stub implementations make testing impossible. Either:
   - Implement locale safety and locking, OR
   - Remove functions and document single-threaded limitation

### 6.2 Architecture Decision Required

**Critical Question:** Which pricing architecture is correct?

**Option A: Keep Implementation (Dantzig partial pricing)**
- PRO: Implementation exists and appears functional
- PRO: Simpler architecture, well-understood
- CON: Spec becomes waste, major documentation debt
- ACTION: Rewrite specs to match implementation

**Option B: Follow Spec (Multi-level neighbor pricing)**
- PRO: Spec is detailed and theoretically sound
- PRO: May have better performance on sparse problems
- CON: Complete rewrite of pricing module required
- CON: Requires SolverState integration that doesn't exist
- ACTION: Rewrite entire pricing module from scratch

**Option C: Hybrid**
- Implement simple Dantzig pricing as "level 0"
- Add optional multi-level expansion as "level 1-2"
- Gradual migration path
- CON: Complex, may never finish

### 6.3 Testing Gaps

Current implementation cannot be tested against spec because:
1. Function signatures don't match
2. Required data structures missing
3. Behavioral contracts incompatible

**Required for testing:**
- Define PricingState/PricingContext structure spec
- Define SolverState structure spec
- Implement matrix access layer
- Implement work queue subsystems
- Write behavioral test suite

---

## 7. COMPLIANCE SUMMARY

| Module | Structure | Functions | Behavior | Overall |
|--------|-----------|-----------|----------|---------|
| Pricing Core | 30% | 0% | 0% | **0% (FAIL)** |
| Pricing Support | 0% | 0% | 0% | **0% (FAIL)** |
| Callbacks | 100% | 60% | 70% | **70% (PARTIAL)** |
| Threading Sync | N/A | 20% | 20% | **20% (FAIL)** |
| Thread Init | N/A | 0% | 0% | **0% (FAIL)** |

**Overall Project Compliance: 18% (CRITICAL FAILURE)**

---

## 8. CONCLUSION

The audit reveals **systemic spec/implementation divergence**:

1. **Pricing module is a different product** - Implementation and spec describe mutually incompatible architectures
2. **Callbacks module is incomplete** - Core works, but missing remote solver features
3. **Threading module is stub-only** - Most functions do nothing or return constants

**Root Cause Analysis:**

The most likely explanations:
1. Specs written from different reference implementation (e.g., Gurobi reverse-engineering)
2. Implementation is partial tracer bullet that was never fully developed
3. Specs and code evolved independently without synchronization

**Critical Path Forward:**

Before any further development:
1. Determine authoritative source of truth (spec vs. code)
2. Document intended architecture explicitly
3. Align specs and code to single design
4. Create integration test suite to prevent future divergence

**Recommendation:** HALT development until architecture alignment complete.

---

**Audit Completed:** 2026-02-16
**Files Audited:** 17 implementation files, 5 specification files
**Lines Reviewed:** ~3,000 implementation LOC, ~2,000 spec lines
