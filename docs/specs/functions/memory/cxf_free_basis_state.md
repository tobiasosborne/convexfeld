# cxf_free_basis_state

**Module:** Memory Management
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Deallocate a basis state structure and its constituent arrays. Basis state structures store snapshots of the simplex basis for warm starts, sensitivity analysis, and basis comparison. This function provides focused cleanup of the relatively small basis snapshot (compared to full SolverState).

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| basis | BasisState* | Pointer to basis state to free | NULL or valid BasisState pointer | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | void | No return value |

### 2.3 Side Effects

- Frees variable status array
- Frees basis header array
- Frees optional variable values array
- Frees optional dual values array
- Frees optional name string
- Frees BasisState structure itself

## 3. Contract

### 3.1 Preconditions

- [ ] If basis is not NULL: must be valid BasisState pointer
- [ ] All constituent arrays allocated via cxf_ allocation functions

### 3.2 Postconditions

- [ ] All arrays within basis are freed
- [ ] BasisState structure itself is freed
- [ ] No memory leaks from basis snapshot

### 3.3 Invariants

- [ ] Function is NULL-safe
- [ ] Optional arrays (values, duals, name) are freed only if present

## 4. Algorithm

### 4.1 Overview

The function performs hierarchical deallocation of a basis snapshot. It checks and frees the always-present arrays (variable status, basis header), then conditionally frees optional arrays (variable values, dual values, name string) if they were allocated, and finally frees the container structure.

### 4.2 Detailed Steps

1. Check if basis is NULL - if so, return immediately
2. Free variable status array (if not NULL)
3. Free basis header array (if not NULL)
4. Free variable values array (if not NULL - optional)
5. Free dual values array (if not NULL - optional)
6. Free basis name string (if not NULL - optional)
7. Free BasisState structure itself
8. Return (void)

### 4.3 Pseudocode (if needed)

```
PROCEDURE cxf_free_basis_state(basis)
  IF basis = NULL THEN
    RETURN
  END IF

  free_if_not_null(basis.varStatus)
  free_if_not_null(basis.basisHeader)
  free_if_not_null(basis.varValues)      # Optional
  free_if_not_null(basis.dualValues)     # Optional
  free_if_not_null(basis.name)           # Optional

  cxf_free(basis)

  RETURN
END PROCEDURE
```

## 5. Complexity

### 5.1 Time Complexity

- **All cases:** O(1) - fixed number of frees

### 5.2 Space Complexity

- **Auxiliary space:** O(1)
- **Total space:** O(-(numVars + numConstrs)) - typical basis size

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| basis is NULL | None (no-op) | Safe to call with NULL |

### 6.2 Error Behavior

No error return. NULL input handled safely.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| NULL basis | basis=NULL | Return immediately |
| Minimal basis | Only status and header | Free those 2 arrays + structure |
| Full basis | All fields allocated | Free all 5 arrays + structure |

## 8. Thread Safety

**Thread-safe:** Conditionally - no internal synchronization, caller must ensure exclusive access.

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_free | Memory | Free each array and structure |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_freemodel | API | Free saved basis during model cleanup |
| Warm start functions | Solver | Cleanup temporary basis snapshots |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_basis_snapshot | Inverse - creates basis state |
| cxf_getbasis | API function returning basis data |
| cxf_setbasis | API function setting basis data |

## 11. Design Notes

### 11.1 Design Rationale

Separate basis state structure enables:
- **Warm starts** - Save optimal basis for next solve
- **Basis comparison** - Detect basis changes
- **Sensitivity** - Perturb and resolve from saved basis

Much smaller than full SolverState (~60-180KB vs MB-GB).

### 11.2 Performance Considerations

- Very fast: ~5 free calls
- Typical time: <1 microsecond
- Much simpler than cxf_free_solver_state

### 11.3 Future Considerations

- Reference counting for shared basis states
- Compressed basis storage for very large problems

## 12. References

- Linear programming basis theory
- Warm start techniques in optimization

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
