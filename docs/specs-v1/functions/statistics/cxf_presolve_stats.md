# cxf_presolve_stats

**Module:** Statistics
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Logs descriptive statistics about advanced model features to help users understand their model structure before optimization begins. This is a diagnostic logging function that reports the presence of quadratic terms, special ordered sets, piecewise-linear objectives, and general constraints. It does NOT perform any model transformations or presolve reductions.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| model | CxfModel* | Model structure containing problem data | Valid pointer | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | void | No return value |
| logging | side effect | Messages written to environment log |

### 2.3 Side Effects

Writes formatted log messages to the environment's logging system. No model state is modified.

## 3. Contract

### 3.1 Preconditions

- [ ] model pointer must be valid
- [ ] model->env must be valid for logging
- [ ] model->matrix must be initialized with counts

### 3.2 Postconditions

- [ ] Log contains count of quadratic objective terms (if any)
- [ ] Log contains count of quadratic constraints (if any)
- [ ] Log contains count of bilinear constraints (if any)
- [ ] Log contains count of SOS constraints (if any)
- [ ] Log contains count of PWL objective terms (if any)
- [ ] Log contains breakdown of general constraints by type (if any)

### 3.3 Invariants

- [ ] Model structure is not modified
- [ ] All internal data structures remain unchanged

## 4. Algorithm

### 4.1 Overview

The function reads statistical counts from the model's matrix data structure and formats them for user-friendly logging. It uses singular/plural grammatical forms for readability. For general constraints, it categorizes them into three groups: simple constraints (MAX, MIN, ABS, AND, OR, etc.), function constraints approximated by piecewise-linear (PWL), and function constraints treated as fully nonlinear. Within each category, it enumerates constraint types with counts, inserting line breaks after every 5 types for readability.

### 4.2 Detailed Steps

1. Extract matrix data pointer from model structure
2. Read quadratic objective term count and log if >= 1 (suppress for QCP models where quadratic objective is expected)
3. Read quadratic constraint count and log if >= 1
4. Read bilinear constraint count and log if >= 1
5. Read SOS constraint count and log if >= 1
6. Read piecewise-linear objective term count and log if >= 1
7. If general constraint count is zero, exit early
8. Call helper function to count general constraints by type, separating PWL-approximated from nonlinear
9. Categorize counts into three buckets: simple (types 0-8 excluding type 6), PWL function constraints (types 9+), and nonlinear function constraints (types 9+)
10. Log simple general constraint summary with type breakdown
11. Log PWL function constraint summary with type breakdown
12. Log nonlinear function constraint summary with type breakdown
13. Log general nonlinear constraint summary (type 6) with nonlinear term count

### 4.3 Pseudocode (if needed)

```
READ counts from model matrix:
  quadObjTerms, quadConstrCount, bilinearCount, sosCount, pwlObjCount, genConstrCount

IF quadObjTerms >= 1 AND modelType != QCP:
  LOG "Model has {count} quadratic objective term(s)"

IF quadConstrCount >= 1:
  LOG "Model has {count} quadratic constraint(s)"

IF bilinearCount >= 1:
  LOG "Model has {count} bilinear constraint(s)"

IF sosCount >= 1:
  LOG "Model has {count} SOS constraint(s)"

IF pwlObjCount >= 1:
  LOG "Model has {count} piecewise-linear objective term(s)"

IF genConstrCount == 0:
  RETURN

COUNT general constraints by type → (pwlCounts[], nlCounts[])

FOR each type in 0..18:
  IF type in {0..8} excluding 6:
    simpleCount += pwlCounts[type] + nlCounts[type]
  ELSE IF type >= 9:
    pwlFuncCount += pwlCounts[type]
    nlFuncCount += nlCounts[type]
  ELSE IF type == 6:
    nlGenconCount += nlCounts[type]

IF simpleCount > 0:
  LOG "Model has {simpleCount} simple general constraint(s)"
  ENUMERATE types with counts, insert line break every 5 items

IF pwlFuncCount > 0:
  LOG "Model has {pwlFuncCount} function constraint(s) approximated by PWL"
  ENUMERATE types with counts

IF nlFuncCount > 0:
  LOG "Model has {nlFuncCount} function constraint(s) treated as nonlinear"
  ENUMERATE types with counts

IF nlGenconCount > 0:
  LOG "Model has {nlGenconCount} general nonlinear constraint(s) ({nlTermCount} nonlinear terms)"
```

### 4.4 Mathematical Foundation (if applicable)

N/A - This is a reporting function with no mathematical computation.

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) - No general constraints
- **Average case:** O(k) - k constraint types present
- **Worst case:** O(k) - k = 19 constraint types

Where:
- k = number of distinct general constraint types (max 19)

### 5.2 Space Complexity

- **Auxiliary space:** O(k) - Two count arrays for constraint types
- **Total space:** O(k)

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| N/A | N/A | Function does not return error codes |

### 6.2 Error Behavior

Function assumes valid pointers. NULL pointer dereference would cause crash (caller responsibility).

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Empty model | All counts = 0 | No output logged |
| Only linear | quadObj=0, genConstr=0 | No output (early exit) |
| Single item | quadConstr=1 | Uses singular "constraint" |
| Multiple items | quadConstr=5 | Uses plural "constraints" |
| Many constraint types | 10+ distinct types | Inserts line breaks every 5 types for readability |
| QCP model | modelType=7, quadObj>0 | Suppresses quadratic objective logging |

## 8. Thread Safety

**Thread-safe:** No

**Synchronization required:** Caller must ensure model is not modified during execution. Logging system should be thread-safe, but this function does not provide synchronization.

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_log_printf | Logging | Format and write log messages |
| cxf_count_genconstr_types | Statistics | Count general constraints by type |
| cxf_get_genconstr_name | Statistics | Get human-readable constraint type name |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_optimize | API | Called early in optimization to log model characteristics |
| cxf_solver_dispatch | Simplex Core | Called before solver algorithm selection |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_coefficient_stats | Logs coefficient ranges (numerical diagnostics) |
| cxf_simplex_preprocess | Actual presolve algorithm (not this logging function) |

## 11. Design Notes

### 11.1 Design Rationale

Provides users with visibility into model structure before optimization begins. Helps identify which solver algorithms will be triggered (e.g., nonlinear solver for general nonlinear constraints). Separates PWL-approximated from fully nonlinear function constraints to clarify solution method.

### 11.2 Performance Considerations

Negligible overhead - O(1) for basic counts, O(k) for general constraint enumeration where k is typically small. Called once per optimization.

### 11.3 Future Considerations

Could be extended to log more detailed statistics (e.g., distribution of constraint types within each category, sparsity patterns).

## 12. References

- Convexfeld Optimizer Reference Manual: General Constraints
- Convexfeld Optimizer Reference Manual: Piecewise-Linear Objectives

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
