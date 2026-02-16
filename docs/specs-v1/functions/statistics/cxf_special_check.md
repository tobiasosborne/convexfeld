# cxf_special_check

**Module:** Statistics
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Determines whether a variable qualifies for special pivot handling in the simplex algorithm by validating its bounds, flags, and quadratic structure. Variables that pass all checks can use optimized pivot operations that exploit special structural properties. This screening function prevents invalid special pivots that could cause numerical errors or algorithmic failures.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| state | void* | SolverState structure pointer | Valid pointer | Yes |
| var | int | Variable index to check | 0 to numVars-1 | Yes |
| work_accum | double* | Optional work accumulator for complexity tracking | NULL or valid pointer | No |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | 1 if variable qualifies for special pivot, 0 otherwise |
| work_accum | double* | Incremented by work complexity metric if non-NULL |

### 2.3 Side Effects

If work_accum is non-NULL, increments it by a scaled count of quadratic matrix entries processed. No other state modifications.

## 3. Contract

### 3.1 Preconditions

- [ ] state pointer must be valid SolverState structure
- [ ] var must be a valid variable index
- [ ] All internal arrays (lb, varFlags, diagQ, etc.) must be initialized
- [ ] If work_accum is non-NULL, must point to valid double

### 3.2 Postconditions

- [ ] Return value is 0 or 1 (boolean)
- [ ] If work_accum non-NULL and quadratic terms exist, work_accum is incremented
- [ ] No state modifications except work_accum

### 3.3 Invariants

- [ ] SolverState structure is not modified
- [ ] Variable bounds and flags remain unchanged

## 4. Algorithm

### 4.1 Overview

The function performs a series of hierarchical checks on the variable's properties. First, it validates that the variable's lower bound is finite (not negative infinity). Second, it checks that reserved flag bits are not set, indicating a valid variable configuration. Third, if the upper bound finite flag is set, it verifies the bound value is valid. Fourth, if the variable has quadratic terms, it validates the quadratic matrix structure: the diagonal entry must be non-negative (positive semidefinite requirement), all off-diagonal entries must be non-negative, and all referenced column variables must have valid bounds. If a work accumulator is provided, the function also tracks the computational cost of scanning quadratic entries.

### 4.2 Detailed Steps

1. Extract variable's lower bound from state
2. Extract variable's flags from state
3. Check if lower bound is below negative infinity threshold - if yes, return 0
4. Check if any reserved flag bits (0xFFFFFFB0 mask) are set - if yes, return 0
5. If VARFLAG_UPPER_FINITE (0x04) is set:
   - Check if lower bound exceeds positive infinity - if yes, return 0
6. If VARFLAG_HAS_QUADRATIC (0x08) is set:
   - Extract diagonal Q matrix entry for this variable
   - If diagonal entry is negative (not zero), return 0
   - If Q row count array exists:
     - Extract row start index, row count
     - For each Q matrix entry in row:
       - Extract Q value - if negative, return 0
       - Extract column variable index
       - Extract column variable's lower bound
       - If column bound is invalid (below negative infinity and not equal), return 0
     - If work accumulator is non-NULL:
       - Compute work increment: 0.5 * scaleFactor * num_entries_processed
       - Add to work accumulator
     - Verify all expected entries were processed - if not, return 0
7. Return 1 (variable qualifies)

### 4.3 Pseudocode (if needed)

