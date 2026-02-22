# Module: Solve Entry & Dispatch

## Purpose

The Solve Entry & Dispatch module contains the six functions that form the top-level optimization call chain, from the public API entry point through internal dispatch to the appropriate solver. This module is the gateway through which every optimization request flows: it validates the model, acquires the necessary locale safety state, initializes message buffers and logging, determines whether to use the callback or non-callback execution path, applies pending model modifications, detects the model's problem type (LP, QP), handles multi-scenario routing, and ultimately delegates to the solver algorithm modules (P3.25 Solve LP Core, P3.26 Solve Barrier & Concurrent, P3.27 [out of scope: MIP]) for actual computation.

The six functions form a strict call chain:

```
User Code
    |
    v
cxf_optimize (public API)
    |
    v
cxf_optimize_internal (internal dispatch)
    |
    +--[remote solver / async]--> cxf_solve_no_callbacks or cxf_solve_with_callbacks
    |
    +--[normal path]--> cxf_solve_entry
                            |
                            +--[multi-scenario]--> cxf_solve_dispatch
                            |                           |
                            |                           v
                            |                      cxf_solve_entry (on scenario clone)
                            |
                            +--[single model]--> cxf_solver_dispatch (P3.25)
```

This module does not contain any solver algorithms. Its role is purely organizational: validation, state management, mode detection, and routing. The actual mathematical computation is performed by downstream modules.

## Functions

### cxf_optimize

**Purpose:** Public API entry point for optimization, responsible for model validation, locale safety, solve lock management, version/hardware logging, remote solver delegation, initialization validation, result file writing, and lifecycle callback invocation.

**Signature:**
- Input: `model` : pointer-to-Model - The model to optimize
- Output: int - Zero on success, or an error code on failure

**Preconditions:**
- The model pointer may be arbitrary (the function validates it before use)

**Postconditions:**
- On success (return value zero): The model has been optimized. The model's status code reflects the optimization outcome (optimal, infeasible, unbounded, time limit, iteration limit, etc.). Solution data is available through the attribute system if the solve found a feasible solution. The model's modification-blocked flag has been cleared, re-enabling model modifications. If a result file path was configured and the solve was successful, the result file has been written. The locale has been restored to its pre-call state.
- On failure (nonzero return): The error code identifies the failure reason. The environment's error buffer contains a human-readable error message. The model's state has been cleaned up (modification-blocked flag cleared, locale restored).

**Side Effects:**
- Validates the model via the structural validation check (P3.07)
- Sets up a signal handler for interrupt handling on applicable deployment types
- Acquires the locale safety state (cxf_acquire_solve_lock, P3.11), ensuring the "C" locale for consistent numeric formatting
- Clears the environment's message buffers and resets message state
- Sets the model's modification-blocked flag to prevent concurrent modifications
- Clears the model's status code
- Logs solver version, CPU model, instruction set, and thread count information (on first call per environment, gated by the output verbosity parameter)
- Delegates to remote solver if the model is configured for remote computation
- Registers a log callback relay if a logging path is configured
- Invokes cxf_optimize_internal for the actual optimization
- Validates single-use license restrictions after optimization
- Logs callback invocation statistics (call count and cumulative time) if callbacks were used
- Invokes the pre-optimize and post-optimize lifecycle hooks (P3.13) to manage the error buffer lock
- Writes result files (solution or IIS) if a result file path is configured and the optimization status is optimal or suboptimal
- Clears the modification-blocked flag, releases the locale safety state, and resets the optimization-active flag on the environment

**Error Conditions:**
- Model validation failure (null pointer, invalid sentinel, freed model) -> propagated error code from cxf_checkmodel (P3.07)
- Locale safety acquisition failure (memory allocation) -> out-of-memory error code
- remote solver synchronization failure -> propagated error code
- Internal optimization failure -> propagated error code from cxf_optimize_internal
- Single-use license violation -> error code with diagnostic message
- Out-of-memory at any stage -> out-of-memory error code with message an appropriate out-of-memory error message

**Behavioral Description:**
This function is the sole public entry point for optimization. It wraps the entire optimization lifecycle in a comprehensive setup/teardown sequence.

**Step 1: Model validation.** The function validates the model using the standard structural validation check. If validation fails with an out-of-memory error, a specific diagnostic message is set. For other validation failures, the error is returned directly.

**Step 2: Signal handler setup.** For deployment types that support interrupt handling (local file and web license service), the function registers a signal handler to allow graceful interruption of long-running optimizations via operating system signals.

**Step 3: Locale safety.** The function acquires the locale safety state (P3.11), saving the calling thread's locale and switching to the standard "C" locale. This ensures consistent decimal point formatting throughout the optimization.

**Step 4: State initialization.** The function sets the optimization-active flag on the environment, clears message buffers, sets the model's modification-blocked flag to prevent concurrent modifications, and clears the model's status code.

**Step 5: Version and hardware logging.** On the first optimization call per environment (when the environment has not yet logged hardware information), and when the output verbosity parameter is positive, the function logs the solver version, build information, CPU model, instruction set capabilities, and thread count (physical cores, logical processors, and effective threads). The effective thread count is also validated against available hardware, with a warning if oversubscription is detected (P3.11).

