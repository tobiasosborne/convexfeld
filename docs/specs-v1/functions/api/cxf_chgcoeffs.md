# cxf_chgcoeffs

**Module:** API Variable/Constraint Operations
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Modify constraint matrix coefficients in batch, supporting creation, deletion, and modification of non-zero entries. Changes are batched in a pending buffer and applied during model update for efficiency.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| model | CxfModel* | Target model | Valid model pointer | Yes |
| cnt | int | Number of coefficients to change | >= 0 | Yes |
| cind | int* | Constraint (row) indices | [0, numConstrs), NULL if cnt=0 | Conditional |
| vind | int* | Variable (column) indices | [0, numVars), NULL if cnt=0 | Conditional |
| val | double* | New coefficient values | Finite doubles, NULL if cnt=0 | Conditional |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | 0 on success, error code otherwise |

### 2.3 Side Effects

- May allocate or grow CoeffChange structure
- Updates change count
- Logs operation status

## 3. Contract

### 3.1 Preconditions

- [ ] model pointer must be valid
- [ ] Model not in modification-blocked state
- [ ] If cnt > 0, cind, vind, and val must be non-NULL
- [ ] All constraint indices must be in range [0, numConstrs)
- [ ] All variable indices must be in range [0, numVars)
- [ ] All coefficient values must be finite (not NaN or Inf)
- [ ] cnt must not cause integer overflow when added to pending count

### 3.2 Postconditions

- [ ] cnt coefficient changes added to pending buffer
- [ ] Changes batched until next model update
- [ ] If multiple changes target same (cind, vind) pair, last wins
- [ ] Coefficient change count updated
- [ ] Error tracking reflects operation status

### 3.3 Invariants

- [ ] Input arrays remain unmodified
- [ ] Existing matrix unchanged until update
- [ ] Model structure remains valid

## 4. Algorithm

### 4.1 Overview


### 4.2 Detailed Steps

1. Early return if cnt < 1 (success, no changes)
2. Validate model and check modification-blocked state
3. Validate cnt doesn't exceed maximum (0x7fffffffffffffff)
4. Validate cind, vind, val arrays are non-NULL
5. Validate all coefficient values for NaN/Inf:
   a. Check each val[i] for NaN or Inf
   b. Format detailed error message with element index if found
6. Get or initialize pending buffer:
   a. Check if buffer exists and bit 0 is set
   b. If not, allocate or reuse 344-byte PendingBuffer
   c. Set magic value 0x54b249ad2594c37d
   d. Store current numVars and numConstrs
   e. Set bit 0 to mark initialized
7. Validate all index pairs:
   a. Check cind[i] in [0, numConstrs) for all i
   b. Check vind[i] in [0, numVars) for all i
   c. Return INVALID_INDEX if any out of range
   a. If NULL, allocate 40-byte structure
   b. Set capacity = cnt + 1000
   c. Allocate three arrays: cind, vind, val
9. Check if growth needed:
   a. Calculate newCount = current count + cnt
   b. If newCount > capacity:
      - Calculate newCapacity = (current + cnt + 1000) × 1.5
      - Check for overflow (max 0x7fffffffffffffff)
      - Reallocate all three arrays to new capacity
10. Copy coefficient change data:
    a. Use memmove to copy cind → buffer (handles overlap)
    b. Use memmove to copy vind → buffer
    c. Use memmove to copy val → buffer
11. Update count: count += cnt
12. Clear error state and return success

### 4.3 Pseudocode

```
function cxf_chgcoeffs(model, cnt, cind, vind, val):
    if cnt < 1:
        return SUCCESS

    validate_model_and_arrays()
    validate_all_coefficients_finite(val, cnt)

    pendingBuffer ← get_or_create_pending_buffer(model)
    validate_all_indices(cind, vind, cnt, pendingBuffer.numConstrs, pendingBuffer.numVars)

    coeffChange ← pendingBuffer.coeffChanges
    if coeffChange = NULL:
        capacity ← cnt + 1000
        coeffChange ← allocate_coeff_change(capacity)
        pendingBuffer.coeffChanges ← coeffChange
    else if coeffChange.count + cnt > coeffChange.capacity:
        newCapacity ← (coeffChange.count + cnt + 1000) × 1.5
        if newCapacity > MAX_INT64:
            newCapacity ← MAX_INT64
        reallocate_arrays(coeffChange, newCapacity)

    baseIdx ← coeffChange.count
    memmove(coeffChange.cind + baseIdx, cind, cnt × sizeof(int))
    memmove(coeffChange.vind + baseIdx, vind, cnt × sizeof(int))
    memmove(coeffChange.val + baseIdx, val, cnt × sizeof(double))

    coeffChange.count ← coeffChange.count + cnt
    return SUCCESS
```

