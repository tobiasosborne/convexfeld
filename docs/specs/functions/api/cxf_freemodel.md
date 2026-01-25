# cxf_freemodel

**Module:** API Model Management
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Releases all memory and resources associated with a Convexfeld optimization model. After this call, the model pointer becomes invalid and must not be used. This is the cleanup function that should be called for every model created with cxf_newmodel or cxf_copymodel before freeing the parent environment.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| model | CxfModel* | Model to free | Valid model pointer or NULL | No |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | Error code: 0 on success, non-zero on failure |

### 2.3 Side Effects

- Frees all memory allocated for the model (variables, constraints, solution data, etc.)
- Updates environment's model tracking to remove this model
- Unregisters all callbacks
- Invalidates the model pointer
- May log debug messages if environment has debug logging enabled
- May release environment lock if model held it

## 3. Contract

### 3.1 Preconditions

- [ ] If model is not NULL, it must be a valid model pointer
- [ ] Model must not be freed more than once (double-free)
- [ ] It is safe to call with model = NULL (no-op)

### 3.2 Postconditions

On success:
- [ ] All memory associated with the model is freed
- [ ] Environment's model tracking no longer references this model
- [ ] Model validity marker is set to 0
- [ ] All callbacks are unregistered
- [ ] Environment lock is released if model held it

On failure (validation error):
- [ ] No changes made to system state
- [ ] Error code returned

### 3.3 Invariants

- [ ] Environment structure remains valid
- [ ] Other models in the same environment are unaffected
- [ ] Total freed memory equals total allocated memory for this model

## 4. Algorithm

### 4.1 Overview

The function performs systematic deallocation of all model resources in a specific order to respect dependencies. First, it validates the model pointer and performs debug logging if enabled. Second, it unregisters all user callbacks to prevent dangling function pointers. Third, it frees dynamic data structures like vector containers and internal arrays. Fourth, it frees the main matrix data structures containing variables and constraints. Fifth, it frees solver-specific state including IIS data, basis information, and solution pools. Sixth, it updates the environment's model tracking to remove references to this model. Finally, it frees the model wrapper structure itself and releases any held locks.

The cleanup order is critical: child structures must be freed before parent structures, and callback resources must be released before local memory to avoid use-after-free issues.

### 4.2 Detailed Steps

1. Check if model pointer is NULL; if so, return success immediately (NULL-safe)
2. Validate model structure to ensure it's a valid Convexfeld model
3. Retrieve environment pointer from model
4. If debug logging is enabled in environment, log "MODEL LOG: cxf_freemodel {address}"
5. Save model's environment flag for later use in lock management
6. If model has registered callbacks (callbackCount > 0):
   - Unregister all callback functions
   - Free callback state data
8. Free all vector container structures (7 internal dynamic arrays)
9. Free individual allocated array pointers (approximately 20+ arrays for various data)
10. Free primary matrix data structure (variables, constraints, CSC format data)
11. Free working matrix data structure (used during solving)
12. Free SOS (Special Ordered Set) data if present
13. Free general constraint data if present
14. Free IIS (Irreducible Inconsistent Subsystem) state
15. Free basis state information
16. Free solution pool (multiple solutions if available)
17. Free general solver state
18. Set model validity marker to 0 to detect use-after-free
18. Update environment's model manager:
    - Clear currentModel reference if it points to this model
    - Clear activeModel reference if it points to this model
    - Update active model tracking
19. Free the model wrapper structure itself
20. If model held environment lock (saved flag != 0), release it
21. Return success code (0)

### 4.3 Pseudocode

```
function cxf_freemodel(model):
    # NULL check - safe no-op
    if model == NULL:
        return SUCCESS

    # Validation
    status ← cxf_checkmodel(model)
    if status ≠ SUCCESS:
        return status

    # Debug logging
    env ← model.env
    if env ≠ NULL and env.debugLogEnabled ≠ 0:
        log(env, "MODEL LOG: cxf_freemodel %p", model)
        if model == NULL:  # Paranoid re-check after logging
            return SUCCESS

    # Save lock state
    had_env_flag ← model.envFlag

    # Free callback resources
    if model.callbackCount > 0:
        free_callback_state(model)

    # Free vector containers (internal dynamic arrays)
    for each vector_ptr in model.vectors[0..6]:
        if vector_ptr ≠ NULL:
            free_vector(vector_ptr)
            vector_ptr ← NULL

    # Free individual arrays (callback arrays, solution arrays, etc.)
    # [Multiple individual pointer fields freed]

    # Free matrix structures
    if model.primaryMatrix ≠ NULL:
        free_matrix_data(env, model.primaryMatrix)
        model.primaryMatrix ← NULL

    if model.workMatrix ≠ NULL:
        free_matrix_data(env, model.workMatrix)
        model.workMatrix ← NULL

    if model.sosData ≠ NULL:
        free(env, model.sosData)
        model.sosData ← NULL

    if model.genConstrData ≠ NULL:
        free(env, model.genConstrData)
        model.genConstrData ← NULL

    # Free solver state
    free_iis_state(model)
    free_basis_state(model)
    free_solution_pool(model)
    free_solver_state(model)

    # Mark invalid
    model.valid ← 0

    # Update environment tracking
    if env ≠ NULL:
        mgr ← env.modelManager
        if mgr ≠ NULL:
            if mgr.currentModel == model:
                mgr.currentModel ← NULL
            if mgr.activeModel == model:
                update_active_model(env, mgr)

    # Free model structure
    free(env, model)

    # Release lock if held
    if had_env_flag ≠ 0:
        release_env_lock(env)

    return SUCCESS
```

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) when model is NULL
- **Average case:** O(m + n + nnz) for a model with data
- **Worst case:** O(m + n + nnz + s) with solution pool