**Step 6: remote solver delegation.** If the model is configured for remote solver operation, the model is synchronized to the remote server and the optimization is delegated to the remote solver subsystem. The local function then waits for the remote result.

**Step 7: Log callback registration.** If a logging path is configured on the environment and no session reference is active, a log callback relay is registered to forward log messages.

**Step 8: Internal optimization.** The function delegates to cxf_optimize_internal for the actual optimization work.

**Step 9: License and callback cleanup.** After optimization, the function validates single-use license restrictions. If callbacks were used and the deployment type permits callback statistics logging, the function logs the total callback invocation count and cumulative time spent in user callbacks.

**Step 10: Lifecycle callbacks.** The pre-optimize callback (error buffer lock) is invoked on the error path; the post-optimize callback (error buffer unlock) is invoked on all paths (P3.13).

**Step 11: Result file writing.** If a result file path is configured and the optimization achieved an optimal or suboptimal status, the function writes the result file. If the file path indicates an IIS file format, the function first computes the Irreducible Inconsistent Subsystem before writing.

**Step 12: Cleanup.** The function performs post-optimization cleanup, clears the session reference, releases the locale safety state, and clears the optimization-active flag.

**Thread Safety:** Conditional. The locale safety mechanism (P3.11) provides per-thread locale isolation. The modification-blocked flag prevents concurrent model modifications. However, the function itself must not be called concurrently on the same model from multiple threads.

**Dependencies:**
- P3.07 (Input Validation) - cxf_checkmodel for model validation
- P3.11 (Threading & Synchronization) - cxf_acquire_solve_lock / cxf_release_solve_lock for locale safety; cxf_get_threads, cxf_get_physical_cores, cxf_get_logical_processors, cxf_set_thread_count for hardware logging
- P3.13 (Callbacks) - cxf_pre_optimize_callback / cxf_post_optimize_callback for error buffer lifecycle
- P3.09 (Error Handling) - cxf_error_model, cxf_set_error_message for error reporting
- P1.01 (Environment) - environment state fields (message buffers, output flag, session reference, callback state)
- P1.02 (Model) - model state fields (modification-blocked, status code, callback count, remote solver flag)

---

### cxf_optimize_internal

**Purpose:** Internal optimization dispatcher that handles thread-local state initialization, callback/non-callback path selection, model modification application, concurrent environment parameter management, model type detection, fingerprint computation, coefficient analysis, and delegation to the solve chain.

**Signature:**
- Input: `model` : pointer-to-Model - The model to optimize (already validated by cxf_optimize)
- Output: int - Zero on success, or an error code on failure

**Preconditions:**
- The model must have been validated and the locale safety state must be active (ensured by cxf_optimize)
- The environment must be in the optimization-active state

**Postconditions:**
- On success: The model has been optimized through one of three paths (no-callbacks, with-callbacks, or normal solve chain). Solution data is available in the model's work arrays. Thread-local timing state has been cleaned up. Concurrent environment parameters have been restored to their pre-optimization values.
- On failure: The error code identifies the failure reason. All saved state (solver focus, fingerprint flag, concurrent environment parameters) has been restored. Thread-local and solve state have been cleaned up.

**Side Effects:**
- Initializes thread-local timing state for performance tracking
- Initializes solve state (timing, counters) for the optimization
- Selects the callback or non-callback execution path based on the model's callback count and async mode
- Checks the model update timing metric against a complexity-based threshold and logs a warning if excessive time was spent in model updates
- Applies pending model modifications to the matrix data (lazy update flush)
- Sets up solver resources (memory pools, internal data structures)
- For concurrent optimization: caches and clamps tolerance parameters on all concurrent environments, restoring them after optimization
- Saves and restores solver focus and fingerprint flags across the optimization
- Sets the model's self-reference pointer for callback access during optimization
- Detects whether the model is a MIP or continuous model; for continuous models with solver focus or NLP mode, marks the model for MIP-like or NLP-like treatment
- Clears existing solution data for MIP models with quadratic terms
- Validates model labels if label checking is enabled
- Logs model dimensions (row count, column count, nonzero count)
- Computes model fingerprint for determinism verification (controlled by fingerprint mode parameter)
- Logs presolve statistics and performs MIP-specific presolve analysis
- Analyzes coefficient ranges and logs warnings for numerically challenging matrices
- Dispatches to the solve chain (cxf_solve_entry or directly to cxf_solver_dispatch depending on problem size)
- Handles non-convex QP detection: when the solver returns a non-positive-semidefinite error for a continuous model, the function may automatically convert it to a MIP formulation based on the non-convex handling parameter
- Checks for asynchronous completion when in remote solver mode
- Cleans up solve state and frees cached parameter arrays

**Error Conditions:**
- Pending model modification application failure -> propagated error code
- Resource setup failure -> propagated error code (typically out-of-memory)
- Memory allocation failure for concurrent parameter cache -> out-of-memory error code
- Label validation failure -> propagated error code
- Coefficient analysis detects fatal numerical issues -> propagated error code
- Solver dispatch failure -> propagated error code
- Non-positive-semidefinite error for non-convex QP with QCP duals requested -> error logged with guidance

