# cxf_simplex_preprocess

**Module:** Simplex Core
**Spec Version:** 1.0

## 1. Purpose

Performs preprocessing reductions on the LP problem before simplex iterations begin. Preprocessing improves solver performance by reducing problem size (eliminating redundant constraints and fixed variables), improving numerical conditioning (through scaling), and tightening variable bounds (through constraint propagation). It may also detect infeasibility early, avoiding the need for simplex iterations entirely.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| state | SolverState* | Solver state with working problem data | Non-null, valid state | Yes |
| env | CxfEnv* | Environment containing presolve parameters | Non-null, valid env | Yes |
| flags | int | Control flags (bit 0: skip preprocessing if set) | 0 or 1 | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | Error code: 0=success, 3=infeasible detected |

### 2.3 Side Effects

- Modifies working bounds (lb_working, ub_working)
- Computes and stores row/column scaling factors
- Updates removedRows and removedCols counters
- Sets boundsFlag to indicate bounds were updated
- May set specialFlag1 if preprocessing skipped

## 3. Contract

### 3.1 Preconditions

- [ ] State is initialized with working bound arrays
- [ ] Problem dimensions are set in state
- [ ] Objective, RHS, and sense arrays are populated
- [ ] Row and column scaling arrays are allocated

### 3.2 Postconditions

- [ ] If preprocessing disabled: specialFlag1=1, no changes made
- [ ] If successful: working bounds may be tightened
- [ ] If successful: scaling factors computed (rowScale, colScale)
- [ ] If successful: removedRows/removedCols updated
- [ ] If infeasible: return code 3, bounds violated

### 3.3 Invariants

- [ ] Original bounds (lb_original, ub_original) unchanged
- [ ] Problem dimensions unchanged
- [ ] Matrix structure unchanged

## 4. Algorithm

### 4.1 Overview

Preprocessing applies a sequence of reduction techniques:

1. **Fixed variable elimination**: Variables with lb = ub are fixed
2. **Singleton row elimination**: Rows with exactly one nonzero directly bound that variable
3. **Bound propagation**: Iteratively tightens bounds using constraint information
4. **Scaling**: Computes row and column scaling factors

### 4.2 Detailed Steps

1. **Check if preprocessing enabled**:
   - Read Presolve parameter from environment
   - If disabled or skip flag set: set specialFlag1=1 and return

2. **Extract problem data** from state.

3. **Fixed variable elimination**:
   - For each variable j where ub[j] - lb[j] < intTol:
     - Compute fixed value: (lb[j] + ub[j]) / 2
     - Substitute into objective and constraints
     - Increment removedCols counter

4. **Singleton row elimination**:
   - For each constraint i with exactly one nonzero:
     - Derive bound on that variable from constraint
     - Check for infeasibility
     - Increment removedRows counter

5. **Bound propagation** (iterate until convergence):
   - For each constraint i:
     - Compute LHS range using current bounds
     - For each variable j in constraint:
       - Derive implied bounds from constraint
       - Update if tighter
     - Check for infeasibility
   - Repeat for up to maxPasses (typically 10)
   - Stop early if no bounds changed

6. **Geometric mean scaling**:
   - For each row i:
     - Find min/max coefficient magnitudes
     - rowScale[i] = 1 / sqrt(min * max)
   - For each column j:
     - Find min/max coefficient magnitudes
     - colScale[j] = 1 / sqrt(min * max)
   - Clamp scales to [1e-6, 1e6]

7. **Update state** with statistics.

### 4.3 Pseudocode

```
PREPROCESS(state, env, flags):
    IF presolve_disabled OR (flags & 1):
        state.specialFlag1 := 1
        RETURN 0

    // Fixed variable elimination
    FOR j := 0 TO numVars-1:
        IF ub[j] - lb[j] < intTol:
            fixed := (lb[j] + ub[j]) / 2
            SUBSTITUTE_INTO_CONSTRAINTS(j, fixed)
            removedCols++

    // Singleton row elimination
    FOR i := 0 TO numConstrs-1:
        (count, j, a) := COUNT_NONZEROS_IN_ROW(i)
        IF count = 1:
            UPDATE_BOUND_FROM_SINGLETON(j, a, rhs[i], sense[i])
            IF lb[j] > ub[j] + feasTol:
                RETURN INFEASIBLE
            removedRows++

    // Bound propagation
    FOR pass := 1 TO maxPasses:
        changed := FALSE
        FOR each constraint i:
            [lhsMin, lhsMax] := COMPUTE_LHS_RANGE(i)
            FOR each variable j in constraint i:
                [newLb, newUb] := DERIVE_BOUNDS(j, i, lhsMin, lhsMax)
                IF newLb > lb[j] + feasTol:
                    lb[j] := newLb
                    changed := TRUE
                IF newUb < ub[j] - feasTol:
                    ub[j] := newUb
                    changed := TRUE
                IF lb[j] > ub[j] + feasTol:
                    RETURN INFEASIBLE
        IF NOT changed:
            BREAK

    // Geometric mean scaling
    FOR i := 0 TO numConstrs-1:
        [minC, maxC] := ROW_COEFF_RANGE(i)
        rowScale[i] := CLAMP(1/sqrt(minC*maxC), 1e-6, 1e6)

    FOR j := 0 TO numVars-1:
        [minC, maxC] := COL_COEFF_RANGE(j)
        colScale[j] := CLAMP(1/sqrt(minC*maxC), 1e-6, 1e6)

    UPDATE_STATE(removedRows, removedCols, boundsFlag=1)
    RETURN 0
```

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(n + m) if preprocessing disabled
- **Average case:** O(k * nnz) where k is propagation passes
- **Worst case:** O(maxPasses * n * m) for dense matrix

### 5.2 Space Complexity

- **Auxiliary space:** O(1) (uses existing arrays)

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| Bound violation detected | 3 (INFEASIBLE) | lb[j] > ub[j] after tightening |

### 6.2 Error Behavior

On infeasibility:
- Returns immediately with error code 3
- State may be partially modified

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Preprocessing disabled | Presolve=0 | Skip all, return success |
| All fixed variables | All lb=ub | All removed |
| No reductions possible | Dense, no fixed | Only scaling |
| Unbounded variables | lb=-inf, ub=inf | No propagation effect |

## 8. Thread Safety

**Thread-safe:** Yes

The function only modifies state arrays and reads from environment.

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| fabs, fmin, fmax, sqrt | C math | Mathematical operations |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_solve_lp | Simplex | After crash, before setup |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_simplex_init | Allocates arrays used here |
| cxf_simplex_crash | Called before this |
| cxf_simplex_setup | Called after this |
| cxf_simplex_cleanup | Restores eliminated variables |

## 11. Design Notes

### 11.1 Design Rationale

**Non-destructive reductions:** Eliminations are tracked via counters and flags rather than actually removing rows/columns. This simplifies implementation.

**Iterative bound propagation:** Multiple passes catch cascading implications.

**Geometric mean scaling:** Balances both directions, reducing very large and very small coefficients.

## 12. References

- Andersen, E.D. and Andersen, K.D. (1995). "Presolving in Linear Programming"
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
