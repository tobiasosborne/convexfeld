# cxf_timing_update

**Module:** Timing
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Updates accumulated timing statistics and derived performance metrics after a timed operation completes. This function calculates elapsed time from recorded start and end timestamps, accumulates the time into category-specific totals, increments operation counters, and computes derived metrics such as average operation time and iteration rate. The statistics support performance profiling, progress reporting, bottleneck identification, and adaptive algorithm parameter tuning throughout the optimization process.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| timing | TimingState* | Pointer to timing state structure | Non-NULL pointer | Yes |
| category | int | Timing category/section identifier | 0 to MAX_SECTIONS-1 | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| (none) | void | Function has no return value |

### 2.3 Side Effects

Modifies multiple fields in the TimingState structure:
- Computes elapsed time from start/end timestamps if not already calculated
- Accumulates elapsed time into category-specific total time counter
- Increments operation count for the specified category
- Updates average time for the category
- Stores last elapsed time for the category
- Updates overall iteration rate if category is total/overall timing

## 3. Contract

### 3.1 Preconditions

- [ ] timing pointer must be valid (non-NULL)
- [ ] category must be within valid range [0, MAX_SECTIONS)
- [ ] timing structure must have valid frequency value
- [ ] Start and end timestamps should have been recorded previously
- [ ] If elapsed time not pre-calculated, endCounter must be >= startCounter

### 3.2 Postconditions

- [ ] elapsedSeconds field contains the calculated elapsed time (if it was zero)
- [ ] totalTime[category] has been incremented by elapsed time
- [ ] operationCount[category] has been incremented by 1
- [ ] averageTime[category] equals totalTime[category] / operationCount[category]
- [ ] lastElapsed[category] equals the measured elapsed time
- [ ] If category is TOTAL, iterationRate has been updated

### 3.3 Invariants

- [ ] Frequency field remains unchanged
- [ ] Start and end timestamps remain unchanged
- [ ] Statistics for other categories remain unchanged
- [ ] All time values remain non-negative
- [ ] Operation counts are monotonically increasing

## 4. Algorithm

### 4.1 Overview

The function first ensures that the elapsed time for the current measurement interval has been calculated, either retrieving it from a pre-computed field or computing it from the start and end timestamps. The elapsed time is then added to the cumulative total for the specified timing category, and the operation count is incremented. The average time per operation for the category is recalculated as total time divided by operation count. The function also stores the elapsed time for potential immediate reporting and updates the overall iteration rate if the category represents total solver operations.

Timing categories typically include: overall operations, pricing, pivot operations, basis refactorization, forward/backward transformations, and other algorithmic phases. This categorization enables detailed performance breakdowns showing where computational time is spent.

### 4.2 Detailed Steps

1. Validate that the category parameter is within the valid range [0, MAX_SECTIONS)
2. If category is invalid, return immediately without modifying state
3. Check if elapsedSeconds field is zero (indicating elapsed time not yet computed)
4. If elapsed time is zero and frequency is non-zero:
   a. Calculate elapsed ticks as (endCounter - startCounter)
   b. Convert to seconds by dividing by frequency
   c. Store result in elapsedSeconds field
5. Otherwise, retrieve the pre-computed elapsed time from elapsedSeconds field
6. Add elapsed time to the category's total time accumulator: totalTime[category] += elapsed
7. Increment the category's operation counter: operationCount[category]++
8. If operation count is greater than zero (should always be true after increment):
   a. Calculate new average: averageTime[category] = totalTime[category] / operationCount[category]
9. Store the elapsed time in the category's last elapsed field: lastElapsed[category] = elapsed
10. If category represents overall/total timing and total time is positive:
    a. Calculate iteration rate: iterationRate = operationCount[TOTAL] / totalTime[TOTAL]

### 4.3 Pseudocode

```
PROCEDURE timing_update(timing, category)
    // Validate category
    IF category < 0 OR category >= MAX_SECTIONS THEN
        RETURN  // Invalid category
    END IF

    // Ensure elapsed time is calculated
    IF timing.elapsedSeconds = 0.0 AND timing.frequency ≠ 0 THEN
        delta ← timing.endCounter - timing.startCounter
        elapsed ← delta / timing.frequency
        timing.elapsedSeconds ← elapsed
    ELSE
        elapsed ← timing.elapsedSeconds
    END IF

    // Accumulate statistics for this category
    timing.totalTime[category] ← timing.totalTime[category] + elapsed
    timing.operationCount[category] ← timing.operationCount[category] + 1

    // Update average
    IF timing.operationCount[category] > 0 THEN
        timing.averageTime[category] ← timing.totalTime[category] / timing.operationCount[category]
    END IF

    // Store last elapsed
    timing.lastElapsed[category] ← elapsed

    // Update iteration rate if this is total timing
    IF category = CATEGORY_TOTAL AND timing.totalTime[CATEGORY_TOTAL] > 0.0 THEN
        timing.iterationRate ← timing.operationCount[CATEGORY_TOTAL] / timing.totalTime[CATEGORY_TOTAL]
    END IF
END PROCEDURE
```

