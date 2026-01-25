# cxf_extract_solution

**Module:** Simplex Core
**Spec Version:** 1.0

## 1. Purpose

Extracts the final solution from the solver state and copies it to the model's solution arrays. Converts internal representation to API format, setting primal values, dual values, reduced costs, and objective value. Also sets the model's status attribute.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| state | SolverState* | Solver state with solution | Non-null, valid | Yes |
| model | CxfModel* | Model to receive solution | Non-null, valid | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | 0=success |

### 2.3 Side Effects

- Sets model.X (primal solution)
- Sets model.RC (reduced costs)
- Sets model.Pi (dual values)
- Sets model.Slack (slack values)
- Sets model.ObjVal
- Sets model.Status

## 3. Contract

### 3.1 Preconditions

- [ ] Solver state has valid solution
- [ ] Model arrays are allocated

### 3.2 Postconditions

- [ ] All solution arrays populated
- [ ] Model status set correctly

## 4. Algorithm

### 4.1 Detailed Steps

1. **Copy primal values**: model.X[j] = state.x[j] for all j.

2. **Copy reduced costs**: model.RC[j] = state.reducedCosts[j] for all j.

3. **Copy dual values**: model.Pi[i] = state.dualValues[i] for all i.

4. **Compute slack values**: model.Slack[i] = b[i] - sum(A[i,:] * x) for all i.

5. **Set objective value**: model.ObjVal = state.objValue.

6. **Set status**: Based on solver termination status.

### 4.2 Pseudocode

```
EXTRACT_SOLUTION(state, model):
    n := state.numVars
    m := state.numConstrs

    // Primal values
    FOR j := 0 TO n - 1:
        model.X[j] := state.x[j]

    // Reduced costs
    FOR j := 0 TO n - 1:
        model.RC[j] := state.reducedCosts[j]

    // Dual values
    FOR i := 0 TO m - 1:
        model.Pi[i] := state.dualValues[i]

    // Slack values
    FOR i := 0 TO m - 1:
        model.Slack[i] := COMPUTE_SLACK(state, i)

    // Objective
    model.ObjVal := state.objValue

    // Status
    SWITCH state.status:
        CASE OPTIMAL: model.Status := CXF_OPTIMAL
        CASE INFEASIBLE: model.Status := CXF_INFEASIBLE
        CASE UNBOUNDED: model.Status := CXF_UNBOUNDED
        DEFAULT: model.Status := state.status

    RETURN 0
```

## 5. Complexity

- O(n + m + nnz) for slack computation

## 6. Error Handling

None - always succeeds

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Infeasible | No solution | Set status only |
| Unbounded | Ray available | Extract ray if requested |

## 8. Thread Safety

**Thread-safe:** No (modifies model)

## 9. Dependencies

### 9.1 Functions Called

None significant

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_simplex_cleanup | Called before this |
| cxf_solve_lp | Calls this |

## 11. Design Notes

**API boundary:** Converts internal state to API representation.

## 12. References

- Standard LP API design

## 13. Validation Checklist

- [x] All criteria met

---

*Convexfeld LP Solver Specification*
*Based on published optimization literature*
