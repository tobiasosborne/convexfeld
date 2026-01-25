# cxf_quadratic_adjust

**Module:** Simplex Core
**Spec Version:** 1.0

## 1. Purpose

Adjusts reduced costs for quadratic programming (QP) problems. In QP, the reduced cost depends on the current solution due to the quadratic term: c_j + sum(Q[j,k] * x[k]). This function updates reduced costs to reflect the current primal values, enabling the simplex method to handle convex quadratic objectives.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| state | SolverState* | Solver state with QP data | Non-null, valid | Yes |
| varIndex | int | Variable to adjust (-1 for all) | -1 or valid index | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | 0=success |

### 2.3 Side Effects

- Updates reduced costs to include quadratic contribution

## 3. Contract

### 3.1 Preconditions

- [ ] Problem has quadratic terms (hasQuadratic = 1)
- [ ] Primal solution is current
- [ ] Q matrix is available

### 3.2 Postconditions

- [ ] Reduced costs include Qx term

## 4. Algorithm

### 4.1 Overview

For quadratic objective: min c'x + 0.5 * x'Qx

The gradient is: c + Qx

Reduced costs must be updated to include Qx contribution.

### 4.2 Detailed Steps

1. **For each nonbasic variable j** (or specified variable):
   - Compute q_j = sum over k of Q[j,k] * x[k]
   - Update: rc[j] = c[j] + q_j - (dual contribution)

### 4.3 Pseudocode

```
QUADRATIC_ADJUST(state, varIndex):
    IF NOT state.hasQuadratic:
        RETURN 0

    IF varIndex >= 0:
        // Single variable
        q := 0.0
        FOR k := Q_col_start[varIndex] TO Q_col_end[varIndex] - 1:
            row := Q_rows[k]
            q += Q_values[k] * state.x[row]
        state.reducedCosts[varIndex] += q
    ELSE:
        // All variables
        FOR j := 0 TO n - 1:
            IF state.varStatus[j] >= 0:  // Basic
                CONTINUE
            q := 0.0
            FOR k := Q_col_start[j] TO Q_col_end[j] - 1:
                row := Q_rows[k]
                q += Q_values[k] * state.x[row]
            state.reducedCosts[j] += q

    RETURN 0
```

## 5. Complexity

- O(nnz_Q) for all variables
- O(nnz_Q_col) for single variable

## 6. Error Handling

None - succeeds or skips if no quadratic

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| No quadratic | hasQuadratic = 0 | Return immediately |
| All zeros x | x = 0 | No adjustment needed |
| Single variable | varIndex >= 0 | Adjust only that variable |

## 8. Thread Safety

**Thread-safe:** No

## 9. Dependencies

### 9.1 Functions Called

None significant

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_pricing_update | May call this for QP |
| cxf_simplex_iterate | Uses adjusted reduced costs |

## 11. Design Notes

**Convex QP:** The simplex method extends to convex QP. Non-convex requires different methods.

**Solution-dependent:** Unlike LP, QP reduced costs depend on current x.

## 12. References

- Gill, P.E. et al. (1981). "Practical Optimization"
- Wolfe, P. (1959). "The Simplex Method for Quadratic Programming"

## 13. Validation Checklist

- [x] All criteria met

---

*Convexfeld LP Solver Specification*
*Based on published optimization literature*