### 4.4 Mathematical Foundation

Elapsed time calculation:
- T = (E - S) / F
- Where E = end timestamp, S = start timestamp, F = frequency

Accumulated total after n measurements:
- Total_n = Σᵢ₌₁ⁿ Tᵢ

Average time per operation:
- Avg = Total_n / n

Iteration rate (operations per second):
- Rate = n / Total_n

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1)
- **Average case:** O(1)
- **Worst case:** O(1)

All operations are constant time: arithmetic, array indexing, and comparisons.

### 5.2 Space Complexity

- **Auxiliary space:** O(1)
- **Total space:** O(1)

No memory allocation; operates on existing structure fields.

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| Invalid category | N/A | Validated and causes early return |
| NULL pointer | N/A | Would cause segmentation fault; caller responsibility |
| Division by zero | N/A | Prevented by checking operationCount > 0 before dividing |
| Uninitialized frequency | N/A | Would cause incorrect elapsed time calculation |

### 6.2 Error Behavior

The function validates the category parameter and returns silently if invalid, preventing array out-of-bounds access. Other error conditions are not explicitly checked; callers must ensure the timing structure is properly initialized. If frequency is zero or invalid, the elapsed time calculation would produce incorrect results or infinity/NaN.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| First update for category | operationCount[category] = 0 | Sets count to 1, average equals first measurement |
| Pre-computed elapsed time | elapsedSeconds already set | Uses existing value, does not recalculate |
| Zero elapsed time | End = Start | Accumulates 0.0 seconds; valid behavior |
| Very large accumulated time | Millions of operations | May lose precision due to floating-point rounding |
| Invalid category | category < 0 or >= MAX_SECTIONS | Returns without modification |

## 8. Thread Safety

**Thread-safe:** No

The function modifies multiple fields in the timing structure without synchronization. Concurrent calls from multiple threads would cause race conditions and incorrect accumulated statistics.

**Synchronization required:** Each thread should maintain its own TimingState structure. If shared timing is needed, callers must provide external synchronization (mutex/lock).

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| (none) | - | Function performs only arithmetic and memory operations |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_simplex_iterate | Simplex | Update iteration timing after each simplex step |
| cxf_basis_refactor | Basis | Update refactorization timing statistics |
| cxf_pricing_step | Pricing | Update pricing operation statistics |
| cxf_solve_lp | Solver Core | Update overall solve timing |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_timing_start | Records start timestamp; pairs with update |
| cxf_timing_end | Records end timestamp; may call update afterward |
| cxf_timing_pivot | Specialized update for pivot operations with work tracking |
| cxf_timing_mark | Records intermediate checkpoints for multi-phase timing |
| cxf_timing_elapsed_wall | Queries current elapsed time without updating statistics |

## 11. Design Notes

### 11.1 Design Rationale

The function separates timing measurement (start/end) from statistics accumulation (update), providing flexibility in when and how statistics are computed. By supporting multiple independent categories, the design allows detailed performance breakdowns without requiring separate timing structures for each operation type.

The separation of last elapsed time from accumulated totals enables both immediate reporting (e.g., "last iteration took 0.5ms") and aggregate statistics (e.g., "1000 iterations, average 0.45ms"). The derived metrics (average, rate) are computed on each update rather than on demand, trading a small computational cost for instant availability when needed for logging or callbacks.

### 11.2 Performance Considerations

The function is called after measured operations that typically take milliseconds or longer, making its own overhead (tens of nanoseconds) negligible. The immediate calculation of averages and rates simplifies calling code and avoids repeated divisions when these metrics are accessed multiple times.

Floating-point accumulation could theoretically lose precision after millions of operations, but for typical solver runs (thousands to millions of iterations over hours), the error remains well below measurement noise. If extreme precision is required, Kahan summation could be employed.

### 11.3 Future Considerations

The current design uses simple arithmetic mean for averages. Weighted averages or exponential moving averages could provide better responsiveness to recent performance changes in adaptive algorithms.

The category-based design could be extended with:
- Hierarchical categories (parent/child relationships)
- Automatic detection of bottlenecks (categories exceeding expected percentages)
- Dynamic category creation for ad-hoc timing
- Percentile tracking (min/max/median in addition to average)

## 12. References

- "The Art of Computer Programming, Vol 2: Seminumerical Algorithms" - Donald Knuth (floating-point accuracy)
- "What Every Computer Scientist Should Know About Floating-Point Arithmetic" - David Goldberg
- IEEE 754 Standard for Floating-Point Arithmetic
- Windows Performance Counter documentation (Microsoft MSDN)

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