**Behavioral Description:**
This function is the central routing point for optimization. It bridges the gap between the public API (cxf_optimize) and the actual solver algorithms.

**Phase 1: Execution path selection.** The function selects among three execution paths based on the model's callback configuration and asynchronous mode:

1. **No-callback fast path:** When the model has no registered callbacks and is in asynchronous mode, the function delegates directly to cxf_solve_no_callbacks, which spawns a worker thread for the optimization. This path bypasses the full model update and analysis pipeline.

2. **Callback path:** When the model has registered callbacks, the function delegates to cxf_solve_with_callbacks, which sets up the callback communication channel and manages synchronization between the solver thread and the callback thread.

3. **Normal path:** For standard synchronous optimization without special callback requirements, the function proceeds through the full analysis and dispatch pipeline described below.

**Phase 2: Model update timing check.** The function computes a complexity estimate based on the model's dimensions (variable count, constraint count, nonzero count, and quadratic term counts) and compares it against the cumulative time spent in model update operations. If the update time exceeds the complexity-scaled threshold, a warning is logged advising the user to call the update function less frequently.

**Phase 3: Model modification flush.** Pending model modifications (variable additions, constraint additions, coefficient changes) that were accumulated in the model's pending buffer are applied to the matrix data. This is the lazy update flush that ensures the matrix data is current before optimization begins.

**Phase 4: Concurrent environment parameter management.** For concurrent optimization (where multiple solver instances run in parallel with different parameter settings), the function caches tolerance parameters from all concurrent environments. The tolerance values are clamped to safe ranges to prevent numerical instability. These cached values are restored after optimization completes, ensuring that the concurrent environments' parameter state is preserved across solves.

**Phase 5: Solver focus and model type detection.** The function saves the solver focus and fingerprint flags, then determines the model type. For continuous models that have the solver focus flag set (indicating the user wants MIP-like treatment of a continuous model) or NLP mode enabled, the model is marked for special treatment.

**Phase 6: Solution clearing for MIP with quadratic terms.** If the model is classified as MIP and contains quadratic terms or integer variables, any existing solution data is cleared to prevent stale results from a previous solve.

**Phase 7: Model analysis and logging.** The function logs model dimensions, computes the model fingerprint (for determinism verification), logs presolve statistics, performs MIP-specific presolve analysis, and analyzes coefficient ranges for numerical warnings.

**Phase 8: Solver dispatch.** The function dispatches to the appropriate solver through either cxf_solve_entry (for models with multi-scenario support) or directly to cxf_solver_dispatch (P3.25) for single-model optimization.

**Phase 9: Non-convex QP handling.** If the solver returns a non-positive-semidefinite error (indicating the quadratic objective or constraints are non-convex) and the model is not already classified as MIP, the function consults the non-convex handling parameter:
- If the parameter indicates automatic MIP conversion (value >= 2), the model is marked as non-convex and re-dispatched as a MIP.
- If the parameter indicates automatic handling with a check for QCP dual requests (value == -1), and QCP duals are not requested, the model is converted to MIP. If QCP duals are requested, an error is logged with guidance.
- Otherwise, the non-positive-semidefinite error is returned as-is.

**Phase 10: State restoration and cleanup.** Solver focus and fingerprint flags are restored. Concurrent environment parameters are restored from their cached values. Thread-local and solve state are cleaned up.

**Thread Safety:** Not thread-safe. Must be called from a single thread per model. Concurrent optimization uses internal threading managed by the solver.

**Dependencies:**
- P3.07 (Input Validation) - cxf_check_label for label validation
- P3.06 (Model Type Checking) - cxf_is_mip_model for MIP detection
- P3.33 (Statistics & Diagnostics) - cxf_presolve_stats, cxf_coefficient_stats for model analysis
- P3.25 (Solve LP Core) - cxf_solver_dispatch for algorithm routing
- P3.28 (Multi-Objective & Scenario) - multi-scenario support
- P3.31 (Model Lifecycle) - cxf_model_apply_modifications for pending change flush; cxf_update_model_manager
- P3.32 (Optimization Preparation) - cxf_setup_resources, cxf_wait_async, cxf_async_check
- P1.01 (Environment) - tolerance parameters, solver focus, fingerprint mode, non-convex handling parameter
- P1.02 (Model) - matrix data, concurrent environments, self-reference, fingerprint

---

### cxf_solve_entry

**Purpose:** Solve chain entry point for a single model that handles model modification flush, model type detection, solver focus configuration, non-convex QP handling, label validation, and routing to either single-model solver dispatch or multi-scenario dispatch.

**Signature:**
- Input: `model` : pointer-to-Model - The model to solve
- Input: `thread_local_data` : pointer-to-ThreadLocalData - Thread-local timing state for performance tracking
- Output: int - Zero on success, or an error code on failure

**Preconditions:**
- The model must have been validated by the calling function (cxf_optimize_internal or cxf_solve_dispatch)
- The model's environment must be in the optimization-active state
- Thread-local data must be initialized

