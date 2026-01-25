# cxf_pricing_invalidate

**Module:** Pricing
**Spec Version:** 1.0

## 1. Purpose

Invalidates cached pricing data, forcing recomputation on the next pricing call. This is necessary when problem data changes in ways that make incremental updates invalid, such as bound changes, objective changes, or basis refactorization that affects SE weights.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| pricingState | PricingState* | Pricing state to invalidate | Non-null, initialized | Yes |
| flags | int | What to invalidate (bitmask) | See flags table | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | void | No return value |

### 2.3 Side Effects

- Sets invalidity flags in pricing state
- May clear candidate list
- May mark weights for recomputation

## 3. Contract

### 3.1 Preconditions

- [ ] Pricing state is initialized

### 3.2 Postconditions

- [ ] Specified data marked invalid
- [ ] Next pricing call will recompute as needed

### 3.3 Invariants

- [ ] Memory allocation unchanged

## 4. Algorithm

### 4.1 Overview

Sets flags indicating what pricing data is invalid. The next pricing operation checks these flags and recomputes as needed.

### 4.2 Detailed Steps

**Flag definitions:**
- INVALID_CANDIDATES = 0x01
- INVALID_REDUCED_COSTS = 0x02
- INVALID_WEIGHTS = 0x04
- INVALID_ALL = 0xFF

1. **Apply flags**:
   - pricingState.invalidFlags |= flags

2. **Clear candidate list** (if INVALID_CANDIDATES):
   - candidateCount = 0

3. **Mark weights** (if INVALID_WEIGHTS):
   - weightsValid = false
   - (Will be recomputed on next SE pricing)

### 4.3 Pseudocode

```
PRICING_INVALIDATE(pricingState, flags):
    ps := pricingState

    ps.invalidFlags |= flags

    IF flags & INVALID_CANDIDATES:
        ps.candidateCount := 0

    IF flags & INVALID_WEIGHTS:
        ps.weightsValid := FALSE
```

## 5. Complexity

### 5.1 Time Complexity

- O(1)

### 5.2 Space Complexity

- O(1) - no allocation

## 6. Error Handling

No error conditions.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| No flags | flags = 0 | No change |
| All flags | flags = 0xFF | Everything invalidated |
| Already invalid | Double invalidate | Idempotent |

## 8. Thread Safety

**Thread-safe:** Yes (simple flag set)

## 9. Dependencies

### 9.1 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_basis_refactor | Basis | After refactorization |
| cxf_simplex_preprocess | Simplex | After bound changes |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_pricing_candidates | Checks invalid flags |
| cxf_pricing_steepest | Recomputes invalid weights |

## 11. Design Notes

### 11.1 Design Rationale

**Lazy invalidation:** Mark invalid now, recompute later only if needed. Avoids unnecessary work.

**Granular flags:** Different operations invalidate different things. Fine-grained control reduces recomputation.

## 12. References

- Standard software engineering practice for cache invalidation

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
