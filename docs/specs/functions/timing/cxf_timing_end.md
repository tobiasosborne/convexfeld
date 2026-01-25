# cxf_timing_end

**Module:** Timing
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Records a high-resolution end timestamp and accumulates the elapsed time since the corresponding start timestamp into timing statistics. This function completes a timing measurement initiated by a previous call to the timing start function, calculating the time spent in the measured section and updating accumulated totals, operation counts, and derived metrics. The function supports performance profiling, progress reporting, time limit enforcement, and adaptive algorithm tuning throughout the optimization process.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| timing | TimingState* | Pointer to timing state structure | Non-NULL pointer | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| (none) | void | Function has no return value |

### 2.3 Side Effects

Modifies multiple fields in the TimingState structure:
- Records the current high-resolution timestamp as the end time
- Calculates elapsed time by subtracting start timestamp from end timestamp
- Accumulates elapsed time into the current section's total time counter
- Increments the operation counter for the current section
- Updates the last elapsed time field for reporting purposes

## 3. Contract

### 3.1 Preconditions

- [ ] timing pointer must be valid (non-NULL)
- [ ] timing structure must have been initialized with valid frequency
- [ ] A corresponding start timestamp must have been recorded previously
- [ ] Current section index must be valid (0-7)
- [ ] High-resolution performance counter must be available

### 3.2 Postconditions

- [ ] timing->endCounter contains the current high-resolution timestamp
- [ ] Elapsed time has been calculated as (endCounter - startCounter) / frequency
- [ ] Section's total accumulated time has been incremented by elapsed time
- [ ] Section's operation count has been incremented by 1
- [ ] Last elapsed time field contains the measured interval
- [ ] No overflow or undefined behavior in time calculations

### 3.3 Invariants

- [ ] Frequency field remains unchanged
- [ ] Start timestamp remains unchanged
- [ ] Other sections' statistics remain unchanged
- [ ] Total time is non-decreasing (always accumulates positively)

## 4. Algorithm

### 4.1 Overview

The function queries the system's high-resolution performance counter to obtain the current timestamp, then calculates the elapsed time since the previous start timestamp by subtracting and dividing by the cached counter frequency. This elapsed time is added to the running total for the currently active timing section, and the section's operation counter is incremented. The function uses floating-point arithmetic to maintain microsecond precision while supporting measurements spanning hours or days.

The algorithm handles multiple independent timing sections (typically 6-8 sections for different operation types like pricing, pivoting, refactorization) by indexing into arrays based on the current section identifier stored in the timing structure.

### 4.2 Detailed Steps

1. Query the high-resolution performance counter to obtain the current timestamp
2. Retrieve the current section index from the timing structure (field or 0x04)
3. Validate that the section index is within the valid range (0 to maximum sections minus 1)
4. If section index is invalid, return immediately without modifying state
5. Retrieve the start timestamp for the current section from the section start array
6. Calculate elapsed ticks by subtracting start timestamp from current timestamp
7. Convert elapsed ticks to seconds by dividing by the cached frequency value
8. Add the elapsed seconds to the section's accumulated total time
9. Store the elapsed time in the last elapsed field for reporting
10. Increment the section's operation call counter by 1

### 4.3 Pseudocode

```
PROCEDURE timing_end(timing)
    current_time ← query_performance_counter()
    section ← timing.current_section

    IF section < 0 OR section >= MAX_SECTIONS THEN
        RETURN  // Invalid section, ignore
    END IF

    start_time ← timing.section_start[section]
    elapsed_ticks ← current_time - start_time
    elapsed_seconds ← elapsed_ticks / timing.frequency

    timing.section_total[section] ← timing.section_total[section] + elapsed_seconds
    timing.last_elapsed ← elapsed_seconds
    timing.call_count[section] ← timing.call_count[section] + 1
END PROCEDURE
```

### 4.3 Mathematical Foundation

Given:
- S = start timestamp (ticks)
- E = end timestamp (ticks)
- F = performance counter frequency (ticks/second)

Elapsed time T in seconds:
- T = (E - S) / F

Accumulated time after n measurements:
- Total = Σᵢ₌₁ⁿ Tᵢ

