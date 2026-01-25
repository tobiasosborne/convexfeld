# cxf_ftran

**Module:** Basis Operations
**Spec Version:** 1.0

## 1. Purpose

Performs forward transformation (FTRAN) to compute the representation of a column in terms of the current basis. Given a column vector a (typically a column of the constraint matrix), computes x = B^(-1) * a where B is the current basis matrix. This is equivalent to solving B * x = a. The result x contains the pivot column of the simplex tableau, used for ratio test and basis updates.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| state | SolverState* | Solver state containing basis factorization | Non-null, valid state | Yes |
| column | double* | Input column vector to transform | Non-null, size numConstrs | Yes |
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
- [ ] Column vector is valid (may be sparse)
- [ ] Result array is allocated with sufficient size
- [ ] Eta vectors (if any) are valid

### 3.2 Postconditions

- [ ] result contains x = B^(-1) * column
- [ ] Basis factorization unchanged
- [ ] State timing counters updated

### 3.3 Invariants

- [ ] Basis matrix B unchanged
- [ ] Problem dimensions unchanged

## 4. Algorithm

### 4.1 Overview

FTRAN solves B * x = a by applying the factorization B = L * U. We solve:
1. First: L * z = a (forward substitution)
2. Then: U * x = z (backward substitution)

If eta vectors exist from previous pivots (product form update), they are applied in chronological order after the LU solve.

### 4.2 Detailed Steps

1. **Copy input to result**:
   - result := copy of column vector

2. **Solve L * z = result** (forward substitution):
   - For each column k from 0 to numConstrs-1:
     - For each j in L column k (below diagonal):
       - result[j] -= L[j,k] * result[k]

3. **Solve U * x = z** (backward substitution):
   - For each column k from numConstrs-1 down to 0:
     - If result[k] != 0:
       - For each j in U column k (above diagonal):
         - result[k] -= U[j,k] * result[j]
       - Divide by diagonal: result[k] /= U[k,k]

4. **Apply eta vectors in order** (if product form used):
   - For each eta vector from oldest to newest:
     - Compute: temp = result[pivot]
     - Update: result[pivot] = temp * eta_multiplier
     - For each nonzero eta[j]: result[j] -= eta[j] * temp

5. **Record timing** if profiling enabled.

### 4.3 Pseudocode

```
FTRAN(state, column, result):
    m := state.numConstrs

    // Copy input
    COPY(result, column, m)

    // Forward solve L * z = result
    FOR k := 0 TO m-1:
        IF |result[k]| > EPSILON:
            FOR j IN L_col[k] WHERE j > k:
                result[j] -= L[j,k] * result[k]

    // Backward solve U * x = z
    FOR k := m-1 DOWNTO 0:
        IF |result[k]| > EPSILON:
            FOR j IN U_col[k] WHERE j < k:
                result[k] -= U[j,k] * result[j]
            result[k] /= U_diag[k]

    // Apply eta vectors in chronological order
    FOR eta IN state.etaList:
        temp := result[eta.pivotRow]
        result[eta.pivotRow] := temp * eta.multiplier
        FOR (j, val) IN eta.entries:
            result[j] -= val * temp

    RETURN 0
```

### 4.4 Mathematical Foundation

For a basis B with factorization B = L * U, the FTRAN operation computes x = B^(-1) * a by solving B * x = a.

The solution has the following interpretation:
- x[i] is the coefficient of basic variable i in expressing the entering column
- These coefficients form the pivot column of the simplex tableau
- The ratio test uses these to determine the leaving variable

With product form updates: B_new = B_old * E1 * E2 * ... * Ek where each Ei is an eta matrix. After LU solve, we must apply each eta matrix:
- x_new = Ek^(-1) * ... * E1^(-1) * x_old

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
| Excessive fill-in | 1001 | Out of memory during sparse operations |

### 6.2 Error Behavior

On numerical issues:
- Returns error code
- Result array may contain partial/invalid results
- Caller should trigger basis refactorization

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Zero column | All zeros in column | Result is all zeros |
| Unit vector | e_i input | Result is column i of B^(-1) |
| Single constraint | m=1 | Trivial: result[0] = column[0]/B[0,0] |
| Dense column | All nonzeros | Full triangular solves |
| Very sparse column | Few nonzeros | Exploit sparsity in forward solve |

## 8. Thread Safety

**Thread-safe:** Yes (read-only on state, writes only to result)

**Synchronization required:** None if state is not modified concurrently

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_timing_record | Timing | Record FTRAN time |
| memcpy | C stdlib | Copy column to result |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_simplex_iterate | Simplex | Compute pivot column |
| cxf_ratio_test | Ratio Test | Get column for ratio computation |
| cxf_pricing_update | Pricing | Update reduced costs |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_btran | Backward transformation (dual operation) |
| cxf_basis_refactor | Recomputes LU factorization |
| cxf_pivot_with_eta | Updates factorization with eta vector |

## 11. Design Notes

### 11.1 Design Rationale

**Separate input/output arrays:** Unlike some implementations that work in-place, this uses separate arrays to preserve the original column, which may be needed for other operations.

**Eta vector ordering:** Chronological order is required for FTRAN (opposite of BTRAN) because we're applying the inverse operations.

**Sparse handling:** The implementation exploits sparsity in both the factors and the input/output vectors.

### 11.2 Performance Considerations

- FTRAN is called once per simplex iteration (for entering column)
- May be called multiple times for dual simplex (evaluating candidates)
- Sparse operations are critical for performance
- Cache-friendly access patterns improve speed

### 11.3 Future Considerations

- Hyper-sparse FTRAN when result is expected to be sparse
- Partial FTRAN for early termination in pricing
- Vectorized operations for dense subproblems

## 12. References

- Maros, I. (2003). "Computational Techniques of the Simplex Method" - Chapter 9: Basis Factorization
- Forrest, J.J. and Tomlin, J.A. (1972). "Updated Triangular Factors of the Basis to Maintain Sparsity in the Product Form Simplex Method"

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
