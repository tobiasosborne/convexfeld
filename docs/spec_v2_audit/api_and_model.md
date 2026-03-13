# Spec V2 Audit: API & Model

## Files Reviewed

### Spec Files
- docs/specs-v2/specs/modules/environment_lifecycle.md
- docs/specs-v2/specs/modules/model_lifecycle.md
- docs/specs-v2/specs/modules/solve_entry.md
- docs/specs-v2/specs/modules/optimization_preparation.md
- docs/specs-v2/specs/modules/solution_processing.md
- docs/specs-v2/specs/modules/query_utilities.md
- docs/specs-v2/specs/integration/parameter_system.md
- docs/specs-v2/specs/reference/parameters_defaults.md

### Implementation Files
- src/api/env.c
- src/api/model.c
- src/api/model_copy.c
- src/api/model_stub.c
- src/api/optimize_api.c
- src/api/io_api.c
- src/api/params_api.c
- src/api/attrs_api.c
- src/api/api_stub.c
- src/api/quadratic_api.c
- src/api/constr_stub.c
- src/parameters/params.c
- src/solver_state/extract.c
- src/simplex/post.c
- include/convexfeld/cxf_env.h
- include/convexfeld/cxf_model.h
- include/convexfeld/convexfeld.h

---

## Compliant Functions

### cxf_checkmodel (model.c:172)
- Validates NULL and magic sentinel. Matches spec structural validation pattern.

