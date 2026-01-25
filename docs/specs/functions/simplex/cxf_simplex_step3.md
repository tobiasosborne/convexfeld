# cxf_simplex_step3

**Module:** Simplex Core
**Spec Version:** 1.0

## 1. Purpose

Performs a dual simplex pivot step. In dual simplex, the leaving variable is chosen first (by dual feasibility violation), then the entering variable is selected via dual ratio test. This function handles the specific pivot mechanics for dual simplex, including dual solution updates and primal value recovery.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| state | SolverState* | Solver state | Non-null, valid | Yes |
| leavingRow | int | Row of leaving variable | 0 <= row < numConstrs | Yes |
| entering | int | Entering variable (from dual ratio test) | 0 <= entering < numVars | Yes |
| pivotCol | double* | FTRAN result | Non-null | Yes |
| pivotRow | double* | BTRAN result | Non-null | Yes |
| dualStepSize | double | Dual step length | Any | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | 0=success, -1=error |

### 2.3 Side Effects

- Updates basis representation
- Updates dual values
- Recovers primal values
- Updates variable status

## 3. Contract

### 3.1 Preconditions

- [ ] Leaving variable has dual feasibility violation
- [ ] Entering variable selected by dual ratio test
- [ ] Pivot element is sufficiently large

### 3.2 Postconditions

- [ ] Basis updated with swap
- [ ] Dual values updated
- [ ] Primal feasibility may be violated (dual simplex maintains dual feasibility)

## 4. Algorithm

### 4.1 Overview

Dual simplex pivots maintain dual feasibility while working toward primal feasibility. The mechanics differ from primal simplex in the interpretation of step sizes and the order of variable selection.

### 4.2 Detailed Steps

1. **Get pivot element**: pivot = pivotCol[leavingRow].

2. **Validate pivot**.

3. **Update dual values**.

4. **Update basis representation** (eta vector).

5. **Swap basis header**.

6. **Update variable status**.

7. **Recover primal values** (optional, can defer).

### 4.3 Pseudocode

```
STEP3(state, leavingRow, entering, pivotCol, pivotRow, dualStepSize):
    pivot := pivotCol[leavingRow]
    IF |pivot| < TOLERANCE:
        RETURN -1

    // Update dual values
    FOR i := 0 TO m - 1:
        state.dualValues[i] += dualStepSize * pivotRow[i]

    // Create eta vector
    cxf_pivot_with_eta(state, leavingRow, pivotCol)

    // Update basis
    leaving := state.basisHeader[leavingRow]
    state.basisHeader[leavingRow] := entering
    state.varStatus[entering] := leavingRow
    state.varStatus[leaving] := DETERMINE_BOUND(state, leaving)

    // Primal recovery (optional here)
    // Can be done periodically for efficiency

    RETURN 0
```

## 5. Complexity

- O(m) per pivot

## 6. Error Handling

| Condition | Error Code | Description |
|-----------|------------|-------------|
| Small pivot | -1 | Numerical instability |

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Dual unbounded | No entering found | Indicates primal infeasibility |

## 8. Thread Safety

**Thread-safe:** No

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_pivot_with_eta | Basis | Basis update |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_simplex_step | Primal simplex step |
| cxf_simplex_step2 | Extended primal step |

## 11. Design Notes

**Dual simplex advantage:** Maintains dual feasibility, good for reoptimization after adding constraints.

## 12. References

- Forrest, J.J. and Goldfarb, D. (1992). "Steepest-Edge Simplex Algorithms"

## 13. Validation Checklist

- [x] All criteria met

---

*Convexfeld LP Solver Specification*
*Based on published optimization literature*
