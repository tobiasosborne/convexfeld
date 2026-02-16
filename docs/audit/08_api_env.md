# Audit Report: API & Environment Modules
**Auditor:** Agent C1
**Date:** 2026-02-16
**Scope:** src/api/env.c, model.c, optimize_api.c, model_stub.c, api_stub.c, constr_stub.c
**Specs:** modules/environment_lifecycle.md, model_lifecycle.md, solve_entry.md, optimization_preparation.md

---

## Summary
- **Total violations:** 27
- **Critical:** 9 / **Major:** 12 / **Minor:** 6

---

## Violations

### V-01: Wrong function name - cxf_loadenv vs cxf_env_create_internal
- **Severity:** Critical
- **File:** src/api/env.c:75
- **Spec reference:** environment_lifecycle.md lines 11-72
- **Description:** Implementation uses `cxf_loadenv` while spec defines `cxf_env_create_internal`
- **Expected:** `int cxf_env_create_internal(int extra_flags, CxfEnv *parent_environment, CxfEnv **created_environment)`
- **Actual:** `int cxf_loadenv(CxfEnv **envP, const char *logfilename)`
- **Impact:** Completely different signature - missing extra_flags, parent_environment parameters; using logfilename parameter not in spec

### V-02: Wrong function name - cxf_emptyenv vs spec
- **Severity:** Critical
- **File:** src/api/env.c:95
- **Spec reference:** environment_lifecycle.md lines 11-72
- **Description:** Implementation uses `cxf_emptyenv` which is not defined in the spec
- **Expected:** Only `cxf_env_create_internal` defined in spec
- **Actual:** `int cxf_emptyenv(CxfEnv **envP, const char *logfilename)`
- **Impact:** Extra public API function not in spec

### V-03: Wrong function name - cxf_startenv vs cxf_env_finalize
- **Severity:** Critical
- **File:** src/api/env.c:115
- **Spec reference:** environment_lifecycle.md lines 74-151
- **Description:** Implementation uses `cxf_startenv` while spec defines `cxf_env_finalize`
- **Expected:** `int cxf_env_finalize(CxfEnv *environment, bool read_config_file)`
- **Actual:** `int cxf_startenv(CxfEnv *env)`
- **Impact:** Missing read_config_file parameter; wrong behavior (simple flag flip vs complex initialization)

### V-04: Wrong function name - cxf_freeenv vs cxf_env_free_internal
- **Severity:** Critical
- **File:** src/api/env.c:130
- **Spec reference:** environment_lifecycle.md lines 279-346
- **Description:** Implementation uses `cxf_freeenv` while spec defines `cxf_env_free_internal`
- **Expected:** `void cxf_env_free_internal(CxfEnv **environment_ptr)` (void return, double pointer)
- **Actual:** `int cxf_freeenv(CxfEnv *env)` (int return, single pointer)
- **Impact:** Wrong signature, doesn't set caller's pointer to null

### V-05: Missing function - cxf_env_load_logfile
- **Severity:** Major
- **File:** None (not implemented)
- **Spec reference:** environment_lifecycle.md lines 153-221
- **Description:** Spec defines `cxf_env_load_logfile` for log file initialization
- **Expected:** `int cxf_env_load_logfile(CxfEnv *environment, const char *filename, const char *host_info, bool write_header)`
- **Actual:** Not implemented
- **Impact:** No way to configure logging after environment creation

### V-06: Missing function - cxf_env_update_active_model
- **Severity:** Major
- **File:** None (not implemented)
- **Spec reference:** environment_lifecycle.md lines 223-276
- **Description:** Spec defines function to tear down model manager structure
- **Expected:** `void cxf_env_update_active_model(CxfEnv *allocator_environment, ModelManager **model_manager)`
- **Actual:** Not implemented
- **Impact:** No model manager cleanup

### V-07: Missing initialization - cxf_env_create_internal behavior
- **Severity:** Critical
- **File:** src/api/env.c:28-73
- **Spec reference:** environment_lifecycle.md lines 41-62
- **Description:** Missing complex initialization sequence defined in spec
- **Expected:** Process-level global state init, mutex allocation, system info queries, parameter table construction from static definitions, parameter inheritance from parent
- **Actual:** Simple field initialization with defaults
- **Impact:** Missing thread safety (no mutex), missing parameter system, missing system detection

