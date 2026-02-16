# cxf_realloc

**Module:** Memory Management
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Resize a previously allocated memory block while preserving its contents. This function changes the size of a memory allocation obtained from cxf_malloc or cxf_calloc, either growing or shrinking it. The original contents are preserved up to the minimum of the old and new sizes. Used for dynamic arrays and buffers that need to grow or shrink during operation, such as name buffers, coefficient lists, and solution storage.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| env | CxfEnv* | Environment context pointer | Valid CxfEnv pointer, or NULL if ptr is NULL | Yes |
| ptr | void* | Existing allocation pointer, or NULL | NULL or pointer from cxf_malloc/cxf_calloc/cxf_realloc | Yes |
| new_size | size_t | New size in bytes | Any non-negative value | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | void* | Pointer to resized memory, or NULL on failure/special cases |

### 2.3 Side Effects

- May move memory block to new address (invalidates original pointer)
- Updates environment's total memory tracking counter
- Original pointer becomes invalid (whether function succeeds or fails)
- Acquires and releases environment's critical section lock

## 3. Contract

### 3.1 Preconditions

- [ ] If ptr is not NULL: env must be valid CxfEnv pointer
- [ ] If ptr is not NULL: ptr must be from cxf_malloc/cxf_calloc/cxf_realloc
- [ ] Environment lock must be available

### 3.2 Postconditions

- [ ] If successful: Returns pointer to at least 'new_size' bytes
- [ ] If successful: Original data preserved up to min(old_size, new_size)
- [ ] If successful: If new_size > old_size, new bytes are uninitialized
- [ ] If successful: Environment memory counter updated (old size → new size)
- [ ] If failed: Returns NULL and original pointer REMAINS VALID
- [ ] Special case: If ptr is NULL, behaves like cxf_malloc(env, new_size)
- [ ] Special case: If new_size is 0, behaves like cxf_free(env, ptr) and returns NULL

### 3.3 Invariants

- [ ] Environment's total memory count accurately reflects all allocations
- [ ] Original data is never corrupted during resize operation

## 4. Algorithm

### 4.1 Overview

The function handles three distinct cases: (1) ptr is NULL - delegate to cxf_malloc, (2) new_size is 0 - delegate to cxf_free, (3) normal resize - acquire lock, call system realloc to resize the allocation (potentially moving data), update tracking structures with new size, release lock. The critical feature is that on failure, the original allocation remains valid and unchanged, allowing the caller to continue with reduced capacity or cleanup gracefully.

### 4.2 Detailed Steps

1. Handle special case: If ptr is NULL, call cxf_malloc(env, new_size) and return
2. Validate environment (must not be NULL for non-NULL ptr)
3. Handle special case: If new_size is 0, call cxf_free(env, ptr) and return NULL
4. Acquire environment's critical section lock
5. Retrieve old allocation size from tracking structure
6. Call system realloc to resize allocation
   - May return same address or different address
   - Preserves data up to min(old_size, new_size)
7. If realloc successful:
   - Update environment's total memory counter (subtract old size, add new size)
   - Update tracking structure with new size and possibly new address
8. Release critical section lock
9. Return pointer (same or different address), or NULL on failure

### 4.3 Pseudocode (if needed)

```
FUNCTION cxf_realloc(env, ptr, new_size) → pointer or NULL
  # Special case: NULL pointer acts as malloc
  IF ptr = NULL THEN
    RETURN cxf_malloc(env, new_size)
  END IF

  # Validate environment
  IF env = NULL THEN
    RETURN NULL
  END IF

  # Special case: zero size acts as free
  IF new_size = 0 THEN
    cxf_free(env, ptr)
    RETURN NULL
  END IF

  ACQUIRE lock(env.criticalSection)

  old_size ← get_allocation_size(ptr)  # From tracking structure
  new_ptr ← system_realloc(ptr, new_size)

  IF new_ptr ≠ NULL THEN
    env.totalBytesAllocated ← env.totalBytesAllocated - old_size + new_size
    update_tracking(env, ptr, new_ptr, old_size, new_size)
  END IF
  # If realloc failed, ptr is still valid, tracking unchanged

  RELEASE lock(env.criticalSection)
  RETURN new_ptr
END FUNCTION
```

