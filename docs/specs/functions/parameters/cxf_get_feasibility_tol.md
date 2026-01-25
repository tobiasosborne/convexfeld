# cxf_get_feasibility_tol

**Module:** Parameters
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Internal helper function that retrieves the primal feasibility tolerance parameter from a Convexfeld environment. This tolerance defines the threshold for determining when a solution satisfies constraints within acceptable numerical precision. The function provides a convenient typed wrapper around the general parameter retrieval mechanism, used extensively throughout the simplex and other LP solving algorithms.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| env | CxfEnv* | Environment containing the tolerance setting | Valid environment pointer | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | double | Primal feasibility tolerance value, default 1e-6 on error |

### 2.3 Side Effects

None. This is a read-only query operation. If the underlying parameter retrieval fails, the function returns a hardcoded default value without recording errors.

## 3. Contract

### 3.1 Preconditions

- [ ] Environment pointer should be valid (but function handles null gracefully)
- [ ] Environment should have parameter table initialized (function handles failure)

### 3.2 Postconditions

- [ ] Return value is a valid positive double in range [1e-9, 1e-2]
- [ ] Return value is exactly the current FeasibilityTol parameter value, or 1e-6 if retrieval fails
- [ ] No environment state is modified

### 3.3 Invariants

- [ ] Environment structure is not modified
- [ ] Parameter values remain unchanged
- [ ] Function is idempotent (multiple calls return same value)

## 4. Algorithm

### 4.1 Overview

This function serves as a typed convenience wrapper that retrieves the "FeasibilityTol" parameter using the general double parameter retrieval mechanism. If retrieval fails for any reason (invalid environment, parameter not found, etc.), the function returns the standard default value rather than propagating errors. This design simplifies error handling in inner loops of numerical algorithms where error checking overhead would be excessive.

### 4.2 Detailed Steps

1. Call the general double parameter retrieval function with parameter name "FeasibilityTol"
2. Check the error code returned from the retrieval
3. If successful (error code 0), return the retrieved tolerance value
4. If failed (non-zero error code), return the default value 1e-6
5. Do not record errors or update environment state on failure

### 4.3 Alternative Implementation Strategies

**Strategy A: Parameter lookup wrapper (most likely)**
```
value ← get_double_parameter(env, "FeasibilityTol")
if retrieval_succeeded:
    return value
else:
    return 1e-6 (default)
```

**Strategy B: Direct offset access (optimization)**
```
if env is valid:
    return *(double*)(env + PARAM_STORAGE_BASE + FEASTOL_OFFSET)
else:
    return 1e-6
```

**Strategy C: Cached in solver state**
```
Read from solver state structure where value was cached during initialization
Avoids repeated parameter table lookups in inner loops
```

The actual implementation likely uses Strategy A, with possible optimization to Strategy B or C in performance-critical code paths.

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) - if using direct offset access or cached value
- **Average case:** O(log n) - if using parameter table lookup
- **Worst case:** O(n) - if parameter table uses linear search

Where:
- n = number of parameters in parameter table (~100-200)

### 5.2 Space Complexity

- **Auxiliary space:** O(1) - no allocations or significant stack usage
- **Total space:** O(1) - purely a query operation

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| Null environment | N/A | Returns default value 1e-6, no error propagated |
| Invalid environment | N/A | Returns default value 1e-6, no error propagated |
| Parameter retrieval fails | N/A | Returns default value 1e-6, no error propagated |

### 6.2 Error Behavior

This function uses defensive programming with silent failure recovery:
- Does not throw exceptions or abort
- Does not record error messages
- Returns a reasonable default value on any failure
- Allows calling code to continue without error checking

This design is appropriate for an internal helper function called in performance-critical inner loops where error handling overhead is unacceptable and using the default value is a safe fallback.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Null environment | NULL | Return 1e-6 (default) |
| Uninitialized environment | Invalid env | Return 1e-6 (default) |
| Modified tolerance | env with custom value | Return the custom value |
| Default tolerance | env with default params | Return 1e-6 (default) |
| First call after creation | Fresh environment | Return 1e-6 (default) |

## 8. Thread Safety

**Thread-safe:** Yes

The function performs only read operations on the environment structure. Multiple threads can safely call this function concurrently on the same environment as long as:
- No other thread is modifying the FeasibilityTol parameter value
- The environment structure is fully initialized

**Synchronization required:** None for read-only access. Caller must synchronize if concurrent parameter modifications are possible.

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_getdblparam | Parameters | Retrieves the FeasibilityTol parameter value |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_simplex_step2 | Simplex Core | Checking bound violations in basis update |
| cxf_simplex_step3 | Simplex Core | Checking constraint satisfaction in feasibility test |
| cxf_ratio_test | Simplex Core | Determining feasible step length in pivot |
| cxf_check_feasibility | Validation | Verifying solution satisfies all constraints |
| Barrier primal feasibility check | Barrier | Convergence criteria evaluation |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_get_optimality_tol | Parallel function - retrieves dual feasibility tolerance |
| cxf_get_infinity | Parallel function - retrieves infinity constant |
| cxf_getdblparam | Underlying implementation - general parameter retrieval |
| cxf_get_int_feas_tol | Parallel function - retrieves integer feasibility tolerance |

## 11. Design Notes

### 11.1 Design Rationale

The function exists as a convenience wrapper for several reasons:
1. **Type safety**: Returns double directly instead of through output parameter
2. **Simplicity**: One-line calls instead of error-checking boilerplate
3. **Performance**: Can be optimized to direct memory access if offset is known
4. **Robustness**: Silent fallback to default avoids error propagation in inner loops
5. **Readability**: `tol = get_feasibility_tol(env)` is clearer than parameter name strings

### 11.2 Performance Considerations

This function may be called thousands or millions of times during solve:
- Inner loop of simplex iteration checks feasibility repeatedly
- Called for every constraint and bound check
- Consider caching the tolerance value in solver state structures
- Consider inlining or direct offset access for critical paths
- Profile to determine if parameter lookup overhead is significant

Optimization strategies:
- Cache in SolverState during initialization
- Use direct memory access if FeasibilityTol offset is fixed
- Compiler may inline this function automatically

### 11.3 Future Considerations

- Could be implemented as inline function or macro for zero-cost abstraction
- Might benefit from thread-local caching in multi-threaded environments
- Consider return value optimization (RVO) for efficiency
- May add debug mode that validates tolerance is in valid range

## 12. References

- Simplex method literature on constraint satisfaction tolerances
- Numerical computing best practices for floating-point comparison
- LP_SOLVE_FLOW.md: Usage in simplex iteration steps

## 13. Validation Checklist

- [x] No code copied from implementation
- [x] Algorithm description is implementation-agnostic
- [x] All parameters documented
- [x] All error conditions listed
- [x] Complexity analysis complete
- [x] Edge cases identified
- [x] A competent developer could implement from this spec alone

---


---
*Convexfeld LP Solver Specification*
*Based on published optimization literature*
