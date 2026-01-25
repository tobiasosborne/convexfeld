# cxf_basis_warm

**Module:** Basis Operations
**Spec Version:** 1.0

## 1. Purpose

Initializes solver state using a warm start basis from a previous solve or related problem. Validates the provided basis against the current problem dimensions, repairs any inconsistencies, and sets up the factorization. A good warm start can dramatically reduce iteration count compared to cold start.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| state | SolverState* | Solver state to initialize | Non-null, valid state | Yes |
| warmBasis | BasisSnapshot* | Basis from previous solve | Non-null, valid | Yes |
| env | CxfEnv* | Environment with parameters | Non-null, valid env | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | Error code: 0=success, positive=warnings, negative=error |

### 2.3 Side Effects

- Copies basis header from warm start
- Copies variable status from warm start
- May repair basis if inconsistencies found
- Triggers basis refactorization
- Updates warm start statistics

## 3. Contract

### 3.1 Preconditions

- [ ] State is initialized with problem dimensions
- [ ] Warm basis has compatible dimensions (or fewer variables)
- [ ] Problem bounds are set

### 3.2 Postconditions

- [ ] state.basisHeader initialized
- [ ] state.varStatus initialized
- [ ] Basis factorization computed
- [ ] Warning returned if repairs were needed

### 3.3 Invariants

- [ ] Problem data unchanged
- [ ] Exactly m basic variables after initialization

## 4. Algorithm

### 4.1 Overview

Warm starting involves three phases: validation (checking the warm basis against the current problem), repair (fixing any issues found), and initialization (copying data and factorizing).

### 4.2 Detailed Steps

1. **Dimension check**:
   - If warmBasis.numConstrs != state.numConstrs: handle constraint change
   - If warmBasis.numVars != state.numVars: handle variable change

2. **Variable mapping**:
   - Create mapping from old variable indices to new
   - Mark variables that no longer exist
   - Mark new variables as candidates for basis

3. **Copy and validate basis header**:
   - For each row in warm basis:
     - Map variable index to current problem
     - If variable no longer exists: mark row for repair
     - If variable bounds changed significantly: flag warning

4. **Repair invalid entries**:
   - For each row needing repair:
     - Select slack variable for that row
     - Or select structural with good properties

5. **Copy variable status**:
   - Map status from warm basis
   - New variables: set to nonbasic at appropriate bound
   - Removed variables: ignore

6. **Verify basic count**:
   - Count basic variables
   - If count < m: add slacks
   - If count > m: remove excess (shouldn't happen)

7. **Factorize basis**:
   - Call cxf_basis_refactor
   - If factorization fails: fall back to crash

### 4.3 Pseudocode

```
WARM_START(state, warmBasis, env):
    m := state.numConstrs
    n := state.numVars
    repairs := 0

    // Handle dimension mismatch
    IF warmBasis.numConstrs != m OR warmBasis.numVars != n:
        // Create variable mapping for modified problem
        mapping := CREATE_MAPPING(warmBasis, state)

    // Copy and validate basis header
    FOR row := 0 TO m - 1:
        IF row < warmBasis.numConstrs:
            oldVar := warmBasis.basisHeader[row]
            newVar := APPLY_MAPPING(mapping, oldVar)
            IF newVar < 0:  // Variable no longer exists
                state.basisHeader[row] := n + row  // Use slack
                repairs++
            ELSE:
                state.basisHeader[row] := newVar
        ELSE:
            state.basisHeader[row] := n + row  // New row, use slack
            repairs++

    // Copy variable status
    FOR j := 0 TO n + m - 1:
        IF j < warmBasis.numVars + warmBasis.numConstrs:
            state.varStatus[j] := MAP_STATUS(warmBasis, j, mapping)
        ELSE:
            // New variable: nonbasic at best bound
            state.varStatus[j] := CHOOSE_BOUND(state.lb[j], state.ub[j])

    // Verify and repair
    IF COUNT_BASIC(state) != m:
        REPAIR_BASIS_COUNT(state)
        repairs++

    // Factorize
    err := cxf_basis_refactor(state, env)
    IF err != 0:
        // Fall back to crash
        RETURN cxf_simplex_crash(state, env)

    IF repairs > 0:
        RETURN repairs  // Warning: basis was repaired
    RETURN 0
```

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(n + m) for copy and map
- **Average case:** O(n + m) plus O(m^2) for refactorization
- **Worst case:** O(m^3) if refactorization is expensive

### 5.2 Space Complexity

- O(n) for variable mapping
- Standard space for refactorization

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| Factorization fails | -1 | Basis singular after repair |
| Massive mismatch | -2 | Problem changed too much |
| Invalid warm basis | 1003 | Warm basis corrupted |

### 6.2 Return Values

| Value | Meaning |
|-------|---------|
| 0 | Success, no repairs needed |
| 1-100 | Success, number of repairs made |
| -1 | Failed, fell back to crash |
| -2 | Failed completely |

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Identical problem | Same dimensions | Direct copy |
| Added variables | n increased | New vars nonbasic |
| Removed variables | n decreased | Replace in basis |
| Added constraints | m increased | Use slacks for new rows |
| Removed constraints | m decreased | Subset of basis |

## 8. Thread Safety

**Thread-safe:** No (modifies state extensively)

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_basis_refactor | Basis | Factorize repaired basis |
| cxf_simplex_crash | Simplex | Fallback if warm start fails |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_simplex_init | Simplex | When warm start available |
| cxf_solve_lp | Simplex | Model reoptimization |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_basis_snapshot | Creates basis for warm start |
| cxf_simplex_crash | Alternative cold start |
| cxf_basis_diff | Analyze warm start quality |

## 11. Design Notes

### 11.1 Design Rationale

**Repair over reject:** Minor basis issues are repaired rather than rejecting the warm start entirely. Even a partially valid warm start is better than cold start.

**Fallback to crash:** If warm start completely fails, gracefully degrade to crash rather than error.

### 11.2 Performance Considerations

- Warm start typically saves 50-90% of iterations
- Even repaired warm start usually better than crash
- Factorization is the expensive part

### 11.3 Future Considerations

- Warm start from interior point solution
- Incremental factorization update

## 12. References

- Bixby, R.E. (1992). "Implementing the Simplex Method: The Initial Basis"
- standard LP literature on warm starting techniques

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
