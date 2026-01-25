# cxf_simplex_cleanup

**Module:** Simplex Core
**Spec Version:** 1.0

## 1. Purpose

Performs problem cleanup after preprocessing reductions. Restores eliminated variables to the solution, unscales values, and reverses other preprocessing transformations. Called before extracting the final solution to ensure results correspond to the original problem formulation.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| state | SolverState* | Solver state with solution | Non-null, valid | Yes |
| env | CxfEnv* | Environment | Non-null, valid | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | 0=success |

### 2.3 Side Effects

- Restores eliminated variable values
- Unscales primal and dual values
- Restores constraint values

## 3. Contract

### 3.1 Preconditions

- [ ] Solve completed on preprocessed problem
- [ ] Preprocessing records available

### 3.2 Postconditions

- [ ] Solution corresponds to original problem
- [ ] All eliminated variables have values

## 4. Algorithm

### 4.1 Detailed Steps

1. **Unscale primal values**:
   - x_orig[j] = x_scaled[j] * colScale[j]

2. **Unscale dual values**:
   - y_orig[i] = y_scaled[i] * rowScale[i]

3. **Restore fixed variables**:
   - For each fixed variable: x[j] = fixed_value

4. **Restore eliminated rows**:
   - Compute slack values from Ax - b

5. **Unscale reduced costs**:
   - rc_orig[j] = rc_scaled[j] / colScale[j]

### 4.2 Pseudocode

```
CLEANUP(state, env):
    // Unscale primals
    FOR j := 0 TO n - 1:
        state.x[j] *= state.colScale[j]

    // Unscale duals
    FOR i := 0 TO m - 1:
        state.dualValues[i] *= state.rowScale[i]

    // Restore fixed variables
    FOR each fixed variable j:
        state.x[j] := state.fixedValues[j]

    // Unscale reduced costs
    FOR j := 0 TO n - 1:
        state.reducedCosts[j] /= state.colScale[j]

    RETURN 0
```

## 5. Complexity

- O(n + m)

## 6. Error Handling

None

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| No preprocessing | No scaling or eliminations | Trivial cleanup |
| All fixed | All variables eliminated | Restore all |

## 8. Thread Safety

**Thread-safe:** No

## 9. Dependencies

### 9.1 Functions Called

None significant

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_simplex_preprocess | Creates transformations reversed here |
| cxf_extract_solution | Called after this |

## 11. Design Notes

**Reverse preprocessing:** Each preprocessing step must have corresponding cleanup to restore original problem space.

## 12. References

- Andersen, E.D. (1995). "Presolving in Linear Programming"

## 13. Validation Checklist

- [x] All criteria met

---

*Convexfeld LP Solver Specification*
*Based on published optimization literature*