```
FUNCTION special_check(state, var, work_accum):
  lb_value ← state.lb_original[var]
  flags ← state.varFlags[var]

  // Check 1: Finite lower bound
  IF lb_value < NEG_INFINITY:
    RETURN 0

  // Check 2: Reserved flags
  IF (flags & RESERVED_MASK) != 0:
    RETURN 0

  // Check 3: Upper bound validity
  IF (flags & UPPER_FINITE_FLAG) != 0:
    IF lb_value > INFINITY:
      RETURN 0

  // Check 4: Quadratic structure
  IF (flags & HAS_QUADRATIC_FLAG) != 0:
    diag_q ← state.diagQ[var]

    // Check 4a: Non-negative diagonal
    IF diag_q < 0 AND diag_q != 0:
      RETURN 0

    // Check 4b: Off-diagonal entries
    IF state.qRowCount != NULL:
      row_start ← state.qRowStart[var]
      row_count ← state.qRowCount[var]

      processed ← 0
      FOR i in row_start .. row_start+row_count-1:
        q_val ← state.qValues[i]

        // Non-negative Q value
        IF NOT (q_val > 0 OR q_val == 0):
          RETURN 0

        // Valid column variable bound
        col_var ← state.qColIndices[i]
        col_lb ← state.lb_original[col_var]

        IF NOT (col_lb > NEG_INFINITY OR col_lb == NEG_INFINITY):
          RETURN 0

        processed ← processed + 1

      // Track work complexity
      IF work_accum != NULL:
        work_increment ← 0.5 * state.scaleFactor * processed
        work_accum ← work_accum + work_increment

      // Verify complete processing
      IF processed < row_count:
        RETURN 0

  RETURN 1
```

### 4.4 Mathematical Foundation (if applicable)

For quadratic programming, the quadratic objective matrix Q must be positive semidefinite for convexity. The special pivot algorithm exploits this property, so the function validates that all Q entries are non-negative (sufficient for positive semidefiniteness in the context of this check). The lower bound validation ensures that the variable's feasible region is bounded from below, which is required for certain pivot operations.

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) - Variable fails early bound or flag check
- **Average case:** O(q_var) - Variable has q_var quadratic entries
- **Worst case:** O(q_var) - Must scan all quadratic entries

Where:
- q_var = number of quadratic matrix entries for this variable (typically small)

### 5.2 Space Complexity

- **Auxiliary space:** O(1) - Fixed-size local variables
- **Total space:** O(1)

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| Invalid variable | N/A | Returns 0 (does not raise error) |
| Invalid bounds | N/A | Returns 0 (does not raise error) |
| Invalid Q structure | N/A | Returns 0 (does not raise error) |

### 6.2 Error Behavior

Function does not raise errors - it returns 0 for any invalid configuration. Caller interprets 0 as "use standard pivot" and 1 as "can use special pivot". NULL pointer dereference would cause crash (caller responsibility).

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| No quadratic terms | flags & 0x08 == 0 | Skip Q validation, return 1 if bounds valid |
| Zero diagonal Q | diagQ[var] == 0 | Accept (allow off-diagonal-only Q) |
| Negative diagonal Q | diagQ[var] < 0 | Reject (return 0) |
| Unbounded variable | lb == -∞ | Reject (return 0) |
| Reserved flags set | flags & 0xFFFFFFB0 != 0 | Reject (return 0) |
| NULL work_accum | work_accum == NULL | Skip work tracking, continue validation |
| Empty Q row | row_count == 0 | Accept (no entries to validate) |

## 8. Thread Safety

**Thread-safe:** Yes (read-only)

**Synchronization required:** None if SolverState is not modified concurrently. Function only reads state, except for optional work_accum write.

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| None | N/A | Pure validation function with no external calls |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_pivot_special | Simplex Core | Determine if special pivot algorithm can be used |
| cxf_simplex_step | Simplex Core | Classify variables during simplex iteration |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_pivot_special | Uses this check to decide pivot algorithm |
| cxf_pivot_primal | Standard pivot used when special check fails |

## 11. Design Notes

### 11.1 Design Rationale

Special pivot algorithms exploit structural properties (bounded variables, positive semidefinite Q) to improve performance. This check ensures these preconditions are met before invoking the specialized algorithm. Returning a boolean (0/1) rather than raising errors allows graceful fallback to standard pivot operations. The work accumulator parameter enables iteration limit enforcement based on actual computational cost.

### 11.2 Performance Considerations

Fast rejection on bound/flag checks avoids unnecessary Q matrix scanning. Loop over Q entries is unavoidable for validation but typically involves few entries per variable. Read-only nature allows concurrent calls for different variables (if needed).

### 11.3 Future Considerations

Could cache validation results if variables are checked repeatedly. Could provide detailed rejection reasons for debugging (currently returns 0 for all failures).

## 12. References

- Nocedal, Wright (2006). "Numerical Optimization" - Positive Semidefinite Matrices
- Convexfeld Optimizer Reference Manual: Quadratic Programming

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
