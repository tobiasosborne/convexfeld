# cxf_count_genconstr_types

**Module:** Utilities
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Counts general constraints in a model by type (0-18) and treatment mode (piecewise-linear approximation vs. true nonlinear). Populates two output arrays with counts for each constraint type, enabling detailed statistical reporting of model characteristics. Used primarily by presolve/logging code to inform users about constraint composition.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| model | CxfModel* | Model containing general constraints | Valid pointer | Yes |
| pwlCounts | int* | Output array for PWL-approximated counts | Array[19], caller must zero-initialize | Yes |
| nlCounts | int* | Output array for nonlinear counts | Array[19], caller must zero-initialize | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| void | N/A | No return value |
| pwlCounts | int[19] | Modified: counts of PWL-treated constraints by type |
| nlCounts | int[19] | Modified: counts of nonlinear-treated constraints by type |

### 2.3 Side Effects

- Increments counters in pwlCounts and nlCounts arrays
- Does NOT initialize arrays (caller must zero them)

## 3. Contract

### 3.1 Preconditions

- [ ] Model pointer should be valid (gracefully handles NULL)
- [ ] pwlCounts array must be at least 19 elements
- [ ] nlCounts array must be at least 19 elements
- [ ] **Caller must zero-initialize both arrays before calling**

### 3.2 Postconditions

- [ ] pwlCounts[i] incremented for each type-i constraint treated as PWL
- [ ] nlCounts[i] incremented for each type-i constraint treated as nonlinear
- [ ] Total sum: ∑(pwlCounts[i] + nlCounts[i]) = numGenConstrs
- [ ] Function never decrements or resets counters

### 3.3 Invariants

- [ ] Model data not modified (read-only)
- [ ] Only valid type indices (0-18) are incremented
- [ ] Invalid types are silently skipped

## 4. Algorithm

### 4.1 Overview

Iterates through all general constraints in the model, extracting each constraint's type and treatment mode, then incrementing the appropriate counter in either pwlCounts or nlCounts.

### 4.2 Detailed Steps

1. Validate model pointer
   - If NULL, return immediately (no-op)
2. Access MatrixData structure from model
   - If NULL, return immediately
3. Read numGenConstrs from MatrixData
   - If ≤ 0, return immediately (no constraints)
4. Read genConstrArray pointer from MatrixData
   - If NULL, return immediately
5. For i = 0 to numGenConstrs-1:
   a. Get constraint pointer: constr = genConstrArray[i]
   b. Skip if constr is NULL
   c. Read type from constr
   d. Validate type is in range [0, 18]
      - If out of range, skip this constraint
   e. Read treatment mode from constraint structure
      - Offset uncertain, likely constr or encoded in flags
   f. If treatment mode is PWL:
      - Increment pwlCounts[type]
   g. Else (nonlinear):
      - Increment nlCounts[type]
6. Return (void)

### 4.3 Pseudocode

```
PROCEDURE cxf_count_genconstr_types(model, pwlCounts, nlCounts):
    IF model = NULL THEN RETURN

    matrix ← model
    IF matrix = NULL THEN RETURN

    numGenConstrs ← matrix
    IF numGenConstrs ≤ 0 THEN RETURN

    genConstrArray ← matrix
    IF genConstrArray = NULL THEN RETURN

    FOR i ← 0 TO numGenConstrs - 1:
        constr ← genConstrArray[i]
        IF constr = NULL THEN CONTINUE

        type ← constr
        IF type < 0 OR type ≥ 19 THEN CONTINUE

        treatmentMode ← constr  # Offset uncertain

        IF treatmentMode = PWL THEN
            pwlCounts[type] ← pwlCounts[type] + 1
        ELSE
            nlCounts[type] ← nlCounts[type] + 1
    END FOR
END PROCEDURE
```

### 4.4 General Constraint Types

```
Type 0:  MAX       - max(x₁, ..., xₙ) = y
Type 1:  MIN       - min(x₁, ..., xₙ) = y
Type 2:  ABS       - |x| = y
Type 3:  AND       - y = AND(x₁, ..., xₙ)
Type 4:  OR        - y = OR(x₁, ..., xₙ)
Type 5:  INDICATOR - z=1 ⟹ ax ≤ b
Type 6:  NL        - General nonlinear (internal)
Type 7:  PWL       - Piecewise-linear
Type 8:  POLY      - Polynomial
Type 9:  EXP       - exp(x) = y
Type 10: EXPA      - aˣ = y
Type 11: LOG       - ln(x) = y
Type 12: LOGA      - logₐ(x) = y
Type 13: POW       - xᵃ = y
Type 14: SIN       - sin(x) = y
Type 15: COS       - cos(x) = y
Type 16: TAN       - tan(x) = y
Type 17-18: Reserved
```

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) - no constraints (immediate return)
- **Average case:** O(k) where k = number of general constraints
- **Worst case:** O(k)

