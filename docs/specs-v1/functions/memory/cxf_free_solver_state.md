# cxf_free_solver_state

**Module:** Memory Management
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Comprehensive deallocation of SolverState structure and all constituent arrays. SolverState is the main working data structure for simplex solver containing matrix data, basis information, working arrays, and algorithm state. This function performs complete cleanup of the multi-level allocation hierarchy totaling potentially gigabytes of memory.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| state | SolverState* | Pointer to solver state to free | NULL or valid SolverState pointer | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | void | No return value |

### 2.3 Side Effects

- Frees 40+ constituent arrays (matrix, bounds, scaling, etc.)
- Frees eta vector chains
- Frees auxiliary structures (timing, pricing)
- Frees SolverState structure itself
- All memory associated with solver state released

## 3. Contract

### 3.1 Preconditions

- [ ] If state is not NULL: must be valid SolverState pointer
- [ ] All constituent arrays allocated via cxf_ allocation functions
- [ ] State has not been freed previously (no double-free)

### 3.2 Postconditions

- [ ] All memory in allocation hierarchy is freed
- [ ] No memory leaks from nested structures
- [ ] State pointer becomes invalid (caller must not use)

### 3.3 Invariants

- [ ] Function is NULL-safe (no-op if state is NULL)
- [ ] All allocations freed regardless of which are present

## 4. Algorithm

### 4.1 Overview

The function implements systematic hierarchical deallocation. It checks each field in SolverState for non-NULL pointers and frees them in logical groups (constraint matrix CSC/CSR, basis data, scaling, bounds, quadratic data, piecewise linear data, working arrays, eta structures, auxiliary data). Special handling for linked structures (eta chains) requires recursive freeing. Finally, the SolverState structure itself is freed.

### 4.2 Detailed Steps

1. Check if state is NULL - if so, return immediately
2. Free constraint matrix (CSC format): colStart, colCount, rowIndices, values
3. Free constraint matrix (CSR format): rowStart, rowCount, colIndices, rowValues
4. Free basis data: varStatus, basisHeader, workBuffers
5. Free scaling arrays: rowScaling, colScaling
6. Free dual/working arrays: dualValues1, dualValues2
7. Free bounds/objective: lb, ub, objCoeffs, rhs, sense
8. Free variable data: varTypes, varFlags
9. Free quadratic data (if present): diagQ, qRowCount, qColIndices, qValues
10. Free piecewise linear data (if present): pwlStart, pwlCount, pwlValues, pwlSlopes, pwlThresh
11. Free eta structures: Walk eta chain freeing each node, free eta buffer pool
12. Free working arrays: tempIndices, working bounds, selection flags, temp values
13. Free auxiliary structures: timingState, pricingState
14. Free SolverState structure itself
15. Return (void)

### 4.3 Pseudocode (if needed)

```
PROCEDURE cxf_free_solver_state(state)
  IF state = NULL THEN
    RETURN
  END IF

  # Free matrix data (CSC)
  free_if_not_null(state.colStart)
  free_if_not_null(state.colCount)
  free_if_not_null(state.rowIndices)
  free_if_not_null(state.values_csc)

  # Free matrix data (CSR)
  free_if_not_null(state.rowStart)
  free_if_not_null(state.rowCount)
  free_if_not_null(state.colIndices)
  free_if_not_null(state.values_csr)

  # Free basis and working arrays
  free_if_not_null(state.varStatus)
  free_if_not_null(state.basisHeader)
  # ... (many more arrays)

  # Free eta vector chain (recursive)
  IF state.etaListHead ≠ NULL THEN
    free_eta_chain(state.etaListHead)
  END IF

  # Free eta buffer pool
  IF state.etaBuffer ≠ NULL THEN
    free_eta_buffer(state.etaBuffer)
  END IF

  # Free auxiliary structures
  free_if_not_null(state.timingState)
  free_if_not_null(state.pricingState)

  # Free main structure
  cxf_free(state)

  RETURN
END PROCEDURE
```

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) - state is NULL
- **Average case:** O(N) where N = number of eta vectors in chain
- **Worst case:** O(N) for eta chain traversal

Where:
- Most frees are O(1)
- Eta chain requires O(N) traversal

### 5.2 Space Complexity

- **Auxiliary space:** O(1) - no additional allocation
- **Total space:** O(-(state size)) - releases all memory

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| state is NULL | None (no-op) | Safe to call with NULL |
| Double-free | Undefined | Freeing same state twice (caller error) |

### 6.2 Error Behavior

No error return. NULL input handled safely. Double-free results in undefined behavior.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| NULL state | state=NULL | Return immediately, no-op |
| Empty state | All arrays NULL | Free only structure |
| Partial state | Some arrays NULL | Free only allocated arrays |
| Full state | All arrays allocated | Free everything |

## 8. Thread Safety

**Thread-safe:** Conditionally

No internal synchronization. Caller must ensure exclusive access.

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_free | Memory | Free each array and structure |
| cxf_free_eta_buffer | Memory | Free eta buffer pool |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_freemodel | API | Free solver state during model cleanup |
| cxf_optimize | API | Cleanup on error during solve |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_simplex_init | Inverse - allocates SolverState |
| cxf_simplex_final | Similar - normal cleanup after solve |
| cxf_free_basis_state | Simpler - frees basis snapshot only |

## 11. Design Notes

### 11.1 Design Rationale

Comprehensive cleanup ensures:
- No memory leaks
- Clean environment shutdown
- Error recovery without resource loss

The systematic approach (check every field, free if not NULL) is robust against partial initialization.

### 11.2 Performance Considerations

- Fast operation: <1 millisecond even for large problems
- Most time in system free() calls
- No algorithmic complexity beyond linear eta chain

### 11.3 Future Considerations

- Add memory usage logging before freeing
- Debug mode: Validate magic numbers before freeing
- Statistics: Track peak memory usage

## 12. References

- Memory management in optimization solvers
- Hierarchical resource cleanup patterns

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
