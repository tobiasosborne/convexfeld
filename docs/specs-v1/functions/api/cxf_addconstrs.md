# cxf_addconstrs

**Module:** API Variable/Constraint Operations
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Add multiple linear constraints to an optimization model in a single batch operation using Compressed Sparse Row (CSR) format. Provides efficient bulk constraint addition with extensive validation and preprocessing.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| model | CxfModel* | Target model | Valid model pointer | Yes |
| numconstrs | int | Number of constraints to add | >= 0 | Yes |
| numnz | int | Total non-zero coefficients | >= 0 | Yes |
| cbeg | int* | CSR row start indices | [0, numnz], NULL if numnz<1 | Conditional |
| cind | int* | Variable indices for non-zeros | [0, numVars) | Conditional |
| cval | double* | Coefficient values | Finite doubles | Conditional |
| sense | char* | Constraint sense for each row | '<','>', '=', 'L','G','E' | Conditional |
| rhs | double* | Right-hand side values | Finite doubles, NULL=all 0.0 | No |
| constrnames | const char** | Constraint names | Any strings, NULL=auto | No |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | 0 on success, error code otherwise |

### 2.3 Side Effects

- Adds numconstrs constraints to pending buffer
- Allocates/extends buffer arrays for constraint data
- Normalizes sense characters and filters small coefficients
- Issues warning if small coefficients dropped
- Updates pending constraint count

## 3. Contract

### 3.1 Preconditions

- [ ] model pointer must be valid
- [ ] If numnz > 0, cbeg must be non-NULL
- [ ] cbeg[i] defines start of constraint i's data in cind/cval
- [ ] For constraint i, data spans [cbeg[i], cbeg[i+1]) or [cbeg[i], numnz) for last
- [ ] All variable indices must be valid and unique within each constraint
- [ ] All coefficients must not be NaN
- [ ] All RHS values must not be NaN

### 3.2 Postconditions

- [ ] numconstrs constraints added to pending buffer
- [ ] Duplicate variable indices within same constraint detected and rejected
- [ ] Constraint senses normalized to '<', '>', '='
- [ ] RHS values clipped to [-1e100, 1e100] range
- [ ] Small coefficients (|coeff| < 1e-13) filtered with warning
- [ ] Constraint names validated (max 255 chars) or auto-generated

### 3.3 Invariants

- [ ] Input arrays remain unmodified
- [ ] Existing model structure unchanged until update
- [ ] Model remains valid after operation

## 4. Algorithm

### 4.1 Overview

This function performs extensive preprocessing of constraint data before storage. It validates NaN/Inf values, detects duplicate variable indices within constraints using a temporary array, normalizes sense characters (e.g., 'L'→'<'), clips RHS values to valid range, filters coefficients below 1e-13 threshold with a warning, validates name lengths, and generates default names where needed.

### 4.2 Detailed Steps

1. Early exit if numconstrs < 1 (return success)
2. Validate model and check modification state
3. Validate coefficient and RHS arrays for NaN
4. Get or initialize pending buffer
5. For duplicate detection:
   a. Allocate temporary index-tracking array sized for numVars
   b. For each constraint, mark seen variables
   c. If duplicate detected, return error 1018
   d. Clear marks after each constraint
6. Validate all variable indices in range [0, numVars)
7. Normalize sense characters:
   - '<', '>', '=' accepted as-is
   - 'L'/'l' → '<', 'G'/'g' → '>', 'E'/'e' → '='
   - Invalid sense → error
8. Clip RHS values to [-1e100, 1e100]
9. Filter coefficients:
   - If |coeff| >= 1e-13, keep it
   - If |coeff| < 1e-13, drop with warning (once per session)
10. Process constraint names:
    - Validate length <= 255 characters
    - Generate defaults like "R0", "R1" for NULL names
    - Store in name buffer if available
11. Update pending constraint count and arrays
12. Return success

### 4.3 Pseudocode

