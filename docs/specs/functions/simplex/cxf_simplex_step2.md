# cxf_simplex_step2

**Module:** Simplex Core
**Spec Version:** 1.0

## 1. Purpose

Extended pivot operation that includes dual solution updates and handles bound flip cases. This function performs the same basic pivot as cxf_simplex_step but additionally updates dual values, handles the case where the entering variable reaches its opposite bound (bound flip), and maintains additional state for advanced pricing strategies.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| state | SolverState* | Solver state | Non-null, valid | Yes |
| entering | int | Entering variable index | 0 <= entering < numVars | Yes |
| leavingRow | int | Leaving row index | 0 <= row < numConstrs | Yes |
| pivotCol | double* | FTRAN result | Non-null | Yes |
| pivotRow | double* | BTRAN result | Non-null | Yes |
| stepSize | double | Primal step length | >= 0 | Yes |
| dualStepSize | double | Dual step length | Any | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | 0=normal pivot, 1=bound flip, -1=error |

### 2.3 Side Effects

- All side effects of cxf_simplex_step
- Additionally updates dual values
- May perform bound flip instead of basis change

## 3. Contract

### 3.1 Preconditions

- [ ] All preconditions of cxf_simplex_step
- [ ] BTRAN result available in pivotRow

### 3.2 Postconditions

- [ ] If normal: same as cxf_simplex_step
- [ ] If bound flip: entering variable at opposite bound, no basis change
- [ ] Dual values updated

## 4. Algorithm

### 4.1 Overview

Extends the basic step with:
1. Detection of bound flip case
2. Dual solution update
3. SE weight update support

### 4.2 Detailed Steps

1. **Check for bound flip**:
   - If entering variable can reach opposite bound before ratio test limit
   - And this improves objective more

2. **If bound flip**:
   - Move entering variable to opposite bound
   - Update objective
   - No basis change
   - Return 1

3. **Otherwise, perform standard step**.

4. **Update dual values**:
   - y_new = y_old + dualStepSize * pivotRow

5. **Return 0**.

### 4.3 Pseudocode

```
STEP2(state, entering, leavingRow, pivotCol, pivotRow, stepSize, dualStepSize):
    // Check bound flip
    lb := state.lb[entering]
    ub := state.ub[entering]
    range := ub - lb

    IF state.varStatus[entering] == -1:  // At lower
        flipStep := range
    ELSE:
        flipStep := range

    IF flipStep < stepSize AND IMPROVES_MORE(flipStep):
        // Bound flip
        IF state.varStatus[entering] == -1:
            state.x[entering] := ub
            state.varStatus[entering] := -2
        ELSE:
            state.x[entering] := lb
            state.varStatus[entering] := -1
        state.objValue += state.reducedCosts[entering] * flipStep
        RETURN 1  // Bound flip

    // Standard step
    err := cxf_simplex_step(state, entering, leavingRow, pivotCol, stepSize)
    IF err != 0:
        RETURN err

    // Update duals
    FOR i := 0 TO m - 1:
        state.dualValues[i] += dualStepSize * pivotRow[i]

    RETURN 0
```

## 5. Complexity

- Same as cxf_simplex_step plus O(m) for dual update

## 6. Error Handling

Same as cxf_simplex_step

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Bound flip optimal | Flip better than pivot | Return 1, flip performed |
| No upper bound | ub = infinity | No flip possible |
| Fixed variable | lb = ub | Should not occur |

## 8. Thread Safety

**Thread-safe:** No

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_simplex_step | Simplex | Base pivot operation |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_simplex_step | Base version without dual update |
| cxf_simplex_step3 | Dual simplex variant |

## 11. Design Notes

**Bound flip optimization:** Can reduce iteration count by avoiding unnecessary basis changes.

## 12. References

- Maros, I. (2003). "Computational Techniques of the Simplex Method"

## 13. Validation Checklist

- [x] All criteria met

---

*Convexfeld LP Solver Specification*
*Based on published optimization literature*
