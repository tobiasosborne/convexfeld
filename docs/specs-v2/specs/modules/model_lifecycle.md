# Module: Model Lifecycle

## Purpose

The Model Lifecycle module manages the creation, modification, and cleanup of the Model structure -- the central data object in any LP solver. It encompasses four key lifecycle operations: (1) allocating and initializing a new Model with all its constituent sub-structures, (2) applying batched (lazy) modifications to the Model's internal matrix representation, (3) maintaining consistency between the Model and the Environment's model tracking structures, and (4) tearing down child environments when a parent environment is being cleaned up.

The most complex function in this module is the lazy modification applicator. Commercial LP solvers universally use a "lazy update" pattern where user modifications (adding variables, adding constraints, changing coefficients, changing bounds, changing names) are buffered in a pending modifications buffer rather than applied immediately. When the user explicitly requests an update -- or when an optimization is about to begin -- the solver applies all pending modifications in a single batch operation. This avoids the cost of rebuilding internal data structures (sparse matrix indices, row-major representations, presolve state) after every individual change. The batch application function is responsible for validating warm-start data, counting and classifying modifications, checking name uniqueness, invalidating stale cached data, rebuilding the constraint matrix, and updating auxiliary structures. Its computational complexity is proportional to the problem dimensions and the number of nonzeros affected.

## Functions

### cxf_model_create_internal

**Purpose:** Allocate and initialize a new Model structure with all required fields, sentinels, and sub-structures, optionally creating a private child Environment.

**Signature:**
- Input: `parent_environment` : pointer-to-Environment -- The environment with which the new model will be associated
- Input: `create_child_environment` : int -- If nonzero, create a private child environment for the model (enabling per-model parameter overrides); if zero, use the parent environment directly
- Input: `child_environment_parameter` : int -- Configuration parameter passed to the child environment creation function (only used when create_child_environment is nonzero)
- Output: pointer-to-Model -- Pointer to the newly created Model, or NULL on failure

**Preconditions:**
- The parent_environment must be a valid, initialized Environment

**Postconditions:**
- On success, a fully initialized Model structure is returned with:
  - The validity sentinel set to the expected constant value
  - The primary model self-reference set to point to the new Model itself
  - All modification control flags cleared (modifications allowed, no optimization in progress)
  - The environment reference set (either to the parent directly, or to the newly created child environment)
  - If a child environment was requested, the environment_owned flag is set to indicate the Model owns its environment
  - Internal data storage allocated and zero-initialized
  - Initial model setup performed (attribute table registration, internal state configuration)
  - A secondary fingerprint/seed value initialized for determinism support
- On failure (any allocation fails), all partially allocated memory is freed and NULL is returned

**Side Effects:**
- Allocates memory for the Model structure and its internal data storage
- If create_child_environment is nonzero, creates a child environment (which may involve parameter table duplication)
- Performs initial model setup (attribute table construction, internal vector initialization)

**Error Conditions:**
- Memory allocation failure for the Model structure -> returns NULL
- Child environment creation failure -> frees partially constructed Model, returns NULL
- Memory allocation failure for the internal data storage -> frees partially constructed Model, returns NULL

**Behavioral Description:**
The function performs the following steps in sequence:

1. **Allocate Model structure:** Allocate a zero-initialized memory block for the Model structure using the parent environment's memory allocator.

2. **Initialize identity and validity fields:** Write the validity sentinel constant to the sentinel field. Set the primary model self-reference to point to the Model itself. Clear the modification blocked flag, the additional status flag, and the callback count to zero. Initialize the fingerprint/seed field to a predefined constant value.

3. **Set environment reference:** Store the parent environment pointer in the Model's environment field.

4. **Optionally create child environment:** If create_child_environment is nonzero, invoke the child environment creation function with the parent environment and the configuration parameter. If this succeeds, update the Model's environment reference to the new child environment and set the environment_owned flag to indicate ownership. If this fails, proceed to the cleanup path.

5. **Allocate internal data storage:** Allocate a zero-initialized memory block for the Model's internal data structure (which holds matrix data pointers, solution data, basis information, and working arrays). Store the pointer on the Model. If allocation fails, proceed to cleanup.

