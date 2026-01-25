# cxf_ratio_test

**Module:** Ratio Test
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Determines which basic variable should leave the basis during a simplex pivot operation by computing the minimum ratio test. This maintains primal feasibility by selecting the variable that reaches its bound first as the entering variable increases. Implements the Harris two-pass ratio test for improved numerical stability by preferring larger pivot elements among near-optimal candidates.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| state | SolverState* | Solver state containing basis, bounds, and current solution | Non-null, valid state | Yes |
| env | CxfEnv* | Environment containing tolerance parameters | Non-null, valid environment | Yes |
| enteringVar | int | Index of variable entering the basis | 0 to numVars-1 | Yes |
| pivotColumn | double* | BTRAN result: B^-1 A_entering in dense format | Non-null array of length numConstrs | Yes |
| columnNZ | int | Number of nonzeros in pivot column | 0 to numConstrs | Yes |
| leavingRow_out | int* | Output: row index of leaving variable | Non-null | Yes |
| pivotElement_out | double* | Output: pivot element value | Non-null | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | 0 (cxf__OK) on success, 5 (cxf__UNBOUNDED) if no bound reached |
| *leavingRow_out | int | Row index of selected leaving variable (0 to numConstrs-1) |
| *pivotElement_out | double | Pivot element value at pivotColumn[leavingRow] |

### 2.3 Side Effects

None. This is a pure computation function that reads from solver state and environment but does not modify them.

## 3. Contract

### 3.1 Preconditions

- [ ] state pointer must be valid and properly initialized
- [ ] env pointer must be valid and contain tolerance parameters
- [ ] pivotColumn must be dense array of length numConstrs
- [ ] pivotColumn must contain result of BTRAN(A_entering)
- [ ] leavingRow_out and pivotElement_out must be valid pointers
- [ ] basisHeader array must correctly identify basic variables
- [ ] xValues array must contain current basic variable values
- [ ] Lower and upper bound arrays must be consistent (lb[i] <= ub[i])

### 3.2 Postconditions

- [ ] If return is cxf__OK: leavingRow contains valid row index (0 to numConstrs-1)
- [ ] If return is cxf__OK: pivotElement contains pivotColumn[leavingRow]
- [ ] If return is cxf__OK: selected pivot element has largest magnitude among near-minimum ratios
- [ ] If return is cxf__UNBOUNDED: no basic variable reaches a bound (problem is unbounded)
- [ ] Pivot element magnitude is at least relaxedTol (feasTol * 10) if cxf__OK returned

### 3.3 Invariants

- [ ] Input arrays (pivotColumn, basisHeader, bounds, xValues) are not modified
- [ ] Solver state is not modified
- [ ] Environment parameters are not modified

## 4. Algorithm

### 4.1 Overview

Implements the Harris two-pass ratio test, a numerically stable variant of the standard minimum ratio test used in the simplex method. The ratio test determines which basic variable should leave the basis when a non-basic entering variable increases from its current bound.

In standard primal simplex, as the entering variable x_j increases, the basic variables adjust according to x_B = B^-1 b - B^-1 A_j * x_j. The pivot column d = B^-1 A_j represents the rate of change for each basic variable. A basic variable x_i reaches its bound when the ratio (bound - x_i) / d_i is minimized.

The Harris modification improves numerical stability by using a two-pass approach. The first pass finds the minimum ratio using a relaxed tolerance, allowing small numerical violations. The second pass examines all ratios within a small threshold of the minimum and selects the one with the largest pivot element magnitude. This prevents selection of very small pivot elements that cause numerical instability in subsequent basis updates.

The algorithm also detects unboundedness: if no basic variable reaches a bound (all coefficients have wrong sign or infinite bounds), the problem is unbounded in the entering variable direction.

### 4.2 Detailed Steps

1. **Initialize parameters**
   - Extract feasibility tolerance and infinity value from environment
   - Compute relaxed tolerance as 10 times the feasibility tolerance
   - Initialize minimum ratio to infinity and minimum row to -1

2. **First pass: Find minimum ratio with relaxed tolerance**
   - For each row i from 0 to numConstrs-1:
     - Get pivot coefficient d_i from pivotColumn[i]
     - Skip if |d_i| <= relaxedTol (near-zero pivot element)
     - Get basic variable index from basisHeader[i]
     - Skip if basicVar < 0 (slack or artificial variable)
     - Get current value x_i from xValues[basicVar]
     - If d_i > relaxedTol (positive coefficient):
       - Check if upper bound is finite (ub < infinity)
       - Compute ratio = (ub - x_i) / d_i
     - Else if d_i < -relaxedTol (negative coefficient):
       - Check if lower bound is finite (lb > -infinity)
       - Compute ratio = (lb - x_i) / d_i
     - If ratio >= -feasTol and ratio < minRatio:
       - Update minRatio = ratio
       - Update minRow = i

