# Module: State Cleanup - Solver

## Purpose

This module provides the functions responsible for cleaning up solver-related state at the end of optimization or during model destruction. It covers six distinct cleanup operations: post-optimization finalization of timing and callbacks, deallocation of the attribute table, release of concurrent solver environments (with reference counting and remote job termination), deallocation of IIS diagnostic data, deallocation of warm-start basis data, and sending a disconnect message to a remote solver. These functions collectively ensure that all solver-allocated resources are properly released in the correct order to prevent memory leaks, dangling references, and orphaned remote processes.

## Functions

### cxf_cleanup_solve_state

**Purpose:** Finalize solver state after optimization completes, recording timing statistics, resetting interrupt flags, restoring environment state, and invoking any registered post-optimization callbacks.

**Signature:**
- Input: model : pointer-to-Model - The model that was being optimized
- Input: timingData : pointer-to-array-of-double - Two-element timing array where element 0 is total work done and element 1 is the solve start timestamp; may be null if no timing data is available
- Output: void

**Preconditions:**
- The model must be valid with a non-null environment reference.
- The function should be called exactly once at the end of each optimization invocation (paired with cxf_init_solve_state at the beginning).

**Postconditions:**
- Thread-local cleanup has been performed for the current worker thread.
- The environment's termination/interrupt flag has been cleared (reset to zero), ensuring it does not carry over to subsequent API calls.
- The environment's objective offset tolerance has been restored to its saved pre-optimization value (the tolerance adjustment made by cxf_init_solve_state is reversed).
- If the model has active callback infrastructure (used in concurrent/remote solver scenarios), the callback timing subsystem has been notified of the solve completion.
- The model's timing fields have been populated:
  - Solve duration: computed as the difference between the current timestamp and the start timestamp from the timing data. Set to zero if the start timestamp is invalid (negative) or if timing data is null.
  - Work rate: computed as total work divided by a standard scaling constant.
  - Total work done: copied directly from the timing data.
  - All three fields are set to zero if no timing data is provided.
- The environment's finalization callback has been invoked, performing any environment-level post-solve cleanup.

**Side Effects:**
- Invokes the thread cleanup callback (handles thread-local state for parallel/remote solver environments).
- Resets the environment's interrupt flag.
- Swaps the environment's timing buffers (restores the saved objective offset tolerance).
- Writes solve duration, work rate, and total work to model fields.
- Invokes callback timing notification if concurrent/remote solver callbacks are active.
- Invokes the environment finalization callback.

**Error Conditions:**
- None. This function does not return an error code. It handles null timing data gracefully by zeroing the model's timing fields. It validates the model before attempting callback invocation.

**Behavioral Description:**
cxf_cleanup_solve_state is the cleanup counterpart to cxf_init_solve_state, called once at the end of every optimization path (success, failure, or interruption). It performs six actions in sequence: (1) invoke the thread cleanup callback to release thread-local resources, (2) clear the environment's interrupt flag so it does not persist, (3) restore the environment's objective offset tolerance to its pre-solve value by swapping the saved and active tolerance fields, (4) if the model has valid callback infrastructure for remote solver or concurrent optimization, initialize and invoke the callback with the timing data, (5) compute and store timing statistics on the model (duration, work rate, total work), and (6) invoke the environment's finalization callback for any final environment-level cleanup.

**Thread Safety:** unsafe -- Must be called from the same thread that initiated the optimization. Modifies environment-level state without locking.

**Dependencies:**
- cxf_get_timestamp (timing module) -- retrieves the current timestamp for duration calculation
- cxf_thread_cleanup (threading module) -- performs thread-local cleanup
- cxf_validate_model (validation module) -- checks model validity before callback invocation
- cxf_callback_init, cxf_callback_timing (callback module) -- callback notification
- cxf_get_finalize_cb, cxf_invoke_callback (callback module) -- environment finalization callback

---

### cxf_free_solver_state