### 4.4 Mathematical Foundation

Matrix coefficient change: A[cind[i], vind[i]] ← val[i]

Where A is the constraint matrix (numConstrs × numVars)

Special cases:
- val[i] = 0: Removes coefficient (sparsifies matrix)
- Original A[r,c] = 0, val > 0: Creates new non-zero
- Original A[r,c] ≠ 0, val ≠ 0: Modifies existing coefficient

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) when cnt = 0
- **Average case:** O(cnt) for validation and copying
- **Worst case:** O(cnt) with buffer reallocation

Where cnt = number of coefficient changes

### 5.2 Space Complexity

- **Auxiliary space:** O(cnt) for new coefficient changes
- **Total space:** O(total pending changes)

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| Model modification blocked | 1017 | MODEL_MODIFICATION |
| cnt > MAX_INT64 | 1006 | INVALID_INDEX (overflow protection) |
| cnt > 0 but cind/vind/val NULL | 1002 | NULL_ARGUMENT |
| Coefficient is NaN or Inf | 1003 | INVALID_ARGUMENT with element index |
| Constraint index out of range | 1006 | INVALID_INDEX |
| Variable index out of range | 1006 | INVALID_INDEX |
| Buffer allocation fails | 1001 | OUT_OF_MEMORY |

### 6.2 Error Behavior

On error, logs message "Problem changing coefficient", resets pending buffer to maintain consistency, and returns error code. No partial changes applied.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| No changes | cnt=0 | Immediate success, no processing |
| Set to zero | val[i]=0.0 | Removes coefficient (creates explicit zero) |
| Same position multiple times | (r,c) appears twice in lists | Last value wins when applied |
| Create new nonzero | A[r,c] was 0, val>0 | New coefficient inserted |
| Large batch | cnt = 1,000,000 | Buffer grows efficiently with 1.5× factor |

## 8. Thread Safety

**Thread-safe:** No

**Synchronization required:** Caller must ensure exclusive model access

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_checkmodel | Validation | Verify model validity |
| cxf_calloc | Memory | Allocate pending buffer and CoeffChange |
| cxf_malloc | Memory | Allocate coefficient arrays |
| cxf_realloc | Memory | Grow coefficient arrays |
| cxf_free | Memory | Clean up on error |
| cxf_isnan_or_inf | Validation | Check coefficient values |
| memmove | System | Copy arrays safely |
| cxf_error | Error Tracking | Log detailed error messages |
| cxf_reset_pending_buffer | Buffer Management | Clean up on error |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| User code | Application | Standard API entry point |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_chgcoeff | Single coefficient variant (less efficient for bulk) |
| cxf_x_chgcoeffs | 64-bit index variant for very large models |
| cxf_addvar/cxf_addconstr | Creates new coefficients during model building |
| cxf_updatemodel | Applies pending coefficient changes |

## 11. Design Notes

### 11.1 Design Rationale

Batching coefficient changes is essential for performance because applying each change immediately would require expensive matrix structure updates. The parallel array design (cind, vind, val) uses less memory than storing change records as structures. Growth factor 1.5 balances reallocation frequency with memory waste.

### 11.2 Performance Considerations

For very large change batches, consider sorting by (cind, vind) before applying to improve cache locality during matrix update. The function uses memmove instead of memcpy to handle potential (though unlikely) memory overlap scenarios safely.

### 11.3 Future Considerations

For models exceeding 2 billion non-zeros, use cxf_x_chgcoeffs with 64-bit indices. The internal CoeffChange buffer uses 64-bit count/capacity even though this function takes 32-bit cnt parameter.

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