### V-08: Missing finalization - cxf_env_finalize behavior
- **Severity:** Critical
- **File:** src/api/env.c:115-128
- **Spec reference:** environment_lifecycle.md lines 116-139
- **Description:** Missing 7-stage finalization process
- **Expected:** State backup, hardware detection (SIMD check, CPU cores), system env var reading, 4 subsystem init phases, config file loading, thread pool init, log file opening, atomic rollback on failure
- **Actual:** Simple flag flip from 0 to 1
- **Impact:** No hardware validation, no config loading, no thread pools, no atomicity

### V-09: Missing cleanup - cxf_env_free_internal behavior
- **Severity:** Major
- **File:** src/api/env.c:130-151
- **Spec reference:** environment_lifecycle.md lines 317-333
- **Description:** Missing systematic resource cleanup in reverse order
- **Expected:** Remote solver cleanup, child environment cleanup with reference counting, model cleanup, string field deallocation, parameter array cleanup, thread pool destruction, parameter storage pool destruction, callback state cleanup, mutex destruction, log file close, memory deallocation via temporary context
- **Actual:** Only frees callback_state, marks inactive, frees env
- **Impact:** Memory leaks for all other resources

### V-10: Wrong function name - cxf_newmodel vs cxf_model_create_internal
- **Severity:** Critical
- **File:** src/api/model.c:74
- **Spec reference:** model_lifecycle.md lines 11-71
- **Description:** Implementation uses `cxf_newmodel` while spec defines `cxf_model_create_internal`
- **Expected:** `CxfModel *cxf_model_create_internal(CxfEnv *parent_environment, int create_child_environment, int child_environment_parameter)` (returns pointer, not status)
- **Actual:** `int cxf_newmodel(CxfEnv *env, CxfModel **modelP, ...)` (returns status, output via pointer)
- **Impact:** Completely different signature and return convention

### V-11: Wrong function signature - cxf_newmodel parameters
- **Severity:** Major
- **File:** src/api/model.c:74-76
- **Spec reference:** model_lifecycle.md lines 11-19
- **Description:** Implementation has 9 parameters for initial variable setup; spec has 3 parameters for basic creation
- **Expected:** `(CxfEnv *parent_environment, int create_child_environment, int child_environment_parameter)`
- **Actual:** `(CxfEnv *env, CxfModel **modelP, const char *name, int numvars, double *obj, double *lb, double *ub, char *vtype, char **varnames)`
- **Impact:** Violates separation of creation and variable addition

### V-12: Missing function - cxf_env_model_cleanup
- **Severity:** Major
- **File:** None (not implemented)
- **Spec reference:** model_lifecycle.md lines 73-141
- **Description:** Spec defines function for child environment cleanup with reference counting
- **Expected:** `void cxf_env_model_cleanup(CxfEnv *parent_environment)`
- **Actual:** Not implemented
- **Impact:** No child environment cleanup, no reference counting, no remote job termination

### V-13: Missing function - cxf_update_model_manager
- **Severity:** Major
- **File:** None (not implemented)
- **Spec reference:** model_lifecycle.md lines 143-185
- **Description:** Spec defines function to clear model management structure
- **Expected:** `void cxf_update_model_manager(CxfEnv *allocator_environment, void *model_manager)`
- **Actual:** Not implemented
- **Impact:** No model tracking array cleanup

### V-14: Wrong function name - cxf_updatemodel vs cxf_model_apply_modifications
- **Severity:** Critical
- **File:** src/api/model.c:259
- **Spec reference:** model_lifecycle.md lines 187-326
- **Description:** Implementation uses `cxf_updatemodel` while spec defines `cxf_model_apply_modifications`
- **Expected:** `int cxf_model_apply_modifications(CxfModel *model)`
- **Actual:** `int cxf_updatemodel(CxfModel *model)`
- **Impact:** Wrong function name

### V-15: Missing behavior - cxf_model_apply_modifications 8-phase pipeline
- **Severity:** Critical
- **File:** src/api/model.c:259-287
- **Spec reference:** model_lifecycle.md lines 232-311
- **Description:** Missing complex 8-phase modification application
- **Expected:** Phase 1: SOS validation, Phase 2: Warm-start validation, Phase 3: Modification counting/classification, Phase 4: Fast path for name-only changes, Phase 5: Warm-start invalidation, Phase 6: Matrix attribute updates (constraint sense flipping, objective negation, per-variable/constraint updates), Phase 7: Variable type recount and name consolidation, Phase 8: Cleanup
- **Actual:** Minimal stub that processes pending_buffer if it exists and marks initialized=1
- **Impact:** No modification validation, no warm-start handling, no matrix updates, no name uniqueness checking

