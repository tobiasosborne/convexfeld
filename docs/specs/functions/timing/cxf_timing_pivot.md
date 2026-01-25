# cxf_timing_pivot

**Module:** Timing
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Records computational work performed during simplex pivot operations and accumulates metrics used to trigger basis refactorization decisions and track performance statistics. This function updates work counters that help the solver determine when accumulated numerical errors and eta vector density warrant rebuilding the basis factorization from scratch. The function also maintains detailed timing breakdowns for pricing, ratio test, and basis update phases to support performance profiling and adaptive algorithm tuning.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| state | SolverState* | Solver state structure containing timing and work tracking fields | Non-NULL pointer | Yes |
| pricing_work | double | Work units spent in pricing phase (selecting entering variable) | >= 0.0 | Yes |
| ratio_work | double | Work units spent in ratio test phase (selecting leaving variable) | >= 0.0 | Yes |
| update_work | double | Work units spent in basis update phase (creating eta vector) | >= 0.0 | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| (none) | void | Function has no return value |

### 2.3 Side Effects

Modifies solver state fields:
- Accumulates total work into the work counter (used for refactorization triggering)
- Updates detailed timing statistics for pricing, ratio test, and update phases
- Increments pivot operation counter
- May indirectly influence subsequent refactorization decisions through accumulated work threshold

## 3. Contract

### 3.1 Preconditions

- [ ] state pointer must be valid (non-NULL)
- [ ] Work parameters (pricing_work, ratio_work, update_work) must be non-negative
- [ ] Solver state structure must be properly initialized
- [ ] If work counter is enabled, the work counter pointer must be valid

### 3.2 Postconditions

- [ ] Work counter has been incremented by (pricing_work + ratio_work + update_work) * scaleFactor
- [ ] If timing state exists, phase-specific timing totals have been updated
- [ ] Pivot operation counter has been incremented
- [ ] All accumulated values remain non-negative and finite

### 3.3 Invariants

- [ ] Work counter is monotonically increasing (never decreases)
- [ ] Pivot count is monotonically increasing
- [ ] Other solver state fields remain unchanged
- [ ] Function does not trigger refactorization directly (only updates counters)

## 4. Algorithm

### 4.1 Overview

The function accumulates computational work metrics from the three main phases of a simplex pivot operation: pricing (finding the entering variable by computing reduced costs), ratio test (finding the leaving variable via backward transformation), and basis update (creating an eta vector and updating the basis inverse representation).

The work units are scaled by a problem-specific factor and accumulated into a global work counter. When this counter exceeds a threshold, the solver will trigger basis refactorization to clear accumulated eta vectors and restore numerical stability. The function also updates detailed timing breakdowns if performance profiling is enabled.

Work units typically represent operation counts (e.g., number of non-zero elements processed) rather than wall-clock time, providing a platform-independent measure of computational effort.

### 4.2 Detailed Steps

1. Retrieve the work counter pointer from solver state (offset 0x448)
2. If work counter is not NULL (work tracking is enabled):
   a. Retrieve the scale factor from solver state (offset 0x438)
   b. Calculate total work as sum of pricing_work, ratio_work, and update_work
   c. Multiply total work by scale factor to normalize for problem characteristics
   d. Add scaled work to the accumulated work counter value
3. Retrieve the timing state pointer from solver state (offset 0x3c8)
4. If timing state is not NULL (detailed timing is enabled):
   a. Add pricing_work to the pricing time accumulator
   b. Add ratio_work to the ratio test time accumulator
   c. Add update_work to the update time accumulator
   d. Increment the pivot operation counter

### 4.3 Pseudocode

```
PROCEDURE timing_pivot(state, pricing_work, ratio_work, update_work)
    work_counter ← state.work_counter_ptr  // Pointer

    IF work_counter ≠ NULL THEN
        scale_factor ← state.scale_factor  // Value
        total_work ← pricing_work + ratio_work + update_work
        scaled_work ← total_work × scale_factor
        *work_counter ← *work_counter + scaled_work
    END IF

    timing_state ← state.timing_state_ptr  // Pointer

    IF timing_state ≠ NULL THEN
        timing_state.pricing_time ← timing_state.pricing_time + pricing_work
        timing_state.ratio_test_time ← timing_state.ratio_test_time + ratio_work
        timing_state.update_time ← timing_state.update_time + update_work
        timing_state.pivot_count ← timing_state.pivot_count + 1
    END IF
END PROCEDURE
```

### 4.4 Mathematical Foundation

