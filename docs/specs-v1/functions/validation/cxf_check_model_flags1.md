# cxf_check_model_flags1

**Module:** Validation
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Determines whether a model contains Mixed-Integer Programming (MIP) features that require branch-and-bound algorithms rather than pure continuous optimization. This check is used during solver dispatch to select appropriate solution algorithms.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| model | CxfModel* | Model to check for MIP features | Valid pointer | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | 1 if model has MIP features, 0 if pure continuous |

### 2.3 Side Effects

None (read-only analysis function).

## 3. Contract

### 3.1 Preconditions

- Model pointer should be valid (NULL is handled gracefully)

### 3.2 Postconditions

- Return value indicates presence of MIP features
- Model state is unchanged
- Result is based on current model structure

### 3.3 Invariants

- Model structure is not modified
- Function has no global side effects

## 4. Algorithm

### 4.1 Overview

Examines the model for three categories of features that require integer programming algorithms: integer-type variables, Special Ordered Set (SOS) constraints, and certain general constraints. Returns 1 if any such feature is present.

### 4.2 Detailed Steps

1. Validate model and matrix pointers. If NULL, return 0.
2. Check for integer-type variables:
   a. Retrieve variable type array and count
   b. Scan array for any non-continuous type ('B', 'I', 'S', 'N')
   c. If found, return 1 (early exit)
3. Check for SOS constraints:
   a. Read SOS constraint count
   b. If count > 0, return 1
4. Check for general constraints:
   a. Read general constraint count
   b. If count > 0, return 1
5. If all checks pass without finding MIP features, return 0.

### 4.3 Pseudocode

```
FUNCTION has_mip_features(model):
    IF model = NULL THEN RETURN 0

    matrix ← model.matrix
    IF matrix = NULL THEN RETURN 0

    # Check for integer variables
    numVars ← matrix.numVars
    vtype ← matrix.vtype

    IF vtype ≠ NULL AND numVars > 0 THEN
        FOR i FROM 0 TO numVars-1 DO
            IF vtype[i] ≠ 'C' THEN
                RETURN 1  # Found integer variable
            END IF
        END FOR
    END IF

    # Check for SOS constraints
    sosCount ← matrix.sosCount
    IF sosCount > 0 THEN
        RETURN 1
    END IF

    # Check for general constraints
    genConstrCount ← matrix.genConstrCount
    IF genConstrCount > 0 THEN
        RETURN 1
    END IF

    RETURN 0  # Pure continuous model
END FUNCTION
```

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) - NULL matrix or first variable is integer
- **Average case:** O(n) - scan variables if no other features present
- **Worst case:** O(n) - scan all variables if pure LP

Where n = number of variables.

### 5.2 Space Complexity

- **Auxiliary space:** O(1)
- **Total space:** O(1)

## 6. Error Handling

### 6.1 Error Conditions

None - function cannot fail.

### 6.2 Error Behavior

Returns 0 for NULL or invalid inputs (conservative: assumes pure continuous).

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| NULL model | model = NULL | Return 0 |
| All continuous | All variables type 'C' | Return 0 |
| One binary variable | One variable type 'B' | Return 1 |
| Pure LP with SOS | Continuous + SOS constraints | Return 1 |
| Pure LP with general constraints | Continuous + general constraints | Return 1 |
| Empty model | numVars = 0 | Return 0 |

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
| Solver dispatch logic | Solver | Determine if MIP algorithms needed |
| Presolve analysis | Presolve | Model classification |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_is_mip_model | Checks only integer variables (subset) |
| cxf_check_model_flags2 | Checks for quadratic features |
| cxf_is_quadratic | Checks for QP objective |
| cxf_is_socp | Checks for conic constraints |

## 11. Design Notes

### 11.1 Design Rationale

**MIP Features Detected:**
1. **Integer Variables:** Binary (B), Integer (I), Semi-continuous (S), Semi-integer (N) all require branch-and-bound.
2. **SOS Constraints:** Special Ordered Sets require specialized branching even with continuous variables.
3. **General Constraints:** Some types (AND, OR, INDICATOR) imply integer behavior.

**Why SOS requires MIP:** SOS constraints (at most one/two variables nonzero) are enforced through special branching rules in branch-and-bound, even if the variables themselves are continuous.

**Why general constraints imply MIP:** Constraints like AND, OR require binary indicator variables. MAX, MIN, ABS can be formulated continuously but are often treated as MIP features.

### 11.2 Performance Considerations

- Designed for early exit on first MIP feature found
- Variable scan is O(n) worst case but often exits early
- SOS and general constraint checks are O(1) counter reads
- Not a bottleneck (called once during solve initialization)

### 11.3 Future Considerations

Could refine general constraint check to distinguish types that truly require MIP (AND, OR) from those that don't (MAX, MIN with continuous relaxation).

## 12. References

- Mixed-Integer Programming literature
- Branch-and-bound algorithms
- Convexfeld solver architecture

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
