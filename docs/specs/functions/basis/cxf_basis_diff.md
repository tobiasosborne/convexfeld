# cxf_basis_diff

**Module:** Basis Operations
**Spec Version:** 1.0

## 1. Purpose

Computes the difference between two basis states, identifying which variables entered and left the basis between snapshots. This is useful for warm-starting from a modified problem or analyzing pivot sequences. The function returns lists of entering and leaving variables along with statistics about the basis change.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| basis1 | BasisSnapshot* | First (older) basis snapshot | Non-null, valid | Yes |
| basis2 | BasisSnapshot* | Second (newer) basis snapshot | Non-null, valid | Yes |
| result | BasisDiff* | Output structure for differences | Non-null, allocated | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | Error code: 0=success, 1003=dimension mismatch |

### 2.3 Side Effects

- Allocates entering/leaving variable lists in result
- Computes and stores diff statistics

## 3. Contract

### 3.1 Preconditions

- [ ] Both snapshots are valid
- [ ] Both snapshots have same dimensions
- [ ] Result structure is allocated

### 3.2 Postconditions

- [ ] result.entering contains variables in basis2 but not basis1
- [ ] result.leaving contains variables in basis1 but not basis2
- [ ] result.numChanges = |entering| = |leaving|

### 3.3 Invariants

- [ ] Input snapshots unchanged
- [ ] Entering and leaving lists have equal size

## 4. Algorithm

### 4.1 Overview

Compare the basis headers of both snapshots to identify variables that changed status. A variable is "entering" if it's basic in basis2 but nonbasic in basis1, and "leaving" if the opposite.

### 4.2 Detailed Steps

1. **Verify dimensions match**:
   - Check numVars1 == numVars2
   - Check numConstrs1 == numConstrs2

2. **Create basic variable sets**:
   - Set basicInBasis1 from basis1.basisHeader
   - Set basicInBasis2 from basis2.basisHeader

3. **Find entering variables**:
   - For each variable in basicInBasis2:
     - If not in basicInBasis1: add to entering list

4. **Find leaving variables**:
   - For each variable in basicInBasis1:
     - If not in basicInBasis2: add to leaving list

5. **Compute statistics**:
   - numChanges = size of entering list
   - percentChanged = numChanges / numConstrs * 100

### 4.3 Pseudocode

```
DIFF(basis1, basis2, result):
    IF basis1.numVars != basis2.numVars:
        RETURN DIMENSION_MISMATCH
    IF basis1.numConstrs != basis2.numConstrs:
        RETURN DIMENSION_MISMATCH

    m := basis1.numConstrs

    // Create sets of basic variables
    set1 := SET()
    set2 := SET()

    FOR i := 0 TO m-1:
        ADD(set1, basis1.basisHeader[i])
        ADD(set2, basis2.basisHeader[i])

    // Find differences
    entering := LIST()
    leaving := LIST()

    FOR var IN set2:
        IF var NOT IN set1:
            APPEND(entering, var)

    FOR var IN set1:
        IF var NOT IN set2:
            APPEND(leaving, var)

    result.entering := entering
    result.leaving := leaving
    result.numChanges := SIZE(entering)
    result.percentChanged := 100.0 * SIZE(entering) / m

    RETURN 0
```

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(m) using hash sets
- **Worst case:** O(m log m) using sorted comparison

### 5.2 Space Complexity

- O(m) for the two sets plus O(k) for result lists where k = number of changes

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| Dimension mismatch | 1003 | Snapshots have different sizes |
| Invalid snapshot | 1003 | Snapshot marked as invalid |

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Identical bases | Same basisHeader | Empty entering/leaving lists |
| Complete change | No overlap | All m variables in each list |
| Single change | One pivot | One entering, one leaving |

## 8. Thread Safety

**Thread-safe:** Yes (read-only on inputs)

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_alloc | Memory | Allocate result lists |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_basis_warm | Basis | Analyze basis change for warm start |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_basis_snapshot | Creates snapshots to compare |
| cxf_basis_equal | Simpler equality check |
| cxf_basis_warm | Uses diff for warm start |

## 11. Design Notes

### 11.1 Design Rationale

**Symmetric output:** Returning both entering and leaving lists allows caller to decide which perspective is useful.

**Statistics:** Percentage changed helps decide if warm start is worthwhile.

## 12. References

- Bixby, R.E. (1992). "Implementing the Simplex Method: The Initial Basis" - Warm start analysis

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