**Postconditions:**
- On success: The model has been optimized. Solution data is available in the model's work arrays. The solver focus and fingerprint flags have been restored to their pre-call values. The solver execution flag on the matrix data has been cleared.
- On failure: The error code identifies the failure. All saved state has been restored.

**Side Effects:**
- Saves and restores solver focus and fingerprint flags on the environment
- Sets the model's self-reference pointer for stable callback access
- Checks the model update timing metric and logs a warning if excessive
- Applies pending model modifications to the matrix data
- Updates the model manager state
- Clears the solver focus flag during optimization (the saved value is used for mode detection)
- For continuous models with solver focus or NLP mode: sets the solver execution flag, disables fingerprinting in NLP mode, and logs the special solve mode
- Clears existing solution data for MIP models with quadratic terms or integer variables
- Validates model labels if label checking is enabled
- For single models: dispatches to cxf_solver_dispatch (P3.25) and handles non-convex QP conversion
- For multi-scenario models: logs model dimensions, computes fingerprint, logs presolve statistics, analyzes coefficient ranges, and dispatches to cxf_solve_dispatch
- Restores the solver execution flag, solver focus, and fingerprint flag

**Error Conditions:**
- Model modification application failure -> propagated error code
- Label validation failure -> propagated error code
- Solver dispatch failure -> propagated error code
- Non-positive-semidefinite error for non-convex QP -> may convert to MIP or return error (see behavioral description)
- Fingerprint computation failure -> propagated error code
- Coefficient analysis failure -> propagated error code

**Behavioral Description:**
This function serves as the decision point between single-model optimization and multi-scenario optimization. It performs many of the same pre-optimization steps as cxf_optimize_internal (model modification flush, type detection, solver focus handling) because it can be called recursively through the multi-scenario path.

**Step 1: State saving.** The solver focus flag and fingerprint flag are saved from the environment for later restoration. The model's self-reference pointer is set.

**Step 2: Update timing check.** The model update timing metric is compared against a complexity-based threshold. If excessive, a warning is logged and the metric is reset.

**Step 3: Model modification flush.** Pending modifications are applied to the matrix data.

**Step 4: Model type detection and mode setup.** The function determines whether the model is a MIP. For continuous models with the solver focus flag or NLP mode enabled, the model is marked for special treatment (MIP-like solve of a continuous model, or NLP solve of a convex model).

**Step 5: Solution clearing.** For MIP models with quadratic terms or integer variables, existing solution data is cleared.

**Step 6: Label validation.** If label checking is enabled on the environment and the model is not in asynchronous mode, label attributes are validated.

**Step 7: Routing decision.** The function examines the model's scenario count:

- **Single model (scenario count < 1):** The function dispatches directly to cxf_solver_dispatch (P3.25) with the model and thread-local data. If the solver returns a non-positive-semidefinite error and the model is continuous, the non-convex handling logic (identical to cxf_optimize_internal Phase 9) is applied, potentially converting the model to MIP and retrying.

- **Multi-scenario model (scenario count >= 1):** The function logs model dimensions, computes the model fingerprint (gated by the fingerprint mode parameter), logs presolve statistics and MIP presolve analysis, analyzes coefficient ranges, and then dispatches to cxf_solve_dispatch for multi-scenario handling.

**Step 8: State restoration.** The model manager state is updated, the solver execution flag is cleared, and the saved solver focus and fingerprint flags are restored.

**Thread Safety:** Not thread-safe. Must be called from a single thread per model.

**Dependencies:**
- P3.06 (Model Type Checking) - cxf_is_mip_model for MIP classification
- P3.07 (Input Validation) - cxf_check_label for label validation
- P3.25 (Solve LP Core) - cxf_solver_dispatch for algorithm routing
- P3.31 (Model Lifecycle) - cxf_model_apply_modifications for modification flush; cxf_update_model_manager
- P3.33 (Statistics & Diagnostics) - cxf_presolve_stats, cxf_coefficient_stats, cxf_compute_fingerprint
- P1.01 (Environment) - solver focus, fingerprint mode, non-convex handling, NLP mode, label check flag
- P1.02 (Model) - matrix data, self-reference, scenario count, fingerprint

---

### cxf_solve_dispatch

**Purpose:** Multi-scenario optimization dispatcher that validates multi-objective/multi-scenario compatibility, creates a scenario model clone, delegates optimization to cxf_solve_entry, and copies all solution data back to the original model.

**Signature:**
- Input: `model` : pointer-to-Model - The original model with multi-scenario configuration
- Input: `thread_local_data` : pointer-to-ThreadLocalData - Thread-local timing state
- Output: int - Zero on success, or an error code on failure

**Preconditions:**
- The model must have a positive scenario count in its matrix data
- The model must have valid matrix data with populated dimensions

**Postconditions:**
- On success: The scenario model has been created, solved, and all solution data (primal values, dual values, range duals, SOS duals, solution pool entries, and cut data) has been copied back to the original model. Result attributes have been wired for fast access through the attribute system.
- On failure: The error code identifies the failure. Partial allocation may have occurred.

