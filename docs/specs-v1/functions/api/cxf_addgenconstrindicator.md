# cxf_addgenconstrIndicator

**Module:** API Variable/Constraint Operations
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Add an indicator general constraint to an optimization model. Indicator constraints enforce linear constraints conditionally based on a binary variable's value, enabling if-then logic in mathematical programming models without requiring big-M reformulations.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| model | CxfModel* | Target model | Valid model pointer | Yes |
| name | const char* | Constraint identifier | Any string, NULL for default | No |
| binvar | int | Index of binary indicator variable | [0, numVars) | Yes |
| binval | int | Trigger value for indicator | 0 or 1 | Yes |
| nvars | int | Number of variables in linear constraint | >= 0 | Yes |
| ind | const int* | Variable indices for linear constraint | [0, numVars), NULL if nvars=0 | Conditional |
| val | const double* | Coefficients for linear constraint | Finite doubles, NULL if nvars=0 | Conditional |
| sense | char | Linear constraint sense | '<', '>', '=' | Yes |
| rhs | double | Right-hand side for linear constraint | Finite double | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | 0 on success, error code otherwise |

### 2.3 Side Effects

- Automatically forces binvar to binary type if not already
- Updates general constraint count
- Logs operation status

## 3. Contract

### 3.1 Preconditions

- [ ] model pointer must be valid
- [ ] Model not in modification-blocked state
- [ ] binvar must be a valid variable index
- [ ] binval must be exactly 0 or 1
- [ ] If nvars > 0, ind and val must be non-NULL
- [ ] All variable indices in ind must be valid
- [ ] All coefficient values must be finite
- [ ] RHS must be finite

### 3.2 Postconditions

- [ ] One indicator constraint added to general constraint storage
- [ ] Binary variable type automatically enforced on binvar
- [ ] Linear constraint data stored for conditional enforcement
- [ ] Constraint will be processed during model update

### 3.3 Invariants

- [ ] Input arrays remain unmodified
- [ ] Existing model structure unchanged until update
- [ ] Model remains valid after operation

## 4. Algorithm

### 4.1 Overview


### 4.2 Detailed Steps

1. Forward all parameters to internal general constraint implementation:
   - Pass model pointer
   - Pass constraint name
   - Pass type code 7 (INDICATOR)
   - Pass binvar (indicator variable index)
   - Pass binval (trigger value: 0 or 1)
   - Pass nvars (linear constraint size)
   - Pass ind (variable indices)
   - Pass val (coefficients)
   - Pass sense as integer (character widened to int)
   - Pass rhs (right-hand side)
2. Return status from internal implementation

### 4.3 Pseudocode

```
function cxf_addgenconstrIndicator(model, name, binvar, binval, nvars, ind, val, sense, rhs):
    return cxf_addgenconstr_impl(model, name, GENCONSTR_INDICATOR,
                                binvar, binval, nvars, ind, val,
                                (int)sense, rhs)
```

### 4.4 Mathematical Foundation

Indicator constraint semantics:

If binvar = binval, then Σ(val[i] × x[ind[i]]) {sense} rhs

Where:
- binvar ∈ {0, 1} (binary variable)
- binval ∈ {0, 1} (trigger value)
- Linear constraint activated only when condition met

Logical implication: binvar = binval ⟹ Σ(val[i] × x[ind[i]]) {sense} rhs

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) wrapper overhead + internal implementation cost
- **Average case:** O(nvars) for storing linear constraint data
- **Worst case:** O(nvars) with buffer allocation

Where nvars = number of variables in linear constraint

### 5.2 Space Complexity

- **Auxiliary space:** O(nvars) for constraint coefficient storage
- **Total space:** O(total general constraints × average size)

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| model is NULL or invalid | varies | From internal implementation |
| Model modification blocked | 1017 | MODEL_MODIFICATION |
| binvar out of range | 1006 | INVALID_INDEX |
| binval not 0 or 1 | 1003 | INVALID_ARGUMENT |
| nvars > 0 but ind/val NULL | 1002 | NULL_ARGUMENT |
| Variable index in ind out of range | 1006 | INVALID_INDEX |
| Coefficient is NaN or Inf | 1003 | INVALID_ARGUMENT |
| Invalid sense character | 1003 | INVALID_ARGUMENT |

### 6.2 Error Behavior

Error handling delegated to internal implementation. On error, no constraint is added and model remains in consistent state.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Empty linear constraint | nvars=0 | Valid, condition triggers 0 {sense} rhs |
| binval=0 | Constraint active when binvar=0 | Reverse indicator logic |
| binval=1 | Constraint active when binvar=1 | Standard indicator logic |
| binvar already binary | Variable already type 'B' | No type change needed |
| binvar continuous | Variable type 'C' | Automatically changed to 'B' |

## 8. Thread Safety

**Thread-safe:** No

**Synchronization required:** Caller must ensure exclusive model access

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_addgenconstr_impl | General Constraints | Internal implementation for all genconstr types |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| User code | Application | Standard API entry point |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_addgenconstrMax | Different genconstr type (type code 0) |
| cxf_addgenconstrMin | Different genconstr type (type code 1) |
| cxf_addgenconstrAbs | Different genconstr type (type code 2) |
| cxf_addgenconstrAnd | Different genconstr type (type code 3) |
| cxf_addgenconstrOr | Different genconstr type (type code 4) |
| cxf_getgenconstrIndicator | Retrieves indicator constraint data |
| cxf_delgenconstrs | Removes general constraints |

## 11. Design Notes

### 11.1 Design Rationale

The wrapper pattern allows a unified internal implementation for all 18 general constraint types while providing type-specific APIs for user convenience. This reduces code duplication and ensures consistent behavior across constraint types.

### 11.2 Performance Considerations

Indicator constraints are processed more efficiently than equivalent big-M reformulations because the solver can exploit the conditional structure directly. During branch-and-cut, indicator constraints guide branching decisions more effectively than large penalty coefficients.

### 11.3 Future Considerations

Additional general constraint types may be added using the same wrapper pattern with new type codes.

## 12. References

- Convexfeld Optimizer Reference Manual: General Constraint documentation
- Integer Programming: Branch-and-cut methods for indicator constraints

## 13. Validation Checklist

- [x] No code copied from implementation
- [x] Algorithm description is implementation-agnostic
- [x] All parameters documented
- [x] All error conditions listed (from internal implementation)
- [x] Complexity analysis complete
- [x] Edge cases identified
- [x] A competent developer could implement from this spec alone

---

---
*Convexfeld LP Solver Specification*
*Based on published optimization literature*