Where:
- k = numGenConstrs
- Per constraint: ~3 memory reads, 1 increment (~10 cycles)
- Total: k × 10 cycles ≈ 10 microseconds for 1000 constraints

### 5.2 Space Complexity

- **Auxiliary space:** O(1) - no allocations
- **Total space:** O(1) - output written to provided arrays

## 6. Error Handling

### 6.1 Error Conditions

This function has no return value (void). It silently handles errors:

| Condition | Error Code | Description |
|-----------|------------|-------------|
| NULL model | N/A | Silent return, no-op |
| NULL matrix | N/A | Silent return, no-op |
| NULL constraint array | N/A | Silent return, no-op |
| NULL constraint entry | N/A | Skip entry, continue |
| Invalid type | N/A | Skip entry, continue |

### 6.2 Error Behavior

Function never fails or throws exceptions. All error conditions result in silent skipping or early return. Caller cannot detect errors except by checking if output arrays are still zero.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| No constraints | numGenConstrs = 0 | Return immediately, arrays unchanged |
| NULL model | model = NULL | Return immediately, arrays unchanged |
| Mixed types | Various | Each type counted separately |
| Invalid types | type < 0 or ≥ 19 | Silently skip those constraints |
| NULL entries | Some array[i] = NULL | Skip NULL entries |
| All PWL | No NL constraints | nlCounts all remain 0 |
| All NL | No PWL constraints | pwlCounts all remain 0 |

## 8. Thread Safety

**Thread-safe:** No

The function is not thread-safe because:
- Concurrent calls with same output arrays create race conditions (non-atomic increments)
- Concurrent model modification could corrupt constraint data

**Synchronization required:** Caller must ensure:
- Exclusive access to output arrays during call
- Model not modified during call

## 9. Dependencies

### 9.1 Functions Called

None (direct field access)

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_presolve_stats | Statistics | Log model statistics before optimization |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_get_genconstr_name | Maps type index to name string (used together) |
| cxf_addgenconstrXXX | Functions that add general constraints |
| cxf_presolve_stats | Main caller for statistical logging |

## 11. Design Notes

### 11.1 Design Rationale

- Separate PWL and NL counts allow reporting solver strategy to user
- Caller-provided arrays avoid allocation overhead
- Void return simplifies caller (no error checking needed)
- Silent error handling prevents crash from invalid data

### 11.2 Performance Considerations

Single-pass O(k) algorithm is optimal. Function not performance-critical (called once during presolve logging). Zero-allocation design minimizes overhead.

### 11.3 Future Considerations

- Could add return value to indicate success/failure
- Could validate array sizes (currently caller's responsibility)
- Could support partial counting (count only specific types)
- Could combine with cxf_get_genconstr_name for single-call reporting

## 12. References

- Function constraint treatment parameters: FuncPieces, FuncNonlinear

## 13. Validation Checklist

Before finalizing this spec, verify:

- [x] No code copied from implementation
- [x] Algorithm description is implementation-agnostic
- [x] All parameters documented
- [x] All error conditions listed
- [x] Complexity analysis complete
- [x] Edge cases identified
- [ ] **Structure offset verification needed** (genConstrArray, treatmentMode field)
- [x] A competent developer could implement from this spec alone

---

*Reviewed by: Pending*

## Important Caller Requirements

**CRITICAL:** Caller must zero-initialize output arrays:
```c
int pwlCounts[19] = {0};
int nlCounts[19] = {0};
cxf_count_genconstr_types(model, pwlCounts, nlCounts);
```

The function only INCREMENTS counters, never initializes them.

## Verification Required

The following offsets are estimated and require verification:
- MatrixData: numGenConstrs location
- MatrixData: genConstrArray pointer location
- GenConstr: type field location
- GenConstr: treatmentMode field location (TENTATIVE)


---
*Convexfeld LP Solver Specification*
*Based on published optimization literature*
