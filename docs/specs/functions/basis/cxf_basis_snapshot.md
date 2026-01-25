# cxf_basis_snapshot

**Module:** Basis Operations
**Spec Version:** 1.0

## 1. Purpose

Creates a snapshot (checkpoint) of the current basis state for later restoration. This allows the solver to implement backtracking when a sequence of pivots leads to numerical difficulties or cycling. The snapshot captures the basis header, variable status array, and optionally the LU factorization.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| state | SolverState* | Current solver state | Non-null, valid state | Yes |
| snapshot | BasisSnapshot* | Output structure for snapshot | Non-null, allocated | Yes |
| includeFactors | int | Whether to copy LU factors (0=no, 1=yes) | 0 or 1 | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | Error code: 0=success, 1001=out of memory |

### 2.3 Side Effects

- Allocates memory for snapshot arrays
- Copies basis header to snapshot
- Copies variable status array to snapshot
- Optionally copies LU factors

## 3. Contract

### 3.1 Preconditions

- [ ] State has valid basis (initialized)
- [ ] Snapshot structure is allocated
- [ ] If includeFactors=1, valid LU factorization exists

### 3.2 Postconditions

- [ ] Snapshot contains copy of basisHeader
- [ ] Snapshot contains copy of varStatus
- [ ] If includeFactors=1, snapshot contains factor copies
- [ ] Snapshot marked as valid

### 3.3 Invariants

- [ ] Original state unchanged
- [ ] Problem dimensions stored in snapshot

## 4. Algorithm

### 4.1 Overview

The snapshot operation performs deep copies of all basis-related arrays. For the factorization, this includes the L and U factor structures with their sparse index arrays and value arrays.

### 4.2 Detailed Steps

1. **Store dimensions**:
   - snapshot.numVars = state.numVars
   - snapshot.numConstrs = state.numConstrs

2. **Allocate and copy basis header**:
   - Allocate numConstrs integers
   - Copy basisHeader array

3. **Allocate and copy variable status**:
   - Allocate (numVars + numConstrs) integers
   - Copy varStatus array

4. **If includeFactors**:
   - Copy L factor structure
   - Copy U factor structure
   - Copy pivot permutation
   - Copy eta vectors (if any)

5. **Mark snapshot valid**:
   - Set snapshot.valid = 1
   - Record iteration number for reference

### 4.3 Pseudocode

```
SNAPSHOT(state, snapshot, includeFactors):
    m := state.numConstrs
    n := state.numVars

    snapshot.numVars := n
    snapshot.numConstrs := m

    // Copy basis header
    snapshot.basisHeader := ALLOCATE(m * sizeof(int))
    MEMCPY(snapshot.basisHeader, state.basisHeader, m)

    // Copy variable status
    snapshot.varStatus := ALLOCATE((n + m) * sizeof(int))
    MEMCPY(snapshot.varStatus, state.varStatus, n + m)

    IF includeFactors:
        snapshot.L := COPY_SPARSE_MATRIX(state.L)
        snapshot.U := COPY_SPARSE_MATRIX(state.U)
        snapshot.pivotPerm := COPY_ARRAY(state.pivotPerm, m)
        snapshot.etaList := COPY_ETA_LIST(state.etaList)
    ELSE:
        snapshot.L := NULL
        snapshot.U := NULL

    snapshot.valid := 1
    snapshot.iteration := state.iteration

    RETURN 0
```

## 5. Complexity

### 5.1 Time Complexity

- **Without factors:** O(n + m)
- **With factors:** O(n + m + L_nnz + U_nnz + k * eta_nnz)

Where:
- n = number of variables
- m = number of constraints
- L_nnz, U_nnz = nonzeros in factors
- k = number of eta vectors

### 5.2 Space Complexity

- **Without factors:** O(n + m)
- **With factors:** O(n + m + L_nnz + U_nnz)

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| Allocation failure | 1001 | Cannot allocate snapshot arrays |

### 6.2 Error Behavior

On failure:
- Free any partially allocated arrays
- Set snapshot.valid = 0
- Return error code

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Empty basis | m = 0 | Allocate zero-size arrays |
| No factors requested | includeFactors = 0 | Only copy header and status |
| Large eta list | Many updates | Copy all eta vectors |

## 8. Thread Safety

**Thread-safe:** Yes (creates independent copy)

**Synchronization required:** State must not change during snapshot

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_alloc | Memory | Allocate snapshot arrays |
| memcpy | C stdlib | Copy array data |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_simplex_iterate | Simplex | Before risky pivot sequences |
| cxf_crossover | Crossover | Before crossover attempts |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_basis_restore | Restores from snapshot |
| cxf_basis_diff | Compares two snapshots |
| cxf_basis_free_snapshot | Frees snapshot memory |

## 11. Design Notes

### 11.1 Design Rationale

**Deep copy:** Independent copy allows state to continue evolving while snapshot remains unchanged.

**Optional factors:** Factor copying is expensive; skip when only basis structure matters.

**Iteration tagging:** Knowing when snapshot was taken helps debug pivot sequences.

### 11.2 Performance Considerations

- Snapshot creation is O(n + m) without factors
- With factors, can be significant for large problems
- Use sparingly (e.g., every 100 iterations for checkpoint)

## 12. References

- Maros, I. (2003). "Computational Techniques of the Simplex Method" - Basis recovery techniques

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
