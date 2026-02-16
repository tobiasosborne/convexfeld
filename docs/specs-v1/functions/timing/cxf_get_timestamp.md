# cxf_get_timestamp

**Module:** Timing
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Returns the current wall-clock time as a high-resolution timestamp suitable for measuring elapsed time intervals. This function provides a monotonic time value with microsecond precision that serves as the foundation for timeout checking, solve time tracking, performance measurement, and callback timing throughout the optimization process. The timestamp can be subtracted from a previously recorded timestamp to calculate elapsed time in seconds.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| (none) | - | Function takes no parameters | - | - |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return value | double | Current timestamp in seconds since an arbitrary epoch |

### 2.3 Side Effects

None. The function is read-only and queries system time without modifying any state.

## 3. Contract

### 3.1 Preconditions

- [ ] High-resolution performance counter must be available on the system (always true on modern platforms)

### 3.2 Postconditions

- [ ] Returns a valid timestamp value greater than or equal to previous calls (monotonic)
- [ ] The timestamp represents seconds with microsecond precision
- [ ] Subtracting two timestamps yields accurate elapsed time in seconds

### 3.3 Invariants

- [ ] Function has no side effects
- [ ] Returned values are monotonically increasing within a single system boot
- [ ] The zero point (epoch) is arbitrary but consistent for a system boot

## 4. Algorithm

### 4.1 Overview

The function queries the operating system's high-resolution performance counter to obtain a tick count representing the current time. This tick count is obtained from a monotonic clock that increments at a fixed frequency, typically derived from the CPU's Time Stamp Counter on modern systems. The raw tick count is converted to seconds by querying the counter's frequency (ticks per second) and performing floating-point division. The result is a double-precision value representing time in seconds since an arbitrary but fixed reference point.

On Windows, this uses QueryPerformanceCounter and QueryPerformanceFrequency APIs. On other platforms, equivalent high-resolution monotonic timers are used (e.g., clock_gettime with CLOCK_MONOTONIC on Linux, mach_absolute_time on macOS).

### 4.2 Detailed Steps

1. Query the system's high-resolution performance counter to obtain the current tick count
2. Query the performance counter frequency to determine ticks per second
3. Convert the tick count to seconds by dividing the counter value by the frequency
4. Return the resulting timestamp as a double-precision floating-point value

### 4.3 Pseudocode

```
FUNCTION get_timestamp() → double
    counter ← query_performance_counter()
    frequency ← query_performance_frequency()
    timestamp ← counter / frequency
    RETURN timestamp
END FUNCTION
```

### 4.4 Mathematical Foundation

Given:
- C = current counter value (ticks)
- F = counter frequency (ticks per second)

Timestamp T in seconds:
- T = C / F

Elapsed time between two timestamps:
- Δt = T₂ - T₁ = (C₂ - C₁) / F

Precision:
- Time resolution = 1 / F
- Typical F = 10 MHz → resolution = 100 nanoseconds
- Practical precision limited by OS to ~1 microsecond

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1)
- **Average case:** O(1)
- **Worst case:** O(1)

The function performs two constant-time system queries and one division operation.

### 5.2 Space Complexity

- **Auxiliary space:** O(1)
- **Total space:** O(1)

No memory allocation; only local variables for counter and frequency values.

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| Performance counter unavailable | N/A | Extremely rare; would return 0.0 on Windows |
| Frequency zero | N/A | Should never occur on supported platforms |

### 6.2 Error Behavior

On modern systems, performance counter APIs are guaranteed to succeed. In the extraordinarily unlikely event of failure, the Windows API returns zero for both counter and frequency, which would result in a timestamp of 0.0. The function does not check for or handle this condition, as it represents a catastrophic system failure beyond application control.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| First call after boot | (none) | Returns timestamp relative to boot or fixed epoch |
| Repeated calls | (none) | Returns monotonically increasing values |
| Very long uptime | (none) | Remains accurate for ~100+ years before counter overflow |
| Cross-thread calls | (none) | Returns consistent values; same reference across all threads |
| During DST change | (none) | Unaffected; uses monotonic clock not wall-clock time |

## 8. Thread Safety

**Thread-safe:** Yes

The function performs read-only system queries and maintains no shared state. Multiple threads can call this function concurrently without synchronization. The underlying performance counter is system-wide and thread-safe.

**Synchronization required:** None

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| QueryPerformanceCounter | Windows API | Obtain current performance counter tick value |
| QueryPerformanceFrequency | Windows API | Obtain performance counter frequency (ticks/second) |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_init_solve_state | Solver Core | Record solve start time |
| cxf_setcallbackfunc | Callbacks | Record callback registration time |
| cxf_optimize | API | Check for timeout conditions during optimization |
| cxf_cleanup_solve_state | Solver Core | Calculate total solve time |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_timing_start | Uses similar mechanism to record start timestamp in structure |
| cxf_timing_end | Uses similar mechanism to record end timestamp |
| cxf_timing_elapsed_wall | Returns elapsed wall-clock time from timing structure |
| cxf_timing_elapsed_cpu | Returns CPU time variant (process time vs wall time) |

## 11. Design Notes

### 11.1 Design Rationale

The function provides a simple, stateless interface for obtaining timestamps, making it suitable for use in diverse contexts (solve state initialization, timeout checking, callback timing) without requiring complex setup. By returning time in seconds as a double rather than raw ticks, the interface is platform-agnostic and human-readable, simplifying timeout comparisons and logging.

The choice of double precision provides sufficient range and precision: 53 bits of mantissa allows exact representation of microseconds over a range of approximately 285 years. This far exceeds any practical optimization run time while maintaining microsecond precision.

### 11.2 Performance Considerations

The function is extremely fast, typically completing in 20-50 nanoseconds on modern x64 systems. The performance counter query uses the CPU's Time Stamp Counter (TSC) via a non-serializing read on modern processors, causing minimal pipeline disruption. On Windows, the frequency query is also fast (a memory read from cached kernel data) due to vDSO-like mechanisms.

An alternative design could cache the frequency value to eliminate one system call, reducing overhead from ~50 to ~30 nanoseconds. However, QueryPerformanceFrequency is already extremely fast on modern systems, and the simplicity of the non-cached approach (no static state, simpler code) may outweigh the minor performance benefit.

### 11.3 Future Considerations

For cross-platform support, the implementation could be abstracted behind a platform-specific wrapper:
- Windows: QueryPerformanceCounter / QueryPerformanceFrequency
- Linux: clock_gettime(CLOCK_MONOTONIC)
- macOS: mach_absolute_time with mach_timebase_info
- C++11: std::chrono::high_resolution_clock
- C11: timespec_get

The current specification focuses on the Windows implementation but the contract and semantics apply to all platforms.

## 12. References

- Windows API Documentation: QueryPerformanceCounter, QueryPerformanceFrequency
- Intel Software Developer's Manual: Time Stamp Counter (RDTSC)
- POSIX: clock_gettime specification
- IEEE 754: Double Precision Floating Point Standard
- "Acquiring high-resolution time stamps" - Microsoft MSDN

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
