# cxf_simplex_step

**Module:** Simplex Core
**Spec Version:** 1.0

## 1. Purpose

Executes the core pivot operation in a simplex iteration. Updates the primal solution, basis representation (via eta vector), and variable status arrays. This function is called after pricing and ratio test have determined the entering and leaving variables.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| state | SolverState* | Solver state | Non-null, valid | Yes |
| entering | int | Index of entering variable | 0 <= entering < numVars | Yes |
| leavingRow | int | Row index of leaving variable | 0 <= row < numConstrs | Yes |
| pivotCol | double* | FTRAN result (B^(-1) * a_entering) | Non-null | Yes |
| stepSize | double | Step length for pivot | >= 0 | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | Error code: 0=success, -1=pivot too small |

### 2.3 Side Effects

- Updates primal solution values
- Updates basis header
- Updates variable status arrays
- Creates new eta vector
- Updates eta count

## 3. Contract

### 3.1 Preconditions

- [ ] Entering variable is nonbasic
- [ ] Leaving row contains valid basic variable
- [ ] Pivot element pivotCol[leavingRow] is sufficiently large
- [ ] Step size is non-negative

### 3.2 Postconditions

- [ ] Entering variable is now basic
- [ ] Leaving variable is now nonbasic
- [ ] Primal solution updated
- [ ] Basis factorization updated (eta)

## 4. Algorithm

### 4.1 Overview

The pivot operation:
1. Update primal values of basic variables
2. Update entering variable value
3. Create eta vector for basis update
4. Update basis header and status

### 4.2 Detailed Steps

1. **Get pivot element**: pivot = pivotCol[leavingRow].

2. **Check pivot magnitude**: If |pivot| < tolerance, return error.

3. **Update basic variable values**:
   - For each basic variable in row i:
     - x_B[i] -= stepSize * pivotCol[i]

4. **Update entering variable value**:
   - x_entering = bound + stepSize (or bound - stepSize depending on direction)

5. **Create eta vector**:
   - Call cxf_pivot_with_eta(state, leavingRow, pivotCol)

6. **Update basis header**:
   - leaving = basisHeader[leavingRow]
   - basisHeader[leavingRow] = entering

7. **Update variable status**:
   - varStatus[entering] = leavingRow (now basic)
   - varStatus[leaving] = determine bound (-1 or -2)

### 4.3 Pseudocode

```
STEP(state, entering, leavingRow, pivotCol, stepSize):
    pivot := pivotCol[leavingRow]
    IF |pivot| < PIVOT_TOLERANCE:
        RETURN -1

    // Update basic variable values
    FOR i := 0 TO m - 1:
        basicVar := state.basisHeader[i]
        state.x[basicVar] -= stepSize * pivotCol[i]

    // Update entering variable
    IF state.varStatus[entering] == -1:  // At lower
        state.x[entering] := state.lb[entering] + stepSize
    ELSE:  // At upper
        state.x[entering] := state.ub[entering] - stepSize

    // Create eta vector
    cxf_pivot_with_eta(state, leavingRow, pivotCol)

    // Update basis
    leaving := state.basisHeader[leavingRow]
    state.basisHeader[leavingRow] := entering
    state.varStatus[entering] := leavingRow

    // Determine leaving variable bound
    IF state.x[leaving] <= state.lb[leaving] + tol:
        state.varStatus[leaving] := -1
    ELSE:
        state.varStatus[leaving] := -2

    RETURN 0
```

## 5. Complexity

### 5.1 Time Complexity

- O(m) for updating basic values
- O(m) for eta vector creation

### 5.2 Space Complexity

- O(m) for eta vector storage

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| Pivot too small | -1 | |pivot| < tolerance |

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Degenerate | stepSize = 0 | Basis changes, values unchanged |
| At upper bound | Status = -2 | Step in negative direction |
| Exact bound | x exactly at bound | Clean transition |

## 8. Thread Safety

**Thread-safe:** No (modifies state)

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_pivot_with_eta | Basis | Create eta vector |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_simplex_iterate | Simplex | After ratio test |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_simplex_step2 | Extended step with more updates |
| cxf_simplex_step3 | Dual simplex step |

## 11. Design Notes

### 11.1 Design Rationale

**Separate from iterate:** Allows different step variants (primal, dual, etc.).

**Eta-based update:** Efficient for single pivots; periodic refactorization handles accumulation.

## 12. References

- Maros, I. (2003). "Computational Techniques of the Simplex Method"

## 13. Validation Checklist

- [x] No code copied from implementation
- [x] Algorithm description is implementation-agnostic
- [x] All parameters documented
- [x] All error conditions listed
- [x] Complexity analysis complete
- [x] Edge cases identified
- [x] A competent developer could implement from this spec alone

---

*Convexfeld LP Solver Specification*
*Based on published optimization literature*
