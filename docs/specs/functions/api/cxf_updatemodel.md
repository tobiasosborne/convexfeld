# cxf_updatemodel

**Module:** API Model Management
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Applies all pending model modifications that have been queued but not yet committed to the internal model representation. Convexfeld uses a lazy update mechanism where changes from functions like cxf_addvar, cxf_addconstr, cxf_setdblattrelement, and cxf_chgcoeffs are batched in a modification queue. This function processes the queue, rebuilding internal data structures to reflect the changes. This allows efficient batch modifications without the overhead of updating internal structures after every individual change.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| model | CxfModel* | The model to update | Valid model pointer | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | Error code: 0 on success, non-zero on failure |

### 2.3 Side Effects

- Applies all queued model modifications to internal structures
- Rebuilds CSC constraint matrix if structure changed
- Updates variable/constraint arrays with new values
- Invalidates any cached solution data
- Records operation status in model for error reporting
- May acquire and release internal locks

## 3. Contract

### 3.1 Preconditions

- [ ] Model pointer must be valid (not NULL, not freed)
- [ ] Model's environment must be valid and active
- [ ] Model may have pending modifications from prior API calls

### 3.2 Postconditions

On success:
- [ ] All pending modifications are applied to internal model
- [ ] Modification queue is cleared
- [ ] Internal CSC matrix reflects structural changes
- [ ] Variable/constraint arrays reflect value changes
- [ ] Model is ready for optimization or queries
- [ ] Operation status recorded in model

On failure:
- [ ] Model state is unchanged (rollback semantics)
- [ ] Pending modifications remain in queue
- [ ] Error code is returned
- [ ] Error details stored in model

### 3.3 Invariants

- [ ] Model validity is preserved
- [ ] If no pending changes, model state is unchanged
- [ ] Operation is idempotent (calling twice with no intermediate changes has no effect)

## 4. Algorithm

### 4.1 Overview

The function applies pending model modifications through a multi-phase process. First, it validates the model to ensure it's in a usable state. Then, it applies all pending modifications locally by processing the modification queue - this may include adding/removing variables or constraints, updating bounds/objectives/RHS values, changing coefficients, and rebuilding the CSC matrix structure. Finally, it records the operation status in the model for error reporting through cxf_geterrormsg.

The lazy update mechanism is a core Convexfeld design pattern that enables O(1) individual modifications followed by a single O(m + n + nnz) rebuild, rather than O(n) or O(m) cost per modification.

### 4.2 Detailed Steps

1. Validate model structure via cxf_checkmodel
2. If validation fails, skip to status recording and return error
3. Call cxf_model_apply_modifications to process all pending changes locally:
   - Process variable additions: allocate array space, extend CSC columns, initialize attributes
   - Process variable deletions: remove from arrays, compress CSC structure, renumber indices
   - Process constraint additions: extend row arrays, add nonzeros to CSC structure
   - Process constraint deletions: remove rows, compress CSC, renumber indices
   - Process bound changes: update lb/ub arrays
   - Process objective changes: update objective coefficient array
   - Process RHS changes: update RHS array
   - Process coefficient changes: modify CSC nonzero values or structure
   - Rebuild CSC matrix if structure changed (colStart, rowIndices, coeffValues)
   - Invalidate cached solution data (mark as unsolved)
   - Clear modification queue
4. Store the status code from apply_modifications
5. Record operation status in model via cxf_model_set_status (for error reporting)
6. Return final status code

### 4.3 Pseudocode

```
function cxf_updatemodel(model) → int:
    # Validate model
    status ← cxf_checkmodel(model)
    if status ≠ SUCCESS:
        goto finish

    # Apply all pending modifications locally
    status ← apply_modifications(model)
    # This is the core work:
    #   - Process addition/deletion queues
    #   - Update bounds, objectives, RHS
    #   - Rebuild CSC matrix
    #   - Invalidate solution

finish:
    # Record status for error reporting
    set_model_status(model, status)
    return status
```

### 4.4 Mathematical Foundation

Consider a model being built incrementally:

**Initial state:**
```
minimize    c₀^T x₀
subject to  A₀ x₀ ≤ b₀
            lb₀ ≤ x₀ ≤ ub₀
```

**After queued modifications (not yet applied):**
- Add variables: x₁, x₂, ..., xₖ with costs c₁, c₂, ..., cₖ
- Add constraints: Aₙ xₙ ≤ bₙ for new rows
- Change bounds: lb'ᵢ, ub'ᵢ for some variables
- Change coefficients: A'ᵢⱼ for some matrix entries

**After cxf_updatemodel:**
```
minimize    c^T x
subject to  A x ≤ b
            lb ≤ x ≤ ub
```

Where:
- x = [x₀; x₁; ...; xₖ] (extended variable vector)
- c = [c₀; c₁; ...; cₖ] (extended cost vector)
- A = [A₀ Aₙ; ...] (extended constraint matrix)
- b = [b₀; bₙ] (extended RHS)
- lb, ub incorporate bound changes

