# Module: State Cleanup - Buffers

## Purpose

This module provides functions for releasing and resetting model-level state that accumulates during optimization and through the lazy update pattern. It handles three distinct cleanup responsibilities: (1) disconnecting from remote solvers and releasing callback state, (2) releasing concurrent environment pools with reference counting, (3) clearing all solution-related data from a model, (4) performing a deep free of the pending modifications buffer, and (5) performing a soft reset of the pending modifications buffer for reuse. These functions are called during model destruction, solution clearing, and update processing.

## Functions

### cxf_free_callback_state

**Purpose:** Cleanly disconnects a model from a remote solver and releases the associated callback registration state.

**Signature:**
- Input: model : pointer-to-Model - The model whose remote callback state should be freed
- Output: void

**Preconditions:**
- model is a valid, non-null pointer to an initialized Model
- The Model has been operating in remote solver mode (has an active remote solver connection reachable through its Environment)
- The Model's Environment has a valid root Environment with an active remote solver connection

**Postconditions:**
- The Model's callback registration count is set to zero
- If a remote optimization was in progress, a termination request has been sent and the function has either waited for completion or timed out
- A disconnect message has been sent to the remote solver
- The remote solver connection handle has been freed (if one existed)
- Any server-side error messages have been propagated to the Environment's error state

**Side Effects:**
- Sends a termination request to the remote solver if optimization is active
- Blocks (with bounded polling) until the remote operation completes or the polling limit is reached
- Sends a disconnect protocol message to the remote server
- May send a result-fetch protocol message if the disconnect did not complete cleanly
- Frees the remote solver connection handle
- May set error state on the Environment if the server reports an error
- Zeroes the Model's callback registration count

**Error Conditions:**
- If optimization is in progress on the Environment (not the model), the function logs a warning and returns without freeing
- If the disconnect message fails with a remote server error, the server's error message is propagated to the Environment
- If the disconnect returns an unexpected error, the function attempts to wait for any outstanding optimization and fetch results before returning

**Behavioral Description:**
This function manages the complex cleanup required when a model with registered callbacks is being freed while connected to a remote solver. It first checks whether a remote optimization is still active for this model. If so, it requests termination and polls until the model becomes inactive (with a bounded maximum number of polling iterations to prevent infinite loops). Once the model is inactive, it acquires the remote solver lock, frees any existing connection handle, sends a disconnect message using the remote solver protocol, and releases the lock. The callback registration count on the model is then cleared. If the disconnect completes successfully or times out, the function returns. If the server reports an error, the error is propagated to the Environment. For other error conditions, the function waits for any outstanding remote optimization, then sends a result-fetch request and processes the response, propagating any error information to the Environment.

**Thread Safety:** Conditional. Acquires the remote solver lock for protocol operations. The bounded polling loop for termination does not hold a lock.

**Dependencies:** remote solver protocol functions (acquire lock, release lock, send disconnect, send result request, receive response). Termination function on the Model. Logging and error-reporting functions on the Environment.

---

### cxf_free_solution_pool

**Purpose:** Frees the array of concurrent environment references stored in a model, handling reference counting, deferred cleanup, and remote job termination.

**Signature:**
- Input: model : pointer-to-Model - The model whose concurrent environment pool should be freed
- Output: void

**Preconditions:**
- model is a valid, non-null pointer to an initialized Model

**Postconditions:**
- All environments in the concurrent environment pool have either been freed or had their reference counts decremented
- The concurrent environment pool array has been freed
- The Model's concurrent environment pool pointer is null and the pool count is zero

**Side Effects:**
- For each environment in the pool: decrements the reference count on the environment's root environment under the root environment's mutex
- If an environment's reference count reaches zero, recursively frees the environment and (if the root environment also reaches zero) the root environment
- If an environment's reference count is still positive after decrement, performs deferred cleanup: logs a warning, and if a remote solver job is active, attempts to terminate it (with bounded polling), sends a kill message, and frees the remote solver connection
- Frees the pool array using the model's primary environment as the memory context
- Clears the pool pointer and count on the model

**Error Conditions:**
- None (void return). Deferred-free conditions are handled gracefully via logging and remote job termination.

**Behavioral Description:**
This function iterates through the model's concurrent environment pool -- an array of Environment references created for parallel optimization (e.g., concurrent solving with different parameter settings). For each environment, it decrements the root environment's reference count under the root environment's mutex. If the reference count reaches zero, the environment is fully freed via the internal environment destructor. If the reference count is still positive (meaning other code still references this environment), the function performs deferred cleanup: it logs a warning, checks for an active remote solver job associated with the environment, and if found, requests termination with bounded polling and ultimately kills the remote job. After processing all environments, the pool array itself is freed and the model's pool pointer and count are cleared.

**Thread Safety:** Conditional. Reference count decrements are performed under the root environment's mutex. Remote job termination uses bounded polling without holding a lock.