3. **Check for unboundedness**
   - If minRow == -1, no variable reaches a bound
   - Return cxf__UNBOUNDED

4. **Second pass: Select largest pivot among near-minimum ratios (Harris stability)**
   - Set threshold = minRatio + feasTol
   - Initialize maxPivot = |pivotColumn[minRow]|
   - Initialize finalRow = minRow
   - For each row i from 0 to numConstrs-1:
     - Get pivot coefficient d_i
     - Skip if |d_i| <= relaxedTol
     - Get basic variable and compute ratio (same logic as first pass)
     - If ratio <= threshold:
       - If |d_i| > maxPivot:
         - Update maxPivot = |d_i|
         - Update finalRow = i

5. **Return results**
   - Set *leavingRow_out = finalRow
   - Set *pivotElement_out = pivotColumn[finalRow]
   - Return cxf__OK

### 4.3 Pseudocode

```
Input: pivotColumn d = B^-1 A_entering, current values x_B, bounds l and u
Output: leavingRow p, pivotElement d_p, or UNBOUNDED

// First pass: find minimum ratio with relaxed tolerance
τ_relax ← 10 * FeasibilityTol
θ_min ← ∞
p ← -1

FOR each row i ∈ {0, ..., m-1}:
    IF |d_i| ≤ τ_relax:
        CONTINUE  // skip near-zero pivots

    j ← basisHeader[i]
    IF j < 0:
        CONTINUE  // skip slack variables

    IF d_i > τ_relax:
        θ_i ← (u_j - x_j) / d_i  // hits upper bound
    ELSE IF d_i < -τ_relax:
        θ_i ← (l_j - x_j) / d_i  // hits lower bound
    ELSE:
        CONTINUE

    IF θ_i ≥ -FeasibilityTol AND θ_i < θ_min:
        θ_min ← θ_i
        p ← i

IF p = -1:
    RETURN UNBOUNDED

// Second pass: maximize pivot magnitude among near-minimum ratios
δ ← FeasibilityTol
γ_max ← |d_p|
p_final ← p

FOR each row i ∈ {0, ..., m-1}:
    IF |d_i| ≤ τ_relax:
        CONTINUE

    j ← basisHeader[i]
    IF j < 0:
        CONTINUE

    Compute θ_i (same as first pass)

    IF θ_i ≤ θ_min + δ:
        IF |d_i| > γ_max:
            γ_max ← |d_i|
            p_final ← i

RETURN (p_final, d_p_final)
```

### 4.4 Mathematical Foundation

**Standard Ratio Test (Dantzig)**
Given basic feasible solution x_B = B^-1 b and entering column A_j, compute:

θ* = min { (u_i - x_i) / d_i : d_i > 0, i ∈ B } ∪ { (l_i - x_i) / d_i : d_i < 0, i ∈ B }

where d = B^-1 A_j is the pivot column.

**Harris Modification**
Instead of selecting the exact minimum ratio, Harris' method selects among ratios within a tolerance window:

R_δ = { i : θ_i ≤ θ* + δ }

From R_δ, select i that maximizes |d_i|. This prevents selection of very small pivots that cause:
- Large roundoff errors in basis updates
- Growth in matrix entries (fill-in)
- Degradation of LU factorization quality

**References:**
- Harris, P.M.J. (1973). "Pivot selection methods of the Devex LP code." Mathematical Programming Study, 4, 30-57.
- Tomlin, J.A. (1972). "Pivoting for size and sparsity in linear programming inversion routines." Journal of the Institute of Mathematics and Its Applications, 10, 289-295.

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(m) where m = numConstrs (dense iteration)
- **Average case:** O(m) dense, O(columnNZ) for sparse implementation
- **Worst case:** O(m)

Where:
- m = numConstrs (number of constraint rows)
- columnNZ = number of nonzeros in pivot column (typically << m)

Note: Current implementation uses dense iteration. Production code would iterate only over nonzeros for efficiency.

### 5.2 Space Complexity

- **Auxiliary space:** O(1) - only scalar variables used
- **Total space:** O(m) - input pivot column array

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| No variable reaches bound | 5 (cxf__UNBOUNDED) | All coefficients have wrong sign or bounds are infinite |

### 6.2 Error Behavior

