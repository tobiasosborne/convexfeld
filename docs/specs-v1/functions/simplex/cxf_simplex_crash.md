# cxf_simplex_crash

**Module:** Simplex Core
**Spec Version:** 1.0

## 1. Purpose

Constructs an initial crash basis for the simplex algorithm by heuristically selecting which variables should be basic vs. nonbasic. A good initial basis significantly reduces the number of simplex iterations needed to reach optimality (typically 20-50% reduction). The function allocates basis tracking structures, applies scoring heuristics to evaluate candidate basic variables, and establishes the initial basis structure before iterations begin.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| state | SolverState* | Initialized solver state with problem data | Non-null, valid state | Yes |
| env | CxfEnv* | Environment containing solver parameters | Non-null, valid env | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | Error code: 0=success, 1001=out of memory |

### 2.3 Side Effects

- Allocates varStatus array (numVars + numConstrs integers)
- Allocates basisHeader array (numConstrs integers)
- Sets initial variable status codes (basic/nonbasic)
- Sets initial phase indicator (Phase I or Phase II)

## 3. Contract

### 3.1 Preconditions

- [ ] State is initialized with valid dimensions
- [ ] Working bounds are set (lb_working, ub_working)
- [ ] Objective coefficients are available
- [ ] Constraint senses are set

### 3.2 Postconditions

- [ ] varStatus array allocated and initialized for all variables and slacks
- [ ] basisHeader array allocated with basic variable index for each row
- [ ] Exactly numConstrs variables are marked as basic
- [ ] All nonbasic variables have valid status (-1 for LB, -2 for UB)
- [ ] Phase indicator set (1 if Phase I needed, 2 otherwise)

### 3.3 Invariants

- [ ] Problem data (bounds, objective, senses) unchanged
- [ ] Working arrays remain valid

## 4. Algorithm

### 4.1 Overview

The crash procedure selects an initial basis by scoring candidate variables for each constraint row. For inequality constraints, slack variables are natural candidates. For equality constraints, structural variables must be selected. The scoring function considers coefficient magnitude, bound range, objective cost, and potential for objective improvement.

### 4.2 Detailed Steps

1. **Extract dimensions**: numVars and numConstrs from state.

2. **Allocate varStatus array**: Size (numVars + numConstrs) integers.

3. **Allocate basisHeader array**: Size numConstrs integers.

4. **Initialize all structural variables as nonbasic**:
   - If lb[j] > -infinity: status[j] = -1 (at lower bound)
   - Else if ub[j] < infinity: status[j] = -2 (at upper bound)
   - Else: status[j] = -1 (free variable)

5. **Initialize all slack variables as nonbasic**: status[numVars + i] = -1.

6. **Initialize basisHeader**: all entries = -1.

7. **For each constraint row**, select best basic variable:

   a. **For inequality constraints** (sense is '<' or '>'):
      - Slack variable is a natural candidate (score = 1e6)

   b. **Score each structural variable** in the row:
      - Skip if already basic
      - Skip if coefficient is zero
      - Scoring function:
        - +100 * |coefficient|
        - +50 / (1 + bound_range)
        - -10 * |objective_coeff|
        - +30 if variable bound includes zero
        - +20 if variable can improve objective

   c. **Select best candidate**: variable with highest score.

   d. **Make selected variable basic**:
      - basisHeader[row] = bestVar
      - varStatus[bestVar] = row

8. **Verify basis validity**: Exactly m basic variables.

9. **Determine phase**.

### 4.3 Pseudocode

```
CRASH_BASIS(state, env):
    n := numVars
    m := numConstrs

    // Allocate basis structures
    varStatus := ALLOCATE((n + m) * sizeof(int))
    basisHeader := ALLOCATE(m * sizeof(int))

    // Initialize all as nonbasic
    FOR j := 0 TO n-1:
        varStatus[j] := DETERMINE_NONBASIC_STATUS(lb[j], ub[j])
    FOR i := 0 TO m-1:
        varStatus[n + i] := -1
        basisHeader[i] := -1

    // Select basic variables
    FOR row := 0 TO m-1:
        bestVar := -1
        bestScore := -INFINITY

        // Inequalities prefer slacks
        IF sense[row] IN {'<', '>'}:
            bestVar := n + row
            bestScore := 1e6

        // Score structural variables
        FOR j := 0 TO n-1:
            IF varStatus[j] >= 0:
                CONTINUE
            coeff := GET_COEFFICIENT(row, j)
            IF |coeff| < 1e-10:
                CONTINUE

            score := 0
            score += 100 * |coeff|
            score += 50 / (1 + (ub[j] - lb[j]))
            score -= 10 * |obj[j]|
            IF lb[j] <= 0 AND ub[j] >= 0:
                score += 30
            IF obj[j] * coeff < 0:
                score += 20

            IF score > bestScore:
                bestScore := score
                bestVar := j

        // Make best variable basic
        IF bestVar >= 0:
            basisHeader[row] := bestVar
            varStatus[bestVar] := row
        ELSE:
            basisHeader[row] := n + row
            varStatus[n + row] := row

    state.phase := CHECK_NEED_PHASE_I(basisHeader, sense)

    RETURN 0
```

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(n + m) if all slacks selected
- **Average case:** O(m * n) for scoring all variables
- **Worst case:** O(m * n)

### 5.2 Space Complexity

- **Auxiliary space:** O(n + m) for varStatus and basisHeader

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| varStatus allocation fails | 1001 | Cannot allocate |
| basisHeader allocation fails | 1001 | Cannot allocate |

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| All inequalities | No '=' constraints | All slacks basic, Phase II |
| All equalities | No '<' or '>' | All structural basic, likely Phase I |
| Single variable | numVars=1 | That variable must be basic if equality exists |
| Zero row | Row with all zeros | Slack selected |

## 8. Thread Safety

**Thread-safe:** Yes

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_alloc | Memory | Allocate basis arrays |
| cxf_free | Memory | Free on error cleanup |
| fabs | C math | Absolute value |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_solve_lp | Simplex | After init, before preprocess |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_simplex_init | Allocates state used here |
| cxf_simplex_preprocess | Called after this |
| cxf_basis_refactor | Factorizes the selected basis |

## 11. Design Notes

### 11.1 Design Rationale

**Slack preference for inequalities:** Slack variables have unit coefficients, making them numerically stable choices.

**Multi-factor scoring:** Balances numerical stability, optimality proximity, and bound structure.

**Fallback to Phase I:** When equality constraints cannot find good structural basics, Phase I is needed.

## 12. References

- Bixby, R.E. (1992). "Implementing the Simplex Method: The Initial Basis"
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
