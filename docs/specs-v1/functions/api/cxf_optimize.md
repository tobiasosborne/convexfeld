# cxf_optimize

**Module:** API - Optimization
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Initiates the optimization process for a model, dispatching to the appropriate solver algorithm based on model type (LP, QP, QCP, MIP). This is the main entry point for solving optimization problems. The function handles pre-optimization setup, logging, callback management, result file writing, and cleanup, returning when optimization completes or is terminated.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| model | CxfModel* | Loaded optimization model | Valid model pointer | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | Zero on success, non-zero error code on failure |

### 2.3 Side Effects

- Sets model's modificationBlocked flag to prevent concurrent modifications
- Populates solution attributes (objective value, variable values, constraint slacks, etc.)
- May write result files to disk if ResultFile parameter is set
- Updates environment logging state and message buffers
- Modifies model's optimizing flag in environment
- Acquires and releases solve lock for thread safety
- May trigger callbacks at various stages
- Logs version, CPU, and optimization statistics to console/log file based on verbosity settings

## 3. Contract

### 3.1 Preconditions

- [ ] Model pointer must be valid (passes cxf_checkmodel)
- [ ] Model environment must be initialized
- [ ] Any pending model modifications must be processable
- [ ] If log file is configured, file path must be writable
- [ ] Sufficient memory available for optimization data structures

### 3.2 Postconditions

- [ ] Model Status attribute is set to optimization result code
- [ ] If optimization succeeds, solution attributes contain valid values
- [ ] modificationBlocked flag is cleared
- [ ] Solve lock is released
- [ ] optimizing flag is cleared in environment
- [ ] Error messages are logged if applicable
- [ ] If ResultFile parameter is set and status is optimal/suboptimal, file is written
- [ ] Session reference counter is reset
- [ ] Update time accumulator is reset

### 3.3 Invariants

- [ ] Model structure validity magic remains unchanged
- [ ] Model dimensions (number of variables, constraints) remain unchanged
- [ ] Environment pointer remains valid
- [ ] Matrix data structure pointers remain valid (content may change)

## 4. Algorithm

### 4.1 Overview

The function orchestrates a multi-stage optimization workflow. First, it validates the model and acquires a thread-safe solve lock. It initializes optimization state flags and message buffers, then conditionally prints version and CPU information based on verbosity settings.

The core optimization dispatch occurs through an internal function that handles presolve, algorithm selection (simplex, barrier, branch-and-cut), and iterative solving. After optimization completes, the function logs callback statistics if applicable and handles any out-of-memory conditions specially.

If a ResultFile parameter is configured and the optimization status is optimal or suboptimal, the function automatically writes the solution to the specified file format. For ILP-format files, it may compute infeasibility information first. Throughout execution, error codes are logged and accumulated, with comprehensive cleanup ensuring locks are released and state is restored regardless of success or failure.

### 4.2 Detailed Steps

1. **Model Validation**: Invoke model validation check to ensure pointer validity and internal consistency
2. **Lock Acquisition**: Acquire environment-level solve lock to prevent concurrent optimization attempts
3. **State Initialization**: Set environment optimizing flag to 1, initialize message buffer pointers, clear temporary counters, copy session identifier
4. **Modification Blocking**: Set model's modificationBlocked flag to prevent structural changes during optimization
5. **Verbosity Check**: If OutputFlag parameter is positive OR (zero with active session), proceed with information logging
6. **Version Logging**: Log full version string including major/minor/patch numbers, git revision, and platform
7. **CPU Information**: If verbosity level is less than 1, log CPU model string, instruction set extensions, physical core count, logical processor count, and configured thread count
8. **Log File Setup**: If LogFile parameter is non-empty and no active session, register an internal log callback function to duplicate output to file
9. **Pre-Optimize Callback**: Trigger user-registered pre-optimization callbacks if any exist
10. **Core Optimization**: Invoke internal optimization dispatcher which handles presolve, solver selection, and iterative algorithm execution
11. **Callback Statistics**: If session is active and callback state exists, log total callback invocation count and cumulative callback execution time
12. **Post-Optimize Callback**: Trigger user-registered post-optimization callbacks
13. **Error Handling**: If error code indicates out-of-memory, set specific error message about memory exhaustion
14. **Error Logging**: Log the final error code to the model's error message buffer
15. **Result File Writing**: If error code is zero and ResultFile parameter is set, query Status attribute; if optimal (3) or suboptimal (4), write solution to specified file format; for ILP formats, potentially compute IIS first
16. **Cleanup**: Clear modificationBlocked flag, reset session reference to zero, release solve lock, clear optimizing flag
17. **Return**: Return the final error code

### 4.3 Mathematical Foundation

Not applicable - this is an orchestration function rather than an algorithm implementation. The actual optimization algorithms (simplex, barrier, branch-and-cut) are invoked through internal dispatch.

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) for validation failures before optimization begins
- **Average case:** O(optimization algorithm complexity) - depends on problem type and size
- **Worst case:** O(optimization algorithm complexity) - unbounded for difficult problems

Where:
- Optimization algorithm complexity varies by problem class: LP (polynomial to exponential), MIP (exponential), QP/QCP (polynomial to exponential)
- Setup and logging overhead is O(1)
- Result file writing is O(n + m + nnz) where n = variables, m = constraints, nnz = nonzeros

### 5.2 Space Complexity