On unboundedness detection, function returns cxf__UNBOUNDED immediately without modifying output parameters. Solver state remains unchanged and consistent.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Degenerate pivot | Multiple ratios ≈ 0 | Harris pass selects largest pivot among degenerate candidates |
| All near-zero coefficients | All \|d_i\| < relaxedTol | Returns UNBOUNDED (cannot find valid pivot) |
| Single nonzero coefficient | columnNZ = 1 | Selects that row if ratio is valid |
| All slacks basic | All basisHeader[i] < 0 | Returns UNBOUNDED (no structural variables) |
| Negative ratios | Some θ_i < 0 due to infeasibility | Accepts ratios down to -feasTol for numerical tolerance |
| Multiple near-minimum | Several θ_i within threshold | Selects one with maximum \|d_i\| |

## 8. Thread Safety

**Thread-safe:** Yes (with external synchronization)

This function is read-only with respect to shared state. Multiple threads can safely call this function on different solver states. If the same solver state is accessed by multiple threads, external locking is required.

**Synchronization required:** None for read-only access; caller must ensure solver state is not modified concurrently

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_get_feasibility_tol | Parameters | Retrieve feasibility tolerance from environment |
| cxf_get_infinity | Parameters | Retrieve infinity representation (typically 1e100) |
| fabs | Math | Compute absolute value of pivot elements |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_pivot_primal | Simplex Core | Main primal simplex iteration to select leaving variable |
| cxf_simplex_step | Simplex Core | Combined iteration step (pricing + ratio test + update) |
| cxf_simplex_iterate | Simplex Core | Main simplex loop |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_price_steepest | Complementary operation: selects entering variable while ratio test selects leaving |
| cxf_dual_ratio_test | Dual variant: operates on reduced costs instead of primal values |
| cxf_pivot_check | Pre-validation: checks if proposed pivot maintains feasibility |
| cxf_basis_update | Downstream: performs basis update using selected pivot |

## 11. Design Notes

### 11.1 Design Rationale

The Harris two-pass approach trades slight optimality for significantly improved numerical stability. While the standard ratio test always selects the exact minimum ratio, this can lead to very small pivot elements that cause:

1. Large numerical errors during basis updates (FTRAN/BTRAN operations)
2. Growth in matrix coefficients due to roundoff error accumulation
3. Premature refactorization due to basis ill-conditioning

By allowing a small tolerance window (typically FeasibilityTol ≈ 1e-6), the Harris test can select pivots that are 10-1000 times larger in magnitude while only sacrificing negligible optimality. On poorly scaled problems, this dramatically improves solver robustness.

The relaxed tolerance (10x feasibility tolerance) in the first pass further improves stability by rejecting extremely small pivot candidates entirely, even if they produce the minimum ratio.

### 11.2 Performance Considerations

**Dense vs Sparse Iteration:** Current implementation uses dense iteration over all m rows. Production-quality implementation should iterate only over columnNZ nonzeros for efficiency, especially when columnNZ << m (common in large sparse problems).

**Two-pass overhead:** The second pass adds approximately 2x cost compared to standard ratio test. However, this is negligible compared to overall iteration cost (BTRAN, FTRAN, basis update) and pays for itself through reduced refactorization frequency.

**Cache efficiency:** Sequential array access patterns (basisHeader, xValues, bounds) provide good cache locality. Sparse implementation would have pointer-chasing overhead.

### 11.3 Future Considerations

**Steepest-edge variant:** Could incorporate edge norms for even better pivot selection in combination with steepest-edge pricing.

**Expand procedure:** For highly degenerate problems, could perform multiple degenerate pivots in a single iteration to escape degenerate vertices faster.

**Sparse implementation:** Should be added for large-scale problems where columnNZ << m.

**Partial pricing:** Could use partial ratio test (check subset of candidates) during initial iterations when basis quality is poor.

## 12. References

- Dantzig, G.B. (1963). *Linear Programming and Extensions*. Princeton University Press. (Standard ratio test)
- Harris, P.M.J. (1973). "Pivot selection methods of the Devex LP code." *Mathematical Programming Study*, 4, 30-57. (Two-pass ratio test)
- Tomlin, J.A. (1972). "Pivoting for size and sparsity in linear programming inversion routines." *Journal of the Institute of Mathematics and Its Applications*, 10, 289-295. (Numerical stability analysis)
- Koberstein, A. (2005). *The dual simplex method, techniques for a fast and stable implementation*. PhD thesis, Universität Paderborn. (Modern ratio test implementations)
- Huangfu, Q. and Hall, J.A.J. (2018). "Parallelizing the dual revised simplex method." *Mathematical Programming Computation*, 10(1), 119-142. (HiGHS solver ratio test)

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

*Reviewed by: Pending*

---
*Convexfeld LP Solver Specification*
*Based on published optimization literature*
