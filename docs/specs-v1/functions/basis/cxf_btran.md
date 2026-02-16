# cxf_btran

**Module:** Basis Operations
**Spec Version:** 1.0

## 1. Purpose

Performs backward transformation (BTRAN) to compute the row of the simplex tableau corresponding to a leaving variable. Given a unit vector e_i (with 1 in position i), computes y = B^(-T) * e_i where B is the current basis matrix. This is equivalent to solving B^T * y = e_i. The result y contains the coefficients needed for ratio test computations and reduced cost updates during simplex iterations.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| state | SolverState* | Solver state containing basis factorization | Non-null, valid state | Yes |
| row | int | Row index (leaving variable position in basis) | 0 <= row < numConstrs | Yes |
| result | double* | Output array for transformed vector | Non-null, size numConstrs | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | Error code: 0=success, 1001=out of memory |

### 2.3 Side Effects

- Writes numConstrs values to result array
- May update timing statistics
- May trigger basis refactorization if eta file is too long

## 3. Contract

### 3.1 Preconditions

- [ ] State has valid basis factorization (L, U factors available)
- [ ] Row index is within valid range
- [ ] Result array is allocated with sufficient size
- [ ] Eta vectors (if any) are valid

### 3.2 Postconditions

- [ ] result contains y = B^(-T) * e_row
- [ ] Basis factorization unchanged
- [ ] State timing counters updated

### 3.3 Invariants

- [ ] Basis matrix B unchanged
- [ ] Problem dimensions unchanged

## 4. Algorithm

### 4.1 Overview

BTRAN solves B^T * y = e_i by applying the factorization B = L * U in reverse order. Since B^T = U^T * L^T, we solve:
1. First: U^T * z = e_i (forward substitution on U^T)
2. Then: L^T * y = z (backward substitution on L^T)

If eta vectors exist from previous pivots (product form update), they are applied in reverse chronological order after the LU solve.

### 4.2 Detailed Steps

1. **Initialize result vector**:
   - Set result to zero
   - Set result[row] = 1.0 (unit vector e_row)

2. **Apply eta vectors in reverse** (if product form used):
   - For each eta vector from newest to oldest:
     - Compute dot product: temp = sum(eta[j] * result[j])
     - Update pivot position: result[pivot] -= temp

3. **Solve U^T * z = result** (forward substitution):
   - For each column k from 0 to numConstrs-1:
     - If result[k] != 0:
       - Divide by diagonal: result[k] /= U[k,k]
       - Update remaining: for j > k: result[j] -= U[k,j] * result[k]

4. **Solve L^T * y = z** (backward substitution):
   - For each column k from numConstrs-1 down to 0:
     - For each j in L column k (below diagonal):
       - result[j] -= L[j,k] * result[k]

5. **Record timing** if profiling enabled.

### 4.3 Pseudocode

```
BTRAN(state, row, result):
    m := state.numConstrs

    // Initialize unit vector
    ZERO(result, m)
    result[row] := 1.0

    // Apply eta vectors in reverse order
    FOR eta IN REVERSE(state.etaList):
        temp := DOT_PRODUCT(eta.values, result, eta.indices)
        result[eta.pivotRow] -= temp

    // Forward solve U^T * z = result
    FOR k := 0 TO m-1:
        IF |result[k]| > EPSILON:
            result[k] /= U_diag[k]
            FOR j IN U_row[k] WHERE j > k:
                result[j] -= U[k,j] * result[k]

    // Backward solve L^T * y = z
    FOR k := m-1 DOWNTO 0:
        FOR j IN L_col[k] WHERE j > k:
            result[j] -= L[j,k] * result[k]

    RETURN 0
```

### 4.4 Mathematical Foundation

The basis matrix B consists of m columns selected from the constraint matrix A plus slack/artificial columns. The factorization B = L * U allows efficient solving of systems involving B.

For BTRAN, we need y such that B^T * y = e_i:
- This is equivalent to y^T * B = e_i^T
- The solution y gives row i of B^(-1)
- In simplex, this row is used to compute the pivot column of the tableau

With product form updates: B_new = B_old * E where E is an eta matrix (elementary matrix). The BTRAN must account for all eta matrices accumulated since last refactorization.

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(m) for sparse triangular solves
- **Average case:** O(m * fill_L + m * fill_U + k * nnz_eta)
- **Worst case:** O(m^2) for dense factors

Where:
- m = number of constraints
- fill_L, fill_U = average nonzeros per column in factors
- k = number of eta vectors
- nnz_eta = average nonzeros per eta vector

### 5.2 Space Complexity

- **Auxiliary space:** O(1) (uses result array provided)
- **Total space:** O(m) for result vector

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| Numerical instability detected | 1001 | Division by near-zero diagonal |

### 6.2 Error Behavior

On numerical issues:
- Returns error code
- Result array may contain partial/invalid results
- Caller should trigger basis refactorization

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| No eta vectors | Empty eta list | Pure LU solve |
| Row 0 | row=0 | First unit vector |
| Last row | row=m-1 | Last unit vector |
| Single constraint | m=1 | Trivial: result[0] = 1/B[0,0] |
| Dense basis | High fill-in | Slower but correct |

## 8. Thread Safety

**Thread-safe:** Yes (read-only on state, writes only to result)

**Synchronization required:** None if state is not modified concurrently

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_timing_record | Timing | Record BTRAN time |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_pricing_steepest | Pricing | Compute steepest edge weights |
| cxf_simplex_iterate | Simplex | Pivot selection |
| cxf_ratio_test | Ratio Test | Compute tableau row |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_ftran | Forward transformation (dual operation) |
| cxf_basis_refactor | Recomputes LU factorization |
| cxf_pivot_with_eta | Updates factorization with eta vector |

## 11. Design Notes

### 11.1 Design Rationale

**Sparse implementation:** Only nonzero elements are processed, critical for large-scale LP where factors are typically very sparse.

**Eta vector ordering:** Reverse chronological order is required because eta matrices don't commute. The most recent pivot must be undone first.

**In-place computation:** The result array is used for intermediate values, avoiding additional memory allocation.

### 11.2 Performance Considerations

- BTRAN is called once per simplex iteration
- Sparse vector operations are essential for performance
- Eta file length affects BTRAN time; refactorization needed when too long

### 11.3 Future Considerations

- Hyper-sparse BTRAN for very sparse results
- Blocked operations for cache efficiency
- Parallel triangular solves for dense subproblems

## 12. References

- Maros, I. (2003). "Computational Techniques of the Simplex Method" - Chapter 9: Basis Factorization
- Suhl, U.H. and Suhl, L.M. (1990). "Computing Sparse LU Factorizations for Large-Scale Linear Programming Bases"

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
