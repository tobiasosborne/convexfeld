# cxf_basis_validate

**Module:** Basis Operations
**Spec Version:** 1.0

## 1. Purpose

Validates the current basis for consistency, feasibility, and numerical stability. Checks that exactly m variables are basic, the basis matrix is non-singular (within tolerance), and the current solution satisfies bound and constraint feasibility. Returns a status code indicating any problems found.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| state | SolverState* | Solver state with basis | Non-null, valid state | Yes |
| env | CxfEnv* | Environment with tolerances | Non-null, valid env | Yes |
| flags | int | Validation checks to perform (bitmask) | See flags table | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | Validation status: 0=valid, negative=specific issue |

### 2.3 Side Effects

- May update diagnostic fields in state
- May trigger refactorization if instability detected

## 3. Contract

### 3.1 Preconditions

- [ ] State has initialized basis structures
- [ ] Tolerances are set in environment

### 3.2 Postconditions

- [ ] Returns 0 if basis passes all requested checks
- [ ] Returns specific error code for first failure
- [ ] Diagnostic info stored in state

### 3.3 Invariants

- [ ] Basis data unchanged (validation only)

## 4. Algorithm

### 4.1 Overview

Validation performs multiple checks based on the flags parameter:
- COUNT: Verify exactly m basic variables
- SINGULAR: Test if basis matrix is near-singular
- FEASIBILITY: Check primal feasibility
- DUAL: Check dual feasibility (reduced costs)
- CONSISTENCY: Verify varStatus matches basisHeader

### 4.2 Detailed Steps

**Flag definitions:**
- CHECK_COUNT = 0x01
- CHECK_SINGULAR = 0x02
- CHECK_FEASIBILITY = 0x04
- CHECK_DUAL = 0x08
- CHECK_CONSISTENCY = 0x10
- CHECK_ALL = 0xFF

1. **Count check** (if CHECK_COUNT):
   - Count variables with status >= 0
   - If count != numConstrs: return -1

2. **Singularity check** (if CHECK_SINGULAR):
   - Compute condition estimate or check pivot magnitudes
   - If condition > threshold: return -2

3. **Primal feasibility** (if CHECK_FEASIBILITY):
   - For each variable: check lb <= x <= ub
   - For each constraint: check |Ax - b| <= feasTol
   - If violated: return -3

4. **Dual feasibility** (if CHECK_DUAL):
   - For nonbasic at lower: reduced cost >= -optTol
   - For nonbasic at upper: reduced cost <= optTol
   - If violated: return -4

5. **Consistency check** (if CHECK_CONSISTENCY):
   - For each basic variable in basisHeader:
     - Check varStatus[var] == row
   - If mismatch: return -5

6. **Return 0** if all checks pass

### 4.3 Pseudocode

```
VALIDATE(state, env, flags):
    m := state.numConstrs
    n := state.numVars
    feasTol := env.FeasibilityTol
    optTol := env.OptimalityTol

    IF flags & CHECK_COUNT:
        count := 0
        FOR i := 0 TO n + m - 1:
            IF state.varStatus[i] >= 0:
                count++
        IF count != m:
            state.diagnostic := "Wrong basic count"
            RETURN -1

    IF flags & CHECK_SINGULAR:
        IF ESTIMATE_CONDITION(state) > 1e12:
            state.diagnostic := "Near-singular basis"
            RETURN -2

    IF flags & CHECK_FEASIBILITY:
        FOR j := 0 TO n - 1:
            IF state.x[j] < state.lb[j] - feasTol:
                RETURN -3
            IF state.x[j] > state.ub[j] + feasTol:
                RETURN -3
        // Check Ax = b
        residual := COMPUTE_RESIDUAL(state)
        IF MAX_ABS(residual) > feasTol:
            RETURN -3

    IF flags & CHECK_DUAL:
        FOR j := 0 TO n - 1:
            IF state.varStatus[j] == -1:  // At lower
                IF state.reducedCost[j] < -optTol:
                    RETURN -4
            IF state.varStatus[j] == -2:  // At upper
                IF state.reducedCost[j] > optTol:
                    RETURN -4

    IF flags & CHECK_CONSISTENCY:
        FOR row := 0 TO m - 1:
            var := state.basisHeader[row]
            IF state.varStatus[var] != row:
                RETURN -5

    RETURN 0
```

## 5. Complexity

### 5.1 Time Complexity

- **Count/Consistency:** O(n + m)
- **Singularity:** O(m^2) for condition estimate
- **Feasibility:** O(nnz) for residual computation

### 5.2 Space Complexity

- O(m) for residual vector

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| Wrong basic count | -1 | Not exactly m basic variables |
| Near-singular | -2 | Condition number too large |
| Primal infeasible | -3 | Bound or constraint violation |
| Dual infeasible | -4 | Incorrect reduced cost sign |
| Inconsistent status | -5 | varStatus doesn't match basisHeader |

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Empty problem | m=0, n=0 | Returns 0 (trivially valid) |
| Optimal basis | All checks pass | Returns 0 |
| Degenerate | Zero basic values | May still be valid |
| flags = 0 | No checks | Returns 0 immediately |

## 8. Thread Safety

**Thread-safe:** Yes (mostly read-only)

**Note:** May trigger refactorization which requires exclusive access

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_basis_refactor | Basis | Refactor if needed |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_simplex_iterate | Simplex | Periodic validation |
| cxf_simplex_final | Simplex | Solution verification |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_basis_refactor | May be triggered on instability |
| cxf_simplex_cleanup | Called if validation fails |

## 11. Design Notes

### 11.1 Design Rationale

**Selective checks:** Different checks are appropriate at different times. Consistency checks are cheap; singularity checks are expensive.

**Early return:** Returns on first failure for efficiency. Caller can re-run with different flags to isolate issues.

### 11.2 Performance Considerations

- Avoid expensive checks (singularity) every iteration
- Consistency check is O(n + m), suitable for frequent use

## 12. References

- Maros, I. (2003). "Computational Techniques of the Simplex Method" - Numerical stability monitoring

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