**Dependencies:** Environment internal destructor. Memory free function. remote solver remote job check, yield, sleep, terminate, and connection free functions. Critical section enter/leave for reference counting.

---

### cxf_clear_solution

**Purpose:** Clears all solution-related data from a model, resetting it to a pre-solve state ready for re-optimization.

**Signature:**
- Input: model : pointer-to-Model - The model to clear
- Input: clearHints : int - If nonzero, also clear start hints, warm-start data, and user-supplied basis information
- Output: int - Zero on success, nonzero error code on failure

**Preconditions:**
- model is a valid, non-null pointer to an initialized Model
- The Model is not currently being optimized (modification_blocked is zero, or the function handles the callback path separately)

**Postconditions:**
- All solver state has been freed: barrier state, timing state, internal vector containers, basis factorization, solution state
- All solution output structures have been freed: solution info containers, SOS data, general constraint data, IIS state
- The concurrent environment pool has been freed
- Basis state has been freed
- Matrix version and solution status flags have been cleared
- The model's attribute cache has been invalidated
- If clearHints was nonzero: start hint arrays, branch priorities, user-supplied basis arrays (variable basis, constraint basis, quadratic constraint basis, SOS basis, PWL basis), start values, partition data, lazy constraint flags, variable hint values, and variable hint priorities have all been freed and their pointers nulled
- The model's initialized flag is set to 1
- The environment's thread pool has been freed
- Asynchronous optimization threads have been joined

**Side Effects:**
- Applies any pending model modifications before clearing (calls the update function)
- Invalidates the model's attribute cache (breaking wired pointers from the attribute table into freed structures)
- Frees the environment's thread pool
- Waits for any asynchronous optimization thread to complete
- Copies the environment's work limit parameter to the model
- Clears the model fingerprint (if clearHints is set)

**Error Conditions:**
- If applying pending modifications fails, the error is reported and the error code is returned
- All other cleanup operations are unconditional and do not produce errors

**Behavioral Description:**
This function performs a comprehensive clearing of all solution and solver state from a model. It first applies any pending modifications to ensure the model is in a consistent state. If callbacks are registered, it delegates to a callback-aware cleanup path that handles callback state before proceeding with basis and pool cleanup. In the standard (no-callback) path, it clears matrix status flags, optionally frees all user-supplied hint and warm-start data (when clearHints is nonzero), frees all solver working state (barrier state, timing data, vector containers, basis factorization, warm-start data including any nested factorization cache), frees solution output structures (with attribute cache invalidation before each free to break wired pointers), frees special constraint data (SOS and general constraints), frees IIS state, basis state, and the concurrent environment pool, waits for any asynchronous thread to finish, frees the environment's thread pool, and finally marks the model as initialized and copies the work limit from the environment.

**Thread Safety:** Unsafe. This function modifies Model state extensively and must not be called concurrently with any other operation on the same Model.

**Dependencies:** Model update function (to apply pending modifications). Callback clear function. Basis state free, solution pool free, environment model cleanup, barrier state free, timing state free, vector free, vector pair free, basis factorization free, solution state clear, attribute cache clear, IIS state free, thread wait, and thread pool free functions. Memory free function.

---

### cxf_clear_pending_buffer

**Purpose:** Performs a complete deep free of the pending modifications buffer, releasing all sub-structures and the buffer itself.

**Signature:**
- Input: env : pointer-to-Environment - The environment providing the memory management context
- Input: bufferPtr : pointer-to-pointer-to-PendingBuffer - Pointer to the Model's pending buffer pointer
- Output: void

**Preconditions:**
- env is a valid, non-null pointer to an initialized Environment
- bufferPtr is a valid pointer (may point to a null buffer pointer, in which case the function is a no-op)

**Postconditions:**
- All dynamically allocated sub-structures within the PendingBuffer have been freed: pending linear constraint changes, range constraint changes, variable additions, SOS constraint data, indicator constraint data, general constraint data, quadratic constraint data, quadratic objective terms, cone constraint data, and all specialized sub-structures handled by delegated cleanup functions
- All direct pointer fields in the PendingBuffer (deletion masks, name change arrays, variable type changes, etc.) have been freed
- The PendingBuffer structure itself has been freed
- The caller's pointer (*bufferPtr) has been set to null

**Side Effects:**
- Frees all memory associated with every pending modification category
- Sets the caller's buffer pointer to null

**Error Conditions:**
- None. The function is null-safe at all levels: null bufferPtr causes immediate return, null sub-structure pointers are skipped, and null array pointers within sub-structures are skipped.

**Behavioral Description:**
This function performs a complete teardown of the PendingBuffer, which implements the lazy update pattern by accumulating batched model modifications before they are applied. It iterates through each modification category (linear constraints, range constraints, variable additions, SOS constraints, indicator constraints, general constraints, quadratic constraints, quadratic objective terms, cone constraints, and several additional specialized sub-structures) and for each: checks whether the sub-structure pointer is non-null, frees all dynamically allocated arrays within the sub-structure (checking each for null before freeing and nulling the pointer after), then frees the sub-structure itself. After all sub-structures are processed, it frees direct pointer fields stored in the main buffer (deletion masks for variables, constraints, and quadratic constraints; name change arrays; variable type change arrays; and various other data arrays). Finally, it frees the PendingBuffer structure itself and sets the caller's pointer to null. The cleanup is exhaustive and null-safe at every level, ensuring no memory leaks regardless of which modification types were active.