The modification queue stores delta operations that are applied atomically to transform the initial state to the final state.

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) when no pending modifications
- **Average case:** O(m + n + nnz) when structural changes occur
- **Worst case:** O(m·n) for dense matrix modifications

Where:
- m = number of constraints
- n = number of variables
- nnz = number of nonzeros in constraint matrix

Most common scenarios:
- Adding k variables: O(k + nnz_new) where nnz_new = nonzeros in new columns
- Adding k constraints: O(k·n + nnz_new)
- Changing bounds only: O(k) where k = number of bound changes
- Changing coefficients: O(k) to O(m + n + nnz) depending on structure

### 5.2 Space Complexity

- **Auxiliary space:** O(m + n + nnz) for temporary arrays during CSC rebuild
- **Total space:** O(m + n + nnz) (existing model storage)

The function may allocate temporary storage for:
- Sorting coefficient changes
- Building new CSC structure
- Mapping old to new indices

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| Invalid model | Varies | cxf_checkmodel fails |
| Memory allocation failure | 1001 | Out of memory during rebuild |
| Invalid modifications | 1003 | Inconsistent or invalid queued changes |

### 6.2 Error Behavior

On error:
- Model state remains consistent (rollback semantics)
- Pending modifications are NOT applied (remain in queue)
- Operation can be retried after fixing the issue
- Status is recorded in model for error reporting
- Function returns non-zero error code

The function provides transactional semantics - either all modifications apply or none do.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| No pending changes | Already updated model | Returns success immediately (O(1)) |
| Empty model | No variables or constraints | Successfully applies additions |
| Only bound changes | No structural modifications | Fast path (O(k) updates only) |
| Large batch additions | Thousands of variables/constraints | Memory-intensive rebuild |
| Conflicting modifications | Invalid queue state | Error returned, queue preserved |
| Repeated calls | cxf_updatemodel called twice | Second call is no-op |

## 8. Thread Safety

**Thread-safe:** Conditionally

The model should not be accessed by other threads during update:
- Internal locks may be acquired during CSC rebuild
- Model state transitions from "pending" to "committed"
- Not safe to query or modify during update

However, other models in the same environment can be updated concurrently by other threads.

**Synchronization required:** None for caller (but model must not be in use)

The function handles internal synchronization. Callers must ensure exclusive access to the model.

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_checkmodel | Validation | Validates model pointer and state |
| cxf_model_apply_modifications | Model | Processes modification queue (core work) |
| cxf_model_set_status | Model | Records operation status |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| User code | N/A | Explicit update after modifications |
| cxf_optimize | Solver | Implicit update before optimization |
| cxf_write | I/O | Implicit update before writing model |
| cxf_copymodel | Model | Explicit update if requested |
| Attribute getters | Attributes | Implicit partial update for queries |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_addvar | Queues variable addition (requires update) |
| cxf_addvars | Queues multiple variable additions |
| cxf_addconstr | Queues constraint addition |
| cxf_addconstrs | Queues multiple constraint additions |
| cxf_delvars | Queues variable deletion |
| cxf_delconstrs | Queues constraint deletion |
| cxf_setdblattrelement | Queues bound/objective/RHS change |
| cxf_chgcoeffs | Queues coefficient change |
| cxf_optimize | Calls update implicitly |
| cxf_write | Calls update implicitly |

## 11. Design Notes

### 11.1 Design Rationale

**Lazy update design:** Convexfeld's lazy update mechanism is a key performance optimization. Consider building a model with 1000 variables and 500 constraints:

Without lazy update (hypothetical):
```c
for (int i = 0; i < 1000; i++) {
    cxf_addvar(...);  // Rebuild CSC matrix each time: O(i·m) per call
}
// Total: O(n²·m) = O(1000²·500) = 500 million operations
```

With lazy update (actual Convexfeld):
```c
for (int i = 0; i < 1000; i++) {
    cxf_addvar(...);  // Just queue: O(1) per call
}
cxf_updatemodel(...);  // Single rebuild: O(n·m + nnz)
// Total: O(n + n·m + nnz) = much faster
```

### 11.2 Performance Considerations

**When to call explicitly:**
- After batch modifications, before querying new variable/constraint indices
- After cxf_addvar, to get the new variable's index
- Before accessing newly added element attributes

**When NOT to call:**
- Before cxf_optimize (it updates automatically)
- Before cxf_write (it updates automatically)
- After every single modification (defeats lazy update purpose)

**Performance tips:**
- Batch all modifications before calling update
- Avoid interleaving modifications and queries
- Use cxf_addvars instead of many cxf_addvar calls

### 11.3 Future Considerations

- Separate queues for additions, deletions, and changes
- Hash tables for fast coefficient lookup
- Compression for repeated bound changes to same variable

The CSC rebuild algorithm must handle:
- Preserving nonzero ordering within columns
- Efficient reallocation when size changes
- Minimizing memory fragmentation

## 12. References

- Convexfeld Optimizer Reference Manual: Lazy Update Mechanism
- "The Design and Implementation of Optimization Software" - Gill, Murray, Wright
- Sparse Matrix Storage Formats: CSC/CSR documentation

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