6. **Perform initial setup:** Invoke initialization routines that configure environment-related data and perform model setup (attribute table construction, internal state preparation).

7. **Return the Model pointer.**

If any step fails, the function invokes a cleanup routine that frees all previously allocated memory (including any child environment) and returns NULL.

**Thread Safety:** Unsafe. The caller must ensure no concurrent access to the parent environment during model creation. The parent environment's memory allocator may or may not be thread-safe depending on the implementation.

**Dependencies:**
- Memory allocation (via the environment's allocator)
- Child environment creation (optional)
- Model setup and initialization routines

---

### cxf_env_model_cleanup

**Purpose:** Clean up all child environments associated with a parent environment, handling reference counting, deferred frees for still-referenced environments, and termination of remote solver jobs.

**Signature:**
- Input: `parent_environment` : pointer-to-Environment -- The parent environment whose child environment array should be cleaned up
- Output: void

**Preconditions:**
- The parent_environment must be a valid Environment with a properly initialized child environment tracking array (which may be empty)

**Postconditions:**
- All child environments that can be immediately freed have been freed via the internal environment destructor
- Child environments that are still referenced by other entities (reference count has not reached zero) have been logged with a warning and their slots cleared; the actual free is deferred until the reference count reaches zero
- Any active remote solver jobs associated with child environments have been terminated
- The parent's child environment count is set to zero
- The parent's child environment array is freed and its pointer set to null

**Side Effects:**
- Frees child environment structures and all their owned resources
- Decrements reference counts on root/parent environments referenced by children
- May free root environments whose reference counts reach zero
- Logs warning messages when environments cannot be immediately freed (still referenced)
- Logs warning messages when remote jobs are killed
- Terminates remote solver jobs (sends termination signals over the network)
- Frees the child environment array itself

**Error Conditions:**
- No error return (void function); errors during cleanup are handled internally:
  - If a child environment is still referenced, a warning is logged and the free is deferred
  - If a remote job cannot be gracefully stopped within the polling limit, a forceful termination is sent

**Behavioral Description:**
The function processes all child environments registered with the parent environment, performing cleanup in three phases:

1. **Retrieve child array:** Read the child environment count and array pointer from the parent environment. If the count is zero or negative, skip directly to freeing the array.

2. **Iterate through child environments:** For each non-null entry in the child environment array:

   a. **Reference count management:** Retrieve the child's root environment reference. If a root environment exists, acquire the root's mutex, decrement the root's reference count, record whether the reference count reached zero, and release the mutex.

   b. **Determine freeing strategy:**
      - If the child is a different object from its root, or the root's reference count reached zero, free the child environment immediately using the internal environment destructor. If the root is a different object and its reference count also reached zero, free the root as well.
      - If the child is the same as its root and the reference count is still positive (other entities still hold references), the environment cannot be freed yet. Log a warning indicating the free is deferred.

   c. **Handle deferred-free environments:** For environments that cannot be freed immediately, log appropriate warnings.

   d. **Terminate remote solver jobs:** If the deferred-free environment has an active remote solver connection with a valid server address and job identifier:
      - Check if the remote job is still active.
      - If active, set a termination flag on the environment's async state to signal the job.
      - Poll for graceful job completion, yielding and sleeping between polls, up to a maximum poll count (on the order of hundreds of thousands of iterations).
      - After polling completes (or the job stops), log a warning identifying the killed job and server.
      - Send an explicit termination message to the server and free the connection resources.

   e. **Clear the slot:** Set the child array entry to null for deferred-free environments.

   f. **Refresh counts:** After each child is processed, re-read the child count and array from the parent, as they may have been modified by the cleanup of other children.

3. **Finalize:** Set the parent's child count to zero. Free the child environment array memory and set the array pointer to null.

**Thread Safety:** Partially thread-safe. The function acquires the root environment's mutex when modifying reference counts, ensuring correct reference count management across threads. However, the function itself modifies the parent's child array and count without acquiring the parent's mutex, so it must not be called concurrently with other operations that modify the parent's child array.

**Dependencies:**
- Internal environment destructor (for freeing child environments)
- remote solver connection management (for checking and terminating remote jobs)
- Logging subsystem
- Memory deallocation

---

### cxf_update_model_manager

**Purpose:** Clear all model pointer entries from an environment's model management structure and free the associated storage, used when models are being disassociated from an environment.

**Signature:**
- Input: `allocator_environment` : pointer-to-Environment -- The environment to use as the memory deallocation context
- Input: `model_manager` : pointer -- Pointer to the model management structure to clear (may be null)
- Output: void

**Preconditions:**
- If non-null, model_manager must point to a valid model management structure with consistent count, capacity, and array fields

**Postconditions:**
- All non-null model pointer entries in the array have been individually freed
- The model pointer array itself has been freed and its pointer set to null
- Both the model count and model capacity are set to zero
- If model_manager was null, no action is taken

**Side Effects:**
- Frees each non-null model pointer entry in the array
- Frees the model pointer array
- Zeros the count and capacity fields on the model management structure

**Error Conditions:**
- Null model_manager -> silent return, no action

**Behavioral Description:**
The function performs a clean teardown of the model management structure:

1. **Null check:** If the model_manager pointer is null, return immediately with no action.

2. **Free individual model pointers:** Read the model count and model pointer array from the model management structure. If the count is positive, iterate through each entry in the array. For each non-null entry, free the model pointer using the allocator environment's deallocation function, then set the slot to null. After each free, re-read the count and array pointer from the structure (defensive re-read in case of side effects during deallocation).

3. **Free the array:** If the model pointer array is non-null after all entries have been freed, free the array itself and set the array pointer to null.

4. **Reset counts:** Set both the model count and model capacity fields to zero.

**Thread Safety:** Unsafe. The caller must ensure exclusive access to the model management structure during this operation.

**Dependencies:**
- Memory deallocation (via the allocator environment)

---

### cxf_model_apply_modifications

**Purpose:** Apply all pending model modifications from the lazy update buffer to the Model's internal matrix representation, performing validation, invalidation of stale cached data, and reconstruction of internal structures.

**Signature:**
- Input: `model` : pointer-to-Model -- The model whose pending modifications should be applied
- Output: int -- Zero on success, nonzero error code on failure

**Preconditions:**
- The model must be a valid Model (validity sentinel must match the expected constant)
- If the model is in remote solver mode (remote optimization), certain phases of local processing are skipped, and the function delegates to the remote solver synchronization path

**Postconditions:**
- On success (return 0):
  - All pending modifications have been applied to the model's matrix data
  - The pending modifications buffer has been cleared and reset
  - Warm-start data has been validated, downgraded, or discarded as appropriate based on compatibility with the modifications
  - Cached data structures that are invalidated by the modifications (solution data, row-major representation, presolve state, basis data, solver state) have been freed
  - Name uniqueness has been validated (duplicate variable, constraint, or range constraint names produce an error)
  - Variable type counts (binary, integer, continuous) have been updated
  - The model's state flags have been updated to reflect the modification
  - The model's work counter has been incremented by the total elements processed
  - String pool consolidation has been performed if the name update count exceeds a high-water threshold
- On failure (nonzero return):
  - An error message has been set on the model's environment (an appropriate model-update error message)
  - The error code has been logged
  - If the pending buffer existed, it has been cleared to prevent reprocessing of failed modifications
  - The model's update-succeeded flag is cleared

**Side Effects:**
- Modifies the model's matrix data (dimensions, arrays, coefficients, bounds, types, names, senses, objective)
- Frees and reallocates internal cached structures
- May log warning messages about discarded warm-start data
- Updates the model's work counter (accumulated operation count)
- May allocate temporary hash tables for name uniqueness checking
- May allocate and consolidate string pools for name storage

**Error Conditions:**
- Out of memory during any allocation -> returns OUT_OF_MEMORY error code
- SOS constraint validation failure -> returns the validation error code
- Duplicate variable, constraint, or range constraint names detected -> returns INVALID_ARGUMENT error code with message indicating tags must be unique
- Dimension reduction processing failure -> returns the processing error code
- Various internal processing failures -> return appropriate error codes

**Behavioral Description:**
This is the most complex function in the module, processing an 8-phase pipeline that handles all categories of model modifications in a single batch. The phases are:

**Phase 1 -- SOS Constraint Validation:**
If the model contains Special Ordered Set constraints that have not been validated, invoke the SOS validation function. If validation fails, abort with the validation error.

**Phase 2 -- Warm-Start Data Validation:**
If the model has warm-start data (primal start, dual start, or basis start):

- **Solution validation:** For a primal/dual start, scan the solution vector comparing each value against the solver's infinity threshold. Classify the warm-start as fully validated (all values finite), primal-only (only variable values are finite), dual-only (only constraint dual values are finite), or invalid (mixed infinite values). Log a warning and discard invalid warm-starts.

- **Basis validation:** For a basis start, scan the basis status array checking for invalid markers. Count the number of basic variables. A valid basis must span the entire array and have exactly as many basic variables as there are constraints (with an exception for range constraints). Log a warning and discard invalid bases.

**Phase 3 -- Modification Counting and Classification:**
If the pending modifications buffer is not active (no modifications pending), skip to cleanup. Otherwise:

- Cache the current matrix dimensions (number of variables, number of constraints).
- Check if dimensions have decreased (indicating deletions). If so, invoke the dimension reduction handler, which compacts the matrix and updates all dependent structures. This involves freeing numerous cached structures (solution cache, callback state, internal vectors, solver state, presolve state), recalculating the nonzero count from the sparse column start array, and recomputing variable type counts.

- **Variable modification flag counting:** If variable modifications exist, iterate through the per-variable modification flags array, counting modifications by category: bound changes, objective coefficient changes, variable type changes, name changes, and several other per-variable attribute changes. Update the name change counter on the model.

- **Constraint modification flag counting:** If constraint modifications exist, iterate through the per-constraint modification flags array, counting: RHS changes, sense changes, name changes, lazy constraint flag changes, and other per-constraint attribute changes.

- **Quadratic constraint modification counting:** If quadratic constraint modifications exist, iterate through quadratic constraint modification flags, counting additions, deletions, name changes, and coefficient changes.

- **Additional modification sources:** Count modifications from SOS constraints, general constraints, indicator constraints, piecewise-linear constraints, network flow modifications, solution hints, and multi-objective modifications by examining the corresponding modification sub-structures.

- **Objective sense change detection:** Check if the objective scaling factor or direction differs from the current matrix's objective sense.

**Phase 4 -- Fast Path for Name-Only Changes:**
If the only pending modifications are name changes (no structural, coefficient, bound, type, or other substantive changes), take a fast path:

- For variable names, constraint names, and range constraint names in turn: create a hash table with a load factor appropriate for efficient duplicate detection, insert each non-empty name, and check for collisions. If a duplicate is detected, set the error code to INVALID_ARGUMENT and report that tags must be unique.
- Record the unique name count and exit without further processing.

**Phase 5 -- Warm-Start Invalidation and Structural Preparation:**
For substantive modifications:

- **Warm-start compatibility check:** Determine whether existing warm-start data is compatible with the modifications. Structural changes (variable/constraint additions or deletions) generally invalidate warm-starts. Log warnings for each discarded warm-start component (PStart, DStart). Free incompatible warm-start data.

- **Modification tracking allocation:** Allocate a modification tracking structure if one does not exist, recording summary counts of additions, deletions, and other changes.

- **Cache invalidation:** Free all cached derived data that is invalidated by the modifications. The set of caches freed depends on the modification categories: constraint changes invalidate solution-related caches, variable/constraint additions or deletions invalidate objective and bound caches, and any structural change invalidates internal vectors, solver state, presolve state, and global caches.

- **Basis transfer:** If a prior basis exists and the modifications are compatible with incremental basis reuse, transfer the basis from the old structure to a new one, skipping entries for deleted variables/constraints and appending space for new ones. This avoids a full basis recomputation on the next solve.

**Phase 6 -- Matrix Attribute Updates:**
Apply the actual data changes to the matrix:

- **Constraint sense flipping:** If any constraint's sense has changed between '<=' and '>=', the internal storage convention requires negating the affected coefficients and RHS values. For each changed constraint, negate all matrix coefficients in the corresponding row and the RHS value using the IEEE 754 sign-bit flip technique (which is exact and handles infinity and NaN correctly). This technique is a standard optimization in numerical software.

- **Objective sense change:** If the optimization direction has changed (minimization to maximization or vice versa), negate the objective sense indicator, all objective coefficients, the objective constant, and all quadratic objective coefficients. The negation uses the same sign-bit flip technique, with an unrolled loop processing multiple coefficients per iteration for throughput.

- **Per-variable attribute updates:** For each variable with modified attributes, copy the new values from the modification buffer to the corresponding arrays in the matrix data (bounds, objective coefficients, variable types, variable names).

- **Per-constraint attribute updates:** For each constraint with modified attributes, copy the new values (RHS, sense, names, lazy flags).

- **SOS constraint processing:** Apply SOS constraint modifications (additions, deletions, member changes).

- **Quadratic constraint processing:** Apply quadratic constraint modifications, including resizing Q-matrix storage, compacting deleted entries, and appending new entries. Acquire and release the appropriate locks for thread safety.

- **General constraint processing:** Apply general constraint modifications (indicator, piecewise-linear, min/max/abs), including compaction of deleted entries and appending of new ones.

- **Multi-objective and hint processing:** Apply multi-objective modifications and solution hint data through delegated processing functions.

**Phase 7 -- Variable Type Recount and Name Consolidation:**
After all modifications are applied:

- **Variable type recount:** If variable types have changed, re-scan the variable type array to update the counts of binary, integer, and other non-continuous variable types.

- **String pool consolidation:** If the cumulative name update count exceeds a high-water threshold (on the order of tens of thousands), consolidate all variable, constraint, and range constraint name strings into a contiguous memory pool. This reduces heap fragmentation from many small string allocations. The consolidation process calculates the maximum name string length, allocates a pool, copies all names into the pool, updates the name pointers, and frees the old scattered allocations.

- **Name uniqueness validation:** Perform the same hash-table-based duplicate name detection as in the fast path (Phase 4), checking variable names, constraint names, and range constraint names for uniqueness. Report an INVALID_ARGUMENT error if duplicates are found.

**Phase 8 -- Cleanup and Return:**

- **Success path:** Log success, reset the pending modifications buffer, update the model's state indicator by computing the current model type (LP, QP, etc.), free any temporary allocations, and return zero.

- **Error path:** Set the error message on the environment (an appropriate model-update error message), log the error code, clear the model's update-succeeded flag, clear the pending buffer to avoid reprocessing failed modifications, free temporary allocations, and return the error code.

Throughout all phases, a work counter on the model is incremented by the number of elements processed, providing a measure of computational work performed.

**Thread Safety:** Unsafe. The model must not be accessed concurrently during modification application. If the model is in remote solver mode, the function delegates to the remote solver synchronization subsystem, which handles its own synchronization.

**Dependencies:**
- SOS validation function
- Dimension reduction handler
- Various cache invalidation and cleanup functions (solution cache, callback state, internal vectors, solver state, presolve state)
- Memory allocation and deallocation (via the environment's allocator)
- Hash table creation, lookup, insertion, and destruction (for name uniqueness checking)
- String pool management (creation, allocation from pool, freeing)
- remote solver synchronization (when in remote mode)
- Error handling and logging subsystem
- Model state computation function

---

## Module-Level Behavioral Notes

### Relationship Between the Four Functions

The four functions cover three distinct lifecycle phases and a supporting cleanup operation:

| Phase | Function | Scope |
|-------|----------|-------|
| **Creation** | cxf_model_create_internal | Allocates and initializes a new Model |
| **Modification** | cxf_model_apply_modifications | Applies batched changes to an existing Model |
| **Cleanup** | cxf_env_model_cleanup | Cleans up child environments (environment-level teardown) |
| **Cleanup** | cxf_update_model_manager | Clears the model tracking array (environment-level teardown) |

The creation function produces a Model; the modification function maintains it during its active lifetime; and the two cleanup functions are invoked during environment destruction to tear down child environments and model tracking data, respectively.

### The Lazy Update Pattern

The lazy update pattern is a well-established optimization in commercial LP solver design. Rather than applying each individual user modification (add variable, add constraint, change coefficient, change bound) immediately -- which would require rebuilding sparse matrix indices, invalidating cached factorizations, and updating auxiliary structures after every change -- modifications are buffered in a pending modifications buffer. The buffer is flushed in a single batch by cxf_model_apply_modifications, either on explicit user request or automatically before optimization begins.

This pattern has three key benefits:
1. **Amortized rebuild cost:** Internal structures are rebuilt once regardless of how many modifications were buffered.
2. **Efficient compaction:** Deletions can be compacted in a single pass rather than requiring repeated array shifts.
3. **Consistent state:** The model is always in a consistent state -- either the original state or the fully updated state -- never in a partially modified state.

### Modification Categories

The modification applicator recognizes and handles the following categories of changes:

- **Variable additions and deletions:** Resize all per-variable arrays, compact deleted entries
- **Constraint additions and deletions:** Resize all per-constraint arrays, compact deleted entries, rebuild sparse matrix indices
- **Bound changes:** Update lower/upper bound arrays
- **Objective coefficient changes:** Update objective coefficient array
- **Variable type changes:** Update variable type array, recount type categories
- **RHS changes:** Update right-hand side array
- **Constraint sense changes:** Update sense array, negate affected coefficients and RHS for internal '<=' convention
- **Name changes:** Update name pointer arrays, trigger string pool consolidation above threshold
- **Quadratic constraint changes:** Add, delete, or modify quadratic constraint Q-matrices
- **General constraint changes:** Add, delete, or modify general constraints (indicator, PWL, etc.)
- **SOS constraint changes:** Add, delete, or modify SOS membership
- **Objective sense change:** Negate all objective data for min-to-max or max-to-min conversion
- **Solution hints:** Apply or update hint data

### Reference Counting in Environment Cleanup

The child environment cleanup function uses a reference counting scheme to safely manage shared environments. When a child environment is created, it increments the reference count on its root environment. During cleanup, the reference count is decremented under mutex protection. The environment is only freed when its reference count reaches zero, ensuring that no other entity holds a dangling reference. Environments that are still referenced when cleanup is requested have their cleanup deferred with a logged warning.

### Internal Storage Convention for Constraint Senses

The solver internally normalizes all constraints to '<=' form. When a user specifies a '>=' constraint, the coefficients and RHS for that constraint are stored in negated form. The sense array preserves the user's original direction. When constraint senses are changed via modification, the affected coefficients and RHS values must be negated or un-negated accordingly. This negation uses the IEEE 754 sign-bit flip (XOR with the sign bit), which is exact, handles infinity and NaN correctly, and is computationally cheaper than floating-point multiplication. This technique is standard in numerical software (see Goldberg, "What Every Computer Scientist Should Know About Floating-Point Arithmetic", ACM Computing Surveys, 1991).

### String Pool Consolidation

After many name modifications, the solver may have many small, scattered string allocations from individual variable and constraint name changes. When the cumulative name update count exceeds a threshold, the modification applicator consolidates all name strings into a single contiguous memory pool. This reduces heap fragmentation and improves cache locality for name lookups. The consolidation computes the maximum name length, allocates a pool sized as a multiple of the maximum length, copies all names into the pool, updates the name pointers, and frees the old storage.

---

## Verification Checklist

```
[x] No hex addresses or offsets
[x] No Ghidra artifacts (param_, local_, iVar, DAT_, FUN_)
[x] No binary-specific constants or magic numbers
[x] No copied code fragments
[x] All descriptions are behavioral, not implementational
[x] All data structures described semantically using Layer 1 types
[x] Algorithms reference standard published techniques where applicable
[x] Constants described by algorithmic role, not numeric value
[x] Passes the Clean Room Test
```
