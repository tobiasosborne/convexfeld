# Module: Timing

**Spec Version:** 1.0
**Last Updated:** 2026-01-25
**Functions:** 5

## 1. Overview

### 1.1 Purpose

The Timing module provides high-resolution performance measurement infrastructure for the Convexfeld optimizer. This module enables precise tracking of wall-clock time spent in various solver phases, supports timeout enforcement, facilitates performance profiling, and drives adaptive algorithm decisions such as basis refactorization triggering. The module abstracts platform-specific timing APIs and provides consistent microsecond-precision measurements across all optimization components.

The module serves as the instrumentation layer for performance analysis, progress reporting, time limit enforcement, and algorithmic heuristics that depend on elapsed time or computational work metrics.

### 1.2 Responsibilities

This module is responsible for:

- Recording high-resolution start and end timestamps for timed sections
- Computing elapsed time from timestamp pairs with microsecond precision
- Accumulating timing statistics across multiple invocations (totals, counts, averages)
- Tracking computational work performed during simplex pivots for refactorization decisions
- Providing category-based timing breakdowns (pricing, pivot, refactor, etc.)
- Maintaining iteration rates and derived performance metrics
- Abstracting platform-specific high-resolution timer APIs

This module is NOT responsible for:

- Creating or managing timing structures (allocated by caller)
- Deciding when to start/stop timing (caller's responsibility)
- Enforcing time limits (uses timing data but doesn't enforce)
- Logging or displaying timing results (provides data for other modules)
- Thread synchronization for timing state (caller must provide if needed)

### 1.3 Design Philosophy

The module follows a lightweight instrumentation philosophy: timing overhead is minimized to enable frequent measurements without distorting results. Functions are simple and focused, with clear separation between timestamp recording (start/end), statistics accumulation (update), and work tracking (pivot). The design supports both immediate reporting (last elapsed time) and aggregate analysis (totals, averages).

Timing is category-based to allow detailed performance breakdowns without requiring separate timing structures for each operation type.

## 2. Public Interface

### 2.1 Exported Functions

| Function | Purpose | Typical Caller |
|----------|---------|----------------|
| cxf_timing_start | Record start timestamp for timed section | Simplex, Basis, Pricing |
| cxf_timing_end | Record end timestamp and accumulate | Simplex, Basis, Pricing |
| cxf_timing_update | Update timing statistics for category | Solver core |
| cxf_timing_pivot | Record pivot work and timing | Simplex pivot |
| cxf_get_timestamp | Get current high-resolution timestamp | Timeout checking, solve tracking |

### 2.2 Exported Types

| Type | Purpose |
|------|---------|
| TimingState | Structure holding timestamps, totals, counts, averages |
| (Embedded in SolverState, not separately allocated) | - |

### 2.3 Exported Constants

| Constant | Value | Purpose |
|----------|-------|---------|
| CATEGORY_TOTAL | 0 | Overall solve timing category |
| CATEGORY_PRICING | 1 | Pricing operation category |
| CATEGORY_PIVOT | 2 | Pivot operation category |
| CATEGORY_REFACTOR | 3 | Basis refactorization category |
| CATEGORY_FTRAN | 4 | Forward transformation category |
| CATEGORY_BTRAN | 5 | Backward transformation category |
| MAX_SECTIONS | 6-8 | Maximum number of timing categories |

## 3. Internal Functions

### 3.1 Private Functions

All five functions are public-facing; no purely internal helper functions exist in this module.

### 3.2 Helper Functions

| Function | Purpose | Used By |
|----------|---------|---------|
| QueryPerformanceCounter | Get high-resolution timestamp (Windows) | All timing functions |
| QueryPerformanceFrequency | Get timer frequency (Windows) | Initialization |

## 4. State Management

### 4.1 Module State

| State Element | Type | Lifetime | Thread Safety |
|--------------|------|----------|---------------|
| TimingState.startCounter | int64_t | Per measurement | Not thread-safe |
| TimingState.endCounter | int64_t | Per measurement | Not thread-safe |
| TimingState.frequency | double | Persistent | Read-only after init |
| TimingState.sectionTotal[N] | double | Accumulated | Not thread-safe |
| TimingState.operationCount[N] | int | Accumulated | Not thread-safe |
| TimingState.averageTime[N] | double | Derived | Not thread-safe |
| TimingState.lastElapsed[N] | double | Transient | Not thread-safe |
| TimingState.iterationRate | double | Derived | Not thread-safe |
| work_counter | double* | External state | Not thread-safe |

### 4.2 State Lifecycle

```
TIMING_STATE_CREATED
    ↓
Frequency initialized (QueryPerformanceFrequency)
    ↓
All totals/counts zeroed
    ↓
READY_FOR_MEASUREMENT
    ↓
[Timing loop for optimization]
    ↓
cxf_timing_start → records startCounter
    ↓
[Operation executes]
    ↓
cxf_timing_end → records endCounter, computes elapsed
    ↓
cxf_timing_update → accumulates statistics for category
    ↓
[Repeat for each operation]
    ↓
FINAL_STATISTICS_AVAILABLE
    ↓
Totals, averages, rates can be queried
    ↓
CLEANUP (part of SolverState destruction)
```

### 4.3 State Invariants

At all times, the following must be true:

- frequency > 0 (timer frequency is valid)
- For all categories i: operationCount[i] >= 0
- For all categories i: sectionTotal[i] >= 0
- If operationCount[i] > 0, then averageTime[i] = sectionTotal[i] / operationCount[i]
- startCounter <= endCounter for valid measurements (monotonic time)
- elapsedSeconds >= 0 (non-negative time intervals)
- iterationRate >= 0 (non-negative rate)

## 5. Dependencies

### 5.1 Required Modules

| Module | What We Use | Why |
|--------|-------------|-----|
| (None - standalone module) | - | - |

### 5.2 Initialization Order

This module must be initialized:
- **After:** (No dependencies)
- **Before:** Simplex, Basis, Pricing (consumers of timing services)

### 5.3 External Dependencies

| Dependency | Type | Purpose |
|------------|------|---------|
| QueryPerformanceCounter | Windows API | Get high-resolution timestamp |
| QueryPerformanceFrequency | Windows API | Get timer frequency (ticks/second) |
| clock_gettime (Linux) | POSIX API | Alternative timing source |
| mach_absolute_time (macOS) | macOS API | Alternative timing source |

## 6. Dependents

### 6.1 Modules That Depend on Us

| Module | What They Use | Stability |
|--------|---------------|-----------|
| Simplex | timing_start/end/update for iteration tracking | Critical |
| Basis | timing_start/end for refactorization profiling | Critical |
| Pricing | timing_start/end for pricing profiling | Critical |
| Solver Core | cxf_get_timestamp for timeout checking | Critical |
| Callbacks | cxf_get_timestamp for callback timing | Stable |
| Logging | Timing statistics for progress reports | Stable |

### 6.2 API Stability

The following interfaces are stable and must not change:

- cxf_timing_start/end paired usage pattern
- cxf_get_timestamp return value (double seconds)
- TimingState structure layout (offset dependencies)
- Category indexing scheme (0-7)

## 7. Invariants

### 7.1 Module Invariants

What this module guarantees:

- Timestamps are monotonically increasing within a system boot
- Elapsed time calculations are accurate to microsecond precision
- Accumulated totals never decrease (only increase)
- Operation counts never decrease (only increase)
- Derived metrics (average, rate) are consistent with totals and counts
- Zero point (epoch) is consistent for duration of process

### 7.2 Required Invariants

What this module requires from others:

- TimingState structure must be initialized with valid frequency
- Caller must pair timing_start with timing_end
- Caller must not modify timing state concurrently from multiple threads
- Caller must provide exclusive access for timing_update calls

## 8. Error Handling

### 8.1 Error Detection

How errors are detected in this module:

| Error Type | Detection Method |
|------------|------------------|
| NULL timing pointer | Explicit check (some functions) |
| Invalid category | Range check (timing_update) |
| Zero frequency | Would cause division by zero |
| Uninitialized state | No explicit check (caller error) |

### 8.2 Error Propagation

How errors flow through this module:

```
NULL pointer in timing_start
    → Undefined behavior (no check in hot path)

Invalid category in timing_update
    → Validate and return early (no-op)
    → No error code returned

Performance counter failure (extremely rare)
    → Would store zero timestamp
    → Elapsed time calculation would be incorrect
```

### 8.3 Recovery Strategies

| Error Type | Recovery |
|------------|----------|
| NULL pointer | Undefined behavior (caller error) |
| Invalid category | Silent no-op (defensive programming) |
| Counter failure | No recovery (catastrophic system failure) |
| Frequency zero | Division by zero (undefined behavior) |

## 9. Thread Safety

### 9.1 Concurrency Model

Timing functions are NOT thread-safe by default. Each solver thread should maintain its own TimingState structure to avoid race conditions. The stateless cxf_get_timestamp function IS thread-safe.

**Per-thread timing:** Safe - each thread has its own TimingState
**Shared timing:** Unsafe - requires external synchronization (mutex)
**Timestamp queries:** Safe - stateless, read-only OS queries

### 9.2 Synchronization Primitives

| Primitive | Protects | Granularity |
|-----------|----------|-------------|
| (None internal) | - | - |
| (External lock if shared) | TimingState | Caller responsibility |

### 9.3 Thread Safety Guarantees

- cxf_timing_start: NOT thread-safe (modifies shared state)
- cxf_timing_end: NOT thread-safe (modifies shared state)
- cxf_timing_update: NOT thread-safe (modifies shared state)
- cxf_timing_pivot: NOT thread-safe (modifies shared state)
- cxf_get_timestamp: Thread-safe (stateless, read-only)

### 9.4 Known Race Conditions

**Concurrent timing_end calls:**
- Thread A: calls timing_end, writes endCounter
- Thread B: calls timing_end, writes endCounter
- Thread A: reads endCounter for elapsed calculation
- Result: Thread A may use Thread B's timestamp (incorrect elapsed time)

Mitigation: Each thread must maintain its own TimingState.

**Concurrent access to work counter:**
- Thread A: reads work_counter
- Thread B: increments work_counter
- Thread A: writes incremented value
- Result: Lost update from Thread B

Mitigation: Use atomic operations or external lock for work_counter.

## 10. Performance Characteristics

### 10.1 Complexity Summary

| Operation | Time | Space |
|-----------|------|-------|
| cxf_timing_start | O(1) | O(1) |
| cxf_timing_end | O(1) | O(1) |
| cxf_timing_update | O(1) | O(1) |
| cxf_timing_pivot | O(1) | O(1) |
| cxf_get_timestamp | O(1) | O(1) |

All operations are constant time with fixed memory usage.

### 10.2 Hot Paths

Performance-critical functions called frequently:

1. **cxf_timing_start/end** - Called every simplex iteration
   - Overhead per call: 20-50 nanoseconds (QueryPerformanceCounter)
   - Typical iteration: 1-100 milliseconds
   - Overhead percentage: <0.01% (negligible)

2. **cxf_get_timestamp** - Called for timeout checking in tight loops
   - Overhead per call: 20-50 nanoseconds
   - Can be called thousands of times per second without impact

3. **cxf_timing_pivot** - Called once per pivot
   - Overhead: 10-20 nanoseconds (arithmetic only)
   - Negligible compared to pivot cost (microseconds to milliseconds)

Cold paths (called infrequently):
- cxf_timing_update: Once per iteration or phase, not time-critical

### 10.3 Memory Usage

Minimal memory footprint:

**Per TimingState structure:**
- startCounter: 8 bytes
- endCounter: 8 bytes
- frequency: 8 bytes
- elapsedSeconds: 8 bytes
- sectionTotal[6-8]: 48-64 bytes
- operationCount[6-8]: 24-32 bytes
- averageTime[6-8]: 48-64 bytes
- lastElapsed[6-8]: 48-64 bytes
- iterationRate: 8 bytes
- currentSection: 4 bytes

**Total: ~160-256 bytes per timing structure**

Typically one per solver state, so memory impact is negligible even for thousands of concurrent solves.

## 11. Function Index

Complete list of functions in this module with links to individual specs:

### Public Functions

1. [cxf_timing_start](../functions/timing/cxf_timing_start.md) - Record start timestamp
2. [cxf_timing_end](../functions/timing/cxf_timing_end.md) - Record end timestamp and accumulate
3. [cxf_timing_update](../functions/timing/cxf_timing_update.md) - Update category statistics
4. [cxf_timing_pivot](../functions/timing/cxf_timing_pivot.md) - Record pivot work and timing
5. [cxf_get_timestamp](../functions/timing/cxf_get_timestamp.md) - Get current timestamp

## 12. Design Decisions

### 12.1 Key Design Choices

| Decision | Rationale | Alternatives Considered |
|----------|-----------|------------------------|
| Category-based timing | Detailed breakdowns without multiple structures | Per-operation structures (more memory) |
| Paired start/end functions | Flexible timing of arbitrary code sections | Automatic scope-based timing (less flexible) |
| Double precision for times | Sufficient range and precision for all uses | Integer microseconds (overflow issues) |
| Lazy initialization | Minimal overhead for unused timing | Eager initialization (simpler) |
| Work units separate from time | Platform-independent algorithmic metrics | Wall-clock time only (inconsistent) |
| Direct array indexing | Fast access to category statistics | Hash table (slower, overkill) |

### 12.2 Known Limitations

- Windows-specific implementation (requires porting for cross-platform)
- No automatic thread-local timing (caller must manage)
- No hierarchical timing (parent/child relationships)
- No percentile tracking (only totals and averages)
- Elapsed time may lose precision after millions of measurements (floating-point accumulation)
- No protection against clock adjustments (uses monotonic clock)

### 12.3 Future Improvements

- Cross-platform abstraction layer (POSIX, C++11 chrono)
- Thread-local timing state with automatic aggregation
- Hierarchical timing categories (nested sections)
- Percentile tracking (min, max, median) in addition to average
- Kahan summation for long-running accumulations
- Automatic detection and reporting of timing overhead
- Lock-free atomic operations for shared work counters

## 13. References

- Windows API Documentation: QueryPerformanceCounter, QueryPerformanceFrequency
- POSIX: clock_gettime specification (CLOCK_MONOTONIC)
- Intel Architecture Manual: Time Stamp Counter (TSC)
- IEEE 754: Double Precision Floating Point
- Knuth, D. E. (1997). *The Art of Computer Programming, Vol 2*. Section on floating-point accuracy.

## 14. Validation Checklist

Before finalizing this spec:

- [x] All public functions documented
- [x] All dependencies identified
- [x] Thread safety analyzed
- [x] Error handling complete
- [x] No implementation details leaked
- [x] Module boundaries clear
- [x] Performance characteristics documented
- [x] State lifecycle defined
- [x] Invariants specified
- [x] Category system explained

---

*Date: 2026-01-25*
*Reviewed by: Pending*

---
*Convexfeld LP Solver Specification*
*Based on published optimization literature*
