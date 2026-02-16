# cxf_validate_vartypes

**Module:** Validation
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Validates variable type codes and adjusts bounds for special variable types. This function ensures that all variable types in a model are legal (Continuous, Binary, Integer, Semi-continuous, Semi-integer) and enforces bound constraints for binary variables by clamping them to the range [0, 1].

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| model | CxfModel* | Model containing variable types to validate | Valid pointer | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | 0 if valid, error code if invalid type found |

### 2.3 Side Effects

- Binary variable bounds are modified (clamped to [0, 1])
- Model's MatrixData structure is modified (bounds arrays)

## 3. Contract

### 3.1 Preconditions

- Model pointer must be valid
- Model's MatrixData structure must be initialized
- Variable types array must contain uppercase characters (caller's responsibility)

### 3.2 Postconditions

- All variable types are valid (C, B, I, S, or N)
- Binary variable bounds are within [0, 1]
- Non-binary variable bounds are unchanged
- If error returned, model state is unspecified (may be partially modified)

### 3.3 Invariants

- Number of variables does not change
- Variable types themselves are not modified (only bounds)

## 4. Algorithm

### 4.1 Overview

Iterates through all variables in the model, validating that each type code is one of the five legal values. For binary variables, enforces the [0, 1] constraint by clamping both lower and upper bounds. After clamping, verifies that lb ≤ ub for binary variables.

### 4.2 Detailed Steps

1. Extract MatrixData pointer from model. If NULL, return success.
2. Get numVars from MatrixData. If ≤ 0, return success.
3. Get pointers to vtype, lb, and ub arrays. If vtype is NULL, return success (all continuous).
4. For each variable i from 0 to numVars-1:
   a. Read variable type t = vtype[i]
   b. Check if t is one of: 'C', 'B', 'I', 'S', 'N'
   c. If not, return error code for invalid argument
   d. If t == 'B' (Binary):
      - Clamp lb[i] to range [0, 1]: `lb[i] = max(0, min(1, lb[i]))`
      - Clamp ub[i] to range [0, 1]: `ub[i] = max(0, min(1, ub[i]))`
      - Verify lb[i] ≤ ub[i]. If not, return error.
5. Return success.

### 4.3 Pseudocode

```
FUNCTION validate_vartypes(model):
    matrix ← model.matrix
    IF matrix = NULL THEN RETURN SUCCESS

    numVars ← matrix.numVars
    IF numVars ≤ 0 THEN RETURN SUCCESS

    vtype ← matrix.vtype
    IF vtype = NULL THEN RETURN SUCCESS

    lb ← matrix.lb
    ub ← matrix.ub

    FOR i FROM 0 TO numVars-1 DO
        t ← vtype[i]

        IF t ∉ {'C', 'B', 'I', 'S', 'N'} THEN
            RETURN ERROR_INVALID_ARGUMENT
        END IF

        IF t = 'B' THEN
            # Clamp binary bounds to [0, 1]
            IF lb[i] < 0 THEN
                lb[i] ← 0
            ELSE IF lb[i] > 1 THEN
                lb[i] ← 1
            END IF

            IF ub[i] < 0 THEN
                ub[i] ← 0
            ELSE IF ub[i] > 1 THEN
                ub[i] ← 1
            END IF

            # Check feasibility
            IF lb[i] > ub[i] THEN
                RETURN ERROR_INVALID_ARGUMENT
            END IF
        END IF
    END FOR

    RETURN SUCCESS
END FUNCTION
```

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) - NULL matrix or numVars ≤ 0
- **Average case:** O(n) - full scan required
- **Worst case:** O(n) - full scan required

Where n = numVars (number of variables).

### 5.2 Space Complexity

- **Auxiliary space:** O(1)
- **Total space:** O(1)

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| Invalid type character | 1003 (CXF_ERR_INVALID_ARGUMENT) | Type not in {C,B,I,S,N} |
| Binary infeasible bounds | 1003 (CXF_ERR_INVALID_ARGUMENT) | Binary variable has lb > ub after clamping |

### 6.2 Error Behavior

Returns immediately upon finding first invalid type or infeasible binary bounds. Model may be in partially modified state (some binary bounds clamped before error).

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| All continuous | vtype = ['C', 'C'] | Return SUCCESS, no bounds modified |
| Valid binary | vtype = ['B'], lb = [0], ub = [1] | Return SUCCESS, no modification needed |
| Binary out of range | vtype = ['B'], lb = [-1], ub = [2] | Clamp to lb = [0], ub = [1], return SUCCESS |
| Binary infeasible | vtype = ['B'], lb = [2], ub = [0.5] | Clamp to lb = [1], ub = [0.5], then ERROR (1 > 0.5) |
| Invalid type | vtype = ['X'] | Return ERROR |
| Lowercase type | vtype = ['b'] | Return ERROR (caller must uppercase) |
| Mixed types | vtype = ['C', 'B', 'I'] | Validate all, clamp binary only |

## 8. Thread Safety

**Thread-safe:** No

Function modifies model state (bounds arrays). Must not be called concurrently with other operations on the same model.

**Synchronization required:** Model-level locking

## 9. Dependencies

### 9.1 Functions Called

None (direct memory access and arithmetic only).

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_newmodel | Model | After initial variable setup |
| cxf_addvars | Model | When adding variables with types |
| cxf_chgvtype | Model | After changing variable type |
| cxf_read | I/O | When loading model from file |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_validate_array | Validates numeric arrays (bounds) |
| cxf_is_mip_model | Checks if model has integer variables |

## 11. Design Notes

### 11.1 Design Rationale

**Variable Type Semantics:**
- **Continuous (C):** Any real value in [lb, ub]
- **Binary (B):** Only values {0, 1}
- **Integer (I):** Integer values in [lb, ub]
- **Semi-continuous (S):** Either 0 or continuous in [lb, ub]
- **Semi-integer (N):** Either 0 or integer in [lb, ub]

**Why clamp binary bounds:** Binary variables must have domain {0, 1} by definition. If a user specifies lb = -5, ub = 10 for a binary variable, the only meaningful bounds are [0, 1]. Clamping is more user-friendly than rejecting the input.

**Why check feasibility after clamping:** If user specifies lb = 2, ub = 0.5 for a binary variable, clamping gives lb = 1, ub = 0.5, which is infeasible (empty domain). This must be rejected.

**Why not validate semi-continuous/semi-integer bounds:** The specification for S and N types typically requires lb > 0, but this may be validated elsewhere or adjusted during presolve. Conservative approach is to only enforce binary constraints here.

### 11.2 Performance Considerations

- O(n) scan unavoidable (must check every variable type)
- Binary variables require additional bound adjustments
- Not a bottleneck (called once during model construction)
- Typical time: ~1-10 microseconds for 1000 variables

### 11.3 Future Considerations

Could add validation for semi-continuous/semi-integer lower bound requirements (lb > 0), but this may be deferred to presolve or solve phase.

## 12. References

- Convexfeld documentation: Variable types (VType attribute)
- Standard MIP formulation conventions

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
