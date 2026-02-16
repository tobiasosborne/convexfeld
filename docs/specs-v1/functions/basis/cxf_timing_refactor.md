# cxf_timing_refactor

**Module:** Basis Operations
**Spec Version:** 1.0

## 1. Purpose

Determines whether basis refactorization should be triggered based on timing and performance criteria. Monitors eta file growth, FTRAN/BTRAN times, and iteration count since last refactorization to decide when the cost of accumulated eta vectors exceeds the cost of refactorization. Returns a recommendation to the caller.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| state | SolverState* | Solver state with timing info | Non-null, valid state | Yes |
| env | CxfEnv* | Environment with refactor parameters | Non-null, valid env | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | 0=no refactor needed, 1=refactor recommended, 2=refactor required |

### 2.3 Side Effects

- Updates timing statistics
- May update performance counters

## 3. Contract

### 3.1 Preconditions

- [ ] Timing state is initialized
- [ ] Eta count is tracked in state
- [ ] Last refactor time is recorded

### 3.2 Postconditions

- [ ] Returns valid recommendation
- [ ] Timing statistics updated

## 4. Algorithm

### 4.1 Overview

The function evaluates multiple criteria:
1. Eta count threshold (hard limit)
2. Eta memory usage
3. Average FTRAN/BTRAN time increase
4. Iteration count since last refactor

### 4.2 Detailed Steps

1. **Check hard limits**:
   - If etaCount > maxEtaCount: return 2 (required)
   - If eta memory > maxEtaMemory: return 2 (required)

2. **Check soft criteria**:
   - Compute avgFTRANTime over recent iterations
   - If avgFTRANTime > baselineFTRAN * threshold: return 1

3. **Check iteration count**:
   - If itersSinceRefactor > refactorInterval: return 1

4. **Return 0** if no criteria met

### 4.3 Pseudocode

```
TIMING_REFACTOR(state, env):
    // Hard limits
    IF state.etaCount > env.MaxEtaCount:
        RETURN 2  // Required

    IF state.etaMemory > env.MaxEtaMemory:
        RETURN 2  // Required

    // Performance-based
    avgFTRAN := state.totalFTRANTime / MAX(state.ftranCount, 1)
    IF avgFTRAN > state.baselineFTRAN * 3.0:
        RETURN 1  // Recommended

    // Iteration-based
    IF state.iteration - state.lastRefactorIter > env.RefactorInterval:
        RETURN 1  // Recommended

    RETURN 0  // Not needed
```

## 5. Complexity

### 5.1 Time Complexity

- O(1) - simple comparisons

### 5.2 Space Complexity

- O(1) - no allocation

## 6. Error Handling

No error conditions - always returns valid recommendation.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Just refactored | etaCount = 0 | Return 0 |
| At hard limit | etaCount = maxEtaCount | Return 2 |
| Slow FTRAN | High average time | Return 1 |

## 8. Thread Safety

**Thread-safe:** Yes (read-only comparisons)

## 9. Dependencies

### 9.1 Functions Called

None - pure computation

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_simplex_iterate | Simplex | After each pivot |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_basis_refactor | Called when this returns > 0 |

## 11. Design Notes

### 11.1 Design Rationale

**Multiple criteria:** Different problems benefit from different refactorization strategies. Combining criteria handles varied problem characteristics.

**Hard vs soft limits:** Hard limits prevent catastrophic slowdown; soft limits optimize average performance.

**Return values:** Three-level return allows caller to prioritize (2 = must refactor now, 1 = good time to refactor, 0 = not needed).

### 11.2 Typical Parameters

- maxEtaCount: 100-500
- refactorInterval: 50-200 iterations
- FTRAN time threshold: 2-5x baseline

## 12. References

- Maros, I. (2003). "Computational Techniques of the Simplex Method" - Refactorization strategies

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