Work accumulation model:
- W_total = Σᵢ (W_pricing,i + W_ratio,i + W_update,i) × S
- Where S is a scale factor normalizing for problem size and characteristics

Refactorization trigger condition (evaluated elsewhere):
- If W_total > T_refactor, then trigger basis refactorization
- T_refactor is adaptive based on problem density, size, and numerical stability

Work unit estimation:
- W_pricing ≈ number of candidate variables evaluated
- W_ratio ≈ number of non-zeros in pivot column (BTRAN work)
- W_update ≈ number of non-zeros in eta vector created

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1)
- **Average case:** O(1)
- **Worst case:** O(1)

All operations are constant time: pointer dereferences, arithmetic, and memory stores.

### 5.2 Space Complexity

- **Auxiliary space:** O(1)
- **Total space:** O(1)

No memory allocation; only updates existing structure fields.

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| NULL state pointer | N/A | Would cause segmentation fault; caller responsibility |
| Negative work values | N/A | Not validated; would accumulate incorrectly |
| NULL pointers in state | N/A | Checked before dereferencing; NULL disables that feature |

### 6.2 Error Behavior

The function performs NULL checks on work counter and timing state pointers before dereferencing, gracefully handling cases where these features are disabled. No error codes are returned. Invalid work values (negative or NaN) are not validated and would be accumulated as-is, potentially causing issues in downstream refactorization logic.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Zero work | All work parameters = 0.0 | Increments pivot count but adds no work |
| Work tracking disabled | work_counter_ptr = NULL | Only updates timing state if present |
| Timing disabled | timing_state_ptr = NULL | Only updates work counter if present |
| Both disabled | Both pointers NULL | Function does nothing (no-op) |
| Very large work | Extreme work values | Accumulates; may trigger immediate refactorization |

## 8. Thread Safety

**Thread-safe:** No

The function modifies shared state without synchronization. Concurrent calls from multiple threads operating on the same solver state would cause race conditions in work counter and timing accumulations.

**Synchronization required:** In parallel simplex implementations, each thread should maintain separate work counters and timing states. These can be aggregated at synchronization points if needed.

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| (none) | - | Function performs only arithmetic and memory operations |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_pivot_primal | Pivot | After completing a primal pivot operation |
| cxf_pivot_dual | Pivot | After completing a dual pivot operation |
| cxf_simplex_step | Simplex | Record work for simplex iteration step |
| cxf_simplex_iterate | Simplex | Main iteration loop pivot tracking |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_timing_refactor | Similar work tracking but for refactorization operations |
| cxf_timing_update | General timing statistics update for various operation types |
| cxf_basis_refactor | Consumes the work counter to decide refactorization timing |
| cxf_timing_start | Records start timestamp for timed sections |
| cxf_timing_end | Records end timestamp and accumulates time |

## 11. Design Notes

### 11.1 Design Rationale

The function separates work tracking (for refactorization decisions) from timing statistics (for performance analysis) by using independent pointers that can be NULL to disable each feature. This allows production builds to disable expensive timing while maintaining essential work tracking for algorithmic correctness.

The three-phase breakdown (pricing, ratio test, update) reflects the natural structure of simplex pivot operations and allows detailed performance analysis to identify bottlenecks. Work units are kept separate from wall-clock time to provide platform-independent metrics suitable for algorithmic decisions.

### 11.2 Performance Considerations

The function is called once per simplex pivot, which may occur thousands of times per second for small problems or tens of times per second for large problems. The overhead is minimal (a few dozen CPU cycles) and negligible compared to the actual pivot operation (typically thousands to millions of cycles).

The use of pointers to enable/disable features allows zero overhead when timing is disabled, as the NULL check causes early return before any expensive operations.

### 11.3 Future Considerations

The current design uses simple accumulated work counters. More sophisticated approaches could use:
- Moving averages to detect trends in pivot work
- Separate work counters for primal vs dual pivots
- Decay factors to emphasize recent work over old work
- Histogram of work distribution to detect pathological cases

The phase breakdown could be extended to include:
- FTRAN work (forward transformation)
- Pricing strategy overhead
- Degeneracy detection work

## 12. References

- Dantzig, G. (1963). "Linear Programming and Extensions" - Chapter on revised simplex method
- Reid, J.K. (1982). "A sparsity-exploiting variant of the Bartels-Golub decomposition for linear programming bases" - Refactorization strategies
- Forrest, J.J. & Tomlin, J.A. (1972). "Updated triangular factors of the basis to maintain sparsity in the product form simplex method" - Eta vector management

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
