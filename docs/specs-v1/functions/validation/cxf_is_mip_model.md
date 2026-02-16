# cxf_is_mip_model

**Module:** Validation
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Determines whether a model contains integer-type variables, classifying it as a Mixed-Integer Program (MIP) rather than a pure Linear Program (LP). This is a focused check used for algorithm selection and model type reporting.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| model | CxfModel* | Model to check for integer variables | Valid pointer | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | 1 if model has integer variables, 0 if all continuous |

### 2.3 Side Effects

None (read-only predicate function).

## 3. Contract

### 3.1 Preconditions

- Model pointer should be valid (NULL is handled gracefully)

### 3.2 Postconditions

- Return value indicates presence of integer-type variables
- Model state is unchanged
- Result based on variable type array

### 3.3 Invariants

- Model structure is not modified
- Function has no global side effects

## 4. Algorithm

### 4.1 Overview

Scans the model's variable type array looking for any non-continuous variable. Returns immediately upon finding the first integer-type variable (Binary, Integer, Semi-continuous, or Semi-integer).

### 4.2 Detailed Steps

1. Validate model and matrix pointers. If NULL, return 0.
2. Retrieve number of variables. If ≤ 0, return 0.
3. Retrieve variable type array pointer. If NULL, return 0 (all continuous by default).
4. For each variable i from 0 to numVars-1:
   a. Read type code vtype[i]
   b. If type ≠ 'C' (Continuous), return 1 immediately
5. If loop completes without finding integer variable, return 0.

### 4.3 Pseudocode

```
FUNCTION is_mip(model):
    IF model = NULL THEN RETURN 0

    matrix ← model.matrix
    IF matrix = NULL THEN RETURN 0

    numVars ← matrix.numVars
    IF numVars ≤ 0 THEN RETURN 0

    vtype ← matrix.vtype
    IF vtype = NULL THEN RETURN 0

    FOR i FROM 0 TO numVars-1 DO
        IF vtype[i] ≠ 'C' THEN
            RETURN 1  # Found integer variable
        END IF
    END FOR

    RETURN 0  # All continuous
END FUNCTION
```

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) - NULL checks or first variable is integer
- **Average case:** O(1) - early exit on first integer variable (common)
- **Worst case:** O(n) - all variables are continuous

Where n = number of variables.

### 5.2 Space Complexity

- **Auxiliary space:** O(1)
- **Total space:** O(1)

## 6. Error Handling

### 6.1 Error Conditions

None - function cannot fail.

### 6.2 Error Behavior

Returns 0 for NULL or invalid inputs (conservative: assumes pure LP).

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| NULL model | model = NULL | Return 0 |
| Empty model | numVars = 0 | Return 0 |
| NULL vtype array | vtype = NULL | Return 0 (all continuous) |
| All continuous | All vtype[i] = 'C' | Return 0 |
| One binary | One vtype[i] = 'B' | Return 1 |
| One integer | One vtype[i] = 'I' | Return 1 |
| Semi-continuous | One vtype[i] = 'S' | Return 1 |
| Semi-integer | One vtype[i] = 'N' | Return 1 |
| Mixed types | Some 'C', some 'I' | Return 1 |

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
| cxf_check_model_flags1 | Validation | As part of broader MIP check |
| Solver dispatch | Solver | Determine MIP vs LP algorithms |
| Attribute reporting | Attributes | IsMIP attribute |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_check_model_flags1 | Superset check (includes SOS, general constraints) |
| cxf_validate_vartypes | Validates variable type codes |

## 11. Design Notes

### 11.1 Design Rationale

**Variable Types That Make a MIP:**
- **Binary (B):** Integer restricted to {0, 1}
- **Integer (I):** General integer values
- **Semi-continuous (S):** Either 0 or continuous range [lb, ub]
- **Semi-integer (N):** Either 0 or integer range {lb, ..., ub}

All of these require branch-and-bound algorithms. Even semi-continuous and semi-integer variables, despite allowing continuous values, require integer branching logic.

**Why NULL vtype means continuous:** If no types are explicitly set, Convexfeld defaults all variables to continuous. This is the standard API behavior.

**Difference from cxf_check_model_flags1:** This function checks ONLY variable types, while flags1 also checks for SOS constraints and general constraints. This is a more focused check.

### 11.2 Performance Considerations

- Designed for early exit (returns on first integer variable)
- Most MIP models have integer variables near the beginning
- Average case is O(1) in practice
- Pure LP models require O(n) scan but are less common
- Function likely inlined in many places

### 11.3 Future Considerations

None - function is complete for its focused purpose.

## 12. References

- Mixed-Integer Programming theory
- Convexfeld variable type documentation
- IsMIP model attribute

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
