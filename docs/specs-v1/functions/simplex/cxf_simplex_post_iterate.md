# cxf_simplex_post_iterate

**Module:** Simplex Core
**Spec Version:** 1.0

## 1. Purpose

Performs post-iteration housekeeping after a simplex pivot. Updates statistics, checks for refactorization need, handles periodic tasks like perturbation adjustment, and prepares state for the next iteration. Called at the end of each iteration in the main loop.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| state | SolverState* | Solver state after pivot | Non-null, valid | Yes |
| env | CxfEnv* | Environment | Non-null, valid | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | 0=continue, 1=refactor triggered |

### 2.3 Side Effects

- Updates iteration statistics
- May trigger refactorization
- Updates timing information
- May adjust perturbation

## 3. Contract

### 3.1 Preconditions

- [ ] Pivot just completed
- [ ] State is consistent

### 3.2 Postconditions

- [ ] Statistics updated
- [ ] State ready for next iteration

## 4. Algorithm

### 4.1 Detailed Steps

1. **Update iteration count**.

2. **Update work counter** based on pivot cost.

3. **Check refactorization need**:
   - Call cxf_timing_refactor
   - If needed: trigger refactorization

4. **Periodic tasks** (every N iterations):
   - Update perturbation
   - Log progress

5. **Return status**.

### 4.2 Pseudocode

```
POST_ITERATE(state, env):
    state.iteration++
    state.workCounter += PIVOT_COST

    // Check refactorization
    need := cxf_timing_refactor(state, env)
    IF need >= 1:
        cxf_basis_refactor(state, env)
        cxf_pricing_invalidate(state.pricingState, INVALID_WEIGHTS)
        RETURN 1

    // Periodic tasks
    IF state.iteration MOD 100 == 0:
        LOG_PROGRESS(state)

    RETURN 0
```

## 5. Complexity

- O(1) typically, O(m^2) if refactorization triggered

## 6. Error Handling

None - always succeeds

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| First iteration | iteration = 1 | Normal processing |
| Refactor trigger | High eta count | Refactorize |

## 8. Thread Safety

**Thread-safe:** No

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_timing_refactor | Basis | Check refactor need |
| cxf_basis_refactor | Basis | Perform refactorization |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_simplex_iterate | Calls this at end |

## 11. Design Notes

**Separate function:** Allows flexible iteration loop design.

## 12. References

- Maros, I. (2003). "Computational Techniques of the Simplex Method"

## 13. Validation Checklist

- [x] All criteria met

---

*Convexfeld LP Solver Specification*
*Based on published optimization literature*
