# cxf_get_genconstr_name

**Module:** Utilities
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Maps general constraint type indices (0-18) to human-readable string names for logging, error messages, and statistical output. Convexfeld supports 19 types of general constraints for modeling non-linear and logical relationships. This function provides consistent naming for display purposes.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| typeIndex | int | General constraint type | 0-18 | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | const char* | Pointer to static string with constraint type name, or "UNKNOWN" for invalid indices |

### 2.3 Side Effects

None (returns pointer to static read-only data)

## 3. Contract

### 3.1 Preconditions

- [ ] None (handles all integer inputs)

### 3.2 Postconditions

- [ ] Returns pointer to null-terminated string
- [ ] Returned string is valid for program lifetime
- [ ] String content matches Convexfeld's official constraint type names
- [ ] Out-of-range indices return "UNKNOWN"

### 3.3 Invariants

- [ ] Returned pointer always valid (never NULL)
- [ ] Returned string never modified
- [ ] Function is deterministic

## 4. Algorithm

### 4.1 Overview

Simple array lookup that maps constraint type index to its corresponding name string. Uses bounds checking to handle invalid indices gracefully.

### 4.2 Detailed Steps

1. Check if typeIndex is less than 0
   - If true, return pointer to "UNKNOWN" string
2. Check if typeIndex is greater than or equal to 19
   - If true, return pointer to "UNKNOWN" string
3. Return pointer to typeNames[typeIndex] from static lookup table

### 4.3 Constraint Type Mapping

```
Index   Name        Description
-----   ----        -----------
0       MAX         Maximum of variables
1       MIN         Minimum of variables
2       ABS         Absolute value
3       AND         Logical AND
4       OR          Logical OR
5       INDICATOR   Indicator constraint (if-then)
6       NL          General nonlinear (internal)
7       PWL         Piecewise-linear
8       POLY        Polynomial
9       EXP         Exponential (e^x)
10      EXPA        Exponential to base a
11      LOG         Natural logarithm
12      LOGA        Logarithm to base a
13      POW         Power function
14      SIN         Sine
15      COS         Cosine
16      TAN         Tangent
17      UNKNOWN     Reserved/future type
18      UNKNOWN     Reserved/future type
```

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) - single array lookup
- **Average case:** O(1)
- **Worst case:** O(1)

All cases perform at most 2 comparisons and 1 array access (< 5 CPU cycles).

### 5.2 Space Complexity

- **Auxiliary space:** O(1) - no allocations
- **Total space:** O(1) - returns pointer to static data

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| Negative index | N/A | Returns "UNKNOWN" (not an error) |
| Index ≥ 19 | N/A | Returns "UNKNOWN" (not an error) |

### 6.2 Error Behavior

Function never fails. Invalid indices return "UNKNOWN" string, allowing calling code to continue safely.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Minimum valid | 0 | Returns "MAX" |
| Maximum valid | 18 | Returns "UNKNOWN" (reserved type) |
| Negative | -1 | Returns "UNKNOWN" |
| Too large | 100 | Returns "UNKNOWN" |
| Reserved types | 17, 18 | Returns "UNKNOWN" |

## 8. Thread Safety

**Thread-safe:** Yes

The function is fully thread-safe because:
- Returns pointer to static const data (read-only)
- No mutable state
- No synchronization needed

**Synchronization required:** None

## 9. Dependencies

### 9.1 Functions Called

None (pure lookup)

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_presolve_stats | Statistics | Log constraint counts by type |
| cxf_count_genconstr_types | Utilities | Used together for constraint categorization |
| Error/logging functions | Various | Format constraint type in messages |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_count_genconstr_types | Counts constraints by type (uses this for display) |
| cxf_addgenconstrXXX | Functions that add each constraint type |
| cxf_getgenconstrXXX | Functions that query each constraint type |

## 11. Design Notes

### 11.1 Design Rationale

- Static lookup table is fastest and simplest approach
- "UNKNOWN" default prevents crashes from invalid indices
- Names match official Convexfeld API constants (without cxf__GENCONSTR_ prefix)
- Returning "UNKNOWN" for types 17-18 allows for future constraint types without code changes

### 11.2 Performance Considerations

Extremely fast (< 5 cycles). Frequently inlined by compiler. No performance concerns.

### 11.3 Future Considerations

Types 17-18 are reserved for future constraint types. When Convexfeld adds new constraint types, this lookup table must be updated with appropriate names.

## 12. References

- Convexfeld API Constants: cxf__GENCONSTR_MAX through cxf__GENCONSTR_TAN

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

## Implementation Note

The lookup table should match Convexfeld's official naming exactly:
- Type 6 ("NL") is for internal general nonlinear constraints
- Types 17-18 use "UNKNOWN" as placeholders for future types
- All names are uppercase strings
- The function can be implemented as either array lookup or switch statement

---
*Convexfeld LP Solver Specification*
*Based on published optimization literature*
