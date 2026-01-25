# cxf_get_infinity

**Module:** Parameters
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Internal helper function that retrieves or returns the Convexfeld infinity constant used throughout the solver to represent unbounded values. Convexfeld uses the finite constant 1e100 rather than IEEE 754 infinity to avoid arithmetic issues with NaN propagation and to enable meaningful comparisons. This value is used for unbounded variable bounds, infinite constraint limits, and unlimited solver parameters.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| (none) | void | No parameters in simplest implementation | N/A | N/A |
| env | CxfEnv* | Environment pointer (in alternative implementation) | Valid or null pointer | No |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | double | The infinity constant value (1e100) |

### 2.3 Side Effects

None. This is a pure read-only function that returns a constant value.

## 3. Contract

### 3.1 Preconditions

- [ ] None - function is callable in any context

### 3.2 Postconditions

- [ ] Return value is exactly 1e100 (0x46293e5939a08cea in IEEE 754 hex)
- [ ] No state is modified anywhere
- [ ] Function always succeeds

### 3.3 Invariants

- [ ] Return value is constant across all calls
- [ ] No side effects occur
- [ ] Thread-safe by nature (returns constant)

## 4. Algorithm

### 4.1 Overview

The function provides access to Convexfeld's infinity constant through one of three possible implementation strategies. The simplest and most likely implementation returns the constant 1e100 directly. Alternative implementations might read from a fixed location in the environment structure for configurability (though the value is never actually changed), or might conditionally check for environment validity before returning the constant or reading from the environment.

The choice of 1e100 as infinity is deliberate: it is large enough to be effectively infinite for all practical optimization problems, yet small enough to avoid IEEE 754 special values and their associated arithmetic hazards.

### 4.2 Detailed Steps

**Implementation Strategy A (most likely):**
1. Return the compile-time constant 1e100

**Implementation Strategy B (environment-based):**
1. Check if environment pointer is null
2. If null, return constant 1e100
3. If valid, read double value from environment structure at fixed offset (0x2050)
4. Return the read value

**Implementation Strategy C (parameter-based):**
1. Attempt to retrieve "Infinity" parameter via parameter system
2. If successful, return the retrieved value
3. If failed, return default constant 1e100

Strategy A is most efficient and likely, as the infinity value never changes. Strategy B might exist if early Convexfeld versions considered making infinity configurable. Strategy C is least likely due to performance overhead.

### 4.3 Mathematical Foundation

The value 1e100 is chosen based on several numerical considerations:

- **Finite arithmetic**: Avoids IEEE 754 infinity which produces NaN in operations like ∞ - ∞
- **Comparison safety**: Enables meaningful comparisons (x < INF always works correctly)
- **Range adequacy**: Large enough that no practical optimization problem has values approaching this magnitude
- **Detection capability**: Can distinguish between "large finite" (e.g., 1e50) and "infinite" (≥1e100)

This is a common convention in optimization software. CPLEX uses 1e20, GLPK uses 1e10, demonstrating that the exact value is less important than having a consistent, finite representation.

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) - returns constant
- **Average case:** O(1) - returns constant
- **Worst case:** O(1) - returns constant

### 5.2 Space Complexity

- **Auxiliary space:** O(1) - no allocations
- **Total space:** O(1) - single return value

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| (none) | N/A | Function cannot fail |

### 6.2 Error Behavior

The function is infallible - it always returns a valid infinity constant regardless of input or environment state.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| No environment | (void) | Return 1e100 |
| Null environment | NULL | Return 1e100 |
| Valid environment | env | Return 1e100 |
| First call | Any | Return 1e100 |
| Millionth call | Any | Return 1e100 (constant) |
| Concurrent calls | Any | Return 1e100 (thread-safe) |

## 8. Thread Safety

**Thread-safe:** Yes

The function is inherently thread-safe because:
- Returns a constant value with no state access (Strategy A)
- Or performs only read operations on immutable environment field (Strategy B)
- No writes occur
- No shared mutable state exists