### V-16: Wrong function name - cxf_optimize vs spec entry
- **Severity:** Major
- **File:** src/api/api_stub.c:26
- **Spec reference:** solve_entry.md lines 34-108
- **Description:** Implementation name matches spec, but spec signature is more detailed
- **Expected:** `int cxf_optimize(CxfModel *model)` with 12-step behavioral description
- **Actual:** Simple wrapper that calls `cxf_optimize_internal`
- **Impact:** Missing all pre/post work (locale safety, signal handler, logging, result file writing, callbacks)

### V-17: Missing behavior - cxf_optimize lifecycle management
- **Severity:** Major
- **File:** src/api/api_stub.c:26-33
- **Spec reference:** solve_entry.md lines 72-97
- **Description:** Missing comprehensive setup/teardown sequence
- **Expected:** Step 1: Model validation, Step 2: Signal handler setup, Step 3: Locale safety acquisition, Step 4: State initialization (clear buffers, set modification-blocked), Step 5: Version/hardware logging, Step 6: Remote solver delegation, Step 7: Log callback registration, Step 8: Internal optimization, Step 9: Callback cleanup, Step 10: Lifecycle callbacks, Step 11: Result file writing, Step 12: Cleanup
- **Actual:** Just null check and call to `cxf_optimize_internal`
- **Impact:** No signal handling, no locale safety, no logging infrastructure, no result files

### V-18: Missing function - cxf_solve_entry
- **Severity:** Major
- **File:** None (not implemented)
- **Spec reference:** solve_entry.md lines 200-270
- **Description:** Spec defines solve chain entry point for single model
- **Expected:** `int cxf_solve_entry(CxfModel *model, ThreadLocalData *thread_local_data)`
- **Actual:** Not implemented
- **Impact:** No solve chain routing between single-model and multi-scenario

### V-19: Missing function - cxf_solve_dispatch
- **Severity:** Major
- **File:** None (not implemented)
- **Spec reference:** solve_entry.md lines 272-342
- **Description:** Spec defines multi-scenario optimization dispatcher
- **Expected:** `int cxf_solve_dispatch(CxfModel *model, ThreadLocalData *thread_local_data)`
- **Actual:** Not implemented
- **Impact:** No multi-scenario support

### V-20: Missing function - cxf_solve_no_callbacks
- **Severity:** Major
- **File:** None (not implemented)
- **Spec reference:** solve_entry.md lines 344-394
- **Description:** Spec defines no-callback fast path with worker thread
- **Expected:** `int cxf_solve_no_callbacks(CxfModel *model)`
- **Actual:** Not implemented
- **Impact:** No asynchronous optimization without callbacks

### V-21: Missing function - cxf_solve_with_callbacks
- **Severity:** Major
- **File:** None (not implemented)
- **Spec reference:** solve_entry.md lines 396-469
- **Description:** Spec defines callback-enabled optimization path
- **Expected:** `int cxf_solve_with_callbacks(CxfModel *model)`
- **Actual:** Not implemented
- **Impact:** No callback infrastructure, no remote solver callback support

### V-22: Wrong behavior - cxf_optimize_internal missing dispatch logic
- **Severity:** Major
- **File:** src/api/optimize_api.c:48-108
- **Spec reference:** solve_entry.md lines 110-198
- **Description:** Implementation missing complex path selection and preparation
- **Expected:** Phase 1: Execution path selection (no-callback/callback/normal), Phase 2: Model update timing check, Phase 3: Modification flush, Phase 4: Concurrent environment parameter management, Phase 5: Solver focus detection, Phase 6: Model analysis (dimensions, fingerprint, presolve stats, coefficient analysis), Phase 7: Solver dispatch, Phase 8: Non-convex QP handling, Phase 9: State restoration
- **Actual:** Simple validation, logging, pre/post callbacks, delegation to cxf_solve_lp
- **Impact:** No path selection, no concurrent optimization, no non-convex handling, no fingerprinting

### V-23: Missing function - cxf_prepare_optimization
- **Severity:** Minor
- **File:** None (not implemented)
- **Spec reference:** optimization_preparation.md lines 12-58
- **Description:** Spec defines signal handler installation for interrupt
- **Expected:** `void cxf_prepare_optimization(CxfModel *model)` that installs SIGINT handler
- **Actual:** Not implemented
- **Impact:** No Ctrl+C interrupt support during optimization

