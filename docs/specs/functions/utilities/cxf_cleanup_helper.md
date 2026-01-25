# cxf_cleanup_helper

**Module:** Utilities
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Performs iterative bound propagation to tighten variable bounds during the simplex cleanup phase. The function uses constraint activity analysis to derive implied bounds on variables, propagating improvements through the constraint matrix. It can detect infeasibility early when bounds become inconsistent and supports sophisticated numerical precision handling for floating-point operations.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| env | void* | Environment pointer for memory allocation | Non-NULL | Yes |
| state | SolverState* | Current solver state with solution | Valid pointer | Yes |
| lb_working | double* | Working lower bounds array | Array of size numVars | Yes |
| ub_working | double* | Working upper bounds array | Array of size numVars | Yes |
| constrSenses | uint8_t* | Constraint sense array | Array with '<', '=', '>' | Yes |
| lb_delta | double* | Lower bound improvement accumulators | Array of size numConstrs | Yes |
| ub_delta | double* | Upper bound improvement accumulators | Array of size numConstrs | Yes |
| lb_count | int32_t* | Lower bound update counters | Array of size numConstrs | Yes |
| ub_count | int32_t* | Upper bound update counters | Array of size numConstrs | Yes |
| lb_threshold | double | Lower bound tolerance | Positive value | Yes |
| ub_threshold | double | Upper bound tolerance | Positive value | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | Status code: 0=success, 3=infeasible, 0x2711=out of memory |
| lb_working | double* | Updated lower bounds (in-place modification) |
| ub_working | double* | Updated upper bounds (in-place modification) |
| lb_delta | double* | Updated improvement accumulators |
| ub_delta | double* | Updated improvement accumulators |
| lb_count | int32_t* | Updated counters |
| ub_count | int32_t* | Updated counters |

### 2.3 Side Effects

- Allocates temporary worklist arrays (freed before return)
- Updates work counter in solver state for statistics tracking
- Stores infeasible variable index in state on failure

## 3. Contract

### 3.1 Preconditions

- [ ] Environment pointer must be valid for memory allocation
- [ ] SolverState must contain valid constraint matrix (CSR and CSC formats)
- [ ] All input arrays must have correct dimensions (numVars or numConstrs)
- [ ] Constraint senses must be valid ('<', '=', or '>')
- [ ] Thresholds must be positive values

### 3.2 Postconditions

- [ ] On success: Variable bounds are tightened (never loosened)
- [ ] On success: All bound improvements propagated through constraints
- [ ] On infeasibility: Returns error code 3
- [ ] On infeasibility: Stores index of infeasible variable in state
- [ ] Temporary memory is freed before return

### 3.3 Invariants

- [ ] Original constraint matrix is never modified
- [ ] Lower bounds never exceed upper bounds after valid return
- [ ] Function is idempotent (multiple calls produce same result)

## 4. Algorithm

### 4.1 Overview

The function implements a worklist-based bound propagation algorithm that tightens variable bounds by analyzing constraint activity. Starting with all nonbasic variables, it iteratively:

1. Checks if current variable bounds violate constraint feasibility thresholds
2. Computes tighter bounds based on reduced cost direction and constraint coefficients
3. Propagates bound changes through the constraint matrix to affected variables
4. Adds affected variables to a worklist for further processing

The algorithm performs up to 10 complete passes through the variable set to ensure convergence. It uses Kahan-style compensated summation to handle floating-point precision issues when adding numbers with very different magnitudes.

### 4.2 Detailed Steps

1. Validate inputs and early exit if no variables exist
2. Allocate two temporary arrays: worklist (variable indices) and inWorklist (boolean flags)
3. Initialize worklist with all nonbasic variables (those not in the basis)
4. Set up circular buffer pointers (head and tail) for worklist iteration
5. For each variable in worklist:
   - Check constraint sense and verify feasibility against thresholds
   - If infeasible, return error code 3 with variable index stored
   - For each nonzero coefficient in constraint row:
     - Compute potential bound improvements based on coefficient sign
     - Apply bounds from both <= and >= constraint interpretations
     - Keep tightest bounds found
   - If lower bound improved significantly:
     - Propagate change through all constraints containing this variable
     - For each affected constraint: update delta values and counters
     - Add affected variables to worklist if not already present
   - If upper bound improved significantly:
     - Similar propagation through constraint columns
   - Mark variable as processed and move to next in worklist
6. Wrap around circular buffer when reaching end of variable array
7. Count complete passes and terminate after 10 iterations to prevent infinite loops
8. Free temporary worklist arrays before return

### 4.3 Pseudocode (if needed)