**Thread Safety:** Unsafe. Must not be called concurrently with any operation that reads or writes the PendingBuffer.

**Dependencies:** Memory free function. Six specialized sub-structure cleanup functions that handle complex nested structures with their own internal allocation patterns.

---

### cxf_reset_pending_buffer

**Purpose:** Resets the pending modifications buffer to an empty state without freeing the buffer structure itself, enabling reuse.

**Signature:**
- Input: env : pointer-to-Environment - The environment providing the memory management context
- Input: buffer : pointer-to-PendingBuffer - The buffer to reset (not freed)
- Output: void

**Preconditions:**
- env is a valid, non-null pointer to an initialized Environment
- buffer may be null (in which case the function is a no-op) or a valid pointer to an allocated PendingBuffer

**Postconditions:**
- All modification counters and flags in the PendingBuffer have been reset to zero
- The buffer's validation marker has been refreshed to indicate a properly initialized state
- Certain internal arrays that hold per-batch temporary data have been freed (coefficient change arrays, temporary arrays within selected sub-structures)
- Sub-structure counters have been reset to zero, but the sub-structures themselves remain allocated
- The PendingBuffer itself remains allocated and ready for reuse

**Side Effects:**
- Frees selected internal arrays (coefficient changes, temporary arrays in certain sub-structures)
- Zeroes counters within sub-structures without freeing the sub-structures themselves
- Writes a validation marker to the buffer to indicate proper reset

**Error Conditions:**
- None. Null-safe: if buffer is null, returns immediately.

**Behavioral Description:**
This function performs a soft reset of the PendingBuffer, preparing it to accept a new batch of modifications without the overhead of deallocation and reallocation. Unlike cxf_clear_pending_buffer which fully frees everything, this function zeroes all modification counters and flags, writes a validation marker to confirm the buffer is properly initialized, frees only the temporary arrays that accumulate per-batch data (such as coefficient change arrays and certain sub-structure temporary arrays), and resets the internal counters of sub-structures to zero while leaving the sub-structures themselves allocated. This is used after applying modifications (during update or optimize) to prepare the buffer for the next batch of API calls, avoiding the cost of repeatedly allocating and freeing the buffer structure and its sub-structures. Two specialized sub-structure reset functions are called to handle complex nested structures that require their own reset logic.

**Thread Safety:** Unsafe. Must not be called concurrently with any operation that reads or writes the PendingBuffer.

**Dependencies:** Memory free function. Two specialized sub-structure reset functions that handle complex nested structures.

---

## Module-Level Design Notes

### Cleanup Ordering Discipline

The functions in this module follow a consistent cleanup pattern: free inner allocations before outer structures, null each pointer immediately after freeing, and re-read pointers from the parent structure after each free in case a side effect (such as reference counting or callback invocation) modified the parent. This defensive re-reading pattern guards against cases where freeing one structure triggers changes to sibling pointers.

### Callback-Aware Path

cxf_clear_solution has two distinct cleanup paths: one for models with registered callbacks and one for models without. The callback-aware path delegates to a specialized cleanup function that properly handles callback state before proceeding with basis and pool cleanup. This separation ensures that callback state is released in the correct order relative to solver state.

### Lazy Update Pattern Integration

cxf_clear_pending_buffer and cxf_reset_pending_buffer are the cleanup counterparts of the lazy update system described in the ModificationTracker (PendingBuffer) data structure specification. The "clear" function is the full teardown used at model destruction, while the "reset" function is the lightweight counterpart used between update cycles to avoid reallocation overhead.

### Null Safety

All functions in this module are null-safe: they check pointers at every level before dereferencing and treat null as a no-op condition. This defensive programming practice is essential for cleanup functions, which may be called during error recovery paths where the state is partially initialized.

### Reference Counting for Environments

cxf_free_solution_pool demonstrates the reference counting pattern used for shared environments. Each concurrent environment has a root environment with a reference count protected by a mutex. The reference count is decremented under the mutex, and the environment is freed only when the count reaches zero. This allows safe sharing of environments across concurrent solver instances.

## References

- Maros, I. (2003). *Computational Techniques of the Simplex Method*. Springer. Section 2.5: Efficient model management for LP solvers.
- McConnell, S. (2004). *Code Complete*, 2nd edition. Microsoft Press. Chapter 24: Defensive Programming (null-safety patterns in cleanup functions).
- Butenhof, D.R. (1997). *Programming with POSIX Threads*. Addison-Wesley. (Reference counting with mutex synchronization.)

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
