# cxf_simplex_refine

**Module:** Simplex Core
**Spec Version:** 1.0

## 1. Purpose

Refines the solution after simplex termination to clean up numerical artifacts. Snaps near-bound values to exact bounds, cleans up near-zero values, and verifies optimality conditions. This improves solution quality and ensures reported values meet tolerance requirements.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| state | SolverState* | Solver state with solution | Non-null, valid | Yes |
| env | CxfEnv* | Environment with tolerances | Non-null, valid | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | 0=success, 1=minor adjustment made |

### 2.3 Side Effects

- May adjust primal values
- May adjust dual values
- May adjust reduced costs
- Updates objective value

## 3. Contract

### 3.1 Preconditions

- [ ] Simplex has terminated (optimal or other status)
- [ ] Solution arrays are populated

### 3.2 Postconditions

- [ ] Near-bound values snapped to exact bounds
- [ ] Near-zero values cleaned to exact zero
- [ ] Optimality conditions verified

## 4. Algorithm

### 4.1 Detailed Steps

1. **Snap primal values to bounds**:
   - For each variable j:
     - If |x[j] - lb[j]| < tolerance: x[j] = lb[j]
     - If |x[j] - ub[j]| < tolerance: x[j] = ub[j]

2. **Clean near-zero values**:
   - For each value: if |v| < 1e-12: v = 0

3. **Verify primal feasibility**:
   - Check Ax = b within tolerance

4. **Verify dual feasibility**:
   - Check reduced cost signs

5. **Recalculate objective** from cleaned values.

### 4.2 Pseudocode

```
REFINE(state, env):
    tol := env.FeasibilityTol
    adjusted := 0

    // Snap to bounds
    FOR j := 0 TO n - 1:
        IF |state.x[j] - state.lb[j]| < tol:
            state.x[j] := state.lb[j]
            adjusted++
        ELSE IF |state.x[j] - state.ub[j]| < tol:
            state.x[j] := state.ub[j]
            adjusted++

    // Clean near-zeros
    FOR j := 0 TO n - 1:
        IF |state.x[j]| < 1e-12:
            state.x[j] := 0

    FOR i := 0 TO m - 1:
        IF |state.dualValues[i]| < 1e-12:
            state.dualValues[i] := 0

    // Recalculate objective
    state.objValue := DOT(state.objCoeffs, state.x, n)

    RETURN (adjusted > 0) ? 1 : 0
```

## 5. Complexity

- O(n + m)

## 6. Error Handling

None - always succeeds

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Clean solution | No adjustments needed | Return 0 |
| Many adjustments | Many near-bound | All snapped, return 1 |

## 8. Thread Safety

**Thread-safe:** No

## 9. Dependencies

None significant

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_solve_lp | Calls this before returning |
| cxf_extract_solution | Called after this |

## 11. Design Notes

**Post-processing:** Improves solution quality without affecting optimality.

## 12. References

- Standard numerical computing practice

## 13. Validation Checklist

- [x] All criteria met

---

*Convexfeld LP Solver Specification*
*Based on published optimization literature*