**Side Effects:**
- Validates that multi-objective and multi-scenario modes are not combined (returns a specific error if they are)
- Logs the start of multi-scenario optimization with the scenario count
- Creates a scenario model clone with its own environment
- Copies callback configuration (callback count and callback data) to the scenario clone
- Validates labels on the scenario model if label checking is enabled
- Copies environment callback registration from the original environment to the scenario model's environment
- Delegates optimization of the scenario model to cxf_solve_entry
- Allocates work arrays on the original model using the scenario model's work arrays as a template
- Copies solution data arrays from the scenario model to the original model: primal variable values, constraint dual values, range constraint dual values, and SOS dual values
- Copies solution pool data: allocates arrays for pool variable values, pool objective values, and pool objective bounds; copies each pool solution entry
- Copies cut data: allocates arrays for cut variable values and cut objective values; copies each cut entry
- Wires result attributes on the original model for fast attribute access

**Error Conditions:**
- Multi-objective with multi-scenario -> specific incompatibility error code with diagnostic message
- Scenario model setup failure -> propagated error code
- Label validation failure -> propagated error code
- Environment callback copy failure -> propagated error code
- cxf_solve_entry failure on the scenario model -> propagated error code
- Work array allocation failure -> out-of-memory error code
- Any solution data array allocation failure -> out-of-memory error code

**Behavioral Description:**
This function implements the multi-scenario optimization pattern. Multi-scenario optimization allows solving a model under multiple parameter or data variations in a single call, sharing preprocessing and other fixed costs.

**Step 1: Compatibility check.** The function queries whether the model has multi-objective configurations. Multi-scenario optimization combined with multi-objective optimization is not supported; if detected, the function logs an error message and returns the incompatibility error code.

**Step 2: Scenario model creation.** The function creates a scenario model clone using the scenario setup infrastructure. This clone contains all scenario variations and is optimized as a single unit. Callback configuration is copied from the original model to the clone.

**Step 3: Label and callback setup.** If label checking is enabled on the environment, labels are validated on the scenario model. Environment-level callbacks (log callback, callback state) are propagated from the original environment to the scenario model's environment.

**Step 4: Optimization.** The scenario model is optimized by calling cxf_solve_entry, which routes through the normal solve chain.

**Step 5: Solution data copy-back.** After successful optimization, solution data is transferred from the scenario model's work arrays to the original model. This involves allocating matching arrays on the original model and copying:

1. **Primary solution arrays:** Variable values (primal solution), constraint duals (shadow prices), range constraint duals, and SOS constraint duals. The dual arrays are laid out contiguously with constraint duals first, followed by range duals, followed by SOS duals.

2. **Solution pool data:** If the scenario model produced multiple solutions (pool solution count > 0), the function allocates and copies per-solution variable value arrays, objective values, and objective bounds. A secondary pool counter tracks the total across scenarios.

3. **Cut data:** If the scenario model generated cuts (cut count > 0), the function allocates and copies per-cut variable value arrays and objective values.

**Step 6: Result attribute wiring.** The function calls the result attribute wiring function (P3.29) to connect the original model's attributes to their storage locations, enabling fast access through the public attribute API.

**Thread Safety:** Not thread-safe. Must be called from a single thread per model.

**Dependencies:**
- P3.07 (Input Validation) - cxf_check_multiobj_scenario for compatibility check; cxf_check_label for label validation
- P3.13 (Callbacks) - cxf_copy_env_callbacks for callback propagation
- P3.29 (Solution Processing) - cxf_wire_result_attributes for attribute wiring
- P3.01 (Memory Primitives) - cxf_malloc, cxf_calloc for array allocation
- P1.02 (Model) - matrix data dimensions, work arrays, scenario model pointer, callback configuration

---

### cxf_solve_no_callbacks

**Purpose:** Non-callback optimization path that initializes a state tracker for progress monitoring, spawns a worker thread to perform the optimization, and waits for completion.

**Signature:**
- Input: `model` : pointer-to-Model - The model to optimize
- Output: int - Zero on success, or an error code on failure

**Preconditions:**
- The model must have no registered callbacks (callback count is zero)
- The model must be in asynchronous mode
- The model's environment must be valid

**Postconditions:**
- On success: The optimization has been performed on a worker thread. The state tracker structure contains the final optimization status and progress data. The completion flag in the state tracker is set.
- On failure: The error code identifies the failure (typically out-of-memory for state tracker allocation or worker thread spawn failure).

**Side Effects:**
- Initializes the callback data structure on the environment (even though no user callbacks are registered, the infrastructure is initialized for internal progress tracking)
- Frees any existing state tracker structure on the model
- Allocates a new state tracker structure containing cached attribute indices for efficient progress reporting (indices for model dimensions, optimization status, objective value, objective bound, runtime, node count, iteration counts, and barrier iteration count)
- Initializes the state tracker with sentinel values for objective bounds and zero values for all counters
- Spawns a worker thread that invokes the optimization worker function with the model
- In asynchronous mode: enters a polling loop, sleeping between checks, until the optimization completes (the async flag is cleared by the worker thread). After a large number of polling iterations, performs a timeout check.

**Error Conditions:**
- Memory allocation failure for state tracker -> out-of-memory error code
- Worker thread spawn failure -> propagated error code from the thread spawn function

