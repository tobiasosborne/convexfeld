# cxf_basis_refactor

**Module:** Basis Operations
**Spec Version:** 1.0

## 1. Purpose

Recomputes the LU factorization of the current basis matrix B. This operation discards all accumulated eta vectors and creates a fresh factorization, restoring numerical accuracy and FTRAN/BTRAN performance. Refactorization is triggered periodically (every 50-200 iterations) or when eta file becomes too large or numerical instability is detected.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| state | SolverState* | Solver state with basis header | Non-null, valid state | Yes |
| env | CxfEnv* | Environment with tolerances | Non-null, valid env | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | Error code: 0=success, 1001=out of memory, 3=singular basis |

### 2.3 Side Effects

- Deallocates old L, U factor storage
- Allocates new L, U factor storage
- Clears eta vector list
- Resets eta count to zero
- Updates factorization timestamp
- Updates timing statistics

## 3. Contract

### 3.1 Preconditions

- [ ] State has valid basis header (basisHeader array)
- [ ] Basis header contains valid column indices
- [ ] Matrix structure is accessible
- [ ] Sufficient memory for new factors

### 3.2 Postconditions

- [ ] New L, U factors computed for current basis
- [ ] Eta list is empty
- [ ] etaCount = 0
- [ ] Factors satisfy B = L * U (within tolerance)

### 3.3 Invariants

- [ ] Basis header unchanged
- [ ] Problem dimensions unchanged
- [ ] Variable status array unchanged

## 4. Algorithm

### 4.1 Overview

Refactorization extracts the current basis columns from the constraint matrix and computes a fresh LU factorization using partial pivoting for numerical stability. The Markowitz criterion is used to select pivots that minimize fill-in while maintaining stability.

### 4.2 Detailed Steps

1. **Clear existing factorization**:
   - Free L factor storage
   - Free U factor storage
   - Clear eta vector list
   - Set etaCount = 0

2. **Extract basis columns**:
   - For each row i in basis header:
     - If basisHeader[i] < numVars: extract structural column
     - Else: extract slack column (unit vector)

3. **Initialize working matrix**:
   - Create sparse copy of basis matrix
   - Initialize row/column counts for Markowitz

4. **Markowitz LU factorization**:
   - For k = 0 to m-1:
     - Select pivot using Markowitz criterion (minimize row_count * col_count)
     - Apply stability threshold (|pivot| >= threshold * max_in_column)
     - If no suitable pivot found: return SINGULAR_BASIS
     - Perform elimination, updating L and U factors
     - Update Markowitz counts

5. **Store factors in compressed format**:
   - L: column-wise sparse storage (unit diagonal implicit)
   - U: column-wise sparse storage (explicit diagonal)

6. **Record statistics**:
   - Factor nonzeros (L_nnz, U_nnz)
   - Fill-in ratio
   - Factorization time

### 4.3 Pseudocode

```
REFACTOR(state, env):
    m := state.numConstrs

    // Clear old factorization
    FREE_FACTORS(state.L, state.U)
    CLEAR_ETA_LIST(state)
    state.etaCount := 0

    // Extract basis matrix
    B := SPARSE_MATRIX(m, m)
    FOR i := 0 TO m-1:
        col := state.basisHeader[i]
        IF col < state.numVars:
            COPY_COLUMN(B, i, state.matrix, col)
        ELSE:
            SET_UNIT_COLUMN(B, i, col - state.numVars)

    // Markowitz factorization
    rowCount := ARRAY(m)
    colCount := ARRAY(m)
    INIT_COUNTS(B, rowCount, colCount)

    perm := ARRAY(m)  // Pivot permutation

    FOR k := 0 TO m-1:
        // Select pivot
        (pivRow, pivCol) := SELECT_MARKOWITZ_PIVOT(B, rowCount, colCount, k, threshold)

        IF pivRow < 0:
            RETURN SINGULAR_BASIS_ERROR

        perm[k] := (pivRow, pivCol)

        // Eliminate
        pivot := B[pivRow, pivCol]
        FOR each row r with nonzero in column pivCol:
            IF r != pivRow:
                mult := B[r, pivCol] / pivot
                STORE_L_ENTRY(k, r, mult)
                FOR each col c with nonzero in row pivRow:
                    B[r, c] -= mult * B[pivRow, c]
                UPDATE_COUNTS(rowCount, colCount, r)

        STORE_U_ROW(k, pivRow, B)
        MARK_ELIMINATED(pivRow, pivCol)

    // Finalize factors
    state.L := COMPRESS_L()
    state.U := COMPRESS_U()
    state.pivotPerm := perm

    RECORD_TIMING(state, "refactor")
    RETURN 0
```

