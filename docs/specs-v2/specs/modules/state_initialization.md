# Module: State Initialization

## Purpose

This module provides the functions that prepare model-level state at the beginning of an optimization call. It encompasses three concerns: initializing the solver's runtime environment (clearing flags, recording timestamps, adjusting tolerances), freeing stale warm-start basis data when a reset has been requested, and tearing down the solution output container (SolutionData) to prepare for fresh results. Despite the "setup" naming convention in some of these functions, two of the three actually perform cleanup operations that establish a clean slate for the next optimization attempt. This reflects the solver's lifecycle pattern: "initialization" at the optimization boundary often means "clear previous results."

## Functions

### cxf_init_solve_state

**Purpose:** Prepare the model's environment and timing state at the very beginning of an optimization call, before the actual solver algorithm runs.

**Signature:**
- Input: model : pointer-to-Model - The model about to be optimized
- Input: threadLocalData : pointer-to-opaque - Thread-local timing and state data for the current worker thread
- Output: void

**Preconditions:**
- The model must be a valid, initialized Model with a non-null environment reference.
- The model must have passed validation checks (modification not blocked, etc.).
- The environment must be active.

**Postconditions:**
- The environment's termination flag is cleared (set to zero), allowing the solver to run without a premature stop signal.
- The model's solve-duration, work-rate, and total-work fields are cleared to zero.
- If a model manager (callback state container) is present in the environment, its timestamp is set to the current time and its counter is reset to zero.
- The environment's objective offset tolerance is saved (for later restoration) and then adjusted by a small perturbation proportional to the current tolerance magnitude and the number of constraints in the model. This perturbation prevents tolerance drift across repeated solves.
- If memory counting is disabled in the master environment but a memory limit parameter is set, a warning message is logged indicating the limit cannot be enforced.
- Thread-local memory tracking is initialized for the current thread.

**Side Effects:**
- Writes to the environment's termination flag, objective offset tolerance, and saved tolerance fields.
- Writes to the model's timing/statistics fields.
- Writes to the model manager's timestamp and counter fields (if present).
- May log a warning message to the environment's log output.
- Initializes thread-local memory pool tracking.

**Error Conditions:**
- None. This function does not return an error code. It operates defensively, checking for null pointers before accessing optional structures.

**Behavioral Description:**
cxf_init_solve_state is called once at the top of each optimization invocation to establish a clean starting state. It clears the user-accessible termination flag so that the solver does not immediately stop. It zeros the model's solve-performance counters (duration, work rate, total work). It records the solve start time in the model manager for progress reporting. It applies a small perturbation to the objective offset tolerance -- a standard numerical hygiene technique that prevents repeated solves from accumulating numerical bias. The perturbation direction depends on whether the model has constraints (positive adjustment) or not (negative adjustment), and a dampening multiplier prevents the tolerance from drifting too far from its original value. Finally, it checks whether the memory limit parameter can actually be enforced and warns the user if not.

**Thread Safety:** unsafe -- Must be called from a single thread before the solve begins. The function modifies environment-level state that is not protected by locks.

**Dependencies:**
- cxf_get_timestamp (timing module) -- retrieves the current timestamp
- cxf_log (error/logging module) -- logs warning messages
- cxf_get_memory_pool_id, cxf_init_thread_memory (memory module) -- thread-local memory setup

---

### cxf_free_warmstart_basis

**Purpose:** Free the warm-start basis data structure associated with the model, deallocating all owned arrays and nested sub-structures.

**Naming history:** Formerly `cxf_setup_basis`; renamed to better reflect its actual behavior as a destructor rather than an initialization function.

