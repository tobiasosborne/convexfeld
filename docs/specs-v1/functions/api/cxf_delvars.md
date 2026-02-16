# cxf_delvars

**Module:** API Variable/Constraint Operations
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Mark variables for deletion from an optimization model. The actual deletion is deferred until the next model update operation, allowing efficient batching of multiple modifications.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| model | CxfModel* | Target model for variable deletion | Valid model pointer | Yes |
| numdel | int | Number of variables to delete | >= 0 | Yes |
| ind | int* | Indices of variables to delete | [0, numVars), NULL if numdel=0 | Conditional |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | 0 on success, error code otherwise |

### 2.3 Side Effects

- Sets deletion flags in pending buffer's deletion mask
- Allocates or extends deletion mask array if needed
- Updates pending modification tracking
- Logs operation status

## 3. Contract

### 3.1 Preconditions

- [ ] model pointer must be valid
- [ ] Model not in modification-blocked state (e.g., during optimization)
- [ ] If numdel > 0, ind array must be non-NULL
- [ ] All indices in ind array must be in range [0, numVars)

### 3.2 Postconditions

- [ ] Deletion bit set for each specified variable in pending buffer
- [ ] Variables marked for deletion but not yet removed
- [ ] Actual deletion occurs on next model update
- [ ] Duplicate indices handled correctly (same variable can appear multiple times)

### 3.3 Invariants

- [ ] Input ind array remains unmodified
- [ ] Existing variable data unchanged until update
- [ ] Model structure remains valid

## 4. Algorithm

### 4.1 Overview

The function uses a pending buffer with a deletion mask array. Each variable has a corresponding mask entry; setting bit 0 marks it for deletion. The function ensures the pending buffer is initialized, allocates the deletion mask if needed, validates all indices, and sets the deletion bit for each specified variable.

### 4.2 Detailed Steps

1. Validate model and check modification state
2. Return success immediately if numdel < 1
3. Validate ind array is non-NULL when numdel > 0
   a. Check if buffer exists and is in variable mode (bit 0 set)
   b. If not, allocate or reuse buffer with current matrix dimensions
   c. Set validation magic value and environment version
5. Initialize deletion mask if not already set (bit 2 check):
   a. Allocate mask array sized for maximum variables
   b. Zero-initialize the mask array
   c. Set bit 2 to indicate mask is initialized
6. Validate each variable index in ind array:
   a. Check index >= 0 and < numVars
   b. Return INVALID_INDEX error if out of range
7. For each valid index, set bit 0 in mask[index] to mark deletion
8. Return success

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) when numdel = 0
- **Average case:** O(numdel) for index validation and bit setting
- **Worst case:** O(numVars) when first initializing deletion mask

Where:
- numdel = number of variables to delete
- numVars = current variable count in model

### 5.2 Space Complexity

- **Auxiliary space:** O(numVars) for deletion mask (allocated once)
- **Total space:** O(numVars) for pending buffer and mask

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| model is NULL or invalid | varies | cxf_checkmodel determines code |
| Model modification blocked | 1017 | MODEL_MODIFICATION - cannot modify during optimization |
| numdel > 0 but ind is NULL | 1002 | NULL_ARGUMENT - array required |
| Variable index out of range | 1006 | INVALID_INDEX - index >= numVars or < 0 |
| Pending buffer allocation fails | 1001 | OUT_OF_MEMORY - cannot allocate tracking structures |

### 6.2 Error Behavior

On error, the function logs a descriptive message and resets the pending buffer to maintain consistency. No partial deletions are applied.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| No deletions | numdel=0 | Immediate success, no processing |
| Duplicate indices | ind={0,0,0} | Same variable marked multiple times, OK |
| Delete all variables | ind=[0..numVars-1] | All variables marked, model becomes empty after update |
| NULL ind with numdel=0 | numdel=0, ind=NULL | Success, no error |

## 8. Thread Safety

**Thread-safe:** No

**Synchronization required:** Caller must ensure exclusive access to model

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_checkmodel | Validation | Verify model validity |
| cxf_calloc | Memory | Allocate pending buffer and mask |
| memset | System | Zero-initialize deletion mask |
| cxf_error | Error Tracking | Log error with message |
| cxf_reset_pending_buffer | Buffer Management | Clean up on error |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_addvar/cxf_addvars | Inverse operation - adds variables |
| cxf_updatemodel | Applies pending deletions to model |
| cxf_delconstrs | Parallel function for constraint deletion |

## 11. Design Notes

### 11.1 Design Rationale

Lazy deletion using a bit mask allows efficient batching of multiple delete operations before expensive matrix updates. The bit-per-variable approach minimizes memory overhead compared to storing indices.

### 11.2 Performance Considerations

Deletion mask allocated once and reused for multiple operations. Actual variable removal during update can be expensive (O(numVars × numConstrs) for matrix repack), making batching critical for performance.

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
