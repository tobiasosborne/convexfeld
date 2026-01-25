# cxf_generate_seed

**Module:** Threading
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Generates a pseudo-random seed value for initializing random number generators used throughout the optimization process. The seed combines multiple sources of entropy (timestamp, process ID, thread ID) to ensure different values across different runs, processes, and threads while remaining deterministic given the same starting conditions. This supports both reproducible results (when user specifies a seed) and varied exploration (when using auto-generated seeds).

## 2. Signature

### 2.1 Inputs

None. This is a pure generation function with no parameters.

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | Pseudo-random seed value (always non-negative) |

### 2.3 Side Effects

None. This is a pure function that queries system state without modifying anything.

## 3. Contract

### 3.1 Preconditions

None. Function can be called at any time from any thread.

### 3.2 Postconditions

- Returns a non-negative 32-bit integer (range: 0 to 2^31-1)
- Different calls at different times produce different values (high probability)
- Same timestamp/process/thread produces same value (deterministic)

### 3.3 Invariants

- Return value is always non-negative
- No global state is modified
- No system state is modified

## 4. Algorithm

### 4.1 Overview

The function combines three sources of entropy using bitwise XOR operations and hash mixing:

1. **High-resolution timestamp:** Provides temporal uniqueness (different across time)
2. **Process ID:** Provides process uniqueness (different across concurrent processes)
3. **Thread ID:** Provides thread uniqueness (different across parallel threads)

These sources are mixed using a hash function (MurmurHash3 finalizer) to improve distribution quality. The final value has the sign bit cleared to ensure a non-negative result, which is the conventional range for random seeds in optimization solvers.

### 4.2 Detailed Steps

1. Query high-resolution performance counter to get current timestamp
2. Query current process ID
3. Query current thread ID
4. Mix high and low 32 bits of timestamp using XOR
5. XOR the result with process ID
6. XOR the result with thread ID
7. Apply hash mixing function to improve bit distribution
8. Clear sign bit to ensure non-negative result
9. Return the final seed value

### 4.3 Pseudocode

```
FUNCTION generate_seed():
    timestamp ← get_high_resolution_counter()
    process_id ← get_current_process_id()
    thread_id ← get_current_thread_id()

    # Mix timestamp (combine high and low 32 bits)
    seed ← (timestamp XOR (timestamp >> 32))

    # Mix in process and thread IDs
    seed ← seed XOR process_id
    seed ← seed XOR thread_id

    # Hash mixing for better distribution
    seed ← hash_mix(seed)

    # Ensure non-negative
    seed ← seed AND 0x7FFFFFFF

    RETURN seed
END FUNCTION

FUNCTION hash_mix(value):
    # MurmurHash3 finalizer for good avalanche
    value ← value XOR (value >> 16)
    value ← value × 0x85ebca6b
    value ← value XOR (value >> 13)
    value ← value × 0xc2b2ae35
    value ← value XOR (value >> 16)
    RETURN value
END FUNCTION
```

### 4.4 Mathematical Foundation

The hash mixing function is based on MurmurHash3's finalizer, which has proven avalanche properties:
- Changing 1 input bit affects approximately 50% of output bits
- Uniform distribution across output range
- Low collision rate for similar inputs

This improves the quality of seeds even when input sources (timestamp, IDs) have poor distribution.

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1)
- **Average case:** O(1)
- **Worst case:** O(1)

All operations are constant time: system queries (~20-50 ns each) and arithmetic operations (~5 ns each).

Where:
- System queries are OS-dependent but typically < 100 nanoseconds total

### 5.2 Space Complexity

- **Auxiliary space:** O(1)
- **Total space:** O(1)

No dynamic allocation, only local variables.

## 6. Error Handling

### 6.1 Error Conditions

None. Function cannot fail. System queries always succeed.

### 6.2 Error Behavior

Not applicable - function has no error conditions.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Called at same microsecond | Same timestamp | Different if different threads, otherwise may collide |
| First call in process | Initial state | Valid seed based on startup time |
| Concurrent calls | Multiple threads | Each gets different seed (different thread IDs) |
| Sequential calls | Same thread | Different seeds (timestamp changes) |
| Maximum timestamp | Counter overflow | Still produces valid seed (uses modulo arithmetic) |

## 8. Thread Safety

**Thread-safe:** Yes

Each thread queries its own thread ID, ensuring different seeds for concurrent calls. No shared state is accessed or modified.

**Synchronization required:** None

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| QueryPerformanceCounter | Windows API | Get high-resolution timestamp |
| GetCurrentProcessId | Windows API | Get current process ID |
| GetCurrentThreadId | Windows API | Get current thread ID |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| Simplex initialization | Optimization | Generate seed for anti-cycling perturbations |
| Random heuristics | MIP | Initialize random number generator for diving |
| Parameter initialization | API | When Seed parameter is -1 (auto) |
| Concurrent optimizer | MIP | Generate different seeds for parallel search threads |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_get_timestamp | Provides timestamp component (if separate function) |
| srand (C stdlib) | Consumes the seed produced by this function |
| Random number generator initialization | Uses seed for reproducibility |

## 11. Design Notes

### 11.1 Design Rationale

**Why combine multiple sources?**
- Timestamp alone: Same for concurrent calls in same microsecond
- Process ID alone: Same for all calls in same process
- Thread ID alone: Same for sequential calls in same thread
- Combined: Unique across time, processes, and threads

**Why hash mixing?**
- Raw XOR may have poor distribution (e.g., sequential timestamps)
- Hash function improves avalanche (small input changes → large output changes)
- Better statistical properties for random number generator seeding

**Why non-negative?**
- Convexfeld's Seed parameter range is 0 to 2^31-1
- Simplifies display and user specification
- Avoids confusion with negative values having special meaning

### 11.2 Performance Considerations

Total time: ~50-100 nanoseconds
- QueryPerformanceCounter: ~20-50 ns
- GetCurrentProcessId: ~10 ns
- GetCurrentThreadId: ~10 ns
- Hash mixing: ~10-20 ns (6 arithmetic operations)

This is fast enough for frequent calls, though seeds are typically generated only once per optimization run.

### 11.3 Future Considerations

Possible enhancements:
- Use hardware random number generator (RDRAND instruction) if available
- Add static counter for deterministic within-run sequence
- Cache physical core ID for NUMA-aware seeding
- Use cryptographic RNG for security-sensitive applications (unnecessary for optimization)

Current implementation balances speed, quality, and simplicity appropriately for typical optimization use cases.

## 12. References

- Random number generation: Knuth, The Art of Computer Programming, Vol. 2 (Seminumerical Algorithms)

## 13. Validation Checklist

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