**Purpose:** Free the model's attribute table structure and its owned entries array, removing the metadata that supports the model's attribute access API.

**Note:** Despite the name suggesting it frees "solver state," this function specifically frees the attribute table structure. The naming reflects an earlier architectural phase where this model field held broader solver state.

**Signature:**
- Input: model : pointer-to-Model - The model whose attribute table should be freed
- Output: void

**Preconditions:**
- The model must be a valid pointer with an accessible environment reference.
- May be called when the attribute table is already null (no-op in that case).

**Postconditions:**
- If the attribute table existed:
  - The entries array within the attribute table has been freed (if it was non-null).
  - The attribute table structure itself has been freed.
  - The model's reference to the attribute table has been set to null.
- If the attribute table did not exist, the function has no effect.
- After this function completes, all attribute queries on the model will fail (the metadata supporting them has been removed).

**Side Effects:**
- Deallocates the attribute entries array and the attribute table structure through the environment's memory management system.
- Nulls the entries pointer within the attribute table after freeing (defensive pattern).
- Nulls the model's attribute table reference after freeing.

**Error Conditions:**
- None. This function does not return an error code. Null attribute table is handled gracefully.

**Behavioral Description:**
cxf_free_solver_state performs a two-level deallocation of the model's attribute table. It first frees the nested entries array (which contains the per-attribute metadata descriptors), nulls the entries pointer defensively, and then frees the attribute table wrapper structure itself. The model's reference to the attribute table is set to null to prevent use-after-free. The function re-reads the attribute table pointer after freeing the entries array as a defensive measure against potential side effects.

**Null Safety:** The function checks for null at every level: if the attribute table pointer is null, it returns immediately; if the entries array is null, it skips to freeing the table structure.

**Deallocation Order:**
1. Attribute entries array (within the attribute table)
2. Attribute table structure itself
3. Model's attribute table reference set to null

**Thread Safety:** unsafe -- Assumes single-threaded access to the model during destruction.

**Dependencies:**
- Memory primitives module (cxf_free) -- for each individual deallocation

---

### cxf_free_basis_state

**Purpose:** Free the array of concurrent solver environments associated with the model, handling reference counting, deferred cleanup for environments that are still in use by other owners, and termination of active remote solver jobs.

**Signature:**
- Input: model : pointer-to-Model - The model whose concurrent environment array should be freed
- Output: void

**Preconditions:**
- The model must be a valid pointer with accessible concurrent environment fields and a primary environment reference.
- The concurrent environments array may be null or empty.

**Postconditions:**
- Every environment in the concurrent environments array has been processed:
  - Its root environment's reference count has been decremented (under lock).
  - If the reference count reached zero, the environment has been fully freed via the internal environment destruction function.
  - If the reference count remained positive (environment still in use by another owner), the environment is handled with deferred cleanup: a warning is logged, any active remote solver job is terminated, and the array slot is cleared.
- The concurrent environments array itself has been freed.
- The model's concurrent environments pointer has been set to null.
- The model's concurrent environment count has been set to zero.

**Side Effects:**
- Decrements reference counts on root environments (under critical section lock for thread safety).
- May fully destroy environments whose reference counts reach zero.
- May log warning messages about deferred environment cleanup.
- May terminate active remote solver jobs by:
  - Setting a termination flag in the async operation state.
  - Polling the remote job status with a bounded retry loop.
  - Sending a termination message to the remote solver.
  - Freeing the remote solver connection resources.
- Deallocates the array through the primary environment's memory management system.

**Error Conditions:**
- None. This function does not return an error code. Empty arrays and null pointers are handled gracefully. Remote job termination failures are logged but do not cause the function to fail.

