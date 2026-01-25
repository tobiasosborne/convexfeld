# cxf_check_model_flags2

**Module:** Validation
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Determines whether a model contains quadratic or conic features that require the barrier (interior point) algorithm rather than simplex methods. This check is critical for solver dispatch as simplex algorithms cannot handle quadratic or conic constraints.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| model | CxfModel* | Model to check for quadratic/conic features | Valid pointer | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | 1 if model has quadratic/conic features, 0 if pure linear |

### 2.3 Side Effects

None (read-only analysis function).

## 3. Contract

### 3.1 Preconditions

- Model pointer should be valid (NULL is handled gracefully)

### 3.2 Postconditions

- Return value indicates presence of quadratic/conic features
- Model state is unchanged
- Result based on current model structure

### 3.3 Invariants

- Model structure is not modified
- Function has no global side effects

## 4. Algorithm

### 4.1 Overview

Examines the model for various types of non-linear features: quadratic objective terms, quadratic constraints, bilinear terms, and multiple conic constraint types (second-order, rotated, exponential, power cones). Returns 1 if any such feature is present.

### 4.2 Detailed Steps

1. Validate model and matrix pointers. If NULL, return 0.
2. Check for quadratic objective:
   a. Read quadratic objective term count
   b. If count > 0, return 1
3. Check for quadratic constraints:
   a. Read quadratic constraint count
   b. If count > 0, return 1
4. Check for bilinear constraints:
   a. Read bilinear term count
   b. If count > 0, return 1
5. Check for various conic constraint types:
   a. Second-order cone constraints
   b. Rotated cone constraints
   c. Exponential cone constraints
   d. Power cone constraints
   e. Other nonlinear constraint types
   f. If any count > 0, return 1
6. If all checks pass without finding quadratic/conic features, return 0.

### 4.3 Pseudocode

```
FUNCTION has_quadratic_or_conic_features(model):
    IF model = NULL THEN RETURN 0

    matrix ← model.matrix
    IF matrix = NULL THEN RETURN 0

    # Check quadratic objective
    IF matrix.quadObjTerms > 0 THEN
        RETURN 1
    END IF

    # Check quadratic constraints
    IF matrix.quadConstrCount > 0 THEN
        RETURN 1
    END IF

    # Check bilinear terms
    IF matrix.bilinearCount > 0 THEN
        RETURN 1
    END IF

    # Check conic constraint types
    IF matrix.socCount > 0 THEN RETURN 1
    IF matrix.rotatedConeCount > 0 THEN RETURN 1
    IF matrix.expConeCount > 0 THEN RETURN 1
    IF matrix.powConeCount > 0 THEN RETURN 1
    IF matrix.nlConstrCount > 0 THEN RETURN 1

    RETURN 0  # Pure linear model
END FUNCTION
```

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) - NULL matrix or first check finds feature
- **Average case:** O(1) - fixed number of counter checks
- **Worst case:** O(1) - all checks performed (fixed count)

### 5.2 Space Complexity

- **Auxiliary space:** O(1)
- **Total space:** O(1)

## 6. Error Handling

### 6.1 Error Conditions

None - function cannot fail.

### 6.2 Error Behavior

Returns 0 for NULL or invalid inputs (conservative: assumes pure linear).

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| NULL model | model = NULL | Return 0 |
| Pure LP | No quadratic/conic features | Return 0 |
| QP objective only | Quadratic objective, no constraints | Return 1 |
| QCP constraints only | Quadratic constraints, linear objective | Return 1 |
| SOCP | Second-order cone constraints | Return 1 |
| Mixed MIP + QP | Integer variables + quadratic objective | Return 1 |
| Empty model | All counters = 0 | Return 0 |

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
| cxf_optimize (internal) | Solver | Algorithm selection |
| Solver dispatch logic | Solver | Determine if barrier required |
| Presolve analysis | Presolve | Model classification |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_is_quadratic | Checks only for QP objective (subset) |
| cxf_is_socp | Checks for SOCP/QCP constraints (subset) |
| cxf_check_model_flags1 | Checks for MIP features (complementary) |

## 11. Design Notes

### 11.1 Design Rationale

**Quadratic/Conic Features Detected:**
1. **Quadratic Objective (QP):** Objective contains x'Qx terms
2. **Quadratic Constraints (QCP):** Constraints contain x'Qx terms
3. **Bilinear Terms:** Cross-product terms x_i * x_j
4. **Second-Order Cones (SOC):** ||Ax + b|| ≤ cx + d
5. **Rotated Cones:** ||Ax|| ≤ yz with y,z ≥ 0
6. **Exponential Cones:** y·exp(x/y) ≤ z
7. **Power Cones:** ||Ax||^p ≤ y^α · z^(1-α)

**Why barrier required:** All these features define non-polyhedral feasible regions. Simplex algorithms navigate vertices of polytopes and cannot handle curved boundaries. The barrier (interior point) method can handle these features by working within the feasible region.

**Model Classification:**
- flags2=0: Pure LP → simplex or barrier available
- flags2=1: QP/QCP/SOCP → barrier required

### 11.2 Performance Considerations

- O(1) operation: fixed number of integer comparisons
- Very fast: ~10-20 CPU cycles typical
- Not a bottleneck (called once during solve initialization)
- Designed for early exit (checks most common features first)

### 11.3 Future Considerations

Could add environment flag checks to force quadratic handling for debugging purposes, similar to related functions.

## 12. References

- Convex optimization: Boyd & Vandenberghe
- Interior point methods for conic programming
- Second-order cone programming formulations

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