Where:
- m = number of constraints
- n = number of variables
- nnz = number of nonzeros in constraint matrix
- s = total size of solution pool

### 5.2 Space Complexity

- **Auxiliary space:** O(1) - no additional allocations during freeing
- **Total space:** Frees O(m + n + nnz + s) memory

The function deallocates all memory used by the model but requires no temporary storage.

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| NULL model | 0 | Returns success immediately (safe) |
| Invalid model | Varies | cxf_checkmodel fails, returns error code |

### 6.2 Error Behavior

The function is designed to be robust:
- NULL pointer is explicitly handled and returns success (idempotent)
- Invalid model structure returns error without attempting to free
- All internal free operations handle NULL pointers gracefully
- No partial cleanup - either full cleanup or no cleanup
- Cannot leave system in inconsistent state

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| NULL model | model = NULL | Returns 0 immediately (safe no-op) |
| Empty model | numvars = 0, numconstrs = 0 | Frees minimal structures, returns success |
| Model with callbacks | callbackCount > 0 | Unregisters callbacks before freeing |
| Model with solution pool | Multiple solutions stored | Frees all solutions |
| Model holding env lock | envFlag != 0 | Releases lock after all cleanup |
| Already freed model | model.valid = 0 | cxf_checkmodel catches, returns error |
| Active model | env.activeModel points to this | Updates env tracking to NULL |

## 8. Thread Safety

**Thread-safe:** Conditionally

The function itself does not acquire locks, but:
- The model being freed should not be used by other threads
- The environment remains valid and can be used by other threads
- If the model held an environment lock, it's released atomically
- Other models in the same environment are unaffected

**Synchronization required:** None (but model must not be in use)

Callers must ensure the model is not being accessed by other threads during freeing. This is typically guaranteed by application design (each thread owns its models).

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_checkmodel | Validation | Validates model pointer |
| cxf_log | Logging | Debug logging if enabled |
| cxf_free_callback_state | Callbacks | Unregisters callbacks |
| cxf_vector_free | Memory | Frees vector containers |
| cxf_free_matrix_data | Memory | Frees matrix structures |
| cxf_free_iis_state | Solver | Frees IIS analysis data |
| cxf_free_basis_state | Solver | Frees basis information |
| cxf_free_solution_pool | Solver | Frees multiple solutions |
| cxf_free_solver_state | Solver | Frees general solver state |
| cxf_env_update_active_model | Environment | Updates environment tracking |
| cxf_env_release_lock | Threading | Releases environment lock |
| cxf_free | Memory | Environment-scoped deallocation |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| User code | N/A | Primary cleanup API |
| Error handlers | Various | Clean up on failed operations |
| cxf_freeenv | Environment | Frees all models before environment |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_newmodel | Inverse operation - creates model |
| cxf_copymodel | Creates model that also needs freeing |
| cxf_freeenv | Frees environment (must be called AFTER freeing all models) |

## 11. Design Notes

### 11.1 Design Rationale

The cleanup order is carefully designed to respect dependencies:
1. User callbacks first - may reference model data
2. Dynamic containers second - owned by model
3. Matrix data third - contains bulk of memory
4. Solver state fourth - may reference matrix
5. Environment tracking fifth - updates parent
6. Model structure last - container for all above

This ordering prevents use-after-free and ensures complete cleanup.

### 11.2 Performance Considerations

Environment-scoped allocation allows efficient bulk deallocation. Instead of tracking each individual allocation, the environment maintains memory pools. Freeing a model can deallocate an entire pool in O(1) time if it's the last model using that pool.


### 11.3 Future Considerations

The validity marker (model.valid = 0) provides a safety net for detecting use-after-free bugs. Other functions can check this marker before dereferencing the model.

- Fast lookup of which model is being optimized
- Detection of dangling model references
- Support for model switching in advanced scenarios

## 12. References

- Convexfeld Optimizer Reference Manual: Model Management
- "The C Programming Language" - Kernighan & Ritchie: Resource management patterns

## 13. Validation Checklist

- [x] No code copied from implementation
- [x] Algorithm description is implementation-agnostic
- [x] All parameters documented
- [x] All error conditions listed
- [x] Complexity analysis complete
- [x] Edge cases identified
- [x] A competent developer could implement from this spec alone

---

*Reviewed by: Pending*

---
*Convexfeld LP Solver Specification*
*Based on published optimization literature*
