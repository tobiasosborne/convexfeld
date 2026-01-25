# cxf_is_multi_obj

**Module:** Utilities
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Determines whether a model uses multi-objective optimization (NumObj > 1). Convexfeld supports hierarchical or blended multi-objective optimization where multiple objective functions are defined with priorities or weights. This function checks if the model has more than one objective layer, which affects solver algorithm selection.


## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| model | CxfModel* | Pointer to model structure | Valid pointer or NULL | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | 1 if model has multiple objectives (NumObj > 1), 0 otherwise |

### 2.3 Side Effects

None (read-only check)

## 3. Contract

### 3.1 Preconditions

- [ ] None (handles NULL gracefully)

### 3.2 Postconditions

- [ ] Returns 1 if NumObj > 1
- [ ] Returns 0 if NumObj ≤ 1, model is NULL, or matrix is NULL
- [ ] No model state modified

### 3.3 Invariants

- [ ] Function is deterministic
- [ ] Model structure not modified

## 4. Algorithm

### 4.1 Overview

The function performs a simple check of the number of objective functions defined in the model. It accesses the MatrixData structure to read the numObjectives field and compares it to 1.

### 4.2 Detailed Steps

1. Validate model pointer
   - If NULL, return 0 (default to single objective)
2. Access MatrixData structure from model
   - If NULL, return 0 (no data, default to single objective)
3. Read numObjectives field from MatrixData
   - **Note:** Exact offset uncertain, estimated at 
4. Compare numObjectives to 1
   - If > 1, return 1 (multi-objective)
   - Otherwise, return 0 (single objective)

### 4.3 Mathematical Foundation

Multi-objective optimization problems have the form:
```
Optimize (f₁(x), f₂(x), ..., fₖ(x))
subject to constraints
```

Where k > 1 objectives can be:
- **Hierarchical (lexicographic):** Optimize f₁ first, then f₂ subject to f₁ being optimal, etc.
- **Blended (weighted sum):** Optimize w₁f₁ + w₂f₂ + ... + wₖfₖ

This function simply checks if k > 1.

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) - two pointer reads and one comparison
- **Average case:** O(1)
- **Worst case:** O(1)

### 5.2 Space Complexity

- **Auxiliary space:** O(1) - no allocations
- **Total space:** O(1)

## 6. Error Handling

### 6.1 Error Conditions

This function returns 0 or 1, not error codes. Invalid inputs (NULL) return 0 (safe default).

| Condition | Error Code | Description |
|-----------|------------|-------------|
| NULL model | N/A | Returns 0 (treated as single objective) |
| NULL matrix | N/A | Returns 0 (treated as single objective) |

### 6.2 Error Behavior

Function never fails. All error conditions return 0 (safe default assuming single objective).

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| NULL model | model = NULL | Returns 0 |
| NULL matrix | model->matrix = NULL | Returns 0 |
| Single objective | NumObj = 1 | Returns 0 |
| Two objectives | NumObj = 2 | Returns 1 |
| Multiple objectives | NumObj = 5 | Returns 1 |

## 8. Thread Safety

**Thread-safe:** Yes (conditionally)

The function is thread-safe for read-only access because:
- Only reads model data
- No mutable state modified
- No synchronization primitives needed for reads

However, concurrent modification of model (adding/removing objectives) creates race condition.

**Synchronization required:** Only if model is being modified concurrently

## 9. Dependencies

### 9.1 Functions Called

None (direct field access)

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| Solver selection logic | Optimization | Determine which optimization algorithm to use |
| Multi-objective solvers | Optimization | Check before multi-obj-specific operations |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_setobjectiven | API to add objective layers (modifies NumObj) |
| cxf_getintattr("NumObj") | Public API to query NumObj attribute |
| cxf_is_mip | Similar check for integer variables |
| cxf_is_qp | Similar check for quadratic objective |

## 11. Design Notes

### 11.1 Design Rationale

Simple boolean check enables efficient branching in solver selection logic without overhead of attribute query mechanism.

### 11.2 Performance Considerations

Extremely fast (< 5 CPU cycles). Can be called frequently in inner loops without performance impact.

### 11.3 Future Considerations

**IMPORTANT:** The offset  for numObjectives is an educated guess. Actual offset must be verified by:
3. Runtime analysis with multi-objective models

## 12. References

- Ehrgott, M. (2005). Multicriteria Optimization. Springer.

## 13. Validation Checklist

Before finalizing this spec, verify:

- [x] No code copied from implementation
- [x] Algorithm description is implementation-agnostic
- [x] All parameters documented
- [ ] **Offset verification required** - numObjectives location
- [x] Complexity analysis complete
- [x] Edge cases identified
- [x] A competent developer could implement from this spec alone (pending offset verification)

---

*Reviewed by: Pending*
*Status: TENTATIVE - Requires offset verification*

## VERIFICATION REQUIRED

The numObjectives field offset is speculative. To verify:

2. Trace where it writes the objective count
3. Confirm offset in MatrixData structure
4. Update this specification with verified offset

---
*Convexfeld LP Solver Specification*
*Based on published optimization literature*
