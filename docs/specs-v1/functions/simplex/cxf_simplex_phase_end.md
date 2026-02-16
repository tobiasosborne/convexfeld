# cxf_simplex_phase_end

**Module:** Simplex Core
**Spec Version:** 1.0

## 1. Purpose

Handles the transition between Phase I and Phase II of the two-phase simplex method. When Phase I finds a feasible solution (artificial variables driven to zero), this function prepares the solver for Phase II by resetting the objective function to the original and reinitializing reduced costs.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| state | SolverState* | Solver state at end of phase | Non-null, valid | Yes |
| env | CxfEnv* | Environment | Non-null, valid | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | 0=transition to Phase II, 2=infeasible |

### 2.3 Side Effects

- Resets objective coefficients (if Phase I -> II)
- Reinitializes reduced costs
- Updates phase indicator
- May remove artificial variables from basis

## 3. Contract

### 3.1 Preconditions

- [ ] Phase I just completed (optimal or infeasible)
- [ ] Basis is valid

### 3.2 Postconditions

- [ ] If feasible: ready for Phase II
- [ ] If infeasible: returns error code

## 4. Algorithm

### 4.1 Overview

At end of Phase I:
1. Check if truly feasible (infeasibility measure = 0)
2. If feasible: restore original objective, reset reduced costs
3. If infeasible: return infeasible status

### 4.2 Detailed Steps

1. **Check infeasibility measure**:
   - If state.objValue > tolerance: return INFEASIBLE

2. **Remove artificial variables** (if any in basis):
   - Try to pivot them out
   - Or mark as fixed at zero

3. **Restore original objective**:
   - Copy original obj coefficients to working array

4. **Recompute reduced costs**:
   - For each nonbasic variable: rc = c_j - c_B * B^(-1) * a_j

5. **Reset objective value** to current primal cost.

6. **Set phase = 2**.

### 4.3 Pseudocode

```
PHASE_END(state, env):
    // Check feasibility
    IF state.objValue > env.FeasibilityTol:
        RETURN INFEASIBLE

    // Handle artificials
    FOR i := 0 TO m - 1:
        basicVar := state.basisHeader[i]
        IF IS_ARTIFICIAL(basicVar):
            // Try to pivot out
            replacement := FIND_REPLACEMENT(state, i)
            IF replacement >= 0:
                PIVOT_ARTIFICIAL_OUT(state, i, replacement)
            ELSE:
                // Fix at zero
                state.lb[basicVar] := 0
                state.ub[basicVar] := 0

    // Restore objective
    FOR j := 0 TO n - 1:
        state.objCoeffs[j] := state.originalObj[j]

    // Recompute reduced costs
    COMPUTE_REDUCED_COSTS(state)

    // Reset objective value
    state.objValue := COMPUTE_PRIMAL_OBJ(state)

    state.phase := 2
    RETURN 0
```

## 5. Complexity

- O(n + m) for objective reset and reduced cost computation

## 6. Error Handling

| Condition | Error Code | Description |
|-----------|------------|-------------|
| Phase I infeasible | 2 | Original problem has no feasible solution |

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Already feasible | Phase I trivial | Immediate transition |
| Artificial in basis | Zero artificial | Try to remove or fix |

## 8. Thread Safety

**Thread-safe:** No

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| COMPUTE_REDUCED_COSTS | Internal | Reset pricing |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_solve_lp | Calls this at phase transition |

## 11. Design Notes

**Two-phase method:** Phase I minimizes infeasibility. If zero achieved, problem is feasible and Phase II optimizes the original objective.

## 12. References

- Chvatal, V. (1983). "Linear Programming" - Two-phase method

## 13. Validation Checklist

- [x] All criteria met

---

*Convexfeld LP Solver Specification*
*Based on published optimization literature*
