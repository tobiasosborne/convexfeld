# cxf_get_optimality_tol

**Module:** Parameters
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Internal helper function that retrieves the dual feasibility (optimality) tolerance parameter from a Convexfeld environment. This tolerance defines the threshold for determining when reduced costs are considered effectively zero, indicating an optimal solution has been reached. The function provides a typed wrapper around the general parameter retrieval mechanism, used extensively in simplex pricing and optimality verification algorithms.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| env | CxfEnv* | Environment containing the tolerance setting | Valid environment pointer | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | double | Dual feasibility tolerance value, default 1e-6 on error |

### 2.3 Side Effects

None. This is a read-only query operation. If the underlying parameter retrieval fails, the function returns a hardcoded default value without recording errors.

## 3. Contract

### 3.1 Preconditions

- [ ] Environment pointer should be valid (but function handles null gracefully)
- [ ] Environment should have parameter table initialized (function handles failure)

### 3.2 Postconditions

- [ ] Return value is a valid positive double in range [1e-9, 1e-2]
- [ ] Return value is exactly the current OptimalityTol parameter value, or 1e-6 if retrieval fails
- [ ] No environment state is modified

### 3.3 Invariants

- [ ] Environment structure is not modified
- [ ] Parameter values remain unchanged
- [ ] Function is idempotent (multiple calls return same value)

## 4. Algorithm

### 4.1 Overview

This function serves as a typed convenience wrapper that retrieves the "OptimalityTol" parameter using the general double parameter retrieval mechanism. The OptimalityTol parameter controls dual feasibility checking in the simplex method - when all reduced costs are within this tolerance of zero (with appropriate signs), the current solution is considered optimal. If retrieval fails, the function returns the standard default value to allow algorithms to continue with reasonable behavior.

The tolerance is critical for termination of simplex algorithms: too small may cause infinite cycling or numerical instability, too large may terminate prematurely at a suboptimal solution.

### 4.2 Detailed Steps

1. Call the general double parameter retrieval function with parameter name "OptimalityTol"
2. Check the error code returned from the retrieval operation
3. If successful (error code 0), return the retrieved tolerance value
4. If failed (non-zero error code), return the standard default value 1e-6
5. Do not record errors or update environment state on failure

### 4.3 Alternative Implementation Strategies

**Strategy A: Parameter lookup wrapper (most likely)**
```
value ← get_double_parameter(env, "OptimalityTol")
if retrieval_succeeded:
    return value
else:
    return 1e-6 (default)
```

**Strategy B: Direct offset access (optimization)**
```
if env is valid:
    return *(double*)(env + PARAM_STORAGE_BASE + OPTTOL_OFFSET)
else:
    return 1e-6
```

**Strategy C: Cached in solver state**
```
Read from solver state structure where value was cached during initialization
Avoids repeated parameter table lookups in pricing inner loop
```

The actual implementation likely uses Strategy A with possible optimization to Strategy C in the pricing routine, which may call this thousands of times per iteration.

### 4.4 Mathematical Foundation

The optimality tolerance relates to the Karush-Kuhn-Tucker (KKT) conditions for linear programming:

For a minimization problem with reduced costs r_j:
- If x_j is at lower bound: require r_j ≥ -ε (within tolerance of non-negative)
- If x_j is at upper bound: require r_j ≤ +ε (within tolerance of non-positive)
- If x_j is between bounds: require |r_j| ≤ ε (within tolerance of zero)

Where ε = OptimalityTol. These conditions ensure dual feasibility, which combined with primal feasibility guarantees optimality.

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
- Allows calling code to continue without explicit error checking

This design is appropriate for performance-critical inner loops where:
- Error handling overhead is unacceptable
- Using the default tolerance is a safe and reasonable fallback
- The calling context (simplex pricing) will detect issues through other means

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Null environment | NULL | Return 1e-6 (default) |
| Uninitialized environment | Invalid env | Return 1e-6 (default) |
| Modified tolerance | env with custom value | Return the custom value |
| Default tolerance | env with default params | Return 1e-6 (default) |
| Very tight tolerance | env with 1e-9 | Return 1e-9 (may cause slow convergence) |
| Loose tolerance | env with 1e-3 | Return 1e-3 (may terminate early) |

## 8. Thread Safety

**Thread-safe:** Yes

The function performs only read operations on the environment structure. Multiple threads can safely call this function concurrently on the same environment as long as:
- No other thread is modifying the OptimalityTol parameter value
- The environment structure is fully initialized

**Synchronization required:** None for read-only access. Caller must synchronize if concurrent parameter modifications are possible.

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_getdblparam | Parameters | Retrieves the OptimalityTol parameter value |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_simplex_step3 | Simplex Core | Checking reduced costs for optimality |
| cxf_pricing_dantzig | Pricing | Selecting entering variable in primal simplex |
| cxf_pricing_steepest | Pricing | Steepest edge pricing for variable selection |
| cxf_dual_simplex_check | Simplex Core | Dual feasibility verification |
| cxf_check_optimality | Validation | Verifying KKT conditions satisfied |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_get_feasibility_tol | Parallel function - retrieves primal feasibility tolerance |
| cxf_get_infinity | Parallel function - retrieves infinity constant |
| cxf_getdblparam | Underlying implementation - general parameter retrieval |
| cxf_get_markowicz_tol | Parallel function - retrieves pivoting threshold tolerance |

## 11. Design Notes

### 11.1 Design Rationale

The function exists as a convenience wrapper for several reasons:
1. **Type safety**: Returns double directly instead of through output parameter
2. **Simplicity**: One-line calls in pricing loop instead of error-checking boilerplate
3. **Performance**: Can be optimized to direct memory access or caching
4. **Robustness**: Silent fallback to default avoids error propagation in critical paths
5. **Readability**: `tol = get_optimality_tol(env)` is self-documenting
6. **Maintainability**: Centralizes tolerance access pattern

### 11.2 Performance Considerations

This function is called in the innermost loop of simplex pricing:
- Pricing evaluates all non-basic variables to find entering variable
- May be called hundreds of times per simplex iteration
- For large problems with thousands of variables, called millions of times
- Parameter lookup overhead can be measurable in profiles

Optimization strategies:
- **Critical**: Cache in SolverState during initialization
- Use direct memory access if OptimalityTol offset is fixed and known
- Compiler inlining eliminates function call overhead
- Consider passing tolerance as parameter to pricing functions

Performance measurements should verify:
- Is this function called in inner loops?
- What percentage of solve time is spent in parameter access?
- Does caching improve overall performance measurably?

### 11.3 Future Considerations

- Could be implemented as inline function or macro for zero-cost abstraction
- Might benefit from thread-local caching in parallel simplex implementations
- Consider validating tolerance is in valid range in debug builds
- May add alternative precision modes (quad precision) in future

## 12. References

- Dantzig, G. (1963). Linear Programming and Extensions - KKT conditions
- Chvátal, V. (1983). Linear Programming - Chapter on simplex termination criteria
- LP_SOLVE_FLOW.md: Usage in optimality check (Step 3)
- Numerical analysis literature on floating-point comparison tolerances

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
