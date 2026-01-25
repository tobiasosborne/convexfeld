# cxf_crossover

**Module:** Crossover
**Spec Version:** 1.0

## 1. Purpose

Performs crossover from an interior-point (barrier) solution to a basic (vertex) solution. Interior-point methods produce solutions in the relative interior of the optimal face, while simplex-based analysis often requires a basic solution. This function converts the IPM solution to a basic feasible solution by identifying active constraints and constructing a valid simplex basis.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| state | SolverState* | Solver state with IPM solution | Non-null, valid state | Yes |
| env | CxfEnv* | Environment with tolerances | Non-null, valid env | Yes |
| crossoverMode | int | Crossover strategy (0=auto, 1=primal, 2=dual) | 0, 1, or 2 | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | Error code: 0=success, 3=infeasible, 10=iteration limit |

### 2.3 Side Effects

- Modifies primal solution (x) to vertex
- Modifies dual solution (y, reduced costs)
- Establishes simplex basis (basisHeader, varStatus)
- Updates solve statistics

## 3. Contract

### 3.1 Preconditions

- [ ] IPM solution is available (primal and dual)
- [ ] Solution is optimal (within tolerance)
- [ ] Problem dimensions are valid

### 3.2 Postconditions

- [ ] Solution is a basic feasible solution
- [ ] Exactly m basic variables
- [ ] All nonbasic variables at bounds
- [ ] Optimality preserved (same objective value within tolerance)

### 3.3 Invariants

- [ ] Problem data unchanged
- [ ] Bound arrays unchanged

## 4. Algorithm

### 4.1 Overview

Crossover proceeds in three phases:
1. **Bound identification:** Determine which variables are at or near bounds
2. **Basis construction:** Build initial basis from near-bound variables
3. **Cleanup:** Use simplex iterations to push remaining interior variables to bounds

### 4.2 Detailed Steps

1. **Initialize crossover**:
   - Set tolerance = primal feasibility tolerance
   - Prepare bound arrays and basis structures

2. **Classify variables by position**:
   - For each variable j:
     - If |x_j - lb_j| < tol: mark at-lower
     - Else if |x_j - ub_j| < tol: mark at-upper
     - Else: mark interior (superbasic)

3. **Snap near-bound variables**:
   - For at-lower: set x_j = lb_j
   - For at-upper: set x_j = ub_j
   - Update constraint residuals

4. **Construct initial basis**:
   - Slacks for inequality constraints
   - Select structural variables for equality constraints
   - Fill remaining with superbasic variables

5. **Factorize basis**:
   - Compute LU factorization
   - Check for singularity

6. **Push superbasics to bounds**:
   - While superbasic count > 0:
     - Select superbasic variable to move
     - Determine direction (toward lb or ub)
     - Compute pivot to maintain feasibility
     - Perform simplex-like pivot
     - Update basis and solution

7. **Verify optimality**:
   - Check reduced costs
   - If suboptimal, perform cleanup iterations

8. **Finalize**:
   - Store basis in state
   - Update statistics

### 4.3 Pseudocode

