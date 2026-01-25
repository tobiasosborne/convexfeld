# cxf_timing_start

**Module:** Timing
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Records a high-resolution timestamp to mark the beginning of a timed section in the optimization process. This function initializes timing for performance measurement of solver phases such as presolve, simplex iterations, basis refactorization, pricing operations, and other algorithmic steps. The timestamp is later compared with an end timestamp to calculate elapsed time for profiling, progress reporting, and adaptive algorithm decisions.

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

Modifies the TimingState structure by recording the current high-resolution timestamp in the startCounter field. This timestamp represents the current moment in time using a monotonic, high-frequency counter suitable for precise elapsed time measurements.

## 3. Contract

### 3.1 Preconditions

- [ ] timing pointer must be valid (non-NULL)
- [ ] timing structure must be properly initialized
- [ ] High-resolution performance counter must be available on the system

### 3.2 Postconditions

- [ ] timing->startCounter contains a valid high-resolution timestamp
- [ ] The timestamp is obtained from the system's monotonic high-resolution timer
- [ ] The timestamp can be used to calculate elapsed time when paired with a subsequent end timestamp

### 3.3 Invariants

- [ ] No other fields in the timing structure are modified
- [ ] The function does not allocate or free memory
- [ ] The function has no effect on global state

## 4. Algorithm

### 4.1 Overview

The function queries the operating system's high-resolution performance counter to obtain a monotonic timestamp value. On Windows systems, this uses the QueryPerformanceCounter API which provides microsecond-precision timing based on the CPU's Time Stamp Counter on modern processors. The raw counter value is stored in the timing structure's startCounter field for later comparison with an end timestamp.

The timestamp represents wall-clock time with very high precision, suitable for measuring operations that may complete in milliseconds or microseconds. The counter is guaranteed to be monotonic and consistent across all CPU cores on modern systems.

### 4.2 Detailed Steps

1. Query the system's high-resolution performance counter to obtain the current timestamp value
2. Store the raw counter value in the timing structure's startCounter field
3. Return to caller (no cleanup or additional processing required)

### 4.3 Mathematical Foundation

The performance counter provides a tick count at a fixed frequency (typically 10 MHz on Windows systems):

- Counter value C(t) represents time t in ticks
- Frequency F is constant (ticks per second)
- Elapsed time = (C(t₂) - C(t₁)) / F seconds

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1)
- **Average case:** O(1)
- **Worst case:** O(1)

The function performs a constant-time system query and a single memory store operation.

### 5.2 Space Complexity

- **Auxiliary space:** O(1)
- **Total space:** O(1)

No additional memory allocation; only modifies one 64-bit field in the existing structure.

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| NULL pointer | N/A | Would cause segmentation fault; caller must ensure valid pointer |
| Performance counter unavailable | N/A | Extremely rare; would return zero timestamp on Windows |

### 6.2 Error Behavior

The function does not perform explicit error checking or return error codes. Callers are responsible for ensuring the timing pointer is valid. On the rare occasion that the performance counter is unavailable, the system API returns zero, which would be stored as the timestamp.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| First call | Uninitialized timing structure | Records timestamp; frequency field should be initialized separately |
| Repeated calls | Same timing structure | Overwrites previous start timestamp |
| Very long measurement | Start recorded, end delayed hours | Counter wraps after ~100 years; no practical concern |

## 8. Thread Safety

**Thread-safe:** Conditionally

The function itself is thread-safe as it only performs a read-only system call and writes to caller-provided memory. However, if multiple threads share the same TimingState structure, race conditions may occur. Each solver thread should maintain its own TimingState to avoid conflicts.

**Synchronization required:** None for the function itself; callers must ensure exclusive access to the TimingState structure if sharing between threads.

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| QueryPerformanceCounter | Windows API | Obtain high-resolution timestamp from system |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_simplex_iterate | Simplex | Start timing for iteration phases |
| cxf_solve_lp | Solver Core | Start timing for overall LP solve |
| cxf_basis_refactor | Basis | Start timing for refactorization |
| cxf_pricing_step | Pricing | Start timing for pricing operations |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_timing_end | Paired function that records end timestamp and calculates elapsed time |
| cxf_timing_mark | Records checkpoint timestamps for nested timing sections |
| cxf_get_timestamp | Returns current timestamp as double (higher-level interface) |
| cxf_timing_update | Updates accumulated timing statistics |

## 11. Design Notes

### 11.1 Design Rationale

The function is deliberately simple and lightweight to minimize overhead in performance-critical code paths. By only recording the timestamp without performing calculations, the function can be inlined and executed in approximately 20-50 nanoseconds on modern systems. The separation of start/end operations allows flexible timing of arbitrary code sections without requiring structured wrappers.

### 11.2 Performance Considerations

The function is designed to be extremely fast and suitable for inline expansion by the compiler. On modern x64 processors with invariant Time Stamp Counter support, the performance counter query does not cause pipeline serialization and completes in a handful of CPU cycles. The overhead is negligible even when called thousands of times per second in tight iteration loops.

### 11.3 Future Considerations

The implementation assumes a single start timestamp per timing structure. To support nested timing or multiple simultaneous sections, the structure could be extended with arrays of start timestamps indexed by section identifier. This would allow hierarchical timing without requiring separate TimingState allocations.

## 12. References

- Windows API Documentation: QueryPerformanceCounter
- Intel Architecture Manual: Time Stamp Counter (TSC)
- MSDN: Acquiring high-resolution time stamps

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
