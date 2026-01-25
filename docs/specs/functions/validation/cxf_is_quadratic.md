# cxf_is_quadratic

**Module:** Validation
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Determines whether a model is a Quadratic Program (QP), meaning it has a quadratic objective function without disqualifying features such as quadratic constraints, bilinear terms, or other nonlinear elements. This distinction is important for algorithm selection and model classification.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| model | CxfModel* | Model to check for QP structure | Valid pointer | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | 1 if model is QP (quadratic objective only), 0 otherwise |

### 2.3 Side Effects

None (read-only classification function).

## 3. Contract

### 3.1 Preconditions

- Model pointer should be valid (NULL is handled gracefully)

### 3.2 Postconditions

- Return value indicates whether model is a "pure" QP
- Model state is unchanged
- Result based on objective and constraint structure

### 3.3 Invariants

- Model structure is not modified
- Function has no global side effects

## 4. Algorithm

### 4.1 Overview

Checks if the model has quadratic objective terms while ensuring it lacks features that would classify it as something other than a pure QP (such as quadratic constraints, bilinear terms, or nonlinear constraints). Environment flags can override and force QP classification.

### 4.2 Detailed Steps

1. Validate model and matrix pointers. If NULL, return 0.
2. Check environment override flag. If set, return 1 (forced QP handling).
3. Check for quadratic objective terms. If none, return 0.
4. Check for disqualifying features:
   a. Quadratic constraints - if present, return 0 (QCP, not QP)
   b. Bilinear terms - if present, return 0 (non-convex)
   c. Various nonlinear constraint types - if present, return 0
5. If quadratic objective present and no disqualifying features found, return 1.

### 4.3 Pseudocode

```
FUNCTION is_quadratic_program(model):
    IF model = NULL THEN RETURN 0

    matrix ← model.matrix
    IF matrix = NULL THEN RETURN 0

    env ← model.env

    # Check environment override
    IF env.quadraticForceFlag ≠ 0 THEN
        RETURN 1
    END IF

    # Must have quadratic objective
    IF matrix.quadObjTerms ≤ 0 THEN
        RETURN 0
    END IF

    # Check for disqualifying features
    IF matrix.quadConstrCount > 0 THEN RETURN 0
    IF matrix.bilinearCount > 0 THEN RETURN 0
    IF matrix.nlConstrType1 > 0 THEN RETURN 0
    IF matrix.nlConstrType2 > 0 THEN RETURN 0
    IF matrix.nlConstrType3 > 0 THEN RETURN 0
    IF matrix.nlConstrType4 > 0 THEN RETURN 0
    IF matrix.nlConstrType5 > 0 THEN RETURN 0

    RETURN 1  # Pure QP
END FUNCTION
```

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) - NULL checks or environment override
- **Average case:** O(1) - fixed number of counter checks
- **Worst case:** O(1) - all checks performed (fixed count)

### 5.2 Space Complexity

- **Auxiliary space:** O(1)
- **Total space:** O(1)

## 6. Error Handling

### 6.1 Error Conditions

None - function cannot fail.

### 6.2 Error Behavior

Returns 0 for NULL or invalid inputs.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| NULL model | model = NULL | Return 0 |
| Pure LP | No quadratic terms | Return 0 |
| QP | Quadratic objective, linear constraints | Return 1 |
| QCP | Quadratic objective + quadratic constraints | Return 0 |
| Bilinear | Quadratic objective + bilinear terms | Return 0 |
| Environment override | Force flag set | Return 1 |
| SOCP | No quadratic objective | Return 0 |

## 8. Thread Safety

**Thread-safe:** Yes

Function only reads model structure. Safe for concurrent calls.

**Synchronization required:** None

## 9. Dependencies

### 9.1 Functions Called

None (direct memory access only).

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_check_model_flags2 | Validation | Part of broader quadratic/conic check |
| Solver dispatch | Solver | Algorithm selection (QP-specific methods) |
| Model classification | Analysis | Determine problem class |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_check_model_flags2 | Superset (checks QP + QCP + SOCP) |
| cxf_is_socp | Complementary (checks constraints) |

## 11. Design Notes

### 11.1 Design Rationale

**QP Definition:** A Quadratic Program has:
- Objective: ½x'Qx + c'x
- Constraints: Ax ≤ b (linear only)

This is distinct from:
- **LP:** Linear objective and constraints
- **QCP:** Quadratic objective AND quadratic constraints
- **SOCP:** Conic constraints (special case of QCP)

**Why distinction matters:** Some algorithms (active set methods) are specialized for QP and more efficient than general barrier methods for QCP. Proper classification enables optimal algorithm selection.

**Environment override:** Allows forcing QP interpretation for debugging or parameter tuning purposes, bypassing normal checks.

**Disqualifying features:** Bilinear terms and nonlinear constraints indicate the problem is more complex than pure QP (potentially non-convex), requiring specialized handling.

### 11.2 Performance Considerations

- O(1) operation: fixed number of counter checks
- Very fast: ~20-30 CPU cycles typical
- Not a bottleneck
- Designed for early exit (environment flag checked first)

### 11.3 Future Considerations

Could distinguish between convex and non-convex QP by examining the Q matrix (positive semi-definite check), but this is likely done elsewhere.

## 12. References

- Quadratic Programming: Nocedal & Wright
- Convex Optimization: Boyd & Vandenberghe
- Active set methods for QP

## 13. Validation Checklist

- [x] No code copied from implementation
- [x] Algorithm description is implementation-agnostic
- [x] All parameters documented
- [x] All error conditions listed
- [x] Complexity analysis complete
- [x] Edge cases identified
- [x] A competent developer could implement from this spec alone

---

---
*Convexfeld LP Solver Specification*
*Based on published optimization literature*