### cxf_extract_solution (extract.c:37)
- Correctly copies primal (work_x), dual (work_pi), and objective from SolverState to Model.
- Correctly does NOT set status (comment at line 87 notes this is caller's responsibility).

### cxf_simplex_post_iterate (post.c:37)
- Stall detection, iteration limit check, objective stagnation tracking all present.
- Uses adaptive eta_limit matching refactor.c's threshold.

### cxf_simplex_phase_end (post.c:112)
- Free variable dual feasibility check, inactive constraint removal, small-contribution variable scan all implemented.

---

## VIOLATIONS

### [V1] cxf_env_create_internal / cxf_loadenv / cxf_emptyenv -- Grossly simplified environment creation
- **Spec says:** Allocate a zeroed Environment with validation sentinels (primary and secondary), ISV parameter storage, circular linked list for tracking, batch size limit, process-level global state initialization, mutex allocation, root environment self-reference, error message buffer allocation, system info queries (CPU, platform, hostname, distribution), parameter table construction from static definition table with name registration in uppercase lookup structure, parent inheritance, ISV protection, and secondary/final initialization phases. 17-step behavioral description.
- **Code does:** Simple calloc + field initialization. No mutex, no system info queries, no parameter table from static definitions, no name-based lookup structure, no ISV handling, no parent inheritance, no secondary sentinel, no batch size limit, no process-level global state. Only sets magic, active flag, tolerances, verbosity, output_flag, terminate flags, refactor params, ref_count, and session tracking.
- **File:** src/api/env.c:75-93 (cxf_loadenv), src/api/env.c:95-113 (cxf_emptyenv)
- **Severity:** CRITICAL -- Environment creation is a skeleton.

### [V2] cxf_env_finalize / cxf_startenv -- Trivial activation instead of 8-stage finalization
- **Spec says:** 8-stage finalization process: snapshot/restore for atomic rollback, hardware capability detection (SIMD check), CPU core count detection, environment variable overrides, 4 subsystem initialization phases, license file discovery and parsing, license acquisition through priority-ordered backend chain, post-license validation, thread pool initialization, log file opening. Transitions INACTIVE -> ACTIVE.
- **Code does:** NULL check, magic check, sets `active = 1`. Three lines of logic.
- **File:** src/api/env.c:115-128
- **Severity:** CRITICAL -- No licensing, no hardware detection, no subsystem init, no config file loading.

### [V3] cxf_env_load_logfile -- Missing entirely
- **Spec says:** Initialize or reconfigure logging subsystem. Open log file in append mode, write version/timestamp header, handle null/empty filename to disable logging, handle pre-finalization case by storing filename for later.
- **Code does:** Function does not exist anywhere in codebase.
- **File:** N/A
- **Severity:** MAJOR -- No log file support.

### [V4] cxf_env_update_active_model -- Missing entirely
- **Spec says:** Tear down a ModelManager structure by freeing all tracked model pointers, releasing mutex, deallocating manager. Sets caller's pointer to null.
- **Code does:** Function does not exist. No ModelManager structure exists.
- **File:** N/A
- **Severity:** MAJOR -- No model manager / model tracking infrastructure.

### [V5] cxf_env_free_internal / cxf_freeenv -- Grossly simplified destruction
- **Spec says:** Comprehensive 14-step destruction: terminate remote solver sessions, recursive child environment cleanup with reference counting and deferred free, model cleanup via iteration through model entries, license cleanup, free ~35 string fields, parameter string arrays, WLS credential buffers, thread pool/async cleanup, parameter flags, callback state, mutex destruction, sentinel invalidation, log file close, final deallocation respecting root/child distinction.
- **Code does:** Free callback_state, set active=0, clear magic, cxf_free(env). Does NOT: free child environments, free models, release license resources, destroy mutexes, free parameter tables, free string fields, close log file, handle reference counting.
- **File:** src/api/env.c:130-151
- **Severity:** CRITICAL -- No child env cleanup, no model cleanup, no reference counting.

### [V6] cxf_model_create_internal / cxf_newmodel -- Simplified model creation
- **Spec says:** Allocate Model with validity sentinel, primary model self-reference, modification control flags cleared, optional child environment creation with environment_owned flag, internal data storage allocation, initial model setup (attribute table registration, internal state configuration), fingerprint/seed initialization to predefined constant.
- **Code does:** Allocates Model with sentinel, self-reference, basic fields. But: no child environment creation support (create_child_environment parameter missing from signature), no attribute table registration, no environment_owned flag, no internal data storage as separate allocation, fingerprint initialized to 0 instead of predefined constant.
- **File:** src/api/model.c:73-139
- **Severity:** MAJOR -- Signature mismatch (no child env support), no attribute table.

### [V7] cxf_model_apply_modifications / cxf_updatemodel -- Stub returns NOT_SUPPORTED
- **Spec says:** Complex 8-phase pipeline: SOS validation, warm-start validation, modification counting/classification, fast path for name-only changes, warm-start invalidation, cache invalidation, basis transfer, matrix attribute updates (sense flipping, objective sense change, per-variable/per-constraint updates, SOS/quadratic/general constraint processing), variable type recount, string pool consolidation, name uniqueness validation.
- **Code does:** Validates model, returns CXF_ERROR_NOT_SUPPORTED.
- **File:** src/api/model_copy.c:79-90
- **Severity:** CRITICAL -- Lazy update pattern completely unimplemented.

### [V8] cxf_env_model_cleanup -- Missing entirely
- **Spec says:** Clean up all child environments associated with a parent: reference counting, deferred frees, remote job termination, child array freeing.
- **Code does:** Function does not exist.
- **File:** N/A
- **Severity:** MAJOR -- No child environment cleanup.

### [V9] cxf_update_model_manager -- Missing entirely
- **Spec says:** Clear all model pointer entries from model management structure, free storage.
- **Code does:** Function does not exist.
- **File:** N/A
- **Severity:** MAJOR -- No model management structure.

### [V10] cxf_optimize -- Missing most of the 12-step lifecycle
- **Spec says:** 12-step lifecycle: (1) model validation with OOM diagnostic, (2) signal handler setup via cxf_prepare_optimization, (3) locale safety via cxf_acquire_solve_lock, (4) state init (optimization-active flag, clear message buffers, set modification-blocked, clear status), (5) version/hardware logging on first call, (6) remote solver delegation, (7) log callback registration, (8) cxf_optimize_internal delegation, (9) license/callback cleanup, (10) lifecycle hooks (pre/post-optimize error buffer lock), (11) result file writing, (12) cleanup (clear modification-blocked, release locale, reset optimization-active).
- **Code does:** NULL check, delegates to cxf_optimize_internal. No locale safety, no signal handler, no modification-blocked management, no version/hardware logging, no result file writing, no lifecycle hooks, no remote solver delegation, no status clearing.
- **File:** src/api/api_stub.c:26-33
- **Severity:** CRITICAL -- Public API entry point is a trivial wrapper.

### [V11] cxf_optimize_internal -- Missing most phases
- **Spec says:** 10-phase pipeline: (1) execution path selection (no-callbacks, with-callbacks, normal), (2) model update timing check, (3) model modification flush, (4) concurrent environment parameter management (cache/clamp/restore), (5) solver focus and model type detection, (6) solution clearing for MIP+quadratic, (7) model analysis (dimensions, fingerprint, presolve stats, coefficient ranges), (8) solver dispatch to cxf_solve_entry or cxf_solver_dispatch, (9) non-convex QP handling, (10) state restoration and cleanup.
- **Code does:** Model validation, env check, logging, set self_ptr, reset termination, set optimizing=1, pre-optimization callback, call cxf_solve_lp directly, post-optimization callback, log result, clear optimizing. Missing: execution path selection, model modification flush, concurrent parameter management, model type detection, fingerprint, coefficient analysis, non-convex QP handling, parameter backup/restore.
- **File:** src/api/optimize_api.c:45-105
- **Severity:** CRITICAL -- No lazy update flush before solve, no parameter backup/restore, no method selection.

### [V12] cxf_solve_entry -- Missing entirely
- **Spec says:** Solve chain entry point handling model modification flush, model type detection, solver focus, non-convex QP handling, label validation, routing to single-model or multi-scenario dispatch.
- **Code does:** Function does not exist.
- **File:** N/A
- **Severity:** MAJOR -- No solve chain entry point, no multi-scenario routing.

### [V13] cxf_solve_dispatch -- Missing entirely
- **Spec says:** Multi-scenario optimization dispatcher: validates multi-obj/multi-scenario compatibility, creates scenario model clone, delegates to cxf_solve_entry, copies solution data back.
- **Code does:** Function does not exist.
- **File:** N/A
- **Severity:** MAJOR -- No multi-scenario support.

### [V14] cxf_solve_no_callbacks -- Missing entirely
- **Spec says:** Non-callback optimization path: state tracker allocation with cached attribute indices, worker thread spawn, polling loop for completion.
- **Code does:** Function does not exist.
- **File:** N/A
- **Severity:** MAJOR -- No async optimization support.

### [V15] cxf_solve_with_callbacks -- Missing entirely
- **Spec says:** Callback-enabled optimization path: callback lock acquisition, model state validation, variable name validation, channel setup, sync/async dispatch, result processing with error recovery.
- **Code does:** Function does not exist.
- **File:** N/A
- **Severity:** MAJOR -- No callback communication channel.

### [V16] cxf_prepare_optimization -- Missing entirely
- **Spec says:** Install OS signal handler (SIGINT) for graceful interruption. Acquire solve lock, check silent mode, store model reference in module-level variable, set interrupt-enabled flag, save previous handler.
- **Code does:** Function does not exist.
- **File:** N/A
- **Severity:** MODERATE -- No signal handler installation for Ctrl+C.

### [V17] cxf_wait_async -- Missing entirely
- **Spec says:** Serialize optimization results to remote solver communication channel. 7-phase pipeline: model state preparation, result attribute retrieval, result descriptor table, channel acquisition, header serialization, entry serialization in network byte order, flush and cleanup.
- **Code does:** Function does not exist.
- **File:** N/A
- **Severity:** LOW (remote solver feature) -- No compute server result serialization.

### [V18] cxf_process_lp_solution -- Missing entirely
- **Spec says:** Bind LP result attributes to storage via direct-pointer wiring pattern. Wire iteration counts, node counts, solution count, objective value (conditionally based on status). Handle infeasibility diagnostic objective computation.
- **Code does:** Function does not exist. Solution extraction is done via cxf_extract_solution which copies arrays rather than wiring attribute pointers.
- **File:** N/A
- **Severity:** MAJOR -- No attribute wiring system for LP results.

### [V19] cxf_wire_result_attributes -- Missing entirely
- **Spec says:** Connect MIP/general result attributes to storage. Wire iteration counts, node counts, solution arrays (X, Slack, QCSlack), objective attributes with mode-dependent dispatch (optimal/limited, infeasible/unbounded, general MIP).
- **Code does:** Function does not exist.
- **File:** N/A
- **Severity:** MAJOR -- No MIP result attribute wiring.

### [V20] cxf_uncrush_solution -- Missing entirely
- **Spec says:** Reverse presolve transformations on solution vector. Direct mode vs partial copy mode. Delegate to core uncrushing helper.
- **Code does:** Function does not exist.
- **File:** N/A
- **Severity:** MODERATE -- No presolve reversal (presolve itself is also not implemented).

### [V21] cxf_scale_objval -- Missing entirely
- **Spec says:** Evaluate full objective function: linear terms, quadratic terms, PWL terms, column scaling, global objective unscaling, objective constant. 7-stage computation.
- **Code does:** Function does not exist. Objective is computed inline in the simplex solver.
- **File:** N/A
- **Severity:** MODERATE -- No standalone objective evaluation function.

### [V22] cxf_compute_gap -- Missing entirely
- **Spec says:** Compute relative MIP optimality gap with 5-case priority handling: infinite bound, infinite obj/bound, absolute tolerance check, denominator too small, normal computation.
- **Code does:** Function does not exist.
- **File:** N/A
- **Severity:** LOW (MIP feature) -- No MIP gap computation.

### [V23] cxf_copy_solution -- Missing entirely
- **Spec says:** MIP solution pool management: 11-phase pipeline for sorted insertion, duplicate detection, capacity growth, size limit enforcement, gap-based pruning, integer solution tracking.
- **Code does:** Function does not exist.
- **File:** N/A
- **Severity:** LOW (MIP feature) -- No solution pool.

### [V24] cxf_get_genconstr_name -- Missing entirely
- **Spec says:** Map general constraint type index to name string from static table of ~20 types.
- **Code does:** Function does not exist.
- **File:** N/A
- **Severity:** LOW -- No general constraint name lookup.

### [V25] cxf_get_qconstr_data -- Missing entirely
- **Spec says:** Retrieve sparse quadratic constraint data with lazy caching.
- **Code does:** Function does not exist.
- **File:** N/A
- **Severity:** LOW -- No quadratic constraint data retrieval.

### [V26] cxf_count_genconstr_types -- Missing entirely
- **Spec says:** Count general constraints by type, separating NL vs standard groups.
- **Code does:** Function does not exist.
- **File:** N/A
- **Severity:** LOW -- No general constraint type counting.

### [V27] cxf_has_history -- Missing entirely
- **Spec says:** Check model for valid optimization history (5 sequential null/validity checks).
- **Code does:** Function does not exist.
- **File:** N/A
- **Severity:** LOW -- No optimization history tracking.

### [V28] cxf_fix_variable (query_utilities spec version) -- Signature/scope mismatch
- **Spec says:** Record variable-fixing via eta vector in solver state. Takes (Environment, SolverState, variable_index, fixed_value, fixing_mode). Creates eta vector with header, affected constraint indices, running offsets, fill-in variable indices, coefficient ratios. Uses memory pool allocation.
- **Code does:** cxf_fix_variable in cxf_model.h:178 takes (CxfModel, int var_index, double value) -- a model-level API, not a solver-state-level operation. The spec describes a solver-internal function operating on SolverState, not a public API function.
- **File:** include/convexfeld/cxf_model.h:178
- **Severity:** MODERATE -- Different abstraction level; spec describes solver-internal eta vector creation.

### [V29] Parameter system -- Hardcoded instead of table-driven
- **Spec says:** Parameters stored in a parameter table built from static definition table with name, type, default, min, max, flags. Name-based lookup via uppercase registration. Supports int, double, and string types. Full catalog of ~150+ parameters across 15 categories. Case-insensitive lookup. Layered precedence (defaults -> license file -> config file -> programmatic). Parameter backup/restore around solve (~30 params).
- **Code does:** params_api.c: Hardcoded if/else chain for 4 int parameters (OutputFlag, Verbosity, RefactorInterval, MaxEtaCount). Case-sensitive matching. params.c: Hardcoded if/else chain for 3 double parameters (FeasibilityTol, OptimalityTol, Infinity) with case-insensitive matching. No parameter table, no static definition table, no min/max validation from table, no string parameters, no backup/restore.
- **File:** src/api/params_api.c, src/parameters/params.c
- **Severity:** CRITICAL -- Only 7 of 150+ parameters implemented. No table-driven architecture.

### [V30] Parameter API inconsistency -- int params case-sensitive, double params case-insensitive
- **Spec says:** Parameter names converted to uppercase and registered in lookup structure (implying case-insensitive matching throughout).
- **Code does:** cxf_setintparam/cxf_getintparam use strcmp (case-sensitive). cxf_getdblparam uses strcasecmp_local (case-insensitive). No cxf_setdblparam function exists.
- **File:** src/api/params_api.c (strcmp), src/parameters/params.c (strcasecmp_local)
- **Severity:** MODERATE -- Inconsistent case handling; missing cxf_setdblparam.

### [V31] Missing parameter: Method (-1 to 5)
- **Spec says:** Method parameter controls root LP algorithm. -1=auto, 0=primal simplex, 1=dual simplex, 2=barrier, 3=concurrent, 4=deterministic concurrent, 5=PDHG.
- **Code does:** No Method parameter. optimize_api.c always calls cxf_solve_lp directly.
- **File:** src/api/optimize_api.c:87
- **Severity:** MAJOR -- No algorithm selection.

### [V32] Missing parameter: IterationLimit
- **Spec says:** IterationLimit (double, default Infinity, min 0, max Infinity).
- **Code does:** Iteration limit exists on SolverState (max_iterations), but no API to set it via parameter name. Hardcoded in simplex init.
- **File:** N/A
- **Severity:** MODERATE -- Limit exists internally but not API-configurable by name.

### [V33] Missing parameter: TimeLimit
- **Spec says:** TimeLimit (double, default Infinity).
- **Code does:** No TimeLimit parameter support.
- **File:** N/A
- **Severity:** MODERATE -- No time limit support.

### [V34] Missing parameters: ~140 additional parameters
- **Spec says:** Full catalog of ~150 parameters across categories: termination (13), tolerances (15), simplex (11), barrier (9), MIP strategy (7+), MIP node management (6), MIP heuristics (9), solution pool (5), MIP cuts (24), presolve (14), scaling (3), output/logging (9), threading (8), compute server (12+), cluster manager (5), token server (2), cloud (4), WLS (8), tuning (15), multi-objective (4), function constraints (6), miscellaneous (7).
- **Code does:** 7 parameters total: OutputFlag, Verbosity, RefactorInterval, MaxEtaCount, FeasibilityTol, OptimalityTol, Infinity.
- **File:** src/api/params_api.c, src/parameters/params.c
- **Severity:** CRITICAL -- 95% of parameters missing.

### [V35] Attribute system -- Direct field access instead of attribute table with wiring
- **Spec says:** Attribute table with hash map lookup, entries array, direct-value pointer wiring pattern. Attributes looked up by name, entry found, direct-value pointer set to target storage. Supports scalar and array attributes with size pointers.
- **Code does:** attrs_api.c uses strcmp if/else chain to return model fields directly. No attribute table, no hash map, no wiring, no array attribute support.
- **File:** src/api/attrs_api.c
- **Severity:** MAJOR -- No attribute table infrastructure.

### [V36] Missing attributes: Many result attributes
- **Spec says:** Attributes include: IterCount, BarIterCount, FDIterCount, NodeCount, OpenNodeCount, SolCount, X (array), Slack (array), QCSlack (array), MIPGap, PoolObjBound, PoolObjVal, numerous model-level attributes.
- **Code does:** Only Status, NumVars, NumConstrs, ModelSense, IsMIP (int); ObjVal, Runtime, ObjBound, ObjBoundC, MaxCoeff, MinCoeff (double). No iteration count, no array attributes (X, Slack), no MIP attributes.
- **File:** src/api/attrs_api.c
- **Severity:** MAJOR -- Most result attributes missing.

### [V37] cxf_freemodel -- Does not match spec destruction sequence
- **Spec says:** (Implicit from cxf_env_free_internal) Model cleanup involves freeing model pointers tracked by model manager. Model itself should have sentinel invalidation.
- **Code does:** Frees arrays and sets magic=0, which is correct for sentinel invalidation. But does not handle: child environment cleanup if model owns one (env_flag/environment_owned), does not unregister from any model manager, does not handle solution pool cleanup, does not handle presolve data cleanup.
- **File:** src/api/model.c:141-170
- **Severity:** MODERATE -- Simplified cleanup, no model manager unregistration.

### [V38] cxf_copymodel -- Simplified copy, skips matrix and pending buffer
- **Spec says:** (Implicit) A model copy should include all state for the model to be independently usable.
- **Code does:** Comment at line 72-74: "Skipping complex pending buffer, matrix, and callback handling for now." Copies only variable arrays, dimensions, status. The copied model has no usable constraint matrix.
- **File:** src/api/model_copy.c:14-77
- **Severity:** MAJOR -- Copy produces unusable model (no constraints).

### [V39] cxf_optimize does not set modification_blocked flag
- **Spec says:** Step 4 of cxf_optimize: "sets the model's modification-blocked flag to prevent concurrent modifications."
- **Code does:** cxf_optimize in api_stub.c just delegates to cxf_optimize_internal. cxf_optimize_internal does not set modification_blocked. The flag is only checked in cxf_addconstr/cxf_addconstrs/cxf_chgcoeffs but never set by the optimize path.
- **File:** src/api/api_stub.c:26-33, src/api/optimize_api.c:45-105
- **Severity:** MAJOR -- No protection against concurrent model modification during optimization.

### [V40] cxf_optimize does not clear model status before optimization
- **Spec says:** Step 4: "clears the model's status code."
- **Code does:** Does not clear model->status before calling cxf_solve_lp. Stale status from previous solve may leak.
- **File:** src/api/optimize_api.c:45-105
- **Severity:** MODERATE -- Stale status possible on re-optimize.

### [V41] cxf_optimize does not acquire/release locale safety
- **Spec says:** Step 3: "acquires the locale safety state, saving the calling thread's locale and switching to the standard 'C' locale."
- **Code does:** No locale management at all.
- **File:** src/api/api_stub.c:26-33
- **Severity:** MODERATE -- Decimal point formatting may vary by locale.

### [V42] cxf_optimize converts CXF_OPTIMAL to CXF_OK
- **Spec says:** Return zero on success. The model's status code reflects the optimization outcome.
- **Code does:** optimize_api.c:96 converts CXF_OPTIMAL status to CXF_OK for API return. This is correct for the return value. However, if cxf_solve_lp returns CXF_ITERATION_LIMIT or other non-zero status, that is returned as-is, which may confuse callers expecting only error codes (not solver status codes) from the return value.
- **File:** src/api/optimize_api.c:93-99
- **Severity:** MINOR -- Return value semantics may mix error codes with solver status codes.

### [V43] Environment struct missing fields for spec compliance
- **Spec says:** Environment needs: parameter table (entry array + flags array), name lookup structure, child environment array, model manager/model tracking, license data, mutex (critical section), system info strings (CPU, platform, hostname), log file handle, log filename, activation state (INACTIVE/INITIALIZING/ACTIVE), ISV parameter storage, thread pool, memory pools, WLS credentials, numerous server address strings.
- **Code does:** CxfEnv struct has: magic, active, error_code, error_buffer, tolerances (3), logging (2), terminate flags (2), refactor params (3), ref_count, version, session tracking (2), state flags (3), log callback (2), callback_state, master_env.
- **File:** include/convexfeld/cxf_env.h:20-64
- **Severity:** CRITICAL -- Most env fields specified by spec are absent.

### [V44] Model struct missing fields for spec compliance
- **Spec says:** Model needs: internal data storage (separate allocation holding matrix data, solution data, basis info, working arrays), attribute table, environment_owned flag, pending modifications buffer (structured, not just void*), warm-start data (primal/dual/basis start), quadratic constraint storage, general constraint storage, SOS data, solution pool, history structure, scenario count, concurrent environments, fingerprint/seed initialized to predefined constant, work counter, name update counter, string pool.
- **Code does:** CxfModel struct has basic fields but: no attribute table, no environment_owned flag, no structured pending buffer (just void*), no warm-start data fields, no scenario count, no work counter, no name update counter, fingerprint initialized to 0 not predefined constant.
- **File:** include/convexfeld/cxf_model.h:20-65
- **Severity:** MAJOR -- Many model fields missing.

---

## Missing Functions

### From environment_lifecycle.md:
| Function | Status |
|----------|--------|
| cxf_env_create_internal | **Simplified** as cxf_loadenv/cxf_emptyenv (see V1) |
| cxf_env_finalize | **Simplified** as cxf_startenv (see V2) |
| cxf_env_load_logfile | **MISSING** (see V3) |
| cxf_env_update_active_model | **MISSING** (see V4) |
| cxf_env_free_internal | **Simplified** as cxf_freeenv (see V5) |

### From model_lifecycle.md:
| Function | Status |
|----------|--------|
| cxf_model_create_internal | **Simplified** as cxf_newmodel (see V6) |
| cxf_model_apply_modifications | **STUB** returns NOT_SUPPORTED (see V7) |
| cxf_env_model_cleanup | **MISSING** (see V8) |
| cxf_update_model_manager | **MISSING** (see V9) |

### From solve_entry.md:
| Function | Status |
|----------|--------|
| cxf_optimize | **Skeleton** (see V10) |
| cxf_optimize_internal | **Partial** (see V11) |
| cxf_solve_entry | **MISSING** (see V12) |
| cxf_solve_dispatch | **MISSING** (see V13) |
| cxf_solve_no_callbacks | **MISSING** (see V14) |
| cxf_solve_with_callbacks | **MISSING** (see V15) |

### From optimization_preparation.md:
| Function | Status |
|----------|--------|
| cxf_prepare_optimization | **MISSING** (see V16) |
| (compute server delegate) | **MISSING** (unnamed in spec) |
| cxf_wait_async | **MISSING** (see V17) |

### From solution_processing.md:
| Function | Status |
|----------|--------|
| cxf_process_lp_solution | **MISSING** (see V18) |
| cxf_wire_result_attributes | **MISSING** (see V19) |
| cxf_uncrush_solution | **MISSING** (see V20) |
| cxf_scale_objval | **MISSING** (see V21) |
| cxf_compute_gap | **MISSING** (see V22) |
| cxf_copy_solution | **MISSING** (see V23) |

### From query_utilities.md:
| Function | Status |
|----------|--------|
| cxf_get_genconstr_name | **MISSING** (see V24) |
| cxf_get_qconstr_data | **MISSING** (see V25) |
| cxf_count_genconstr_types | **MISSING** (see V26) |
| cxf_has_history | **MISSING** (see V27) |
| cxf_fix_variable (solver-internal) | **Wrong abstraction** (see V28) |

### From parameter_system.md:
| Component | Status |
|-----------|--------|
| Static definition table | **MISSING** |
| Parameter table construction | **MISSING** |
| Name-based lookup structure | **MISSING** |
| cxf_setdblparam | **MISSING** |
| cxf_setstringparam / cxf_getstringparam | **MISSING** |
| Parameter backup/restore (~30 params) | **MISSING** |
| Concurrent environment parameter clamping | **MISSING** |

---

## Extra Functions (in implementation but not in spec)

| Function | File | Notes |
|----------|------|-------|
| cxf_clearerrormsg | env.c:156 | Utility; reasonable addition |
| cxf_set_callback_context | env.c:168 | Callback management; reasonable |
| cxf_get_callback_context | env.c:185 | Callback management; reasonable |
| cxf_addvar | model_stub.c:87 | Public API building block; not in scoped spec modules |
| cxf_addvars | model_stub.c:152 | Public API building block; not in scoped spec modules |
| cxf_delvars | model_stub.c:204 | Public API stub; not in scoped spec modules |
| cxf_addconstr | constr_stub.c:118 | Public API building block; not in scoped spec modules |
| cxf_addconstrs | constr_stub.c:165 | Public API building block; not in scoped spec modules |
| cxf_chgcoeffs | constr_stub.c:228 | Public API stub; not in scoped spec modules |
| cxf_addqpterms | quadratic_api.c:31 | Public API stub; not in scoped spec modules |
| cxf_addqconstr | quadratic_api.c:97 | Public API stub; not in scoped spec modules |
| cxf_addgenconstrindicator | quadratic_api.c:191 | Public API stub; not in scoped spec modules |
| cxf_read | io_api.c:34 | I/O stub; not in scoped spec modules |
| cxf_write | io_api.c:75 | I/O stub; not in scoped spec modules |
| cxf_getconstrs | api_stub.c:49 | Query stub; not in scoped spec modules |
| cxf_getcoeff | api_stub.c:73 | Query stub; not in scoped spec modules |
| cxf_copymodel | model_copy.c:14 | Not spec'd in audited modules |
| cxf_model_is_blocked | model.c:182 | Utility; reasonable addition |
| cxf_get_feasibility_tol | params.c:82 | Convenience accessor; reasonable |
| cxf_get_optimality_tol | params.c:99 | Convenience accessor; reasonable |
| cxf_get_infinity | params.c:115 | Convenience accessor; reasonable |
| cxf_pre_optimize_hook | (called from optimize_api.c) | Not in spec'd modules |
| cxf_post_optimize_hook | (called from optimize_api.c) | Not in spec'd modules |

---

## Notes

### Overall Assessment
The API layer is in an early/prototype state. Of the ~35 functions specified across the 7 audited spec modules, only 4-5 have implementations that meaningfully match the spec. The remainder are either missing entirely (25+ functions), implemented as stubs returning NOT_SUPPORTED (2), or are grossly simplified skeletons that cover less than 10% of the specified behavior (5-6 functions).

### Highest-Priority Gaps
1. **Parameter system** (V29, V34): Only 7 of ~150 parameters. No table-driven architecture. This blocks almost every other spec feature.
2. **Model modification pipeline** (V7): cxf_updatemodel is a stub. The lazy update pattern is unimplemented.
3. **Optimize lifecycle** (V10, V11): cxf_optimize is a trivial wrapper. No locale safety, no modification blocking, no parameter backup/restore, no method selection, no model modification flush before solve.
4. **Environment lifecycle** (V1, V2, V5): Environment creation/finalization/destruction are skeletons. No licensing, no hardware detection, no parameter table.
5. **Solution processing** (V18-V23): All 6 functions missing. No attribute wiring, no presolve reversal, no gap computation, no solution pool.

### Contextual Note
The project is an LP solver under active development. The core simplex algorithm is functional (18/22 Netlib pass). The API layer appears to have been built as a thin wrapper sufficient for the simplex pipeline to work end-to-end, with many commercial solver features (licensing, compute server, MIP, multi-scenario, solution pool, attribute wiring) deferred. The violations documented here reflect this prioritization rather than overlooked requirements -- the spec describes a full commercial solver's API surface, and the implementation has focused on solver correctness over API completeness.