### 4.4 Mathematical Foundation (if applicable)

Not applicable - this is a system-level resource management function.

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) - realloc extends in place
- **Average case:** O(min(old_size, new_size)) - data must be copied
- **Worst case:** O(max(old_size, new_size)) - worst-case copy + fragmentation

Where:
- old_size = original allocation size
- new_size = requested new size
- Depends heavily on memory allocator and heap fragmentation

### 5.2 Space Complexity

- **Auxiliary space:** O(1) - tracking structure update
- **Total space:** O(new_size) - the new allocation size
- **Temporary:** May require O(old_size) during copy if address changes

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| env is NULL (when ptr not NULL) | N/A (returns NULL) | Invalid environment |
| Insufficient memory | N/A (returns NULL) | System realloc failed |
| Memory limit exceeded | N/A (returns NULL) | Would exceed configured MemLimit |

### 6.2 Error Behavior

Returns NULL on failure. **Critical:** The original pointer remains valid when NULL is returned, allowing the caller to either continue with the old allocation or free it explicitly. Callers must use pattern: `temp = cxf_realloc(...); if (temp) { ptr = temp; }` to avoid losing the original pointer.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| NULL pointer | ptr=NULL, size=100 | Equivalent to cxf_malloc(env, 100) |
| Zero new size | ptr=valid, size=0 | Equivalent to cxf_free(env, ptr), return NULL |
| Same size | ptr=valid, size=old_size | May return same pointer, minimal work |
| Shrink by 99% | old_size=1MB, new_size=10KB | Usually returns same pointer, quick |
| Grow by 10x | old_size=1KB, new_size=10KB | May move data, O(old_size) copy |
| Grow beyond limit | new_size exceeds MemLimit | Return NULL, original ptr valid |

## 8. Thread Safety

**Thread-safe:** Yes

**Synchronization required:** Acquires environment->criticalSection during reallocation and tracking updates. Multiple threads can call cxf_realloc concurrently without corruption. However, caller must ensure no other thread is accessing the memory being resized.

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_malloc | Memory | Handle NULL pointer case |
| cxf_free | Memory | Handle zero size case |
| realloc | System | Actual memory reallocation |
| EnterCriticalSection | System | Acquire environment lock |
| LeaveCriticalSection | System | Release environment lock |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_name_buffer_realloc | Internal | Grow name storage buffer dynamically |
| cxf_chgcoeffs | API | Grow coefficient arrays during batch operations |
| Various internal functions | Various | Dynamic array growth |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_malloc | Initial allocation - called when ptr is NULL |
| cxf_free | Deallocation - called when new_size is 0 |
| realloc | Underlying system function (wrapped by this) |
| memmove | May be used internally if manual reallocation needed |

## 11. Design Notes

### 11.1 Design Rationale

Realloc provides efficient dynamic resizing for:
1. **Growing arrays** - Add elements without frequent reallocation
2. **Name buffers** - Expand as more variables/constraints added
3. **Coefficient lists** - Batch operations with unknown final size
4. **Solution storage** - Grow as more solutions found

The "original pointer valid on failure" semantic is critical for error recovery - caller can continue with reduced capacity or cleanup without data loss.

### 11.2 Performance Considerations

Realloc performance characteristics:
- **Shrinking:** Usually in-place, very fast (microseconds)
- **Growing small amount:** Often in-place if space available after block
- **Growing large amount:** Typically requires copy, O(old_size) time
- **Fragmented heap:** More likely to require move and copy

Common growth strategies:
- **Conservative:** Multiply by 1.5 (lower memory waste, more reallocations)
- **Aggressive:** Multiply by 2.0 (higher memory waste, fewer reallocations)
- **Hybrid:** Double until 64KB, then 1.5x until 1MB, then fixed increments

### 11.3 Future Considerations

Potential improvements:
- Provide bulk realloc for multiple arrays (single lock acquisition)
- Expose in-place vs move information for caller optimization
- Support shrink-to-fit operation (reduce wasted space)

## 12. References

- Standard C realloc specification (ISO C standard)
- Lea allocator documentation (common malloc implementation)
- "The Art of Computer Programming" Vol 1 - Dynamic Storage Allocation

## 13. Validation Checklist

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
