# cxf_pricing_steepest

**Module:** Pricing
**Spec Version:** 1.0

## 1. Purpose

Selects the entering variable using the steepest edge criterion. This method accounts for both the reduced cost magnitude and the length of the edge traversed in the simplex step. Returns the variable that provides the greatest improvement per unit distance traveled in the solution space.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| pricingState | PricingState* | Pricing state with SE weights | Non-null, initialized | Yes |
| state | SolverState* | Solver state with reduced costs | Non-null, valid state | Yes |
| tolerance | double | Optimality tolerance | > 0, typically 1e-6 | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | Index of entering variable, or -1 if optimal |

### 2.3 Side Effects

- May update SE weights if initialization needed
- Updates pricing statistics

## 3. Contract

### 3.1 Preconditions

- [ ] SE weights are initialized or will be computed
- [ ] Reduced costs are current
- [ ] Pricing state has strategy == STEEPEST_EDGE

### 3.2 Postconditions

- [ ] Returns best entering variable or -1
- [ ] SE weights used (not modified except lazy init)

### 3.3 Invariants

- [ ] Weights remain consistent with current basis

## 4. Algorithm

### 4.1 Overview

Steepest edge pricing selects variable j that maximizes |d_j| / sqrt(gamma_j) where d_j is the reduced cost and gamma_j is the squared norm of the edge direction (approximated by the SE weight).

### 4.2 Detailed Steps

1. **Initialize if needed**:
   - If weights not initialized (first call), compute them

2. **Scan nonbasic variables**:
   - For each nonbasic variable j:
     - Get reduced cost d_j and weight gamma_j
     - Compute SE ratio: |d_j| / sqrt(gamma_j)
     - Track best ratio and variable

3. **Check optimality**:
   - If no attractive variable found: return -1

4. **Return best variable**:
   - Return variable with highest SE ratio

### 4.3 Pseudocode

```
PRICING_STEEPEST(pricingState, state, tolerance):
    ps := pricingState
    n := state.numVars

    bestVar := -1
    bestRatio := 0.0

    FOR j := 0 TO n - 1:
        status := state.varStatus[j]

        // Skip basic variables
        IF status >= 0:
            CONTINUE

        rc := state.reducedCosts[j]
        weight := ps.weights[j]

        // Ensure positive weight
        IF weight < 1e-10:
            weight := 1.0

        // Check if attractive
        IF status == -1:  // At lower
            IF rc < -tolerance:
                ratio := (-rc) / SQRT(weight)
                IF ratio > bestRatio:
                    bestRatio := ratio
                    bestVar := j
        ELSE IF status == -2:  // At upper
            IF rc > tolerance:
                ratio := rc / SQRT(weight)
                IF ratio > bestRatio:
                    bestRatio := ratio
                    bestVar := j

    RETURN bestVar
```

### 4.4 Mathematical Foundation

**Steepest Edge Criterion:**
The entering variable j maximizes objective improvement per unit distance:
```
SE_ratio(j) = |d_j| / ||e_j||
```

where e_j is the edge direction (column of B^(-1) * A for variable j).

**Weight computation:**
gamma_j = ||B^(-1) * a_j||^2 = (B^(-1) * a_j)^T * (B^(-1) * a_j)

This is expensive to compute from scratch but can be updated incrementally after pivots.

**Reference frame:**
Initial weights assume unit reference frame: gamma_j = 1 for all j. True weights are computed lazily or updated during iterations.

## 5. Complexity

### 5.1 Time Complexity

- O(n) to scan all nonbasic variables
- Plus O(m) for each BTRAN if weights need update

### 5.2 Space Complexity

- O(n) for weights array (allocated in init)

## 6. Error Handling

### 6.1 Error Conditions

None - always returns valid index or -1

### 6.2 Return Values

| Value | Meaning |
|-------|---------|
| >= 0 | Index of entering variable |
| -1 | Optimal (no attractive variable) |

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Optimal | All RCs satisfy tolerance | Return -1 |
| Zero weight | weight[j] = 0 | Use weight = 1.0 |
| Many ties | Equal SE ratios | Return first found |
| Single candidate | One violation | Return that variable |

## 8. Thread Safety

**Thread-safe:** No (may update weights)

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| sqrt | C math | Square root for SE ratio |
| cxf_btran | Basis | Compute weight (if needed) |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_simplex_iterate | Simplex | When SE strategy selected |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_pricing_update | Updates weights after pivot |
| cxf_pricing_candidates | Alternative: Dantzig pricing |
| cxf_pricing_init | Initializes weights array |

## 11. Design Notes

### 11.1 Design Rationale

**Lazy weight computation:** Computing all weights initially is O(n * m). Instead, initialize to 1.0 and update incrementally.

**Weight safeguard:** Zero or negative weights are replaced with 1.0 to avoid division by zero or sqrt of negative.

**No sorting needed:** Unlike candidate-based pricing, SE directly finds the best variable.

### 11.2 SE vs Dantzig

- SE: Fewer iterations, more work per iteration
- Dantzig: More iterations, less work per iteration
- SE typically wins for difficult problems
- Dantzig may win for easy problems

### 11.3 Performance Considerations

- Square root is relatively expensive; could compare squared ratios
- Weight updates after pivot are O(m) - main cost
- Consider Devex as approximate SE

## 12. References

- Goldfarb, D. and Reid, J.K. (1977). "A Practicable Steepest-Edge Simplex Algorithm"
- Forrest, J.J. and Goldfarb, D. (1992). "Steepest-Edge Simplex Algorithms for Linear Programming"
- Maros, I. (2003). "Computational Techniques of the Simplex Method" - Chapter 10

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