**Behavioral Description:**
cxf_free_basis_state manages the lifecycle of concurrent solver environments created during concurrent optimization (where multiple solver algorithms run in parallel with different parameter settings). For each environment in the array, it locates the root environment (which may be a shared parent) and decrements its reference count under a critical section lock to ensure thread safety. If the reference count reaches zero, the environment is destroyed immediately. If the count remains positive (another owner still needs the environment), the function performs a deferred cleanup: it logs a diagnostic warning, checks for active remote solver jobs associated with the environment, and if found, attempts graceful termination with a bounded polling loop before forcefully killing the job and freeing the connection. After processing all environments, the array itself is freed and the model's tracking fields are cleared.

**Reference Counting Protocol:**
1. Enter critical section on the root environment's mutex
2. Decrement the root environment's reference count
3. Record whether the count reached zero
4. Leave critical section
5. If count is zero: destroy the environment
6. If count is positive and this environment IS the root: deferred cleanup (warn, kill jobs, clear slot)
7. If count is positive and this environment is NOT the root: destroy the child, then destroy the root if its count separately reached zero

**Remote Job Termination Protocol:**
1. Check for an active remote solver connection with a non-empty job identifier and server address
2. Poll the job status; if already completed, proceed to cleanup
3. Set the termination flag in the async operation state
4. Poll with bounded retries (with sleep between polls) for graceful termination
5. If the job does not terminate within the retry limit, proceed to forced termination
6. Log a warning identifying the job and server
7. Send a termination message to the server
8. Free the connection resources

**Thread Safety:** conditional -- The reference count decrement is protected by a critical section. However, the overall function assumes it is the only caller modifying the concurrent environments array on this model.

**Dependencies:**
- Memory primitives module (cxf_free) -- for array deallocation
- cxf_env_free_internal (environment module) -- for full environment destruction
- EnterCriticalSection, LeaveCriticalSection (threading module) -- for reference count protection
- cxf_log (error/logging module) -- for warning messages

---

### cxf_free_iis_state

**Purpose:** Free the Irreducible Infeasible Subsystem (IIS) diagnostic data structure associated with the model, including all owned arrays and individually allocated constraint name strings.

**Signature:**
- Input: model : pointer-to-Model - The model whose IIS state should be freed; may be null
- Output: void

**Preconditions:**
- The model may be null; if so, the function returns immediately.
- If the model is non-null but has no IIS state, the function returns immediately.

**Postconditions:**
- If IIS state existed:
  - All individually allocated constraint name strings have been freed.
  - The constraint names array has been freed.
  - The variable upper bound IIS membership array has been freed.
  - The constraint IIS membership array has been freed.
  - The variable lower bound IIS membership array has been freed.
  - The IISState structure itself has been freed.
  - The model's reference to the IIS state has been set to null.
- If no IIS state existed, the function has no effect.

**Side Effects:**
- Deallocates memory through the environment's memory management system.
- Nulls each pointer field within the IIS state after freeing (defensive pattern).
- Nulls the model's IIS state reference after freeing.

**Error Conditions:**
- None. This function does not return an error code. Null model and null IIS state are handled gracefully. Individual null name string entries within the names array are skipped.