### 4.4 Mathematical Foundation

**LU Factorization:**
Given basis matrix B, we compute B = P * L * U * Q where:
- P, Q are permutation matrices (for pivoting)
- L is lower triangular with unit diagonal
- U is upper triangular

The factorization allows efficient solving:
- B * x = b becomes P * L * U * Q * x = b
- Solve in stages: L * y = P^T * b, U * z = y, x = Q^T * z

**Markowitz Criterion:**
At step k, select pivot (i, j) that minimizes:
- (row_count[i] - 1) * (col_count[j] - 1)

This minimizes expected fill-in (new nonzeros created).

**Numerical Stability:**
Threshold pivoting requires |pivot| >= threshold * max_in_column.
Typical threshold: 0.01 to 0.1.

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(m * nnz_avg) for sparse basis
- **Average case:** O(m^2 * nnz_avg)
- **Worst case:** O(m^3) for dense basis

Where:
- m = number of constraints
- nnz_avg = average nonzeros per column

### 5.2 Space Complexity

- **Auxiliary space:** O(m^2) worst case for working matrix
- **Factor space:** O(m + fill_in) typically much less than O(m^2)

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| Singular basis detected | 3 | No suitable pivot found |
| Memory allocation failure | 1001 | Cannot allocate factor storage |

### 6.2 Error Behavior

On singular basis:
- Returns error code 3
- Old factors may be partially overwritten
- Caller must handle (e.g., repair basis)

On memory failure:
- Returns error code 1001
- Partial cleanup attempted
- State may be inconsistent

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Identity basis | All slacks | L = I, U = I, trivial factorization |
| Single row | m = 1 | L = [1], U = [pivot value] |
| Near-singular | Very small pivots | May succeed with warnings or fail |
| Dense basis | High fill-in | Slower, more memory |
| All structural | No slacks in basis | Full matrix extraction |

## 8. Thread Safety

**Thread-safe:** No (modifies state extensively)

**Synchronization required:** Exclusive access to state during refactorization

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_alloc | Memory | Allocate factor storage |
| cxf_free | Memory | Free old factors |
| cxf_timing_record | Timing | Record refactorization time |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_simplex_iterate | Simplex | Periodic refactorization |
| cxf_basis_validate | Basis | After detecting instability |
| cxf_simplex_setup | Simplex | Initial factorization |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_ftran | Uses factorization for forward solve |
| cxf_btran | Uses factorization for backward solve |
| cxf_pivot_with_eta | Alternative: update without refactor |
| cxf_basis_validate | Checks if refactorization needed |

## 11. Design Notes

### 11.1 Design Rationale

**Markowitz ordering:** Reduces fill-in dramatically compared to natural ordering. For LP bases, fill-in is typically low.

**Threshold pivoting:** Balances sparsity (lower threshold, more pivot choices) with stability (higher threshold, larger pivots).

**Periodic refactorization:** Eta vectors accumulate error and increase FTRAN/BTRAN time. Refactorizing every 50-200 iterations is standard practice.

### 11.2 Performance Considerations

- Refactorization is expensive but infrequent
- Amortized cost over iterations is acceptable
- Sparse storage critical for large problems
- Pivot selection dominates time for large bases

### 11.3 Future Considerations

- Incremental update for small basis changes
- Supernodal factorization for dense submatrices
- Parallel factorization for large bases

## 12. References

- Markowitz, H.M. (1957). "The Elimination Form of the Inverse and its Application to Linear Programming"
- Suhl, U.H. and Suhl, L.M. (1990). "Computing Sparse LU Factorizations for Large-Scale Linear Programming Bases"
- Maros, I. (2003). "Computational Techniques of the Simplex Method" - Chapter 9

## 13. Validation Checklist

Before finalizing this spec, verify:

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
