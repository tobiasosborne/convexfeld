# cxf_pricing_step2

**Module:** Pricing
**Spec Version:** 1.0

## 1. Purpose

Performs the second step of two-phase pricing when the initial candidate selection doesn't yield a suitable entering variable. This fallback mechanism either expands the search (for partial pricing) or applies more expensive but thorough evaluation criteria. Ensures the pricing phase finds an entering variable if one exists.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| pricingState | PricingState* | Pricing state after first attempt | Non-null, initialized | Yes |
| state | SolverState* | Solver state with reduced costs | Non-null, valid state | Yes |
| tolerance | double | Optimality tolerance | > 0, typically 1e-6 | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | Index of entering variable, or -1 if optimal |

### 2.3 Side Effects

- May scan additional variables
- May reset section counter
- Updates pricing statistics

## 3. Contract

### 3.1 Preconditions

- [ ] First pricing pass found no candidates
- [ ] Reduced costs are current
- [ ] Pricing state reflects first pass

### 3.2 Postconditions

- [ ] Returns entering variable or confirms optimality
- [ ] If partial pricing: full scan completed
- [ ] Statistics updated

### 3.3 Invariants

- [ ] Reduced costs unchanged
- [ ] Weights unchanged

## 4. Algorithm

### 4.1 Overview

When the first pricing pass fails to find a candidate (returns 0 or -1), this function ensures completeness:
- For partial pricing: scans remaining sections
- For SE/Devex: may use relaxed tolerance
- Returns -1 only when truly optimal

### 4.2 Detailed Steps

1. **Determine recovery action based on strategy**:
   - Partial: scan all remaining sections
   - SE/Devex: full scan with current tolerance
   - Full (Dantzig): already complete, return -1

2. **For partial pricing**:
   - Save current section
   - Scan all sections that haven't been checked
   - Restore section counter

3. **For SE/Devex fallback**:
   - Perform full scan (all variables)
   - Use same SE criterion

4. **Return result**:
   - If candidate found: return index
   - If none found: return -1 (optimal)

### 4.3 Pseudocode

```
PRICING_STEP2(pricingState, state, tolerance):
    ps := pricingState

    IF ps.strategy == PARTIAL:
        // Full scan of all sections
        savedSection := ps.currentSection

        FOR section := 0 TO ps.numSections - 1:
            ps.currentSection := section
            count := PRICING_CANDIDATES(ps, state, tolerance)
            IF count > 0:
                bestVar := SELECT_BEST_CANDIDATE(ps, state)
                ps.currentSection := savedSection
                RETURN bestVar

        ps.currentSection := savedSection
        RETURN -1  // Truly optimal

    ELSE IF ps.strategy == STEEPEST_EDGE OR ps.strategy == DEVEX:
        // Already scans all variables, but double-check
        RETURN PRICING_STEEPEST(ps, state, tolerance)

    ELSE:  // Full Dantzig
        // Already complete
        RETURN -1
```

## 5. Complexity

### 5.1 Time Complexity

- **Partial pricing:** O(n) full scan
- **SE/Devex:** O(n) (same as first pass)
- **Worst case:** O(n)

### 5.2 Space Complexity

- O(1) additional

## 6. Error Handling

### 6.1 Error Conditions

None - always returns valid result

### 6.2 Return Values

| Value | Meaning |
|-------|---------|
| >= 0 | Index of entering variable |
| -1 | Problem is optimal |

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Truly optimal | No violations | Return -1 |
| Candidate in last section | Partial pricing | Found on full scan |
| Numerical near-optimal | Tiny violations | May find or return -1 |

## 8. Thread Safety

**Thread-safe:** No (modifies pricing state)

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_pricing_candidates | Pricing | Section scan |
| cxf_pricing_steepest | Pricing | SE evaluation |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_simplex_iterate | Simplex | After first pass returns -1 |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_pricing_candidates | First pricing pass |
| cxf_pricing_steepest | First SE pass |

## 11. Design Notes

### 11.1 Design Rationale

**Two-phase pricing:** First pass is fast (partial scan or current section). Second pass ensures completeness. Avoids false optimality claims.

**Partial pricing completeness:** Single section may miss all candidates. Full scan needed to confirm optimality.

### 11.2 Performance Considerations

- Step2 rarely needed if step1 effective
- Full scan still O(n) with small constant
- May add iteration count but improves robustness

## 12. References

- Maros, I. (2003). "Computational Techniques of the Simplex Method" - Partial pricing

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
