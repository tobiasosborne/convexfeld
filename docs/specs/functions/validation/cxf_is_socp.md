# cxf_is_socp

**Module:** Validation
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Determines whether a model contains Second-Order Cone Programming (SOCP) or Quadratically Constrained Programming (QCP) features. This classification is critical for solver selection, as models with conic or quadratic constraints must use the barrier (interior point) algorithm rather than simplex methods.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| model | CxfModel* | Model to check for SOCP/QCP features | Valid pointer | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | 1 if model has SOCP/QCP features, 0 if pure linear |

### 2.3 Side Effects

None (read-only classification function).

## 3. Contract

### 3.1 Preconditions

- Model pointer should be valid (NULL is handled gracefully)

### 3.2 Postconditions

- Return value indicates presence of conic or quadratic constraints
- Model state is unchanged
- Result based on constraint structure

### 3.3 Invariants

- Model structure is not modified
- Function has no global side effects

## 4. Algorithm

### 4.1 Overview

Examines the model for various types of non-linear constraints including quadratic constraints, bilinear terms, and multiple conic constraint types (second-order cones, rotated cones, exponential cones, power cones). Environment flags can force SOCP handling. Returns 1 if any such feature is present.

### 4.2 Detailed Steps

1. Validate model and matrix pointers. If NULL, return 0.
2. Check environment override flag. If set, return 1.
3. Check for quadratic/conic constraint types:
   a. Quadratic constraints (general QCP)
   b. Second-order cone constraints (specific SOCP form)
   c. Bilinear constraints
   d. Second-order cone (SOC) - standard form
   e. Rotated cone constraints
   f. Exponential cone constraints (with environment flag check)
   g. Power cone constraints
   h. Other nonlinear constraint types
4. If any counter > 0, return 1.
5. If all checks pass without finding SOCP/QCP features, return 0.

### 4.3 Pseudocode

```
FUNCTION has_socp_or_qcp_features(model):
    IF model = NULL THEN RETURN 0

    matrix ← model.matrix
    IF matrix = NULL THEN RETURN 0

    env ← model.env

    # Check environment override
    IF env.quadraticFlag1 ≠ 0 THEN
        RETURN 1
    END IF

    # Check various constraint types
    IF matrix.qcpConstrCount > 0 THEN RETURN 1
    IF matrix.sosConstrCount > 0 THEN RETURN 1
    IF matrix.bilinearCount > 0 THEN RETURN 1
    IF matrix.socConstrCount > 0 THEN RETURN 1
    IF matrix.nlConstrType1 > 0 THEN RETURN 1
    IF matrix.rotatedConeCount > 0 THEN RETURN 1

    # Conditional check for exponential cones
    IF env.quadraticFlag2 ≠ 0 AND matrix.expConeCount > 0 THEN
        RETURN 1
    END IF

    IF matrix.powConeCount > 0 THEN RETURN 1

    RETURN 0  # Pure linear constraints
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

Returns 0 for NULL or invalid inputs (assumes pure linear).

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| NULL model | model = NULL | Return 0 |
| Pure LP | No nonlinear constraints | Return 0 |
| QCP | Quadratic constraints present | Return 1 |
| SOCP | Second-order cone constraints | Return 1 |
| Bilinear | Bilinear terms present | Return 1 |
| Rotated cone | Rotated cone constraints | Return 1 |
| Environment override | Force flag set | Return 1 |
| QP only | Quadratic objective, no constraints | Return 0 |

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
| Solver dispatch | Solver | Determine if barrier required |
| Algorithm selection | Solver | Choose between simplex and barrier |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_is_quadratic | Complementary (checks objective) |
| cxf_check_model_flags2 | Superset (checks objective + constraints) |

## 11. Design Notes

### 11.1 Design Rationale

**SOCP/QCP Constraint Types:**
1. **Quadratic Constraints (QCP):** x'Qx + q'x ≤ b
2. **Second-Order Cone (SOC):** ||Ax + b||₂ ≤ cx + d
3. **Rotated Cone:** ||Ax||₂ ≤ yz, with y,z ≥ 0
4. **Exponential Cone:** y·exp(x/y) ≤ z
5. **Power Cone:** ||Ax||ₚ ≤ y^α · z^(1-α)
6. **Bilinear:** x_i · x_j terms (non-convex in general)

**Why barrier required:** All these constraint types define non-polyhedral (curved) feasible regions. Simplex methods, which work by moving along edges of polytopes, cannot handle such constraints. The barrier (interior point) method navigates the interior of the feasible region and can handle these features.

**Algorithm implications:**
- If this returns 1, simplex is NOT available
- Barrier must be used (or branch-and-bound with barrier at nodes for MIQCP)
- If this returns 0, both simplex and barrier are available

**Environment flags:** Allow forcing SOCP handling or enabling experimental features (exponential cones controlled by secondary flag).

### 11.2 Performance Considerations

- O(1) operation: fixed number of counter checks
- Very fast: ~30-40 CPU cycles typical
- Not a bottleneck (called once during solve)
- Designed for early exit (most common features checked first)

### 11.3 Future Considerations

Could add more detailed constraint type analysis to distinguish convex from non-convex cases, but this is likely done in presolve.

## 12. References

- Second-Order Cone Programming: Lobo et al. (1998)
- Convex Optimization: Boyd & Vandenberghe, Chapter 4 (conic optimization)
- Interior Point Methods: Wright (1997)

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