**Behavioral Description:**
This function provides the fast path for optimization when no user callbacks are needed. It is used when the model is configured for asynchronous operation without callbacks, typically for remote solver or background optimization scenarios.

**Step 1: Callback infrastructure initialization.** The callback data structure is initialized on the environment. Even without user callbacks, the internal infrastructure is set up for progress tracking and termination signaling.

**Step 2: State tracker allocation.** Any existing state tracker is freed, and a new one is allocated. The state tracker is a fixed-size structure that caches attribute indices for the model's key progress metrics: constraint count, variable count, SOS count, quadratic constraint count, general constraint count, objective count, scenario count, optimization status, objective value, objective bound, runtime, node count, open node count, iteration count, and barrier iteration count. These indices enable efficient attribute lookups without name-based string searches during progress reporting.

**Step 3: State tracker initialization.** The state tracker is initialized with sentinel values for objective bounds (large positive and negative values indicating no bound has been established), negative values for gap metrics (indicating no gap has been computed), and zero values for all counters and progress fields. The completion flag is cleared to indicate the optimization is in progress.

**Step 4: Worker thread dispatch.** The optimization is dispatched to a worker thread via the thread spawn function. The worker thread executes the optimization pipeline (solver dispatch) and sets the completion flag when finished.

**Step 5: Completion wait.** In asynchronous mode, the function enters a polling loop: it sleeps briefly, then checks whether the async flag has been cleared (indicating the worker thread has completed). After a substantial number of iterations without completion, a timeout check is performed. The polling loop continues until the optimization completes.

**Thread Safety:** Conditional. The function spawns a worker thread and communicates completion through the model's async flag and the state tracker's completion flag. These flags serve as the synchronization mechanism between the main thread and the worker thread.

**Dependencies:**
- P3.13 (Callbacks) - cxf_init_callback_data for infrastructure initialization
- P3.01 (Memory Primitives) - cxf_malloc for state tracker allocation; cxf_free for cleanup
- P3.11 (Threading & Synchronization) - cxf_sleep for polling; cxf_timeout_check for timeout detection
- P1.02 (Model) - state tracker pointer, callback initialization data, async flag

---

### cxf_solve_with_callbacks

**Purpose:** Callback-enabled optimization path that acquires synchronization, validates model state, sets up a callback communication channel, dispatches the solve (synchronously or asynchronously), and processes the callback result including error propagation.

**Signature:**
- Input: `model` : pointer-to-Model - The model to optimize
- Output: int - Zero on success, or an error code on failure

**Preconditions:**
- The model must have registered callbacks or be configured for remote solver / asynchronous operation
- The model's environment must be valid with an active license information structure containing a callback communication channel

**Postconditions:**
- On success: The optimization has been performed (either locally or remotely through the callback channel). Any results from the callback thread have been processed and errors propagated.
- On failure: The error code identifies the failure. The callback communication channel has been unlocked. Any callback result errors have been reported through the environment's error system.