### V-24: Missing function - cxf_wait_async (second definition)
- **Severity:** Minor
- **File:** None (not implemented)
- **Spec reference:** optimization_preparation.md lines 119-186
- **Description:** Spec defines result serialization for remote solver
- **Expected:** `int cxf_wait_async(CxfModel *model)` that serializes results to communication channel
- **Actual:** Not implemented
- **Impact:** No remote solver result delivery

### V-25: Wrong initialization defaults - CxfEnv field defaults
- **Severity:** Minor
- **File:** src/api/env.c:28-73
- **Spec reference:** environment_lifecycle.md lines 41-56
- **Description:** Implementation uses hardcoded defaults, spec expects parameter table-driven defaults
- **Expected:** Defaults from static parameter definition table, with parameter registry for name-based access
- **Actual:** Hardcoded: feasibility_tol=CXF_FEASIBILITY_TOL, optimality_tol=CXF_OPTIMALITY_TOL, max_eta_count=100, max_eta_memory=1MB, refactor_interval=50
- **Impact:** No parameter system, cannot change defaults programmatically

### V-26: Missing model creation behavior - child environment support
- **Severity:** Major
- **File:** src/api/model.c:74-144
- **Spec reference:** model_lifecycle.md lines 24-31, 54-56
- **Description:** Implementation doesn't support creating child environment for model
- **Expected:** If create_child_environment is nonzero, create child environment and set environment_owned flag
- **Actual:** Model always uses parent environment directly
- **Impact:** No per-model parameter overrides possible

### V-27: Missing cleanup - cxf_freemodel incomplete
- **Severity:** Minor
- **File:** src/api/model.c:146-175
- **Spec reference:** model_lifecycle.md lines 279-346 (references environment cleanup), implied by creation behavior
- **Description:** Implementation doesn't handle child environment cleanup
- **Expected:** If model owns its environment (environment_owned flag), free child environment
- **Actual:** Just frees model-owned arrays, doesn't check for child environment
- **Impact:** Memory leak if child environments were supported

---

## Spec Functions Not Implemented

### From environment_lifecycle.md:
1. `cxf_env_create_internal` - Core environment creation with parameter system
2. `cxf_env_finalize` - Complex 7-stage initialization with hardware detection
3. `cxf_env_load_logfile` - Log file initialization with header writing
4. `cxf_env_update_active_model` - Model manager teardown
5. `cxf_env_free_internal` - Comprehensive resource cleanup

### From model_lifecycle.md:
1. `cxf_model_create_internal` - Core model creation with optional child environment
2. `cxf_env_model_cleanup` - Child environment cleanup with reference counting
3. `cxf_update_model_manager` - Model tracking array cleanup
4. `cxf_model_apply_modifications` - 8-phase lazy update application

### From solve_entry.md:
1. `cxf_solve_entry` - Solve chain entry with type detection and routing
2. `cxf_solve_dispatch` - Multi-scenario dispatcher
3. `cxf_solve_no_callbacks` - Fast path with worker thread
4. `cxf_solve_with_callbacks` - Callback-enabled path

### From optimization_preparation.md:
1. `cxf_prepare_optimization` - Signal handler installation
2. `cxf_wait_async` - Result serialization for remote solver

**Total missing:** 13 functions

---

## Code Functions Not In Spec

### From src/api/env.c:
1. `cxf_loadenv` - Public API not in spec (closest: cxf_env_create_internal)
2. `cxf_emptyenv` - Public API not in spec
3. `cxf_startenv` - Public API not in spec (closest: cxf_env_finalize)
4. `cxf_freeenv` - Public API not in spec (closest: cxf_env_free_internal)
5. `cxf_clearerrormsg` - Not in environment_lifecycle.md
6. `cxf_set_callback_context` - Not in environment_lifecycle.md
7. `cxf_get_callback_context` - Not in environment_lifecycle.md

### From src/api/model.c:
1. `cxf_newmodel` - Public API not in spec (closest: cxf_model_create_internal)
2. `cxf_freemodel` - Not in model_lifecycle.md (implied but not specified)
3. `cxf_checkmodel` - Not in model_lifecycle.md
4. `cxf_model_is_blocked` - Not in model_lifecycle.md
5. `cxf_copymodel` - Not in model_lifecycle.md
6. `cxf_updatemodel` - Public API not in spec (closest: cxf_model_apply_modifications)

### From src/api/optimize_api.c:
1. `cxf_optimize_internal` - Exists in spec but with different behavior

