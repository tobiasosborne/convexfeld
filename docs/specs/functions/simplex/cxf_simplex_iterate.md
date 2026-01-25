# cxf_simplex_iterate

**Module:** Simplex Core
**Spec Version:** 1.0

## 1. Purpose

Performs a single iteration of the simplex algorithm. Handles pricing (selecting entering variable), FTRAN (computing pivot column), ratio test (selecting leaving variable), and basis update. Returns status indicating whether to continue iterating, or whether optimality/infeasibility/unboundedness has been detected.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| state | SolverState* | Current solver state | Non-null, valid state | Yes |
| env | CxfEnv* | Environment with tolerances | Non-null, valid env | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | Status: 0=continue, 1=optimal, 2=infeasible, 3=unbounded |

### 2.3 Side Effects

- Updates basis (header, status arrays)
- Updates primal/dual solution values
- Updates reduced costs
- Increments iteration counter
- May trigger refactorization

## 3. Contract

### 3.1 Preconditions

- [ ] Basis is factorized and valid
- [ ] Reduced costs are current
- [ ] Phase indicator is set (1 or 2)

### 3.2 Postconditions

- [ ] If continue: one pivot completed, basis updated
- [ ] If optimal: no improving variable found
- [ ] If unbounded: improving direction with no blocking constraint
- [ ] Iteration counter incremented

## 4. Algorithm

### 4.1 Overview

Each simplex iteration performs:
1. Pricing: Find candidate entering variable
2. FTRAN: Compute representation in current basis
3. Ratio test: Find blocking constraint
4. Pivot: Update basis and solution

### 4.2 Detailed Steps

1. **Pricing**: Select entering variable
   - Call cxf_pricing_candidates or cxf_pricing_steepest
   - If no candidates: return OPTIMAL

2. **FTRAN**: Compute pivot column
   - Get column of entering variable
   - Call cxf_ftran to compute B^(-1) * a

3. **Ratio test**: Find leaving variable
   - Call cxf_ratio_test
   - If no leaving variable: return UNBOUNDED

4. **BTRAN** (for dual updates):
   - Compute row of leaving variable
   - Call cxf_btran

5. **Pivot**: Update basis
   - Call cxf_simplex_step
   - Update basis header and status
   - Update eta vectors or refactorize

6. **Update reduced costs**:
   - Call cxf_pricing_update

7. **Update objective value**.

8. **Check refactorization**:
   - If eta count high or timing indicates: refactorize

9. **Return** CONTINUE.

### 4.3 Pseudocode

```
ITERATE(state, env):
    // Pricing
    entering := PRICING(state, env)
    IF entering < 0:
        RETURN OPTIMAL

    // FTRAN
    column := GET_COLUMN(state.matrix, entering)
    pivotCol := state.workArray
    cxf_ftran(state, column, pivotCol)

    // Ratio test
    (leaving, stepSize, leavingStatus) := RATIO_TEST(state, pivotCol, entering)
    IF leaving < 0:
        RETURN UNBOUNDED

    // BTRAN for dual update
    pivotRow := state.workArray2
    cxf_btran(state, leaving, pivotRow)

    // Pivot
    cxf_simplex_step(state, entering, leaving, pivotCol, pivotRow, stepSize)

    // Update reduced costs
    cxf_pricing_update(state.pricingState, state, entering, leaving, pivotCol, pivotRow)

    // Update objective
    rc := state.reducedCosts[entering]
    state.objValue += rc * stepSize

    // Check refactorization
    IF NEED_REFACTOR(state):
        cxf_basis_refactor(state, env)

    state.iteration++
    RETURN CONTINUE
```

## 5. Complexity

### 5.1 Time Complexity

- **Per iteration:** O(m * nnz_col) for FTRAN, ratio test, and update

### 5.2 Space Complexity

- O(m) for pivot column and row work arrays

## 6. Error Handling

### 6.1 Return Values

| Value | Meaning |
|-------|---------|
| 0 | Continue iterating |
| 1 | Optimal solution found |
| 2 | Problem is infeasible (Phase I) |
| 3 | Problem is unbounded |
| 12 | Numerical error |

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Degenerate pivot | Step size = 0 | Basis changes, objective unchanged |
| Multiple candidates | Ties in pricing | Select first or by tie-breaking rule |
| Near-singular | Small pivot element | May trigger refactorization |

## 8. Thread Safety

**Thread-safe:** No (modifies state)

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_pricing_* | Pricing | Select entering variable |
| cxf_ftran | Basis | Forward transformation |
| cxf_btran | Basis | Backward transformation |
| cxf_ratio_test | Ratio Test | Select leaving variable |
| cxf_simplex_step | Simplex | Perform pivot |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_solve_lp | Simplex | Main iteration loop |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_simplex_step | Performs the actual pivot |
| cxf_basis_refactor | Called when refactor needed |

## 11. Design Notes

### 11.1 Design Rationale

**Single iteration function:** Allows caller to check termination conditions and handle special cases between iterations.

**Refactorization check:** Prevents numerical degradation from accumulated eta vectors.

## 12. References

- Dantzig, G.B. (1963). "Linear Programming and Extensions"
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
