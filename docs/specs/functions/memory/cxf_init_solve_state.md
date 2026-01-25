# cxf_init_solve_state

**Module:** Memory Management
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Initialize a solve state structure for optimization. SolveState is a lightweight control structure that wraps SolverState and tracks solve progress, manages limits (time, iterations), handles interrupts, and coordinates callbacks. Unlike the heavyweight SolverState allocation, this function performs simple field initialization on a caller-provided structure (typically stack-allocated).

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| solve | SolveState* | Pointer to SolveState to initialize | Valid SolveState pointer | Yes |
| state | SolverState* | Pointer to solver working state | Valid SolverState pointer or NULL | Yes |
| env | CxfEnv* | Environment with parameters | Valid CxfEnv pointer or NULL | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | Error code: 0 on success, CXF_ERR_NULL_ARGUMENT if solve is NULL |

### 2.3 Side Effects

- Initializes all fields in solve structure
- Sets validation magic number
- Captures current timestamp for solve timing
- Reads parameters from environment (time limit, iteration limit, callback state)
- Reads solve method from solver state

## 3. Contract

### 3.1 Preconditions

- [ ] solve must be a valid SolveState pointer (not NULL)
- [ ] solve has sufficient space for SolveState structure (~72 bytes)

### 3.2 Postconditions

- [ ] If successful: solve->magic is set to validation value (0x534f4c56 = "SOLV")
- [ ] If successful: solve->status is set to STATUS_LOADED
- [ ] If successful: solve->iterations is set to 0
- [ ] If successful: solve->phase is set to 0 (initial)
- [ ] If successful: solve->solverState points to state parameter
- [ ] If successful: solve->env points to env parameter
- [ ] If successful: solve->startTime set to current timestamp
- [ ] If successful: Time and iteration limits read from environment
- [ ] If successful: Callback data reference established
- [ ] If successful: Function returns 0 (CXF_OK)
- [ ] If failed: Returns error code, solve contents undefined

### 3.3 Invariants

- [ ] Magic number enables validation of initialized structure
- [ ] All pointers are either NULL or valid references

## 4. Algorithm

### 4.1 Overview

The function performs simple field-by-field initialization of a SolveState structure. It sets a validation magic number, initializes status and counters to zero, stores references to the provided SolverState and environment, captures the current timestamp for timing, reads configuration parameters (time limit, iteration limit, callback state) from the environment, and extracts the solve method from the solver state. The entire operation is non-allocating and very fast.

### 4.2 Detailed Steps

1. Validate solve pointer (return error if NULL)
2. Set validation magic number (0x534f4c56 = "SOLV" in ASCII)
3. Initialize status to STATUS_LOADED (value 1)
4. Zero iteration count
5. Set phase to 0 (initial/setup)
6. Store solverState pointer reference
7. Store env pointer reference
8. Capture current timestamp via timing function
9. If env is not NULL:
10. If env is NULL:
   - Set timeLimit to infinity (1e100)
   - Set iterLimit to max integer
   - Set callbackData to NULL
11. Clear interrupt flag (set to 0)
12. If state is not NULL:
13. If state is NULL:
   - Default method to 1 (dual simplex)
14. Clear control flags (set to 0)
15. Return CXF_OK (0)

### 4.3 Pseudocode (if needed)

```
FUNCTION cxf_init_solve_state(solve, state, env) → error_code
  IF solve = NULL THEN
    RETURN CXF_ERR_NULL_ARGUMENT
  END IF

  solve.magic ← 0x534f4c56  # "SOLV"
  solve.status ← STATUS_LOADED
  solve.iterations ← 0
  solve.phase ← 0
  solve.solverState ← state
  solve.env ← env
  solve.startTime ← get_current_timestamp()

  IF env ≠ NULL THEN
    solve.timeLimit ← env.parameters[TimeLimit]
    solve.iterLimit ← env.parameters[IterationLimit]
    solve.callbackData ← env.callbackState
  ELSE
    solve.timeLimit ← INFINITY
    solve.iterLimit ← MAX_INT
    solve.callbackData ← NULL
  END IF

  solve.interruptFlag ← 0

  IF state ≠ NULL THEN
    solve.method ← state.solveMode
  ELSE
    solve.method ← 1  # Default: dual simplex
  END IF

  solve.flags ← 0

  RETURN CXF_OK
END FUNCTION
```

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1)
- **Average case:** O(1)
- **Worst case:** O(1)

Where:
- All operations are simple field assignments
- Timestamp call is typically constant time
- No loops or allocations

### 5.2 Space Complexity

- **Auxiliary space:** O(1) - no allocation
- **Total space:** O(1) - operates on caller-provided structure

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| solve is NULL | CXF_ERR_NULL_ARGUMENT (0x2712) | Invalid solve pointer |

### 6.2 Error Behavior

Returns CXF_ERR_NULL_ARGUMENT if solve is NULL. NULL env or state pointers are handled gracefully with default values. Function cannot fail otherwise (no allocations, no system calls that can fail).

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| All valid | solve valid, state valid, env valid | Normal initialization, read all parameters |
| NULL state | solve valid, state=NULL, env valid | Use default method (dual simplex) |
| NULL env | solve valid, state valid, env=NULL | Use default limits (infinity, max int) |
| Both NULL | solve valid, state=NULL, env=NULL | Both defaults applied |
| NULL solve | solve=NULL, others valid | Return CXF_ERR_NULL_ARGUMENT immediately |

## 8. Thread Safety

**Thread-safe:** No

The function has no internal synchronization. Caller must ensure:
- Exclusive access to solve structure during initialization
- No concurrent access to same SolveState

Safe for concurrent calls on different SolveState structures.

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_get_timestamp | Timing | Capture current timestamp for solve timing |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_optimize | API | Initialize solve control before optimization |
| cxf_solve_lp | Solver | Set up solve state for LP solve |
| Internal solve functions | Various | Initialize control structure |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_cleanup_solve_state | Paired cleanup function |
| cxf_simplex_init | Allocates SolverState (heavyweight allocation) |
| cxf_get_timestamp | Called to capture start time |

## 11. Design Notes

### 11.1 Design Rationale

SolveState design separates concerns:
- **SolverState:** Algorithm-specific working data (large, heap-allocated, long-lived)
- **SolveState:** Algorithm-agnostic control (small, stack-allocated, short-lived)

This separation enables:
- Fast initialization (no allocation)
- Algorithm reuse (same SolverState, different SolveState instances)
- Clean interruption (modify SolveState without touching SolverState)
- Simple progress tracking

### 11.2 Performance Considerations

- Extremely fast: ~20-30 nanoseconds
- No memory allocation
- Single system call (timestamp)
- Negligible overhead for solve
- Can be called frequently without concern

### 11.3 Future Considerations

- Add validation of parameters (time limit >= 0, etc.)
- Support for checkpoint/restart (save/restore solve state)
- Additional statistics tracking (wall time vs CPU time)

## 12. References

- Optimization solver architecture patterns
- Stack vs heap allocation strategies

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