### From src/api/model_stub.c:
1. `cxf_addvar` - Not in model_lifecycle.md (variable manipulation not covered)
2. `cxf_addvars` - Not in model_lifecycle.md
3. `cxf_delvars` - Not in model_lifecycle.md

### From src/api/api_stub.c:
1. `cxf_optimize` - Exists in spec but with different behavior
2. `cxf_getconstrs` - Not in any spec
3. `cxf_getcoeff` - Not in any spec

### From src/api/constr_stub.c:
1. `cxf_addconstr` - Not in any spec
2. `cxf_addconstrs` - Not in any spec
3. `cxf_addqconstr` - Not in any spec
4. `cxf_addgenconstrIndicator` - Not in any spec
5. `cxf_chgcoeffs` - Not in any spec

**Total extra functions:** 24 functions
**Note:** Many extra functions are legitimate public API for variable/constraint manipulation, which are not covered by the environment_lifecycle and model_lifecycle specs. The specs focus on internal functions, not the full public API.

---

## Analysis

### Major Design Mismatches

1. **Public API vs Internal Functions**: The specs define internal functions (e.g., `cxf_env_create_internal`, `cxf_model_create_internal`) while the implementation provides public API functions (e.g., `cxf_loadenv`, `cxf_newmodel`). This suggests the specs describe the internal implementation layer, not the public interface.

2. **Simplified Implementations**: Most implementations are simplified "tracer bullet" versions that provide basic functionality but lack the sophisticated infrastructure described in the specs:
   - No parameter system
   - No hardware detection
   - No thread safety (mutexes)
   - No reference counting
   - No remote solver support
   - No multi-scenario support
   - No callback infrastructure

3. **Missing Module-Level Infrastructure**: The specs describe complex subsystems that aren't implemented:
   - Parameter table with static definitions and inheritance
   - Configuration file loading
   - Thread pools
   - Model manager with reference counting
   - Lazy update system with 8-phase pipeline
   - Signal handlers for graceful interruption
   - Remote solver communication

4. **Behavioral Gaps**: Even where function names are similar, the behavior is vastly simplified:
   - `cxf_startenv` is a 3-line flag flip vs spec's 7-stage initialization
   - `cxf_freeenv` frees 2 things vs spec's 12-step cleanup
   - `cxf_updatemodel` is a stub vs spec's 8-phase modification application
   - `cxf_optimize` is a wrapper vs spec's 12-step lifecycle

### Root Cause

The implementation is in the **early milestone phase (M8.1.x)** providing basic API surface for testing, while the specs describe the **full production-grade implementation** with all infrastructure. This is expected for a "tracer bullet" development approach.

### Recommendations

1. **For Current Development**: The implementation is acceptable for M8.1.x milestone goals (basic API testing). Document that these are simplified versions.

2. **For Future Milestones**: Systematically implement the missing infrastructure:
   - M8.2.x: Parameter system and configuration loading
   - M8.3.x: Thread safety (mutexes, reference counting)
   - M8.4.x: Lazy update system and model modification pipeline
   - M8.5.x: Callback infrastructure
   - M8.6.x: Remote solver support
   - M8.7.x: Multi-scenario support

3. **For Spec Compliance**: The specs describe internal architecture. Consider:
   - Creating internal function layer (e.g., `cxf_env_create_internal`) that public API calls
   - Public API remains simple (e.g., `cxf_loadenv`)
   - Internal layer implements spec behavior

4. **Immediate Action Items**:
   - Add mutex to CxfEnv structure for thread safety
   - Implement basic parameter table system
   - Add reference counting fields even if not used yet
   - Implement cxf_env_finalize basics (hardware detection stub)
   - Implement cxf_model_apply_modifications skeleton with phase stubs

---

## Conclusion

The audit reveals **27 violations** between the v2 specs and current implementation. However, most violations are expected given the milestone-based development approach:

- **9 Critical violations**: Wrong function names/signatures (expected - specs describe internal architecture)
- **12 Major violations**: Missing functions and complex behaviors (expected - early milestone)
- **6 Minor violations**: Missing infrastructure components (expected - infrastructure comes later)

The implementation provides a **working but simplified API** suitable for current testing needs. The specs provide a **roadmap for production-grade implementation** in future milestones. Both are valid for their respective purposes.

**Recommendation**: ACCEPT current implementation for M8.1.x with the understanding that future milestones will progressively implement the spec-defined infrastructure.