Average time per operation:
- Average = Total / n

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1)
- **Average case:** O(1)
- **Worst case:** O(1)

All operations are constant time: one system call, array indexing, arithmetic operations, and memory stores.

### 5.2 Space Complexity

- **Auxiliary space:** O(1)
- **Total space:** O(1)

No memory allocation; all operations work on existing structure fields.

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| Invalid section index | N/A | Function validates and returns silently if section < 0 or >= 8 |
| NULL pointer | N/A | Would cause segmentation fault; caller responsibility |
| Uninitialized frequency | N/A | Would cause division by zero or incorrect results |

### 6.2 Error Behavior

The function performs defensive validation of the section index to prevent array out-of-bounds access. Invalid section indices result in early return without modifying state. No error codes or exceptions are produced. Callers must ensure the timing structure is properly initialized before use.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Zero elapsed time | End called immediately after start | Accumulates ~0 seconds; valid behavior |
| Very small intervals | Microsecond-scale measurements | Maintains precision; double can represent microseconds |
| Long measurements | Hours between start and end | Handles correctly up to ~100 years before counter overflow |
| Repeated measurements | Many iterations | Accumulates accurately; potential floating-point rounding after millions of operations |
| Missing start call | End without corresponding start | Uses stale/invalid start timestamp; produces incorrect elapsed time |

## 8. Thread Safety

**Thread-safe:** No

The function modifies multiple fields in the timing structure without synchronization. Concurrent calls from multiple threads using the same TimingState structure would cause race conditions and data corruption.

**Synchronization required:** Each thread should maintain its own TimingState structure. If timing data must be shared between threads, callers must provide external synchronization (e.g., mutex locks).

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| QueryPerformanceCounter | Windows API | Obtain current high-resolution timestamp |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_simplex_iterate | Simplex | End timing for iteration after pivot completion |
| cxf_solve_lp | Solver Core | End timing for overall LP solve |
| cxf_basis_refactor | Basis | End timing after refactorization completes |
| cxf_pricing_step | Pricing | End timing after pricing phase |
| SimplexMainLoop | Solver Core | End timing in main iteration loop |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_timing_start | Paired function that records the initial timestamp |
| cxf_timing_update | Updates statistics; may be called instead of or after this function |
| cxf_timing_pivot | Specialized timing function for pivot operations |
| cxf_timing_elapsed_wall | Queries elapsed wall-clock time without stopping timer |
| cxf_timing_elapsed_cpu | Queries elapsed CPU time variant |

## 11. Design Notes

### 11.1 Design Rationale

The function separates timestamp recording from statistics calculation, allowing flexible use patterns. By storing the last elapsed time separately from accumulated totals, the implementation supports both incremental accumulation and point-in-time queries. The section-based design allows independent timing of different operation types without requiring separate timing structures for each category.

The defensive section index validation prevents crashes from uninitialized or corrupted timing state, trading a small performance cost for robustness in a non-critical path (timing overhead is negligible compared to measured operations).

### 11.2 Performance Considerations

While timing overhead should be minimized, the measured operations (simplex iterations, refactorizations) typically take milliseconds, making the ~50 nanosecond overhead of this function negligible (less than 0.01% overhead). The function is not typically inlined due to the system call, but the performance impact is acceptable for its profiling purpose.

Floating-point division is used instead of integer arithmetic to maintain precision and simplify elapsed time calculations. Modern CPUs execute floating-point division in a few cycles, making this choice reasonable.

### 11.3 Future Considerations

The current design uses separate arrays for start times, totals, and counts. An alternative object-oriented design with per-section timing objects could improve cache locality and clarity but would require more complex memory management. The current flat array structure is simple and efficient for the typical 6-8 sections used.

For very high-frequency timing (millions of measurements), Kahan summation could be employed to prevent floating-point rounding errors in the accumulated totals, though this is unlikely to be necessary in practice.

## 12. References

- Windows API Documentation: QueryPerformanceCounter, QueryPerformanceFrequency
- IEEE 754 Double Precision Floating Point Standard
- "High-Resolution Timing in Windows Applications" - Microsoft MSDN
- "What Every Computer Scientist Should Know About Floating-Point Arithmetic" - David Goldberg

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