**Behavioral Description:**
cxf_free_iis_state performs an inside-out deallocation of the IIS diagnostic structure. The most complex part is the constraint names array: because each name string is individually allocated (copied from the model's constraint name storage for ownership independence), the function must iterate through the array and free each non-null string before freeing the array itself. The iteration count is taken from the IIS state's constraint count field. After handling names, the function frees the three parallel membership arrays (variable upper bound IIS, constraint IIS, and variable lower bound IIS) and then frees the IIS state structure itself.

**Null Safety:** Every pointer is checked before freeing. The function handles null model, null IIS state, null names array, and null individual name entries gracefully. The function re-reads the IIS state pointer after each deallocation as a defensive measure.

**Deallocation Order:**
1. Each individual constraint name string (iterated by constraint count)
2. The constraint names array itself
3. Variable upper bound IIS membership array
4. Constraint IIS membership array
5. Variable lower bound IIS membership array
6. IISState structure itself
7. Model's IIS state reference set to null

**Thread Safety:** unsafe -- Assumes single-threaded access to the model's IIS state.

**Dependencies:**
- Memory primitives module (cxf_free) -- for each individual deallocation

---

### cxf_free_warmstart_basis

**Purpose:** Free the warm-start basis data structure and all its nested sub-structures and arrays, using the double-pointer pattern to null the caller's reference.

**Signature:**
- Input: env : pointer-to-Environment - The environment for memory tracking during deallocation
- Input: warmStartDataRef : pointer-to-pointer-to-WarmStartData - Double pointer to the warm-start data structure, allowing the function to null the caller's reference after freeing
- Output: void

**Preconditions:**
- The environment must be valid (non-null) for memory tracking.
- The warmStartDataRef may be null or may point to a null pointer; both cases are handled gracefully.

**Postconditions:**
- If the WarmStartData structure existed:
  - The variable basis status array has been freed.
  - The primal/dual solution values array has been freed.
  - The constraint basis status array has been freed.
  - If a factorization cache sub-structure existed:
    - Its index array has been freed.
    - Its value array has been freed.
    - The sub-structure itself has been freed.
  - The WarmStartData structure itself has been freed.
  - The caller's pointer (*warmStartDataRef) has been set to null.
- If the WarmStartData structure did not exist, the function has no effect.

**Side Effects:**
- Deallocates memory through the environment's memory management system.
- Nulls each pointer field within the structures after freeing (defensive pattern).
- Nulls the caller's reference pointer to the WarmStartData structure.

**Error Conditions:**
- None. This function does not return an error code. Null pointer inputs at every level are handled gracefully.

**Behavioral Description:**
cxf_free_warmstart_basis performs an inside-out deallocation of the warm-start data structure used to accelerate re-optimization. The structure stores three arrays for the simplex warm-start (variable basis status, constraint basis status, and primal/dual solution values) and an optional nested factorization cache that holds two additional arrays (index and value arrays from a cached LU decomposition). The function frees these in reverse allocation order: first the leaf arrays within the main structure, then the nested sub-structure's arrays and the sub-structure itself, then the main structure, and finally nulls the caller's reference. Each pointer is null-checked before freeing and null-set after freeing, following the solver's standard defensive memory management pattern.

**Null Safety:** Both the outer pointer (warmStartDataRef) and the inner pointer (*warmStartDataRef) are null-checked. Within the structure, each array pointer is individually null-checked.

**Deallocation Order:**
1. Variable basis status array
2. Primal/dual solution values array
3. Constraint basis status array
4. Factorization cache index array (within nested sub-structure)
5. Factorization cache value array (within nested sub-structure)
6. Factorization cache sub-structure itself
7. WarmStartData structure itself
8. Caller's reference pointer set to null

**Thread Safety:** unsafe -- Assumes single-threaded access to the WarmStartData structure.

**Dependencies:**
- Memory primitives module (cxf_free) -- for each individual deallocation

---


**Purpose:** Send a disconnect message over the remote solver protocol to notify the remote server that this model's connection is being terminated, then clean up the connection resources.

**Signature:**
- Input: model : pointer-to-Model - The model connected to a remote solver that is being disconnected
- Output: int - Zero on success, non-zero error code on failure (from message validation)

**Preconditions:**
- The model must be valid with an active environment reference.
- The environment must have a valid remote solver socket/pipe connection.
- The environment must have a valid critical section (mutex) for socket access.

**Postconditions:**
- A disconnect message has been serialized and written to the remote solver socket using the solver's remote solver protocol.
- The socket buffer has been flushed to ensure the message is transmitted.
- The critical section protecting the socket has been acquired and released correctly, even in error cases.

**Side Effects:**
- Writes a protocol message to the remote solver socket (a file-based I/O handle).
- Acquires and releases the environment's socket critical section.
- The disconnect message includes a message type marker identifying it as a graceful disconnection, followed by a serialized descriptor table identifying the model.

**Error Conditions:**
- If validation of any result descriptor entry fails (invalid type/count combination), the function returns a non-zero error code. Serialization stops at the first validation failure, but the critical section is still properly released.

**Behavioral Description:**

**Protocol Details:**
- The message type marker is a small integer constant indicating "disconnect" (distinct from the "results" marker used when sending optimization results).
- All multi-byte values are converted to network byte order before transmission.
- The serialization format handles different entry types (integer arrays, floating-point arrays, and type-specific formats) using a type-dispatch mechanism.
- The descriptor table is typically minimal for a disconnect message (identification data only, no solution data).

**Thread Safety:** conditional -- Socket access is protected by the environment's critical section, making the actual write operation thread-safe. However, the function assumes the model itself is not being concurrently modified.

**Dependencies:**
- cxf_htonl, cxf_htonll (utility module) -- network byte order conversion
- cxf_validate_result, cxf_compute_result_size (utility module) -- descriptor validation and size computation
- EnterCriticalSection, LeaveCriticalSection (threading module) -- socket access protection

---

## Module-Level Notes

### Cleanup Ordering in Model Destruction

During model destruction (cxf_freemodel), the functions in this module are called in a specific order to respect ownership and reference dependencies:

2. **cxf_free_basis_state** -- Free concurrent environments (may involve remote job termination)
3. **cxf_free_iis_state** -- Free IIS diagnostic data
4. **cxf_free_warmstart_basis** -- Free warm-start data
5. **cxf_free_solver_state** -- Free attribute table (last, as other cleanup may query attributes)
6. **cxf_cleanup_solve_state** -- Finalize timing and callbacks (called at end of each solve, not just destruction)

### Defensive Memory Patterns

All deallocation functions in this module follow the solver's standard defensive pattern:

1. **Null-check before free**: Every pointer is tested for null before being passed to the deallocation function.
2. **Null-after-free**: Every pointer field is set to null immediately after the memory it points to is freed, preventing double-free bugs and use-after-free if the cleanup is interrupted or re-entered.
3. **Re-read after free**: Several functions re-read the parent structure pointer after freeing a child allocation, guarding against potential side effects from the memory management system.
4. **Inside-out deallocation**: Nested structures are freed from the innermost allocations outward, ensuring no dangling references exist at any point during the cleanup sequence.

### Relationship to Initialization Module (P3.03)

The cleanup functions in this module are the inverse counterparts to initialization operations:

| Cleanup Function (P3.04) | Initialization Counterpart |
|--------------------------|---------------------------|
| cxf_cleanup_solve_state | cxf_init_solve_state (P3.03) |
| cxf_free_solver_state | Attribute table creation during model init |
| cxf_free_basis_state | Concurrent environment creation during concurrent solve setup |
| cxf_free_iis_state | IIS computation (cxf_computeIIS) |
| cxf_free_warmstart_basis | Warm-start creation (via VBasis/CBasis attribute setting) |

## References

- Maros, I. (2003). *Computational Techniques of the Simplex Method*. Springer. (Chapter 2: efficient memory management in LP solvers)
- Dantzig, G.B. (1963). *Linear Programming and Extensions*. Princeton University Press.
- Gleeson, J. and Ryan, J. (1990). "Identifying Minimally Infeasible Subsystems of Inequalities." *European Journal of Operational Research*, 46(3):375-381. (IIS concept)
- Bixby, R.E. (2002). "Solving Real-World Linear Programs: A Decade and More of Progress." *Operations Research*, 50(1):3-15. (Warm-start and factorization caching)

---

## Verification Checklist

```
[x] No hex addresses or offsets
[x] No Ghidra artifacts (param_, local_, iVar, DAT_, FUN_)
[x] No binary-specific constants
[x] No copied code fragments
[x] All algorithms cite published sources
[x] All data structures described semantically
[x] Passes the Clean Room Test (Rule 10)
```