- **Auxiliary space:** O(1) for function-local state (lock buffer, flags, pointers)
- **Total space:** O(optimization solver state) - depends on algorithm chosen

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| Model validation fails | Various | Invalid model pointer or corrupted structure |
| Lock acquisition fails | Unknown | Thread synchronization failure |
| Out of memory | 1001 | Insufficient memory for optimization data structures |
| Log callback registration fails | Unknown | Callback system error |
| Optimization algorithm error | Various | Numerical issues, infeasibility, unboundedness, etc. |
| Result file write fails | Unknown | I/O error or invalid file path |

### 6.2 Error Behavior

On any error, the function performs comprehensive cleanup: modificationBlocked flag is always cleared, solve lock is always released if acquired, optimizing flag is always reset, and error messages are logged to both the model's error buffer and console/log file based on verbosity. Partial solution data may be available even on error (e.g., best incumbent for interrupted MIP). The function guarantees that the model remains in a consistent state for subsequent API calls.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Empty model | Model with zero variables | May return immediately with trivial solution |
| Already optimal | Re-optimizing solved model | May skip solver, return cached solution |
| Infeasible model | Conflicting constraints | Status set to INFEASIBLE, no solution populated |
| Unbounded model | Objective can improve without limit | Status set to UNBOUNDED, ray available |
| Terminated optimization | User calls cxf_terminate during solve | Status set to INTERRUPTED, partial solution may exist |
| Very large model | Billions of nonzeros | May fail with SIZE_LIMIT_EXCEEDED or OUT_OF_MEMORY |
| Concurrent calls | Multiple threads call on same model | Lock prevents concurrent execution; second call blocks |

## 8. Thread Safety

**Thread-safe:** Conditionally

The function uses an environment-level solve lock to serialize optimization attempts on the same model. Multiple threads may safely call cxf_optimize concurrently on *different* models. Calling on the same model from multiple threads is safe (second thread will block) but not useful. The modificationBlocked flag prevents concurrent modification during optimization. Callbacks may execute on internal solver threads; user must ensure callback code is thread-safe.

**Synchronization required:** Internal - solve lock acquired/released automatically

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_checkmodel | Validation | Validate model pointer and structure |
| cxf_acquire_solve_lock | Threading | Acquire thread-safe optimization lock |
| cxf_gitrevision | Version | Get git revision string for logging |
| cxf_platform | Version | Get platform identifier string |
| cxf_platformext | Version | Get platform extension string |
| cxf_msg | Logging | Formatted message output |
| cxf_get_threads | Parameters | Query configured thread count |
| cxf_get_physical_cores | System | Query physical CPU core count |
| cxf_get_logical_processors | System | Query logical processor count |
| cxf_set_thread_count | Parameters | Set actual thread count to use |
| cxf_flush_log | Logging | Flush log buffer to output |
| cxf_register_log_callback | Callbacks | Register log file callback |
| cxf_pre_optimize_callback | Callbacks | Invoke pre-optimization callbacks |
| cxf_optimize_internal | Core Solver | Main optimization dispatcher |
| cxf_post_optimize_callback | Callbacks | Invoke post-optimization callbacks |
| cxf_error | Error | Set error message |
| cxf_errorlog | Error | Log error code |
| cxf_getintattr | Attributes | Query Status attribute |
| cxf_check_qp_label | Validation | Check QP model type |
| cxf_compute_IIS_internal | Analysis | Compute infeasibility information |
| cxf_compute_IIS_callback | Analysis | Compute IIS with callbacks |
| cxf_write | I/O | Write model/solution to file |
| cxf_cleanup_optimization | Cleanup | Post-optimization cleanup |
| cxf_release_solve_lock | Threading | Release optimization lock |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| User application | External | Primary optimization entry point |
| cxf_optimizeasync | Async API | Asynchronous optimization wrapper |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_optimize_internal | Internal implementation - core dispatcher |
| cxf_optimizeasync | Asynchronous variant - returns immediately |
| cxf_terminate | Counterpart - requests early termination |
| cxf_update | Prerequisite - processes pending modifications |
| cxf_getintattr | Follow-up - queries Status attribute |
| cxf_getdblattr | Follow-up - queries objective value |
| cxf_write | Related - writes solution to file |

## 11. Design Notes

### 11.1 Design Rationale

The function serves as a comprehensive orchestration layer that isolates the complex optimization engine from the public API. By handling all setup, logging, callbacks, and cleanup in one location, it ensures consistent behavior across all optimization invocations. The two-phase structure (outer shell for setup/cleanup, inner call for actual optimization) allows clean error handling with guaranteed resource cleanup.

The verbose logging controlled by OutputFlag provides visibility into solver behavior for debugging and performance analysis. Version and CPU information logging helps reproduce issues and understand performance characteristics. The automatic result file writing feature reduces boilerplate in user code and ensures solution is captured even if the program crashes later.

### 11.2 Performance Considerations

The function adds minimal overhead beyond the core optimization algorithm - typically less than 1% of total runtime. Logging is conditionally executed based on verbosity flags. The solve lock uses a lightweight synchronization primitive with low contention cost. Message buffer initialization is simple pointer assignment.

For very large models, the result file writing phase can add noticeable time (seconds to minutes). This can be disabled by leaving ResultFile parameter empty. The IIS computation for ILP files on infeasible models can be expensive and is skipped if callbacks are registered.

### 11.3 Future Considerations

Potential enhancements: support for progress callbacks during synchronous optimization, more granular logging control (per-component verbosity), streaming result file writing to reduce memory footprint, and automatic checkpoint/restart for long-running optimizations.

## 12. References

- Convexfeld Optimizer Reference Manual - Solving Models
- Convexfeld Optimizer Reference Manual - Callbacks
- Convexfeld Optimizer Reference Manual - Parameters

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
