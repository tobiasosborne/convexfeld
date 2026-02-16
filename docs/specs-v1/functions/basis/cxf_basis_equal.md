# cxf_basis_equal

**Module:** Basis Operations
**Spec Version:** 1.0

## 1. Purpose

Tests whether two basis states are identical, comparing basis headers and optionally variable status arrays. Returns true if both bases have the same set of basic variables, false otherwise. This is used for cycle detection and basis comparison during reoptimization.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| basis1 | BasisSnapshot* | First basis snapshot | Non-null, valid | Yes |
| basis2 | BasisSnapshot* | Second basis snapshot | Non-null, valid | Yes |
| checkStatus | int | Also compare variable status (0=no, 1=yes) | 0 or 1 | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | 1 if equal, 0 if different |

### 2.3 Side Effects

None (pure comparison function)

## 3. Contract

### 3.1 Preconditions

- [ ] Both snapshots are valid
- [ ] Both snapshots have same dimensions

### 3.2 Postconditions

- [ ] Returns 1 if bases are identical
- [ ] Returns 0 if any difference found

### 3.3 Invariants

- [ ] Input snapshots unchanged

## 4. Algorithm

### 4.1 Overview

Compare basis headers element by element. If checkStatus is set, also compare the variable status arrays. Note that basis headers may have the same set of basic variables in different row positions; the comparison checks set equality, not positional equality.

### 4.2 Detailed Steps

1. **Check dimensions**:
   - If dimensions differ, return 0

2. **Compare basis headers as sets**:
   - Sort or hash both headers
   - Compare for set equality

3. **If checkStatus**:
   - Compare varStatus arrays element-wise
   - Return 0 on any difference

4. **Return 1** if all comparisons passed

### 4.3 Pseudocode

```
EQUAL(basis1, basis2, checkStatus):
    IF basis1.numConstrs != basis2.numConstrs:
        RETURN 0
    IF basis1.numVars != basis2.numVars:
        RETURN 0

    m := basis1.numConstrs

    // Compare as sets (order may differ)
    set1 := SORTED_COPY(basis1.basisHeader, m)
    set2 := SORTED_COPY(basis2.basisHeader, m)

    FOR i := 0 TO m-1:
        IF set1[i] != set2[i]:
            RETURN 0

    IF checkStatus:
        n := basis1.numVars
        FOR i := 0 TO n + m - 1:
            IF basis1.varStatus[i] != basis2.varStatus[i]:
                RETURN 0

    RETURN 1
```

## 5. Complexity

### 5.1 Time Complexity

- O(m log m) for sorting and comparison
- O(n + m) additional if checkStatus

### 5.2 Space Complexity

- O(m) for sorted copies

## 6. Error Handling

### 6.1 Error Conditions

None - returns 0 for any invalid input

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Same snapshot | basis1 == basis2 | Returns 1 |
| Empty bases | m = 0 | Returns 1 |
| Different dimensions | m1 != m2 | Returns 0 |
| Same set, different order | Permuted headers | Returns 1 |

## 8. Thread Safety

**Thread-safe:** Yes (read-only)

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| qsort | C stdlib | Sort for set comparison |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_simplex_iterate | Simplex | Cycle detection |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_basis_diff | Detailed difference (when not equal) |
| cxf_basis_snapshot | Creates snapshots to compare |

## 11. Design Notes

### 11.1 Design Rationale

**Set comparison:** Bases are equal if they contain the same basic variables, regardless of row assignment. This is the mathematically meaningful comparison.

**Optional status check:** Sometimes only the basic set matters; other times the exact nonbasic positions (at lower vs upper bound) matter.

## 12. References

- Bland, R.G. (1977). "New Finite Pivoting Rules for the Simplex Method" - Cycle detection via basis comparison

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