```
CROSSOVER(state, env, crossoverMode):
    n := state.numVars
    m := state.numConstrs
    tol := env.FeasibilityTol

    // Phase 1: Classify and snap
    superbasicCount := 0
    FOR j := 0 TO n - 1:
        x := state.primalSolution[j]
        lb := state.lb[j]
        ub := state.ub[j]

        IF |x - lb| < tol:
            state.varStatus[j] := NONBASIC_LOWER
            state.primalSolution[j] := lb
        ELSE IF |x - ub| < tol:
            state.varStatus[j] := NONBASIC_UPPER
            state.primalSolution[j] := ub
        ELSE:
            state.varStatus[j] := SUPERBASIC
            superbasicCount++

    // Phase 2: Construct initial basis
    basicCount := 0
    FOR i := 0 TO m - 1:
        IF state.constrSense[i] != '=':
            // Use slack for inequality
            slackVar := n + i
            state.basisHeader[i] := slackVar
            state.varStatus[slackVar] := i  // Basic
            basicCount++
        ELSE:
            // Need structural for equality
            state.basisHeader[i] := -1  // Mark for filling

    // Fill remaining basis positions with superbasics
    FOR j := 0 TO n - 1:
        IF state.varStatus[j] == SUPERBASIC:
            FOR i := 0 TO m - 1:
                IF state.basisHeader[i] == -1:
                    state.basisHeader[i] := j
                    state.varStatus[j] := i  // Basic
                    basicCount++
                    superbasicCount--
                    BREAK

    // Fill any remaining with slacks or arbitrary
    FILL_REMAINING_BASIS(state)

    // Factorize
    err := cxf_basis_refactor(state, env)
    IF err != 0:
        RETURN err

    // Phase 3: Push superbasics
    maxIter := 1000
    iter := 0
    WHILE superbasicCount > 0 AND iter < maxIter:
        iter++

        // Find superbasic to push
        svar := FIND_SUPERBASIC(state)
        IF svar < 0:
            BREAK

        // Determine push direction
        x := state.primalSolution[svar]
        lb := state.lb[svar]
        ub := state.ub[svar]

        IF ub - x < x - lb:
            targetBound := ub
            targetStatus := NONBASIC_UPPER
        ELSE:
            targetBound := lb
            targetStatus := NONBASIC_LOWER

        // Compute how far we can move
        delta := targetBound - x
        column := GET_COLUMN(state.matrix, svar)
        pivotCol := FTRAN(state, column)

        // Ratio test to find leaving variable
        (leavingRow, stepSize) := RATIO_TEST(state, pivotCol, delta)

        IF leavingRow < 0:
            // Can move all the way
            state.primalSolution[svar] := targetBound
            state.varStatus[svar] := targetStatus
            superbasicCount--
        ELSE:
            // Pivot
            PERFORM_PIVOT(state, svar, leavingRow, pivotCol, stepSize)
            IF state.varStatus[svar] >= 0:  // Entered basis
                superbasicCount--

    IF superbasicCount > 0:
        // Remaining superbasics: force to bounds
        FORCE_REMAINING_SUPERBASICS(state)

    // Cleanup iterations if needed
    CROSSOVER_CLEANUP(state, env)

    RETURN 0
```

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(n + m) if most variables at bounds
- **Average case:** O(k * m^2) where k = superbasic count
- **Worst case:** O(n * m^2) if many superbasics

### 5.2 Space Complexity

- O(n + m) for status and basis arrays
- Standard simplex working space

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| Singular basis | 3 | Cannot construct valid basis |
| Iteration limit | 10 | Too many cleanup iterations |
| Numerical failure | 12 | Pivot element too small |

### 6.2 Error Behavior

On error:
- Returns error code
- Solution may be partially crossed over
- Caller may retry or use IPM solution directly

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Already vertex | IPM at vertex | Minimal work |
| All interior | No variables at bounds | Many iterations |
| Degenerate | Zero basic values | Handle carefully |
| Singular | Linearly dependent constraints | May fail |

## 8. Thread Safety

**Thread-safe:** No (modifies state extensively)

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_basis_refactor | Basis | Factorize basis |
| cxf_ftran | Basis | Forward transformation |
| cxf_crossover_bounds | Crossover | Bound snapping |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_barrier_solve | Barrier | After IPM convergence |
| cxf_solve_lp | Main | When crossover enabled |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_crossover_bounds | Bound identification helper |
| cxf_simplex_iterate | Used for cleanup |
| cxf_barrier_solve | Provides IPM solution |

## 11. Design Notes

### 11.1 Design Rationale

**Bound snapping first:** Moving near-bound variables to exact bounds reduces superbasic count cheaply.

**Primal vs dual crossover:** Primal crossover works in primal space, dual works in dual. Auto-selection based on problem characteristics.

**Cleanup iterations:** Simple bound-pushing may not achieve optimality. Simplex cleanup ensures correctness.

### 11.2 Performance Considerations

- Most IPM solutions have many variables at bounds
- Superbasic count is typically O(m), not O(n)
- Cleanup iterations similar to warm-started simplex

## 12. References

- Andersen, E.D. and Andersen, K.D. (1995). "Presolving in Linear Programming"
- Megiddo, N. (1989). "On Finding Primal- and Dual-Optimal Bases"
- Bixby, R.E. (1994). "Progress in Linear Programming"

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