**Signature:**
- Input: env : pointer-to-Environment - The environment for memory tracking during deallocation
- Input: warmStartDataRef : pointer-to-pointer-to-WarmStartData - Double pointer to the warm-start data structure (allows the function to null the caller's reference after freeing)
- Output: void

**Preconditions:**
- The environment pointer must be valid (non-null) for memory tracking.
- The warmStartDataRef may be null or may point to a null pointer; both cases are handled gracefully.

**Postconditions:**
- If the WarmStartData structure existed:
  - All owned arrays are freed: variable basis status array, constraint basis status array, and primal/dual solution values array.
  - If a factorization cache sub-structure existed within the warm-start data, its index array and value array are freed, followed by the sub-structure itself.
  - The WarmStartData structure itself is freed.
  - The caller's pointer (the value pointed to by warmStartDataRef) is set to null.
- If the WarmStartData structure did not exist (null pointer), the function returns immediately with no effect.

**Side Effects:**
- Deallocates memory through the environment's memory management system.
- Nulls the caller's pointer to the WarmStartData structure, preventing use-after-free.
- Nulls all internal pointer fields within the structure as they are freed (defensive pattern against double-free).

**Error Conditions:**
- None. This function does not return an error code. Null pointer inputs are handled gracefully.

**Behavioral Description:**
cxf_free_warmstart_basis (which is functionally cxf_free_warmstart_basis) performs an inside-out deallocation of the WarmStartData structure. It first frees the leaf-level arrays (variable basis status, primal/dual values, constraint basis status), then frees the nested factorization cache sub-structure (its index array, its value array, then the sub-structure itself), then frees the main WarmStartData structure, and finally nulls the caller's reference pointer. Each pointer is null-checked before freeing, and each pointer is set to null after freeing, following the solver's standard defensive memory management pattern.

**Deallocation Order:**
1. Variable basis status array (within WarmStartData)
2. Primal/dual solution values array (within WarmStartData)
3. Constraint basis status array (within WarmStartData)
4. Factorization cache index array (within nested sub-structure)
5. Factorization cache value array (within nested sub-structure)
6. Factorization cache sub-structure itself
7. WarmStartData structure itself
8. Caller's reference pointer set to null

**Thread Safety:** unsafe -- Assumes single-threaded access to the WarmStartData structure.

**Dependencies:**
- Memory primitives module (cxf_free) -- for each individual deallocation

---

### cxf_free_work_arrays

**Purpose:** Free the SolutionData (solution output container) structure from the model, invalidating cached attributes and deallocating all owned arrays before removing the structure.

**Naming history:** Formerly `cxf_setup_work_arrays`; renamed to better reflect its actual behavior as a destructor/reset function rather than an initialization function.

**Signature:**
- Input: model : pointer-to-Model - The model whose SolutionData structure should be freed
- Output: void

**Preconditions:**
- The model may be null; if so, the function returns immediately.
- If the model is non-null but has no SolutionData structure, the function returns immediately.

**Postconditions:**
- The model's attribute cache has been invalidated, breaking any wired pointers from the attribute table into SolutionData fields.
- All owned arrays within the SolutionData structure have been freed (primal solution array, dual solution array).
- Borrowed pointers within the structure (e.g., range dual and SOS dual aliases into the dual array) have been cleared without being independently freed.
- Any unrecoverable error state associated with the model has been cleaned up.
- The SolutionData structure itself has been freed.
- The model's reference to the SolutionData structure has been set to null.

**Side Effects:**
- Invalidates the model's attribute cache (must occur first, before any field deallocation, to prevent dangling pointers in the attribute table).
- Deallocates memory through the environment's memory management system.
- Clears borrowed pointer fields without freeing them (these alias into other allocations).
- Invokes the unrecoverable state cleanup helper.
- Nulls the model's SolutionData pointer.

**Error Conditions:**
- None. This function does not return an error code. Null model and null SolutionData cases are handled gracefully.

**Behavioral Description:**
cxf_free_work_arrays performs a controlled teardown of the model's solution output container. The first action is attribute cache invalidation, which must precede any deallocation because the attribute table may hold direct pointers into SolutionData fields. After invalidation, the function frees the two owned arrays (solution index array and solution value array), clears two borrowed pointer fields without freeing them (these are aliases into allocations owned by other structures), invokes the unrecoverable state cleanup helper to handle any orphaned allocations, and finally frees the SolutionData structure itself and nulls the model's reference.

**Deallocation Order:**
1. Attribute cache invalidation (must be first)
2. Owned solution index array (within SolutionData)
3. Owned solution value array (within SolutionData)
4. Borrowed pointers cleared (not freed)
5. Unrecoverable state cleanup
6. SolutionData structure itself
7. Model's SolutionData reference set to null

**Thread Safety:** unsafe -- Assumes single-threaded access to the model and its SolutionData.

**Dependencies:**
- cxf_clear_attr_cache (attribute module) -- invalidates the attribute table's cached pointers
- Memory primitives module (cxf_free) -- for each individual deallocation
- cxf_free_unrecoverable (memory module) -- cleans up orphaned error-recovery allocations

---

## Module-Level Notes

### Naming History

Two of the three functions in this module have been renamed to correct historical misnomers:

| Current Name | Former Name | Reason for Rename |
|--------------|-------------|-------------------|
| cxf_free_warmstart_basis | cxf_setup_basis | Original name suggested initialization but function performs deallocation |
| cxf_free_work_arrays | cxf_setup_work_arrays | Original name suggested initialization but function performs cleanup |

The original naming reflected the perspective that "setting up" for a new optimization means "clearing out" the previous optimization's residual state. The current names accurately describe the actual behavior.

### Calling Context

These three functions are called early in the optimization pipeline:

1. **cxf_init_solve_state** is called at the very beginning of the optimization entry point, after model validation but before solver dispatch.
2. **cxf_free_warmstart_basis** is called conditionally when an environment parameter indicates that warm-start data should be cleared before re-optimization.
3. **cxf_free_work_arrays** is called on error recovery paths during optimization and during solution clearing operations.

### Relationship to Cleanup Module (P3.04)

The functions in this module overlap conceptually with the cleanup module (P3.04). Both modules contain functions that free solver-related structures. The distinction is:
- **This module (P3.03)** contains functions called at the *beginning* of optimization to establish a clean slate.
- **The cleanup module (P3.04)** contains functions called at the *end* of optimization or during model destruction to release solver resources.

## References

- Maros, I. (2003). *Computational Techniques of the Simplex Method*. Springer. (Chapter 2: model management and solve lifecycle)
- Dantzig, G.B. (1963). *Linear Programming and Extensions*. Princeton University Press.

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
