# cxf_simplex_unperturb

**Module:** Simplex Core
**Spec Version:** 1.0

## 1. Purpose

Removes perturbations applied to variable bounds, restoring original bounds. This is the inverse operation of cxf_simplex_perturbation. Called after simplex termination to recover the original problem bounds before extracting the final solution.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| state | SolverState* | Solver state with perturbations | Non-null, valid | Yes |
| env | CxfEnv* | Environment | Non-null, valid | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | 0=success, 1=no perturbation was applied |

### 2.3 Side Effects

- Restores working bounds from original bounds
- Clears perturbation arrays
- Resets perturbationApplied flag

## 3. Contract

### 3.1 Preconditions

- [ ] Original bounds are preserved
- [ ] Perturbation arrays exist (if perturbation was applied)

### 3.2 Postconditions

- [ ] Working bounds equal original bounds
- [ ] perturbationApplied = 0

## 4. Algorithm

### 4.1 Detailed Steps

1. **Check if perturbation was applied**:
   - If not: return 1

2. **Restore bounds**:
   - For each variable j:
     - lb_working[j] = lb_original[j]
     - ub_working[j] = ub_original[j]

3. **Free perturbation arrays** (optional).

4. **Clear flag**: perturbationApplied = 0.

### 4.2 Pseudocode

```
UNPERTURB(state, env):
    IF NOT state.perturbationApplied:
        RETURN 1

    // Restore original bounds
    FOR j := 0 TO n - 1:
        state.lb[j] := state.lb_original[j]
        state.ub[j] := state.ub_original[j]

    // Clear perturbation data
    FREE(state.lbPerturb)
    FREE(state.ubPerturb)
    state.lbPerturb := NULL
    state.ubPerturb := NULL

    state.perturbationApplied := 0
    RETURN 0
```

## 5. Complexity

- O(n)

## 6. Error Handling

| Condition | Return | Description |
|-----------|--------|-------------|
| No perturbation | 1 | Nothing to remove |

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Not perturbed | Flag = 0 | Return 1, no changes |
| Empty problem | n = 0 | Success |

## 8. Thread Safety

**Thread-safe:** No

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_free | Memory | Free perturbation arrays |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_simplex_perturbation | Inverse operation |
| cxf_simplex_final | Calls this during cleanup |

## 11. Design Notes

**Restore original problem:** Ensures final solution is with respect to original bounds, not perturbed bounds.

## 12. References

- Wolfe, P. (1963). "A technique for resolving degeneracy"

## 13. Validation Checklist

- [x] All criteria met

---

*Convexfeld LP Solver Specification*
*Based on published optimization literature*
