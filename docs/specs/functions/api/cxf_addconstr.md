# cxf_addconstr

**Module:** API Variable/Constraint Operations
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Add a single linear constraint to an optimization model with sparse coefficient representation. Unlike cxf_addvar which is a thin wrapper, this function directly manages the pending constraints buffer for single-constraint additions.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| model | CxfModel* | Target model for constraint addition | Valid model pointer | Yes |
| numnz | int | Number of non-zero coefficients | >= 0 | Yes |
| cind | int* | Variable indices with non-zero coefficients | [0, numVars), NULL if numnz=0 | Conditional |
| cval | double* | Coefficient values | Finite doubles, NULL if numnz=0 | Conditional |
| sense | char | Constraint type | '<', '>', '=' | Yes |
| rhs | double | Right-hand side value | Finite double (not NaN) | Yes |
| constrname | const char* | Constraint identifier | Any string, NULL for default | No |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | 0 on success, error code otherwise |

### 2.3 Side Effects

- May allocate or extend buffer structures
- Updates constraint counter
- Logs operation status

## 3. Contract

### 3.1 Preconditions

- [ ] model pointer must be valid
- [ ] Model not in modification-blocked state
- [ ] If numnz > 0, cind and cval must be non-NULL
- [ ] All coefficients must be finite (not NaN or Inf)
- [ ] RHS must not be NaN (Inf is allowed)
- [ ] Variable indices in cind must be valid

### 3.2 Postconditions

- [ ] One constraint added to pending buffer
- [ ] Constraint data (coefficients, sense, RHS, name) stored
- [ ] Row index tracked in pending buffer
- [ ] Pending constraint count incremented

### 3.3 Invariants

- [ ] Input arrays remain unmodified
- [ ] Existing constraints unchanged until update
- [ ] Model structure remains valid

## 4. Algorithm

### 4.1 Overview

The function manages a specialized pending constraints buffer (different from the variable buffer structure). It validates inputs including NaN/Inf checks on all floating-point values, initializes or reuses the buffer with capacity tracking, validates variable indices against current model size, allocates row index tracking array if needed, copies constraint data into the buffer, and increments the pending count.

### 4.2 Detailed Steps

1. Validate model and check modification-blocked flag
2. Get environment and matrix data pointers
3. Determine whether to track constraint name (check anonymousMode)
4. If numnz > 0:
   a. Validate cind and cval are non-NULL
   b. Check each coefficient for NaN/Inf, format error if found
   c. Count coefficients below infinity threshold
5. Validate RHS is not NaN
6. Get or initialize pending constraints buffer:
   a. Check buffer exists and bit 0 is set
   b. If not, allocate 344-byte PendingConstrs structure
   c. Set magic value (0x54b249ad2594c37d) for validation
   d. Store current numVars and numConstrs
   e. Set bit 0 to mark as initialized
8. Copy constraint data:
   a. Store coefficient indices and values
   b. Store constraint sense
   c. Store RHS value
   d. Store constraint name if provided and name buffer exists
9. Update pending constraint count
10. Return success

### 4.3 Mathematical Foundation

Linear constraint form: Σ(cval[i] × x[cind[i]]) {sense} rhs

Where sense ∈ {≤, ≥, =} represented as '<', '>', '='

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) when numnz = 0 and buffer exists
- **Average case:** O(numnz) for coefficient validation and copying
- **Worst case:** O(numnz) with buffer allocation overhead

### 5.2 Space Complexity

- **Auxiliary space:** O(1) for existing buffer, O(numConstrs) for new buffer
- **Total space:** O(total pending constraints × average numnz)

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| Model modification blocked | 1017 | MODEL_MODIFICATION |
| numnz > 0 but cind/cval NULL | 1002 | NULL_ARGUMENT |
| Coefficient is NaN or Inf | 1003 | INVALID_ARGUMENT with element index |
| RHS is NaN | 1003 | INVALID_ARGUMENT |
| Buffer allocation fails | 1001 | OUT_OF_MEMORY |

### 6.2 Error Behavior

On error, the function logs a descriptive message "Problem adding constraints", resets the pending buffer to maintain consistency, and returns the error code. No partial constraint data is retained.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| No coefficients | numnz=0 | Constraint added with empty left side (0 {sense} rhs) |
| Infinite RHS | rhs=±CXF_INFINITY | Allowed, creates unbounded constraint |
| NULL name | constrname=NULL | Default name "R{index}" generated |
| Anonymous mode | env->anonymousMode=1 | Name not stored regardless of input |

## 8. Thread Safety

**Thread-safe:** No

**Synchronization required:** Caller must ensure exclusive model access

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_checkmodel | Validation | Verify model validity |
| cxf_calloc | Memory | Allocate pending buffer |
| cxf_check_nan | Validation | Check for NaN in RHS |
| cxf_check_nan_or_inf | Validation | Check coefficients |
| cxf_error | Error Tracking | Log detailed error messages |
| cxf_reset_pending_constrs | Buffer Management | Clean up on error |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_addconstrs | Batch variant - adds multiple constraints efficiently |
| cxf_addqconstr | Adds quadratic constraint (different structure) |
| cxf_delconstrs | Inverse operation - removes constraints |
| cxf_chgcoeff | Modifies existing constraint coefficients |

## 11. Design Notes

### 11.1 Design Rationale

Unlike cxf_addvar (thin wrapper), cxf_addconstr directly manages storage because constraints have variable-length coefficient lists making a simple wrapper less efficient. The specialized buffer structure optimizes for incremental single-constraint additions common in constraint generation callbacks.

### 11.2 Performance Considerations

For adding many constraints, use cxf_addconstrs instead to avoid repeated buffer reallocations and validation overhead. Single additions are optimized for callback scenarios where constraints are generated one at a time.

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
