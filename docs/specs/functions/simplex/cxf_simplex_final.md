# cxf_simplex_final

**Module:** Simplex Core
**Spec Version:** 1.0

## 1. Purpose

Performs final cleanup of the simplex solver state, freeing all allocated memory and resources. This is the last function called in the LP solve sequence, ensuring no memory leaks. Must handle partially initialized states in case of early termination due to errors.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| state | SolverState* | Solver state to clean up | May be partially initialized | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | void | No return value |

### 2.3 Side Effects

- Frees all memory in solver state
- Invalidates state pointer

## 3. Contract

### 3.1 Preconditions

- [ ] State pointer is valid (may be partially initialized)

### 3.2 Postconditions

- [ ] All allocated memory freed
- [ ] No memory leaks

## 4. Algorithm

### 4.1 Overview

Free all arrays in reverse allocation order, checking for NULL before each free.

### 4.2 Detailed Steps

1. **Call cxf_simplex_unperturb** (if perturbation applied).

2. **Free pricing state**:
   - Free candidate arrays
   - Free weight arrays
   - Free pricing structure

3. **Free timing state**.

4. **Free eta vectors**:
   - Walk eta list, free each
   - Clear eta list head

5. **Free working arrays**:
   - Reduced costs
   - Dual values
   - Auxiliary buffer

6. **Free basis structures**:
   - Basis header
   - Variable status

7. **Free problem data copies**:
   - Bounds (original and working)
   - Objective
   - RHS
   - Senses
   - Variable types and flags

8. **Free scaling arrays**.

9. **Free quadratic arrays** (if allocated).

10. **Free main state structure**.

### 4.3 Pseudocode

```
FINAL(state):
    IF state == NULL:
        RETURN

    // Unperturb if needed
    cxf_simplex_unperturb(state, NULL)

    // Free pricing state
    IF state.pricingState != NULL:
        FREE_PRICING(state.pricingState)

    // Free timing state
    FREE(state.timingState)

    // Free eta list
    eta := state.etaHead
    WHILE eta != NULL:
        next := eta.next
        FREE(eta.values)
        FREE(eta.indices)
        FREE(eta)
        eta := next

    // Free working arrays
    FREE(state.reducedCosts)
    FREE(state.dualValues)
    FREE(state.auxBuffer)

    // Free basis structures
    FREE(state.basisHeader)
    FREE(state.varStatus)

    // Free problem data
    FREE(state.lb_original)
    FREE(state.ub_original)
    FREE(state.lb_working)
    FREE(state.ub_working)
    FREE(state.objCoeffs)
    FREE(state.rhs)
    FREE(state.senses)
    FREE(state.vtype)
    FREE(state.varFlags)

    // Free scaling
    FREE(state.rowScale)
    FREE(state.colScale)

    // Free quadratic (if any)
    IF state.hasQuadratic:
        FREE(state.qRowPtrs)
        FREE(state.qColStarts)
        FREE(state.qColEnds)

    // Free state
    FREE(state)
```

## 5. Complexity

- O(n + m + k) where k = number of eta vectors

## 6. Error Handling

None - handles NULL gracefully

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| NULL state | state = NULL | Return immediately |
| Partial init | Some arrays NULL | Skip NULL frees |
| Full state | All allocated | Free everything |

## 8. Thread Safety

**Thread-safe:** Yes (operates on single state)

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_simplex_unperturb | Simplex | Remove perturbations |
| cxf_free | Memory | Free memory blocks |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_simplex_init | Allocates what this frees |
| cxf_solve_lp | Calls this at end |

## 11. Design Notes

**Reverse order freeing:** Frees in reverse allocation order for cleaner memory management.

**NULL checks:** Every pointer checked before free for robustness.

## 12. References

- Standard C memory management practice

## 13. Validation Checklist

- [x] All criteria met

---

*Convexfeld LP Solver Specification*
*Based on published optimization literature*