**Synchronization required:** None

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| (none) | N/A | Strategy A has no dependencies |
| cxf_getdblparam | Parameters | Strategy C would use this (unlikely) |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_addvar | API | Checking for unbounded variables in variable creation |
| cxf_addconstr | API | Validating constraint bounds |
| cxf_is_infinity | Utilities | Determining if a value represents infinity |
| cxf_is_finite | Utilities | Determining if a value is bounded |
| cxf_bound_check | Validation | Validating variable bounds are valid |
| Simplex initialization | LP Core | Setting up unbounded variable handling |
| Presolve unbounded detection | Presolve | Detecting unbounded models |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_is_infinity | Uses this function to define infinity threshold for comparisons |
| cxf_is_neg_infinity | Parallel function - checks for negative infinity (-1e100) |
| cxf_is_finite | Complement function - checks if value is finite (not ±infinity) |
| cxf_get_feasibility_tol | Parallel parameter getter - different parameter type |
| cxf__INFINITY constant | Macro or constant - may be defined as call to this function |

## 11. Design Notes

### 11.1 Design Rationale

**Why 1e100 instead of IEEE 754 infinity:**
1. **NaN avoidance**: Operations like INF - INF produce NaN, which propagates and corrupts calculations
2. **Comparison semantics**: Can use normal comparison operators without special case handling
3. **Arithmetic safety**: Can safely compute expressions like (ub - lb) without overflow
4. **Detectability**: Can distinguish between "very large" and "unbounded" values
5. **Portability**: Consistent across all platforms and floating-point modes
6. **Debugging**: Can see the actual value in debuggers rather than "inf"

**Why as a function rather than macro:**
1. **Abstraction**: Allows future changes to definition or computation
2. **Type safety**: Ensures correct type (double) is returned
3. **Linkage**: Can be used across compilation units
4. **Debugging**: Can set breakpoints on infinity access if needed

**Storage in environment (offset 0x2050):**
- Allows runtime inspection of the infinity value
- Provides single source of truth
- Enables potential future configurability (though never used)
- Consistent with other parameter storage patterns

### 11.2 Performance Considerations

- **Inlining**: Compiler should inline this function for zero overhead
- **Constant propagation**: Optimizer can replace calls with literal 1e100
- **Call frequency**: May be called frequently in bound checking loops
- **Alternative**: Could be a preprocessor macro for guaranteed zero cost
- **Measurement**: Profile to determine if function call overhead matters

In practice, modern compilers will optimize this to a constant load with no function call overhead.

### 11.3 Future Considerations

- Could add validation in debug mode that environment value equals constant
- Could add alternative precision modes (1e200 for quad precision)
- Could expose as part of public API for user infinity checks
- Could provide negative infinity getter for symmetry

## 12. References

- IEEE 754 floating-point standard - special values and arithmetic
- Numerical computing literature on infinity representation in optimization
- Comparison with other solvers' infinity conventions (CPLEX: 1e20, GLPK: 1e10)

## 13. Validation Checklist

- [x] No code copied from implementation
- [x] Algorithm description is implementation-agnostic
- [x] All parameters documented
- [x] All error conditions listed
- [x] Complexity analysis complete
- [x] Edge cases identified
- [x] A competent developer could implement from this spec alone

---


## Appendix: Infinity Usage Patterns

### Variable Bounds Interpretation

| Lower Bound | Upper Bound | Interpretation |
|-------------|-------------|----------------|
| -INF | +INF | Free variable (unbounded) |
| 0 | +INF | Non-negative variable |
| -INF | 0 | Non-positive variable |
| lb | +INF | Lower bounded only |
| -INF | ub | Upper bounded only |
| lb | ub | Box constrained |
| lb | lb | Fixed variable |

### Constraint Right-Hand Side

- RHS = +INF: Constraint is always satisfied (can be removed)
- RHS = -INF: Constraint is never satisfied (model infeasible)
- Finite RHS: Normal constraint with specific bound

### Parameter Limits

- TimeLimit = +INF: No time limit
- NodeLimit = +INF: No node limit
- IterationLimit = +INF: No iteration limit
- Cutoff = +INF: No objective cutoff

### Comparison Utilities

```
is_infinite(x) ≡ (x ≥ 1e100)
is_neg_infinite(x) ≡ (x ≤ -1e100)
is_finite(x) ≡ (-1e100 < x < 1e100)
is_unbounded_above(ub) ≡ (ub ≥ 1e100)
is_unbounded_below(lb) ≡ (lb ≤ -1e100)
is_free(lb, ub) ≡ is_unbounded_below(lb) ∧ is_unbounded_above(ub)
```

---
*Convexfeld LP Solver Specification*
*Based on published optimization literature*