```
worklist ← all nonbasic variables
inWorklist ← flags[numVars] initialized to 1 for nonbasic, 0 for basic
head ← 0, tail ← worklistCount - 1
passCount ← 0

WHILE head ≠ tail AND passCount ≤ 10:
    varIdx ← worklist[head]
    sense ← constrSenses[varIdx]

    # Check infeasibility conditions
    IF (sense is ≤ or =) AND (ub_count[varIdx] = 0) AND (ub_delta[varIdx] > ub_threshold):
        RETURN infeasible with varIdx
    IF (sense is ≥ or =) AND (lb_count[varIdx] = 0) AND (lb_delta[varIdx] < -ub_threshold):
        RETURN infeasible with varIdx

    # Process constraint row to tighten bounds
    FOR each nonzero coefficient in row[varIdx]:
        colIdx ← column index of coefficient
        coeff ← coefficient value
        newLB ← lb_working[colIdx]
        newUB ← ub_working[colIdx]

        # Compute implied bounds based on constraint
        IF sense permits upper bound:
            newUB ← MIN(newUB, derive_ub_from_constraint(varIdx, coeff))
        IF sense permits lower bound:
            newLB ← MAX(newLB, derive_lb_from_constraint(varIdx, coeff))

        # If bounds improved significantly, propagate
        IF newLB > lb_working[colIdx] + tolerance:
            IF newLB > newUB + tolerance:
                RETURN infeasible with colIdx
            lb_working[colIdx] ← newLB
            PROPAGATE bound change through column[colIdx]
            ADD affected variables to worklist

        IF newUB < ub_working[colIdx] - tolerance:
            IF newUB < newLB - tolerance:
                RETURN infeasible with colIdx
            ub_working[colIdx] ← newUB
            PROPAGATE bound change through column[colIdx]
            ADD affected variables to worklist

    # Move to next variable
    head ← (head + 1) MOD numVars
    inWorklist[varIdx] ← 0

    IF head = 0:
        passCount ← passCount + 1

RETURN success
```

### 4.4 Mathematical Foundation (if applicable)

The bound tightening uses constraint activity analysis. For a constraint:
```
∑ aᵢxᵢ {≤,=,≥} b
```

Given bounds [lᵢ, uᵢ] for all variables except xⱼ, we can derive:
- If constraint is ≤: xⱼ ≤ (b - ∑ᵢ≠ⱼ aᵢxᵢ) / aⱼ (considering signs)
- If constraint is ≥: xⱼ ≥ (b - ∑ᵢ≠ⱼ aᵢxᵢ) / aⱼ (considering signs)

The algorithm iteratively applies these implications, propagating tighter bounds through the constraint network until convergence or infeasibility is detected.

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(n) where n = number of variables, single pass with no propagation
- **Average case:** O(n × d × p) where d = average variable degree (nonzeros per column), p = 2-3 passes
- **Worst case:** O(n × d × 10) - maximum 10 passes through all variables

Where:
- n = number of variables
- d = average degree (constraints per variable)
- p = number of passes until convergence (typically 2-3, max 10)

### 5.2 Space Complexity

- **Auxiliary space:** O(n) for two temporary worklist arrays
- **Total space:** O(n) since constraint matrix is not copied

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| Memory allocation failure | 0x2711 | Failed to allocate worklist arrays |
| Bound crossing | 3 | Lower bound exceeds upper bound after tightening |
| Constraint violation | 3 | Constraint activity violates feasibility thresholds |

### 6.2 Error Behavior

On error, the function:
- Frees any allocated temporary memory before return
- Stores the index of the infeasible variable in state structure
- Returns error code without modifying further state
- Leaves bound arrays in partially modified state (some bounds may be tightened)

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| No variables | numVars = 0 | Immediate return with success |
| All basic variables | Empty worklist | No work, immediate success |
| Already tight bounds | No improvements possible | Single pass, no changes |
| Immediate infeasibility | First variable check fails | Quick failure return |
| Maximum iterations | 10 full passes | Terminate with current bounds |

## 8. Thread Safety

**Thread-safe:** No

The function is not thread-safe because:
- Modifies shared bound arrays in-place
- Updates solver state work counter
- Multiple threads would create race conditions

**Synchronization required:** Caller must ensure exclusive access to model during cleanup

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_malloc | Memory | Allocate worklist arrays |
| cxf_free | Memory | Free worklist arrays |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_simplex_cleanup | Simplex | Final solution cleanup after optimization |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_simplex_cleanup | Parent function that invokes bound tightening |
| cxf_presolve_bounds | Similar bound propagation during presolve phase |
| cxf_simplex_refine | Solution refinement before cleanup |

## 11. Design Notes

### 11.1 Design Rationale

The worklist algorithm avoids redundant work by only processing variables that may have improved bounds. The circular buffer implementation prevents memory reallocation during iteration. The 10-pass limit ensures termination even if bounds oscillate numerically.

### 11.2 Performance Considerations

- Worklist avoids O(n²) naive iteration over all variable pairs
- Circular buffer eliminates dynamic allocation in inner loop
- Early termination on infeasibility saves unnecessary computation
- Kahan summation adds ~3x overhead but prevents catastrophic cancellation
- Work counter tracking adds negligible overhead (<1%)

### 11.3 Future Considerations

- Adaptive pass limit based on improvement rate
- Priority queue for worklist (process most promising variables first)
- Parallel bound propagation for large models
- Incremental bound tightening (warm start from previous cleanup)

## 12. References

- Dantzig, G. (1963). Linear Programming and Extensions. Princeton University Press.
- Standard bound propagation algorithms in constraint programming
- Kahan summation algorithm for numerical stability

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