**Side Effects:**
- Acquires a callback synchronization lock (separate from the locale safety lock) for exclusive access to the callback communication channel
- Validates the model state for optimization readiness
- If a variable name validation path is configured on the environment, validates variable name attributes (temporarily clearing the model's modification-blocked flag for attribute access)
- Locks the callback communication channel for message exchange
- Sets up callback state for the optimization session
- Initializes callback communication parameters from configuration tables
- Sends an initialization message through the callback communication channel
- Dispatches the solve: synchronously (calling the solve function directly) or asynchronously (dispatching to a worker thread)
- Unlocks the callback communication channel
- Processes the callback result structure, checking for primary and secondary error codes
- For out-of-memory errors: returns immediately with the out-of-memory code
- For user interrupt: reports the interrupt through the error system and returns the interrupt code
- For other errors: waits for the callback thread to complete (polling with sleep), sends a result request message, waits for the response, and reports the error with the detailed message from the callback thread
- Checks for secondary error codes from the callback result

**Error Conditions:**
- Callback lock acquisition failure -> lock failure error code
- Model state validation failure -> propagated error code
- Variable name validation failure -> propagated error code
- Callback state setup failure -> propagated error code
- Callback initialization message send failure -> propagated error code
- Primary error from callback result -> propagated (out-of-memory, user interrupt, or other error with detailed message)
- Secondary error from callback result -> propagated error code
- Callback error handling failure -> propagated error code

**Behavioral Description:**
This function manages the full callback communication lifecycle for optimization operations that involve remote solver delegation or asynchronous execution.

**Step 1: Callback lock acquisition.** The function acquires a global callback synchronization lock to ensure exclusive access to the callback communication infrastructure. If the lock cannot be acquired, a lock failure error is returned.

**Step 2: Model validation.** The model state is validated for optimization readiness.

**Step 3: Variable name validation.** If the environment has a variable name validation path configured, the function temporarily clears the model's modification-blocked flag, validates variable name attributes, and restores the flag. This ensures that remote solver deployments can verify variable naming consistency.

**Step 4: Channel setup.** The callback communication channel is locked, and callback state is initialized for the optimization session. Callback communication parameters are populated from configuration tables, and the model reference is stored for callback access. An initialization message is sent through the channel.

**Step 5: Solve dispatch.** The function dispatches the actual solve operation. In synchronous mode, the solve function is called directly within the callback context. In asynchronous mode, the solve function is dispatched to a worker thread.

**Step 6: Channel cleanup and result processing.** The callback communication channel is unlocked. The function then examines the callback result structure for errors:

- **No result structure or no primary error:** If no callback result exists and no error occurred during dispatch, the function returns success.
- **Out-of-memory error:** Returns immediately with the out-of-memory code.
- **User interrupt:** Reports the interrupt through the error system and returns the interrupt code.
- **Other errors:** The function enters an error recovery sequence: it waits for the callback thread to complete (polling with sleep and retry on the callback lock), locks the communication channel, sends a result request message to retrieve detailed error information, waits for the response, reports the error with the detailed message, and returns the primary error code.
- **Secondary errors:** If no primary error but a secondary error is present in the callback result, the secondary error code is returned.

**Thread Safety:** Conditional. The function acquires the callback synchronization lock and the callback channel lock to serialize access. The solve dispatch may operate on a separate thread (in async mode), with the callback channel providing the synchronization mechanism.

**Dependencies:**
- P3.07 (Input Validation) - cxf_validate_model_state for model validation; cxf_check_attr_names for variable name validation
- P3.09 (Error Handling) - cxf_error_with_info for detailed error reporting
- P3.11 (Threading & Synchronization) - cxf_sleep for polling during error recovery
- P1.01 (Environment) - license information, callback communication channel, error suppression flag
- P1.02 (Model) - async flag, modification-blocked flag, model identifier
- P1.07 (CallbackState) - callback result structure with primary and secondary error codes

---

## Module-Level Behavioral Notes

### Call Chain Architecture

The six functions in this module form a layered call chain with clear separation of concerns:

| Layer | Function | Primary Responsibility |
|-------|----------|----------------------|
| **Public API** | cxf_optimize | Validation, locale, locking, logging, lifecycle |
| **Internal Dispatch** | cxf_optimize_internal | Path selection, model analysis, parameter management |
| **Solve Chain Entry** | cxf_solve_entry | Type detection, scenario routing, non-convex handling |
| **Scenario Dispatch** | cxf_solve_dispatch | Multi-scenario setup, solve delegation, result copy |
| **Callback Path** | cxf_solve_with_callbacks | Callback channel management, sync/async dispatch |
| **Fast Path** | cxf_solve_no_callbacks | State tracker, worker thread, completion polling |

Each layer adds its specific concerns and delegates downward. No layer skips an intermediate layer, ensuring that all necessary initialization and cleanup occurs in the correct order.

### Three Execution Paths

The optimization system supports three distinct execution paths, selected in cxf_optimize_internal:

1. **Normal synchronous path:** cxf_optimize -> cxf_optimize_internal -> cxf_solve_entry -> cxf_solver_dispatch (P3.25). Used for standard local optimization without callbacks.

2. **Callback path:** cxf_optimize -> cxf_optimize_internal -> cxf_solve_with_callbacks -> [solve function]. Used for remote solver deployments and models with user-registered callbacks. Provides callback communication infrastructure for progress reporting and early termination.

3. **No-callback fast path:** cxf_optimize -> cxf_optimize_internal -> cxf_solve_no_callbacks -> [worker thread]. Used for asynchronous optimization without user callbacks. Spawns a worker thread and monitors progress through a state tracker structure.

### Non-Convex QP Handling

Both cxf_optimize_internal and cxf_solve_entry implement identical non-convex QP handling logic. When the solver returns a non-positive-semidefinite error for a continuous model, the behavior depends on the non-convex handling parameter:

| Parameter Value | Behavior |
|----------------|----------|
| 0 (default) | Return the error as-is |
| -1 (auto) | Convert to MIP unless QCP duals are requested |
| >= 2 (explicit) | Always convert to MIP |

The conversion involves clearing any presolved model, setting the non-convex flag on the matrix data, and retrying the solve through cxf_solver_dispatch with MIP treatment. This behavior follows the standard approach for non-convex quadratic programs described in commercial solver documentation: when a QP is detected as non-convex (the Q matrix is not positive semidefinite), it can be reformulated as a mixed-integer program using spatial branch-and-bound techniques (Belotti et al., "Mixed-Integer Nonlinear Optimization," Acta Numerica, 2013).

### Lazy Update Pattern

Both cxf_optimize_internal and cxf_solve_entry apply pending model modifications before optimization begins. This is the flush step of the lazy update pattern described in the Model data model (P1.02): modifications are accumulated in a pending buffer and applied in batch when optimization is requested. The flush occurs at two points because cxf_solve_entry can be called recursively through the multi-scenario path, and each invocation must ensure its model has current data.

### Model Update Timing Warning

Both cxf_optimize_internal and cxf_solve_entry check whether the model has spent excessive time in update operations relative to the model's complexity. This diagnostic helps users who call the model update function after every individual modification rather than batching modifications and calling update once. The complexity estimate is based on the model's dimensions (variable count, constraint count, nonzero count, and quadratic term counts), and the check fires when the cumulative update time exceeds this estimate.

### Concurrent Environment Parameter Management

cxf_optimize_internal implements a save/clamp/restore pattern for tolerance parameters across concurrent environments. Before optimization, tolerance parameters on each concurrent environment (and the main environment) are cached and clamped to safe ranges. After optimization, the cached values are restored. This ensures that:
1. Concurrent solver instances operate with numerically safe tolerance values
2. The user's original parameter settings are not permanently modified by the optimization

### Relationship to Downstream Modules

| Downstream Module | Entry Point | Purpose |
|------------------|-------------|---------|
| P3.25 (Solve LP Core) | cxf_solver_dispatch | Routes to simplex, barrier, or concurrent solver |
| P3.26 (Solve Barrier & Concurrent) | cxf_solve_barrier, cxf_solve_concurrent | Barrier and concurrent solve algorithms |
| P3.27 (Solve MIP) | cxf_solve_mip | Branch-and-bound solver |
| P3.28 (Multi-Objective & Scenario) | cxf_solve_multiscenario | Multi-scenario optimization |
| P3.29 (Solution Processing) | cxf_wire_result_attributes | Solution data access setup |

### State Tracker Structure (cxf_solve_no_callbacks)

The state tracker allocated by cxf_solve_no_callbacks provides a lightweight mechanism for monitoring optimization progress across the thread boundary. It caches attribute indices (resolved at initialization time) so that the worker thread can update progress fields by direct array access rather than name-based attribute lookup. The tracked metrics include:

- **Model dimensions:** Constraint count, variable count, SOS count, quadratic constraint count, general constraint count, objective count, scenario count
- **Optimization progress:** Status, objective value, objective bound, objective bound (continuous relaxation), runtime, node count, open node count, iteration count, barrier iteration count
- **Completion:** A completion flag set by the worker thread when optimization finishes

### Error Handling Patterns

The module uses several error handling patterns from P3.09:

1. **Early return on validation failure:** cxf_optimize returns immediately if model validation fails, with the error code from the validation function.
2. **Error buffer locking:** cxf_optimize invokes the pre/post-optimize lifecycle hooks (P3.13) to lock the error buffer during optimization, preserving the root-cause error message.
3. **Cascading error propagation:** Errors from internal functions are propagated upward through the call chain. Each level may add its own error context (e.g., an appropriate out-of-memory error message) without overwriting the original error message.
4. **Callback error recovery:** cxf_solve_with_callbacks implements a polling-based error recovery sequence to retrieve detailed error information from a callback thread that encountered an error.

### Return Code Conventions

| Code | Meaning | Functions |
|------|---------|-----------|
| Success (zero) | Operation completed normally | All six functions |
| Out-of-memory code | Memory allocation failed | All six functions |
| Lock failure code | Callback synchronization lock could not be acquired | cxf_solve_with_callbacks |
| User interrupt code | User requested termination via callback or signal | cxf_solve_with_callbacks |
| Non-PSD code | Quadratic objective/constraints not positive semidefinite | cxf_optimize_internal, cxf_solve_entry (may be converted to MIP retry) |
| Multi-scenario incompatibility code | Multi-objective combined with multi-scenario | cxf_solve_dispatch |
| Other error codes | Propagated from downstream modules | All six functions |

### Thread Safety Summary

| Function | Thread Safety | Notes |
|----------|---------------|-------|
| cxf_optimize | Conditional | Per-thread locale isolation; must not be called concurrently on same model |
| cxf_optimize_internal | Not thread-safe | Single-threaded per model |
| cxf_solve_entry | Not thread-safe | Single-threaded per model |
| cxf_solve_dispatch | Not thread-safe | Single-threaded per model |
| cxf_solve_no_callbacks | Conditional | Spawns worker thread; communicates via async flag and state tracker |
| cxf_solve_with_callbacks | Conditional | Acquires callback lock and channel lock for synchronization |

---

## Verification Checklist

```
[x] No hex addresses or offsets
[x] No Ghidra artifacts (param_, local_, iVar, DAT_, FUN_)
[x] No binary-specific constants or magic numbers
[x] No copied code fragments from analyzed source
[x] All descriptions are behavioral, not implementational
[x] All data structures described semantically using Layer 1/2 references
[x] Explicit cross-references to P1.01, P1.02, P1.07 (data model) and P3.01, P3.06, P3.07, P3.09, P3.11, P3.13, P3.25, P3.28, P3.29, P3.31, P3.32, P3.33 (module specs)
[x] Passes the Clean Room Test: could be written without seeing the binary
```

## References

- Belotti, P., Kirches, C., Leyffer, S., Linderoth, J., Luedtke, J., and Mahajan, A. (2013). "Mixed-Integer Nonlinear Optimization." *Acta Numerica*, 22:1-131.
- ConvexFeld Optimization, LLC. *ConvexFeld Optimizer Reference Manual* (public API documentation). cxf_optimize, multi-scenario optimization, NonConvex parameter, callback interface.
- Maros, I. (2003). *Computational Techniques of the Simplex Method*. Springer. International Series in Operations Research and Management Science, Vol. 61. (Solver dispatch architecture and initialization patterns.)