```
function cxf_addconstrs(model, numconstrs, numnz, cbeg, cind, cval, sense, rhs, names):
    if numconstrs < 1:
        return SUCCESS

    validate_model_and_inputs()

    # Duplicate detection within each constraint
    temp_array ← allocate(numVars)
    for i ← 0 to numconstrs-1:
        start ← cbeg[i]
        end ← cbeg[i+1] or numnz
        for j ← start to end-1:
            var ← cind[j]
            if temp_array[var] = 1:
                return ERROR_DUPLICATE_ENTRY
            temp_array[var] ← 1
        clear temp_array for range [start, end)

    # Sense normalization
    for i ← 0 to numconstrs-1:
        sense[i] ← normalize_sense(sense[i])
        if sense[i] = '#':  # invalid
            return ERROR_INVALID_ARGUMENT

    # RHS clipping
    if rhs ≠ NULL:
        for i ← 0 to numconstrs-1:
            rhs[i] ← clip(rhs[i], -1e100, 1e100)

    # Coefficient filtering
    copy coefficients where |val| >= 1e-13
    warn if any dropped

    store_in_pending_buffer()
    return SUCCESS
```

### 4.4 Mathematical Foundation

Linear constraint i: Σ(cval[k] × x[cind[k]]) {sense[i]} rhs[i]
For k ∈ [cbeg[i], cbeg[i+1])

Small coefficient threshold: ε = 1 × 10^-13

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) when numconstrs = 0
- **Average case:** O(numconstrs + numnz) for validation and copying
- **Worst case:** O(numconstrs × numVars) if duplicate check dominates

Where:
- numconstrs = number of constraints
- numnz = total non-zero coefficients
- numVars = current variable count

### 5.2 Space Complexity

- **Auxiliary space:** O(numVars) for duplicate detection array
- **Total space:** O(numconstrs + numnz) for pending buffer storage

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| numnz > 0 but cbeg NULL | varies | Context-dependent error |
| Coefficient is NaN | 1003 | INVALID_ARGUMENT with element index |
| RHS is NaN | 1003 | INVALID_ARGUMENT with element index |
| Variable index out of range | 1006 | INVALID_INDEX |
| Duplicate variable in constraint | 1018 | DUPLICATE_ENTRY |
| Invalid constraint sense | 1003 | INVALID_ARGUMENT |
| Name exceeds 255 chars | 1003 | INVALID_ARGUMENT |

### 6.2 Error Behavior

On error, logs descriptive message and updates error tracking. No partial data stored in pending buffer.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| No constraints | numconstrs=0 | Immediate success |
| Empty constraints | numnz=0 for all | Constraints with form 0 {sense} rhs |
| NULL RHS | rhs=NULL | All RHS default to 0.0 |
| NULL sense | sense=NULL | All default to '<' (LESS_EQUAL) |
| Small coefficients | |coeff| < 1e-13 | Filtered with warning |
| Mixed case senses | 'L', 'l', 'G', 'g', etc. | Normalized correctly |

## 8. Thread Safety

**Thread-safe:** No

**Synchronization required:** Caller must ensure exclusive model access

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_checkmodel | Validation | Verify model validity |
| cxf_calloc | Memory | Allocate temp arrays and buffers |
| cxf_realloc | Memory | Grow buffer arrays as needed |
| cxf_isnan | Validation | Check for NaN values |
| cxf_warning | Logging | Issue small coefficient warning |
| memset | System | Clear temporary arrays |
| memmove | System | Copy coefficient data |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_addconstr | Single-constraint variant |
| cxf_delconstrs | Inverse operation |
| cxf_chgcoeff | Modify existing coefficients |

## 11. Design Notes

### 11.1 Design Rationale

Duplicate detection prevents mathematical errors that would create singular or ill-conditioned matrices. Small coefficient filtering improves numerical stability. Sense normalization provides flexible input formats while maintaining internal consistency.

### 11.2 Performance Considerations

Bulk addition significantly faster than repeated cxf_addconstr calls due to reduced validation overhead and single buffer operation. Duplicate detection adds O(numnz) overhead but prevents costly debug scenarios.

### 11.3 Future Considerations

Consider option to disable duplicate checking for verified input data to improve performance in high-frequency constraint generation scenarios.

## 12. Validation Checklist

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
