# cxf_pivot_with_eta

**Module:** Basis Operations
**Spec Version:** 1.0

## 1. Purpose

Updates the basis factorization after a simplex pivot using the product form of the inverse. Creates an eta vector representing the basis change and appends it to the eta file. This is more efficient than refactorization for individual pivots but accumulates error and increases FTRAN/BTRAN time over many pivots.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| state | SolverState* | Solver state with current basis | Non-null, valid state | Yes |
| pivotRow | int | Row index of leaving variable | 0 <= pivotRow < numConstrs | Yes |
| pivotCol | double* | Pivot column (from FTRAN) | Non-null, size numConstrs | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | Error code: 0=success, 1001=out of memory, -1=pivot too small |

### 2.3 Side Effects

- Allocates new eta vector
- Appends to eta list
- Increments eta count
- Updates basis header
- Updates variable status arrays

## 3. Contract

### 3.1 Preconditions

- [ ] pivotCol[pivotRow] is sufficiently large (|pivot| > threshold)
- [ ] pivotCol contains B^(-1) * a_entering
- [ ] Basis header and status are consistent
- [ ] Sufficient memory for eta vector

### 3.2 Postconditions

- [ ] New eta vector created with pivot column data
- [ ] Eta count incremented
- [ ] Basis header updated: basisHeader[pivotRow] = entering variable
- [ ] Variable status updated for entering and leaving variables

### 3.3 Invariants

- [ ] LU factors unchanged (eta extends them)
- [ ] Problem dimensions unchanged

## 4. Algorithm

### 4.1 Overview

The product form update represents B_new = B_old * E where E is an elementary matrix (eta matrix). E differs from identity only in column pivotRow, where it contains the eta vector. The eta vector is derived from the pivot column to effect the basis change.

### 4.2 Detailed Steps

1. **Validate pivot element**:
   - If |pivotCol[pivotRow]| < pivot_threshold: return error

2. **Compute eta vector**:
   - eta_multiplier = 1.0 / pivotCol[pivotRow]
   - For i != pivotRow: eta[i] = -pivotCol[i] * eta_multiplier

3. **Allocate eta structure**:
   - Allocate space for sparse eta entries
   - Count nonzeros in pivotCol (excluding pivot row)

4. **Store eta vector**:
   - eta.pivotRow = pivotRow
   - eta.multiplier = eta_multiplier
   - For each nonzero pivotCol[i] where i != pivotRow:
     - Store (index, value) pair

5. **Append to eta list**:
   - Link new eta to end of list
   - Increment etaCount

6. **Update basis header**:
   - Store entering variable in basisHeader[pivotRow]

7. **Update variable status**:
   - Set entering variable status to pivotRow (basic)
   - Set leaving variable status to nonbasic (at appropriate bound)

### 4.3 Pseudocode

```
PIVOT_WITH_ETA(state, pivotRow, pivotCol):
    pivot := pivotCol[pivotRow]

    IF |pivot| < PIVOT_THRESHOLD:
        RETURN -1  // Pivot too small

    // Compute eta multiplier
    eta_mult := 1.0 / pivot

    // Count nonzeros for allocation
    nnz := 0
    FOR i := 0 TO m - 1:
        IF i != pivotRow AND |pivotCol[i]| > DROP_TOLERANCE:
            nnz++

    // Allocate eta structure
    eta := ALLOCATE_ETA(nnz)
    eta.pivotRow := pivotRow
    eta.multiplier := eta_mult

    // Store eta entries
    k := 0
    FOR i := 0 TO m - 1:
        IF i != pivotRow AND |pivotCol[i]| > DROP_TOLERANCE:
            eta.indices[k] := i
            eta.values[k] := -pivotCol[i] * eta_mult
            k++
    eta.nnz := k

    // Append to list
    APPEND(state.etaList, eta)
    state.etaCount++

    // Update basis header
    leaving := state.basisHeader[pivotRow]
    entering := state.enteringVar
    state.basisHeader[pivotRow] := entering

    // Update variable status
    state.varStatus[entering] := pivotRow  // Now basic in this row
    state.varStatus[leaving] := DETERMINE_NONBASIC_STATUS(leaving)

    RETURN 0
```

### 4.4 Mathematical Foundation

**Product Form Update:**
After pivot, B_new = B_old * E where E is the eta matrix:
```
E = I except column pivotRow:
    E[i, pivotRow] = -pivotCol[i] / pivot  for i != pivotRow
    E[pivotRow, pivotRow] = 1 / pivot
```

**FTRAN Update:**
For FTRAN with new basis:
```
B_new^(-1) * a = E^(-1) * B_old^(-1) * a
```
So we apply E^(-1) after the old FTRAN.

**BTRAN Update:**
For BTRAN:
```
y * B_new^(-1) = y * E^(-1) * B_old^(-1)
```
Apply E^(-1) first, then old BTRAN. Since we process eta in reverse, this is handled automatically.

## 5. Complexity

### 5.1 Time Complexity

- O(m) to scan pivot column
- O(nnz) to store eta entries

### 5.2 Space Complexity

- O(nnz) for new eta vector where nnz is nonzeros in pivot column

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| Pivot too small | -1 | |pivot| < threshold, would cause instability |
| Allocation failure | 1001 | Cannot allocate eta structure |

### 6.2 Error Behavior

On small pivot:
- Returns -1
- No state changes
- Caller should refactorize or choose different pivot

On allocation failure:
- Returns 1001
- No state changes
- Caller should refactorize (clears eta list)

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Sparse pivot column | Few nonzeros | Small eta vector |
| Dense pivot column | All nonzeros | Large eta vector |
| First pivot | etaCount = 0 | Creates first eta |
| Pivot = 1.0 | Unit pivot | Simple eta (near-identity) |

## 8. Thread Safety

**Thread-safe:** No (modifies state structures)

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_alloc | Memory | Allocate eta structure |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_simplex_step | Simplex | After selecting pivot |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_ftran | Uses eta vectors in computation |
| cxf_btran | Uses eta vectors in computation |
| cxf_basis_refactor | Alternative: fresh factorization |

## 11. Design Notes

### 11.1 Design Rationale

**Sparse storage:** Only nonzero eta entries are stored, critical for large problems where pivot columns are sparse.

**Threshold pivoting:** Small pivots are rejected to maintain numerical stability. Caller must handle by refactorizing or choosing different pivot.

**Drop tolerance:** Very small eta entries are dropped to maintain sparsity. Some implementations use relative drop tolerance.

### 11.2 Performance Considerations

- Eta updates are O(m), much cheaper than refactorization O(m^2)
- But accumulating eta increases FTRAN/BTRAN time
- Typical strategy: refactorize every 50-200 pivots
- Memory for eta file grows with iterations since refactor

### 11.3 Future Considerations

- Forrest-Tomlin update as alternative
- Eta compression (combine old eta vectors)
- Selective eta storage (only large entries)

## 12. References

- Bartels, R.H. and Golub, G.H. (1969). "The Simplex Method of Linear Programming Using LU Decomposition"
- Forrest, J.J. and Tomlin, J.A. (1972). "Updated Triangular Factors of the Basis to Maintain Sparsity"
- Maros, I. (2003). "Computational Techniques of the Simplex Method" - Chapter 9

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
